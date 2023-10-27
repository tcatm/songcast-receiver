cmake_minimum_required(VERSION 3.15)

project(songcast-receiver)

find_package(ohnet REQUIRED COMPONENTS ohNetCore ohNetDevices)
find_package(libxml2 REQUIRED CONFIG)
find_package(pulseaudio REQUIRED CONFIG)
find_package(uriparser REQUIRED CONFIG)
find_package(SampleRate REQUIRED CONFIG)
find_package (Threads)

add_executable(songcast-receiver main.c player.c timespec.c output.c uri.c cache.c audio_frame.c kalman.c log.c upnpdevice.c DvVolume.cpp DvInfo.cpp DvTime.cpp DvReceiver.cpp)
target_link_libraries(songcast-receiver pulseaudio::pulseaudio LibXml2::LibXml2 uriparser::uriparser m ${CMAKE_THREAD_LIBS_INIT} SampleRate::samplerate ohNetCore ohNetDevices stdc++)
set_property(TARGET songcast-receiver PROPERTY C_STANDARD 11)
