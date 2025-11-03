#include "ns_client.h"
#include "base/log/log.h"
#include "base/util.h"

namespace base
{
namespace ns
{

    // 系统日志记录器
    static base::Logger::ptr g_logger = _LOG_NAME("system");

    /**
     * @brief NSClient构造函数实现
     * 初始化域名集合对象，用于存储从名称服务器获取的节点信息
     */
    NSClient::NSClient()
    {
        m_domains = std::make_shared<base::ns::NSDomainSet>();
    }

    /**
     * @brief NSClient析构函数实现
     * 输出调试日志，记录客户端销毁
     */
    NSClient::~NSClient()
    {
        _LOG_DEBUG(g_logger) << "NSClient::~NSClient";
    }

    /**
     * @brief 获取查询域名列表
     * 加读锁后返回内部存储的查询域名集合
     * @return 查询域名集合的常量引用
     */
    const std::set<std::string> &NSClient::getQueryDomains()
    {
        base::RWMutex::ReadLock lock(m_mutex);
        return m_queryDomains;
    }

    /**
     * @brief 设置查询域名列表
     * 加写锁后替换内部查询域名集合，然后触发域名变更处理
     * @param v 新的查询域名集合
     */
    void NSClient::setQueryDomains(const std::set<std::string> &v)
    {
        base::RWMutex::WriteLock lock(m_mutex);
        m_queryDomains = v;
        lock.unlock(); // 提前解锁，避免在回调中可能的死锁
        onQueryDomainChange();
    }

    /**
     * @brief 添加查询域名
     * 加写锁后向内部查询域名集合添加域名，然后触发域名变更处理
     * @param domain 要添加的域名
     */
    void NSClient::addQueryDomain(const std::string &domain)
    {
        base::RWMutex::WriteLock lock(m_mutex);
        m_queryDomains.insert(domain);
        lock.unlock(); // 提前解锁，避免在回调中可能的死锁
        onQueryDomainChange();
    }

    /**
     * @brief 检查是否包含指定查询域名
     * 加读锁后检查内部查询域名集合是否包含指定域名
     * @param domain 要检查的域名
     * @return 如果包含返回true，否则返回false
     */
    bool NSClient::hasQueryDomain(const std::string &domain)
    {
        base::RWMutex::ReadLock lock(m_mutex);
        return m_queryDomains.count(domain) > 0;
    }

    /**
     * @brief 删除查询域名
     * 加写锁后从内部查询域名集合删除域名，然后触发域名变更处理
     * @param domain 要删除的域名
     */
    void NSClient::delQueryDomain(const std::string &domain)
    {
        base::RWMutex::WriteLock lock(m_mutex);
        m_queryDomains.erase(domain);
        lock.unlock(); // 提前解锁，避免在回调中可能的死锁
        onQueryDomainChange();
    }

    /**
     * @brief 查询节点信息实现
     * 构建查询请求，向名称服务器发送请求获取节点信息，并更新本地缓存
     * @return 查询结果对象
     */
    RockResult::ptr NSClient::query()
    {
        // 创建查询请求对象
        base::RockRequest::ptr req = std::make_shared<base::RockRequest>();
        req->setSn(base::Atomic::addFetch(m_sn, 1)); // 原子递增序列号
        req->setCmd((int)NSCommand::QUERY);          // 设置查询命令
        auto data = std::make_shared<base::ns::QueryRequest>();

        // 添加查询域名到请求中
        base::RWMutex::ReadLock lock(m_mutex);
        for (auto &i : m_queryDomains) {
            data->add_domains(i);
        }
        if (m_queryDomains.empty()) {
            // 如果没有查询域名，直接返回成功结果
            return std::make_shared<RockResult>(0, "ok", 0, nullptr, nullptr);
        }
        lock.unlock();

        // 设置请求数据并发送请求
        req->setAsPB(*data);
        auto rt = request(req, 1000); // 1000ms超时

        do {
            // 检查响应是否存在
            if (!rt->response) {
                _LOG_ERROR(g_logger) << "query error result=" << rt->result;
                break;
            }

            // 解析响应数据
            auto rsp = rt->response->getAsPB<base::ns::QueryResponse>();
            if (!rsp) {
                _LOG_ERROR(g_logger) << "invalid data not QueryResponse";
                break;
            }

            // 构建新的域名集合
            NSDomainSet::ptr domains = std::make_shared<NSDomainSet>();
            for (auto &i : rsp->infos()) {
                // 只处理客户端关注的域名
                if (!hasQueryDomain(i.domain())) {
                    continue;
                }

                auto domain = domains->get(i.domain(), true);
                uint32_t cmd = i.cmd();

                // 添加所有节点到域名中
                for (auto &n : i.nodes()) {
                    NSNode::ptr node = std::make_shared<NSNode>(n.ip(), n.port(), n.weight());
                    // 验证节点ID的有效性
                    if (!(node->getId() >> 32)) {
                        _LOG_ERROR(g_logger) << "invalid node: " << node->toString();
                        continue;
                    }
                    domain->add(cmd, node);
                }
            }

            // 原子地交换域名集合，避免长时间锁定
            m_domains->swap(*domains);
        } while (false);

        return rt;
    }

