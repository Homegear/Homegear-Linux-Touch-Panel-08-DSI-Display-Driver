obj-m += lcd_driver.o

all:
	make -C /home/adrian/raspberry/chroot/lib/modules/$(VERSION)/build M=$(shell pwd) modules

clean:
	make -C /home/adrian/raspberry/chroot/lib/modules/$(VERSION)/build M=$(shell pwd) clean