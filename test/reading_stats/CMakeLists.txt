add_executable(ReadingStatsTest
  ReadingStatsTest.cpp
  StatsInsightsTest.cpp
  ${REPO_ROOT}/src/stats/StatsLogic.cpp
  ${REPO_ROOT}/src/stats/StatsInsights.cpp
)

target_include_directories(ReadingStatsTest PRIVATE
  ${REPO_ROOT}/src/stats
)

target_link_libraries(ReadingStatsTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ReadingStatsTest)
