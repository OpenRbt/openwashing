#!/bin/bash

echo "COPYING player script and install player dependencies"
sleep 1
cd ~/openwashing/software/v1-enlight
python -m pip install -r ./video/requirements.txt
mkdir ~/wash/video
cp ./video/player.py ~/wash/video/player.py