cmake_minimum_required(VERSION 3.10)
project(FontBuilder CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

add_executable(FontBuilder FontBuilder.cpp)

if(TARGET SDL2::SDL2)
    target_link_libraries(FontBuilder PRIVATE SDL2::SDL2 SDL2::SDL2main)
else()
    target_include_directories(FontBuilder PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(FontBuilder PRIVATE ${SDL2_LIBRARIES})
endif()

if(TARGET SDL2_ttf::SDL2_ttf)
    target_link_libraries(FontBuilder PRIVATE SDL2_ttf::SDL2_ttf)
else()
    target_include_directories(FontBuilder PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_link_libraries(FontBuilder PRIVATE ${SDL2_TTF_LIBRARIES})
endif()
