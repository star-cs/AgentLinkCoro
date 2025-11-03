#include "consul_client.h"
#include "base/util.h"
#include "base/log/log.h"
#include "base/pack/json_encoder.h"
#include "base/net/http/http_connection.h"
#include "base/pack/json_decoder.h"

namespace base
{

static base::Logger::ptr g_logger = _LOG_NAME("system");

std::string GetConsulUniqID(int port)
{
    return base::GetHostName() + "-" + base::replace(base::GetIPv4(), '.', '-') + "-"
           + std::to_string(port) + "-" + std::to_string((base::GetCurrentUS() % 100000) + 100000);
}

bool ConsulClient::serviceRegister(ConsulRegisterInfo::ptr info)
{
    std::string data = base::pack::EncodeToJsonString(info, 0);
    std::string url = m_url + "/v1/agent/service/register";
    auto rt = base::http::HttpConnection::DoRequest(base::http::HttpMethod::PUT, url, 5000,
                                                    {{"content-type", "application/json"}}, data);
    if (rt->result) {
        _LOG_ERROR(g_logger) << "register fail, " << url << " - " << data << " - "
                             << rt->toString();
        return false;
    }

    if (rt->response->getStatus() != base::http::HttpStatus::OK) {
        _LOG_ERROR(g_logger) << "register fail," << url << " - " << data << " - " << rt->toString();
        return false;
    }
    _LOG_INFO(g_logger) << url << " - " << data;
    return true;
}

bool ConsulClient::serviceUnregister(const std::string &id)
{
    for (int i = 0; i < 2; ++i) {
        std::string url = m_url + "/v1/agent/service/deregister/" + base::StringUtil::UrlEncode(id);
        auto rt = base::http::HttpConnection::DoRequest(base::http::HttpMethod::PUT, url, 5000,
                                                        {{"content-type", "application/json"}}, "");
        if (rt->result) {
            _LOG_ERROR(g_logger) << "unregister fail, " << url << " - " << rt->toString();
            return false;
        }

        if (rt->response->getStatus() != base::http::HttpStatus::OK) {
            _LOG_ERROR(g_logger) << "unregister fail," << url << " - " << rt->toString();
            return false;
        }
        _LOG_INFO(g_logger) << url;
    }
    return true;
}

struct ConsulServiceData {
    std::string service;
    std::string address;
    uint16_t port;
    std::list<std::string> tags;

    _PACK(A("Service", service, "Address", address, "Port", port, "Tags", tags));
};

static void parse_map(const std::list<std::string> &l, std::map<std::string, std::string> &m)
{
    for (auto &i : l) {
        auto parts = base::split(i, '=');
        if (parts.size() == 2) {
            m[parts[0]] = parts[1];
        }
    }
}

bool ConsulClient::serviceQuery(const std::unordered_set<std::string> &service_names,
                                std::map<std::string, std::list<ConsulServiceInfo::ptr> > &infos)
{
    std::string url = m_url + "/v1/agent/services";
    auto rt = base::http::HttpConnection::DoGet(url, 5000);
    if (rt->result) {
        _LOG_ERROR(g_logger) << "servicequery fail, " << url << " - " << rt->toString();
        return false;
    }

    if (rt->response->getStatus() != base::http::HttpStatus::OK) {
        _LOG_ERROR(g_logger) << "servicequery fail," << url << " - " << rt->toString();
        return false;
    }

    std::map<std::string, ConsulServiceData> m;
    if (!base::pack::DecodeFromJsonString(rt->response->getBody(), m, 0)) {
        _LOG_ERROR(g_logger) << "servicequery parse fail," << url << " - "
                             << rt->response->getBody();
        return false;
    }

    if (service_names.empty()) {
        for (auto &item : m) {
            auto csi = std::make_shared<ConsulServiceInfo>(item.second.address, item.second.port);
            parse_map(item.second.tags, csi->tags);
            infos[item.second.service].push_back(csi);
        }
    } else {
        for (auto &item : m) {
            if (service_names.count(item.second.service)) {
                auto csi =
                    std::make_shared<ConsulServiceInfo>(item.second.address, item.second.port);
                parse_map(item.second.tags, csi->tags);
                infos[item.second.service].push_back(csi);
            }
        }
    }
    _LOG_INFO(g_logger) << "serviceQuery " << base::pack::EncodeToJsonString(service_names, 0)
                        << " - " << base::pack::EncodeToJsonString(infos, 0);
    // _LOG_INFO(g_logger) << rt->response->getBody() << std::endl;
    return true;
}

} // namespace base
