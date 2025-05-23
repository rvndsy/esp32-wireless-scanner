#!/bin/bash

./esp-idf/tools/idf.py build &&
./esp-idf/tools/idf.py -p /dev/ttyUSB0 flash &&
./esp-idf/tools/idf.py monitor

