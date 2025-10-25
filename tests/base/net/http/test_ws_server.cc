#include "base/net/http/ws_server.h"
#include "base/log/log.h"

static base::Logger::ptr g_logger = _LOG_ROOT();

void run()
{
    base::http::WSServer::ptr server(new base::http::WSServer);
    base::Address::ptr addr = base::Address::LookupAnyIPAddress("0.0.0.0:8020");
    if (!addr) {
        _LOG_ERROR(g_logger) << "get address error";
        return;
    }
    auto fun = [](base::http::HttpRequest::ptr header, base::http::WSFrameMessage::ptr msg,
                  base::http::WSSession::ptr session) {
        session->sendMessage(msg);
        return 0;
    };

    server->getWSServletDispatch()->addServlet("/api", fun);
    while (!server->bind(addr)) {
        _LOG_ERROR(g_logger) << "bind " << *addr << " fail";
        sleep(1);
    }
    server->start();
}

int main(int argc, char **argv)
{
    base::IOManager iom(2);
    iom.schedule(run);
    return 0;
}
