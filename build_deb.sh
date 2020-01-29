#/bin/sh

./build_dtb.sh
dpkg-buildpackage -b -rfakeroot -us -uc

