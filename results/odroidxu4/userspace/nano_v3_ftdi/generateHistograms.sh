#!/bin/bash

./generateHistogram.py                         digitalWriteEnd_to_interrupt.gpd   digitalWriteEnd_to_interrupt_histogram   >/dev/null
./generateHistogram.py                         digitalWriteStart_to_interrupt.gpd digitalWriteStart_to_interrupt_histogram >/dev/null
./generateHistogram.py                         endWrite_to_endRead.gpd            endWrite_to_endRead_histogram            >/dev/null
./generateHistogram.py -w 10.0 --bin-start 5.0 startWrite_to_endRead.gpd          startWrite_to_endRead_histogram          >/dev/null
./generateHistogram.py                         startWrite_to_endWrite.gpd         startWrite_to_endWrite_histogram         >/dev/null
./generateHistogram.py -w 10.0 --bin-start 5.0 startWrite_to_interrupt.gpd        startWrite_to_interrupt_histogram        >/dev/null

./generatePngs.gpi
