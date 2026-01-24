# @file CMakeLists.txt for MIDI1.0 driver app 
#
# @author Jan-Willem Smaal <usenet@gispen.org> 
# @date 20260119 
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(midi1_serial)

# Application logic 
FILE(GLOB app_sources src/*.c)

# Hardware and driver logic 
FILE(GLOB driver_sources drivers/*.c)

target_include_directories(app PRIVATE
	drivers
)

target_sources(app PRIVATE 
	${app_sources}
	${driver_sources}
)
