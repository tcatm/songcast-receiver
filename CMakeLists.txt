set(ohNetPath "/home/pi/openhome-slave/ohNet")

cmake_minimum_required (VERSION 2.6)
INCLUDE(TestBigEndian)

project(songcast-receiver)
add_executable(songcast-receiver main.c player.c timespec.c output.c uri.c cache.c audio_frame.c kalman.c log.c upnpdevice.c DvVolume.cpp DvInfo.cpp DvTime.cpp DvReceiver.cpp)
target_link_libraries(songcast-receiver pulse-simple pulse xml2 uriparser m pthread samplerate ohNet ohNetDevices stdc++)
set_property(TARGET songcast-receiver PROPERTY C_STANDARD 11)

link_directories(/home/pi/openhome-slave/ohNet/Build/Obj/Posix/Release/)

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

add_library(ohNet SHARED IMPORTED)
set_target_properties(ohNet PROPERTIES
  IMPORTED_LOCATION "${ohNetPath}/Build/Obj/Posix/Release/libohNet.so"
  INTERFACE_INCLUDE_DIRECTORIES "${ohNetPath}/Build/Include"
)

add_library(ohNetDevices SHARED IMPORTED)
set_target_properties(ohNetDevices PROPERTIES
  IMPORTED_LOCATION "${ohNetPath}/Build/Obj/Posix/Release/DvAvOpenhomeOrgProduct1C.o"
  INTERFACE_INCLUDE_DIRECTORIES "${ohNetPath}/Build/Include"
)
TEST_BIG_ENDIAN(BIG_ENDIAN)
if(${BIG_ENDIAN})
  ADD_DEFINITIONS(-DDEFINE_BIG_ENDIAN)
else()
  ADD_DEFINITIONS(-DDEFINE_LITTLE_ENDIAN)
endif(${BIG_ENDIAN})