Hercules DJ Controller Driver for Linux
=============================

This is a Linux userspace driver for some Hercules DJ controllers.

Originally this was a part of Mixxx,
but in order to issue more timely updates
(not bound to the Mixxx release schedule),
it is now a standalone project.
That means you can use it with any version of Mixxx,
without needing to recompile anything other than this driver.

Since it runs in userspace,
you don't need to keep modifying your kernel to keep it working.
And if the driver crashes,
the rest of your system keeps running.
But please tell me if it crashes!


Supported Controllers
---------------------

The driver currently supports:

* Hercules DJ Control MP3 e2
* Hercules DJ Control Steel
* Hercules DJ Console 4-Mx

Support is planned for:

* Hercules Console Mk4 (if I can find one cheap enough)

Things I won't support:

* Hercules DJ 4Set (the one Guillemot sent me doesn't even work in Windows)
* Hercules Console Mk2 (already works with HID code in Mixxx)
* Hercules RMX (already works with HID code in Mixxx)


If you have a Hercules device that doesn't "just work" with Mixxx,
send me an email, there's a chance I can support it with this driver.


How To Get
----------

Check the [Github Project](https://github.com/nealey/hdjd) for the canonical source repo.


How To Run
----------

Also make sure your user can read and write to raw USB devices.
See [this Mixxx wiki page](http://mixxx.org/wiki/doku.php/hercules_dj_control_mp3_e2#usb_hid) for how to do this with udev-based systems (almost every Linux distribution).

Currently, the driver must be started after you plug in a device.
Just run `hdjd` and it will tell you what it found.


Current Issues
--------------

The driver is pretty crappy right now.
If you email me, I'm a lot more likely to fix things.
Even if you just say "hi, I'm using your Hercules driver".

* Driver must be launched after device is plugged in
* It locks up and crashes sometimes

If you find a problem, please, please, pretty-please,
email me <neale@woozle.org>.
Posting on the Mixxx Community Forums is fine too,
maybe someone there can help you,
but I don't check the forums very often.
Sorry.


Thanks
------

Thanks to Guillemot (makers of the Hercules controllers) for sending me
a whole bunch of controllers so I could make a better driver.

Thanks to the Mixxx project for connecting me with Guillemot,
and for providing something interesting to use this driver ;-)

JosepMaJAZ contributed support for 4-Mx,
and found a bug having to do with controller messages not getting read.
This greatly improved stability.


Contact Me
----------

Neale Pickett <neale@woozle.org>
