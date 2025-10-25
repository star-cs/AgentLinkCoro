#pragma once

#include "base/net/tcp_server.h"
#include "ws_session.h"
#include "ws_servlet.h"

namespace base
{
namespace http
{

    class WSServer : public TcpServer
    {
    public:
        typedef std::shared_ptr<WSServer> ptr;

        WSServer(base::IOManager *worker = base::IOManager::GetThis(),
                 base::IOManager *io_worker = base::IOManager::GetThis(),
                 base::IOManager *accept_worker = base::IOManager::GetThis());

        WSServletDispatch::ptr getWSServletDispatch() const { return m_dispatch; }
        void setWSServletDispatch(WSServletDispatch::ptr v) { m_dispatch = v; }

    protected:
        virtual void handleClient(Socket::ptr client) override;

    protected:
        WSServletDispatch::ptr m_dispatch;
    };

} // namespace http
} // namespace base
