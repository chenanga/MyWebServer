# chenWeb
## 项目描述
Linux下C++轻量级Web服务器，基于非阻塞I/O和模拟Proactor事件处理模式的高并发服务器，支持解析GET、POST请求，能够响应静态资源的请求。
 
- 使用线程池、非阻塞Socket、Epoll (LT模式)和模拟Proactor实现的并发模型；
- 使用状态机解析HTTP请求报文，支持GET和POST请求；
- 基于升序的双向链表实现的超时长连接清除，异步的日志系统；
- 访问服务器数据库实现Web端用户注册、登录功能，可以请求服务器图片和视频文件；
- 支持YAML文件配置。

## 在线访问

## 演示

## 目录


## 安装和使用
### 准备工作
* 服务器测试环境
    * Ubuntu版本18.04
    * MySQL版本5.7.38
* 浏览器测试环境
    * Windows、Linux均可
    * Chrome
    * FireFox
    * edge

* 测试前确认已安装MySQL数据库

```mysql
# 建立 webserver 库
create database webserver;

# 创建user表
USE webserver;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

# 添加数据
INSERT INTO user(username, password) VALUES('chen', '12345678');
```

* 修改config.yaml中的数据库初始化信息

```yaml
# 数据库登录名,密码,库名
# 数据库相关参数
databaseParameter:

  # 登录用户名
  user: "chen"

  # 登录密码
  passwd: "12345678"

  # 数据库名称
  databasename: "webserver"
```

### 服务器端代码编译构建 （以下方式选其一即可）
#### 方式1： CMake编译构建 (推荐)
**环境：**
- cmake >= 3.20
- gcc
- g++

1. 构建编译
```
cd 当前目录
sh ./build.sh
```
2. 运行
```bash
./bin/chenWeb
```


#### 方式2：makefile编译构建
**环境：**
- cmake >= 3.20
- gcc
- g++


1. 需要先配置yaml-cpp
```c++
// yaml-cpp 配置
git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp/
mkdir build && cd build
cmake .. && make -j
sudo make install
```

2. 构建编译本项目
```c++
// chenWeb 配置
cd 当前目录
make
```
3. 运行
```c++
./chenWeb
```

### 浏览器端运行
```c++
ip:10000/
```

## 项目结构

```c++
chenWeb/

|-- config/                              // yaml配置文件解析相关
|-- http/                                // http连接处理解析
|-- lock/                                // 互斥锁
|-- log/                                 // 日志
|-- pool/                                // 线程池
|-- utils/                            	 // 工具类
|-- include/                             // 依赖的第三方库的头文件
|   |--yaml-cpp/                         // yaml配置文件解析
|-- lib/                                 // 依赖的第三方库
|   |--yaml-cpp/                         // yaml配置文件解析
|-- config.yaml                          // 配置文件
|-- main.cpp                             // 入口函数
|-- CMakeLists.txt
|-- makefile     
|-- build.sh
|-- README.md
```

## 测试

## TODO
1、http解析使用正则表达式

2、上传/下载文件

3、解析更多的http头部字段

4、加入cookie或者session验证

5、密码MD5加密


## 致谢
1. [Github jbeder/yaml-cpp](https://github.com/jbeder/yaml-cpp)

