cmake_minimum_required(VERSION 3.16)
project(OutlawMode CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCES src/*.cpp)
add_library(OutlawMode SHARED ${SOURCES})
set_target_properties(OutlawMode PROPERTIES PREFIX "" SUFFIX ".asi")
target_compile_definitions(OutlawMode PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(OutlawMode PRIVATE shell32)

if(MINGW)
  target_compile_options(OutlawMode PRIVATE -Wall -Wextra -Wno-unused-parameter)
  # One self-contained file: no MinGW runtime DLLs to ship.
  target_link_options(OutlawMode PRIVATE -static -static-libgcc -static-libstdc++ -s)
elseif(MSVC)
  target_compile_options(OutlawMode PRIVATE /W4 /MT)
endif()
