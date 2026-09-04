add_executable(edgescope-client
    src/main.cpp
    src/EdgeScopeClient.cpp
)

target_include_directories(edgescope-client PRIVATE include)

target_link_libraries(edgescope-client PRIVATE
    edgescope_common
    edgescope_proto
)
