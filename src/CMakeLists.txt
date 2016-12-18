option(BUILD_STATIC_LIB "Build static library" OFF)

if(UNIX OR MINGW)
	find_library(PTHREAD_LIBRARIES pthread REQUIRED)
	mark_as_advanced(PTHREAD_LIBRARIES)
	set(LIBS_TO_LINK ${PTHREAD_LIBRARIES})
endif()

if(APPLE)
	# build universal binaries by default
	set(CMAKE_OSX_ARCHITECTURES "i386;x86_64")
endif()

# use pkg-config for everything that's not Windows
if(NOT WIN32)
	include(FindPkgConfig REQUIRED)
	pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
	link_directories(${LINK_DIRECTORIES} ${LIBUSB_LIBRARY_DIRS})
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

# only boost headers are needed, no runtime libraries
find_package(Boost "1.53" REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

option(USE_OpenMP "Use OpenMP" ON)
if(USE_OpenMP)
	find_package(OpenMP)
	if(OPENMP_FOUND)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
	endif()
endif()

set(LIBS_TO_LINK ${LIBUSB_LIBRARIES})
if(CMAKE_COMPILER_IS_GNUCXX)
	SET(LIBS_TO_LINK ${LIBS_TO_LINK} m)
endif()
file(GLOB LIBSMU_CPPFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB LIBSMU_HEADERS ${CMAKE_SOURCE_DIR}/include/libsmu/*.hpp)

add_library(smu SHARED ${LIBSMU_CPPFILES} ${LIBSMU_HEADERS})
set_target_properties(smu PROPERTIES
	VERSION ${LIBSMU_VERSION}
	SOVERSION ${LIBSMU_VERSION_MAJOR}
	PUBLIC_HEADER "${LIBSMU_HEADERS}")
target_link_libraries(smu ${LIBS_TO_LINK})

# build static library if requested
if(BUILD_STATIC_LIB)
	add_library(smu-static STATIC ${LIBSMU_CPPFILES} ${LIBSMU_HEADERS})
	set_target_properties(smu-static PROPERTIES OUTPUT_NAME smu)
	target_link_libraries(smu-static ${LIBS_TO_LINK})
	install(TARGETS smu-static
		ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()

# force outputted library name for Visual Studio
if (MSVC)
	set_target_properties(smu PROPERTIES OUTPUT_NAME libsmu)
endif()

install(TARGETS smu
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/libsmu")
