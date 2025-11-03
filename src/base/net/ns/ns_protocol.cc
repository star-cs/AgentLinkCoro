#include "ns_protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>

namespace base
{
namespace ns
{

    /**
     * @brief NSNode构造函数实现
     * 初始化节点信息并生成节点唯一标识
     * @param ip 节点IP地址
     * @param port 节点端口号
     * @param weight 节点权重
     */
    NSNode::NSNode(const std::string &ip, uint16_t port, uint32_t weight)
        : m_ip(ip), m_port(port), m_weight(weight)
    {
        m_id = GetID(ip, port);
    }

    /**
     * @brief 根据IP和端口生成节点唯一标识
     * 将IP地址转换为网络字节序的整数，左移32位后与端口号组合成64位唯一标识
     * @param ip 节点IP地址
     * @param port 节点端口号
     * @return 生成的64位节点唯一标识
     */
    uint64_t NSNode::GetID(const std::string &ip, uint16_t port)
    {
        in_addr_t ip_addr = inet_addr(ip.c_str());
        uint64_t v = (((uint64_t)ip_addr) << 32) | port;
        return v;
    }

    /**
     * @brief 将节点信息输出到流中
     * 格式化输出节点的ID、IP、端口和权重信息
     * @param os 输出流
     * @param prefix 前缀字符串
     * @return 输出流引用
     */
    std::ostream &NSNode::dump(std::ostream &os, const std::string &prefix)
    {
        os << prefix << "[NSNode id=" << m_id << " ip=" << m_ip << " port=" << m_port
           << " weight=" << m_weight << "]";
        return os;
    }

    /**
     * @brief 将节点信息转换为字符串
     * 调用dump方法将节点信息格式化为字符串
     * @param prefix 前缀字符串
     * @return 格式化后的节点信息字符串
     */
    std::string NSNode::toString(const std::string &prefix)
    {
        std::stringstream ss;
        dump(ss, prefix);
        return ss.str();
    }

    /**
     * @brief NSNodeSet构造函数实现
     * 初始化命令标识
     * @param cmd 命令标识
     */
    NSNodeSet::NSNodeSet(uint32_t cmd) : m_cmd(cmd)
    {
    }

    /**
     * @brief 获取节点集合大小
     * 加写锁后返回内部存储的节点数量
     * @return 节点数量
     */
    size_t NSNodeSet::size()
    {
        base::RWMutex::WriteLock lock(m_mutex);
        return m_datas.size();
    }

    /**
     * @brief 添加节点到集合中
     * 加写锁后将节点信息添加到内部存储中
     * @param info 要添加的节点信息
     */
    void NSNodeSet::add(NSNode::ptr info)
    {
        base::RWMutex::WriteLock lock(m_mutex);
        m_datas[info->getId()] = info;
    }

    /**
     * @brief 从集合中删除节点
     * 加写锁后查找并删除指定ID的节点
     * @param id 要删除的节点ID
     * @return 被删除的节点信息，如果不存在则返回nullptr
     */
    NSNode::ptr NSNodeSet::del(uint64_t id)
    {
        NSNode::ptr rt;
        base::RWMutex::WriteLock lock(m_mutex);
        auto it = m_datas.find(id);
        if (it != m_datas.end()) {
            rt = it->second;
            m_datas.erase(it);
        }
        return rt;
    }

    /**
     * @brief 将节点集合信息输出到流中
     * 格式化输出节点集合的命令标识、大小以及所有节点信息
     * @param os 输出流
     * @param prefix 前缀字符串
     * @return 输出流引用
     */
    std::ostream &NSNodeSet::dump(std::ostream &os, const std::string &prefix)
    {
        os << prefix << "[NSNodeSet cmd=" << m_cmd;
        base::RWMutex::ReadLock lock(m_mutex);
        os << " size=" << m_datas.size() << "]" << std::endl;
        for (auto &i : m_datas) {
            i.second->dump(os, prefix + "    ") << std::endl;
        }
        return os;
    }

    /**
     * @brief 将节点集合信息转换为字符串
     * 调用dump方法将节点集合信息格式化为字符串
     * @param prefix 前缀字符串
     * @return 格式化后的节点集合信息字符串
     */
    std::string NSNodeSet::toString(const std::string &prefix)
    {
        std::stringstream ss;
        dump(ss, prefix);
        return ss.str();
    }

