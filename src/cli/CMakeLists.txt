include_directories(..)

add_executable(smu_bin smu.cpp)
set_target_properties(smu_bin PROPERTIES OUTPUT_NAME smu)
target_link_libraries(smu_bin smu)

install(TARGETS smu_bin RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