    /**
     * @brief 查询域名变更处理实现
     * 当查询域名列表发生变化时，如果已连接则重新发起查询
     */
    void NSClient::onQueryDomainChange()
    {
        if (isConnected()) {
            query();
        }
    }

    /**
     * @brief 初始化客户端实现
     * 设置连接、断开连接和通知处理回调函数
     */
    void NSClient::init()
    {
        auto self = std::dynamic_pointer_cast<NSClient>(shared_from_this());
        // 设置连接建立回调
        setConnectCb(std::bind(&NSClient::onConnect, self, std::placeholders::_1));
        // 设置连接断开回调
        setDisconnectCb(std::bind(&NSClient::onDisconnect, self, std::placeholders::_1));
        // 设置通知处理回调
        setNotifyHandler(
            std::bind(&NSClient::onNotify, self, std::placeholders::_1, std::placeholders::_2));
    }

    /**
     * @brief 反初始化客户端实现
     * 清除所有回调函数，取消定时器
     */
    void NSClient::uninit()
    {
        setConnectCb(nullptr);
        setDisconnectCb(nullptr);
        setNotifyHandler(nullptr);

        if (m_timer) {
            m_timer->cancel();
        }
    }

    /**
     * @brief 连接建立回调实现
     * 当与名称服务器建立连接时，设置定时器并立即发起查询
     * @param stream 建立的连接流
     * @return 处理结果，始终返回true
     */
    bool NSClient::onConnect(base::AsyncSocketStream::ptr stream)
    {
        // 取消可能存在的旧定时器
        if (m_timer) {
            m_timer->cancel();
        }

        // 设置新的定时器，30秒一次心跳
        auto self = std::dynamic_pointer_cast<NSClient>(shared_from_this());
        m_timer = m_iomanager->addTimer(30 * 1000, std::bind(&NSClient::onTimer, self), true);

        // 立即发起查询获取最新节点信息
        m_iomanager->schedule(std::bind(&NSClient::query, self));
        return true;
    }

    /**
     * @brief 定时器回调实现
     * 定期发送心跳并更新节点信息
     */
    void NSClient::onTimer()
    {
        // 发送心跳请求
        base::RockRequest::ptr req = std::make_shared<base::RockRequest>();
        req->setSn(base::Atomic::addFetch(m_sn, 1));
        req->setCmd((uint32_t)NSCommand::TICK);
        auto rt = request(req, 1000);

        if (!rt->response) {
            _LOG_ERROR(g_logger) << "tick error result=" << rt->result;
        }

        // 暂停1秒后更新节点信息
        sleep(1000);
        query();
    }

    /**
     * @brief 连接断开回调实现
     * 当与名称服务器断开连接时的处理，当前为空实现
     * @param stream 断开的连接流
     */
    void NSClient::onDisconnect(base::AsyncSocketStream::ptr stream)
    {
        // 目前不需要特殊处理
    }

    /**
     * @brief 通知处理回调实现
     * 处理来自名称服务器的通知，主要是节点变更通知
     * @param nty 通知对象
     * @param stream 连接流
     * @return 处理结果，始终返回true
     */
    bool NSClient::onNotify(base::RockNotify::ptr nty, base::RockStream::ptr stream)
    {
        do {
            // 检查是否为节点变更通知
            if (nty->getNotify() == (uint32_t)NSNotify::NODE_CHANGE) {
                // 解析通知数据
                auto nm = nty->getAsPB<base::ns::NotifyMessage>();
                if (!nm) {
                    _LOG_ERROR(g_logger) << "invalid node_change data";
                    break;
                }

                // 处理需要删除的节点
                for (auto &i : nm->dels()) {
                    // 只处理客户端关注的域名
                    if (!hasQueryDomain(i.domain())) {
                        continue;
                    }
                    auto domain = m_domains->get(i.domain());
                    if (!domain) {
                        continue;
                    }
                    int cmd = i.cmd();
                    for (auto &n : i.nodes()) {
                        NSNode::ptr node = std::make_shared<NSNode>(n.ip(), n.port(), n.weight());
                        domain->del(cmd, node->getId());
                    }
                }

                // 处理需要更新的节点
                for (auto &i : nm->updates()) {
                    // 只处理客户端关注的域名
                    if (!hasQueryDomain(i.domain())) {
                        continue;
                    }
                    auto domain = m_domains->get(i.domain(), true);
                    int cmd = i.cmd();
                    for (auto &n : i.nodes()) {
                        NSNode::ptr node = std::make_shared<NSNode>(n.ip(), n.port(), n.weight());
                        // 验证节点ID的有效性
                        if (node->getId() >> 32) {
                            domain->add(cmd, node);
                        } else {
                            _LOG_ERROR(g_logger) << "invalid node: " << node->toString();
                        }
                    }
                }
            }
        } while (false);
        return true;
    }

} // namespace ns
} // namespace base
