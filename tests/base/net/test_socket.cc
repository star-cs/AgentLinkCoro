#include "base/net/socket.h"
#include "base/coro/iomanager.h"
#include "base/log/log.h"
#include "base/util.h"

static base::Logger::ptr g_looger = _LOG_ROOT();

void test_socket()
{
    // std::vector<base::Address::ptr> addrs;
    // base::Address::Lookup(addrs, "www.baidu.com", AF_INET);
    // base::IPAddress::ptr addr;
    // for(auto& i : addrs) {
    //     _LOG_INFO(g_looger) << i->toString();
    //     addr = std::dynamic_pointer_cast<base::IPAddress>(i);
    //     if(addr) {
    //         break;
    //     }
    // }
    base::IPAddress::ptr addr = base::Address::LookupAnyIPAddress("www.baidu.com");
    if (addr) {
        _LOG_INFO(g_looger) << "get address: " << addr->toString();
    } else {
        _LOG_ERROR(g_looger) << "get address fail";
        return;
    }

    base::Socket::ptr sock = base::Socket::CreateTCP(addr);
    addr->setPort(80);
    _LOG_INFO(g_looger) << "addr=" << addr->toString();
    if (!sock->connect(addr)) {
        _LOG_ERROR(g_looger) << "connect " << addr->toString() << " fail";
        return;
    } else {
        _LOG_INFO(g_looger) << "connect " << addr->toString() << " connected";
    }

    const char buff[] = "GET / HTTP/1.0\r\n\r\n";
    int rt = sock->send(buff, sizeof(buff));
    if (rt <= 0) {
        _LOG_INFO(g_looger) << "send fail rt=" << rt;
        return;
    }

    std::string buffs;
    buffs.resize(4096);
    rt = sock->recv(&buffs[0], buffs.size());

    if (rt <= 0) {
        _LOG_INFO(g_looger) << "recv fail rt=" << rt;
        return;
    }

    buffs.resize(rt);
    _LOG_INFO(g_looger) << buffs;
}

void test2()
{
    base::IPAddress::ptr addr = base::Address::LookupAnyIPAddress("www.baidu.com:80");
    if (addr) {
        _LOG_INFO(g_looger) << "get address: " << addr->toString();
    } else {
        _LOG_ERROR(g_looger) << "get address fail";
        return;
    }

    base::Socket::ptr sock = base::Socket::CreateTCP(addr);
    if (!sock->connect(addr)) {
        _LOG_ERROR(g_looger) << "connect " << addr->toString() << " fail";
        return;
    } else {
        _LOG_INFO(g_looger) << "connect " << addr->toString() << " connected";
    }

    uint64_t ts = base::GetCurrentUS();
    for (size_t i = 0; i < 10000000000ul; ++i) {
        if (int err = sock->getError()) {
            _LOG_INFO(g_looger) << "err=" << err << " errstr=" << strerror(err);
            break;
        }

        // struct tcp_info tcp_info;
        // if(!sock->getOption(IPPROTO_TCP, TCP_INFO, tcp_info)) {
        //     _LOG_INFO(g_looger) << "err";
        //     break;
        // }
        // if(tcp_info.tcpi_state != TCP_ESTABLISHED) {
        //     _LOG_INFO(g_looger)
        //             << " state=" << (int)tcp_info.tcpi_state;
        //     break;
        // }
        static int batch = 10000000;
        if (i && (i % batch) == 0) {
            uint64_t ts2 = base::GetCurrentUS();
            _LOG_INFO(g_looger) << "i=" << i << " used: " << ((ts2 - ts) * 1.0 / batch) << " us";
            ts = ts2;
        }
    }
}

int main(int argc, char **argv)
{
    base::IOManager iom;
    // iom.schedule(&test_socket);
    iom.schedule(&test2);
    return 0;
}
