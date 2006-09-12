#!/bin/sh

# change location of binaries if necessary
wavinfo="./wavinfo";

wavsize=$($wavinfo $1 | awk '/Data Size/ {print $3}');
playtime=$($wavinfo $1 | awk '/Playing Time/ {print $3}');
# not-as-accurate alternative for 16-bit 44100Hz stereo:
#wavsize=$(echo -e "$(stat -c %s $1) - 44" | bc);
#playtime=$(echo -e "scale=2\n$wavsize / (44100*4)" | bc)s;

echo "$1";
echo "playing time: ${playtime}s";
echo "original audio size: $wavsize";

flac-test.sh $1
flake-test.sh $1
