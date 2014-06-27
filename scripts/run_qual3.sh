#!/bin/bash
lcdshow -c
#lcdshow -noblink
#lcdshow -setbars 12
echo "Running: Gate -> Travel1 -> Travel2 -> Bin"
lcdshow -s
lcdshow -t "Get ready..."
echo "Get ready..."
#lcdshow -redgreen
sleep 2
#lcdshow -noblink
#lcdshow -setbars 240
lcdshow -t "Running..."

lcdshow -unsafe
source scripts/setenv
/home/tortuga/firereset
python tools/acs/src/main.py -c data/config/trans_qual2.yml -s ram.ai.course.Gate



#lcdshow -noblink
#lcdshow -setbars 12
lcdshow -t "Stopped"