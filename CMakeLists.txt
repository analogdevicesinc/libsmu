cmake_minimum_required(VERSION 2.8.11)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_library(smu
  libsmu.h
  libsmu.hpp
  session.cpp
  device_cee.hpp
  device_cee.cpp
  device_m1000.hpp
  device_m1000.cpp
)

find_package(PkgConfig)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
include_directories(${LIBUSB_INCLUDE_DIRS})
target_link_libraries(smu -lm ${LIBUSB_LIBRARIES})

add_executable(cli cli.cpp)
target_link_libraries(cli smu)
set_target_properties(cli PROPERTIES OUTPUT_NAME smu)
