cmake_minimum_required(VERSION 3.7)
project(FenrirServer)
include(ExternalProject)

set(CMAKE_CXX_STANDARD 20)

# Add libchr
ExternalProject_Add(libchdr
  GIT_REPOSITORY    https://github.com/rtissera/libchdr
  GIT_TAG           master
  PREFIX            "${CMAKE_BINARY_DIR}/libchdr"
  CMAKE_ARGS        "-DINSTALL_STATIC_LIBS=On"
  INSTALL_COMMAND cmake -E echo "Skipping install step."
)

# Add gulrak/filesystem
ExternalProject_Add(cJSON
  GIT_REPOSITORY    https://github.com/DaveGamble/cJSON
  GIT_TAG           master
  PREFIX            "${CMAKE_BINARY_DIR}/cJSON"
  CMAKE_ARGS        "-DENABLE_CJSON_TEST=Off" "-DBUILD_SHARED_AND_STATIC_LIBS=Off" "-DENABLE_SANITIZERS=Off" "-DENABLE_CUSTOM_COMPILER_FLAGS=Off" "-DBUILD_SHARED_LIBS=Off"
  INSTALL_COMMAND   cmake -E echo "Skipping install step."
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


include_directories(
  mongoose 
  "${CMAKE_BINARY_DIR}/libchdr/src/libchdr/include"
  "${CMAKE_BINARY_DIR}/cJSON/src/cJSON"
  "${CMAKE_SOURCE_DIR}/src/ghc/include"
 )

link_directories(
  ${CMAKE_BINARY_DIR}/libchdr/src/libchdr-build
  ${CMAKE_BINARY_DIR}/libchdr/src/libchdr-build/deps/zlib-1.2.11
  ${CMAKE_BINARY_DIR}/libchdr/src/libchdr-build/deps/lzma-19.00
  ${CMAKE_BINARY_DIR}/cJSON/src/cjson-build
)

set(FenrirServerSrc 
  mongoose/mongoose.c
  # mame stuff
  src/util/chdcd.cpp
  src/util/util.cpp
  src/cdfmt.c 
  src/server.c 
  src/data.http.c 
  src/menu.http.c 
  src/httpd.c 
  src/log.c
  src/scandir.cpp
  src/patch.c
)

file(GLOB_RECURSE FenrirServerHdr CONFIGURE_DEPENDS src/*.h)


if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(FenrirServerSrc ${FenrirServerSrc} src/windows/getopt.c)
  include_directories(src/windows mongoose 
  "${CMAKE_BINARY_DIR}/libchdr/src/libchdr/include" 
  "${CMAKE_SOURCE_DIR}/src/ghc/include"
  )

endif()

set(FenrirServerLibs chdr-static lzma zlib)
if(WIN32)
  set(FenrirServerLibs ${FenrirServerLibs} wsock32 ws2_32)
endif()

add_library(FenrirObjLib OBJECT ${FenrirServerSrc})
target_compile_definitions(FenrirObjLib PUBLIC LOG_USE_COLOR)
add_dependencies(FenrirObjLib libchdr)

add_executable(FenrirServer src/cli-main.c ${FenrirServerHdr})
add_dependencies(FenrirServer libchdr)

target_compile_definitions(FenrirServer PUBLIC LOG_USE_COLOR)
target_link_libraries(FenrirServer FenrirObjLib ${FenrirServerLibs})

set(wxWidgets_USE_UNICODE On)
set(wxWidgets_USE_UNIVERSAL On)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(wxWidgets_USE_STATIC On)
endif()

find_package(wxWidgets COMPONENTS base core gl net)
if(wxWidgets_FOUND)
  include(${wxWidgets_USE_FILE}) 
  set(GUI_FLAGS "")

  if(WIN32)
    set(GUI_FLAGS "WIN32")
  endif()

  set(FenrirServerSrc ${FenrirServerSrc} src/windows/fenrir-wx.rc)
  
  set(FenrirServerLibs ${FenrirServerLibs} cjson)

  if(WIN32)
    set(FenrirServerLibs ${FenrirServerLibs})
  endif()

  add_executable(FenrirServerGui ${GUI_FLAGS}
    src/wx-main.cpp 
    src/wx-widgets/app.cpp
    src/wx-widgets/server-intf.cpp
    ${FenrirServerHdr})
    add_dependencies(FenrirServerGui libchdr cJSON)
    target_compile_definitions(FenrirServerGui PUBLIC LOG_USE_COLOR)
    target_link_libraries(FenrirServerGui FenrirObjLib ${FenrirServerLibs} ${wxWidgets_LIBRARIES} )
  
  # target_compile_options(FenrirServerGui PRIVATE -Wall -Wextra -Wpedantic -Werror)
  
  if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set_property(TARGET FenrirServerGui PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()

endif()

install(TARGETS FenrirServer DESTINATION bin)

# tests
option(ENABLE_TEST "enable test" Off)

if (ENABLE_TEST)
  enable_testing()
  add_subdirectory (tests/menu)
endif()

