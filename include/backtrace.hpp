#ifndef __BACKTRACE_HPP
#define __BACKTRACE_HPP

#include "pin.H"
#include <iostream>

using namespace std;

static const INT32 maxDepth = 3;

// Nothing within Backtrace is thread-safe since all of its
// methods are only ever executed by one thread
//
struct Backtrace {
    // A Backtrace is initialized with the maximum number of stack frames
    // that it will go down
    //
    Backtrace() {
        for (INT32 i = 0; i < maxDepth; i++) {
            _fileNames[i] = "";
            _lineNumbers[i] = 0;
        }
    }

    VOID SetTrace(CONTEXT *ctxt) {
        // buf contains maxDepth + 1 addresses because PIN_Backtrace also returns
        // the stack frame for malloc/free
        //
        VOID *buf[maxDepth + 1];
        INT32 depth;

        if (ctxt == nullptr) {
            return;
        }
    
        // Pin requires us to call Pin_LockClient() before calling PIN_Backtrace
        // and PIN_GetSourceLocation
        //
        PIN_LockClient();
        depth = PIN_Backtrace(ctxt, buf, maxDepth + 1) - 1;

        // We set i = 1 because we don't want to include the stack frame 
        // for malloc/free
        //
        for (INT32 i = 1; i < depth + 1; i++) {
            // NOTE: executable must be compiled with -g -gdwarf-2 -rdynamic
            // to locate the invocation of malloc/free
            // NOTE: PIN_GetSourceLocation does not necessarily get the exact
            // invocation point, but it's pretty close
            //
            PIN_GetSourceLocation((ADDRINT) buf[i], nullptr, 
                                    _lineNumbers + i - 1,
                                    _fileNames + i - 1);
        }
        PIN_UnlockClient();
    }

    Backtrace &operator=(const Backtrace &b) {
        for (INT32 i = 0; i < maxDepth; i++) {
            _fileNames[i] = b._fileNames[i];
            _lineNumbers[i] = b._lineNumbers[i];
        }
        return *this;
    }

    string _fileNames[maxDepth];
    INT32 _lineNumbers[maxDepth];
};

ostream& operator<<(ostream& os, Backtrace& bt) {
    os << "[";
    for (int i = 0; i < maxDepth; i++) {
        if (bt._lineNumbers[i] == 0) {
            os << "{\"path\":\"\",\"line\":0}";
        } else {
            os << "{\"path\":\"" << bt._fileNames[i] << "\",\"line\":" << bt._lineNumbers[i] << "}";
        }
        if (i < maxDepth - 1) {
            os << ",";
        }
    }
    os << "]";
    return os;
}

#endif
