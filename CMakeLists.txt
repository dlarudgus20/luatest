cmake_minimum_required(VERSION 3.0.0)
project(luatest VERSION 0.1.0)

include(CTest)
enable_testing()

find_package(Lua51 REQUIRED)

include_directories(${LUA_INCLUDE_DIR})

add_executable(luatest main.cpp)
target_link_libraries(luatest ${LUA_LIBRARIES})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
