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

#include "clocks.h"
#include "core_profiler.h"
#include "function_meter.h"


/* Indices for the main profiler stack and for the original exit function */
static int exit_id;
static int profstate_id;

static int StatisticsID;
static int CalleeMetatableID;
static int LineCount = 1;

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
    lua_State* MainThread = GetMainThread(L);
    lua_pushlightuserdata(MainThread, &profstate_id);
    lua_gettable(MainThread, LUA_REGISTRYINDEX);
    if (!lua_istable(MainThread, -1))
    {
        lua_pop(MainThread, 1);
        lua_pushlightuserdata(MainThread, &profstate_id);
        lua_newtable(MainThread);
        lua_settable(MainThread, LUA_REGISTRYINDEX);
        lua_pushlightuserdata(MainThread, &profstate_id);
        lua_gettable(MainThread, LUA_REGISTRYINDEX);
    }

    int StatesTableIndex = -1;
    lua_pushlightuserdata(MainThread, L);
    StatesTableIndex--;
    lua_pushlightuserdata(MainThread, ProfileState);
    StatesTableIndex--;
    lua_settable(MainThread, StatesTableIndex);
    StatesTableIndex += 2;

    lua_pop(MainThread, -1 * StatesTableIndex);
}

lprofP_STATE* GetProfileState(lua_State *L)
{
    lprofP_STATE* S = NULL;
    lua_State* MainThread = GetMainThread(L);
    lua_pushlightuserdata(MainThread, &profstate_id);
    lua_gettable(MainThread, LUA_REGISTRYINDEX);
    int ProfileStateIndex = -1;
    lua_rawgetp(MainThread, ProfileStateIndex, L);
    ProfileStateIndex--;
    if (lua_isuserdata(MainThread, -1))
    {
        S = (lprofP_STATE*)lua_touserdata(MainThread, -1);
    }
    lua_pop(MainThread, -1 * ProfileStateIndex);
    return S;
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

  if (lua_getstack(L, 1, &previous_ar) == 0) {
    currentline = -1;
  } else {
    lua_getinfo(L, "Sl", &previous_ar);
    currentline = previous_ar.currentline;
    if (previous_ar.source != NULL)
    {
        CallerSource = previous_ar.source;
    }
  }
      
  lua_getinfo(L, "nS", ar);

  switch (ar->event)
  {
  case LUA_HOOKCALL:
  case LUA_HOOKTAILCALL:
    /* entering a function */
    lprofP_callhookIN(S, (char *)ar->name,
		      (char *)ar->source, ar->linedefined,
		      currentline, CallerSource);
    break;
  case LUA_HOOKRET:
    lprofP_callhookOUT(S, GetMainThread(L), &StatisticsID, &CalleeMetatableID);
    break;
  case LUA_HOOKCOUNT:
      lprofP_callhookCount(S, LineCount);
      break;
  }
}

static void ClearCachedData(lua_State *L)
{
    lua_pushlightuserdata(L, &StatisticsID);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L, &CalleeMetatableID);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);
}

static void DeleteProfileStates(lua_State *L)
{
    lprofP_STATE* S;
    if (GetMainThread(L) != L)
    {
        luaL_error(L, "Can't stop profiler from coroutines");
    }

    lua_pushlightuserdata(L, &profstate_id);
    lua_gettable(L, LUA_REGISTRYINDEX);
    int StatesTableIndex = -1;
    if (lua_istable(L, -1))
    {
        lua_pushnil(L);
        StatesTableIndex--;

        while (lua_next(L, StatesTableIndex) != 0) {
            StatesTableIndex--;

            S = (lprofP_STATE*)lua_touserdata(L, -1);
            lua_pop(L, 1);
            StatesTableIndex++;

            if (S)
            {
                free(S);
            }
        }
    }
    lua_pop(L, -1 * StatesTableIndex);

    lua_pushlightuserdata(L, &profstate_id);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);
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
  S = lprofM_init();
  AddProfileState(NL, S);
  lua_sethook(NL, (lua_Hook)callhook, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, LineCount);
  return 1;	
}
#endif

static int profiler_GetInfo(lua_State *L)
{
    lua_pushlightuserdata(L, &StatisticsID);
    lua_gettable(L, LUA_REGISTRYINDEX);
    return 1;
}

