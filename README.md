## Setup for development

### Kernel source

Just clone the rpi-4.19.y branch from here: https://github.com/raspberrypi/linux
It might work with older kernel versions but I've seen reports of ugly bugs in them which were solved (as serious memory leak in the vc4 driver), so we better stick with this version (or later).
I provided a couple of 'sh' files in the 'kernel' directory, just copy them in the linux directory, run 'build_config.sh' then 'build.sh' after having cross compiling set up and it should build and install the kernel.
I have set the following building structure, if different some scripts will need to be changed:

The kernel source is in:
$HOME/raspberry/linux

Installation directory is:
$HOME/raspberry/chroot

Panel sources are in
$HOME/raspberry/panel (this can have any name, but still has to be in $HOME/raspberry).

### Cross compiling

The 'tools' repository is here: https://github.com/raspberrypi/tools
I think a description of how to use is here: https://stackoverflow.com/questions/19162072/how-to-install-the-raspberry-pi-cross-compiler-on-my-linux-host-machine

Anholt has a short description of a build environment here: https://github.com/anholt/linux/wiki/Raspberry-Pi-development-environment
It works for raspberry pi, but for compute module avoid the booting from the network part, I managed to brick the module that way (partially, with some effort it could be still flashed).
Anyway, the chroot setting there is not described well, I had to look elsewhere for the info, works nicely with raspberry pi 3, but not with the compute module on dev board.

### Flashing the compute module

Rpiboot is here:
https://github.com/raspberrypi/usbboot

It will be needed for the initial flashing of the compute module. I downloaded Raspbian from here: https://www.raspberrypi.org/downloads/raspbian/
I got the 'with desktop' variant but not the full one, I installed the additional software I needed, the full one is over 4 GBytes so it does not fit in cm3.

Don't forget that immediately after you flash the device, to create an empty 'ssh' file in /boot, it will allow you to log in with ssh (otherwise it's disabled).

### Firmware

Firmware, if needed, is here: https://github.com/raspberrypi/firmware
I tested with both the current firmware and the 'next' branch, fkms does not work with any of them (yet?). 
kms works with both, because it's a 'full' driver.

### Compiling the panel driver and dts

Just run the two sh scripts in the panel directory and they should generate the ko file (the module) and the dtbo file (the overlay).

### Installing the driver

Remove the contents of overlay directory on cm3 device from /boot. 
Copy the overlay files from chroot/boot/dtbs/4.19.40-v7+/overlays in /boot/overlays. 
Copy the chroot/boot/dtbs/4.19.40-v7+/*.dtb on the device in /boot

Copy the panel overlay (currently lcd_driver.dtbo) from panel directory in /boot/overlays.
Copy System.map-4.19... config-4.19... vmlinuz-4.19... and kernel7.img from chroot/boot on the device in /boot.

Copy the modules directory from chroot/lib/modules/4.19.40-v7+ on the device in /lib/modules/4.19.40-v7+

By the way, 40 is what I have here, probably now it's higher.

Copy the lcd_driver.ko from the panel directory, on the device in /lib/modules/4.19.40-v7+

Copy config.txt I povided in kernel directory, over the one existing on the device in /boot

Reboot the device. Probably it won't work for the first time, you'll have to log on it and issue a `sudo depmod -a`, then reboot.

### The 'touch' part

There is a goodix driver that is already in the kernel, which I used. I only had to deal with the overlay to make it work with the panel.
If we need some customization for the touch driver, here is a script that can generate a 'firmware' for the driver: https://github.com/fluidvoice/gt9xx

### Hardware stuff

0 and 1 pins need to be connected as they were, that's 'standard' stuff.

I changed things a little, 2 is here connected to the reset for display and 7 for touch reset. I used 6 for touch INT. 
Those can be changed, of course, but if changed, the overlay must be changed.
