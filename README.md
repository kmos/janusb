#Janusb - Serial Port Plugin for Janus Gateway

*****

Janusb provides a very simple interface to the low level serial port code necessary to program Arduino chipsets, St MicroControllers and all the IoT.

*****


Janususb, Why?
================
Why not! 


How To Use
==========


To Install
----------

Once you have installed all the dependencies and , get the code:

        git clone https://github.com/kmos/janusb
        cd janusb

Then just use:

        sh autogen.sh

to generate the configure file. After that, configure and compile as
usual to start the whole compilation process:

        ./configure --prefix=/opt/janus
        make
        make install

To also automatically install the default configuration files to use,
also do a:

        make config

after the installation of the plugin, you need to put the sources in html directory in your webserver dir.

To Use
------


