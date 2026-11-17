add_executable(LookupRecordGateTest LookupRecordGateTest.cpp)

target_include_directories(LookupRecordGateTest PRIVATE ../../src)

target_link_libraries(LookupRecordGateTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(LookupRecordGateTest)
