cmake_minimum_required(VERSION 3.15)
project(mxy-sandbox-core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

file(GLOB SANDBOX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(sandbox-core ${SANDBOX_SOURCES})
target_include_directories(sandbox-core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(WIN32)
    target_link_libraries(sandbox-core PRIVATE ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(sandbox-core PRIVATE Threads::Threads)
endif()

if(MSVC)
    target_compile_options(sandbox-core PRIVATE /W3 /utf-8)
else()
    target_compile_options(sandbox-core PRIVATE -Wall -Wextra -O2)
endif()
