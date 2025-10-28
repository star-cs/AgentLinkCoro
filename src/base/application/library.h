#ifndef ___LIBRARY_H__
#define ___LIBRARY_H__

#include <memory>
#include "module.h"

namespace base
{

class Library
{
public:
    static Module::ptr GetModule(const std::string &path);
};

} // namespace base

#endif
