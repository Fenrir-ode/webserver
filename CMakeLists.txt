cmake_minimum_required(VERSION 3.7)
project(FenrirServer)
include(ExternalProject)

# Add libchr
ExternalProject_Add(libchdr
  GIT_REPOSITORY    https://github.com/rtissera/libchdr
  GIT_TAG           master
  PREFIX            "${CMAKE_BINARY_DIR}/libchdr"
  CMAKE_ARGS        "-DINSTALL_STATIC_LIBS=On"
  INSTALL_COMMAND cmake -E echo "Skipping install step."
)

# Add gulrak/filesystem
ExternalProject_Add(ghc_fs
  GIT_REPOSITORY    https://github.com/gulrak/filesystem
  GIT_TAG           master
  PREFIX            "${CMAKE_BINARY_DIR}/ghc_fs"
  BUILD_COMMAND     cmake -E echo "Skipping build step."
  CONFIGURE_COMMAND cmake -E echo "Skipping configure step."
  INSTALL_COMMAND   cmake -E echo "Skipping install step."
)

include_directories(mongoose 
"${CMAKE_BINARY_DIR}/libchdr/src/libchdr/include"
"${CMAKE_BINARY_DIR}/ghc_fs/src/ghc_fs/include"
 )

link_directories(
  ${CMAKE_BINARY_DIR}/libchdr/src/libchdr-build
  ${CMAKE_BINARY_DIR}/libchdr/src/libchdr-build/deps/zlib-1.2.11
  ${CMAKE_BINARY_DIR}/libchdr/src/libchdr-build/deps/lzma-19.00
)

set(FenrirServerSrc 
  mongoose/mongoose.c
  # mame stuff
  src/util/chdcd.cpp
  src/util/util.cpp
  src/cdfmt.c 
  src/server.c 
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
  "${CMAKE_BINARY_DIR}/ghc_fs/src/ghc_fs/include"
  )

endif()

set(FenrirServerLibs chdr-static lzma zlib)
if(WIN32)
  set(FenrirServerLibs ${FenrirServerLibs} wsock32 ws2_32)
endif()

add_executable(FenrirServer src/cli-main.c  ${FenrirServerSrc} ${FenrirServerHdr})
add_dependencies(FenrirServer libchdr ghc_fs)
target_compile_definitions(FenrirServer PUBLIC LOG_USE_COLOR)
target_link_libraries(FenrirServer ${FenrirServerLibs})

find_package(wxWidgets COMPONENTS net gl core base)
if(wxWidgets_FOUND)
  include(${wxWidgets_USE_FILE}) 
  add_executable(FenrirServerGui 
    src/wx-main.cpp 
    src/wx-widgets/app.cpp
    src/wx-widgets/server-intf.cpp
    ${FenrirServerSrc} ${FenrirServerHdr})
  target_compile_definitions(FenrirServerGui PUBLIC LOG_USE_COLOR)
  target_link_libraries(FenrirServerGui ${FenrirServerLibs} ${wxWidgets_LIBRARIES})
endif()

install(TARGETS FenrirServer DESTINATION bin)

# tests
enable_testing()
add_subdirectory (tests/menu)

