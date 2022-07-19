cmake_minimum_required(VERSION 3.20)
project(myweb)

set(CMAKE_CXX_STANDARD 14)

add_executable(myweb main.cpp pool/threadpool.cpp pool/threadpool.h config/config.cpp config/config.h log/log.h http/http_conn.h http/http_conn.cpp utils/utils.h utils/utils.cpp)
message(${PROJECT_BINARY_DIR})
message(${PROJECT_SOURCE_DIR})