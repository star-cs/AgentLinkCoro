#include <iostream>
#include "base/net/http/http_connection.h"
#include "base/log/log.h"
#include "base/coro/iomanager.h"
#include "base/net/http/http_parser.h"
#include "base/net/streams/zlib_stream.h"
#include <fstream>

static base::Logger::ptr g_logger = _LOG_ROOT();

void test_pool()
{
    base::http::HttpConnectionPool::ptr pool(
        new base::http::HttpConnectionPool("www.baidu.com", "", 80, false, 10, 1000 * 30, 5));

    base::IOManager::GetThis()->addTimer(
        1000,
        [pool]() {
            auto r = pool->doGet("/", 300);
            _LOG_INFO(g_logger) << r->toString();
        },
        true);
}

void run()
{
    base::Address::ptr addr = base::Address::LookupAnyIPAddress("www.baidu.com:80");
    if (!addr) {
        _LOG_INFO(g_logger) << "get addr error";
        return;
    }

    base::Socket::ptr sock = base::Socket::CreateTCP(addr);
    bool rt = sock->connect(addr);
    if (!rt) {
        _LOG_INFO(g_logger) << "connect " << *addr << " failed";
        return;
    }

    base::http::HttpConnection::ptr conn(new base::http::HttpConnection(sock));
    base::http::HttpRequest::ptr req(new base::http::HttpRequest);
    req->setPath("/blog/");
    req->setHeader("host", "www.sylar.top");
    _LOG_INFO(g_logger) << "req:" << std::endl << *req;

    conn->sendRequest(req);
    auto rsp = conn->recvResponse();

    if (!rsp) {
        _LOG_INFO(g_logger) << "recv response error";
        return;
    }
    _LOG_INFO(g_logger) << "rsp:" << std::endl << *rsp;

    std::ofstream ofs("rsp.dat");
    ofs << *rsp;

    _LOG_INFO(g_logger) << "=========================";

    auto r = base::http::HttpConnection::DoGet("http://www.baidu.com/", 300);
    _LOG_INFO(g_logger) << "result=" << r->result << " error=" << r->error
                        << " rsp=" << (r->response ? r->response->toString() : "");

    _LOG_INFO(g_logger) << "=========================";
    test_pool();
}

void test_https()
{
    auto r = base::http::HttpConnection::DoGet("http://www.baidu.com/", 300,
                                               {{"Accept-Encoding", "gzip, deflate, br"},
                                                {"Connection", "keep-alive"},
                                                {"User-Agent", "curl/7.29.0"}});
    _LOG_INFO(g_logger) << "result=" << r->result << " error=" << r->error
                        << " rsp=" << (r->response ? r->response->toString() : "");

    // base::http::HttpConnectionPool::ptr pool(new base::http::HttpConnectionPool(
    //             "www.baidu.com", "", 80, false, 10, 1000 * 30, 5));
    auto pool =
        base::http::HttpConnectionPool::Create("https://www.baidu.com", "", 10, 1000 * 30, 5);
    base::IOManager::GetThis()->addTimer(
        1000,
        [pool]() {
            auto r = pool->doGet(
                "/", 3000,
                {{"Accept-Encoding", "gzip, deflate, br"}, {"User-Agent", "curl/7.29.0"}});
            _LOG_INFO(g_logger) << r->toString();
        },
        true);
}

void test_data()
{
    base::Address::ptr addr = base::Address::LookupAny("www.baidu.com:80");
    auto sock = base::Socket::CreateTCP(addr);

    sock->connect(addr);
    const char buff[] = "GET / HTTP/1.1\r\n"
                        "connection: close\r\n"
                        "Accept-Encoding: gzip, deflate, br\r\n"
                        "Host: www.baidu.com\r\n\r\n";
    sock->send(buff, sizeof(buff));

    std::string line;
    line.resize(1024);

    std::ofstream ofs("http.dat", std::ios::binary);
    int total = 0;
    int len = 0;
    while ((len = sock->recv(&line[0], line.size())) > 0) {
        total += len;
        ofs.write(line.c_str(), len);
    }
    std::cout << "total: " << total << " tellp=" << ofs.tellp() << std::endl;
    ofs.flush();
}

void test_parser()
{
    std::ifstream ifs("http.dat", std::ios::binary);
    std::string content;
    std::string line;
    line.resize(1024);

    int total = 0;
    while (!ifs.eof()) {
        ifs.read(&line[0], line.size());
        content.append(&line[0], ifs.gcount());
        total += ifs.gcount();
    }

    std::cout << "length: " << content.size() << " total: " << total << std::endl;
    base::http::HttpResponseParser parser;
    size_t nparse = parser.execute(&content[0], content.size(), false);
    std::cout << "finish: " << parser.isFinished() << std::endl;
    content.resize(content.size() - nparse);
    std::cout << "rsp: " << *parser.getData() << std::endl;

    auto &client_parser = parser.getParser();
    std::string body;
    int cl = 0;
    do {
        size_t nparse = parser.execute(&content[0], content.size(), true);
        std::cout << "content_len: " << client_parser.content_len << " left: " << content.size()
                  << std::endl;
        cl += client_parser.content_len;
        content.resize(content.size() - nparse);
        body.append(content.c_str(), client_parser.content_len);
        content = content.substr(client_parser.content_len + 2);
    } while (!client_parser.chunks_done);

    std::cout << "total: " << body.size() << " content:" << cl << std::endl;

    base::ZlibStream::ptr stream = base::ZlibStream::CreateGzip(false);
    stream->write(body.c_str(), body.size());
    stream->flush();

    body = stream->getResult();

    std::ofstream ofs("http.txt");
    ofs << body;
}

int main(int argc, char **argv)
{
    base::IOManager iom(2);
    // iom.schedule(run);
    iom.schedule(test_https);
    return 0;
}
