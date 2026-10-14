#!/bin/bash
# usage: ./max-test.sh A   (then B, C after changing gnb.conf and restarting)
T=${1:-A}
DUR=60
sudo pkill iperf3; docker exec oai-ext-dn pkill iperf3 2>/dev/null

for i in 1 2 3; do
  ip=$(sudo ip netns exec ue$i ip -4 -br addr show oaitun_ue1 | awk '{print $3}' | cut -d/ -f1)
  echo "ue$i: $ip"
  sudo ip netns exec ue$i iperf3 -s -B $ip -1 -D --logfile /tmp/max_${T}_ue$i.log
  #docker exec -d oai-ext-dn iperf3 -c $ip -u -b 15M -t $DUR
  b=$([ $i = 1 ] && echo 15M || echo 150M)
  docker exec -d oai-ext-dn iperf3 -c $ip -u -b $b -t $DUR
done

./goodput.sh $T $((DUR - 5)) &

sleep $((DUR + 5))

rate() { grep receiver /tmp/max_${T}_ue$1.log | tail -1 | grep -oP '[\d.]+(?= Mbits/sec)'; }
r1=$(rate 1); r2=$(rate 2); r3=$(rate 3)
echo "Test $T  ue1=$r1  ue2=$r2  ue3=$r3 Mbps"
awk -v a=$r1 -v b=$r2 -v c=$r3 'BEGIN { s1=a; s2=b+c; t=s1+s2;
  printf "slice1 %.1f Mbps (%.0f%%)   slice2 %.1f Mbps (%.0f%%)\n", s1, 100*s1/t, s2, 100*s2/t }'
grep "DL slice" ~/repos/hackaton2026/openairinterface5g/build/gnb.log | tail -3