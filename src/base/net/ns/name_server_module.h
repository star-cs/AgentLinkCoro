#ifndef __BASE_NET_NS_NAME_SERVER_MODULE_H__
#define __BASE_NET_NS_NAME_SERVER_MODULE_H__

#include "base/application/module.h"
#include "ns_protocol.h"

namespace base
{
namespace ns
{

    /**
     * @brief 名称服务器客户端信息类
     * 存储客户端会话相关信息，包括节点信息和关注的域名-命令映射
     */
    class NameServerModule;
    class NSClientInfo
    {
        friend class NameServerModule;

    public:
        /// 智能指针类型定义
        typedef std::shared_ptr<NSClientInfo> ptr;

    private:
        /// 客户端节点信息
        NSNode::ptr m_node;
        /// 域名到命令集的映射，记录客户端关注的域名及对应的命令
        std::map<std::string, std::set<uint32_t> > m_domain2cmds;
    };

    /**
     * @brief 名称服务器模块类
     * 实现名称服务的核心功能，负责节点注册、发现、查询和变更通知
     */
    class NameServerModule : public RockModule
    {
    public:
        /// 智能指针类型定义
        typedef std::shared_ptr<NameServerModule> ptr;

        /**
         * @brief 构造函数
         * 初始化名称服务器模块
         */
        NameServerModule();

        /**
         * @brief 处理Rock协议请求
         * 重写自RockModule，处理来自客户端的各种请求
         * @param request 请求对象
         * @param response 响应对象
         * @param stream 连接流
         * @return 处理是否成功
         */
        virtual bool handleRockRequest(base::RockRequest::ptr request,
                                       base::RockResponse::ptr response,
                                       base::RockStream::ptr stream) override;

        /**
         * @brief 处理Rock协议通知
         * 重写自RockModule，处理来自客户端的通知消息
         * @param notify 通知对象
         * @param stream 连接流
         * @return 处理是否成功
         */
        virtual bool handleRockNotify(base::RockNotify::ptr notify,
                                      base::RockStream::ptr stream) override;

        /**
         * @brief 连接建立回调
         * 当客户端连接建立时的处理
         * @param stream 建立的连接流
         * @return 处理是否成功
         */
        virtual bool onConnect(base::Stream::ptr stream) override;

        /**
         * @brief 连接断开回调
         * 当客户端连接断开时的处理，需要清理相关资源
         * @param stream 断开的连接流
         * @return 处理是否成功
         */
        virtual bool onDisconnect(base::Stream::ptr stream) override;

        /**
         * @brief 获取模块状态字符串
         * 生成并返回模块当前状态的描述字符串
         * @return 状态描述字符串
         */
        virtual std::string statusString() override;

    private:
        /**
         * @brief 处理注册请求
         * 处理节点注册请求，更新节点信息
         * @param request 注册请求
         * @param response 响应对象
         * @param stream 连接流
         * @return 处理是否成功
         */
        bool handleRegister(base::RockRequest::ptr request, base::RockResponse::ptr response,
                            base::RockStream::ptr stream);

        /**
         * @brief 处理查询请求
         * 处理域名查询请求，返回对应的节点信息
         * @param request 查询请求
         * @param response 响应对象
         * @param stream 连接流
         * @return 处理是否成功
         */
        bool handleQuery(base::RockRequest::ptr request, base::RockResponse::ptr response,
                         base::RockStream::ptr stream);

        /**
         * @brief 处理心跳请求
         * 处理客户端心跳，维持会话活跃状态
         * @param request 心跳请求
         * @param response 响应对象
         * @param stream 连接流
         * @return 处理是否成功
         */
        bool handleTick(base::RockRequest::ptr request, base::RockResponse::ptr response,
                        base::RockStream::ptr stream);

    private:
        /**
         * @brief 获取客户端信息
         * 根据连接流获取对应的客户端信息
         * @param rs Rock连接流
         * @return 客户端信息智能指针
         */
        NSClientInfo::ptr get(base::RockStream::ptr rs);

        /**
         * @brief 设置客户端信息
         * 关联连接流和客户端信息
         * @param rs Rock连接流
         * @param info 客户端信息
         */
        void set(base::RockStream::ptr rs, NSClientInfo::ptr info);

        /**
         * @brief 设置查询域名
         * 记录客户端关注的域名集合
         * @param rs Rock连接流
         * @param ds 域名集合
         */
        void setQueryDomain(base::RockStream::ptr rs, const std::set<std::string> &ds);

        /**
         * @brief 发送通知
         * 向关注指定域名的所有客户端发送通知消息
         * @param domains 域名集合
         * @param nty 通知消息
         */
        void doNotify(std::set<std::string> &domains, std::shared_ptr<NotifyMessage> nty);

        /**
         * @brief 获取关注指定域名的连接流
         * 返回所有关注特定域名的客户端连接流
         * @param domain 域名
         * @return 连接流集合
         */
        std::set<base::RockStream::ptr> getStreams(const std::string &domain);

    private:
        /// 域名集合，存储所有注册的域名信息
        NSDomainSet::ptr m_domains;

        /// 读写锁，保护共享数据
        base::RWMutex m_mutex;
        /// 会话映射表，关联连接流和客户端信息
        std::map<base::RockStream::ptr, NSClientInfo::ptr> m_sessions;

        /// 会话关注的域名映射表
        std::map<base::RockStream::ptr, std::set<std::string> > m_queryDomains;
        /// 域名对应关注的会话映射表
        std::map<std::string, std::set<base::RockStream::ptr> > m_domainToSessions;
    };

} // namespace ns
} // namespace base

#endif // __BASE_NET_NS_NAME_SERVER_MODULE_H__
