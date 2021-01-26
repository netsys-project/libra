cmake_minimum_required (VERSION 2.8)

# projectname is the same as the main-executable
project(xflow)

SET(PLATFORM_SPECIFIC_LIBS "-lpthread")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
add_definitions('-g')
add_definitions('-Wall')
add_definitions('-std=c++11')
add_definitions('-lz -lrt -lm -lpthread ')

include_directories(
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/ps-lite/include"
  "${PROJECT_BINARY_DIR}"
)
message(${PROJECT_SOURCE_DIR})

link_directories(

  "${PROJECT_BINARY_DIR}"
  "${PROJECT_BINARY_DIR}/ps-lite"
)

add_subdirectory(ps-lite)
add_subdirectory(src/io)
add_subdirectory(src/model)
#add_subdirectory(src/c_api)
