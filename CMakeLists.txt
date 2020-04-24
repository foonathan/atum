cmake_minimum_required(VERSION 3.8)
project(atum VERSION 0.0.0)

# main target
add_library(atum INTERFACE)
add_library(foonathan::atum ALIAS atum)
target_sources(atum INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/atum.hpp>")
target_compile_features(atum INTERFACE cxx_std_17)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    target_include_directories(atum INTERFACE
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
                               $<INSTALL_INTERFACE:include/>)
else()
    target_include_directories(atum SYSTEM INTERFACE
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
                               $<INSTALL_INTERFACE:include/>)
endif()

