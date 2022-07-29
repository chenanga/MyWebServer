# CMake 最低版本号要求
cmake_minimum_required(VERSION 2.6)

# 项目信息
project(chenWeb)

set(CMAKE_CXX_STANDARD 17)
set(CXX_FLAGS
        -std=c++11
        -rdynamic
        )
if(CMAKE_BUILD_BITS EQUAL 32)
    list(APPEND CXX_FLAGS "-m32")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    list(APPEND CXX_FLAGS "-Wno-null-dereference")
    list(APPEND CXX_FLAGS "-Wno-sign-conversion")
    list(APPEND CXX_FLAGS "-Wno-unused-local-typedef")
    list(APPEND CXX_FLAGS "-Wthread-safety")
    list(REMOVE_ITEM CXX_FLAGS "-rdynamic")
endif()
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Werror -Wextra -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

message(${PROJECT_BINARY_DIR})

include_directories(${PROJECT_SOURCE_DIR}) # 头文件

set(INC_DIR include)
include_directories(${INC_DIR})

# 添加子目录
add_subdirectory(lib/yaml-cpp)
add_subdirectory(config)

# 指定生成目标
add_executable(chenWeb main.cpp
        config/config.cpp pool/threadpool.cpp http/http_conn.cpp timer/listtimer.cpp utils/utils.cpp
        http/http_request.cpp http/http_response.cpp global/global.cpp log/log.cpp log/block_queue.h pool/sqlconnpool.h pool/sqlconnpool.cpp webserver/webserver.h webserver/webserver.cpp)


find_package(Threads)

# 添加链接库
target_link_libraries(chenWeb chenWeb_config)
target_link_libraries(chenWeb ${CMAKE_THREAD_LIBS_INIT} )

# mysql， 只能ubuntu使用
include_directories("/usr/include/mysql")
link_directories("usr/lib/x86_64-linux-gnu")
set(MYSQL_LIBS
        mysqlclient pthread z m rt atomic ssl crypto dl
        )
target_link_libraries(chenWeb  ${MYSQL_LIBS})
