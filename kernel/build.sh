#/bin/sh

set -e

if ! test -e .config; then
	echo "Needs make ARCH=arm multi_v7_defconfig or ARCH=arm64 defconfig"
	exit 1
fi

if grep -q CONFIG_ARM64=y .config; then
	export ARCH=arm64
	export CROSS_COMPILE="arm-linux-gnueabihf-"
else
	export ARCH=arm
	export CROSS_COMPILE="arm-linux-gnueabihf-"
fi

export INITRD=No
export INSTALL_PATH=$HOME/raspberry/chroot/boot
export INSTALL_MOD_PATH=$HOME/raspberry/chroot

# Fix up arm64 defconfig to not fill up my disk so badly.
./scripts/config -d CONFIG_LOCALVERSION_AUTO

# Fix up arm64 defconfig for netbooting.
./scripts/config -e CONFIG_USB_USBNET
./scripts/config -e CONFIG_USB_NET_SMSC95XX

make -j10

VERSION=`make -j10 -f Makefile kernelrelease 2> /dev/null`

sudo -E make -j10 zinstall dtbs_install

if grep 'CONFIG_MODULES=y' .config > /dev/null; then
	sudo -E make -j10 modules_install
fi

sudo perl -pi -e "s|^dtoverlay=vc4-kms-v3d.*|#dtoverlay=vc4-kms-v3d|" \
	   $INSTALL_PATH/config.txt

if test $ARCH = arm64; then
	# Upstream 64-bit kernel
	sudo ln -f $INSTALL_PATH/vmlinuz-$VERSION $INSTALL_PATH/kernel8.img
	sudo ln -f $INSTALL_PATH/dtbs/$VERSION/broadcom/bcm2837-rpi-3-b.dtb \
		   $INSTALL_PATH/bcm2710-rpi-3-b.dtb
else
	if test -e arch/arm/boot/dts/bcm2709.dtsi; then
		# Downstream 32-bit kernel
		sudo ln -f $INSTALL_PATH/dtbs/$VERSION/bcm2709-rpi-2-b.dtb \
			   $INSTALL_PATH/bcm2709-rpi-2-b.dtb
		sudo ln -f $INSTALL_PATH/dtbs/$VERSION/bcm2710-rpi-3-b.dtb \
			   $INSTALL_PATH/bcm2710-rpi-3-b.dtb
		sudo perl -pi -e "s|^#dtoverlay=vc4-kms-v3d.*|dtoverlay=vc4-kms-v3d|" \
			   $INSTALL_PATH/config.txt
		echo "Downstream 32-bit kernel"
	else
		# Upstream 32-bit kernel
		sudo ln -f $INSTALL_PATH/dtbs/$VERSION/bcm2836-rpi-2-b.dtb \
			   $INSTALL_PATH/bcm2709-rpi-2-b.dtb
		sudo ln -f $INSTALL_PATH/dtbs/$VERSION/bcm2837-rpi-3-b.dtb \
			   $INSTALL_PATH/bcm2710-rpi-3-b.dtb
		echo "Upstream 32-bit kernel"
	fi
	sudo ln -f $INSTALL_PATH/vmlinuz-$VERSION $INSTALL_PATH/kernel7.img
	# Remove the 64-bit kernel's link, so we default to 32-bit boot.
	sudo rm -f $INSTALL_PATH/kernel8.img
fi

echo "Installed $VERSION"

