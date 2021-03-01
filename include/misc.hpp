#if !defined(__MISC_HPP)
# define __MISC_HPP

#include "pin.H"
#include <iostream>
#include <sstream>
#include "objectdata.hpp"

VOID GetSource(std::ostringstream &source, const CONTEXT *ctxt) {
    std::string fileName;
    INT32 lineNumber;
    ADDRINT ip;

    ip = PIN_GetContextReg(ctxt, REG_INST_PTR);
    PIN_LockClient();
    PIN_GetSourceLocation(ip, nullptr, &lineNumber, &fileName);
    PIN_UnlockClient();

    source << fileName << ":" << lineNumber;
}

#endif // __MISC_HPP
