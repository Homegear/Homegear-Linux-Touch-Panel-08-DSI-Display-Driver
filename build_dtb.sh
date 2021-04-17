#/bin/sh

export ARCH=arm;
export CROSS_COMPILE=arm-linux-gnueabihf;

dtc -I dts -O dtb -o hgltp08.dtbo -@ lcd_driver.dts

