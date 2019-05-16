#/bin/sh

export ARCH=arm;
export CROSS_COMPILE=arm-linux-gnueabihf;

/home/adrian/raspberry/linux/scripts/dtc/dtc -I dts -O dtb -o lcd_driver.dtbo -@ lcd_driver.dts 


