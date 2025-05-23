# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(esp32-wifi-scanner)
