cmake_minimum_required(VERSION 2.8.7)
project(libsmu CXX)

# C++11 is required
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11 -pedantic")

# build a shared library by default
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries")
# don't build python support by default
set(BUILD_PYTHON OFF CACHE BOOL "Build python support")

set(LIBSMU_VERSION_MAJOR 0)
set(LIBSMU_VERSION_MINOR 0)
set(LIBSMU_VERSION_PATCH 0)
set(LIBSMU_VERSION ${LIBSMU_VERSION_MAJOR}.${LIBSMU_VERSION_MINOR}.${LIBSMU_VERSION_PATCH})

find_library(PTHREAD_LIBRARIES pthread REQUIRED)
mark_as_advanced(PTHREAD_LIBRARIES)

include(FindPkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# Classify libusb headers as system headers to suppress various warnings,
# e.g. http://www.libusb.org/ticket/2.
include_directories(SYSTEM ${LIBUSB_INCLUDE_DIRS})

set(LIBS_TO_LINK m ${PTHREAD_LIBRARIES} ${LIBUSB_LIBRARIES})
set(LIBSMU_CPPFILES session.cpp device_cee.cpp device_m1000.cpp)

add_library(smu ${LIBSMU_CPPFILES})
set_target_properties(smu PROPERTIES VERSION ${LIBSMU_VERSION} SOVERSION ${LIBSMU_VERSION_MAJOR})
target_link_libraries(smu ${LIBS_TO_LINK})

add_executable(smu_bin cli.cpp)
target_link_libraries(smu_bin smu)

if (BUILD_PYTHON)
	add_subdirectory(python)
endif() # BUILD_PYTHON

install(TARGETS smu smu_bin
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin)
