cmake_minimum_required(VERSION 2.8.7)

set(Python_ADDITIONAL_VERSIONS 2.7)
include(FindPythonLibs REQUIRED)

find_program(PYTHON "python2.7")
set(SETUP_PY "${CMAKE_CURRENT_SOURCE_DIR}/setup.py")
set(DEPS "${CMAKE_CURRENT_SOURCE_DIR}/pysmu/__init__.py")
set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/timestamp")

add_custom_command(OUTPUT ${OUTPUT}
	COMMAND ${PYTHON} ${SETUP_PY} build build_ext -L "${CMAKE_BINARY_DIR}"
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	DEPENDS ${DEPS})
add_custom_target(python ALL DEPENDS smu ${OUTPUT})

install(CODE "execute_process(COMMAND ${PYTHON} ${SETUP_PY} install -f
	--prefix=${CMAKE_INSTALL_PREFIX}
	--single-version-externally-managed --root=/
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})")
