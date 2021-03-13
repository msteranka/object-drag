#if !defined( __OBJECT_FRAGMENT_HPP)
# define __OBJECT_FRAGMENT_HPP

#include "pin.H"
#include <string>

struct ObjectFragment {
    ObjectFragment() : 
        _lastAccess(0), 
        _wasAccessed(false),
        _accessPath(""), 
        _accessLine(0) { }

    unsigned long _lastAccess;
    bool _wasAccessed;
    std::string _accessPath;
    INT32 _accessLine;
};

std::ostream &operator<<(std::ostream &os, ObjectFragment &f) {
    os << "{" <<
        "\"atime\":" << f._lastAccess << "," <<
        "\"astat\":" << f._wasAccessed << "," <<
        "\"apath\":\"" << f._accessPath << "\"," <<
        "\"aline\":" << f._accessLine << 
        "}";
    return os;
}

#endif
