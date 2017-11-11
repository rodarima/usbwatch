# Maintainer: Rodrigo Arias <rodarima@gmail.com>

pkgname=usbwatch
pkgver=0.0.1
pkgrel=1
pkgdesc="Monitor USB changes"
url="https://github.com/rodarima/usbwatch/"
arch=('i686' 'x86_64')
license=('GPL3')

depends=('systemd' 'libnotify')

source=("$pkgname-$pkgver::git+https://github.com/rodarima/usbwatch/")
md5sums=('SKIP')

build() {
	make -C "$srcdir/$pkgname-$pkgver"
}

package() {
	make -C "$srcdir/$pkgname-$pkgver" DESTDIR="${pkgdir}" PREFIX=/usr install
}

# vim:set ts=2 sw=2 et:
