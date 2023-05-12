cmake_minimum_required(VERSION 3.0.0)
project(luatest VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

find_package(fmt)
find_package(glm)
find_package(Lua51 REQUIRED)
find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)

add_executable(luatest main.cpp)

target_include_directories(luatest PRIVATE ${LUA_INCLUDE_DIR})

target_link_libraries(luatest fmt::fmt)
target_link_libraries(luatest glm::glm)
target_link_libraries(luatest ${LUA_LIBRARIES})

if (TARGET SDL2::SDL2main)
    target_link_libraries(luatest SDL2::SDL2main)
endif()
target_link_libraries(luatest SDL2::SDL2)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
