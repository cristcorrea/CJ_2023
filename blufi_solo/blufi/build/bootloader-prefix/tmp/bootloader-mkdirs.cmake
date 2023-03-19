# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/cris/esp2/esp-idf/components/bootloader/subproject"
  "/home/cris/Documentos/CJ_ESP32/blufi_solo/blufi/build/bootloader"
  "/home/cris/Documentos/CJ_ESP32/blufi_solo/blufi/build/bootloader-prefix"
  "/home/cris/Documentos/CJ_ESP32/blufi_solo/blufi/build/bootloader-prefix/tmp"
  "/home/cris/Documentos/CJ_ESP32/blufi_solo/blufi/build/bootloader-prefix/src/bootloader-stamp"
  "/home/cris/Documentos/CJ_ESP32/blufi_solo/blufi/build/bootloader-prefix/src"
  "/home/cris/Documentos/CJ_ESP32/blufi_solo/blufi/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/cris/Documentos/CJ_ESP32/blufi_solo/blufi/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/cris/Documentos/CJ_ESP32/blufi_solo/blufi/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
