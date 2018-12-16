/*
** LuaProfiler
** Copyright Kepler Project 2005.2007 (http://www.keplerproject.org/luaprofiler)
** $Id: core_profiler.c,v 1.10 2009-01-29 12:39:28 jasonsantos Exp $
*/

/*****************************************************************************
core_profiler.c:
   Lua version independent profiler interface.
   Responsible for handling the "enter function" and "leave function" events
   and for writing the log file.

Design (using the Lua callhook mechanism) :
   'lprofP_init_core_profiler' set up the profile service
   'lprofP_callhookIN'         called whenever Lua enters a function
   'lprofP_callhookOUT'        called whenever Lua leaves a function
*****************************************************************************/

/*****************************************************************************
   The profiled program can be viewed as a graph with the following properties:
directed, multigraph, cyclic and connected. The log file generated by a
profiler section corresponds to a path on this graph.
   There are several graphs for which this path fits on. Some times it is
easier to consider this path as being generated by a simpler graph without
properties like cyclic and multigraph.
   The profiler log file can be viewed as a "reversed" depth-first search
(with the depth-first search number for each vertex) vertex listing of a graph
with the following properties: simple, acyclic, directed and connected, for
which each vertex appears as many times as needed to strip the cycles and
each vertex has an indegree of 1.
   "reversed" depth-first search means that instead of being "printed" before
visiting the vertex's descendents (as done in a normal depth-first search),
the vertex is "printed" only after all his descendents have been processed (in
a depth-first search recursive algorithm).
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <string>

#include "function_meter.h"

#include "core_profiler.h"

/* default log name (%s is used to place a random string) */
#define OUT_FILENAME "lprof_%s.out"

#define MAX_FUNCTION_NAME_LENGTH 200

    /* for faster execution (??) */
static FILE *outf;
static lprofS_STACK_RECORD *info;
static float function_call_time;


/* output a line to the log file, using 'printf()' syntax */
/* assume the timer is off */
static void output(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(outf, format, ap);
    va_end(ap);

    /* write now to avoid delays when the timer is on */
    fflush(outf);
}


/* do not allow a string with '\n' and '|' (log file format reserved chars) */
/* - replace them by ' '                                                    */
static void formats(char *s) {
    if (!s)
        return;
    size_t StrLen = strlen(s);
    for (size_t i = 0; i < StrLen; i++) {
        if ((s[i] == '|') || (s[i] == '\n'))
            s[i] = ' ';
    }
}


/* computes new stack and new timer */
void lprofP_callhookIN(lprofP_STATE* S, char *func_name, char *file, int linedefined, int currentline, const char *CallerFile, int IsTailCall, long TotalMemory) {
    S->stack_level++;
    lprofM_enter_function(S, file, func_name, linedefined, currentline, CallerFile, IsTailCall, TotalMemory);
}

int lprofP_callhookCount(lprofP_STATE* S, int LineCount) {

    if (S->stack_top) {
        S->stack_top->local_step += LineCount;
    }
    return 0;
}

std::string GetFuncFullName(lprofS_STACK_RECORD *info)
{
    char* FuncSource = info->file_defined;
    formats(FuncSource);
    char* FuncName = info->function_name;
    std::string FuncFullName;
    FuncFullName.append(FuncName);
    FuncFullName.append(":");
    FuncFullName.append(FuncSource);
    FuncFullName.append(":");
    FuncFullName.append(std::to_string(info->line_defined));
    return FuncFullName;
}

std::string GetCalleePos(lprofS_STACK_RECORD *info)
{
    std::string CalleePos;
    CalleePos.append(info->CallerSource);
    CalleePos.append(":");
    CalleePos.append(std::to_string(info->current_line));
    return CalleePos;
}

CalleeInfo& GetCalleeInfo(ThreadFuncCalleeInfoMap& InfoMap, int ThreadIndex, std::string FuncFullName, std::string CalleePos)
{
    auto ThreadIndexIter = InfoMap.find(ThreadIndex);
    if (ThreadIndexIter == InfoMap.end())
    {
        InfoMap[ThreadIndex] = FuncCalleeInfoMap();
    }
    FuncCalleeInfoMap& Func2CalleeInfo = InfoMap[ThreadIndex];

    auto CalleeInfoMapIter = Func2CalleeInfo.find(FuncFullName);
    if (CalleeInfoMapIter == Func2CalleeInfo.end())
    {
        Func2CalleeInfo[FuncFullName] = CalleeInfoMap();
    }
    CalleeInfoMap& Callee2Info = Func2CalleeInfo[FuncFullName];

    auto InfoIter = Callee2Info.find(CalleePos);
    if (InfoIter == Callee2Info.end())
    {
        Callee2Info[CalleePos] = CalleeInfo();
    }
    return Callee2Info[CalleePos];
}

/* pauses all timers to write a log line and computes the new stack */
/* returns if there is another function in the stack */
int lprofP_callhookOUT(lprofP_STATE* S, ThreadFuncCalleeInfoMap& InfoMap, long TotalMemory) {

    do
    {
        if (S->stack_level == 0) {
            return 0;
        }
        S->stack_level--;

        /* 0: do not resume the parent function's timer yet... */
        info = lprofM_leave_function(S, 0, TotalMemory);
        /* writing a log may take too long to be computed with the function's time ...*/
        lprofM_pause_total_time(S);

        info->local_time += function_call_time;
        info->total_time += function_call_time;

        std::string FuncFullName = GetFuncFullName(info);

        std::string CalleePos = GetCalleePos(info);

        CalleeInfo& CallInfo = GetCalleeInfo(InfoMap, S->ThreadIndex, FuncFullName, CalleePos);
        CallInfo.StackLevel = S->stack_level;
        CallInfo.Count++;
        CallInfo.LocalStep += info->local_step;
        CallInfo.TotalTime += info->total_time;
        if (info->total_time > CallInfo.MaxTotalTime)
        {
            CallInfo.MaxTotalTime = info->total_time;
        }

    } while (info->IsTailCall);


    /* ... now it's ok to resume the timer */
    if (S->stack_level != 0) {
        lprofM_resume_function(S);
    }

    return 1;

}


/* opens the log file */
/* returns true if the file could be opened */
lprofP_STATE* lprofP_init_core_profiler(float _function_call_time, int ThreadIndex) {
    lprofP_STATE* S;

    function_call_time = _function_call_time;

    /* initialize the 'function_meter' */
    S = lprofM_init(ThreadIndex);
    if (!S) {
        return 0;
    }

    return S;
}

void lprofP_close_core_profiler(lprofP_STATE* S) {
    if (S) free(S);
}

lprofP_STATE* lprofP_create_profiler(float _function_call_time, int ThreadIndex) {
    lprofP_STATE* S;

    function_call_time = _function_call_time;

    /* initialize the 'function_meter' */
    S = lprofM_init(ThreadIndex);
    if (!S) {
        return 0;
    }

    return S;
}