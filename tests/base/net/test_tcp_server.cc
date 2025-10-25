#include "base/net/tcp_server.h"
#include "base/coro/iomanager.h"
#include "base/log/log.h"

base::Logger::ptr g_logger = _LOG_ROOT();

void run()
{
    auto addr = base::Address::LookupAny("0.0.0.0:8033");
    // auto addr2 = base::UnixAddress::ptr(new base::UnixAddress("/tmp/unix_addr"));
    std::vector<base::Address::ptr> addrs;
    addrs.push_back(addr);
    // addrs.push_back(addr2);

    base::TcpServer::ptr tcp_server(new base::TcpServer);
    std::vector<base::Address::ptr> fails;
    while (!tcp_server->bind(addrs, fails)) {
        sleep(2);
    }
    tcp_server->start();
}

int main(int argc, char **argv)
{
    base::IOManager iom(2);
    iom.schedule(run);
    return 0;
}
