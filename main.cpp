#include <iostream>
#include <algorithm>
#include <list>
#include <chrono>
#include <thread>
#include <cstdint>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

using my_clock = std::chrono::steady_clock;

struct sleep_entry
{
    my_clock::time_point time;
    int ref;
};

std::list<sleep_entry> sleep_queue;
std::list<int> atexit_queue;

extern "C" int mylua_atexit(lua_State* ls)
{
    int ref = luaL_ref(ls, LUA_REGISTRYINDEX);
    atexit_queue.push_back(ref);
    return 0;
}

extern "C" int mylua_sleep(lua_State* ls)
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

    lua_register(ls, "__atexit", mylua_atexit);
    lua_register(ls, "__sleep", mylua_sleep);

    if (luaL_loadfile(ls, "prelude.lua") != 0)
    {
        std::cerr << "prelude compile error\n";
        return -1;
    }
    if (lua_pcall(ls, 0, 0, 0) != 0)
    {
        std::cerr << "lua error: " << lua_tostring(ls, -1) << "\n";
        return -1;
    }

    lua_getglobal(ls, "entry");
    if (luaL_loadfile(ls, "code.lua") != 0)
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
        std::cerr << "entry() must return thread\n";
        return -1;
    }
    lua_pop(ls, 1);

    while (lua_status(thread) == LUA_YIELD)
    {
        auto now = my_clock::now();
        my_clock::time_point next = now;

        if (sleep_queue.empty())
        {
            for (int ref : atexit_queue)
            {
                lua_rawgeti(ls, LUA_REGISTRYINDEX, ref);
                if (lua_pcall(ls, 0, 0, 0) != 0)
                {
                    std::cerr << "lua error: " << lua_tostring(ls, -1) << "\n";
                    lua_pop(ls, 1);
                }

                luaL_unref(ls, LUA_REGISTRYINDEX, ref);
            }
            atexit_queue.clear();
        }

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

                luaL_unref(ls, LUA_REGISTRYINDEX, ref);
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
