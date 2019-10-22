cmake_minimum_required(VERSION 3.14)
PROJECT(AudioAgent)
set(CMAKE_CXX_STANDARD 17)

# git branch | grep \* | cut -d ' ' -f2
# git rev-parse --verify HEAD

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR OR EXISTS "${CMAKE_BINARY_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

if(NOT WIN32)
  message(FATAL "This project targets Windows")
endif()

# These settings targets VisualStudio 2019. Change metadata1 and metadata2 according the VisualStudio version.
set(metadata1 "C:\\Program Files (x86)\\Windows Kits\\10\\UnionMetadata\\10.0.18362.0")
set(metadata2 "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\Common7\\IDE\\VC\\vcpackages")

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

include_directories(lib)

set(
  LIBRARY_SOURCES
  lib/logger/logger.h
  lib/logger/logger.cpp
  lib/pcmaudio/reader.h
  lib/pcmaudio/reader.cpp
  lib/pcmaudio/wave.h
  lib/pcmaudio/wave.cpp
  lib/pcmaudio/engine.h
  lib/pcmaudio/engine.cpp
)

add_library(libhelper STATIC ${LIBRARY_SOURCES})
add_compile_options("/ZW")
add_library(AudioAgent SHARED ${SOURCES})

target_compile_options(AudioAgent PRIVATE $<$<BOOL:${MSVC}>:/AI${metadata1} /AI${metadata2}>)
target_link_libraries(AudioAgent libhelper avrt.lib mmdevapi.lib)
