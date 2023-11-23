cmake_minimum_required(VERSION 3.1)
project(MyPcapPlusPlusProject)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)

# Find PcapPlusPlus library
find_package(PcapPlusPlus REQUIRED)

# Add your project source files and link against PcapPlusPlus library
add_executable(hello_world hello_world.cpp)
target_link_libraries(hello_world PcapPlusPlus::Packet++ PcapPlusPlus::Pcap++ PcapPlusPlus::Common++)

add_executable(capture capture.cpp)
target_link_libraries(capture PcapPlusPlus::Packet++ PcapPlusPlus::Pcap++ PcapPlusPlus::Common++)

add_executable(create_send create_send.cpp)
target_link_libraries(create_send PcapPlusPlus::Packet++ PcapPlusPlus::Pcap++ PcapPlusPlus::Common++)

add_executable(arp arp.cpp)
target_link_libraries(arp PcapPlusPlus::Packet++ PcapPlusPlus::Pcap++ PcapPlusPlus::Common++)