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

if(MSVC)
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Classify libusb headers as system headers to suppress various warnings,
# e.g. http://www.libusb.org/ticket/2.
include_directories(SYSTEM ${LIBUSB_INCLUDE_DIRS})

set(LIBS_TO_LINK ${LIBUSB_LIBRARIES})
if(CMAKE_COMPILER_IS_GNUCXX)
	SET(LIBS_TO_LINK ${LIBS_TO_LINK} m)
endif()
set(LIBSMU_CPPFILES session.cpp device_cee.cpp device_m1000.cpp)
set(LIBSMU_HEADERS libsmu.hpp)

add_library(smu ${LIBSMU_CPPFILES} ${LIBSMU_HEADERS})
set_target_properties(smu PROPERTIES
	VERSION ${LIBSMU_VERSION}
	SOVERSION ${LIBSMU_VERSION_MAJOR}
	PUBLIC_HEADER ${LIBSMU_HEADERS})
target_link_libraries(smu ${LIBS_TO_LINK})

# force outputted library name for Visual Studio
if (MSVC)
	set_target_properties(smu PROPERTIES OUTPUT_NAME libsmu)
endif()

install(TARGETS smu
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
