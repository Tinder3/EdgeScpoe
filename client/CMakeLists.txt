add_executable(edgescope-client src/main.cpp)

target_link_libraries(edgescope-client PRIVATE edgescope_common)
