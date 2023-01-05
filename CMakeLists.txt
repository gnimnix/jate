cmake_minimum_required(VERSION 3.24)
project(jate C) # Create project jate that is in C

# Add jate.c file of project root directory as source file
set(SOURCE_FILES jate.c)

# Add executable target with source files listed in SOURCE_FILES variable
add_executable(jate ${SOURCE_FILES})

add_compile_options(-Wall -Wextra -pedantic -std=c99)