    /**
     * @brief 获取指定ID的节点
     * 加读锁后查找并返回指定ID的节点信息
     * @param id 节点ID
     * @return 节点信息，如果不存在则返回nullptr
     */
    NSNode::ptr NSNodeSet::get(uint64_t id)
    {
        base::RWMutex::ReadLock lock(m_mutex);
        auto it = m_datas.find(id);
        return it == m_datas.end() ? nullptr : it->second;
    }

    /**
     * @brief 获取所有节点信息
     * 加读锁后将所有节点信息添加到传入的向量中
     * @param infos 用于存储节点信息的向量
     */
    void NSNodeSet::listAll(std::vector<NSNode::ptr> &infos)
    {
        base::RWMutex::ReadLock lock(m_mutex);
        for (auto &i : m_datas) {
            infos.push_back(i.second);
        }
    }

    /**
     * @brief 向域名添加节点集合
     * 加写锁后将节点集合添加到内部存储中
     * @param info 要添加的节点集合
     */
    void NSDomain::add(NSNodeSet::ptr info)
    {
        base::RWMutex::WriteLock lock(m_mutex);
        m_datas[info->getCmd()] = info;
    }

    /**
     * @brief 获取域名下的节点集合数量
     * 加写锁后返回内部存储的节点集合数量
     * @return 节点集合数量
     */
    size_t NSDomain::size()
    {
        base::RWMutex::WriteLock lock(m_mutex);
        return m_datas.size();
    }

    /**
     * @brief 将域名信息输出到流中
     * 格式化输出域名的名称、命令数量以及所有节点集合信息
     * @param os 输出流
     * @param prefix 前缀字符串
     * @return 输出流引用
     */
    std::ostream &NSDomain::dump(std::ostream &os, const std::string &prefix)
    {
        os << prefix << "[NSDomain name=" << m_domain;
        base::RWMutex::ReadLock lock(m_mutex);
        os << " cmd_size=" << m_datas.size() << "]" << std::endl;
        for (auto &i : m_datas) {
            i.second->dump(os, prefix + "    ") << std::endl;
        }
        return os;
    }

    /**
     * @brief 将域名信息转换为字符串
     * 调用dump方法将域名信息格式化为字符串
     * @param prefix 前缀字符串
     * @return 格式化后的域名信息字符串
     */
    std::string NSDomain::toString(const std::string &prefix)
    {
        std::stringstream ss;
        dump(ss, prefix);
        return ss.str();
    }

    /**
     * @brief 向指定命令添加节点
     * 如果该命令的节点集合不存在则创建，然后将节点添加到集合中
     * @param cmd 命令标识
     * @param info 要添加的节点信息
     */
    void NSDomain::add(uint32_t cmd, NSNode::ptr info)
    {
        auto ns = get(cmd);
        if (!ns) {
            ns = std::make_shared<NSNodeSet>(cmd);
            add(ns);
        }
        ns->add(info);
    }

    /**
     * @brief 删除指定命令的所有节点
     * 加写锁后删除指定命令的节点集合
     * @param cmd 命令标识
     */
    void NSDomain::del(uint32_t cmd)
    {
        base::RWMutex::WriteLock lock(m_mutex);
        m_datas.erase(cmd);
    }

    /**
     * @brief 删除指定命令下的指定节点
     * 获取指定命令的节点集合，删除指定ID的节点
     * 如果删除后节点集合为空，则删除该命令的节点集合
     * @param cmd 命令标识
     * @param id 节点ID
     * @return 被删除的节点信息，如果不存在则返回nullptr
     */
    NSNode::ptr NSDomain::del(uint32_t cmd, uint64_t id)
    {
        auto ns = get(cmd);
        if (!ns) {
            return nullptr;
        }
        auto info = ns->del(id);
        if (!ns->size()) {
            del(cmd);
        }
        return info;
    }

    /**
     * @brief 获取指定命令的节点集合
     * 加读锁后查找并返回指定命令的节点集合
     * @param cmd 命令标识
     * @return 节点集合，如果不存在则返回nullptr
     */
    NSNodeSet::ptr NSDomain::get(uint32_t cmd)
    {
        base::RWMutex::ReadLock lock(m_mutex);
        auto it = m_datas.find(cmd);
        return it == m_datas.end() ? nullptr : it->second;
    }

