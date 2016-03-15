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
# build command line smu application by default
set(BUILD_CLI ON CACHE BOOL "Build command line smu application")

# libsmu soname versioning
set(LIBSMU_VERSION_MAJOR 0)
set(LIBSMU_VERSION_MINOR 0)
set(LIBSMU_VERSION_PATCH 0)

add_subdirectory(src)
if(BUILD_PYTHON)
	add_subdirectory(bindings/python)
endif()
if(BUILD_CLI)
	add_subdirectory(src/cli)
endif()
