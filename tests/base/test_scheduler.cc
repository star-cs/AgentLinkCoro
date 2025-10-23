#include "base/coro/fiber.h"
#include "base/log/log.h"
#include "base/coro/scheduler.h"

static base::Logger::ptr g_logger = _LOG_ROOT();

void test_fiber()
{
    static int s_count = 5;
    _LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;

    sleep(1);
    if (--s_count >= 0) {
        base::Scheduler::GetThis()->schedule(&test_fiber, base::GetThreadId());
    }
}

int main(int argc, char **argv)
{
    _LOG_INFO(g_logger) << "main";
    base::Scheduler sc(3, false, "test", false);
    sc.start();
    sleep(2);
    _LOG_INFO(g_logger) << "schedule";
    sc.schedule(&test_fiber);
    sc.stop();
    _LOG_INFO(g_logger) << "over";
    return 0;
}
