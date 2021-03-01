#ifndef __OBJECT_MANAGER_HPP
#define __OBJECT_MANAGER_HPP

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

        VOID InsertObject(ADDRINT ptr, UINT32 size, Backtrace trace, THREADID threadId) {
            ObjectData *d;
            ADDRINT nextAddr;

            d = new ObjectData(ptr, size, threadId, trace);

            // Create a mapping from every address in this object's range to the same ObjectData
            //
            for (UINT32 i = 0; i < size; i++) {
                nextAddr = (ptr + i);
                PIN_GetLock(&_liveLock, threadId);
                _liveObjects[nextAddr] = d;
                PIN_ReleaseLock(&_liveLock);
            }
        }

        VOID DeleteObject(ADDRINT ptr, Backtrace trace, THREADID threadId) {
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
            d->_freeTime = clock(); // TODO: Use PIN_CallApplicationFunction()?
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

        VOID UpdateLastAccess(ADDRINT ptr, clock_t t, THREADID tid) {
            unordered_map<ADDRINT,ObjectData*>::iterator it;
            ObjectData *d;

            PIN_GetLock(&_liveObjects, tid);
            it = _liveObjects.find(ptr);
            if (it == _liveObjects.end()) {
                PIN_ReleaseLock(&_liveObjects);
                return;
            }
            PIN_ReleaseLock(&_liveObjects);

            d = it->second;
            d->_lastAccess = t;
        }

    private:
        unordered_map<ADDRINT,ObjectData*> _liveObjects;
        vector<ObjectData*> _deadObjects;
        PIN_LOCK _liveLock, _deadLock;
};

#endif
