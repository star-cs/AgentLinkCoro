#include "name_server_module.h"
#include "base/log/log.h"
#include "base/coro/worker.h"

namespace base
{
namespace ns
{

    // 系统日志记录器，用于记录名称服务器模块的日志信息
    static base::Logger::ptr g_logger = _LOG_NAME("system");

    // 全局计数器：请求处理总数
    uint64_t s_request_count = 0;
    // 全局计数器：连接建立总数
    uint64_t s_on_connect = 0;
    // 全局计数器：连接断开总数
    uint64_t s_on_disconnect = 0;

    /**
     * @brief NameServerModule构造函数实现
     * 初始化名称服务器模块，创建域名集合对象用于存储注册的域名和节点信息
     */
    NameServerModule::NameServerModule() : RockModule("NameServerModule", "1.0.0", "")
    {
        m_domains = std::make_shared<NSDomainSet>();
    }

    /**
     * @brief 处理Rock协议请求实现
     * 根据请求命令类型分发到对应的处理函数，增加请求计数
     * @param request 请求对象
     * @param response 响应对象
     * @param stream 连接流
     * @return 处理是否成功
     */
    bool NameServerModule::handleRockRequest(base::RockRequest::ptr request,
                                             base::RockResponse::ptr response,
                                             base::RockStream::ptr stream)
    {
        // 原子递增请求计数
        base::Atomic::addFetch(s_request_count, 1);

        // 根据命令类型分发处理
        switch (request->getCmd()) {
            case (int)NSCommand::REGISTER: // 节点注册请求
                return handleRegister(request, response, stream);
            case (int)NSCommand::QUERY: // 域名查询请求
                return handleQuery(request, response, stream);
            case (int)NSCommand::TICK: // 心跳请求
                return handleTick(request, response, stream);
            default:
                _LOG_WARN(g_logger) << "invalid cmd=0x" << std::hex << request->getCmd();
                break;
        }
        return true;
    }

    /**
     * @brief 处理Rock协议通知实现
     * 当前暂未实现通知处理，直接返回成功
     * @param notify 通知对象
     * @param stream 连接流
     * @return 处理是否成功，始终返回true
     */
    bool NameServerModule::handleRockNotify(base::RockNotify::ptr notify,
                                            base::RockStream::ptr stream)
    {
        return true;
    }

    /**
     * @brief 处理连接建立事件实现
     * 增加连接计数，验证连接类型并记录连接信息日志
     * @param stream 新建立的连接流
     * @return 处理是否成功
     */
    bool NameServerModule::onConnect(base::Stream::ptr stream)
    {
        // 原子递增连接计数
        base::Atomic::addFetch(s_on_connect, 1);
        auto rockstream = std::dynamic_pointer_cast<RockStream>(stream);
        if (!rockstream) {
            _LOG_ERROR(g_logger) << "invalid stream";
            return false;
        }
        auto addr = rockstream->getRemoteAddress();
        if (addr) {
            _LOG_INFO(g_logger) << "onConnect: " << *addr;
        }
        return true;
    }

    /**
     * @brief 处理连接断开事件实现
     * 增加断开计数，验证连接类型，记录断开信息日志，并清理连接相关资源
     * @param stream 断开的连接流
     * @return 处理是否成功
     */
    bool NameServerModule::onDisconnect(base::Stream::ptr stream)
    {
        // 原子递增断开计数
        base::Atomic::addFetch(s_on_disconnect, 1);
        auto rockstream = std::dynamic_pointer_cast<RockStream>(stream);
        if (!rockstream) {
            _LOG_ERROR(g_logger) << "invalid stream";
            return false;
        }
        auto addr = rockstream->getRemoteAddress();
        if (addr) {
            _LOG_INFO(g_logger) << "onDisconnect: " << *addr;
        }
        // 设置客户端信息为nullptr，会触发节点清理
        set(rockstream, nullptr);
        // setQueryDomain(rockstream, {});
        return true;
    }

    /**
     * @brief 获取连接对应的客户端信息
     * 通过连接流查找对应的客户端信息对象
     * @param rs Rock协议连接流
     * @return 客户端信息对象，如果不存在返回nullptr
     */
    NSClientInfo::ptr NameServerModule::get(base::RockStream::ptr rs)
    {
        // 加读锁保护共享数据
        base::RWMutex::ReadLock lock(m_mutex);
        auto it = m_sessions.find(rs);
        return it == m_sessions.end() ? nullptr : it->second;
    }

