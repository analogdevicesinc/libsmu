enable_testing()
find_package(GTest REQUIRED)
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
