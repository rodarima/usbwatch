
#include <libudev.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <libnotify/notify.h>

struct dev_t
{
	char *syspath;
	struct timeval *lastseen;
	int status;
	struct dev_t *next;
};

struct dev_t *dev_history = NULL;

/* Maximum time between resets to consider a fault */
int millis = 400;

/* Status */
#define STATUS_ADDED 1
#define STATUS_REMOVED 2
#define MAXLINE 200

/* If the device is found, return it. Otherwise, return NULL */
struct dev_t *find_dev(const char *syspath)
{
	struct dev_t *node = dev_history;
	while(node)
	{
		if(strcmp(syspath, node->syspath) == 0)
		{
			return node;
		}
		node = node->next;
	}
	return NULL;
}

void free_dev(struct dev_t *dev)
{
	free(dev->syspath);
	free(dev->lastseen);
	free(dev);
}

int remove_dev(struct dev_t *dev)
{
	struct dev_t *node = dev_history;
	struct dev_t **last = &dev_history;
	while(node)
	{
		if(node == dev)
		{
			*(last) = node->next;
			free_dev(node);
			return 0;
		}
		last = &(node->next);
		node = node->next;
	}
	return 1;
}

/* Remove all devices older than the maximum inactivity limit */
int remove_old()
{
	struct timeval maxdiff;
	struct timeval diff;
	struct timeval now;
	if(gettimeofday(&now, NULL) != 0) return -1;
	maxdiff.tv_sec = millis / 1000;
	maxdiff.tv_usec = (millis % 1000) * 1000;

	struct dev_t *node = dev_history;
	struct dev_t *dev;
	while(node)
	{
		timersub(&now, node->lastseen, &diff);
		if(timercmp(&diff, &maxdiff, >))
		{
			dev = node;
			node = node->next;

			//printf("Removing %s due to inactivity\n", dev->syspath);
			remove_dev(dev);
		}
		else
		{
			node = node->next;
		}
	}
	return 0;
}

int add_device(struct dev_t *dev)
{
	struct dev_t **node;
	node = &dev_history;
	while(*node)
	{
		node = &((*node)->next);
	}
	*node = dev;
}

int is_too_recent(struct dev_t *dev, struct timeval *now)
{
	struct timeval maxdiff;
	struct timeval diff;

	maxdiff.tv_sec = millis / 1000;
	maxdiff.tv_usec = (millis % 1000) * 1000;
	
	timersub(now, dev->lastseen, &diff);
	if(!timercmp(&diff, &maxdiff, >))
	{
		return 1;
	}
	return 0;
}

void problem(struct dev_t *dev, int new_status, struct timeval *diff)
{
	char buf[MAXLINE];

	snprintf(buf, MAXLINE, "Problem with %s", dev->syspath);
	buf[MAXLINE-1] = 0;
	//printf("Problem with %s time diff %lu.%lu\n", dev->syspath,
	//	diff->tv_sec, diff->tv_usec);
	printf(buf);

	NotifyNotification * n = notify_notification_new(
		"USB reset", buf, "dialog-information");	
	notify_notification_set_urgency(n, NOTIFY_URGENCY_CRITICAL);
        notify_notification_set_timeout(n, 20000); // 10 seconds
	notify_notification_show(n, NULL);
	g_object_unref(G_OBJECT(n));
}

void notify_device(struct dev_t *dev, int new_status)
{
	char buf[MAXLINE];
	char *fmt;

	if(new_status == STATUS_ADDED) fmt = "Added %s";
	else fmt = "Removed %s";

	snprintf(buf, MAXLINE, fmt, dev->syspath);
	buf[MAXLINE-1] = 0;

	NotifyNotification * n = notify_notification_new(
		"USB event", buf, "dialog-information");	
	notify_notification_show(n, NULL);
	g_object_unref(G_OBJECT(n));
}

/* Search for the device, and add it if not found. Update the lastseen and
status if the device was already found */
int event_device(const char *syspath, int new_status)
{
	int expected_status;
	struct dev_t *dev;
	struct timeval *now = malloc(sizeof(*now));

	if(new_status == STATUS_ADDED) expected_status = STATUS_REMOVED;
	else expected_status = STATUS_ADDED;

	if(gettimeofday(now, NULL) != 0) return -1;

	if(dev = find_dev(syspath))
	{
		/* Compare time diff between events */
		if(dev->status == expected_status)
		{
			if(is_too_recent(dev, now))
			{
				struct timeval diff;
				timersub(now, dev->lastseen, &diff);
				problem(dev, new_status, &diff);
			}
			else
			{
				notify_device(dev, new_status);
			}
		}
		free(dev->lastseen);
		dev->lastseen = now;
		dev->status = new_status;
		return 0;
	}

	/* Device not found, adding */

	dev = malloc(sizeof(*dev));
	dev->syspath = strdup(syspath);
	dev->lastseen = now;
	dev->status = new_status;
	dev->next = NULL;

	notify_device(dev, new_status);
	add_device(dev);

	return 0;
}


int event(struct udev_device *dev)
{
	struct timeval now;
	const char *action = udev_device_get_action(dev);
	const char *syspath = udev_device_get_syspath(dev);
	if(gettimeofday(&now, NULL) != 0) return -1;

	if(strcmp(action, "add") == 0)
	{
		event_device(syspath, STATUS_ADDED);
	}
	else if(strcmp(action, "remove") == 0)
	{
		event_device(syspath, STATUS_REMOVED);
	}

	remove_old();

	return 0;
}

void usage()
{
	printf("Usage: usbwatch [-t <interval>]\n");
	exit(1);
}

void monitor(struct udev *udev)
{
	struct udev_device *dev;
   	struct udev_monitor *mon;
	int fd;

	/* Create the udev object */
	udev = udev_new();
	if (!udev) {
		fprintf(stderr, "Can't create udev\n");
		exit(1);
	}

	
	mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", "usb_device");
	udev_monitor_enable_receiving(mon);
	fd = udev_monitor_get_fd(mon);

	while (1)
	{
		fd_set fds;
		struct timeval tv;
		int ret;
		
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 3600;
		tv.tv_usec = 0;
		
		ret = select(fd+1, &fds, NULL, NULL, &tv);
		
		/* Check if our file descriptor has received data. */
		if (ret > 0 && FD_ISSET(fd, &fds))
		{
			dev = udev_monitor_receive_device(mon);
			if(dev)
			{
				event(dev);
				udev_device_unref(dev);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	struct udev *udev;

	if(argc == 3)
	{
		if(strcmp(argv[1], "-t") == 0)
		{
			millis = atoi(argv[2]);
		}
		else usage();
	}
	else if (argc != 1) usage();


	notify_init("usbwatch");
	
	monitor(udev);

	udev_unref(udev);
	notify_uninit();

	return 0;       
}

