cmake_minimum_required (VERSION 3.1)
project (reach)

find_package(Boost 1.58.0 REQUIRED system)

set (CMAKE_CXX_STANDARD 11)

add_executable(reach_server server.cpp)
target_link_libraries(reach_server ${Boost_LIBRARIES})