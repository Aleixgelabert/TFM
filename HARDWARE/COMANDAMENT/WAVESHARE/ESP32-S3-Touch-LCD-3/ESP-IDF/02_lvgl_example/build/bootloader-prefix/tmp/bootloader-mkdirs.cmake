# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/aleixgelabert/esp/v5.5.1/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "/Users/aleixgelabert/esp/v5.5.1/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "/Users/aleixgelabert/Library/CloudStorage/GoogleDrive-aleixgelabert@gmail.com/La meva unitat/Mecatrònica/Treball Final de Màster/DOCUMENTACIÓ/SCREENS/ILI9488/WAVESHARE/ESP32-S3-Touch-LCD-3/ESP-IDF/02_lvgl_example/build/bootloader"
  "/Users/aleixgelabert/Library/CloudStorage/GoogleDrive-aleixgelabert@gmail.com/La meva unitat/Mecatrònica/Treball Final de Màster/DOCUMENTACIÓ/SCREENS/ILI9488/WAVESHARE/ESP32-S3-Touch-LCD-3/ESP-IDF/02_lvgl_example/build/bootloader-prefix"
  "/Users/aleixgelabert/Library/CloudStorage/GoogleDrive-aleixgelabert@gmail.com/La meva unitat/Mecatrònica/Treball Final de Màster/DOCUMENTACIÓ/SCREENS/ILI9488/WAVESHARE/ESP32-S3-Touch-LCD-3/ESP-IDF/02_lvgl_example/build/bootloader-prefix/tmp"
  "/Users/aleixgelabert/Library/CloudStorage/GoogleDrive-aleixgelabert@gmail.com/La meva unitat/Mecatrònica/Treball Final de Màster/DOCUMENTACIÓ/SCREENS/ILI9488/WAVESHARE/ESP32-S3-Touch-LCD-3/ESP-IDF/02_lvgl_example/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/aleixgelabert/Library/CloudStorage/GoogleDrive-aleixgelabert@gmail.com/La meva unitat/Mecatrònica/Treball Final de Màster/DOCUMENTACIÓ/SCREENS/ILI9488/WAVESHARE/ESP32-S3-Touch-LCD-3/ESP-IDF/02_lvgl_example/build/bootloader-prefix/src"
  "/Users/aleixgelabert/Library/CloudStorage/GoogleDrive-aleixgelabert@gmail.com/La meva unitat/Mecatrònica/Treball Final de Màster/DOCUMENTACIÓ/SCREENS/ILI9488/WAVESHARE/ESP32-S3-Touch-LCD-3/ESP-IDF/02_lvgl_example/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/aleixgelabert/Library/CloudStorage/GoogleDrive-aleixgelabert@gmail.com/La meva unitat/Mecatrònica/Treball Final de Màster/DOCUMENTACIÓ/SCREENS/ILI9488/WAVESHARE/ESP32-S3-Touch-LCD-3/ESP-IDF/02_lvgl_example/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/aleixgelabert/Library/CloudStorage/GoogleDrive-aleixgelabert@gmail.com/La meva unitat/Mecatrònica/Treball Final de Màster/DOCUMENTACIÓ/SCREENS/ILI9488/WAVESHARE/ESP32-S3-Touch-LCD-3/ESP-IDF/02_lvgl_example/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
