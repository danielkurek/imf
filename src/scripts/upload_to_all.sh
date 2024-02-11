#!/bin/sh

source "$HOME/esp/esp-idf/export.sh"

DEVICES="$(find /dev/ -name "ttyUSB*" -o -name "ttyACM*")"

for FILE in $DEVICES; do
    idf.py flash -p "$FILE"
done