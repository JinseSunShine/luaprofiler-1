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

#include "lua.h"
#include "lauxlib.h"

typedef struct CalleeInfo CalleeInfo;
struct CalleeInfo {
    size_t Count;
    size_t LocalStep;
    size_t TotalStep;
    float TotalTime;    
};

/* computes new stack and new timer */
void lprofP_callhookIN(lprofP_STATE* S, char *func_name, char *file, int linedefined, int currentline, const char *CallerFile);

/* pauses all timers to write a log line and computes the new stack */
/* returns if there is another function in the stack */
int  lprofP_callhookOUT(lprofP_STATE* S, lua_State *L, int* StatisticsIDPtr, int* CalleeMetaPtr);

int  lprofP_callhookCount(lprofP_STATE* S, int LineCount);

/* opens the log file */
/* returns true if the file could be opened */
lprofP_STATE* lprofP_init_core_profiler(float _function_call_time);

