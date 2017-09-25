#!/bin/bash
docker run -it --rm --device=/dev/ttyUSB0 -v /home/a3ot/Code/quelab-rfid-door-arduino:/sketch quelab-rfid-arduino "$@"