    /**
     * @brief 获取域名下的所有节点集合
     * 加读锁后将所有节点集合添加到传入的向量中
     * @param infos 用于存储节点集合的向量
     */
    void NSDomain::listAll(std::vector<NSNodeSet::ptr> &infos)
    {
        base::RWMutex::ReadLock lock(m_mutex);
        for (auto &i : m_datas) {
            infos.push_back(i.second);
        }
    }

    /**
     * @brief 向域名集合添加域名
     * 加写锁后将域名信息添加到内部存储中
     * @param info 要添加的域名信息
     */
    void NSDomainSet::add(NSDomain::ptr info)
    {
        base::RWMutex::WriteLock lock(m_mutex);
        m_datas[info->getDomain()] = info;
    }

    /**
     * @brief 从域名集合中删除域名
     * 加写锁后删除指定的域名信息
     * @param domain 要删除的域名
     */
    void NSDomainSet::del(const std::string &domain)
    {
        base::RWMutex::WriteLock lock(m_mutex);
        m_datas.erase(domain);
    }

    /**
     * @brief 获取域名信息
     * 支持自动创建不存在的域名
     * @param domain 域名
     * @param auto_create 是否自动创建
     * @return 域名信息
     */
    NSDomain::ptr NSDomainSet::get(const std::string &domain, bool auto_create)
    {
        // 先尝试读锁查找
        {
            base::RWMutex::ReadLock lock(m_mutex);
            auto it = m_datas.find(domain);
            if (!auto_create) {
                return it == m_datas.end() ? nullptr : it->second;
            }
        }
        // 需要自动创建时，获取写锁并重试
        base::RWMutex::WriteLock lock(m_mutex);
        auto it = m_datas.find(domain);
        if (it != m_datas.end()) {
            return it->second;
        }
        // 创建新的域名信息
        NSDomain::ptr d = std::make_shared<NSDomain>(domain);
        m_datas[domain] = d;
        return d;
    }

    /**
     * @brief 删除指定域名下指定命令的指定节点
     * 递归删除操作，先获取域名，再获取命令的节点集合，最后删除节点
     * @param domain 域名
     * @param cmd 命令标识
     * @param id 节点ID
     */
    void NSDomainSet::del(const std::string &domain, uint32_t cmd, uint64_t id)
    {
        auto d = get(domain);
        if (!d) {
            return;
        }
        auto ns = d->get(cmd);
        if (!ns) {
            return;
        }
        ns->del(id);
    }

    /**
     * @brief 获取所有域名信息
     * 加读锁后将所有域名信息添加到传入的向量中
     * @param infos 用于存储域名信息的向量
     */
    void NSDomainSet::listAll(std::vector<NSDomain::ptr> &infos)
    {
        base::RWMutex::ReadLock lock(m_mutex);
        for (auto &i : m_datas) {
            infos.push_back(i.second);
        }
    }

    /**
     * @brief 将域名集合信息输出到流中
     * 格式化输出域名集合的大小以及所有域名信息
     * @param os 输出流
     * @param prefix 前缀字符串
     * @return 输出流引用
     */
    std::ostream &NSDomainSet::dump(std::ostream &os, const std::string &prefix)
    {
        base::RWMutex::ReadLock lock(m_mutex);
        os << prefix << "[NSDomainSet domain_size=" << m_datas.size() << "]" << std::endl;
        for (auto &i : m_datas) {
            os << prefix;
            i.second->dump(os, prefix + "    ") << std::endl;
        }
        return os;
    }

    /**
     * @brief 将域名集合信息转换为字符串
     * 调用dump方法将域名集合信息格式化为字符串
     * @param prefix 前缀字符串
     * @return 格式化后的域名集合信息字符串
     */
    std::string NSDomainSet::toString(const std::string &prefix)
    {
        std::stringstream ss;
        dump(ss, prefix);
        return ss.str();
    }

    /**
     * @brief 交换两个域名集合的数据
     * 加写锁后交换两个域名集合的内部存储
     * @param ds 另一个域名集合
     */
    void NSDomainSet::swap(NSDomainSet &ds)
    {
        if (this == &ds) {
            return;
        }
        base::RWMutex::WriteLock lock(m_mutex);
        base::RWMutex::WriteLock lock2(ds.m_mutex);
        m_datas.swap(ds.m_datas);
    }

} // namespace ns
} // namespace base
