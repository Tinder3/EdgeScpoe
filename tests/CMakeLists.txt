find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(edgescope-agent-tests
    AgentConfigTest.cpp
    DiagnosticCollectorTest.cpp
    NetworkAndLogCollectorTest.cpp
    ProcessCollectorTest.cpp
    ProcessTableModelTest.cpp
    ServiceManagerTest.cpp
    ${PROJECT_SOURCE_DIR}/agent/src/AgentConfig.cpp
    ${PROJECT_SOURCE_DIR}/agent/src/DiagnosticCollector.cpp
    ${PROJECT_SOURCE_DIR}/agent/src/LogCollector.cpp
    ${PROJECT_SOURCE_DIR}/agent/src/NetworkCollector.cpp
    ${PROJECT_SOURCE_DIR}/agent/src/ProcessCollector.cpp
    ${PROJECT_SOURCE_DIR}/agent/src/ProcessController.cpp
    ${PROJECT_SOURCE_DIR}/agent/src/ServiceManager.cpp
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
gtest_add_tests(
    TARGET edgescope-agent-tests
    TEST_LIST EDGESCOPE_TEST_CASES
)
if(EDGESCOPE_ENABLE_ASAN)
    set_tests_properties(${EDGESCOPE_TEST_CASES} PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
endif()
