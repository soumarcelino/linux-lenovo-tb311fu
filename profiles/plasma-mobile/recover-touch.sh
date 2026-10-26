#!/bin/bash
# Manual recovery only; briefly disconnects touch, preserves the GUI and SSH.
set -euo pipefail
[[ $EUID == 0 && $(uname -r) == 6.6.142-pmos-minimal+ ]]
grep -aq 'mediatek,MT6768' /proc/device-tree/compatible
modprobe -r hx83102e
modprobe hx83102e
echo 0 > /proc/game_mode
cat /proc/game_mode
