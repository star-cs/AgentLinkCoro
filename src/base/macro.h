#pragma once

#include <string.h>
#include <assert.h>
#include "base/log/log.h"
#include "base/util.h"

#if defined __GNUC__ || defined __llvm__
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率成立
#    define _LIKELY(x) __builtin_expect(!!(x), 1)
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率不成立
#    define _UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#    define _LIKELY(x) (x)
#    define _UNLIKELY(x) (x)
#endif

/// 断言宏封装
#define _ASSERT(x)                                                                                 \
    if (_UNLIKELY(!(x))) {                                                                         \
        _LOG_ERROR(_LOG_ROOT()) << "ASSERTION: " #x << "\nbacktrace:\n"                            \
                                << base::BacktraceToString(100, 2, "    ");                        \
        assert(x);                                                                                 \
    }

/// 断言宏封装
#define _ASSERT2(x, w)                                                                             \
    if (_UNLIKELY(!(x))) {                                                                         \
        _LOG_ERROR(_LOG_ROOT()) << "ASSERTION: " #x << "\n"                                        \
                                << w << "\nbacktrace:\n"                                           \
                                << base::BacktraceToString(100, 2, "    ");                        \
        assert(x);                                                                                 \
    }

#define __STR(...) #__VA_ARGS__
#define _TSTR(...) __STR(__VA_ARGS__)
#define _DSTR(s, d) (NULL != (s) ? (const char *)(s) : (const char *)(d))
#define _SSTR(s) _DSTR(s, "")
#define _SLEN(s) (NULL != (s) ? strlen((const char *)(s)) : 0)
