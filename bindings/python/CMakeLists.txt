if(MSVC)
	# needed for CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS support
	cmake_minimum_required(VERSION 3.4)
else()
	cmake_minimum_required(VERSION 2.8.7)
endif()

if(${CMAKE_VERSION} VERSION_LESS "3.12.0")
	if (NOT WIN32)
		set(Python_ADDITIONAL_VERSIONS 3)
	endif()

	include(FindPythonInterp)
	include(FindPythonLibs)

	set(Python_Interpreter_FOUND ${PYTHONINTERP_FOUND})
	set(Python_EXECUTABLE ${PYTHON_EXECUTABLE})
	set(Python_LIBRARIES ${PYTHON_LIBRARIES})
	set(Python_INCLUDE_PATH ${PYTHON_INCLUDE_PATH})
	set(Python_INCLUDE_DIRS ${PYTHON_INCLUDE_DIRS})
else()
	find_package(Python COMPONENTS Interpreter Development)
endif()

#find_package(PythonLibs REQUIRED)

set(SETUP_PY "${CMAKE_CURRENT_SOURCE_DIR}/setup.py")
set(DEPS "${CMAKE_CURRENT_SOURCE_DIR}/pysmu/__init__.py")
set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/timestamp")

add_custom_command(OUTPUT ${OUTPUT}
	COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} build build_ext -L "${CMAKE_BINARY_DIR}/src"
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	DEPENDS ${DEPS})
add_custom_target(python ALL DEPENDS smu ${OUTPUT})

install(CODE "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} install --root=\$ENV{DESTDIR}/ --prefix=${CMAKE_INSTALL_PREFIX})")


