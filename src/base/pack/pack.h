#pragma once

#include "macro.h"
#include <stdint.h>
#include <type_traits>

namespace base
{
namespace pack
{

    struct PackFlag {
        PackFlag(uint64_t f) : flag(f) {}

        enum Flag {

        };

        uint64_t flag;
    };

    template <class T>
    struct is__pack_type {
        typedef char YES;
        typedef int NO;

        template <class Type, class Y>
        struct FieldMatcher;

        template <typename Type>
        static YES Test(FieldMatcher<Type, typename Type::___pack_type__> *);

        template <typename Type>
        static NO Test(...);

        static const bool value = sizeof(Test<T>(nullptr)) == sizeof(YES);
    };

    template <class T>
    struct is__out_pack_type : public std::false_type {
    };

#define _IS_PACK(T, tt) typename std::enable_if<base::pack::is__pack_type<T>::value, tt>::type
#define _NOT_PACK(T, tt)                                                                           \
    typename std::enable_if<!base::pack::is__pack_type<T>::value                                   \
                                && !base::pack::is__out_pack_type<T>::value,                       \
                            tt>::type
#define _IS_PACK_OUT(T, tt)                                                                        \
    typename std::enable_if<base::pack::is__out_pack_type<T>::value, tt>::type

#define __PACK_DECODE_OUT_O(i, name) decoder.decode(#name, v.name, flag);
#define _PACK_DECODE_OUT_O(...) _REPEAT2(__PACK_DECODE_OUT_O, 0, __VA_ARGS__)

#define __PACK_DECODE_OUT_A(i, attr, name) decoder.decode(attr, v.name, flag);
#define _PACK_DECODE_OUT_A(...) _REPEAT_PAIR(__PACK_DECODE_OUT_A, 0, __VA_ARGS__)

#define _PACK_DECODE_OUT(i, arg) _PASTE(_PACK_DECODE_OUT_, arg)

#define __PACK_ENCODE_OUT_O(i, name) encoder.encode(#name, v.name, flag);
#define _PACK_ENCODE_OUT_O(...) _REPEAT2(__PACK_ENCODE_OUT_O, 0, __VA_ARGS__)

#define __PACK_ENCODE_OUT_A(i, attr, name) encoder.encode(attr, v.name, flag);
#define _PACK_ENCODE_OUT_A(...) _REPEAT_PAIR(__PACK_ENCODE_OUT_A, 0, __VA_ARGS__)

#define _PACK_ENCODE_OUT(i, arg) _PASTE(_PACK_ENCODE_OUT_, arg)

#define _PACK_OUT(C, ...)                                                                          \
    namespace base                                                                                \
    {                                                                                              \
        namespace pack                                                                             \
        {                                                                                          \
            template <>                                                                            \
            struct is__out_pack_type<C> : public std::true_type {                                  \
            };                                                                                     \
            template <class T>                                                                     \
            bool ___encode__(T &encoder, const C &v, const base::pack::PackFlag &flag)             \
            {                                                                                      \
                _REPEAT(_PACK_ENCODE_OUT, 0, __VA_ARGS__);                                         \
                return true;                                                                       \
            }                                                                                      \
            template <class T>                                                                     \
            bool ___decode__(T &decoder, C &v, const base::pack::PackFlag &flag)                   \
            {                                                                                      \
                _REPEAT(_PACK_DECODE_OUT, 0, __VA_ARGS__);                                         \
                return true;                                                                       \
            }                                                                                      \
        }                                                                                          \
    }

#define __PACK_DECODE_O(i, name) decoder.decode(#name, name, flag);
#define _PACK_DECODE_O(...) _REPEAT2(__PACK_DECODE_O, 0, __VA_ARGS__)

#define __PACK_DECODE_A(i, attr, name) decoder.decode(attr, name, flag);
#define _PACK_DECODE_A(...) _REPEAT_PAIR(__PACK_DECODE_A, 0, __VA_ARGS__)

#define __PACK_DECODE_I(i, type) decoder.decode_inherit(static_cast<type &>(*this), flag);
#define _PACK_DECODE_I(...) _REPEAT2(__PACK_DECODE_I, 0, __VA_ARGS__)

#define _PACK_DECODE(i, arg) _PASTE(_PACK_DECODE_, arg)

#define __PACK_ENCODE_O(i, name) encoder.encode(#name, name, flag);
#define _PACK_ENCODE_O(...) _REPEAT2(__PACK_ENCODE_O, 0, __VA_ARGS__)

#define __PACK_ENCODE_A(i, attr, name) encoder.encode(attr, name, flag);
#define _PACK_ENCODE_A(...) _REPEAT_PAIR(__PACK_ENCODE_A, 0, __VA_ARGS__)

#define __PACK_ENCODE_I(i, type) encoder.encode_inherit(static_cast<const type &>(*this), flag);
#define _PACK_ENCODE_I(...) _REPEAT2(__PACK_ENCODE_I, 0, __VA_ARGS__)

#define _PACK_ENCODE(i, arg) _PASTE(_PACK_ENCODE_, arg)

#define _PACK_FLAG_DEFINE                                                                          \
public:                                                                                            \
    typedef bool ___pack_type__;

#define _PACK(...)                                                                                 \
    _PACK_FLAG_DEFINE                                                                              \
    template <class T>                                                                             \
    bool ___encode__(T &encoder, const base::pack::PackFlag &flag) const                           \
    {                                                                                              \
        _REPEAT(_PACK_ENCODE, 0, __VA_ARGS__);                                                     \
        return true;                                                                               \
    }                                                                                              \
    template <class T>                                                                             \
    bool ___decode__(T &decoder, const base::pack::PackFlag &flag)                                 \
    {                                                                                              \
        _REPEAT(_PACK_DECODE, 0, __VA_ARGS__);                                                     \
        return true;                                                                               \
    }

} // namespace pack
} // namespace base
