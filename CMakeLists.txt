cmake_minimum_required(VERSION 3.16)
project(trading_bot CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for minimum latency
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

add_executable(trading_bot
    src/main.cpp
    src/data_fetcher.cpp
    src/indicators.cpp
    src/strategies.cpp
    src/simulator.cpp
)

target_include_directories(trading_bot PRIVATE include)
target_link_libraries(trading_bot PRIVATE CURL::libcurl Threads::Threads)
