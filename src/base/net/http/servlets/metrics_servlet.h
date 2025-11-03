#pragma once

#include "base/net/http/servlet.h"
#include "base/util/prometheus.h"

namespace base
{
namespace http
{

    class MetricsServlet : public Servlet
    {
    public:
        MetricsServlet();
        virtual int32_t handle(base::http::HttpRequest::ptr request,
                               base::http::HttpResponse::ptr response,
                               base::SocketStream::ptr session) override;
    };

    PrometheusRegistry::ptr GetPrometheusRegistry();

} // namespace http
} // namespace base