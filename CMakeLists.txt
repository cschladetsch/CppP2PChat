cmake_minimum_required(VERSION 3.20)
project(P2PChat VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Bin)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)

# FetchContent for external dependencies
include(FetchContent)

# Fetch replxx for vi-like CLI
FetchContent_Declare(
    replxx
    GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx
    GIT_TAG        release-0.0.4
)

# Fetch cppzmq (C++ bindings for ZeroMQ)
FetchContent_Declare(
    cppzmq
    GIT_REPOSITORY https://github.com/zeromq/cppzmq
    GIT_TAG        v4.10.0
)

# Fetch libzmq
FetchContent_Declare(
    libzmq
    GIT_REPOSITORY https://github.com/zeromq/libzmq
    GIT_TAG        v4.3.5
)

set(ZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libzmq cppzmq replxx)

# Download rang.h
file(DOWNLOAD 
    https://raw.githubusercontent.com/agauniyal/rang/master/include/rang.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/rang.hpp
    SHOW_PROGRESS
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# Source files
set(SOURCES
    Source/Main.cpp
    Source/Crypto.cpp
    Source/Network.cpp
    Source/Message.cpp
    Source/PeerManager.cpp
    Source/CliInterface.cpp
)

# Create executable
add_executable(p2pchat ${SOURCES})

# Link libraries
target_link_libraries(p2pchat
    libzmq-static
    cppzmq-static
    ${OPENSSL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    replxx::replxx
)

# Include directories for cppzmq
target_include_directories(p2pchat PRIVATE ${cppzmq_SOURCE_DIR})

# Compiler warnings
if(MSVC)
    target_compile_options(p2pchat PRIVATE /W4)
else()
    target_compile_options(p2pchat PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install target
install(TARGETS p2pchat DESTINATION bin)

# Testing
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    
    # Fetch Google Test
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    
    # Create a library with all source files except main
    add_library(p2pchat_lib STATIC
        Source/Crypto.cpp
        Source/Network.cpp
        Source/Message.cpp
        Source/PeerManager.cpp
        Source/CliInterface.cpp
    )
    
    target_link_libraries(p2pchat_lib
        libzmq-static
        cppzmq-static
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        replxx::replxx
    )
    
    target_include_directories(p2pchat_lib PRIVATE ${cppzmq_SOURCE_DIR})
    
    # Test executables
    add_executable(TestCrypto Tests/TestCrypto.cpp)
    target_link_libraries(TestCrypto 
        p2pchat_lib
        gtest_main
    )
    
    add_executable(TestMessage Tests/TestMessage.cpp)
    target_link_libraries(TestMessage 
        p2pchat_lib
        gtest_main
    )
    
    add_executable(TestPeerManager Tests/TestPeerManager.cpp)
    target_link_libraries(TestPeerManager 
        p2pchat_lib
        gtest_main
    )
    
    add_executable(TestNetwork Tests/TestNetwork.cpp)
    target_link_libraries(TestNetwork 
        p2pchat_lib
        gtest_main
        gmock
    )
    
    add_executable(TestCliInterface Tests/TestCliInterface.cpp)
    target_link_libraries(TestCliInterface 
        p2pchat_lib
        gtest_main
    )
    
    # Add tests
    include(GoogleTest)
    gtest_discover_tests(TestCrypto)
    gtest_discover_tests(TestMessage)
    gtest_discover_tests(TestPeerManager)
    gtest_discover_tests(TestNetwork)
    gtest_discover_tests(TestCliInterface)
endif()