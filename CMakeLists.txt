cmake_minimum_required(VERSION 3.14)

project(ipc_study)

set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -fno-sanitize-recover=all")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -fno-sanitize-recover=all")

include_directories(.)
add_library(ipcmmap STATIC ./channels/flavors/mmap.c
                           ./channels/flavors/socket.c
                           ./channels/channel.c
)

add_executable(bench ./benchmarks/channel.bench.c)
target_link_libraries(bench PUBLIC ipcmmap)

add_subdirectory(./demos)
