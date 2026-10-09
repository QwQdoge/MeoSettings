cmake_minimum_required(VERSION 3.21)

project(MeoSettingsSessionEntryCoreTests LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Test)

add_executable(meo-settings-session-entry-core-test
    ../tst_sessionentrydocument.cpp
    ../../src/core/sessionentrydocument.cpp
    ../../src/core/sessionentrydocument.h
    ../../src/core/lockscreenconfigstore.cpp
    ../../src/core/lockscreenconfigstore.h
)

target_include_directories(meo-settings-session-entry-core-test PRIVATE
    ../../src/core
)

target_link_libraries(meo-settings-session-entry-core-test PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(
    NAME meo-settings-session-entry-core
    COMMAND meo-settings-session-entry-core-test
)
