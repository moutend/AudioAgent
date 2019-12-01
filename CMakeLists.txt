cmake_minimum_required(VERSION 3.12)
PROJECT(AudioNode)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR OR EXISTS "${CMAKE_BINARY_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

if(NOT WIN32)
  message(FATAL "This project targets Windows")
endif()

# windows.winmd search path
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -AI\"C:/Program Files (x86)/Windows Kits/10/UnionMetadata/10.0.17763.0\"" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -AI\"C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/Common7/IDE/VC/vcpackages\"" )
set(CMAKE_CXX_STANDARD 17)
set(
  SOURCES
  src/dllmain.cpp
  src/context.h
  src/api.cpp
  src/api.h
  src/global.cpp
  src/global.h
  src/commandloop.cpp
  src/commandloop.h
  src/voiceloop.cpp
  src/voiceloop.h
  src/voiceinfo.cpp
  src/voiceinfo.h
  src/soundloop.cpp
  src/soundloop.h
  src/audioloop.cpp
  src/audioloop.h
  src/notification.cpp
  src/notification.h
  src/audiocore.cpp
  src/audiocore.h
  src/logloop.cpp
  src/logloop.h
  src/util.cpp
  src/util.h
)

add_subdirectory(lib/cpplogger)
add_subdirectory(lib/cppaudio)
include_directories(lib/cpplogger/src lib/cppaudio/src)

add_compile_options("/ZW")
add_library(AudioNode SHARED ${SOURCES})
target_link_libraries(AudioNode cpplogger cppaudio avrt.lib mmdevapi.lib)
