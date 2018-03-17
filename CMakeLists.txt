cmake_minimum_required (VERSION 3.1)
project (reach)

find_package(Boost 1.58.0 REQUIRED COMPONENTS system log unit_test_framework)

set (CMAKE_CXX_STANDARD 11)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(include)

# Shared Code

add_library(reach_common STATIC src/Message.cpp)
target_link_libraries(reach_common ${Boost_LIBRARIES})

# Server Application
include_directories(.)
add_executable(reach_server server.cpp)
target_link_libraries(reach_server reach_common)

# Client Application

add_executable(reach_client client.cpp)
target_link_libraries(reach_client reach_common)


# Unit Test
enable_testing()

# Find all cpp files starting with test_
file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test/test_*.cpp)

#Run through each source
foreach(testSrc ${TEST_SRCS})
        #Extract the filename without an extension (NAME_WE)
        get_filename_component(testName ${testSrc} NAME_WE)

        #Add compile target
        add_executable(${testName} ${testSrc})

        #link to Boost libraries AND your targets and dependencies
        target_link_libraries(${testName} ${Boost_LIBRARIES} reach_common)

        #I like to move testing binaries into a testBin directory
        set_target_properties(${testName} PROPERTIES 
            RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/testBin)

        #Finally add it to test execution - 
        #Notice the WORKING_DIRECTORY and COMMAND
        add_test(NAME ${testName} 
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/testBin 
                 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testBin/${testName} )
endforeach(testSrc)