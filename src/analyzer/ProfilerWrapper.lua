local ProfilerWrapper = {}
local ProfilerInstance
local ProfilerStartTime
local LuaMemoryStart
function ProfilerWrapper:StartProfile()
    if not ProfilerInstance then
        local bSuccess, profiler = pcall(require, "profiler")
        if bSuccess then
            ProfilerInstance = profiler
        end
    end
    if ProfilerInstance then
        local Coroutines = {}
        collectgarbage("collect")
        LuaMemoryStart = collectgarbage("count")
        ProfilerInstance.start(100, table.unpack(Coroutines))
        ProfilerStartTime = os.clock()
    end
end

local function NormalizeString(Str)
    local ReturnCatch = string.match(Str, "return [%w_]+")
    return ReturnCatch or Str
end

function ProfilerWrapper:StopProfile()
    if ProfilerInstance then
        collectgarbage("collect")
        local MemoryDelta = collectgarbage("count") - LuaMemoryStart

        local Coroutines = {}
        local Info = ProfilerInstance.stop(table.unpack(Coroutines))
        if Info then
            local ProfileTotalTime = (os.clock() - ProfilerStartTime)*1000
            local LuaTotalTime = 0
            for _, CalleeInfo in pairs(Info[0]) do
                for _, Info in pairs(CalleeInfo) do
                    if Info.StackLevel == 0 then
                        LuaTotalTime = LuaTotalTime + Info.TotalTime
                    end
                end
            end
            local Logs = {}
            table.insert(Logs, string.format("ProfileTime,%f,LuaTotalTime, %f, %%LuaTotalTime, %f, TotalMemoryDelta, %f", ProfileTotalTime, LuaTotalTime, 100 * LuaTotalTime/ProfileTotalTime, MemoryDelta))
            table.insert(Logs, "StackLevel, ThreadIndex, FuncName,CalleePos,CalledCount, LocalStep, AvgLocalStep, TotalTime, AvgTotalTime, MaxTotalTime, MemoryAllocated, MemoryAllocatedMax, UserdataAllocated, TableAllocated")
            for ThreadIndex, ThreadInfo in pairs(Info) do
                for FuncName, CalleeInfo in pairs(ThreadInfo) do
                    for Callee, Info in pairs(CalleeInfo) do
                        local TotalMemorySize, TotalMemorySizeMax = 0, 0
                        for _, MemorySizeInfo in pairs(Info.MemoryAllocated) do
                            TotalMemorySize = TotalMemorySize + MemorySizeInfo.MemorySize
                            TotalMemorySizeMax = TotalMemorySizeMax + MemorySizeInfo.MemorySizeMax
                        end

                        local Log = string.format("%d,%d,%s,%s,%d,%d,%f,%f,%f,%f, %f, %f, %f, %f", Info.StackLevel, ThreadIndex, NormalizeString(FuncName), NormalizeString(Callee), Info.Count, Info.LocalStep, Info.LocalStep/Info.Count, Info.TotalTime, Info.TotalTime/Info.Count, Info.MaxTotalTime, TotalMemorySize, TotalMemorySizeMax, Info.MemoryAllocated.LUA_TUSERDATA.MemorySize, Info.MemoryAllocated.LUA_TTABLE.MemorySize)
                        table.insert(Logs, Log)
                    end
                end
            end
            local FileHandle = assert(io.open("Profiler.csv", 'wb'))
            FileHandle:write(table.concat(Logs, "\n"))
            FileHandle:close()
        end
    end
end
return ProfilerWrapper