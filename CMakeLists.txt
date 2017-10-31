cmake_minimum_required(VERSION 3.7.2)
project(lab1)

set(CMAKE_CXX_STANDARD 14)

# sequential variant
set(SEQ_SOURCE_FILES src/seq.cpp)
add_executable(seq ${SEQ_SOURCE_FILES})

# parallel variant
find_package(MPI REQUIRED)

include_directories(${MPI_INCLUDE_PATH})

set(PAR_SOURCE_FILES src/parallel.cpp)
add_executable(parallel ${PAR_SOURCE_FILES})
target_link_libraries(parallel ${MPI_LIBRARIES})

set(PAR_LB_SOURCE_FILES src/parallel_lb.cpp)
add_executable(parallel_lb ${PAR_LB_SOURCE_FILES})
target_link_libraries(parallel_lb ${MPI_LIBRARIES})

set(PAR_ASYNC_SOURCE_FILES src/parallel_async.cpp)
add_executable(parallel_async ${PAR_ASYNC_SOURCE_FILES})
target_link_libraries(parallel_async ${MPI_LIBRARIES})
