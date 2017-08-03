/*
** LuaProfiler
** Copyright Kepler Project 2005-2007 (http://www.keplerproject.org/luaprofiler)
** $Id: core_profiler.h,v 1.6 2007-08-22 19:23:53 carregal Exp $
*/

/*****************************************************************************
core_profiler.h:
   Lua version independent profiler interface.
   Responsible for handling the "enter function" and "leave function" events
   and for writing the log file.

Design (using the Lua callhook mechanism) :
   'lprofP_init_core_profiler' set up the profile service
   'lprofP_callhookIN'         called whenever Lua enters a function
   'lprofP_callhookOUT'        called whenever Lua leaves a function
*****************************************************************************/

#include "stack.h"
extern "C"
{
#include "lua.h"
#include "lauxlib.h"
}
#include <map>

typedef std::pair<size_t, size_t> MemorySizeCount;
typedef std::map<int, MemorySizeCount> TypedMemoryInfo;

typedef struct CalleeInfo CalleeInfo;
struct CalleeInfo {
    unsigned StackLevel;
    unsigned Count;
    unsigned LocalStep;
    float TotalTime;
    TypedMemoryInfo MemoryAllocated;
    CalleeInfo()
    {
        StackLevel = 0;
        Count = 0;
        LocalStep = 0;
        TotalTime = 0;
        for (int ObjType = LUA_TNIL; ObjType < LUA_NUMTAGS; ObjType++)
        {
            MemoryAllocated[ObjType] = std::make_pair(0, 0);
        }
    }
};

typedef std::map<std::string, CalleeInfo> CalleeInfoMap;
typedef std::map<std::string, CalleeInfoMap> FuncCalleeInfoMap;
typedef std::map<int, FuncCalleeInfoMap> ThreadFuncCalleeInfoMap;
typedef std::map<lua_State*, lprofP_STATE*> LuaState2ProfilerStateMap;
typedef std::tuple<int, std::string, std::string, int> TupleThreadFuncCallee;
typedef std::map<void*, TupleThreadFuncCallee> MemoryAllocInfoMap;

/* computes new stack and new timer */
void lprofP_callhookIN(lprofP_STATE* S, char *func_name, char *file, int linedefined, int currentline, const char *CallerFile, int IsTailCall, long TotalMemory);

/* pauses all timers to write a log line and computes the new stack */
/* returns if there is another function in the stack */
int  lprofP_callhookOUT(lprofP_STATE* S, ThreadFuncCalleeInfoMap& InfoMap, long TotalMemory);

std::string GetFuncFullName(lprofS_STACK_RECORD *info);

std::string GetCalleePos(lprofS_STACK_RECORD *info);

CalleeInfo& GetCalleeInfo(ThreadFuncCalleeInfoMap& InfoMap, int ThreadIndex, std::string FuncFullName, std::string CalleePos);

int  lprofP_callhookCount(lprofP_STATE* S, int LineCount);

/* opens the log file */
/* returns true if the file could be opened */
lprofP_STATE* lprofP_init_core_profiler(float _function_call_time, int ThreadIndex);

