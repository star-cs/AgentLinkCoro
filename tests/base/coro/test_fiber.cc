#include "base/coro/fiber.h"
#include "base/log/log.h"

base::Logger::ptr g_logger = _LOG_ROOT();

void run_in_fiber()
{
    _LOG_INFO(g_logger) << "run_in_fiber begin";
    base::Fiber::GetThis()->back();
    _LOG_INFO(g_logger) << "run_in_fiber end";
    base::Fiber::GetThis()->back();
}

/**
 * 测试fiber协程功能的函数
 * 展示了主协程和子协程之间的切换和执行流程
 */
void test_fiber()
{
    _LOG_INFO(g_logger) << "main begin -1";
    {
        base::Fiber::ptr main_fiber = base::Fiber::GetThis();
        _LOG_INFO(g_logger) << "main begin";
        base::Fiber::ptr fiber(base::NewFiber(run_in_fiber, 128 * 1024, true), base::FreeFiber);

        // 使用call()和back()方法进行协程切换，而不是swapIn()/swapOut()
        fiber->call();
        _LOG_INFO(g_logger) << "main after call";
        fiber->call();
        _LOG_INFO(g_logger) << "main after call2";
    }
    _LOG_INFO(g_logger) << "main after end2";
}

int main(int argc, char **argv)
{
    base::Thread::SetName("main");

    std::vector<base::Thread::ptr> thrs;
    for (int i = 0; i < 1; ++i) {
        thrs.push_back(
            base::Thread::ptr(new base::Thread(&test_fiber, "name_" + std::to_string(i))));
    }
    for (auto i : thrs) {
        i->join();
    }
    return 0;
}
