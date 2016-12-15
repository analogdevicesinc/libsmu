enable_testing()
if(NOT WIN32)
	find_package(GTest REQUIRED)
else()
	# Download and unpack googletest at configure time
	configure_file(cmake-gtest.in googletest-download/CMakeLists.txt)
	execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download)
	execute_process(COMMAND ${CMAKE_COMMAND} --build .
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download)

	# Prevent GoogleTest from overriding our compiler/linker options
	# when building with Visual Studio
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

	# Add googletest directly to our build. This adds
	# the following targets: gtest, gtest_main, gmock
	# and gmock_main.
	add_subdirectory(
		"${CMAKE_BINARY_DIR}/googletest-src"
		"${CMAKE_BINARY_DIR}/googletest-build")

	# The gtest/gmock targets carry header search path
	# dependencies automatically when using CMake 2.8.11 or
	# later. Otherwise we have to add them here ourselves.
	if (CMAKE_VERSION VERSION_LESS 2.8.11)
	include_directories(
		"${gtest_SOURCE_DIR}/include"
		"${gmock_SOURCE_DIR}/include")
	endif()
endif()

include_directories(${GTEST_INCLUDE_DIRS})

if(NOT WIN32)
	link_directories(${LINK_DIRECTORIES} ${LIBUSB_LIBRARY_DIRS})
endif()
include_directories(SYSTEM ${LIBUSB_INCLUDE_DIRS})

add_executable(read-test read-test.cpp)
target_link_libraries(read-test smu ${GTEST_BOTH_LIBRARIES})
GTEST_ADD_TESTS(read-test "" read-test.cpp)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -V
                  DEPENDS read-test)
