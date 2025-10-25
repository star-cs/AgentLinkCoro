#include "ws_server.h"
#include "base/log/log.h"

namespace base
{
namespace http
{

    static base::Logger::ptr g_logger = _LOG_NAME("system");

    WSServer::WSServer(base::IOManager *worker, base::IOManager *io_worker,
                       base::IOManager *accept_worker)
        : TcpServer(worker, io_worker, accept_worker)
    {
        m_dispatch = std::make_shared<WSServletDispatch>();
        m_type = "websocket_server";
    }

    void WSServer::handleClient(Socket::ptr client)
    {
        _LOG_DEBUG(g_logger) << "handleClient " << *client;
        WSSession::ptr session = std::make_shared<WSSession>(client);
        do {
            HttpRequest::ptr header = session->handleShake();
            if (!header) {
                _LOG_DEBUG(g_logger) << "handleShake error";
                break;
            }
            WSServlet::ptr servlet = m_dispatch->getWSServlet(header->getPath());
            if (!servlet) {
                _LOG_DEBUG(g_logger) << "no match WSServlet";
                break;
            }
            int rt = servlet->onConnect(header, session);
            if (rt) {
                _LOG_DEBUG(g_logger) << "onConnect return " << rt;
                break;
            }
            while (true) {
                auto msg = session->recvMessage();
                if (!msg) {
                    break;
                }
                rt = servlet->handle(header, msg, session);
                if (rt) {
                    _LOG_DEBUG(g_logger) << "handle return " << rt;
                    break;
                }
            }
            servlet->onClose(header, session);
        } while (0);
        session->close();
    }

} // namespace http
} // namespace base
