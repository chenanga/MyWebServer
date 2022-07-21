set(config_SRCS config.cpp)

set(INC_DIR include)
include_directories(${INC_DIR})

add_library(chenWeb_config ${config_SRCS})
target_link_libraries(chenWeb_config yaml-cpp)