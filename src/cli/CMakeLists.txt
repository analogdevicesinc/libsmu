include_directories(..)

include(CheckFunctionExists)
CHECK_FUNCTION_EXISTS(getopt GETOPT_FOUND)

if(GETOPT_FOUND)
	add_executable(smu_bin smu.cpp)
else(GETOPT_FOUND)
	# use internal getopt implementation
	add_executable(smu_bin smu.cpp getopt.c)
endif(GETOPT_FOUND)

if(NOT WIN32)
	link_directories(${LINK_DIRECTORIES} ${LIBUSB_LIBRARY_DIRS})
endif()

include_directories(SYSTEM ${LIBUSB_INCLUDE_DIRS})
set_target_properties(smu_bin PROPERTIES OUTPUT_NAME smu)
target_link_libraries(smu_bin smu)

install(TARGETS smu_bin RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
