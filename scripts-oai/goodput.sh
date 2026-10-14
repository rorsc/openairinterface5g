#!/bin/bash
# usage: ./goodput.sh <label> <seconds>
T=${1:-run}; DUR=${2:-50}
STATS=~/repos/hackaton2026/openairinterface5g/build/nrMAC_stats.log
OUT=/tmp/goodput_$T.csv
echo "t,rnti,dl_mbps,ul_mbps" > $OUT
for t in $(seq 1 $DUR); do
  # line format: "UE f5c5: LCID 1,2,4, goodput DL   12.34 UL    0.01 Mbps"
  grep goodput $STATS | awk -v t=$t '{gsub(":","",$2); print t","$2","$7","$9}' >> $OUT
  sleep 1
done
# average per UE, skipping the first 5 s while the averages settle
awk -F, 'NR>1 && $1>5 {dl[$2]+=$3; ul[$2]+=$4; n[$2]++}
         END {for (r in dl) printf "UE %s  DL %.1f  UL %.1f Mbps (MAC goodput)\n", r, dl[r]/n[r], ul[r]/n[r]}' $OUT