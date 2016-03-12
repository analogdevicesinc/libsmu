cmake_minimum_required(VERSION 2.8.7)
project(libsmu CXX)

# C++11 is required
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11 -pedantic")
# debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

# build a shared library by default
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries")
# don't build python support by default
set(BUILD_PYTHON OFF CACHE BOOL "Build python support")

set(LIBSMU_VERSION_MAJOR 0)
set(LIBSMU_VERSION_MINOR 0)
set(LIBSMU_VERSION_PATCH 0)
set(LIBSMU_VERSION ${LIBSMU_VERSION_MAJOR}.${LIBSMU_VERSION_MINOR}.${LIBSMU_VERSION_PATCH})

if(UNIX OR MINGW)
	find_library(PTHREAD_LIBRARIES pthread REQUIRED)
	mark_as_advanced(PTHREAD_LIBRARIES)
	set(LIBS_TO_LINK ${PTHREAD_LIBRARIES})
endif()

# use pkg-config for everything that's not Windows
if(NOT WIN32)
	include(FindPkgConfig REQUIRED)
	pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
else()
	find_library(LIBUSB_LIBRARIES usb-1.0)
	find_path(LIBUSB_INCLUDE_DIRS libusb-1.0/libusb.h)
endif()

# Classify libusb headers as system headers to suppress various warnings,
# e.g. http://www.libusb.org/ticket/2.
include_directories(SYSTEM ${LIBUSB_INCLUDE_DIRS})

set(LIBS_TO_LINK m ${LIBUSB_LIBRARIES})
set(LIBSMU_CPPFILES session.cpp device_cee.cpp device_m1000.cpp)

add_library(smu ${LIBSMU_CPPFILES})
set_target_properties(smu PROPERTIES VERSION ${LIBSMU_VERSION} SOVERSION ${LIBSMU_VERSION_MAJOR})
target_link_libraries(smu ${LIBS_TO_LINK})

add_executable(smu_bin cli.cpp)
set_target_properties(smu_bin PROPERTIES OUTPUT_NAME smu)
target_link_libraries(smu_bin smu)

if(BUILD_PYTHON)
	add_subdirectory(bindings/python)
endif() # BUILD_PYTHON

install(TARGETS smu smu_bin
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin)
