
#include "global.h"

std::string kStrCurDir;

// 定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form =
    "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form =
    "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form =
    "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form =
    "There was an unusual problem serving the request file.\n";

/*
    对于const int 如果extern和定义分开会出bug
    char数组使用const int初始化时候，被认为是非常量无法初始化
    使用 constpexpr int 声明时候，extern和定义无法分开存放，会报错
    故最终把const int的声明定义都放在.h文件
*/