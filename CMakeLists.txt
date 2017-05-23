cmake_minimum_required(VERSION 2.8)
project(cfp)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(FindPkgConfig)
find_package(Fcitx 4.2.8.1 REQUIRED)

fcitx_add_addon_full(cfp DESC SCAN SCAN_PRIV
  HEADERS 
  SOURCES main.c cfp.c)
