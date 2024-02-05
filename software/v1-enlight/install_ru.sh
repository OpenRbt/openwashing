#!/bin/bash

CUR_DIR=$(pwd)
echo "PLEASE COPY IT TO YOUR FLASH DRIVE AND RUN FROM THE FLASHDRIVE"
echo "NEVER RUN IT FROM THE REPO DIRECTORY"
sleep 3
echo "The script is run from $CUR_DIR"

echo "THIS SCRIPT IS FOR RASPBIAN ONLY! DO NOT USE ON OTHER LINUX"
sleep 5

echo "INSTALLING WIRING PI"
sudo apt-get install -y libi2c-dev


mkdir ~/temp
cd ~/temp
git clone https://github.com/WiringPi/WiringPi
cd WiringPi
sudo ./build
cd $CUR_DIR

echo "CREATING wash directory"
sleep 1
mkdir ~/wash

echo "COPYING cashless payment binaries"
sleep 1
cp -r uic/. ~/wash

echo "CLONING openwashing"
sleep 1
cd ~
git clone https://github.com/OpenRbt/openwashing

echo "INSTALLING REQUIRED PACKAGES TO BUILD OPENWASHING"
sleep 1
cd openwashing/software/v1-enlight
./prepare_pi.sh

echo "LET'S make the project"
sleep 1
make

echo "COPYING firmware.exe binary"
sleep 1
cp firmware.exe ~/wash/firmware.exe

echo "COPYING lua firmware"
sleep 1
mkdir ~/wash/firmware
cp -r ~/openwashing/software/v1-enlight/samples/wash/. ~/wash/firmware

echo "COPYING update script and install updater dependencies"
sleep 1
python -m pip install -r ./updater/dependencies.txt
cp ./updater/update.py ~/update.py

echo "COPYING scripts which prepare AUTORUN feature"
echo "it's ok to see errors here"
sleep 1
cp install/prepare_pi.sh ~/wash/
cp install/update_system.sh ~/wash/
cd ~/wash
./prepare_pi.sh

echo "THAT'S ALL FOLKS, ENJOY! NEXT TIME SCRIPT SHOULD ASK OF DESIRED PROGRAMS"
