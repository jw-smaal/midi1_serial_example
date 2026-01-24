#!/bin/bash
west build -b frdm_mcxc242 -p auto
west build -t ram_report > ram_report.txt
west build -t rom_report > rom_report.txt
#west build -t ram_plot
