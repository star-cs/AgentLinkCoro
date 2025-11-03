#include "base/net/ns/ns_protocol.h"
#include "base/net/ns/ns_client.h"

static base::Logger::ptr g_logger = _LOG_ROOT();

int type = 0;

void run()
{
    g_logger->setLevel(base::LogLevel::INFO);
    auto addr = base::IPAddress::Create("127.0.0.1", 8072);
    // if(!conn->connect(addr)) {
    //     _LOG_ERROR(g_logger) << "connect to: " << *addr << " fail";
    //     return;
    // }
    if (type == 0) {
        for (int i = 0; i < 5000; ++i) {
            base::RockConnection::ptr conn(new base::RockConnection);
            conn->connect(addr);
            base::IOManager::GetThis()->addTimer(
                3000,
                [conn, i]() {
                    base::RockRequest::ptr req(new base::RockRequest);
                    req->setCmd((int)base::ns::NSCommand::REGISTER);
                    auto rinfo = std::make_shared<base::ns::RegisterRequest>();
                    auto info = rinfo->add_infos();
                    info->set_domain(std::to_string(rand() % 2) + "domain.com");
                    info->add_cmds(rand() % 2 + 100);
                    info->add_cmds(rand() % 2 + 200);
                    info->mutable_node()->set_ip("127.0.0.1");
                    info->mutable_node()->set_port(1000 + i);
                    info->mutable_node()->set_weight(100);
                    req->setAsPB(*rinfo);

                    auto rt = conn->request(req, 100);
                    _LOG_INFO(g_logger)
                        << "[result=" << rt->result
                        << " response=" << (rt->response ? rt->response->toString() : "null")
                        << "]";
                },
                true);
            conn->start();
        }
    } else {
        for (int i = 0; i < 1000; ++i) {
            base::ns::NSClient::ptr nsclient(new base::ns::NSClient);
            nsclient->init();
            nsclient->addQueryDomain(std::to_string(i % 2) + "domain.com");
            nsclient->connect(addr);
            nsclient->start();
            _LOG_INFO(g_logger) << "NSClient start: i=" << i;

            if (i == 0) {
                base::IOManager::GetThis()->addTimer(1000, [nsclient](){
                    auto domains = nsclient->getDomains();
                    domains->dump(std::cout, "    ");
                }, true);
            }
        }

        // conn->setConnectCb([](base::AsyncSocketStream::ptr ss) {
        //     auto conn = std::dynamic_pointer_cast<base::RockConnection>(ss);
        //     base::RockRequest::ptr req(new base::RockRequest);
        //     req->setCmd((int)base::ns::NSCommand::QUERY);
        //     auto rinfo = std::make_shared<base::ns::QueryRequest>();
        //     rinfo->add_domains("0domain.com");
        //     req->setAsPB(*rinfo);
        //     auto rt = conn->request(req, 1000);
        //     _LOG_INFO(g_logger) << "[result="
        //         << rt->result << " response="
        //         << (rt->response ? rt->response->toString() : "null")
        //         << "]";
        //     return true;
        // });

        // conn->setNotifyHandler([](base::RockNotify::ptr nty,base::RockStream::ptr stream){
        //         auto nm = nty->getAsPB<base::ns::NotifyMessage>();
        //         if(!nm) {
        //             _LOG_ERROR(g_logger) << "invalid notify message";
        //             return true;
        //         }
        //         _LOG_INFO(g_logger) << base::PBToJsonString(*nm);
        //         return true;
        // });
    }
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        type = 1;
    }
    base::IOManager iom(5);
    iom.schedule(run);
    return 0;
}
