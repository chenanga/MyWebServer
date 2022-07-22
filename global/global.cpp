
#include "global.h"

std::string g_str_cur_dir;

/*
    对于const int 如果extern和定义分开会出bug
    char数组使用const int初始化时候，被认为是非常量无法初始化
    使用 constpexpr int 声明时候，extern和定义无法分开存放，会报错
    故最终把const int的声明定义都放在.h文件
*/