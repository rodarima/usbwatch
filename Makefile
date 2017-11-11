LDFLAGS=-ludev `pkg-config --libs libnotify`
CFLAGS=-O2 `pkg-config --cflags libnotify`

PREFIX ?= /usr/local
MANPREFIX = ${PREFIX}/share/man

all: usbwatch

usbwatch: usbwatch.c

install-usbwatch: usbwatch
	mkdir -p ${DESTDIR}${PREFIX}/bin
	install -m755 usbwatch ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	install -m644 usbwatch.1 ${DESTDIR}${MANPREFIX}/man1

install-service: usbwatch.1
	install -Dm644 usbwatch.service ${DESTDIR}${PREFIX}/lib/systemd/user/usbwatch.service

install: install-usbwatch install-service

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/usbwatch
	rm -f ${DESTDIR}${MANPREFIX}/man1/usbwatch.1
	rm -f ${DESTDIR}${PREFIX}/lib/systemd/user/usbwatch.service

clean:
	rm -f usbwatch

.PHONY: all clean install uninstall
