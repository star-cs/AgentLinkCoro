#pragma once

#include "base/net/rock/rock_stream.h"
#include "base/net/tcp_server.h"

namespace base
{

class RockServer : public TcpServer
{
public:
    typedef std::shared_ptr<RockServer> ptr;
    RockServer(const std::string &type = "rock",
               base::IOManager *worker = base::IOManager::GetThis(),
               base::IOManager *io_worker = base::IOManager::GetThis(),
               base::IOManager *accept_worker = base::IOManager::GetThis());

protected:
    virtual void handleClient(Socket::ptr client) override;
};

} // namespace base