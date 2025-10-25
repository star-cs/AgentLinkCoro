#include "base/net/http/http.h"
#include "base/log/log.h"

void test_request()
{
    base::http::HttpRequest::ptr req(new base::http::HttpRequest);
    req->setHeader("host", "www.baidu.com");
    req->setBody("hello baidu");
    req->dump(std::cout) << std::endl;
}

void test_response()
{
    base::http::HttpResponse::ptr rsp(new base::http::HttpResponse);
    rsp->setHeader("X-X", "baidu");
    rsp->setBody("hello baidu");
    rsp->setStatus((base::http::HttpStatus)400);
    rsp->setClose(false);

    rsp->dump(std::cout) << std::endl;
}

int main(int argc, char **argv)
{
    test_request();
    test_response();
    return 0;
}
