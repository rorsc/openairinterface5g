# Slicing scenario results

Per-phase medians (first 5 samples skipped). Expected = PRB budget from a model of `slicing.c`; a slice using < 85 % of it is demand-limited (*).

## S1

Policies (dedicated/min/max %): slice 1: 10/70/80, slice 2: 10/30/80  
UE → slice: UE1→slice 1, UE2→slice 2, UE3→slice 2

| phase | offered load (UE1..) | slice | expected PRBs/slot | measured PRBs/slot | measured % |
|---|---|---|---|---|---|
| 1 | 250M / 60M / 60M | slice 1 | 74.2 | 71.5 | 67 |
| 1 | 250M / 60M / 60M | slice 2 | 31.8 | 31.2 | 29 |
| 2 | 250M / 0 / 0 | slice 1 | 84.8 | 79.2 | 75 |
| 3 | 15M / 125M / 125M | slice 1 | 74.2 | 5.4 * | 5 |
| 3 | 15M / 125M / 125M | slice 2 | 84.8 | 80.1 | 76 |

| phase | UE | slice | offered | iperf Mbps | MAC goodput Mbps |
|---|---|---|---|---|---|
| 1 | UE1 | slice 1 | 250M | 164.7 | 78.0 |
| 1 | UE2 | slice 2 | 60M | 50.9 | 17.7 |
| 1 | UE3 | slice 2 | 60M | 45.7 | 16.4 |
| 2 | UE1 | slice 1 | 250M | 249.6 | 83.1 |
| 2 | UE2 | slice 2 | 0 | – | 0.0 |
| 2 | UE3 | slice 2 | 0 | – | 0.0 |
| 3 | UE1 | slice 1 | 15M | 15.0 | 5.3 |
| 3 | UE2 | slice 2 | 125M | 125.0 | 43.7 |
| 3 | UE3 | slice 2 | 125M | 125.0 | 43.8 |

## S2

Policies (dedicated/min/max %): slice 1: 10/15/20, slice 2: 10/15/100  
UE → slice: UE1→slice 1, UE2→slice 2, UE3→slice 2

| phase | offered load (UE1..) | slice | expected PRBs/slot | measured PRBs/slot | measured % |
|---|---|---|---|---|---|
| 1 | 100M / 125M / 125M | slice 1 | 21.2 | 20.9 | 20 |
| 1 | 100M / 125M / 125M | slice 2 | 84.8 | 80.9 | 76 |
| 2 | 100M / 0 / 0 | slice 1 | 21.2 | 21.0 | 20 |
| 3 | 0 / 140M / 140M | slice 2 | 95.4 | 95.1 | 90 |

| phase | UE | slice | offered | iperf Mbps | MAC goodput Mbps |
|---|---|---|---|---|---|
| 1 | UE1 | slice 1 | 100M | 57.3 | 22.8 |
| 1 | UE2 | slice 2 | 125M | 122.7 | 44.2 |
| 1 | UE3 | slice 2 | 125M | 122.1 | 44.3 |
| 2 | UE1 | slice 1 | 100M | 64.5 | 22.8 |
| 2 | UE2 | slice 2 | 0 | – | 0.0 |
| 2 | UE3 | slice 2 | 0 | – | 0.0 |
| 3 | UE1 | slice 1 | 0 | – | 0.0 |
| 3 | UE2 | slice 2 | 140M | 140.7 | 50.2 |
| 3 | UE3 | slice 2 | 140M | 142.7 | 50.4 |

## S3

Policies (dedicated/min/max %): slice 1: 0/3/3, slice 2: 0/10/100  
UE → slice: UE1→slice 1, UE2→slice 2, UE3→slice 2

| phase | offered load (UE1..) | slice | expected PRBs/slot | measured PRBs/slot | measured % |
|---|---|---|---|---|---|
| 1 | 15M / 150M / 150M | slice 1 | 3.2 | 3.2 | 3 |
| 1 | 15M / 150M / 150M | slice 2 | 102.8 | 100.5 | 95 |
| 2 | 15M / 0 / 0 | slice 1 | 3.2 | 3.2 | 3 |

| phase | UE | slice | offered | iperf Mbps | MAC goodput Mbps |
|---|---|---|---|---|---|
| 1 | UE1 | slice 1 | 15M | 7.4 | 2.8 |
| 1 | UE2 | slice 2 | 150M | 150.3 | 54.0 |
| 1 | UE3 | slice 2 | 150M | 147.8 | 55.0 |
| 2 | UE1 | slice 1 | 15M | 8.3 | 2.8 |
| 2 | UE2 | slice 2 | 0 | – | 0.0 |
| 2 | UE3 | slice 2 | 0 | – | 0.0 |

## S4

Policies (dedicated/min/max %): slice 1: 0/60/100  
UE → slice: UE1→slice 1, UE2→no slice, UE3→no slice

| phase | offered load (UE1..) | slice | expected PRBs/slot | measured PRBs/slot | measured % |
|---|---|---|---|---|---|
| 1 | 300M / 60M / 60M | slice 1 | 84.8 | 80.8 | 76 |
| 1 | 300M / 60M / 60M | no slice | 21.2 | 22.3 | 21 |
| 2 | 0 / 200M / 200M | no slice | 106.0 | 103.5 | 98 |

| phase | UE | slice | offered | iperf Mbps | MAC goodput Mbps |
|---|---|---|---|---|---|
| 1 | UE1 | slice 1 | 300M | 133.9 | 88.3 |
| 1 | UE2 | no slice | 60M | 31.4 | 12.0 |
| 1 | UE3 | no slice | 60M | 31.6 | 12.1 |
| 2 | UE1 | slice 1 | 0 | – | 0.0 |
| 2 | UE2 | no slice | 200M | 115.9 | 55.2 |
| 2 | UE3 | no slice | 200M | 119.7 | 55.2 |
