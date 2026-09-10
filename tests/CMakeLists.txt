find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(edgescope-agent-tests
    NetworkAndLogCollectorTest.cpp
    ProcessCollectorTest.cpp
    ProcessTableModelTest.cpp
    ${PROJECT_SOURCE_DIR}/agent/src/LogCollector.cpp
    ${PROJECT_SOURCE_DIR}/agent/src/NetworkCollector.cpp
    ${PROJECT_SOURCE_DIR}/agent/src/ProcessCollector.cpp
    ${PROJECT_SOURCE_DIR}/agent/src/ProcessController.cpp
    ${PROJECT_SOURCE_DIR}/client/src/ProcessTableModel.cpp
    ${PROJECT_SOURCE_DIR}/client/include/ProcessTableModel.h
)

set_target_properties(edgescope-agent-tests PROPERTIES AUTOMOC ON)

target_include_directories(edgescope-agent-tests PRIVATE
    ${PROJECT_SOURCE_DIR}/agent/include
    ${PROJECT_SOURCE_DIR}/client/include
)

target_link_libraries(edgescope-agent-tests PRIVATE
    GTest::gtest_main
    Qt6::Core
)

include(GoogleTest)
gtest_discover_tests(edgescope-agent-tests)