    /**
     * @brief 处理注册请求实现
     * 处理客户端节点注册请求，验证请求数据，构建客户端信息并更新节点状态
     * @param request 注册请求
     * @param response 响应对象
     * @param stream 连接流
     * @return 处理是否成功
     */
    bool NameServerModule::handleRegister(base::RockRequest::ptr request,
                                          base::RockResponse::ptr response,
                                          base::RockStream::ptr stream)
    {
        // 解析注册请求数据（Protocol Buffers格式）
        auto rr = request->getAsPB<RegisterRequest>();
        if (!rr) {
            _LOG_ERROR(g_logger) << "invalid register request from: "
                                 << stream->getRemoteAddressString();
            return false;
        }

        // 获取当前已存在的客户端信息（如果有）
        auto old_value = get(stream);
        // 用于存储新的客户端信息
        NSClientInfo::ptr new_value;

        // 遍历处理所有注册信息
        for (int i = 0; i < rr->infos_size(); ++i) {
            auto &info = rr->infos(i);

            // 宏定义：用于检查必填字段是否存在
#define XX(info, attr)                                                                             \
    if (!info.has_##attr()) {                                                                      \
        _LOG_ERROR(g_logger) << "invalid register request from: "                                  \
                             << stream->getRemoteAddressString() << " " #attr " is null";          \
        return false;                                                                              \
    }

            // 检查必要字段是否存在
            XX(info, node);   // 节点信息必须存在
            XX(info, domain); // 域名必须存在

            // 检查命令列表不为空
            if (info.cmds_size() == 0) {
                _LOG_ERROR(g_logger)
                    << "invalid register request from: " << stream->getRemoteAddressString()
                    << " cmds is null";
                return false;
            }

            // 检查节点信息
            auto &node = info.node();
            XX(node, ip);
            XX(node, port);
            XX(node, weight);

            // 创建节点对象
            NSNode::ptr ns_node = std::make_shared<NSNode>(node.ip(), node.port(), node.weight());

            // 验证节点ID有效性
            if (!(ns_node->getId() >> 32)) {
                _LOG_ERROR(g_logger)
                    << "invalid register request from: " << stream->getRemoteAddressString()
                    << " ip=" << node.ip() << " invalid";
                return false;
            }

            // 验证IP一致性（防止同一连接注册不同IP）
            if (old_value) {
                if (old_value->m_node->getId() != ns_node->getId()) {
                    _LOG_ERROR(g_logger)
                        << "invalid register request from: " << stream->getRemoteAddressString()
                        << " old.ip=" << old_value->m_node->getIp()
                        << " old.port=" << old_value->m_node->getPort()
                        << " cur.ip=" << ns_node->getIp() << " cur.port=" << ns_node->getPort();
                    return false;
                }
            }

            // 验证多个注册信息中的IP一致性
            if (new_value) {
                if (new_value->m_node->getId() != ns_node->getId()) {
                    _LOG_ERROR(g_logger)
                        << "invalid register request from: " << stream->getRemoteAddressString()
                        << " new.ip=" << new_value->m_node->getIp()
                        << " new.port=" << new_value->m_node->getPort()
                        << " cur.ip=" << ns_node->getIp() << " cur.port=" << ns_node->getPort();
                    return false;
                }
            } else {
                // 创建新的客户端信息
                new_value = std::make_shared<NSClientInfo>();
                new_value->m_node = ns_node;
            }

            // 更新域名到命令的映射
            for (auto &cmd : info.cmds()) {
                new_value->m_domain2cmds[info.domain()].insert(cmd);
            }
        }

        // 设置客户端信息（会触发节点更新）
        set(stream, new_value);

        // 设置成功响应
        response->setResult(0);
        response->setResultStr("ok");
        return true;
    }

    /**
     * @brief 计算域名命令映射差异实现
     * 比较新旧域名命令映射，找出删除、新增和共同的命令
     * @param old_value 旧的域名命令映射
     * @param new_value 新的域名命令映射
     * @param dels 输出参数，删除的域名命令映射
     * @param news 输出参数，新增的域名命令映射
     * @param comms 输出参数，共同的域名命令映射
     */
    void diff(const std::map<std::string, std::set<uint32_t> > &old_value,
              const std::map<std::string, std::set<uint32_t> > &new_value,
              std::map<std::string, std::set<uint32_t> > &dels,
              std::map<std::string, std::set<uint32_t> > &news,
              std::map<std::string, std::set<uint32_t> > &comms)
    {
        for (auto &i : old_value) {
            auto it = new_value.find(i.first);
            if (it == new_value.end()) {
                dels.insert(i);
                continue;
            }
            for (auto &n : i.second) {
                auto iit = it->second.find(n);
                if (iit == it->second.end()) {
                    dels[i.first].insert(n);
                    continue;
                }
                comms[i.first].insert(n);
            }
        }

        for (auto &i : new_value) {
            auto it = old_value.find(i.first);
            if (it == old_value.end()) {
                news.insert(i);
                continue;
            }
            for (auto &n : i.second) {
                auto iit = it->second.find(n);
                if (iit == it->second.end()) {
                    news[i.first].insert(n);
                    continue;
                }
            }
        }
    }

