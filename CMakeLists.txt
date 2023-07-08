cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS ~/esp/esp-idf-lib/components $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(climate_terminal02)

