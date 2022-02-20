cmake_minimum_required(VERSION 3.4)
project(lab_1 CXX)
set(CMAKE_CXX_STANDRARD 20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") # using Clang  
add_compile_options(-pedantic) 
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # using GCC
add_compile_options(-pedantic)
endif()


add_executable(
    lab_1
    main.cpp
)