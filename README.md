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

Once you have installed all the dependencies and janus-gateway (https://github.com/meetecho/janus-gateway/), get the code:

        git clone https://github.com/kmos/janusb
        cd janusb

Then just use:

        sh autogen.sh

After that, configure&compile adding the path of janus installation:

        ./configure --prefix=/opt/janus
        make
        make install

To also automatically install the default configuration files to use,
also do a:

        make config

To Use
------

After the installation of the plugin, you need to put the sources in html directory in your webserver dir.




