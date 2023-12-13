#!/bin/sh

set -eu

SCRIPT_DIR="$( dirname -- "$0" )"

tmux new-session -d -s esp-idf
tmux bind -n C-x setw synchronize-panes

FIRST=1

for FILE in /dev/ttyUSB*; do
	if [ "$FIRST" -ne 1 ]; then
		tmux split-window
		FIRST=0
	fi
	tmux send "source \"$SCRIPT_DIR\"/init_tmux.sh \"$FILE\"" ENTER
	tmux send "idf.py flash -p \"$FILE\"" ENTER
done

tmux select-layout tiled
tmux a
