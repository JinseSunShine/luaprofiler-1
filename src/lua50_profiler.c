/*
** LuaProfiler
** Copyright Kepler Project 2005-2007 (http://www.keplerproject.org/luaprofiler)
** $Id: lua50_profiler.c,v 1.16 2008-05-20 21:16:36 mascarenhas Exp $
*/

/*****************************************************************************
lua50_profiler.c:
   Lua version dependent profiler interface
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stack>

#include "clocks.h"
#include "core_profiler.h"
#include "function_meter.h"


/* Indices for the main profiler stack and for the original exit function */
static int exit_id;
static int CoroutineCreateID;
static bool IsRunningProfiler = false;

ThreadFuncCalleeInfoMap ProfilerInfoMap;
LuaState2ProfilerStateMap LuaState2ProfilerState;

lua_Alloc DefaultAllocFunc = NULL;
void*   DefaultAllocUserData = NULL;
lprofP_STATE* CurThreadState = NULL;

static int LineCount = 1;
static int ThreadCount = 0;

/* Forward declaration */
static float calcCallTime(lua_State *L);
static int profiler_stop(lua_State *L);

static lua_State* GetMainThread(lua_State *L)
{
    if (lua_isyieldable(L))
    {
        lua_State* CoroutineState = L;
        lua_rawgeti(CoroutineState, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
        L = lua_tothread(CoroutineState, -1);
        lua_pop(CoroutineState, 1);
    }
    return L;
}

static void AddProfileState(lua_State *L, lprofP_STATE* ProfileState)
{
    LuaState2ProfilerState[L] = ProfileState;
}

lprofP_STATE* GetProfileState(lua_State *L)
{
    auto ResultIter = LuaState2ProfilerState.find(L);
    if (ResultIter != LuaState2ProfilerState.end()) {
        return ResultIter->second;
    }
    return NULL;
}

void * LuaAllocWrapper (void *ud, void *ptr, size_t osize, size_t nsize)
{
    if (CurThreadState && CurThreadState->stack_top)
    {
        if (ptr == NULL && osize >= LUA_TNIL && osize < LUA_NUMTAGS)
        {
            CurThreadState->stack_top->MemoryAllocated += nsize;
        }
    }
    return DefaultAllocFunc(ud, ptr, osize, nsize);
}

long GetCurTotalMemory(lua_State *L)
{
    const long Kilo = 1024;
    const int NoUseArg = 0;
    return lua_gc(L, LUA_GCCOUNT, NoUseArg) * Kilo + lua_gc(L, LUA_GCCOUNTB, NoUseArg);
}

/* called by Lua (via the callhook mechanism) */
static void callhook(lua_State *L, lua_Debug *ar) {
    int currentline;
    const char* CallerSource = "";
    lua_Debug previous_ar;
    lprofP_STATE* S = GetProfileState(L);
    if (S == NULL)
    {
        return;
    }
    CurThreadState = S;

    if (lua_getstack(L, 1, &previous_ar) == 0) {
        currentline = -1;
    }
    else {
        lua_getinfo(L, "Sl", &previous_ar);
        currentline = previous_ar.currentline;
        if (previous_ar.source != NULL)
        {
            CallerSource = previous_ar.source;
        }
    }

    lua_getinfo(L, "nS", ar);
    long TotalMemory = GetCurTotalMemory(L);
    switch (ar->event)
    {
    case LUA_HOOKCALL:
        /* entering a function */
        lprofP_callhookIN(S, (char *)ar->name,
            (char *)ar->source, ar->linedefined,
            currentline, CallerSource, 0, TotalMemory);
        break;
    case LUA_HOOKTAILCALL:
        lprofP_callhookIN(S, (char *)ar->name,
            (char *)ar->source, ar->linedefined,
            currentline, CallerSource, 1, TotalMemory);
        break;
    case LUA_HOOKRET:
        lprofP_callhookOUT(S, ProfilerInfoMap, TotalMemory);
        //if (S->stack_level == 0)
        //{
        //    CurThreadState = NULL;
        //}
        break;
    case LUA_HOOKCOUNT:
        lprofP_callhookCount(S, LineCount);
        break;
    }
}

void ForceCompleteProfileStates(lprofP_STATE* S, lua_State *MainThread)
{
    while (S->stack_level > 0)
    {
        lprofP_callhookOUT(S, ProfilerInfoMap, GetCurTotalMemory(MainThread));
    }
}

static void DeleteProfileStates(lua_State *L)
{
    for (auto it = LuaState2ProfilerState.begin(); it != LuaState2ProfilerState.end(); ++it) {
        lprofP_STATE* S = it->second;
        if (S)
        {
            ForceCompleteProfileStates(S, L);
            free(S);
        }
    }
    LuaState2ProfilerState.clear();
}

/* Lua function to exit politely the profiler                               */
/* redefines the lua exit() function to not break the log file integrity    */
/* The log file is assumed to be valid if the last entry has a stack level  */
/* of 1 (meaning that the function 'main' has been exited)                  */
static void exit_profiler(lua_State *L) {
    profiler_stop(L);

    /* call the original Lua 'exit' function */
    lua_pushlightuserdata(L, &exit_id);
    lua_gettable(L, LUA_REGISTRYINDEX);
    lua_call(L, 0, 0);
}

/* Our new coroutine.create function  */
/* Creates a new profile state for the coroutine */
#if 1
static int coroutine_create(lua_State *L) {
    lprofP_STATE* S;
    lua_State *NL = lua_newthread(L);
    luaL_argcheck(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1), 1,
        "Lua function expected");
    lua_insert(L, 1);  /* move function to top */
    lua_xmove(L, NL, 1);  /* move function from L to NL */
    /* Inits profiler and sets profiler hook for this coroutine */
    S = lprofM_init(ThreadCount++);
    AddProfileState(NL, S);
    lua_sethook(NL, (lua_Hook)callhook, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, LineCount);
    return 1;
}
#endif

