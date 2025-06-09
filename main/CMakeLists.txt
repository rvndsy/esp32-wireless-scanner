idf_component_register(SRCS "esp32-wifi-scanner.c" "gui.c" "net-scanner.c" "port-scanner.c" "file-writing.c" "http-server.c"
                       INCLUDE_DIRS ".")


# Copy files from ../server-image to LittleFS partition root at /littlefs
littlefs_create_partition_image(littlefs ../server-image FLASH_IN_PROJECT)
