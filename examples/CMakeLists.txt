cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

# ---- Project ----

project(LarsVisitorExamples CXX)

# ---- Dependencies ----

include(../cmake/CPM.cmake)

if (NOT TARGET LarsVisitor)
  find_package(LarsVisitor)
endif()

# ---- Create binaries ----

file(GLOB example_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

foreach( example_source_file ${example_sources} )
  get_filename_component(filename ${example_source_file} NAME)
  string(REPLACE ".cpp" "" example_name ${filename} )
  add_executable(${example_name} ${example_source_file})
  target_link_libraries(${example_name} LarsVisitor)   
  set_target_properties(${example_name} PROPERTIES CXX_STANDARD 17 COMPILE_FLAGS "-Wall -pedantic -Wextra -Werror")        
endforeach()

