idf_component_register(SRCS "app_main.c" "net.c"
                       INCLUDE_DIRS "."
                       REQUIRES sedona esp_netif esp_eth esp_wifi esp_event nvs_flash
                                esp_timer esp_system)

# LittleFS partition image from build/esp32/data (kits.scode, app.sab, svm.conf)
set(SEDONA_DATA "${CMAKE_SOURCE_DIR}/../../../build/esp32/data")
if(NOT EXISTS "${SEDONA_DATA}/kits.scode")
  message(FATAL_ERROR "${SEDONA_DATA}/kits.scode is missing: run './spro build esp32' first")
endif()
littlefs_create_partition_image(storage "${SEDONA_DATA}" FLASH_IN_PROJECT)