    /**
     * @brief 设置客户端信息实现
     * 更新客户端信息，处理域名命令映射的增删改，并更新节点状态
     * @param rs Rock协议连接流
     * @param new_value 新的客户端信息，如果为nullptr则清理连接相关资源
     */
    void NameServerModule::set(base::RockStream::ptr rs, NSClientInfo::ptr new_value)
    {
        if (!rs->isConnected()) {
            new_value = nullptr;
        }

        auto old_value = get(rs);

        std::map<std::string, std::set<uint32_t> > old_v;
        std::map<std::string, std::set<uint32_t> > new_v;
        std::map<std::string, std::set<uint32_t> > dels;
        std::map<std::string, std::set<uint32_t> > news;
        std::map<std::string, std::set<uint32_t> > comms;

        // auto nty = std::make_shared<NotifyMessage>();
        // std::set<std::string> ds;

        if (old_value) {
            old_v = old_value->m_domain2cmds;
        }
        if (new_value) {
            new_v = new_value->m_domain2cmds;
        }
        diff(old_v, new_v, dels, news, comms);
        for (auto &i : dels) {
            auto d = m_domains->get(i.first);
            if (d) {
                for (auto &c : i.second) {
                    d->del(c, old_value->m_node->getId());
                    // auto info = nty->add_dels();
                    // info->set_domain(i.first);
                    // info->set_cmd(c);
                    // auto ninfo = info->add_nodes();
                    // ninfo->set_ip(old_value->m_node->getIp());
                    // ninfo->set_port(old_value->m_node->getPort());
                    // ninfo->set_weight(old_value->m_node->getWeight());
                }
                // ds.insert(i.first);
            }
        }
        for (auto &i : news) {
            auto d = m_domains->get(i.first);
            if (!d) {
                d = std::make_shared<NSDomain>(i.first);
                m_domains->add(d);
            }
            for (auto &c : i.second) {
                d->add(c, new_value->m_node);

                // auto info = nty->add_updates();
                // info->set_domain(i.first);
                // info->set_cmd(c);
                // auto ninfo = info->add_nodes();
                // ninfo->set_ip(new_value->m_node->getIp());
                // ninfo->set_port(new_value->m_node->getPort());
                // ninfo->set_weight(new_value->m_node->getWeight());
            }
            // ds.insert(i.first);
        }
        if (!comms.empty()) {
            if (old_value->m_node->getWeight() != new_value->m_node->getWeight()) {
                for (auto &i : comms) {
                    auto d = m_domains->get(i.first);
                    if (!d) {
                        d = std::make_shared<NSDomain>(i.first);
                        m_domains->add(d);
                    }
                    for (auto &c : i.second) {
                        d->add(c, new_value->m_node);

                        // auto info = nty->add_updates();
                        // info->set_domain(i.first);
                        // info->set_cmd(c);
                        // auto ninfo = info->add_nodes();
                        // ninfo->set_ip(new_value->m_node->getIp());
                        // ninfo->set_port(new_value->m_node->getPort());
                        // ninfo->set_weight(new_value->m_node->getWeight());
                    }

                    // ds.insert(i.first);
                }
            }
        }

        // base::WorkerMgr::GetInstance()->schedule("notify",
        //         std::bind(&NameServerModule::doNotify, this, ds, nty));

        base::RWMutex::WriteLock lock(m_mutex);
        if (new_value) {
            m_sessions[rs] = new_value;
        } else {
            m_sessions.erase(rs);
        }
    }

    std::set<base::RockStream::ptr> NameServerModule::getStreams(const std::string &domain)
    {
        base::RWMutex::ReadLock lock(m_mutex);
        auto it = m_domainToSessions.find(domain);
        return it == m_domainToSessions.end() ? std::set<base::RockStream::ptr>() : it->second;
    }

