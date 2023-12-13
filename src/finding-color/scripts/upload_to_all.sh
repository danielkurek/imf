#!/bin/sh

source "$HOME/esp/esp-idf/export.sh"

for FILE in /dev/ttyUSB*; do
    idf.py flash -p "$FILE"
done