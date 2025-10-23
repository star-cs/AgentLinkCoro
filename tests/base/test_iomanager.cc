#include "base/coro/fiber.h"
#include "base/log/log.h"
#include "base/coro/iomanager.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>

base::Logger::ptr g_logger = _LOG_ROOT();

int sock = 0;

void test_fiber()
{
    _LOG_INFO(g_logger) << "test_fiber sock=" << sock;

    sleep(3);

    // close(sock);
    // base::IOManager::GetThis()->cancelAll(sock);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "223.5.5.5", &addr.sin_addr.s_addr);

    if (!connect(sock, (const sockaddr *)&addr, sizeof(addr))) {
    } else if (errno == EINPROGRESS) {
        _LOG_INFO(g_logger) << "add event errno=" << errno << " " << strerror(errno);
        base::IOManager::GetThis()->addEvent(sock, base::IOManager::READ,
                                             []() { _LOG_INFO(g_logger) << "read callback"; });
        base::IOManager::GetThis()->addEvent(sock, base::IOManager::WRITE, []() {
            _LOG_INFO(g_logger) << "write callback";
            // close(sock);
            base::IOManager::GetThis()->cancelEvent(sock, base::IOManager::READ);
            close(sock);
        });
    } else {
        _LOG_INFO(g_logger) << "else " << errno << " " << strerror(errno);
    }
}

void test1()
{
    std::cout << "EPOLLIN=" << EPOLLIN << " EPOLLOUT=" << EPOLLOUT << std::endl;
    base::IOManager iom(2, false, "", false);
    iom.schedule(&test_fiber);
}

base::Timer::ptr s_timer;
void test_timer()
{
    base::IOManager iom(2, true, "", false);
    s_timer = iom.addTimer(
        1000,
        []() {
            static int i = 0;
            _LOG_INFO(g_logger) << "hello timer i=" << i;
            if (++i == 3) {
                s_timer->reset(2000, true);
                // s_timer->cancel();
            }
        },
        true);
}

int main(int argc, char **argv)
{
    test1();
    test_timer();
    return 0;
}
