set(Python_ADDITIONAL_VERSIONS 2.7 3.4 3.5)
find_package(PythonInterp)
if(PYTHONINTERP_FOUND)
	find_package(PythonLibs REQUIRED)

	set(SETUP_PY "${CMAKE_CURRENT_SOURCE_DIR}/setup.py")
	set(DEPS "${CMAKE_CURRENT_SOURCE_DIR}/pysmu/__init__.py")
	set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/timestamp")

	add_custom_command(OUTPUT ${OUTPUT}
		COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} build build_ext -L "${CMAKE_BINARY_DIR}/src"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		DEPENDS ${DEPS})
	add_custom_target(python ALL DEPENDS smu ${OUTPUT})

	install(CODE "execute_process(COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} install -f
		--prefix=${CMAKE_INSTALL_PREFIX}
		--single-version-externally-managed --root=/
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})")
else()
	message(STATUS "Skipping building python bindings, python interpreter not found")
endif()
