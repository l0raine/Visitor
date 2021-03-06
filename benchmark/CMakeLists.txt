cmake_minimum_required (VERSION 3.14)

# ---- create project ----

project(LarsVisitorBenchmark 
  LANGUAGES CXX
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CPM.cmake)

CPMAddPackage(
  NAME LarsVisitor
  GIT_REPOSITORY https://github.com/TheLartians/Visitor.git
  GIT_TAG master
)

CPMAddPackage(
  NAME googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  VERSION 1.4.1
  OPTIONS
   "BENCHMARK_ENABLE_TESTING Off"
   "BENCHMARK_USE_LIBCXX ON"
)

# fix google benchmark
set_target_properties(benchmark PROPERTIES CXX_STANDARD 17)        

add_executable(LarsVisitorBenchmark "benchmark.cpp")
target_link_libraries(LarsVisitorBenchmark LarsVisitor benchmark)
set_target_properties(LarsVisitorBenchmark PROPERTIES CXX_STANDARD 17)        
