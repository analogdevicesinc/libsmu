cmake_minimum_required(VERSION 2.8.7)
project(libsmu CXX)

# We want C++11
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11 -pedantic")

# Build a shared library by default
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries")

set(LIBSMU_VERSION_MAJOR 0)
set(LIBSMU_VERSION_MINOR 1)
set(LIBSMU_VERSION ${LIBSMU_VERSION_MAJOR}.${LIBSMU_VERSION_MINOR})

find_library(PTHREAD_LIBRARIES pthread REQUIRED)
mark_as_advanced(PTHREAD_LIBRARIES)

include(FindPkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

include_directories(${LIBUSB_INCLUDE_DIRS})

set(LIBS_TO_LINK m ${PTHREAD_LIBRARIES} ${LIBUSB_LIBRARIES})
set(LIBSMU_CPPFILES session.cpp device_cee.cpp device_m1000.cpp)

add_library(smu ${LIBSMU_CPPFILES})
set_target_properties(smu PROPERTIES VERSION ${LIBSMU_VERSION} SOVERSION ${LIBSMU_VERSION_MAJOR})
target_link_libraries(smu ${LIBS_TO_LINK})

add_executable(smu_bin cli.cpp)
target_link_libraries(smu_bin smu)

#set(Python_ADDITIONAL_VERSIONS 2.7)
#include(FindPythonLibs OPTIONAL)

#if (PYTHONLIBS_FOUND)
#	add_library(pysmu pysmu.cpp)
#	target_link_libraries(pysmu ${PYTHON_LIBRARIES})
#	include_directories(${PYTHON_INCLUDE_DIRS})
#endif()

install(TARGETS smu smu_bin
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin)
