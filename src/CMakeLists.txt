if(UNIX OR MINGW)
	find_library(PTHREAD_LIBRARIES pthread REQUIRED)
	mark_as_advanced(PTHREAD_LIBRARIES)
	set(LIBS_TO_LINK ${PTHREAD_LIBRARIES})
endif()

# use pkg-config for everything that's not Windows
if(NOT WIN32)
	include(FindPkgConfig REQUIRED)
	pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
else()
	find_library(LIBUSB_LIBRARIES usb-1.0)
	find_path(LIBUSB_INCLUDE_DIRS libusb-1.0/libusb.h)
endif()

# Classify libusb headers as system headers to suppress various warnings,
# e.g. http://www.libusb.org/ticket/2.
include_directories(SYSTEM ${LIBUSB_INCLUDE_DIRS})

set(LIBS_TO_LINK m ${LIBUSB_LIBRARIES})
set(LIBSMU_CPPFILES session.cpp device_cee.cpp device_m1000.cpp)

set(LIBSMU_VERSION ${LIBSMU_VERSION_MAJOR}.${LIBSMU_VERSION_MINOR}.${LIBSMU_VERSION_PATCH})

add_library(smu ${LIBSMU_CPPFILES})
set_target_properties(smu PROPERTIES VERSION ${LIBSMU_VERSION} SOVERSION ${LIBSMU_VERSION_MAJOR})
target_link_libraries(smu ${LIBS_TO_LINK})

install(TARGETS smu
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib)
