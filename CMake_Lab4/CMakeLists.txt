cmake_minimum_required(VERSION 3.14)
project(MsgRingBufferWindows LANGUAGES CXX)

set(PROJECT_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# GoogleTest via FetchContent
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        9706f75b8f91c52a3840cf5d878a7f37ea10ef00
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Common source files
set(COMMON_HDR ${PROJECT_SRC_DIR}/header.h)
set(RECEIVER_SRC ${PROJECT_SRC_DIR}/receiver.cpp)
set(SENDER_SRC ${PROJECT_SRC_DIR}/sender.cpp)

# Target for C++11
add_executable(Receiver_cpp11 ${RECEIVER_SRC} ${COMMON_HDR})
target_compile_definitions(Receiver_cpp11 PRIVATE BUILD_STD=11)
target_compile_features(Receiver_cpp11 PRIVATE cxx_std_11)
target_include_directories(Receiver_cpp11 PRIVATE ${PROJECT_SRC_DIR})

add_executable(Sender_cpp11 ${SENDER_SRC} ${COMMON_HDR})
target_compile_definitions(Sender_cpp11 PRIVATE BUILD_STD=11)
target_compile_features(Sender_cpp11 PRIVATE cxx_std_11)
target_include_directories(Sender_cpp11 PRIVATE ${PROJECT_SRC_DIR})

# Target for C++17
add_executable(Receiver_cpp17 ${RECEIVER_SRC} ${COMMON_HDR})
target_compile_definitions(Receiver_cpp17 PRIVATE BUILD_STD=17)
target_compile_features(Receiver_cpp17 PRIVATE cxx_std_17)
target_include_directories(Receiver_cpp17 PRIVATE ${PROJECT_SRC_DIR})

add_executable(Sender_cpp17 ${SENDER_SRC} ${COMMON_HDR})
target_compile_definitions(Sender_cpp17 PRIVATE BUILD_STD=17)
target_compile_features(Sender_cpp17 PRIVATE cxx_std_17)
target_include_directories(Sender_cpp17 PRIVATE ${PROJECT_SRC_DIR})

# Tests
add_executable(Tests
        tests/tests.cpp
        ${COMMON_HDR}
)
target_link_libraries(Tests GTest::gtest_main)
target_include_directories(Tests PRIVATE ${PROJECT_SRC_DIR})
# set test standard to C++17 for tests
target_compile_features(Tests PRIVATE cxx_std_17)

include(GoogleTest)
gtest_discover_tests(Tests)
