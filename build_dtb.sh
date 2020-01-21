#/bin/sh

export ARCH=arm;
export CROSS_COMPILE=arm-linux-gnueabihf;

../linux/scripts/dtc/dtc -I dts -O dtb -o HG_LTP08_lcd_driver.dtbo -@ lcd_driver.dts 

