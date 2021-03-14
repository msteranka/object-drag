#if !defined( __OBJECT_MANAGER_HPP)
# define __OBJECT_MANAGER_HPP

#include "pin.H"
#include "backtrace.hpp"
#include <unordered_map>
#include <vector>
#include <ctime>

using namespace std;

// All of ObjectManager's methods are thread-safe unless specified otherwise
//
class ObjectManager {
    public:
        ObjectManager() {
            PIN_InitLock(&_liveLock);
            PIN_InitLock(&_deadLock);
        }

        VOID InsertObject(ADDRINT ptr, UINT32 size, Backtrace trace, THREADID threadId, size_t fragSize) {
            ObjectData *d;
            ADDRINT nextAddr;

            d = new ObjectData(ptr, size, threadId, trace, fragSize);

            // Create a mapping from every address in this object's range to the same ObjectData
            //
            for (UINT32 i = 0; i < size; i++) {
                nextAddr = (ptr + i);
                PIN_GetLock(&_liveLock, threadId);
                _liveObjects[nextAddr] = d;
                PIN_ReleaseLock(&_liveLock);
            }
        }

        VOID DeleteObject(ADDRINT ptr, Backtrace trace, THREADID threadId, unsigned long time) {
            unordered_map<ADDRINT,ObjectData*>::iterator it;
            ObjectData *d;

            // Determine if this is an invalid/double free, and if it is, then 
            // skip this routine
            //
            PIN_GetLock(&_liveLock, threadId);
            it = _liveObjects.find(ptr);
            if (it == _liveObjects.end()) {
                PIN_ReleaseLock(&_liveLock);
                return;
            }
            PIN_ReleaseLock(&_liveLock);
    
            // Update object metadata, also marking the object as no longer live
            //
            d = it->second;
            d->_freeThread = threadId;
            d->_freeTrace = trace;
            d->_freeTime = time;
            PIN_GetLock(&_deadLock, threadId);
            _deadObjects.push_back(d);
            PIN_ReleaseLock(&_deadLock);

            // Remove all mappings corresponding to this object
            //
            for (ADDRINT i = d->_addr; i < d->_addr + d->_size; i++) {
                PIN_GetLock(&_liveLock, threadId);
                _liveObjects.erase(i);
                PIN_ReleaseLock(&_liveLock);
            }
        }

        VOID UpdateLastAccess(ADDRINT ptr, unsigned long t, THREADID tid, const CONTEXT *ctxt, size_t fragSize) {
            unordered_map<ADDRINT,ObjectData*>::iterator it;
            ObjectData *d;
            ObjectFragment *f;
            ADDRINT ip;

            PIN_GetLock(&_liveLock, tid);
            it = _liveObjects.find(ptr);
            if (it == _liveObjects.end()) {
                PIN_ReleaseLock(&_liveLock);
                return;
            }
            PIN_ReleaseLock(&_liveLock);

            d = it->second;
            f = d->GetFragment(ptr, fragSize);
            // TODO: Add some means of configuring whether you want the invocation
            // point of the last access? - High overhead of calling PIN_GetSourceLocation
            ip = PIN_GetContextReg(ctxt, REG_INST_PTR);
            PIN_LockClient();
            PIN_GetSourceLocation(ip, nullptr, &(f->_accessLine), &(f->_accessPath));
            f->_lastAccess = t;
            f->_wasAccessed = true;
            PIN_UnlockClient();
        }

        vector<ObjectData*> *GetDeadObjects() {
            return &_deadObjects;
        }

    private:
        // TODO: output objects that were never freed?
        unordered_map<ADDRINT,ObjectData*> _liveObjects;
        vector<ObjectData*> _deadObjects;
        PIN_LOCK _liveLock, _deadLock;
};

std::ostream &operator<<(std::ostream &os, ObjectManager &m) {
    vector<ObjectData*> *objs = m.GetDeadObjects();
    vector<ObjectData*>::iterator it;

    os << "[";
    if (objs->size() == 0) {
        os << "]";
        return os;
    }
    for (it = objs->begin(); it != objs->end() - 1; it++) {
        os << **it << ",";
    }
    os << **it << "]";

    return os;
}

#endif
