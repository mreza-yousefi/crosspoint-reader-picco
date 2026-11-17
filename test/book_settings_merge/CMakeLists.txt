# ArduinoJson as pinned in platformio.ini (7.4.2). Same declaration as the
# BlePageTurner tests, so both share one checkout; offline builds can set
# -DFETCHCONTENT_SOURCE_DIR_ARDUINOJSON=<ArduinoJson>.
FetchContent_Declare(arduinojson GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git GIT_TAG v7.4.2
                     SOURCE_SUBDIR no-build)
FetchContent_MakeAvailable(arduinojson)

add_executable(BookSettingsMergeTest
  BookSettingsMergeTest.cpp
  ${REPO_ROOT}/src/util/BookSettingsMerge.cpp
)

target_include_directories(BookSettingsMergeTest PRIVATE
  ${REPO_ROOT}/src/util
)
target_include_directories(BookSettingsMergeTest SYSTEM PRIVATE
  ${arduinojson_SOURCE_DIR}/src
)

target_link_libraries(BookSettingsMergeTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BookSettingsMergeTest)
