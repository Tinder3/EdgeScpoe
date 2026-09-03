add_executable(edgescope-agent src/main.cpp)

target_link_libraries(edgescope-agent PRIVATE edgescope_common)
