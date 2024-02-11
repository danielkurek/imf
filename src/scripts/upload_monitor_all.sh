#!/bin/sh

set -eu

SCRIPT_DIR="$( dirname -- "$0" )"

source "$HOME/esp/esp-idf/export.sh"
idf.py build

tmux new-session -d -s esp-idf
tmux bind -n C-x setw synchronize-panes
tmux set pane-border-status top

FIRST=1
DEVICES="$(find /dev/ -name "ttyUSB*" -o -name "ttyACM*")"

for FILE in $DEVICES; do
	if [ "$FIRST" -ne 1 ]; then
		tmux split-window
	fi
	FIRST=0
	tmux select-pane -T "$FILE"
	tmux send "source \"$SCRIPT_DIR\"/init_tmux.sh \"$FILE\"" ENTER
	tmux select-layout tiled
done
tmux setw synchronize-panes on
tmux send "idf.py flash -p \"\$PORT\" && idf.py monitor -p \"\$PORT\"" ENTER

tmux a