static int profiler_init(lua_State *L) {
    lprofP_STATE* S;
    float function_call_time;

    if (IsRunningProfiler) {
        profiler_stop(L);
    }

    DefaultAllocFunc = lua_getallocf(L, &DefaultAllocUserData);
    lua_setallocf(L, LuaAllocWrapper, DefaultAllocUserData);

    function_call_time = calcCallTime(L);

    /* init with default file name and printing a header line */
    if (!(S = lprofP_init_core_profiler(function_call_time, ThreadCount++))) {
        return luaL_error(L, "LuaProfiler error: Cant init profiler!");
    }

    if (!lua_isnumber(L, 1))
    {
        luaL_error(L, "first param should be LineCount, not '%s'", lua_tostring(L, 1));
    }
    LineCount = lua_tonumber(L, 1);

    lua_sethook(L, (lua_Hook)callhook, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, LineCount);

    AddProfileState(L, S);

    int n = lua_gettop(L);
    for (int index = 2; index <= n; index++)
    {
        if (!lua_isthread(L, index))
        {
            luaL_error(L, "\'%s\' is not coroutine", lua_tostring(L, index));
        }
        lua_State* Coro = lua_tothread(L, index);
        lprofP_STATE* CoroState = lprofM_init(ThreadCount++);
        lua_sethook(Coro, (lua_Hook)callhook, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, LineCount);
        AddProfileState(Coro, CoroState);
        if (lua_status(Coro) == LUA_YIELD)
        {
            lua_Debug AR;
            std::stack<lua_Debug> DebugStack;
            int StackLevel = 0;
            while (lua_getstack(Coro, StackLevel++, &AR))
            {
                DebugStack.push(AR);
            }

            const char* CallerFile = "";
            while (!DebugStack.empty())
            {
                lua_Debug CurAR = DebugStack.top();
                DebugStack.pop();
                lua_getinfo(L, "nSl", &CurAR);
                lprofP_callhookIN(CoroState, (char *)CurAR.name,
                    (char *)CurAR.source, CurAR.linedefined,
                    CurAR.currentline, CallerFile, 0, 0);
                CallerFile = CurAR.source;
            }
        }
    }

    /* use our own exit function instead */
    lua_getglobal(L, "os");
    lua_pushlightuserdata(L, &exit_id);
    lua_pushstring(L, "exit");
    lua_gettable(L, -3);
    lua_settable(L, LUA_REGISTRYINDEX);
    lua_pushstring(L, "exit");
    lua_pushcfunction(L, (lua_CFunction)exit_profiler);
    lua_settable(L, -3);

#if 1
    /* use our own coroutine.create function instead */
    lua_getglobal(L, "coroutine");
    lua_pushlightuserdata(L, &CoroutineCreateID);
    lua_pushstring(L, "create");
    lua_gettable(L, -3);
    lua_settable(L, LUA_REGISTRYINDEX);

    lua_pushstring(L, "create");
    lua_pushcfunction(L, (lua_CFunction)coroutine_create);
    lua_settable(L, -3);
    lua_pop(L, 1);
#endif

    /* the following statement is to simulate how the execution stack is */
    /* supposed to be by the time the profiler is activated when loaded  */
    /* as a library.                                                     */
    IsRunningProfiler = true;

    lprofP_callhookIN(S, "profiler_init", "(C)", -1, -1, "", 0, GetCurTotalMemory(L));
    lua_pushboolean(L, 1);
    return 1;
}

extern void lprofP_close_core_profiler(lprofP_STATE* S);

