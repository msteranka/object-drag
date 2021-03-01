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

namespace DefaultParams {
    static const std::string defaultIsVerbose = "0",
        defaultTraceFile = "dangling.out";
}

namespace Params {
    static BOOL isVerbose;
    static std::ofstream traceFile;
};

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
    MyTLS *tls = static_cast<MyTLS*>(PIN_GetThreadData(tls_key, threadId));
    tls->_cachedSize = size;
    tls->_cachedBacktrace.SetTrace(ctxt);
}

VOID MallocAfter(THREADID threadId, ADDRINT retVal) {
    // Check for success since we don't want to track null pointers
    //
    if ((VOID *) retVal == nullptr) { 
        return; 
    }

    MyTLS *tls = static_cast<MyTLS*>(PIN_GetThreadData(tls_key, threadId));
    manager.InsertObject(retVal, tls->_cachedSize, tls->_cachedBacktrace, threadId);
}

VOID FreeBefore(THREADID threadId, CONTEXT *ctxt, ADDRINT ptr) {
    MyTLS *tls = static_cast<MyTLS*>(PIN_GetThreadData(tls_key, threadId));
    Backtrace freeBacktrace;
    freeBacktrace.SetTrace(ctxt);
    manager.DeleteObject(ptr, freeBacktrace, threadId);
}

VOID MemAccess(THREADID threadId, ADDRINT addrAccessed, UINT32 accessSize, const CONTEXT *ctxt) {
    MyTLS *tls = static_cast<MyTLS*>(PIN_GetThreadData(tls_key, threadId));
    ObjectData *d = manager.IsUseAfterFree(addrAccessed, accessSize, threadId);
    clock_t t = clock(); // TODO: Use PIN_CallApplicationFunction()?
    // TODO: Add some means of configuring whether you want the invocation
    // point of the last access?
    // std::string accessPath;
    // INT32 accessLine;
    // ADDRINT ip;

    // ip = PIN_GetContextReg(ctxt, REG_INST_PTR);
    // PIN_LockClient();
    // PIN_GetSourceLocation(ip, nullptr, &lineNumber, &fileName);
    // PIN_UnlockClient();

    UpdateLastAccess(addrAccessed, t, threadId);
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

    rtn = RTN_FindByName(img, Params::mallocName.c_str());
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
    } else {
        // std::cerr << "WARNING: Could not intercept " << Params::mallocName.c_str() << std::endl;
    }

    rtn = RTN_FindByName(img, Params::freeName.c_str());
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
    // TODO: Output data - HERE!
    for (auto it = AccessData::count.begin(); it != AccessData::count.end(); it++) {
        Params::traceFile << it->second << " invalid accesses @ <" << it->first << ">" << std::endl;
    }
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

    PIN_InitSymbols();
    if (PIN_Init(argc, argv))  {
        return Usage();
    }

    Params::isVerbose = knobIsVerbose.Value();
    Params::traceFile.open(knobTraceFile.Value().c_str());
    Params::traceFile.setf(ios::showbase);

    PIN_InitLock(&outputLock);
    tls_key = PIN_CreateThreadDataKey(NULL);
    if (tls_key == INVALID_TLS_KEY) {
        std::cerr << "number of already allocated keys reached the MAX_CLIENT_TLS_KEYS limit" << std::endl;
        PIN_ExitProcess(1);
    }

    IMG_AddInstrumentFunction(Image, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
	PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
}
