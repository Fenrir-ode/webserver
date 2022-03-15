
include(CodeCoverage.cmake)


add_executable(TestMenu 
${CMAKE_SOURCE_DIR}/src/log.c
${CMAKE_SOURCE_DIR}/src/httpd.c
${CMAKE_SOURCE_DIR}/src/menu.http.c
${CMAKE_SOURCE_DIR}/mongoose/mongoose.c
menu.c)

include_directories(${CMAKE_SOURCE_DIR}/mongoose 
"${CMAKE_BINARY_DIR}/libchdr/src/libchdr/include"
"${CMAKE_BINARY_DIR}/ghc_fs/src/ghc_fs/include"
${CMAKE_SOURCE_DIR}/src
${CMAKE_SOURCE_DIR}/tests 
 )

if(WIN32)
  target_link_libraries(TestMenu wsock32 ws2_32)
endif()

add_test(NAME TestMenu COMMAND TestMenu)
target_code_coverage(TestMenu 
EXCLUDE ${CMAKE_SOURCE_DIR}/src/log.c ${CMAKE_SOURCE_DIR}/tests/* ${CMAKE_SOURCE_DIR}/mongoose/* ${CMAKE_SOURCE_DIR}/src/httpd.c
)
