#!/bin/sh

set -eu

PORT="$1"
if [ ! -f "$1" ]; then
	echo "$1 is not a file. Exiting..."
	sleep 2
	exit 1
fi

source "$HOME/esp/esp-idf/export.sh"

export $PORT
