#pragma once

#include <cstdint>

namespace base
{

typedef void *fcontext_t;

extern "C" intptr_t jump_fcontext(fcontext_t *ofc, fcontext_t nfc, intptr_t vp,
                                  bool preserve_fpu = false);
extern "C" fcontext_t make_fcontext(void *sp, std::size_t size, void (*fn)(intptr_t));

} // namespace base
