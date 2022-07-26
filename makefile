
CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

# centos 和 ubuntu 都可使用
chenWeb: main.cpp  config/config.cpp utils/utils.cpp global/global.cpp log/log.cpp pool/sqlconnpool.cpp timer/listtimer.cpp http/http_request.cpp http/http_response.cpp  http/http_conn.cpp pool/threadpool.cpp
	$(CXX) -std=c++11 -o chenWeb  $^ $(CXXFLAGS) -lpthread -lyaml-cpp -L/usr/lib64/mysql -lmysqlclient   -I/usr/include/mysql

clean:
	rm  -r bin/chenWeb


## ubuntu 使用
#chenWeb: main.cpp  config/config.cpp utils/utils.cpp global/global.cpp log/log.cpp pool/sqlconnpool.cpp timer/listtimer.cpp http/http_request.cpp http/http_response.cpp  http/http_conn.cpp pool/threadpool.cpp
#	$(CXX) -std=c++11 -o chenWeb  $^ $(CXXFLAGS) -lpthread -lyaml-cpp -lmysqlclient
#
#clean:
#	rm  -r bin/chenWeb
