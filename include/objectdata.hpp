#ifndef __OBJECT_DATA_HPP
#define __OBJECT_DATA_HPP

#include "pin.H"
#include "backtrace.hpp"
#include <iostream>
#include <ctime> // PinCRT doesn't support <chrono>

struct ObjectData {
    ObjectData(ADDRINT addr, UINT32 size, THREADID mallocThread, Backtrace mallocTrace) : 
        _addr(addr),
        _size(size),
        _isLive(true),
        _mallocThread(mallocThread),
        _freeThread(-1) 
        { 
            _mallocTrace = mallocTrace;
        }

    ADDRINT _addr;
    UINT32 _size;
    THREADID _mallocThread, _freeThread;
    Backtrace _mallocTrace, _freeTrace;
    clock_t _lastAccess, _freeTime;
    // std::string _accessPath;
    // INT32 _accessLine;
};

#endif
