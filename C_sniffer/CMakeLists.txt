cmake_minimum_required(VERSION 3.1)
project(MyPcapPlusPlusProject)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)

# Find PcapPlusPlus library
find_package(PcapPlusPlus REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpqxx")
set(PQXX /usr/local/include/pqxx)
find_library(PQXX_LIB pqxx)
# Include the nlohmann/json library
include_directories(/usr/include/nlohmann)
include_directories(/usr/include)

# Add your project source files and link against PcapPlusPlus library
add_executable(hello_world hello_world.cpp)
target_link_libraries(hello_world PcapPlusPlus::Packet++ PcapPlusPlus::Pcap++ PcapPlusPlus::Common++ ${PQXX_LIB})

add_executable(capture capture.cpp)
target_link_libraries(capture PcapPlusPlus::Packet++ PcapPlusPlus::Pcap++ PcapPlusPlus::Common++ ${PQXX_LIB})

add_executable(create_send create_send.cpp)
target_link_libraries(create_send PcapPlusPlus::Packet++ PcapPlusPlus::Pcap++ PcapPlusPlus::Common++ ${PQXX_LIB})

add_executable(test test.cpp)
target_link_libraries(test PcapPlusPlus::Packet++ PcapPlusPlus::Pcap++ PcapPlusPlus::Common++ ${PQXX_LIB})