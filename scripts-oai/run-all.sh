#!/bin/bash
# Run all slicing scenarios one after the other, then analyze them.
# usage: sudo ./run-all.sh [S1 S2 ...]      (default: S1 S2 S3 S4)
cd "$(dirname "$0")"
for s in ${@:-S1 S2 S3 S4}; do
  ./run-scenario.sh $s || echo "scenario $s failed"
done
sudo -u "${SUDO_USER:-root}" python3 analyze.py
