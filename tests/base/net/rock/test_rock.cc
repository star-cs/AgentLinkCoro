#include "base/log/log.h"
#include "base/net/rock/rock_stream.h"

static base::Logger::ptr g_logger = _LOG_ROOT();

base::RockConnection::ptr conn(new base::RockConnection);
void run()
{
    conn->setAutoConnect(true);
    base::Address::ptr addr = base::Address::LookupAny("127.0.0.1:8061");
    if (!conn->connect(addr)) {
        _LOG_INFO(g_logger) << "connect " << *addr << " false";
        return;
    }
    conn->start();

    base::IOManager::GetThis()->addTimer(
        5000,
        []() {
            base::RockRequest::ptr req(new base::RockRequest);
            static uint32_t s_sn = 0;
            req->setSn(++s_sn);
            req->setCmd(100);
            req->setBody("hello world sn=" + std::to_string(s_sn));

            auto rsp = conn->request(req, 2000);
            if (rsp->response) {
                _LOG_INFO(g_logger) << rsp->response->toString();
            } else {
                _LOG_INFO(g_logger) << "error result=" << rsp->result;
            }
        },
        true);
}

int main(int argc, char **argv)
{
    base::IOManager iom(1);
    iom.schedule(run);
    return 0;
}
