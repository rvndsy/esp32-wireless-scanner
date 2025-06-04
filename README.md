## Build and flash

### Dependencies

**The versions are very important!**

- [ESP-IDF v4.4.8](https://github.com/espressif/esp-idf/releases/tag/v4.4.8)
- [LVGL v7.11](https://github.com/lvgl/lvgl/releases/tag/v7.11.0)
- [LVGL ESP32 drivers (commit 9fed1cc4)](https://github.com/lvgl/lvgl_esp32_drivers/commit/9fed1cc47b5a45fec6bae08b55d2147d3b50260c), the provided drivers in this repo have a few fixes that are required for this project
- lwip (provided by ESP-IDF)

### Linux

Starting from the root of this repo:

#### 1. Install ESP-IDF (tool used for building and flashing to ESP32):

1. `git clone https://github.com/espressif/esp-idf -b v4.4.8`

2. `./esp-idf/install.sh`

This will install and store esp-idf files locally within this repository (no root needed).

#### 2. Clone this repository, including any submodules in ./components

1. `git clone https://github.com/rvndsy/esp32-wireless-scanner.git --recurse-submodules`

#### 3. Build with ESP-IDF

1. `. ./esp-idf/export.sh`

2. `./esp-idf/tools/idf.py build`

#### 4. Flash onto ESP32

1. `./esp-idf/tools/idf.py flash` or `./esp-idf/tools/idf.py -p <PORT> flash`

Alternatively, run `./build.sh` to build this project (assuming esp-idf is cloned in ./esp-idf) or `./run.sh` to build & flash it.

### Windows

1. Clone this repository, including any submodules in ./components
2. Use the Windows ESP-IDF software to build and flash this project similarly to the **Linux** section.
