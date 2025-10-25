#include "base/net/address.h"
#include "base/log/log.h"

base::Logger::ptr g_logger = _LOG_ROOT();

void test()
{
    std::vector<base::Address::ptr> addrs;

    _LOG_INFO(g_logger) << "begin";
    bool v = base::Address::Lookup(addrs, "localhost:3080");
    if (!v) {
        _LOG_ERROR(g_logger) << "lookup fail";
        return;
    }
    v = base::Address::Lookup(addrs, "www.baidu.com:80", AF_INET);
    if (!v) {
        _LOG_ERROR(g_logger) << "lookup fail";
        return;
    }
    v = base::Address::Lookup(addrs, "www.bilibili.com:80", AF_INET);
    if (!v) {
        _LOG_ERROR(g_logger) << "lookup fail";
        return;
    }
    _LOG_INFO(g_logger) << "end";

    for (size_t i = 0; i < addrs.size(); ++i) {
        _LOG_INFO(g_logger) << i << " - " << addrs[i]->toString();
    }

    auto addr = base::Address::LookupAny("localhost:4080");
    if (addr) {
        _LOG_INFO(g_logger) << *addr;
    } else {
        _LOG_ERROR(g_logger) << "error";
    }
}

void test_iface()
{
    std::multimap<std::string, std::pair<base::Address::ptr, uint32_t> > results;

    bool v = base::Address::GetInterfaceAddresses(results);
    if (!v) {
        _LOG_ERROR(g_logger) << "GetInterfaceAddresses fail";
        return;
    }

    for (auto &i : results) {
        _LOG_INFO(g_logger) << i.first << " - " << i.second.first->toString() << " - "
                            << i.second.second;
    }
}

void test_ipv4()
{
    // auto addr = base::IPAddress::Create("www.sylar.top");
    auto addr = base::IPAddress::Create("127.0.0.8");
    if (addr) {
        _LOG_INFO(g_logger) << addr->toString();
    }
}

int main(int argc, char **argv)
{
    // test_ipv4();
    // test_iface();
    test();
    return 0;
}
