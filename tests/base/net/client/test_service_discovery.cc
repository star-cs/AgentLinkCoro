#include "base/coro/iomanager.h"
#include "base/net/rock/rock_stream.h"
#include "base/log/log.h"
#include "base/coro/worker.h"

#include "base/net/streams/service_discovery.h"

base::ZKServiceDiscovery::ptr zksd(new base::ZKServiceDiscovery("127.0.0.1:21812"));
base::RockSDLoadBalance::ptr rsdlb(new base::RockSDLoadBalance(zksd));

static base::Logger::ptr g_logger = _LOG_ROOT();

std::atomic<uint32_t> s_id;
void on_timer()
{
    g_logger->setLevel(base::LogLevel::INFO);
    // _LOG_INFO(g_logger) << "on_timer";
    base::RockRequest::ptr req(new base::RockRequest);
    req->setSn(++s_id);
    req->setCmd(100);
    req->setBody("hello");

    auto rt = rsdlb->request("sylar.top", "blog", req, 1000);
    if (!rt->response) {
        if (req->getSn() % 50 == 0) {
            _LOG_ERROR(g_logger) << "invalid response: " << rt->toString();
        }
    } else {
        if (req->getSn() % 1000 == 0) {
            _LOG_INFO(g_logger) << rt->toString();
        }
    }
}

void run()
{
    zksd->setSelfInfo("127.0.0.1:2222");
    zksd->setSelfData("aaaa");

    std::unordered_map<std::string, std::unordered_map<std::string, std::string> > confs;
    confs["sylar.top"]["blog"] = "fair";
    rsdlb->start(confs);
    // _LOG_INFO(g_logger) << "on_timer---";

    base::IOManager::GetThis()->addTimer(1, on_timer, true);
}

int main(int argc, char **argv)
{
    base::WorkerMgr::GetInstance()->init({{"service_io", {{"thread_num", "1"}}}});
    base::IOManager iom(1);
    iom.addTimer(1000, []() { std::cout << rsdlb->statusString() << std::endl; }, true);
    iom.schedule(run);
    return 0;
}
