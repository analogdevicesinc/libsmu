if (NOT ${USE_PYTHON2})
	set(Python_ADDITIONAL_VERSIONS 3)
else()
	set(Python_ADDITIONAL_VERSIONS 2.7 3.4 3.5)
endif()

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

	install(CODE "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} install --root=\$ENV{DESTDIR}/ --prefix=${CMAKE_INSTALL_PREFIX})")

else()
	message(STATUS "Skipping building python bindings, python interpreter not found")
endif()