static int IndexCallee(lua_State *L)
{
    if (lua_isuserdata(L, 1) && lua_isstring(L, 2))
    {
        const char* Name = lua_tostring(L, 2);
        CalleeInfo* Data = (CalleeInfo*)lua_touserdata(L, 1);
        if (strcmp(Name, "Count") == 0)
        {
            lua_pushnumber(L, Data->Count);
            return 1;
        }
        else if(strcmp(Name, "LocalStep") == 0)
        {
            lua_pushnumber(L, Data->LocalStep);
            return 1;
        }
        else if (strcmp(Name, "TotalStep") == 0)
        {
            lua_pushnumber(L, Data->TotalStep);
            return 1;
        }
        else if (strcmp(Name, "TotalTime") == 0)
        {
            lua_pushnumber(L, Data->TotalTime);
            return 1;
        }
    }
    return 0;
}

static const luaL_Reg CalleeFuncs[] = {
    { "__index", IndexCallee },
    { NULL, NULL }
};

static int IsRunning(lua_State *L)
{
    lua_State* MainThread = GetMainThread(L);
    if (L != MainThread)
    {
        luaL_error(L, "Can't start profiler from coroutines");
    }
    lua_pushlightuserdata(MainThread, &profstate_id);
    lua_gettable(MainThread, LUA_REGISTRYINDEX);
    int result = !lua_isnil(MainThread, -1);
    lua_pop(MainThread, 1);
    return result;
}

static int profiler_init(lua_State *L) {
  lprofP_STATE* S;
  float function_call_time;

  if(IsRunning(L)) {
    profiler_stop(L);
  }

  function_call_time = calcCallTime(L);

  /* init with default file name and printing a header line */
  if (!(S=lprofP_init_core_profiler(function_call_time))) {
    return luaL_error(L,"LuaProfiler error: output file could not be opened!");
  }

  if (lua_isnumber(L, 1))
  {
      LineCount = lua_tonumber(L, 1);
  }

  lua_sethook(L, (lua_Hook)callhook, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, LineCount);

  AddProfileState(L, S);

  int n = lua_gettop(L);
  for (int index = 2; index <= n; index++)
  {
      if (!lua_isthread(L, index))
      {
          luaL_error(L, "Only support coroutine param");
      }
      lua_State* Coro = lua_tothread(L, index);
      lprofP_STATE* CoroState = lprofM_init();
      lua_sethook(Coro, (lua_Hook)callhook, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, LineCount);
      AddProfileState(Coro, CoroState);
  }

  lua_pushlightuserdata(L, &StatisticsID);
  lua_newtable(L);
  lua_settable(L, LUA_REGISTRYINDEX);

  lua_pushlightuserdata(L, &CalleeMetatableID);
  luaL_newlib(L, CalleeFuncs);
  lua_settable(L, LUA_REGISTRYINDEX);
	
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
  lua_pushstring(L, "create");
  lua_pushcfunction(L, (lua_CFunction)coroutine_create);
  lua_settable(L, -3);
#endif

  /* the following statement is to simulate how the execution stack is */
  /* supposed to be by the time the profiler is activated when loaded  */
  /* as a library.                                                     */

  lprofP_callhookIN(S, "profiler_init", "(C)", -1, -1, "");
	
  lua_pushboolean(L, 1);
  return 1;
}

extern void lprofP_close_core_profiler(lprofP_STATE* S);

static int profiler_stop(lua_State *L) {
  lua_sethook(L, (lua_Hook)callhook, 0, 0);
  DeleteProfileStates(L);
  ClearCachedData(L);
  return 0;
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
  return lprofC_get_seconds(timer) / (float) 100000;
}

static const luaL_Reg prof_funcs[] = {
  { "start", profiler_init },
  { "GetInfo", profiler_GetInfo },
  { "stop", profiler_stop },
  { NULL, NULL }
};

int luaopen_profiler(lua_State *L) {
#if LUA_VERSION_NUM > 501 && !defined(LUA_COMPAT_MODULE)
  lua_newtable(L);
  luaL_setfuncs(L, prof_funcs, 0);
#else
  luaL_openlib(L, "profiler", prof_funcs, 0);
#endif
  lua_pushliteral (L, "_COPYRIGHT");
  lua_pushliteral (L, "Copyright (C) 2003-2007 Kepler Project");
  lua_settable (L, -3);
  lua_pushliteral (L, "_DESCRIPTION");
  lua_pushliteral (L, "LuaProfiler is a time profiler designed to help finding bottlenecks in your Lua program.");
  lua_settable (L, -3);
  lua_pushliteral (L, "_NAME");
  lua_pushliteral (L, "LuaProfiler");
  lua_settable (L, -3);
  lua_pushliteral (L, "_VERSION");
  lua_pushliteral (L, "2.0.1");
  lua_settable (L, -3);
  return 1;
}
