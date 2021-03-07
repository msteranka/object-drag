#include "pin.H"
#include <fstream>
#include <iostream>
#include <utility>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <sstream>
#include <stdatomic.h>
#include <signal.h>
#include "objectdata.hpp"
#include "backtrace.hpp"
#include "objectmanager.hpp"
#include "mytls.hpp"
#include "misc.hpp"

#if defined(_MSC_VER)
# define LIKELY(x) (x)
# define UNLIKELY(x) (x)
#else
# define LIKELY(x) __builtin_expect(!!(x), 1)
# define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif // _MSC_VER

#if defined(TARGET_MAC)
# define MALLOC "_malloc"
# define FREE "_free"
#else
# define MALLOC "malloc"
# define FREE "free"
#endif // TARGET_MAC

using namespace std;

static ObjectManager manager;
static TLS_KEY tls_key = INVALID_TLS_KEY; // Thread Local Storage
static AFUNPTR clockFun;

namespace DefaultParams {
    static const std::string defaultIsVerbose = "0",
        defaultTraceFile = "drag.out",
        defaultEnableInstrumentation = "0";
}

namespace Params {
    static BOOL isVerbose, enableInstrumentation;
    static std::ofstream traceFile;
};

// VOID SignalHandler(THREADID tid, CONTEXT_CHANGE_REASON reason, const CONTEXT *from, const CONTEXT *to, INT32 info, VOID *v) {
//     std::cout << "SignalHandler received signal " << info << std::endl;
//     if (reason == CONTEXT_CHANGE_REASON_SIGNAL) {
//         if (info == SIGUSR1) {
//             Params::enableInstrumentation = true;
//         } else if (info == SIGUSR2) {
//             Params::enableInstrumentation = false;
//         }
//     }
// }

BOOL EnableInstrumentation(THREADID tid, INT32 sig, CONTEXT *ctxt, BOOL hasHandler, const EXCEPTION_INFO *pExceptInfo, VOID *v) {
    Params::enableInstrumentation = true;
    return false; // Don't pass SIGUSR1 to application
}

BOOL DisableInstrumentation(THREADID tid, INT32 sig, CONTEXT *ctxt, BOOL hasHandler, const EXCEPTION_INFO *pExceptInfo, VOID *v) {
    Params::enableInstrumentation = false;
    return false; // Don't pass SIGUSR2 to application
}

VOID ThreadStart(THREADID threadId, CONTEXT *ctxt, INT32 flags, VOID* v) {
    MyTLS *tls = new MyTLS;
    assert(PIN_SetThreadData(tls_key, tls, threadId));
}

VOID ThreadFini(THREADID threadId, const CONTEXT *ctxt, INT32 code, VOID* v) {
    MyTLS *tls = static_cast<MyTLS*>(PIN_GetThreadData(tls_key, threadId));
    delete tls;
}

// Function arguments and backtrace can only be accessed at the function entry point
// Thus, we must insert a routine before malloc and cache these values
//
VOID MallocBefore(THREADID threadId, CONTEXT *ctxt, ADDRINT size) {
    if (!Params::enableInstrumentation) {
        return;
    }
    MyTLS *tls = static_cast<MyTLS*>(PIN_GetThreadData(tls_key, threadId));
    tls->_cachedSize = size;
    tls->_cachedBacktrace.SetTrace(ctxt);
}

VOID MallocAfter(THREADID threadId, ADDRINT retVal) {
    // Check for success since we don't want to track null pointers
    //
    if (!Params::enableInstrumentation || (VOID *) retVal == nullptr) { 
        return; 
    }

    MyTLS *tls = static_cast<MyTLS*>(PIN_GetThreadData(tls_key, threadId));
    manager.InsertObject(retVal, tls->_cachedSize, tls->_cachedBacktrace, threadId);
}

VOID FreeBefore(THREADID threadId, CONTEXT *ctxt, ADDRINT ptr) {
    if (!Params::enableInstrumentation) {
        return;
    }

    Backtrace freeBacktrace;
    clock_t t;

    freeBacktrace.SetTrace(ctxt);
    t = clock();
    // TODO: Call clock(3) within context of application
    // if (clockFun) {
    //     PIN_CallApplicationFunction(ctxt, threadId, CALLINGSTD_DEFAULT, // Call clock()
    //                                 clockFun, nullptr, 
    //                                 PIN_PARG(clock_t), &t, 
    //                                 PIN_PARG_END());
    // }
    manager.DeleteObject(ptr, freeBacktrace, threadId, t);
}

