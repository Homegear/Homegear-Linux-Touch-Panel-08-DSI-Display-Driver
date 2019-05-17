#/bin/sh
export KERNEL=kernel7
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- bcm2709_defconfig
#make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- bcmrmpi_defconfig
