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

Note: This info is obsolete, except the overlay file copying and the config.txt, now the dkms solution will be used, but I leave it here just in case.

Remove the contents of overlay directory on cm3 device from /boot.
Copy the overlay files from chroot/boot/dtbs/4.19.95-v7+/overlays in /boot/overlays.
Copy the chroot/boot/dtbs/4.19.95-v7+/*.dtb on the device in /boot

Copy the panel overlay (currently htltp08_lcd_driver.dtbo) from panel directory in /boot/overlays.
This and copying config.txt is actually the only thing you have to do if the 'official' kernel is used with dkms.

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

This will rotate only the screen, but not the touch input. 

NOTE: This info is obsolete, it's kept here just for the alternative:
To also rotate the touch input, use:

`xinput --set-prop 6 138 0 -1 1 1 0 0 0 0 1`

For some reason, the 'prop' can change, so check its number with xinput to be sure.

Updated info: Now there will be a rule installed in /etc/udev/rules.d/99-goodix.rules that will rotate the input automatically without the above manual setting.


I don't think if this can help or not, but adding in cmdline.txt the boot option:

`video=DSI-1:800x1280,rotate=90`

will rotate the console, but it does not affect the X server, so it doesn't help with the screen rotation in graphical mode.

### DKMS


###### Manual installation

I managed to make dkms working:

- make a /usr/src/htltp08-1.0 directory
- copy there the Makefile, dkms.conf and lcd_driver.c files

- add the module to dkms with: `dkms add -m htltp08 -v 1.0`
- build it with: `dkms build -m htltp08 -v 1.0`
- install it with: `dkms install -m htltp08 -v 1.0`

Apparently only 'install' might be needed.

After this, simply rebooting should make the driver work. It also needs the lines in config.txt and the dtbo file mentioned above to be installed.

The module can be uninstalled with:
`dkms uninstall -m htltp08 -v 1.0`
and removed with (no need to uninstall it first):
`dkms remove htltp08/1.0 --all`


Note: a kernel version can be specified for dkms with -k if building/installing for some other kernel version.

###### Using a package

Building a deb package:

Just run the `build_deb.sh` script.
A deb file will be created, that should be copied on the device.
Note: Some dependencies might be missing, for now I set only dkms and kernel headers.

Installing on the device:

`apt install ./hgltp08_1.0_all.deb`

Removing:

`apt remove hgltp08`


### Notes about timings

I couldn't drive the panel at 60 Hz no matter what I tried. It seems to almost work, but from time to time the screen shifts (usually partially) sideways (downwards for the rotated case). It is noticeable and annoying.

The safe frequency that seems to work without issues seems to be 50 Hz.

That refresh frequency is available in several ways:

- one that I prefered before switching to the new panel was to adjust the front porch and back porch for horizontal timings
- also the clock can be modified, I'm currently investigating if this is a better option

The timings do not stay as set in the panel driver. The kms driver adjusts them.
There aren't available clocks for any value you set, in fact there are several clock values available and depeding on what one sets, the next higher value available is chosen.
The kms driver then adjusts the timings to obtain a refresh rate as if the clock was at the specified value.
The way it works is for example by enlarging the front porch setting (that's why in many cases I set it to zero, it gets bigger than zero with the new clock value selected by the driver).

### Compiling with the 5.10.y

It looks like somewhere between kernel 4.19.127-v7+ and 5.4,83-v7+ they broke the vc4 video driver, the dsi part.
The bug may be related with the issue mentioned here: https://www.raspberrypi.org/forums/viewtopic.php?f=98&t=282974&sid=891de7d58678d73bbd70e4616098398e&start=75 (see 6by9 comment, "I've just found an error in the register definitions").

Anyway, I tested and the driver again works with 5.10.y kernel. I needed to do some changes in order to have it compiled, but still it doesn't compile 'out of the box'.

I'll try to describe here what it takes to have it compiled and installed with the new kernel.

First, install the RaspberryOS image from Jan 2021. With this one, the panel driver won't work. It's a good idea to uninstall first the panel driver before updating the kernel.
Have the 5.10.y kernel installed by issuing `rpi-update`.

Unfortunately, this won't have the panel driver working, because:
- for now the kernel headers for 5.10.y are not in the repository, installing those will install the old version
- they broke dkms

The next step would be to install the kernel sources for 5.10.y (for the headers):
`git clone --depth=1 --branch rpi-5.10.y https://github.com/raspberrypi/linux`

I do this in /home/pi, so the sources will be in /home/pi/linux.

Be sure to install the headers in from the repo, to ensure they won't interfere later with the headers manually installed:

`apt-get install raspberrypi-kernel-headers`

This will install - for now - the headers for the old kernel.

Now, configure the kernel sources (in /home/pi/linux):

`KERNEL=kernel7;make bcm2709_defconfig`

In order to do that, one might need to install additional packages, like flex and bison.

Install the headers:

`make headers_install`

This one is for dkms:

In /lib/modules/5.10... directories, issue a:

`ln -s /home/pi/linux/ build`


In linux/scripts, create a file called module.lds, with contents:

```
/*
 * Common module linker script, always used when linking a module.
 * Archs are free to supply their own linker scripts.  ld will
 * combine them automatically.
 */
