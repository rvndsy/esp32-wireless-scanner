# What?

A simple nearby WiFi access point (AP) and in-network online device and open port scanner using an ESP32. A LVGL graphical interface is used to connect to WiFi (sorry, you have to input the password) and to execute scans. The display used is a ILI9341 TFT display with touch capabilities. All scanned data is stored in flash using a LittleFS filesystem and can be accessed by connecting to the ESP32 in AP mode and opening the ESP32 IP in any web browser.

There are three types of scans. First is a simple scan of nearby Access Points (WiFi) that is periodically executed when not connected. Second type is a full ARP scan that scans the entire subnet range of the network by sending a single ARP request to each IP in batches and reading the ARP table after a small timeout. Third type is a TCP port scan that tries to establish a TCP connection with every port in a predefined port range of a target IP (list of valid IP's is generated from an ARP scan) - ports that are open are stored in flash. Results of all scans are stored in their respective directories in flash with the filename being when the scan finished. List of results are available in serving mode.

Some scan and AP values can be configured in `./main/conf.h`. A slightly more detailed explanation of scans is provided there as well. Some configuration options may be found in `sdkconfig`.

# Build and flash

The building and flashing commands are assuming you have `esp-idf` repository cloned in `./esp-idf` at root of this project directory. That is not a requirement as long as you have cloned this repo with submodules recursed and have installed ESP-IDF v4.4.8 somewhere where you can source `export.sh` and then run the required build and flash commands with `idf.py`.

## Dependencies

**The versions are very important!**

- [ESP-IDF v4.4.8](https://github.com/espressif/esp-idf/releases/tag/v4.4.8)
- [LVGL v7.11](https://github.com/lvgl/lvgl/releases/tag/v7.11.0)
- [LVGL ESP32 drivers (commit 9fed1cc4)](https://github.com/lvgl/lvgl_esp32_drivers/commit/9fed1cc47b5a45fec6bae08b55d2147d3b50260c), the provided drivers in this repo have a few fixes that are required for this project
- lwip (provided by ESP-IDF)

## Linux

### 1. Clone this repository, including any submodules in ./components

1. `git clone https://github.com/rvndsy/esp32-wireless-scanner.git --recurse-submodules && cd esp32-wireless-scanner`

### 2. Install ESP-IDF (tool used for building and flashing to ESP32):

1. `git clone https://github.com/espressif/esp-idf -b v4.4.8`

2. `./esp-idf/install.sh`

This will install and store esp-idf files locally within this repository (no root needed).

### 3. Build with ESP-IDF

1. `. ./esp-idf/export.sh` (sourcing Python environment)

2. `./esp-idf/tools/idf.py build`

### 4. Flash onto ESP32

1. `./esp-idf/tools/idf.py flash` or `./esp-idf/tools/idf.py -p <PORT> flash` (port is something similar to /dev/ttyUSB0, if your user has permission issues [then add it to `dialout` or `uucp` group](possiblehttps://support.arduino.cc/hc/en-us/articles/360016495679-Fix-port-access-on-Linux))

Alternatively, run `./build.sh` to build this project (assuming esp-idf is cloned in ./esp-idf) or `./run.sh` to build & flash it.

### Read serial information

`./esp-idf/tools/idf.py monitor -p <PORT>`

## Windows

1. Clone this repository, including any submodules in ./components
2. Use the Windows ESP-IDF software to build and flash this project with similar commands to the **Linux** section.


