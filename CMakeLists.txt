cmake_minimum_required(VERSION 3.5)
project(chenWeb)

set(CMAKE_CXX_STANDARD 14)

message(${PROJECT_BINARY_DIR})

set(INC_DIR include)
include_directories(${INC_DIR})
add_subdirectory(lib/yaml-cpp)
add_subdirectory(config)
add_executable(chenWeb main.cpp
        config/config.cpp pool/threadpool.cpp http/http_conn.cpp utils/utils.cpp
        http/http_request.cpp http/http_response.cpp global/global.cpp)


find_package(Threads)
target_link_libraries(chenWeb chenWeb_config)
target_link_libraries(chenWeb ${CMAKE_THREAD_LIBS_INIT} )

