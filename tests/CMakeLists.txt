include(CheckFunctionExists)
CHECK_FUNCTION_EXISTS(getopt GETOPT_FOUND)

if(NOT WIN32)
	link_directories(${LINK_DIRECTORIES} ${LIBUSB_LIBRARY_DIRS})
endif()

if(NOT GETOPT_FOUND)
	# use internal getopt implementation
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src/cli)
	set(GETOPT_C_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../src/cli/getopt.c)
endif()

include_directories(SYSTEM ${LIBUSB_INCLUDE_DIRS})
add_executable(smu_stream_in smu_stream_in.cpp ${GETOPT_C_FILE})
add_executable(smu_stream_out smu_stream_out.cpp ${GETOPT_C_FILE})
add_executable(smu_stream_multi_out smu_stream_multi_out.cpp ${GETOPT_C_FILE})
target_link_libraries(smu_stream_in smu)
target_link_libraries(smu_stream_out smu)
target_link_libraries(smu_stream_multi_out smu)
