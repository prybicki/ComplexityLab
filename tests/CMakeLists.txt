add_executable(testLib
    testFact.cpp
)
target_link_libraries(testLib PRIVATE GTest::gtest_main cl_lib)
gtest_discover_tests(testLib PROPERTIES LABELS host)
