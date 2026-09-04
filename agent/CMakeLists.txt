add_executable(edgescope-agent
    src/main.cpp
    src/EdgeScopeServiceImpl.cpp
    src/SystemCollector.cpp
)

target_include_directories(edgescope-agent PRIVATE include)

target_link_libraries(edgescope-agent PRIVATE
    edgescope_common
    edgescope_proto
)
