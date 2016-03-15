cmake_minimum_required(VERSION 2.8.7)
project(libsmu CXX)

# libsmu soname versioning
set(LIBSMU_VERSION_MAJOR 0)
set(LIBSMU_VERSION_MINOR 0)
set(LIBSMU_VERSION_PATCH 0)
set(LIBSMU_VERSION ${LIBSMU_VERSION_MAJOR}.${LIBSMU_VERSION_MINOR}.${LIBSMU_VERSION_PATCH})

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

if (NOT WIN32)
	include("GNUInstallDirs")
endif()

add_subdirectory(src)
if(BUILD_PYTHON)
	add_subdirectory(bindings/python)
endif()
if(BUILD_CLI)
	add_subdirectory(src/cli)
endif()

# Create an installer if compiling for OS X
if(APPLE)
	set(LIBSMU_PKG ${CMAKE_CURRENT_BINARY_DIR}/libsmu-${LIBSMU_VERSION}.pkg)
	set(LIBSMU_TEMP_PKG ${CMAKE_CURRENT_BINARY_DIR}/libsmu-${LIBSMU_VERSION}-temp.pkg)
	set(LIBSMU_DISTRIBUTION_XML ${CMAKE_CURRENT_BINARY_DIR}/distribution.xml)
	configure_file(distribution.xml.cmakein ${LIBSMU_DISTRIBUTION_XML} @ONLY)

	find_program(PKGBUILD_EXECUTABLE
		NAMES pkgbuild
		DOC "OSX Package builder (pkgbuild)")
	mark_as_advanced(PKGBUILD_EXECUTABLE)

	find_program(PRODUCTBUILD_EXECUTABLE
		NAMES productbuild
		DOC "OSX Package builder (productbuild)")
	mark_as_advanced(PRODUCTBUILD_EXECUTABLE)

	add_custom_command(OUTPUT ${LIBSMU_TEMP_PKG} COMMAND ${PKGBUILD_EXECUTABLE}
			--root ${CMAKE_CURRENT_BINARY_DIR}/Debug
			--identifier libsmu --version ${LIBSMU_VERSION}
			--install-location /System/Library/Frameworks ${LIBSMU_TEMP_PKG}
		DEPENDS smu
	)

	add_custom_command(OUTPUT ${LIBSMU_PKG} COMMAND ${PRODUCTBUILD_EXECUTABLE}
			--distribution ${LIBSMU_DISTRIBUTION_XML} ${LIBSMU_PKG}
		DEPENDS ${LIBSMU_DISTRIBUTION_XML} ${LIBSMU_TEMP_PKG}
	)

	if (PKGBUILD_EXECUTABLE AND PRODUCTBUILD_EXECUTABLE)
		add_custom_target(libsmu-pkg ALL DEPENDS ${LIBSMU_PKG})
	else()
		message(WARNING "Missing pkgbuild or productbuild: OS X installer won't be created")
	endif()
endif()
