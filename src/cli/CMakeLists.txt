include_directories(..)

include(CheckFunctionExists)
CHECK_FUNCTION_EXISTS(getopt GETOPT_FOUND)

if(NOT WIN32)
	link_directories(${LINK_DIRECTORIES} ${LIBUSB_LIBRARY_DIRS})
endif()

if(GETOPT_FOUND)
	add_executable(smu_bin smu.cpp)
else(GETOPT_FOUND)
	# use internal getopt implementation
	add_executable(smu_bin smu.cpp getopt_internal.c)
	add_definitions(-DUSE_CUSTOM_GETOPT=1)
endif(GETOPT_FOUND)

include_directories(SYSTEM ${LIBUSB_INCLUDE_DIRS})
set_target_properties(smu_bin PROPERTIES OUTPUT_NAME smu)
target_link_libraries(smu_bin smu)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	install(TARGETS smu_bin RUNTIME DESTINATION ${OSX_INSTALL_FRAMEWORKSDIR}/libsmu.framework/cli)
else()
	install(TARGETS smu_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Make the test targets available to the main CMakeLists.txt
set(SMUCLI_TARGET smu_bin PARENT_SCOPE)