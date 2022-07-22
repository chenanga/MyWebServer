
#ifndef CHENWEB_HTTP_RESPONSE_H
#define CHENWEB_HTTP_RESPONSE_H

#include "state_code.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void init(HTTP_CODE read_ret);
    bool generate_response(char * buf, int &bytes_to_send, struct stat & m_file_stat, struct iovec & m_iv_0, struct iovec & m_iv_1, char * &file_address);

private:

};


#endif //CHENWEB_HTTP_RESPONSE_H
