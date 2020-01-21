## Setup for development

### Kernel source

Just clone the rpi-4.19.y branch from here: https://github.com/raspberrypi/linux
It might work with older kernel versions but I've seen reports of ugly bugs in them which were solved (such as a serious memory leak in the vc4 driver), so we better stick with this version (or later).
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
It works for raspberry pi, but for compute module avoid the booting from the network part, I managed to brick the module that way (partially, with some effort it could still be flashed).
Anyway, the chroot setting there is not described well, I had to look elsewhere for the info, works nicely with raspberry pi 3, but not with the compute module on dev board.

There is also info on kernel building here: https://www.raspberrypi.org/documentation/linux/kernel/building.md

### Flashing the compute module

Rpiboot is here:
https://github.com/raspberrypi/usbboot

It will be needed for the initial flashing of the compute module. I downloaded Raspbian from here: https://www.raspberrypi.org/downloads/raspbian/
I got the 'with desktop' variant but not the full one, I installed the additional software I needed, the full one is over 4 GBytes so it does not fit in cm3.

Don't forget that immediately after you flash the device, to create an empty 'ssh' file in /boot, it will allow you to log in with ssh (otherwise it's disabled).

Some more info is here: https://www.raspberrypi.org/documentation/hardware/computemodule/cm-emmc-flashing.md

### Firmware

Firmware, if needed, is here: https://github.com/raspberrypi/firmware
I tested with both the current firmware and the 'next' branch, fkms does not work with any of them (yet?).
kms works with both, because it's a 'full' driver.

For the 'official' panel they recommend installing the 'blob' bin, here: https://www.raspberrypi.org/documentation/hardware/computemodule/cmio-display.md
I think the goal is to change the default behaviour of 0 and 1 pins, which can also be done in config.txt. Here is some more info about that: https://www.raspberrypi.org/documentation/configuration/pin-configuration.md
For raspbian buster I didn't need to install it.

### Compiling the panel driver and dts

Note: This info is obsolete, the dkms solution will be used, but I leave it here just in case.

Just run the two sh scripts in the panel directory (after having the kernel compiled, of course) and they should generate the ko file (the module) and the dtbo file (the overlay).

### Installing the driver

Note: This info is obsolete, except the overlay file copying and the config.txt, the dkms solution will be used, but I leave it here just in case.

Remove the contents of overlay directory on cm3 device from /boot.
Copy the overlay files from chroot/boot/dtbs/4.19.95-v7+/overlays in /boot/overlays.
Copy the chroot/boot/dtbs/4.19.95-v7+/*.dtb on the device in /boot

Copy the panel overlay (currently HG_LTP08_lcd_driver.dtbo) from panel directory in /boot/overlays.
This is actually the only thing you have to do if the 'official' kernel is used and dkms, and the changing of the config.txt file.

Copy System.map-4.19... config-4.19... vmlinuz-4.19... and kernel7.img from chroot/boot on the device in /boot.
Note: At least for Buster, it boots fine without System.map-4.19..., config-4.19... and vmlinuz-4.19... files, of course, kernel7.img is still needed.

Copy the modules directory from chroot/lib/modules/4.19.95-v7+ on the device in /lib/modules/4.19.95-v7+

By the way, 95 is the latest tested, it could be higher.

Copy the lcd_driver.ko from the panel directory, on the device in /lib/modules/4.19.95-v7+

Copy config.txt I povided in kernel directory, over the one existing on the device in /boot

Reboot the device. Probably it won't work for the first time, you'll have to log on it and issue a `sudo depmod -a`, then reboot.

### The 'touch' part

There is a goodix driver that is already in the kernel, which I used. I only had to deal with the overlay to make it work with the panel.
If we need some customization for the touch driver, here is a script that can generate a 'firmware' for the driver: https://github.com/fluidvoice/gt9xx

### Hardware stuff

0 and 1 pins need to be connected as they were, that's 'standard' stuff.

I changed things a little, 13 is here connected to the reset for display and 16 for touch reset. I used 17 for touch INT.
Those can be changed, of course, but if changed, the overlay and perhaps the device source (reset can be overriden in overlay, but I guess making the chosen variant as a default wouldn't hurt) must be changed.

### Configuration

There are things that can be changed in the overlay - the used pins. Some general info about device trees and overlays, here:

https://www.raspberrypi.org/documentation/configuration/device-tree.md

Some things can be changed for the touch part by using the 'firmware' mentioned above.
There are of course some things that can be changed/added either in kernel start line or config.txt or other configuration files, but there is one thing that I found during tests that is not strwaightforward.
By default, the desktop starts in portrait mode. If you want to rotate it in landscape mode, do not use the usual method of rotating the screen in config.txt, it might prevent booting up.
After the X desktop is started, it can be rotated with these commands:

Screen:

`xrandr --output DSI-1 --rotate left`

This will rotate only the screen, but not the touch input. To also rotate the touch input, use:

`xinput --set-prop 6 132 0 -1 1 1 0 0 0 0 1`

### DKMS

I managed to make dkms working, for now installation is 'manual' (later some packaging method could be used):

- make a /usr/src/HG_LTP08-1.0 directory
- copy there the Makefile, dkms.conf and lcd_driver.c files

- add the module to dkms with: `dkms add -m HG_LTP08 -v 1.0`
- build it with: `dkms build -m HG_LTP08 -v 1.0'
- install it with: `dkms install -m HG_LTP08 -v 1.0`

After this, simply rebooting should make the driver work. It also needs the lines in config.txt and the dtbo file mentioned above to be installed.

The module can be removed from dkms with:
`dkms remove -m HG_LTP08 -v 1.0`

Note: a kernel version can be specified for dkms with -k if building/installing for some other kernel version.
