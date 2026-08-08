cmake_minimum_required(VERSION 3.15)
project(sideous)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(dpf)
add_subdirectory(plugins/Sideous)