static int profiler_stop(lua_State *L) {
    lua_gc(L, LUA_GCCOLLECT, 0);
    lua_sethook(L, (lua_Hook)callhook, 0, 0);
    int n = lua_gettop(L);
    for (int index = 1; index <= n; index++)
    {
        if (!lua_isthread(L, index))
        {
            luaL_error(L, "Only support coroutine param");
        }
        lua_State* Coro = lua_tothread(L, index);
        lua_sethook(Coro, (lua_Hook)callhook, 0, 0);
    }

    lua_setallocf(L, DefaultAllocFunc, DefaultAllocUserData);
    DefaultAllocFunc = NULL;
    DefaultAllocUserData = NULL;

    DeleteProfileStates(L);

    lua_getglobal(L, "coroutine");
    lua_pushstring(L, "create");
    lua_pushlightuserdata(L, &CoroutineCreateID);
    lua_gettable(L, LUA_REGISTRYINDEX);
    lua_settable(L, -3);
    lua_pop(L, 1);

    lua_pushlightuserdata(L, &CoroutineCreateID);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);

    lua_newtable(L);
    for (auto Iter = ProfilerInfoMap.begin(); Iter != ProfilerInfoMap.end(); Iter++)
    {
        lua_pushnumber(L, Iter->first);
        lua_newtable(L);
        for (auto FuncCalleeInfoIter : Iter->second)
        {
            lua_pushstring(L, FuncCalleeInfoIter.first.c_str());
            lua_newtable(L);
            for (auto CalleeInfoIter : FuncCalleeInfoIter.second)
            {
                lua_pushstring(L, CalleeInfoIter.first.c_str());
                lua_newtable(L);
                {
                    lua_pushstring(L, "Count");
                    lua_pushnumber(L, CalleeInfoIter.second.Count);
                    lua_settable(L, -3);

                    lua_pushstring(L, "LocalStep");
                    lua_pushnumber(L, CalleeInfoIter.second.LocalStep);
                    lua_settable(L, -3);

                    lua_pushstring(L, "StackLevel");
                    lua_pushnumber(L, CalleeInfoIter.second.StackLevel);
                    lua_settable(L, -3);

                    lua_pushstring(L, "TotalTime");
                    lua_pushnumber(L, CalleeInfoIter.second.TotalTime);
                    lua_settable(L, -3);

                    lua_pushstring(L, "MemoryAllocated");
                    lua_pushnumber(L, CalleeInfoIter.second.MemoryAllocated);
                    lua_settable(L, -3);
                }

                lua_settable(L, -3);
            }
            lua_settable(L, -3);
        }
        lua_settable(L, -3);
    }

    ThreadCount = 0;
    IsRunningProfiler = false;
    ProfilerInfoMap.clear();
    LuaState2ProfilerState.clear();
    return 1;
}

/* calculates the approximate time Lua takes to call a function */
static float calcCallTime(lua_State *L) {
    clock_t timer;
    char lua_code[] = "                                     \
                   local function lprofT_mesure_function()    \
                   local i                              \
                                                        \
                      local t = function()              \
                      end                               \
                                                        \
                      i = 1                             \
                      while (i < 100000) do             \
                         t()                            \
                         i = i + 1                      \
                      end                               \
                   end                                  \
                                                        \
                   lprofT_mesure_function()             \
                   lprofT_mesure_function = nil         \
                 ";

    lprofC_start_timer(&timer);
    luaL_dostring(L, lua_code);
    return lprofC_get_milliseconds(timer) / (float)100000;
}

static const luaL_Reg prof_funcs[] = {
  { "start", profiler_init },
  { "stop", profiler_stop },
  { NULL, NULL }
};

extern "C"
{

    int luaopen_profiler(lua_State *L) {
#if LUA_VERSION_NUM > 501 && !defined(LUA_COMPAT_MODULE)
        lua_newtable(L);
        luaL_setfuncs(L, prof_funcs, 0);
#else
        luaL_openlib(L, "profiler", prof_funcs, 0);
#endif
        lua_pushliteral(L, "_COPYRIGHT");
        lua_pushliteral(L, "Copyright (C) 2003-2007 Kepler Project");
        lua_settable(L, -3);
        lua_pushliteral(L, "_DESCRIPTION");
        lua_pushliteral(L, "LuaProfiler is a time profiler designed to help finding bottlenecks in your Lua program.");
        lua_settable(L, -3);
        lua_pushliteral(L, "_NAME");
        lua_pushliteral(L, "LuaProfiler");
        lua_settable(L, -3);
        lua_pushliteral(L, "_VERSION");
        lua_pushliteral(L, "2.0.1");
        lua_settable(L, -3);
        return 1;
    }

}