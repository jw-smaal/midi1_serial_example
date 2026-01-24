#!/bin/bash
#west build -b qemu_x86 tests/midi1_parser -t run
west build -b qemu_cortex_m3 midi1_parser -t run
#west build -b qemu_cortex_m0 midi1_parser -t run
