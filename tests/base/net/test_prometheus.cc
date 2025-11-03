#include "base/log/log.h"
#include "base/application/env.h"
#include "base/conf/config.h"
#include "base/util/prometheus.h"
#include <prometheus/counter.h>
#include <prometheus/registry.h>
#include <prometheus/text_serializer.h>

_DEFINE_CONFIG(base::PrometheusClientConfig, s_cfg, "prometheus_client", {}, "prometheus cfg");

void run()
{
    base::Config::LoadFromConfDir("conf");
    auto registry = std::make_shared<base::PrometheusRegistry>();
    base::PrometheusClient::ptr client =
        std::make_shared<base::PrometheusClient>(s_cfg->getValue());

    // auto& counter = prometheus::BuildCounter().Name("sylar_test_counter")
    //                     .Labels({{"key","val"}})
    //                     .Help("Number of counter").Register(*registry);
    // counter.Add({{"url", "haha"},{"code","200"}}).Increment();
    registry->addCounter("sylar_test_counter", "Number of counter", {{"key", "val"}});
    // counter.Add({{"url", "haha"},{"code","200"}}).Increment();
    registry->addCounterLabels("sylar_test_counter", {{"url", "/test"}})->Increment();

    // prometheus::TextSerializer ts;
    // auto str = ts.Serialize(registry->Collect());
    // std::cout << str << std::endl;
    std::cout << registry->toString() << std::endl;

    std::cout << s_cfg->toString() << std::endl;
    client->addRegistry("test", registry, {{"key", "val"}});
    client->start();

    std::map<std::map<std::string, std::string>, std::string> mm;
}

int main(int argc, char **argv)
{
    base::EnvMgr::GetInstance()->init(argc, argv);
    base::IOManager iom(2);
    iom.schedule(run);
    iom.addTimer(1000, []() {}, true);
    iom.stop();
    return 0;
}
