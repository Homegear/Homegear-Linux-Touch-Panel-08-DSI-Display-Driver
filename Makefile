obj-m += HG_LTP08_lcd_driver.o
HG_LTP08_lcd_driver-y := lcd_driver.o

KVERSION := $(shell uname -r)

all:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
#	make -C /home/adrian/raspberry/chroot/lib/modules/$(VERSION)/build M=$(shell pwd) modules


clean:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
#	make -C /home/adrian/raspberry/chroot/lib/modules/$(VERSION)/build M=$(shell pwd) clean