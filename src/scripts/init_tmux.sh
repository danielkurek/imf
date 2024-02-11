#!/bin/sh

PORT="$1"
if [ ! -c "$1" ]; then
	echo "$1 is not a file. Exiting..."
	sleep 5
	exit 1
fi

source "$HOME/esp/esp-idf/export.sh"

export PORT="$1"
