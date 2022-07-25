
CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

# main.cpp  ./config/config.cpp ./utils/utils.cpp ./http/http_conn.cpp ./pool/threadpool.cpp
chenWeb: main.cpp  config/config.cpp utils/utils.cpp global/global.cpp timer/listtimer.cpp http/http_request.cpp http/http_response.cpp  http/http_conn.cpp pool/threadpool.cpp
	$(CXX) -o chenWeb  $^ $(CXXFLAGS) -lpthread -lyaml-cpp

clean:
	rm  -r chenWeb
