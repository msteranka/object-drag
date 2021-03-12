#if !defined( __OBJECT_FRAGMENT_HPP)
# define __OBJECT_FRAGMENT_HPP

#include "pin.H"
#include <string>

struct ObjectFragment {
    ObjectFragment(unsigned long lastAccess, std::string accessPath, 
                   INT32 accessLine) : 
        _lastAccess(lastAccess), 
        _accessPath(accessPath), 
        _accessLine(accessLine) { }

    unsigned long _lastAccess;
    std::string _accessPath;
    INT32 _accessLine;
};

std::ostream &operator<<(std::ostream &os, ObjectFragment &f) {
    os << "{" <<
        "\"atime\":" << f._lastAccess << "," <<
        "\"apath\":\"" << f._accessPath << "\"," <<
        "\"aline\":" << f._accessLine << 
        "}";
    return os;
}

#endif