VOID MemAccess(THREADID threadId, ADDRINT addrAccessed, UINT32 accessSize, const CONTEXT *ctxt) {
    if (!Params::enableInstrumentation) {
        return;
    }

    clock_t t = clock();
    // TODO: PIN_CallApplicationFunction has a high overhead -- Better solution?
    // if (clockFun) {
    // PIN_CallApplicationFunction(ctxt, threadId, CALLINGSTD_DEFAULT, // Call clock()
    //                             clockFun, nullptr, 
    //                             PIN_PARG(clock_t), &t, 
    //                             PIN_PARG_END());
    // }

    manager.UpdateLastAccess(addrAccessed, t, threadId, ctxt);
}

VOID Instruction(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins) && !INS_IsStackRead(ins)) {
        // Intercept read instructions that don't read from the stack with MemAccess
        //
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) MemAccess,
                        IARG_THREAD_ID,
                        IARG_MEMORYREAD_EA,
                        IARG_MEMORYREAD_SIZE,
                        IARG_CONST_CONTEXT,
                        IARG_END);
    }

    if (INS_IsMemoryWrite(ins) && !INS_IsStackWrite(ins)) {
        // Intercept write instructions that don't write to the stack with MemAccess
        //
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) MemAccess,
                        IARG_THREAD_ID,
                        IARG_MEMORYWRITE_EA,
                        IARG_MEMORYWRITE_SIZE,
                        IARG_CONST_CONTEXT,
                        IARG_END);
    }
}

VOID Image(IMG img, VOID *v) {
    RTN rtn;

    rtn = RTN_FindByName(img, MALLOC);
    if (RTN_Valid(rtn)) {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR) MallocBefore, // Hook calls to malloc with MallocBefore
                        IARG_THREAD_ID,
                        IARG_CONST_CONTEXT,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 
                        0, IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR) MallocAfter, // Hook calls to malloc with MallocAfter
                        IARG_THREAD_ID,
                        IARG_FUNCRET_EXITPOINT_VALUE, 
                        IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, FREE);
    if (RTN_Valid(rtn)) {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR) FreeBefore, // Hook calls to free with FreeBefore
                        IARG_THREAD_ID,
                        IARG_CONST_CONTEXT, 
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 
                        0, IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "clock");
    if (RTN_Valid(rtn)) {
        clockFun = RTN_Funptr(rtn);
    } else {
        clockFun = nullptr;
    }
}

VOID Fini(INT32 code, VOID *v) {
    Params::traceFile << manager;
}

INT32 Usage() {
    return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
    KNOB<UINT32> knobIsVerbose(KNOB_MODE_WRITEONCE, "pintool", "v", 
                            DefaultParams::defaultIsVerbose,
                            "Dispay additional information including backtraces");
    KNOB<std::string> knobTraceFile(KNOB_MODE_WRITEONCE, "pintool", "o", 
                            DefaultParams::defaultTraceFile,
                            "Name of output file");
    KNOB<UINT32> knobEnableInstrumentation(KNOB_MODE_WRITEONCE, "pintool", "i",
                            DefaultParams::defaultEnableInstrumentation,
                            "Whether instrumentation should begin enabled");

    PIN_InitSymbols();
    if (PIN_Init(argc, argv))  {
        return Usage();
    }

    Params::isVerbose = knobIsVerbose.Value();
    Params::enableInstrumentation = knobEnableInstrumentation.Value();
    Params::traceFile.open(knobTraceFile.Value().c_str());
    Params::traceFile.setf(ios::showbase);

    tls_key = PIN_CreateThreadDataKey(NULL);
    if (tls_key == INVALID_TLS_KEY) {
        std::cerr << "number of already allocated keys reached the MAX_CLIENT_TLS_KEYS limit" << std::endl;
        PIN_ExitProcess(1);
    }

    // PIN_AddContextChangeFunction((CONTEXT_CHANGE_CALLBACK) SignalHandler, nullptr); // Equivalent to sigaction(2)
    PIN_InterceptSignal(SIGUSR1, EnableInstrumentation, nullptr);
    PIN_InterceptSignal(SIGUSR2, DisableInstrumentation, nullptr);
    IMG_AddInstrumentFunction(Image, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
	PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
}
