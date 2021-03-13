#if !defined(__MY_TLS_HPP)
# define __MY_TLS_HPP

#include "pin.H"
#include "backtrace.hpp"

struct MyTLS {
    MyTLS(void *cachedPtr, size_t cachedSize) : 
          _cachedPtr(cachedPtr), 
          _cachedSize(cachedSize) { }

    void *_cachedPtr;
    size_t _cachedSize;
    Backtrace _cachedBacktrace;
};

#endif // __MY_TLS_HPP
