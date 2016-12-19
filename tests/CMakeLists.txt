enable_testing()
include(CTest)

if(WIN32 OR APPLE)
	# download and build gtest on Windows and OS X
	set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)
	include(DownloadProject)
	download_project(
		PROJ                googletest
		GIT_REPOSITORY      https://github.com/google/googletest.git
		GIT_TAG             master
		${UPDATE_DISCONNECTED_IF_AVAILABLE}
	)

	# Prevent gtest from overriding our compiler/linker options
	# when building with Visual Studio
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

	add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

	# When using CMake 2.8.11 or later, header path dependencies
	# are automatically added to the gtest and gmock targets.
	# For earlier CMake versions, we have to explicitly add the
	# required directories to the header search path ourselves.
	if (CMAKE_VERSION VERSION_LESS 2.8.11)
		include_directories(
			"${gtest_SOURCE_DIR}/include"
			"${gmock_SOURCE_DIR}/include")
	endif()

	set(GTEST_LIBS gtest gtest_main)
else()
	find_package(GTest REQUIRED)
	include_directories(${GTEST_INCLUDE_DIRS})
	set(GTEST_LIBS ${GTEST_BOTH_LIBRARIES})
endif()

if(NOT WIN32)
	link_directories(${LINK_DIRECTORIES} ${LIBUSB_LIBRARY_DIRS})
endif()
include_directories(SYSTEM ${LIBUSB_INCLUDE_DIRS})

# determine all tests from existing sources
file(GLOB TEST_SRCS "test-*.cpp")
foreach(TEST_SRC ${TEST_SRCS})
	# pull the test name from the .cpp file name without the extension
	get_filename_component(TEST "${TEST_SRC}" NAME_WE)

	# create the test application
	add_executable(${TEST} ${TEST}.cpp)
	target_link_libraries(${TEST} smu ${GTEST_LIBS})
	add_test(NAME ${TEST} COMMAND ${TEST})

	# add test to the test list
	list(APPEND TESTS ${TEST})
endforeach(TEST_SRC)

# don't run any tests in parallel
set_tests_properties(${TESTS} PROPERTIES RUN_SERIAL TRUE)

# add support for `make check` to build/run tests
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -V DEPENDS ${TESTS})
