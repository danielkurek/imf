#!/bin/sh

set -eu

for FILE in $(find logs/ -maxdepth 1 -name "*.txt"); do
    python log_parser.py "$FILE" "logs/$( basename -s ".txt" $FILE).json"
    python log_parser.py -p "colors" "$FILE" "logs/$( basename -s ".txt" $FILE)-colors.json"
done