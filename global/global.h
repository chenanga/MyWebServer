

#ifndef CHENWEB_GLOBAL_H
#define CHENWEB_GLOBAL_H

#include <iostream>

// 全局变量定义

extern std::string g_str_cur_dir;

const int FILENAME_LEN = 256;  //设置读取文件的名称m_real_file大小
const int READ_BUFFER_SIZE = 2048;  //设置读缓冲区m_read_buf大小
const int WRITE_BUFFER_SIZE = 1024;  //设置写缓冲区m_write_buf大小

#endif //CHENWEB_GLOBAL_H
