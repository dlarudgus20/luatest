#include <iostream>
#include <algorithm>
#include <list>
#include <chrono>
#include <thread>
#include <cstdint>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

const char* prelude = R"(
function pong(func, callback, ...)
    assert(type(func) == 'function', 'type error :: function is expected')
    local thread = coroutine.create(func)
    local function step(...)
        local status, result = coroutine.resume(thread, ...)
        assert(status, result)
        if coroutine.status(thread) == 'dead' then
            (callback or function () end)(result)
        else
            assert(type(result) == 'function', 'type error :: function is expected')
            result(step)
        end
    end
    step(...)
    return thread
end

function async(func)
    assert(type(func) == 'function', 'type error :: function is expected')
    local function factory(...)
        local args = {...}
        local function thunk(callback)
            pong(func, callback, unpack(args))
        end
        return thunk
    end
    return factory
end

function await(thunk)
    return coroutine.yield(thunk)
end

function join(thunks)
    local len = table.getn(thunks)
    local done = 0
    local acc = {}
    local function thunk(callback)
        if len == 0 then
            return callback()
        end
        for i, tk in ipairs(thunks) do
            assert(type(tk) == 'function', 'type error :: function is expected')
            local function cb(...)
                acc[i] = {...}
                done = done + 1
                if done == len then
                    callback(unpack(acc))
                end
            end
            tk(cb)
        end
    end
    return thunk
end

function sleep(ms)
    local function thunk(callback)
        __sleep(ms, callback)
    end
    return thunk
end
)";

const char* code = R"(
function foo(name, ms, count)
    for i = 1, count do
        print(name .. ': ' .. i)
        await(sleep(ms))
    end
end

await(join{
    async(foo)('x', 1000, 4),
    async(foo)('y', 500, 8)
})
)";

using my_clock = std::chrono::steady_clock;

struct sleep_entry
{
    my_clock::time_point time;
    int ref;
};

std::list<sleep_entry> sleep_queue;

extern "C" int mylua_sleep(lua_State *ls)
{
    auto ms = static_cast<std::chrono::milliseconds::rep>(lua_tonumber(ls, -2));
    auto time = my_clock::now() + std::chrono::milliseconds(ms);

    int ref = luaL_ref(ls, LUA_REGISTRYINDEX);
    lua_pop(ls, 1);

    sleep_queue.push_back({ time, ref });

    return 0;
}

int main(int argc, char** argv)
{
    lua_State* ls = luaL_newstate();
    luaL_openlibs(ls);

    lua_register(ls, "__sleep", mylua_sleep);

    if (luaL_loadstring(ls, prelude) != 0)
    {
        std::cerr << "prelude compile error\n";
        return -1;
    }
    if (lua_pcall(ls, 0, 0, 0) != 0)
    {
        std::cerr << "lua error: " << lua_tostring(ls, -1) << "\n";
        return -1;
    }

    lua_getglobal(ls, "pong");
    if (luaL_loadstring(ls, code) != 0)
    {
        std::cerr << "code compile error\n";
        return -1;
    }
    if (lua_pcall(ls, 1, 1, 0) != 0)
    {
        std::cerr << "lua error: " << lua_tostring(ls, -1) << "\n";
        return -1;
    }

    lua_State* thread = lua_tothread(ls, -1);
    if (!thread)
    {
        std::cerr << "pong() must return thread\n";
        return -1;
    }
    lua_pop(ls, 1);

    while (lua_status(thread) == LUA_YIELD)
    {
        auto now = my_clock::now();
        my_clock::time_point next = now;

        for (auto it = begin(sleep_queue); it != end(sleep_queue); )
        {
            if (it->time <= now)
            {
                int ref = it->ref;

                lua_rawgeti(ls, LUA_REGISTRYINDEX, ref);
                if (lua_pcall(ls, 0, 0, 0) != 0)
                {
                    std::cerr << "lua error: " << lua_tostring(ls, -1) << "\n";
                    lua_pop(ls, 1);
                }

                lua_unref(ls, ref);
                sleep_queue.erase(it++);
            }
            else
            {
                next = std::min(next, it->time);
                ++it;
            }
        }

        std::this_thread::sleep_until(next);
    }

    if (lua_status(thread) != 0)
    {
        std::cerr << "lua error: " << lua_tostring(thread, -1) << "\n";
        lua_pop(ls, -1);
    }

    lua_close(ls);
}
