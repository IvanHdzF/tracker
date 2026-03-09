# Sierra Wireless HL78XX Sample CMake file

# Copyright (c) 2025 Netfeasa
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(tracker)

target_sources(app PRIVATE 
                src/main.c
                sim_app/gps/gps.c
                sim_app/modem/sim_modem.c
                )

include(${ZEPHYR_BASE}/samples/net/common/common.cmake)

target_include_directories(app PRIVATE include)
