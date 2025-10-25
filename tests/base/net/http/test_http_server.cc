#include "base/net/http/http_server.h"
#include "base/log/log.h"

static base::Logger::ptr g_logger = _LOG_ROOT();

#define XX(...) #__VA_ARGS__

base::IOManager::ptr worker;
void run()
{
    g_logger->setLevel(base::LogLevel::INFO);
    // base::http::HttpServer::ptr server(new base::http::HttpServer(true, worker.get(),
    // base::IOManager::GetThis()));
    base::http::HttpServer::ptr server(new base::http::HttpServer(true));
    base::Address::ptr addr = base::Address::LookupAnyIPAddress("0.0.0.0:8020");
    while (!server->bind(addr)) {
        sleep(2);
    }
    auto sd = server->getServletDispatch();
    sd->addServlet("/api/xx",
                   [](base::http::HttpRequest::ptr req, base::http::HttpResponse::ptr rsp,
                      base::SocketStream::ptr session) {
                       rsp->setBody(req->toString());
                       return 0;
                   });

    sd->addGlobServlet("/api/*",
                       [](base::http::HttpRequest::ptr req, base::http::HttpResponse::ptr rsp,
                          base::SocketStream::ptr session) {
                           rsp->setBody("Glob:\r\n" + req->toString());
                           return 0;
                       });

    sd->addGlobServlet("/apix/*", [](base::http::HttpRequest::ptr req,
                                     base::http::HttpResponse::ptr rsp,
                                     base::SocketStream::ptr session) {
        rsp->setBody(XX(<html><head> < title > 404 Not Found</ title></ head><body><center> < h1
                            > 404 Not Found</ h1></ center><hr><center> nginx / 1.16.0 < / center
                            > </ body></ html> < !--a padding to disable MSIE
                        and Chrome friendly error page-- > < !--a padding to disable MSIE
                        and Chrome friendly error page-- > < !--a padding to disable MSIE
                        and Chrome friendly error page-- > < !--a padding to disable MSIE
                        and Chrome friendly error page-- > < !--a padding to disable MSIE
                        and Chrome friendly error page-- > < !--a padding to disable MSIE
                        and Chrome friendly error page-- >));
        return 0;
    });

    server->start();
}

int main(int argc, char **argv)
{
    base::IOManager iom(1, true, "main");
    worker.reset(new base::IOManager(3, false, "worker"));
    iom.schedule(run);
    return 0;
}
