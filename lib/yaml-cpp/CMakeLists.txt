set(INC_DIR include)
include_directories(${INC_DIR})

FILE(GLOB YAML-CPP-SOURCES "*.cpp")

# 生成链接库
add_library(yaml-cpp ${YAML-CPP-SOURCES})
