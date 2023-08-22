#!/bin/bash
sudo rm -f --interactive=never /home/pi/running*
source /home/pi/venv/bin/activate
/home/pi/venv/bin/python /home/pi/startup.py 2>> /home/pi/log 1>> /home/pi/log
