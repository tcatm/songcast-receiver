cmake_minimum_required (VERSION 2.6)
project(songcast-receiver)
add_executable(songcast-receiver main.c player.c timespec.c output.c uri.c cache.c audio_frame.c kalman.c)
target_link_libraries(songcast-receiver pulse-simple pulse xml2 uriparser m pthread samplerate)
set_property(TARGET songcast-receiver PROPERTY C_STANDARD 11)

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})
