#include "rock_server.h"
#include "base/log/log.h"
#include "base/application/module.h"

namespace base
{

static base::Logger::ptr g_logger = _LOG_NAME("system");

RockServer::RockServer(const std::string &type, base::IOManager *worker, base::IOManager *io_worker,
                       base::IOManager *accept_worker)
    : TcpServer(worker, io_worker, accept_worker)
{
    m_type = type;
}

void RockServer::handleClient(Socket::ptr client)
{
    _LOG_DEBUG(g_logger) << "handleClient " << *client;
    base::RockSession::ptr session = std::make_shared<base::RockSession>(client);
    session->setWorker(m_worker);
    ModuleMgr::GetInstance()->foreach (Module::ROCK,
                                       [session](Module::ptr m) { m->onConnect(session); });
    session->setDisconnectCb([](AsyncSocketStream::ptr stream) {
        ModuleMgr::GetInstance()->foreach (Module::ROCK,
                                           [stream](Module::ptr m) { m->onDisconnect(stream); });
    });
    session->setRequestHandler([](base::RockRequest::ptr req, base::RockResponse::ptr rsp,
                                  base::RockStream::ptr conn) -> bool {
        //_LOG_INFO(g_logger) << "handleReq " << req->toString()
        //                         << " body=" << req->getBody();
        bool rt = false;
        // 每个模块都有机会处理请求，直到有模块返回 true 表示已处理
        ModuleMgr::GetInstance()->foreach (Module::ROCK, [&rt, req, rsp, conn](Module::ptr m) {
            if (rt) {
                return;
            }
            rt = m->handleRequest(req, rsp, conn);
        });
        return rt;
    });
    session->setNotifyHandler([](base::RockNotify::ptr nty, base::RockStream::ptr conn) -> bool {
        //_LOG_INFO(g_logger) << "handleNty " << nty->toString()
        //                         << " body=" << nty->getBody();
        bool rt = false;
        // 每个模块都有机会处理通知，直到有模块返回 true 表示已处理
        ModuleMgr::GetInstance()->foreach (Module::ROCK, [&rt, nty, conn](Module::ptr m) {
            if (rt) {
                return;
            }
            rt = m->handleNotify(nty, conn);
        });
        return rt;
    });
    session->start();
}

} // namespace base
