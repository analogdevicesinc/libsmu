cmake_minimum_required(VERSION 2.8.11)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_library(smu SHARED
  libsmu.h
  libsmu.hpp
  session.cpp
  device_cee.hpp
  device_cee.cpp
  device_m1000.hpp
  device_m1000.cpp
)

set(CMAKE_INCLUDE_PATH "C:/libusb/include/libusb-1.0")
target_link_libraries(smu -lm -L"C:/libusb/MinGW32/static/libusb-1.0.a")
