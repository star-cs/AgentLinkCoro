#pragma once

#include "base/net/http/servlet.h"

namespace base
{
namespace http
{

    class ConfigServlet : public Servlet
    {
    public:
        ConfigServlet();
        virtual int32_t handle(base::http::HttpRequest::ptr request,
                               base::http::HttpResponse::ptr response,
                               base::SocketStream::ptr session) override;
    };

} // namespace http
} // namespace base