# Tracker sample made by
# Iván Hernández

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(tracker)

target_sources(app PRIVATE 
                src/main.c
                sim_app/gps/gps.c
                sim_app/modem/sim_modem.c
                coap_app/coap.c
            )

include(${ZEPHYR_BASE}/samples/net/common/common.cmake)

# Add public include dir
target_include_directories(app PUBLIC include)

# Add private include dir
target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/components/
)
