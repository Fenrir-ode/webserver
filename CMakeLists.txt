cmake_minimum_required(VERSION 3.7)
project(FenrirServer)

# Add libchr
include(ExternalProject)
ExternalProject_Add(libchdr
  GIT_REPOSITORY    https://github.com/rtissera/libchdr
  GIT_TAG           master
  PREFIX            "${CMAKE_BINARY_DIR}/libchdr"
  CMAKE_ARGS        "-DINSTALL_STATIC_LIBS=On"
  INSTALL_COMMAND cmake -E echo "Skipping install step."
)

include_directories(mongoose "${CMAKE_BINARY_DIR}/libchdr/src/libchdr/include" )
link_directories(
  ${CMAKE_BINARY_DIR}/libchdr/src/libchdr-build
  ${CMAKE_BINARY_DIR}/libchdr/src/libchdr-build/deps/zlib-1.2.11
  ${CMAKE_BINARY_DIR}/libchdr/src/libchdr-build/deps/lzma-19.00
)

add_executable(FenrirServer 
    mongoose/mongoose.c
    # mame stuff
    src/util/chdcd.cpp
    src/util/util.cpp
    src/cdfmt.c 
    src/server.c 
    src/cli-main.c 
    src/menu.http.c 
    src/httpd.c 
    src/log.c
)

add_dependencies(FenrirServer libchdr)
target_compile_definitions(FenrirServer PUBLIC  LOG_USE_COLOR)
target_link_libraries(FenrirServer chdr-static lzma zlib)

install(TARGETS FenrirServer DESTINATION bin)