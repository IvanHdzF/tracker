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
                components/peripherals/gpio.c
                components/ble_stack/gatt_db/gatt_client.c
                components/ble_stack/gatt_db/gatt_db.c
                components/ble_stack/gatt_db/service_cts.c
                components/ble_stack/gatt_db/service_di.c
                components/ble_stack/gap/disc.c
                components/ble_stack/gap/adv.c
                components/ble_stack/gap/conn.c
                components/ble_stack/gap/scan.c
                components/ble_stack/user_api.c
                components/ble_stack/util/power.c
                components/ble_stack/l2capcoc/coc_transport.c
                components/util/mutex.c
                components/util/mem_debug.c
                components/util/crc16.c
                components/util/context.c
            )

include(${ZEPHYR_BASE}/samples/net/common/common.cmake)

# Add public include dir
target_include_directories(app PUBLIC include)

# Add private include dir
target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/components/
)
