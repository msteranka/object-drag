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
            // trace[i].first = "";
            // trace[i].second = 0;
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
                                    // &(trace[i - 1].second),
                                    // &(trace[i - 1].first));
        }
        PIN_UnlockClient();
    }

    Backtrace &operator=(const Backtrace &b) {
        for (INT32 i = 0; i < maxDepth; i++) {
            _fileNames[i] = b._fileNames[i];
            _lineNumbers[i] = b._lineNumbers[i];
            // trace[i].first = b.trace[i].first;
            // trace[i].second = b.trace[i].second;
        }
        return *this;
    }

    string _fileNames[maxDepth];
    INT32 _lineNumbers[maxDepth];
};

ostream& operator<<(ostream& os, Backtrace& bt)
{
    int i;

    for (i = 0; i < maxDepth - 1; i++) {
        if (bt._lineNumbers[i] == 0) {
            os << "\t\t(NIL)" << std::endl;
        } else {
            os << "\t\t" << bt._fileNames[i] << ":" << bt._lineNumbers[i] << std::endl;
        }
    }

    if (bt._lineNumbers[i] == 0) {
        os << "\t\t(NIL)";
    } else {
        os << "\t\t" << bt._fileNames[i] << ":" << bt._lineNumbers[i];
    }

    return os;
}

#endif
