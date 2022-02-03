if(MSVC)
	# needed for CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS support
	cmake_minimum_required(VERSION 3.4)
else()
	cmake_minimum_required(VERSION 2.8.7)
endif()
project(libsmu CXX C)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.18.0")
       # https://cmake.org/cmake/help/v3.18/policy/CMP0068.html
       cmake_policy(SET CMP0068 NEW)
endif()

# libsmu versioning
set(LIBSMU_VERSION_MAJOR 1)
set(LIBSMU_VERSION_MINOR 0)
set(LIBSMU_VERSION_PATCH 3)
set(LIBSMU_VERSION ${LIBSMU_VERSION_MAJOR}.${LIBSMU_VERSION_MINOR}.${LIBSMU_VERSION_PATCH})

# determine a more descriptive project version using git info if available
set(LIBSMU_VERSION_STR ${LIBSMU_VERSION})
if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/.git)
	# determine if the current revision is a tag
	execute_process(COMMAND git describe --exact-match --tags HEAD
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE LIBSMU_TAG_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)
	# if the current revision isn't a tag, add git revision info
	if(LIBSMU_TAG_VERSION STREQUAL "")
		execute_process(COMMAND git rev-parse --short HEAD
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
			OUTPUT_VARIABLE LIBSMU_GIT_REV
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)
		set(LIBSMU_VERSION_STR ${LIBSMU_VERSION_STR}-g${LIBSMU_GIT_REV})
	endif()
endif()
# write version info to file -- used for CI artifact versioning
file(WRITE ${CMAKE_BINARY_DIR}/.version ${LIBSMU_VERSION_STR})

# generate version header
configure_file(${CMAKE_SOURCE_DIR}/dist/version.hpp.in
	${CMAKE_SOURCE_DIR}/include/libsmu/version.hpp @ONLY)
include_directories(${CMAKE_SOURCE_DIR}/include/)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "-Wall -pedantic -std=c++11 ${CMAKE_CXX_FLAGS}")
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
# don't build examples by default
set(BUILD_EXAMPLES OFF CACHE BOOL "Build examples")
# don't build tests by default
set(BUILD_TESTS OFF CACHE BOOL "Build unit tests")
# install udev rules
set(INSTALL_UDEV_RULES ON CACHE BOOL "Install udev rules for the M1K")
# don't generate docs by default
set(WITH_DOC OFF CACHE BOOL "Generate documentation with Doxygen")

include(GNUInstallDirs)

# set default install path to /usr/local
if (NOT WIN32 AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "default install path" FORCE)
endif()

# handle RPATH issues on OS X
if(APPLE)
	set(CMAKE_MACOSX_RPATH ON)
	set(CMAKE_SKIP_BUILD_RPATH FALSE)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
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
if(BUILD_EXAMPLES)
	add_subdirectory(examples)
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
	add_subdirectory(doc)
endif()

# Create an installer if compiling for OSX
if(OSX_PACKAGE)
	set(LIBSMU_PKG ${CMAKE_CURRENT_BINARY_DIR}/libsmu-${LIBSMU_VERSION_STR}.pkg)
	set(LIBSMU_TEMP_PKG ${CMAKE_CURRENT_BINARY_DIR}/libsmu-${LIBSMU_VERSION_STR}-temp.pkg)
	set(LIBSMU_DISTRIBUTION_XML ${CMAKE_CURRENT_BINARY_DIR}/Distribution.xml)
	set(LIBSMU_FRAMEWORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/src/libsmu.framework)
	configure_file(Distribution.xml.cmakein ${LIBSMU_DISTRIBUTION_XML} @ONLY)

	find_program(PKGBUILD_EXECUTABLE
		NAMES pkgbuild
		DOC "OSX Package builder (pkgbuild)")
	mark_as_advanced(PKGBUILD_EXECUTABLE)

	find_program(PRODUCTBUILD_EXECUTABLE
		NAMES productbuild
		DOC "OSX Package builder (productbuild)")
	mark_as_advanced(PRODUCTBUILD_EXECUTABLE)

	set(COPY_TOOLS_COMMAND)
	if (BUILD_CLI)
		list(APPEND COPY_TOOLS_COMMAND
			COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${SMUCLI_TARGET}> ${LIBSMU_FRAMEWORK_DIR}/cli)
	endif()

	add_custom_command(OUTPUT ${LIBSMU_PKG}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${LIBSMU_FRAMEWORK_DIR}/cli
		${COPY_TOOLS_COMMAND}
		COMMAND ${PKGBUILD_EXECUTABLE}
			--component ${LIBSMU_FRAMEWORK_DIR}
			--identifier libsmu --version ${LIBSMU_VERSION_STR}
			--install-location ${OSX_INSTALL_FRAMEWORKSDIR} ${LIBSMU_TEMP_PKG}
		COMMAND ${PRODUCTBUILD_EXECUTABLE}
			--distribution ${LIBSMU_DISTRIBUTION_XML} ${LIBSMU_PKG}
		COMMAND ${CMAKE_COMMAND} -E remove ${LIBSMU_TEMP_PKG}
		DEPENDS smu ${SMUCLI_TARGET} ${LIBSMU_DISTRIBUTION_XML}
	)

	if (PKGBUILD_EXECUTABLE AND PRODUCTBUILD_EXECUTABLE)
		add_custom_target(libsmu-pkg ALL DEPENDS ${LIBSMU_PKG})

		install(CODE "execute_process(COMMAND /usr/sbin/installer -pkg ${LIBSMU_PKG} -target /)")
	else()
		message(WARNING "Missing pkgbuild or productbuild: OSX installer won't be created.")
	endif()
endif()

if (NOT OSX_PACKAGE)
	# Support creating some basic binpkgs via `make package`.
	# Disabled if OSX_PACKAGE is enabled, as tarballs would end up empty otherwise.
	option(ENABLE_PACKAGING "Create .deb/.rpm or .tar.gz packages via 'make package'" OFF)

	if(ENABLE_PACKAGING)
		if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
			include(cmake/DarwinPackaging.cmake)
		endif()
		if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
			include(cmake/LinuxPackaging.cmake)
		endif()
	endif()
endif()