SECTIONS {
        /DISCARD/ : {
                *(.discard)
                *(.discard.*)
        }

        __ksymtab               0 : { *(SORT(___ksymtab+*)) }
        __ksymtab_gpl           0 : { *(SORT(___ksymtab_gpl+*)) }
        __ksymtab_unused        0 : { *(SORT(___ksymtab_unused+*)) }
        __ksymtab_unused_gpl    0 : { *(SORT(___ksymtab_unused_gpl+*)) }
        __ksymtab_gpl_future    0 : { *(SORT(___ksymtab_gpl_future+*)) }
        __kcrctab               0 : { *(SORT(___kcrctab+*)) }
        __kcrctab_gpl           0 : { *(SORT(___kcrctab_gpl+*)) }
        __kcrctab_unused        0 : { *(SORT(___kcrctab_unused+*)) }
        __kcrctab_unused_gpl    0 : { *(SORT(___kcrctab_unused_gpl+*)) }
        __kcrctab_gpl_future    0 : { *(SORT(___kcrctab_gpl_future+*)) }

        .init_array             0 : ALIGN(8) { *(SORT(.init_array.*)) *(.init_array) }

        __jump_table            0 : ALIGN(8) { KEEP(*(__jump_table)) }
}

/* SPDX-License-Identifier: GPL-2.0 */
SECTIONS {
        .plt : { BYTE(0) }
        .init.plt : { BYTE(0) }
}
```

The content is taken from module.lds.S with the include replaced with the contents of the included file.

For more on this, see: https://bugs.launchpad.net/ubuntu/+source/linux/+bug/1906131 and https://forum.armbian.com/topic/16670-plt-sections-missing-using-out-of-tree-module-on-armbian-20116-buster-orangepizero/

Install libssl-dev package. After that, issue a
`make prepare` in the linux directory.


After this, installing the panel driver should work:

`apt install ./hgltp08_1.0_all.deb`

There is a chance that they also fixed the issue that occured with 60 Hz in the older kernels, I'll try to see if they did after I'll receive the new CM3+ with more flash memory.

For 5.10 I also had to enable i2c in config.txt, otherwise touch won't work. One of the uncommented lines did the trick.


### Error workarounds

It seems that the vc4 driver has some issues and in some circumstances it fails to send DSI commands. In such cases it appears to 'restart' something.
The panel driver tries to detect such situations and attempts to retry sending the commands (three times at this moment, but that's easy to change) after waiting a bit (time interval is currently 50 ms, but it's easy to be changed as well).

If the recovery procedure does not work, now the driver exposes `/proc/hgltp08` read only file that contains a single char, '0' if no error was detected, '1' if an error was detected. The info can be used to recover the issue by a script that watches it (for example by rebooting).
