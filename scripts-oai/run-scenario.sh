#!/bin/bash
# Run one slicing scenario end to end: start gNB + UEs (one netns each), offer DL UDP traffic in
# phases, collect logs in scripts-oai/results/<scenario>/. Stops any running gNB/UE/iperf first.
# usage: sudo ./run-scenario.sh S1|S2|S3|S4      (then: python3 analyze.py)
#
# Phase = "<seconds> <ue1 load> <ue2 load> ..." (iperf3 -b value, 0 = no traffic).
# Loads assume ~2.4 Mbit/s per PRB under rfsim and aim at ~1.2-2x the expected share.
set -u
S=${1:-}
case $S in
  S1) GNB=gnb.conf; NUE=3   # S1 10/70/80, S2 10/30/80: min split, max cap, min is not dedicated
      PHASES=("40 250M 60M 60M" "40 250M 0 0" "40 15M 125M 125M") ;;
  S2) GNB=gnb2.conf; NUE=3  # S1 10/15/20, S2 10/15/100: max cap, dedicated of idle slice 1 blocked
      PHASES=("40 100M 125M 125M" "40 100M 0 0" "40 0 140M 140M") ;;
  S3) GNB=gnb3.conf; NUE=3  # S1 0/3/3, S2 0/10/100: ratio finer than 5 %
      PHASES=("40 15M 150M 150M" "40 15M 0 0") ;;
  S4) GNB=gnb4.conf; NUE=3  # only slice 1 0/60/100; UE2/UE3 in no slice: default scheduling
      PHASES=("40 300M 60M 60M" "40 0 200M 200M") ;;
  *) echo "usage: sudo $0 S1|S2|S3|S4"; exit 1 ;;
esac
[ $EUID -eq 0 ] || { echo "run with sudo"; exit 1; }

SCRIPTS=$(cd "$(dirname "$0")" && pwd)
BUILD=$SCRIPTS/../build
CONF=$SCRIPTS/../conf
UECONF=(ue.conf ue2.conf ue3.conf)
OUT=$SCRIPTS/results/$S
rm -rf "$OUT" && mkdir -p "$OUT"

stop_all() {
  pkill iperf3; docker exec oai-ext-dn pkill iperf3
  pkill -INT -f nr-uesoftmodem; sleep 2
  pkill -INT -f nr-softmodem; sleep 4
  pkill -9 -f nr-uesoftmodem; pkill -9 -f nr-softmodem
  [ -n "${SAMPLER:-}" ] && kill $SAMPLER
  [ -d "${OUT:-}" ] && chown -R "${SUDO_USER:-root}" "$OUT/.."
} 2>/dev/null

ue_ip() { ip netns exec ue$1 ip -4 -br addr show oaitun_ue1 2>/dev/null | awk '{print $3}' | cut -d/ -f1; }
has_ip() { [ -n "$(ue_ip $1)" ]; }

wait_for() { # <seconds> <command...>: poll every second until the command succeeds
  local t=$1; shift
  for _ in $(seq $t); do "$@" && return 0; sleep 1; done
  return 1
}

stop_all

# namespaces (and firewalld trust) for every UE of this scenario
for i in $(seq $NUE); do
  ip netns list | grep -qw ue$i || "$SCRIPTS/../tools/scripts/multi-ue.sh" -c$i
  if command -v firewall-cmd >/dev/null && ! firewall-cmd --zone=trusted --query-interface=v-eth$i >/dev/null 2>&1; then
    firewall-cmd --zone=trusted --add-interface=v-eth$i
  fi
done

cd "$BUILD"
rm -f nrMAC_stats.log
echo "[$S] starting gNB with $GNB"
./nr-softmodem -O "$CONF/$GNB" >> "$OUT/gnb.log" 2>&1 &
wait_for 30 grep -q "Received NGSetupResponse" "$OUT/gnb.log" || echo "[$S] warning: no NGSetupResponse yet"

# start UEs one after the other, so CU-UE-ID i in nrMAC_stats.log is UE i
for i in $(seq $NUE); do
  echo "[$S] starting ue$i (${UECONF[i-1]})"
  ip netns exec ue$i ./nr-uesoftmodem -O "$CONF/${UECONF[i-1]}" --rfsimulator.serveraddr 10.20$i.1.100 \
    > "$OUT/ue$i.log" 2>&1 &
  if ! wait_for 90 has_ip $i; then
    echo "[$S] ue$i got no IP, aborting (see $OUT/ue$i.log)"; stop_all; exit 1
  fi
  echo "[$S] ue$i: $(ue_ip $i)"
done
sleep 5

# MAC goodput, sampled every second: epoch,rnti,dl_mbps,ul_mbps
echo "epoch,rnti,dl_mbps,ul_mbps" > "$OUT/goodput.csv"
(while true; do
   grep goodput nrMAC_stats.log 2>/dev/null | awk -v t=$(date +%s) '{gsub(":","",$2); print t","$2","$7","$9}' >> "$OUT/goodput.csv"
   sleep 1
 done) &
SAMPLER=$!

echo "phase,start_epoch,duration,loads" > "$OUT/phases.csv"
for p in "${!PHASES[@]}"; do
  L=(${PHASES[p]}); dur=${L[0]}; n=$((p + 1))
  echo "[$S] phase $n: ${L[*]:1} for ${dur}s"
  echo "$n,$(date +%s),$dur,${L[*]:1}" >> "$OUT/phases.csv"
  echo "### PHASE $n ${L[*]:1}" >> "$OUT/gnb.log"
  for i in $(seq $NUE); do
    [ "${L[i]}" = 0 ] && continue
    ip netns exec ue$i iperf3 -s -1 -B "$(ue_ip $i)" -D --logfile "$OUT/iperf_p${n}_ue$i.log"
  done
  sleep 1
  for i in $(seq $NUE); do
    [ "${L[i]}" = 0 ] && continue
    docker exec -d oai-ext-dn iperf3 -c "$(ue_ip $i)" -u -b "${L[i]}" -t $dur
  done
  sleep $((dur + 3))
  pkill iperf3; docker exec oai-ext-dn pkill iperf3 2>/dev/null
done
echo "### END" >> "$OUT/gnb.log"

cp nrMAC_stats.log "$OUT/" 2>/dev/null
stop_all
echo "[$S] done, results in $OUT"
