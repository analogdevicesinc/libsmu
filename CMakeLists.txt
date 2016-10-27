if(MSVC)
	# needed for CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS support
	cmake_minimum_required(VERSION 3.4)
else()
	cmake_minimum_required(VERSION 2.8.7)
endif()
project(libsmu CXX C)

option(WITH_DOC "Generate documentation with Doxygen" OFF)

# libsmu versioning
set(LIBSMU_VERSION_MAJOR 0)
set(LIBSMU_VERSION_MINOR 9)
set(LIBSMU_VERSION_PATCH 0)
set(LIBSMU_VERSION ${LIBSMU_VERSION_MAJOR}.${LIBSMU_VERSION_MINOR}.${LIBSMU_VERSION_PATCH})

# support creating some basic binpkgs via `make package`
set(CPACK_GENERATOR STGZ;TGZ)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
set(CPACK_PACKAGE_VERSION_MAJOR ${LIBSMU_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${LIBSMU_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${LIBSMU_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${LIBSMU_VERSION})
set(CPACK_BUNDLE_NAME libsmu)
set(CPACK_SET_DESTDIR ON)
include(CPack)

# generate version header
configure_file(${CMAKE_SOURCE_DIR}/dist/version.hpp.in
	${CMAKE_SOURCE_DIR}/include/libsmu/version.hpp @ONLY)
include_directories(${CMAKE_SOURCE_DIR}/include/)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -std=c++11")
endif()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG_BUILD")

# don't complain about extra format args for g++
if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-extra-args")
endif()

# build a shared library by default
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries")
# build python support by default
set(BUILD_PYTHON ON CACHE BOOL "Build python support")
# build command line smu application by default
set(BUILD_CLI ON CACHE BOOL "Build command line smu application")
# don't build tests by default
set(BUILD_TESTS OFF CACHE BOOL "Build tests")
# install udev rules
set(INSTALL_UDEV_RULES ON CACHE BOOL "Install udev rules for the M1K")

include(GNUInstallDirs)

# set default install path to /usr
if (NOT WIN32 AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "default install path" FORCE)
endif()

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
if(BUILD_TESTS)
	add_subdirectory(tests)
endif()

# windows installer file
if(WIN32)
	configure_file(dist/libsmu-x86.iss.cmakein ${CMAKE_CURRENT_BINARY_DIR}/dist/libsmu-x86.iss @ONLY)
	configure_file(dist/libsmu-x64.iss.cmakein ${CMAKE_CURRENT_BINARY_DIR}/dist/libsmu-x64.iss @ONLY)
endif()

# install pkgconfig file
set(LIBSMU_PC ${CMAKE_CURRENT_BINARY_DIR}/libsmu.pc)
configure_file(dist/libsmu.pc.cmakein ${LIBSMU_PC} @ONLY)
install(FILES ${LIBSMU_PC} DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# install udev rules on Linux
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND INSTALL_UDEV_RULES)
	set(LIBSMU_UDEV_RULES "${CMAKE_CURRENT_SOURCE_DIR}/dist/53-adi-m1k-usb.rules")
	set(UDEV_RULES_PATH "/etc/udev/rules.d" CACHE STRING "Target directory for udev rule installation.")
	install(FILES ${LIBSMU_UDEV_RULES} DESTINATION ${UDEV_RULES_PATH})
endif()

# generate API docs with doxygen
if(WITH_DOC)
	find_package(Doxygen REQUIRED)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
	set(HTML_DEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/html)

	add_custom_target(doc ALL
		COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		COMMENT "Generating API documentation with Doxygen" VERBATIM
	)

	install(DIRECTORY ${HTML_DEST_DIR} DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

# create a package build script for OS X
if(APPLE)
	set(LIBSMU_PKG ${CMAKE_CURRENT_BINARY_DIR}/libsmu-${LIBSMU_VERSION}.pkg)
	set(LIBSMU_TEMP_PKG ${CMAKE_CURRENT_BINARY_DIR}/libsmu-${LIBSMU_VERSION}-temp.pkg)
	configure_file(dist/distribution.xml.cmakein ${CMAKE_CURRENT_BINARY_DIR}/distribution.xml @ONLY)
	configure_file(dist/build-osx-pkg.sh.cmakein ${CMAKE_CURRENT_BINARY_DIR}/build-osx-pkg.sh @ONLY)
endif()
