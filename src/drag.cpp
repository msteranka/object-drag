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
static atomic_ulong allocTime;

namespace DefaultParams {
    static const std::string defaultIsVerbose = "0",
        defaultTraceFile = "drag.json",
        defaultEnableInstrumentation = "0";
}

namespace Params {
    static BOOL isVerbose, enableInstrumentation;
    static std::ofstream traceFile;
};

BOOL EnableInstrumentation(THREADID tid, INT32 sig, CONTEXT *ctxt, BOOL hasHandler, const EXCEPTION_INFO *pExceptInfo, VOID *v) {
    Params::enableInstrumentation = true;
    return false; // Don't pass SIGUSR1 to application
}

BOOL DisableInstrumentation(THREADID tid, INT32 sig, CONTEXT *ctxt, BOOL hasHandler, const EXCEPTION_INFO *pExceptInfo, VOID *v) {
    Params::enableInstrumentation = false;
    return false; // Don't pass SIGUSR2 to application
}

VOID ThreadStart(THREADID threadId, CONTEXT *ctxt, INT32 flags, VOID* v) {
    MyTLS *tls = new MyTLS(nullptr, 0);
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
    MyTLS *tls = static_cast<MyTLS*>(PIN_GetThreadData(tls_key, threadId));
    atomic_fetch_add(&allocTime, tls->_cachedSize);
    // Check for success since we don't want to track null pointers
    //
    if (!Params::enableInstrumentation || (VOID *) retVal == nullptr) { 
        return; 
    }
    manager.InsertObject(retVal, tls->_cachedSize, tls->_cachedBacktrace, threadId);
}

VOID FreeBefore(THREADID threadId, CONTEXT *ctxt, ADDRINT ptr) {
    if (!Params::enableInstrumentation) {
        return;
    }

    Backtrace freeBacktrace;
    unsigned long t;

    freeBacktrace.SetTrace(ctxt);
    t = atomic_load(&allocTime);
    manager.DeleteObject(ptr, freeBacktrace, threadId, t);
}

VOID MemAccess(THREADID threadId, ADDRINT addrAccessed, UINT32 accessSize, const CONTEXT *ctxt) {
    if (!Params::enableInstrumentation) {
        return;
    }

    unsigned long t = atomic_load(&allocTime);
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
}

VOID Fini(INT32 code, VOID *v) {
    // TODO: Don't hardcode this stuff
    Params::traceFile << "{" <<
                         "\"metadata\":" <<
                         "{" <<
                         "\"depth\":3," <<
                         "\"fragsize\":" << sizeof(uintptr_t) <<
                         "}," <<
                         "\"objs\":" << manager <<
                         "}";
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

    atomic_init(&allocTime, 0);
    tls_key = PIN_CreateThreadDataKey(NULL);
    if (tls_key == INVALID_TLS_KEY) {
        std::cerr << "number of already allocated keys reached the MAX_CLIENT_TLS_KEYS limit" << std::endl;
        PIN_ExitProcess(1);
    }

    PIN_InterceptSignal(SIGUSR1, EnableInstrumentation, nullptr);
    PIN_InterceptSignal(SIGUSR2, DisableInstrumentation, nullptr);
    IMG_AddInstrumentFunction(Image, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
	PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
}
