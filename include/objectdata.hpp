#if !defined( __OBJECT_DATA_HPP)
# define __OBJECT_DATA_HPP

#include "pin.H"
#include "backtrace.hpp"
#include "objectfrag.hpp"
#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>

// TODO: allow this to be changed
static const size_t fragSize = sizeof(uintptr_t);

struct ObjectData {
    ObjectData(ADDRINT addr, UINT32 size, THREADID mallocThread, 
               Backtrace mallocTrace) : 
        _addr(addr),
        _size(size),
        _mallocThread(mallocThread),
        _freeThread(-1),
        _mallocTrace(mallocTrace)
        { 
            // _mallocTrace = mallocTrace;
            for (UINT32 i = 0; i <= size; i += fragSize) {
                _fragments.push_back(ObjectFragment(0, "", 0));
            }
        }

    // TODO: overlap into two fragments?
    ObjectFragment *GetFragment(ADDRINT fragAddr) {
        assert(fragAddr >= _addr && fragAddr < _addr + _size);
        UINT32 i = (fragAddr - _addr) / fragSize;
        // printf("Calculated i = %u, Max = %lu\n", i, _fragments.size());
        // printf("fragAddr = %lu, _addr = %lu, fragSize = %lu, _size = %u\n", fragAddr, _addr, fragSize, _size);
        assert(i >= 0 && i < _fragments.size());
        return &_fragments[i];
    }

    ADDRINT _addr;
    UINT32 _size;
    THREADID _mallocThread, _freeThread;
    Backtrace _mallocTrace, _freeTrace;
    unsigned long _freeTime;
    std::vector<ObjectFragment> _fragments;
};

std::ostream &operator<<(std::ostream &os, ObjectData &d) {
    os << "{" <<
        "\"addr\":" << d._addr << "," <<
        "\"size\":" << d._size << "," <<
        "\"mtid\":" << d._mallocThread << "," <<
        "\"ftid\":" << d._freeThread << "," <<
        "\"ftime\":" << d._freeTime << "," <<
        "\"mtrace\":" << d._mallocTrace << "," <<
        "\"ftrace\":" << d._freeTrace << "," <<
        "\"frags\":[";

    for (UINT32 i = 0; i < d._fragments.size() - 1; i++) {
        os << d._fragments[i] << ",";
    }

    os << d._fragments.back() << "]}";
    return os;
}

#endif
