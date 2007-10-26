#!/bin/bash
# requirements: flac, time, awk, stat, bc

# change location of binaries if necessary
flac="flac";
wavinfo="./wavinfo";

enc="$flac -f $1 -P 0--no-seektable -o flac-";
dec="$flac -t flac-";

wavsize=$($wavinfo $1 | awk '/Data Size/ {print $3}');
playtime=$($wavinfo $1 | awk '/Playing Time/ {print $3}');
# not-as-accurate alternative for 16-bit 44100Hz stereo:
#wavsize=$(echo -e "$(stat -c %s $1) - 44" | bc);
#playtime=$(echo -e "scale=2\n$wavsize / (44100*4)" | bc)s;

echo "";
echo "flac:";
echo "";
echo "level   enc time  speed      bytes    ratio   kbps    dec time  speed";
echo "-----  ---------  -----  ----------   -----  ------  ---------  -----";
for i in $(seq 0 8) ; do
    i0=$(printf "%02d" $i);
    time=$((time -p ${enc}$i0.flac -$i) 2>&1 | awk '/user/ {print $2}');
    espeed=$(echo -e "scale=0\n$playtime/$time" | bc);
    csize=$(stat -c %s flac-$i0.flac);
    ratio=$(echo -e "scale=3\n$csize/$wavsize" | bc);
    kbps=$(echo -e "scale=1\n$csize*8/($playtime*1000)" | bc);
    dtime=$((time -p ${dec}$i0.flac) 2>&1 | awk '/user/ {print $2}');
    dspeed=$(echo -e "scale=0\n$playtime/$dtime" | bc);
    printf "%3d    %8ss  %4sx  %10s   %5s  %6s  %8ss  %4sx\n" $i $time $espeed $csize 0$ratio $kbps $dtime $dspeed;
done;
