#!/bin/bash

set -xu

OPTIONS1=("-Gmode=major" "-Gmode=KK" "-Gmode=sgd" "-Gmode=ipsep")
OPTIONS2=("-Gmodel=shortpath" "-Gmodel=circuit" "-Gmodel=subset" "-Gmodel=mds")

for FILE in "$@"; do
	FILENAME="$(basename "$FILE")"
	FILENAME="${FILENAME%.*}"
	for OPT1 in "${OPTIONS1[@]}"; do
		for OPT2 in "${OPTIONS2[@]}"; do
			neato -Tpng -o "$FILENAME-neato${OPT1}${OPT2}.png" "${OPT1}" "${OPT2}" -Gmaxiter=100 "$FILE"
		done
	done
	fdp -Tpng -o "$FILENAME-fdp.png" "$FILE"
	montage -geometry "500x500+20+20" -label %t "$FILENAME-*.png" "${FILENAME}_compare.png"
	xdg-open "${FILENAME}_compare.png"
done