    /**
     * @brief 执行节点变更通知实现
     * 向订阅了相关域名的客户端发送节点变更通知
     * @param domains 发生变更的域名集合
     * @param nty 通知消息对象
     */
    void NameServerModule::doNotify(std::set<std::string> &domains,
                                    std::shared_ptr<NotifyMessage> nty)
    {
        RockNotify::ptr notify = std::make_shared<RockNotify>();
        notify->setNotify((int)NSNotify::NODE_CHANGE);
        notify->setAsPB(*nty);
        for (auto &i : domains) {
            auto ss = getStreams(i);
            for (auto &n : ss) {
                n->sendMessage(notify);
            }
        }
    }

    /**
     * @brief 处理节点查询请求实现
     * 解析查询请求，查找指定域名下的所有节点信息，并构建查询响应
     * @param request 查询请求对象
     * @param response 响应对象
     * @param stream 连接流
     * @return 查询是否成功
     */
    bool NameServerModule::handleQuery(base::RockRequest::ptr request,
                                       base::RockResponse::ptr response,
                                       base::RockStream::ptr stream)
    {
        auto qreq = request->getAsPB<QueryRequest>();
        if (!qreq) {
            _LOG_ERROR(g_logger) << "invalid query request from: "
                                 << stream->getRemoteAddressString();
            return false;
        }
        if (!qreq->domains_size()) {
            _LOG_ERROR(g_logger) << "invalid query request from: "
                                 << stream->getRemoteAddressString() << " domains is null";
        }
        std::set<NSDomain::ptr> domains;
        std::set<std::string> ds;
        for (auto &i : qreq->domains()) {
            auto d = m_domains->get(i);
            if (d) {
                domains.insert(d);
            }
            ds.insert(i);
        }
        auto qrsp = std::make_shared<QueryResponse>();
        for (auto &i : domains) {
            std::vector<NSNodeSet::ptr> nss;
            i->listAll(nss);
            for (auto &n : nss) {
                auto item = qrsp->add_infos();
                item->set_domain(i->getDomain());
                item->set_cmd(n->getCmd());
                std::vector<NSNode::ptr> ns;
                n->listAll(ns);

                for (auto &x : ns) {
                    auto node = item->add_nodes();
                    node->set_ip(x->getIp());
                    node->set_port(x->getPort());
                    node->set_weight(x->getWeight());
                }
            }
        }
        response->setResult(0);
        response->setResultStr("ok");
        response->setAsPB(*qrsp);
        // setQueryDomain(stream, ds);
        return true;
    }

    /**
     * @brief 设置查询域名实现
     * 为指定连接设置感兴趣的域名集合，用于后续节点变更通知
     * @param rs Rock协议连接流
     * @param ds 域名集合
     */
    void NameServerModule::setQueryDomain(base::RockStream::ptr rs, const std::set<std::string> &ds)
    {
        std::set<std::string> old_ds;
        {
            base::RWMutex::ReadLock lock(m_mutex);
            auto it = m_queryDomains.find(rs);
            if (it != m_queryDomains.end()) {
                if (it->second == ds) {
                    return;
                }
                old_ds = it->second;
            }
        }
        base::RWMutex::WriteLock lock(m_mutex);
        if (!rs->isConnected()) {
            return;
        }
        for (auto &i : old_ds) {
            m_domainToSessions[i].erase(rs);
        }
        for (auto &i : ds) {
            m_domainToSessions[i].insert(rs);
        }
        if (ds.empty()) {
            m_queryDomains.erase(rs);
        } else {
            m_queryDomains[rs] = ds;
        }
    }

    /**
     * @brief 处理心跳请求实现
     * 处理客户端的心跳请求，保持连接活跃
     * @param request 心跳请求对象
     * @param response 响应对象
     * @param stream 连接流
     * @return 处理是否成功，始终返回true
     */
    bool NameServerModule::handleTick(base::RockRequest::ptr request,
                                      base::RockResponse::ptr response,
                                      base::RockStream::ptr stream)
    {
        return true;
    }

    std::string NameServerModule::statusString()
    {
        std::stringstream ss;
        ss << RockModule::statusString() << std::endl;
        ss << "s_request_count: " << s_request_count << std::endl;
        ss << "s_on_connect: " << s_on_connect << std::endl;
        ss << "s_on_disconnect: " << s_on_disconnect << std::endl;
        m_domains->dump(ss);

        ss << "domainToSession: " << std::endl;
        base::RWMutex::ReadLock lock(m_mutex);
        for (auto &i : m_domainToSessions) {
            ss << "    " << i.first << ":" << std::endl;
            for (auto &v : i.second) {
                ss << "        " << v->getRemoteAddressString() << std::endl;
            }
            ss << std::endl;
        }
        return ss.str();
    }

} // namespace ns
} // namespace base
