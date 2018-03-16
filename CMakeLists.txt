cmake_minimum_required (VERSION 3.1)
project (reach)

find_package(Boost 1.58.0 REQUIRED COMPONENTS system log)

set (CMAKE_CXX_STANDARD 11)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(include)

# Shared Code

add_library(reach_common STATIC src/Message.cpp)
target_link_libraries(reach_common ${Boost_LIBRARIES})

# Server Application

add_executable(reach_server server.cpp)
target_link_libraries(reach_server reach_common)

# Client Application

add_executable(reach_client client.cpp)
target_link_libraries(reach_client reach_common)
