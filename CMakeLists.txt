cmake_minimum_required(VERSION 2.8.11)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_library(smu
  libsmu.h
  libsmu.hpp
  session.cpp
  signal.cpp
  device_cee.hpp
  device_cee.cpp
)
target_link_libraries(smu -lusb-1.0 -lm)

add_executable(cli cli.cpp)
target_link_libraries(cli smu)
set_target_properties(cli PROPERTIES OUTPUT_NAME smu)
