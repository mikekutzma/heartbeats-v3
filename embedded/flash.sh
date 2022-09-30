#!/bin/bash

SKETCH=monitormqtt
FQBN=esp8266:esp8266:d1_mini

# I define the port like this because the serial port jumps around
# every time I plug the device in, so this might need to be changed
# depending on your setup
PORT=`ls /dev/tty.usbserial* | head -n 1`

arduino-cli \
    compile \
    --fqbn $FQBN $SKETCH \
    && \
arduino-cli upload -p $PORT --fqbn $FQBN $SKETCH

echo "Run 'screen $PORT 115200lll' to monitor logs"
