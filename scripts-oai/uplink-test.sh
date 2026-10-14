docker exec oai-ext-dn pkill iperf3
for i in 1 2 3; do docker exec -d oai-ext-dn iperf3 -s -p 520$i; done
sleep 1
for i in 1 2 3; do
  sudo ip netns exec ue$i iperf3 -c 192.168.70.135 -p 520$i -u -b 100M -t 60 --logfile /tmp/ul_ue$i.log &
done
wait
for i in 1 2 3; do echo -n "ue$i UL: "; grep receiver /tmp/ul_ue$i.log | tail -1; done