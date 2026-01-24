cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(midi1_parser_tests)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers
)

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/midi1_serial.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/note.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_midi1_parser.c
)


