cmake_minimum_required(VERSION 3.10)
project(FenrirServer)

# Add libchr
include(ExternalProject)
ExternalProject_Add(libchdr
  GIT_REPOSITORY    https://github.com/rtissera/libchdr
  GIT_TAG           master
  SOURCE_DIR        "${CMAKE_BINARY_DIR}/libchdr-src"
  BINARY_DIR        "${CMAKE_BINARY_DIR}/libchdr-build"
  CMAKE_ARGS        "-DINSTALL_STATIC_LIBS=On"
)

include_directories(mongoose "${CMAKE_BINARY_DIR}/libchdr-src/include" )

add_executable(FenrirServer 
    mongoose/mongoose.c
    # mame stuff
    src/util/chdcd.cpp
    src/util/util.cpp
    src/cdfmt.c 
    src/main.c 
    src/httpd.c 
    src/log.c
)

add_dependencies(FenrirServer libchdr)
target_compile_definitions(FenrirServer PUBLIC  LOG_USE_COLOR)
target_link_libraries(FenrirServer chdr-static)
