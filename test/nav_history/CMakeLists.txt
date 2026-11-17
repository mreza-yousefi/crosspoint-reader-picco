add_executable(NavHistoryTest
  NavHistoryTest.cpp
  ${REPO_ROOT}/src/activities/reader/NavHistory.cpp
)

target_include_directories(NavHistoryTest PRIVATE
  ${REPO_ROOT}/src/activities/reader
)

target_link_libraries(NavHistoryTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(NavHistoryTest)
