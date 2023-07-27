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
* Hercules Console Mk4
* Hercules DJ 4Set (thanks watchingJu!)

Things that are reported to work but I can't test:

* Hercules DJ Control MP3 LE / Glow
* Hercules Console Mk2 (See comment in the next section)

Things I won't support:

* Hercules Console Mk2 (already works with HID code in Mixxx)
* Hercules RMX (already works with HID code in Mixxx)


If you have a Hercules device that doesn't "just work" with Mixxx,
send me an email, there's a chance I can support it with this driver.


How To Get
----------

Check the [Github Project](https://github.com/nealey/hdjd) for the canonical source repo.


How to Build
------------

Install Dependencies:

* Debian / Ubuntu

 apt-get install libusb-1.0.0-dev libasound2-dev

* Rocky / CentOS:

 yum install libusb-devel alsa-lib-devel

Build hddj:

 make

How To Run
----------

Currently, the driver must be started after you plug in a device.

If you haven't done so, build it by typing `make` under the source dir.
Then, just run it by typing `./hdjd` and it will tell you what it found.

Also make sure your user can read and write to raw USB devices.
See [this Mixxx wiki page](https://mixxx.org/wiki/doku.php/troubleshooting#hid_and_usb_bulk_controllers_on_gnu_linux) for how to do this with udev-based systems (almost every Linux distribution).


If you are trying to add support for another controller and try to debug the messages sent and received, you can build 
it in debug mode
by providing `make` the `DEBUG=1` option:

    $ make clean all DEBUG=1


Current Issues
--------------

Recent versions of Mixxx may not be setting up the USB system correctly,
resulting in your disk running out of space because
`syslog` and the kernel log have millions of messages.
If this happens, make sure to disable the "USB Device" for your controller in Mixxx.


If you find a problem, please, please, pretty-please,
email me <neale@woozle.org>.
Posting on the Mixxx Community Forums is fine too,
maybe someone there can help you,
but I don't check the forums very often.
Sorry.


License
-------

[MIT](LICENSE.md)


Thanks
------

Thanks to Guillemot (makers of the Hercules controllers) for sending me
a whole bunch of controllers so I could make a better driver.

Thanks to the Mixxx project for connecting me with Guillemot,
and for providing something interesting to use this driver ;-)

Thanks to JosepMaJAZ for many stability improvements,
code cleanup,
better error checking,
and testing against the 4-Mx.


Contact Me
----------

Neale Pickett <neale@woozle.org>
