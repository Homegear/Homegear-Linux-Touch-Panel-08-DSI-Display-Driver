#/bin/sh

set -e

if ! test -e ../linux/.config; then
	echo "Needs make ARCH=arm multi_v7_defconfig or ARCH=arm64 defconfig"
	exit 1
fi

export VERSION="4.19.95-v7+"
export CPATH=../linux/include

if grep -q CONFIG_ARM64=y ../linux/.config; then
	export ARCH=arm64
	export CROSS_COMPILE="arm-linux-gnueabihf-"
else
	export ARCH=arm
	export CROSS_COMPILE="arm-linux-gnueabihf-"
fi

export INITRD=No
export INSTALL_MOD_PATH=/home/adrian/raspberry/chroot

make clean
make all

echo "Compiled $VERSION module"

