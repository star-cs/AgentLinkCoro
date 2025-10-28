#include "application.h"

#include <unistd.h>
#include <signal.h>

#include "base/net/tcp_server.h"
#include "daemon.h"
#include "base/conf/config.h"
#include "env.h"
#include "base/log/log.h"
#include "module.h"
#include "base/net/rock/rock_stream.h"
#include "base/coro/worker.h"
#include "base/net/http/ws_server.h"
#include "base/net/rock/rock_server.h"
#include "base/net/ns/name_server_module.h"
#include "base/fox_thread.h"

#if WITH_REDIS
#    include "base/db/redis.h"
#endif
#include "base/net/dns.h"
#include "base/pack/json_encoder.h"

namespace base
{

static base::Logger::ptr g_logger = _LOG_NAME("system");

static base::ConfigVar<std::string>::ptr g_server_work_path =
    base::Config::Lookup("server.work_path", std::string("/apps/work/sylar"), "server work path");

static base::ConfigVar<std::string>::ptr g_server_pid_file =
    base::Config::Lookup("server.pid_file", std::string("sylar.pid"), "server pid file");

#if WITH_ZK_CLIENT
static base::ConfigVar<std::string>::ptr g_service_discovery_zk =
    base::Config::Lookup("service_discovery.zk", std::string(""), "service discovery zookeeper");
#endif

static base::ConfigVar<std::string>::ptr g_service_discovery_redis = base::Config::Lookup(
    "service_discovery.redis.name", std::string(""), "service discovery redis name");

static base::ConfigVar<std::string>::ptr g_service_discovery_consul =
    base::Config::Lookup("service_discovery.consul", std::string(""), "service discovery consul");

_DEFINE_CONFIG(base::ConsulRegisterInfo::ptr, g_consul_register_info, "consul.register_info",
               nullptr, "consul register info");

static base::ConfigVar<std::vector<TcpServerConf> >::ptr g_servers_conf =
    base::Config::Lookup("servers", std::vector<TcpServerConf>(), "http server config");

std::string GetServerWorkPath()
{
    return g_server_work_path->getValue();
}

Application *Application::s_instance = nullptr;

Application::Application()
{
    s_instance = this;
}

bool Application::init(int argc, char **argv)
{
    m_argc = argc;
    m_argv = argv;

    base::EnvMgr::GetInstance()->addHelp("s", "start with the terminal");
    base::EnvMgr::GetInstance()->addHelp("d", "run as daemon");
    base::EnvMgr::GetInstance()->addHelp("c", "conf path default: ./conf");
    base::EnvMgr::GetInstance()->addHelp("p", "print help");

    bool is_print_help = false;
    if (!base::EnvMgr::GetInstance()->init(argc, argv)) {
        is_print_help = true;
    }

    if (base::EnvMgr::GetInstance()->has("p")) {
        is_print_help = true;
    }

    std::string conf_path = base::EnvMgr::GetInstance()->getConfigPath();
    _LOG_INFO(g_logger) << "load conf path:" << conf_path;
    base::Config::LoadFromConfDir(conf_path);

    ModuleMgr::GetInstance()->init();
    std::vector<Module::ptr> modules;
    ModuleMgr::GetInstance()->listAll(modules);

    for (auto i : modules) {
        i->onBeforeArgsParse(argc, argv);
    }

    if (is_print_help) {
        base::EnvMgr::GetInstance()->printHelp();
        return false;
    }

    for (auto i : modules) {
        i->onAfterArgsParse(argc, argv);
    }
    modules.clear();

    int run_type = 0;
    if (base::EnvMgr::GetInstance()->has("s")) {
        run_type = 1;
    }
    if (base::EnvMgr::GetInstance()->has("d")) {
        run_type = 2;
    }

    if (run_type == 0) {
        base::EnvMgr::GetInstance()->printHelp();
        return false;
    }

    std::string pidfile = g_server_work_path->getValue() + "/" + g_server_pid_file->getValue();
    if (base::FSUtil::IsRunningPidfile(pidfile)) {
        _LOG_ERROR(g_logger) << "server is running:" << pidfile;
        return false;
    }

    if (!base::FSUtil::Mkdir(g_server_work_path->getValue())) {
        _LOG_FATAL(g_logger) << "create work path [" << g_server_work_path->getValue()
                             << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Application::run()
{
    bool is_daemon = base::EnvMgr::GetInstance()->has("d");
    return start_daemon(
        m_argc, m_argv,
        std::bind(&Application::main, this, std::placeholders::_1, std::placeholders::_2),
        is_daemon);
}

void sigproc(int sig)
{
    _LOG_INFO(g_logger) << "sigproc sig=" << sig;
    if (sig == SIGUSR1) {
        base::LoggerMgr::GetInstance()->reopen();
    }
}

void initSignal()
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, sigproc);
}

int Application::main(int argc, char **argv)
{
    initSignal();

    _LOG_INFO(g_logger) << "main";
    std::string conf_path = base::EnvMgr::GetInstance()->getConfigPath();
    base::Config::LoadFromConfDir(conf_path, true);
    {
        std::string pidfile = g_server_work_path->getValue() + "/" + g_server_pid_file->getValue();
        std::ofstream ofs(pidfile);
        if (!ofs) {
            _LOG_ERROR(g_logger) << "open pidfile " << pidfile << " failed";
            return false;
        }
        ofs << getpid();
    }

    m_mainIOManager = std::make_shared<base::IOManager>(1, true, "main");
    m_mainIOManager->schedule(std::bind(&Application::run_fiber, this));
    m_mainIOManager->addTimer(
        2000,
        [conf_path]() {
            // _LOG_INFO(g_logger) << "hello";
            base::Config::LoadFromConfDir(conf_path);
        },
        true);
    m_mainIOManager->stop();
    return 0;
}

int Application::run_fiber()
{
    base::WorkerMgr::GetInstance()->init();
    FoxThreadMgr::GetInstance()->init();
    FoxThreadMgr::GetInstance()->start();
#if WITH_REDIS
    RedisMgr::GetInstance();
#endif
#if WITH_TAIR
    TairMgr::GetInstance();
#endif
    DnsMgr::GetInstance()->init();
    DnsMgr::GetInstance()->start();

    std::vector<Module::ptr> modules;
    ModuleMgr::GetInstance()->listAll(modules);
    bool has_error = false;
    for (auto &i : modules) {
        if (!i->onLoad()) {
            _LOG_ERROR(g_logger) << "module name=" << i->getName() << " version=" << i->getVersion()
                                 << " filename=" << i->getFilename();
            has_error = true;
        }
    }
    if (has_error) {
        _exit(0);
    }

    auto http_confs = g_servers_conf->getValue();
    std::vector<TcpServer::ptr> svrs;
    for (auto &i : http_confs) {
        _LOG_DEBUG(g_logger) << std::endl << LexicalCast<TcpServerConf, std::string>()(i);

        std::vector<Address::ptr> address;
        for (auto &a : i.address) {
            size_t pos = a.find(":");
            if (pos == std::string::npos) {
                // _LOG_ERROR(g_logger) << "invalid address: " << a;
                address.push_back(std::make_shared<UnixAddress>(a));
                continue;
            }
            int32_t port = atoi(a.substr(pos + 1).c_str());
            // 127.0.0.1
            auto addr = base::IPAddress::Create(a.substr(0, pos).c_str(), port);
            if (addr) {
                address.push_back(addr);
                continue;
            }
            std::vector<std::pair<Address::ptr, uint32_t> > result;
            if (base::Address::GetInterfaceAddresses(result, a.substr(0, pos))) {
                for (auto &x : result) {
                    auto ipaddr = std::dynamic_pointer_cast<IPAddress>(x.first);
                    if (ipaddr) {
                        ipaddr->setPort(atoi(a.substr(pos + 1).c_str()));
                    }
                    address.push_back(ipaddr);
                }
                continue;
            }

            auto aaddr = base::Address::LookupAny(a);
            if (aaddr) {
                address.push_back(aaddr);
                continue;
            }
            _LOG_ERROR(g_logger) << "invalid address: " << a;
            _exit(0);
        }
        IOManager *accept_worker = base::IOManager::GetThis();
        IOManager *io_worker = base::IOManager::GetThis();
        IOManager *process_worker = base::IOManager::GetThis();
        if (!i.accept_worker.empty()) {
            accept_worker = base::WorkerMgr::GetInstance()->getAsIOManager(i.accept_worker).get();
            if (!accept_worker) {
                _LOG_ERROR(g_logger) << "accept_worker: " << i.accept_worker << " not exists";
                _exit(0);
            }
        }
        if (!i.io_worker.empty()) {
            io_worker = base::WorkerMgr::GetInstance()->getAsIOManager(i.io_worker).get();
            if (!io_worker) {
                _LOG_ERROR(g_logger) << "io_worker: " << i.io_worker << " not exists";
                _exit(0);
            }
        }
        if (!i.process_worker.empty()) {
            process_worker = base::WorkerMgr::GetInstance()->getAsIOManager(i.process_worker).get();
            if (!process_worker) {
                _LOG_ERROR(g_logger) << "process_worker: " << i.process_worker << " not exists";
                _exit(0);
            }
        }

        TcpServer::ptr server;
        if (i.type == "http") {
            server = std::make_shared<base::http::HttpServer>(i.keepalive, process_worker,
                                                              io_worker, accept_worker);
        } else if (i.type == "ws") {
            server =
                std::make_shared<base::http::WSServer>(process_worker, io_worker, accept_worker);
        } else if (i.type == "rock") {
            server = std::make_shared<base::RockServer>("rock", process_worker, io_worker,
                                                        accept_worker);
        } else if (i.type == "nameserver") {
            server = std::make_shared<base::RockServer>("nameserver", process_worker, io_worker,
                                                        accept_worker);
            ModuleMgr::GetInstance()->add(std::make_shared<base::ns::NameServerModule>());
        } else {
            _LOG_ERROR(g_logger) << "invalid server type=" << i.type
                                 << LexicalCast<TcpServerConf, std::string>()(i);
            _exit(0);
        }
        if (!i.name.empty()) {
            server->setName(i.name);
        }
        std::vector<Address::ptr> fails;
        if (!server->bind(address, fails, i.ssl)) {
            for (auto &x : fails) {
                _LOG_ERROR(g_logger) << "bind address fail:" << *x;
            }
            _exit(0);
        }
        if (i.ssl) {
            if (!server->loadCertificates(i.cert_file, i.key_file)) {
                _LOG_ERROR(g_logger) << "loadCertificates fail, cert_file=" << i.cert_file
                                     << " key_file=" << i.key_file;
            }
        }
        server->setConf(i);
        // server->start();
        m_servers[i.type].push_back(server);
        svrs.push_back(server);
    }

#if WITH_ZK_CLIENT
    if (!g_service_discovery_zk->getValue().empty()) {
        m_serviceDiscovery =
            std::make_shared<ZKServiceDiscovery>(g_service_discovery_zk->getValue());
        m_rockSDLoadBalance = std::make_shared<RockSDLoadBalance>(m_serviceDiscovery);
    }
#endif
#if WITH_REDIS
    if (!g_service_discovery_redis->getValue().empty()) {
        m_serviceDiscovery =
            std::make_shared<RedisServiceDiscovery>(g_service_discovery_redis->getValue());
        m_rockSDLoadBalance = std::make_shared<RockSDLoadBalance>(m_serviceDiscovery);
    }
#endif
    if (!g_service_discovery_consul->getValue().empty()) {
        if (!g_consul_register_info->getValue()) {
            _LOG_ERROR(g_logger) << "consul info invalid " << g_consul_register_info->getName();
            return false;
        }
        auto regInfo = g_consul_register_info->getValue();
        regInfo->id = base::GetConsulUniqID(regInfo->port);
        regInfo->address = base::GetIPv4();
        regInfo->check->http = "http://" + base::GetIPv4() + ":" + std::to_string(regInfo->port)
                               + regInfo->check->http;
        _LOG_INFO(g_logger) << base::pack::EncodeToJsonString(regInfo, 0);
        base::ConsulClient::ptr client = std::make_shared<base::ConsulClient>();
        client->setUrl(g_service_discovery_consul->getValue());
        m_serviceDiscovery = std::make_shared<ConsulServiceDiscovery>(
            g_service_discovery_consul->getValue(), client, regInfo);
        m_rockSDLoadBalance = std::make_shared<RockSDLoadBalance>(m_serviceDiscovery);
    }

    // if(m_rockSDLoadBalance) {
    //     m_rockSDLoadBalance->doQuery();
    // }

    if (m_rockSDLoadBalance) {
        m_rockSDLoadBalance->start();
        sleep(1);
    }

    for (auto &i : modules) {
        i->onServerReady();
    }

    for (auto &i : svrs) {
        i->start();
    }

    for (auto &i : modules) {
        i->onServerUp();
    }

    if (m_rockSDLoadBalance) {
        m_rockSDLoadBalance->doRegister();
    }

    modules.clear();

    // ZKServiceDiscovery::ptr m_serviceDiscovery;
    // RockSDLoadBalance::ptr m_rockSDLoadBalance;
    // base::ZKServiceDiscovery::ptr zksd(new base::ZKServiceDiscovery("127.0.0.1:21811"));
    // zksd->registerServer("blog", "chat", base::GetIPv4() + ":8090", "xxx");
    // zksd->queryServer("blog", "chat");
    // zksd->setSelfInfo(base::GetIPv4() + ":8090");
    // zksd->setSelfData("vvv");
    // static RockSDLoadBalance::ptr rsdlb(new RockSDLoadBalance(zksd));
    // rsdlb->start();
    return 0;
}

void Application::initEnv()
{
    base::WorkerMgr::GetInstance()->init();
    FoxThreadMgr::GetInstance()->init();
    FoxThreadMgr::GetInstance()->start();
#if WITH_REDIS
    RedisMgr::GetInstance();
#endif
#if WITH_TAIR
    TairMgr::GetInstance();
#endif
    DnsMgr::GetInstance()->init();
    DnsMgr::GetInstance()->start();

#if WITH_ZK_CLIENT
    if (!g_service_discovery_zk->getValue().empty()) {
        m_serviceDiscovery =
            std::make_shared<ZKServiceDiscovery>(g_service_discovery_zk->getValue());
        m_rockSDLoadBalance = std::make_shared<RockSDLoadBalance>(m_serviceDiscovery);
    }
#endif
#if WITH_REDIS
    if (!g_service_discovery_redis->getValue().empty()) {
        m_serviceDiscovery =
            std::make_shared<RedisServiceDiscovery>(g_service_discovery_redis->getValue());
        m_rockSDLoadBalance = std::make_shared<RockSDLoadBalance>(m_serviceDiscovery);
    }
#endif

    if (m_rockSDLoadBalance) {
        m_rockSDLoadBalance->start();
    }
}

bool Application::getServer(const std::string &type, std::vector<TcpServer::ptr> &svrs)
{
    auto it = m_servers.find(type);
    if (it == m_servers.end()) {
        return false;
    }
    svrs = it->second;
    return true;
}

void Application::listAllServer(std::map<std::string, std::vector<TcpServer::ptr> > &servers)
{
    servers = m_servers;
}

} // namespace base
