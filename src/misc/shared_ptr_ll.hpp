#pragma once

#include <memory>

namespace ll {

// Lock-less shared pointer, even when linking pthread
template<typename T>
using shared_ptr = std::__shared_ptr<T, __gnu_cxx::_S_single>;

template<typename T>
using weak_ptr = std::__weak_ptr<T, __gnu_cxx::_S_single>;

}
