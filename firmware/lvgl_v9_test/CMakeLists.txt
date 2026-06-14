# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
# The GUI-Guider demo component (ui_bsp) is superseded by stock_core's UI and
# is no longer built. Files are kept for reference; excluded from compilation.
set(EXCLUDE_COMPONENTS "ui_bsp")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(stock_monitor)
