cmake_minimum_required (VERSION 2.6)
project(songcast-receiver)
add_executable(songcast-receiver main.c timespec.c)
target_link_libraries(songcast-receiver pulse-simple pulse xml2 uriparser)

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})
