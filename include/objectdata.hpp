#if !defined( __OBJECT_DATA_HPP)
# define __OBJECT_DATA_HPP

#include "pin.H"
#include "backtrace.hpp"
#include <iostream>
#include <ctime> // PinCRT doesn't support <chrono>

struct ObjectData {
    ObjectData(ADDRINT addr, UINT32 size, THREADID mallocThread, Backtrace mallocTrace) : 
        _addr(addr),
        _size(size),
        _mallocThread(mallocThread),
        _freeThread(-1),
        _accessLine(0)
        { 
            _mallocTrace = mallocTrace;
        }

    ADDRINT _addr;
    UINT32 _size;
    THREADID _mallocThread, _freeThread;
    Backtrace _mallocTrace, _freeTrace;
    unsigned long _lastAccess, _freeTime;
    std::string _accessPath;
    INT32 _accessLine;
};

std::ostream &operator<<(std::ostream &os, ObjectData &d) {
    os << "{" <<
        "\"addr\":" << d._addr << "," <<
        "\"size\":" << d._size << "," <<
        "\"mtid\":" << d._mallocThread << "," <<
        "\"ftid\":" << d._freeThread << "," <<
        "\"atime\":" << d._lastAccess << "," <<
        "\"ftime\":" << d._freeTime << "," <<
        "\"apath\":\"" << d._accessPath << "\"," <<
        "\"aline\":" << d._accessLine << "," <<
        "\"mtrace\":" << d._mallocTrace << "," <<
        "\"ftrace\":" << d._freeTrace <<
        "}";
    // os << "Object " << std::hex << d._addr << std::dec << ", Drag: " << d._freeTime - d._lastAccess << " bytes";
    // if (d._accessLine != 0) { // Valid path and line number
    //     os << ", Last accessed @ " << d._accessPath << ":" << d._accessLine << std::endl;
    // } else {
    //     os << std::endl;
    // }
    // os << "\tmalloc(3) backtrace: " << std::endl << d._mallocTrace << std::endl;
    // os << "\tfree(3) backtrace: " << std::endl << d._freeTrace;
    return os;
}

#endif
