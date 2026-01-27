#!/bin/bash
rm -rf build/
#west build -b frdm_mcxc242 -p auto -- -DEXTRA_DTC_OVERLAY_FILE="boards/sh1106_display.overlay"
#west build -b frdm_mcxc242 -p auto 
west build -b frdm_mcxw71 -p auto 
west build -t ram_report > ram_report.txt
west build -t rom_report > rom_report.txt
#west build -t rom_plot
