enable_testing()

set(TestSrc 
  ${CMAKE_SOURCE_DIR}/src/log.c
  ${CMAKE_SOURCE_DIR}/src/util/chdcd.cpp
  ${CMAKE_SOURCE_DIR}/src/cdfmt.c
)

include_directories(${CMAKE_SOURCE_DIR}/mongoose 
  "${CMAKE_BINARY_DIR}/libchdr/src/libchdr/include"
  "${CMAKE_BINARY_DIR}/ghc_fs/src/ghc_fs/include"
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/tests 
 )

add_executable(TocNekketsu ${TestSrc} nekketsu.c)

target_link_libraries(TocNekketsu ${MINGW_STATIC} chdr-static lzma zlib)

add_test(NAME TocNekketsu COMMAND TocNekketsu)