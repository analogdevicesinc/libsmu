cmake_minimum_required(VERSION 2.8.7)
project(libsmu CXX C)

# libsmu soname versioning
set(LIBSMU_VERSION_MAJOR 0)
set(LIBSMU_VERSION_MINOR 0)
set(LIBSMU_VERSION_PATCH 0)
set(LIBSMU_VERSION ${LIBSMU_VERSION_MAJOR}.${LIBSMU_VERSION_MINOR}.${LIBSMU_VERSION_PATCH})

# C++11 is required
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11 -pedantic")
# debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

# don't complain about extra format args for g++
if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-extra-args")
endif()

# build a shared library by default
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries")
# don't build python support by default
set(BUILD_PYTHON OFF CACHE BOOL "Build python support")
# build command line smu application by default
set(BUILD_CLI ON CACHE BOOL "Build command line smu application")
# create an OS X package
set(BUILD_OSX_PKG OFF CACHE BOOL "Create an OS X package")

include("GNUInstallDirs")

# handle RPATH issues on OS X
if(APPLE)
	set(CMAKE_MACOSX_RPATH ON)
	set(CMAKE_SKIP_BUILD_RPATH FALSE)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}")
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_LIBDIR}" isSystemDir)
	if("${isSystemDir}" STREQUAL "-1")
		set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}")
	endif()
endif()

add_subdirectory(src)
if(BUILD_PYTHON)
	add_subdirectory(bindings/python)
endif()
if(BUILD_CLI)
	add_subdirectory(src/cli)
endif()

# windows installer file
if(WIN32)
	configure_file(dist/libsmu.iss.cmakein ${CMAKE_CURRENT_BINARY_DIR}/dist/libsmu.iss @ONLY)
endif()

# install pkgconfig file
set(LIBSMU_PC ${CMAKE_CURRENT_BINARY_DIR}/libsmu.pc)
configure_file(dist/libsmu.pc.cmakein ${LIBSMU_PC} @ONLY)
install(FILES ${LIBSMU_PC} DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Create an installer for OS X
if(APPLE AND BUILD_OSX_PKG)
	set(LIBSMU_PKG ${CMAKE_CURRENT_BINARY_DIR}/libsmu-${LIBSMU_VERSION}.pkg)
	set(LIBSMU_TEMP_PKG ${CMAKE_CURRENT_BINARY_DIR}/libsmu-${LIBSMU_VERSION}-temp.pkg)
	set(LIBSMU_DISTRIBUTION_XML ${CMAKE_CURRENT_BINARY_DIR}/distribution.xml)
	configure_file(dist/distribution.xml.cmakein ${LIBSMU_DISTRIBUTION_XML} @ONLY)

	find_program(PKGBUILD_EXECUTABLE
		NAMES pkgbuild
		DOC "OSX Package builder (pkgbuild)")
	mark_as_advanced(PKGBUILD_EXECUTABLE)

	find_program(PRODUCTBUILD_EXECUTABLE
		NAMES productbuild
		DOC "OSX Package builder (productbuild)")
	mark_as_advanced(PRODUCTBUILD_EXECUTABLE)

	add_custom_command(OUTPUT ${LIBSMU_TEMP_PKG} COMMAND ${PKGBUILD_EXECUTABLE}
			--root ${CMAKE_CURRENT_BINARY_DIR}
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
