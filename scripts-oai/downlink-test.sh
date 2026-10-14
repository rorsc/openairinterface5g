sudo pkill iperf3
for i in 1 2 3; do
  ip=$(sudo ip netns exec ue$i ip -4 -br addr show oaitun_ue1 | awk '{print $3}' | cut -d/ -f1)
  echo "ue$i: $ip"
  sudo ip netns exec ue$i iperf3 -s -B $ip -D --logfile /tmp/dl_ue$i.log
  docker exec -d oai-ext-dn iperf3 -c $ip -u -b 300M -t 60
done
sleep 65
for i in 1 2 3; do echo -n "ue$i DL: "; grep receiver /tmp/dl_ue$i.log | tail -1; done