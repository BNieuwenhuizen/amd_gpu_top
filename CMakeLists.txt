project(amd_gpu_top)
cmake_minimum_required(VERSION 3.1)

find_package(PkgConfig REQUIRED)
pkg_search_module(PCIACCESS REQUIRED pciaccess)

add_executable(amd_gpu_top source/main.cpp)
target_link_libraries(amd_gpu_top ${PCIACCESS_LIBRARIES})
target_include_directories(amd_gpu_top PRIVATE ${PCIACCESS_INCLUDE_DIRS})
target_compile_options(amd_gpu_top PRIVATE ${PCIACCESS_CFLAGS_OTHER})

set_property(TARGET amd_gpu_top PROPERTY CXX_STANDARD 11)
set_property(TARGET amd_gpu_top PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET amd_gpu_top PROPERTY CXX_EXTENSIONS OFF)

