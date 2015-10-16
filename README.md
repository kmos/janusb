#Janusb - Serial Port Plugin for Janus Gateway

*****

Janusb provides a very simple interface to the low level serial port code necessary communicate with Arduino, St MicroControllers and all the IoT.

*****


JanUsb, Why?
================
Why not! 


How To Use
==========

Using janusb is pretty easy because it is pretty basic. The main goal of janusb is to send/recive JSON messages from web application  to microcontroller (and vice-versa) without the knowledge of hardware.

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

#### Html:

The "janusb" project allows the communication between STM32 microcontroller and a web page.
janusb is able to command the STM32 (on/off leds) and to show the data acquired from it (temperature and accelerometer).
The communications is based on JSON message exchange and it is made possible by Janus WebRTC Gateway © Meetecho 2015.

#### Stm32f4Discovery:

You can use the example in Microcontroller dir to flash the Stm32f4 Microcontroller and test the plugin.

#### Arduino:


To Configure
------------

