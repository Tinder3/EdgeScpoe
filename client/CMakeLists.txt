add_executable(edgescope-cli
    src/main.cpp
    src/EdgeScopeClient.cpp
)

target_include_directories(edgescope-cli PRIVATE include)

target_link_libraries(edgescope-cli PRIVATE
    edgescope_common
    edgescope_proto
)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_executable(edgescope-client
    src/gui_main.cpp
    src/EdgeScopeClient.cpp
    src/MainWindow.cpp
    src/ProcessTableModel.cpp
    src/RpcWorker.cpp
    include/MainWindow.h
    include/ProcessTableModel.h
    include/RpcWorker.h
)

set_target_properties(edgescope-client PROPERTIES
    AUTOMOC ON
)

target_include_directories(edgescope-client PRIVATE include)

target_link_libraries(edgescope-client PRIVATE
    edgescope_common
    edgescope_proto
    Qt6::Core
    Qt6::Widgets
)
