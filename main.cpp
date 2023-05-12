#include <iostream>
#include <algorithm>
#include <functional>
#include <utility>
#include <list>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <concepts>
#include <cstdint>
#include <cstring>

#include <fmt/format.h>

#include <glm/glm.hpp>

#include <SDL2/SDL.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

int handleLuaError(lua_State* ls)
{
    luaL_dostring(ls, "debug.traceback");
    lua_call(ls, 1, 1);
    return 1;
}

class mylua_ref final
{
public:
    mylua_ref() = default;
    explicit mylua_ref(lua_State* ls);
    ~mylua_ref();

    void swap(mylua_ref& other) noexcept;
    mylua_ref(mylua_ref&& other) noexcept;
    mylua_ref& operator =(mylua_ref other) noexcept;

    bool isnil() const { return ref == LUA_NOREF || ref == LUA_REFNIL; }
    lua_State* state() const { return ls; }

    explicit operator bool() const { return !isnil(); }

    void push() const;
    void set() const;

private:
    lua_State* ls = nullptr;
    int ref = LUA_NOREF;
};

mylua_ref::mylua_ref(lua_State* ls)
{
    this->ls = ls;
    ref = luaL_ref(ls, LUA_REGISTRYINDEX);
}

mylua_ref::~mylua_ref()
{
    luaL_unref(ls, LUA_REGISTRYINDEX, ref);
}

void mylua_ref::swap(mylua_ref& other) noexcept
{
    using std::swap;
    swap(ls, other.ls);
    swap(ref, other.ref);
}

mylua_ref::mylua_ref(mylua_ref&& other) noexcept
{
    swap(other);
}

mylua_ref& mylua_ref::operator =(mylua_ref other) noexcept
{
    swap(other);
    return *this;
}

void mylua_ref::push() const
{
    lua_rawgeti(ls, LUA_REGISTRYINDEX, ref);
}

void mylua_ref::set() const
{
    lua_rawseti(ls, LUA_REGISTRYINDEX, ref);
}

using my_clock = std::chrono::steady_clock;

struct sleep_entry
{
    my_clock::time_point time;
    int ref;
};

class mylua final
{
public:
    mylua();
    ~mylua();

    lua_State* state() const { return ls; }

    void register_closure(const char* name, lua_CFunction fn, void* self);
    void register_ludata(const char* name, void* data);

    void prelude();
    mylua_ref start_sync();

    void start_async();
    bool run_async();
    void run_atexit();

    mylua_ref global(const char* name);

private:
    lua_State* ls;
    lua_State* thread;

    std::list<sleep_entry> sleep_queue;
    std::list<int> atexit_queue;

    friend int mylua_atexit(lua_State* ls);
    friend int mylua_sleep(lua_State* ls);

    mylua(const mylua&) = delete;
    mylua& operator =(const mylua&) = delete;
};

template <typename T>
T closure_upvalue(lua_State* ls, int index)
{
    return static_cast<T>(lua_touserdata(ls, lua_upvalueindex(index)));
}

mylua::mylua()
{
    ls = luaL_newstate();
    luaL_openlibs(ls);

    register_closure("__atexit", [](lua_State* ls) {
        auto self = closure_upvalue<mylua*>(ls, 1);
        int ref = luaL_ref(ls, LUA_REGISTRYINDEX);
        self->atexit_queue.push_back(ref);
        return 0;
    }, this);

    register_closure("__sleep", [](lua_State* ls) {
        auto self = closure_upvalue<mylua*>(ls, 1);

        auto ms = static_cast<std::chrono::milliseconds::rep>(lua_tonumber(ls, -2));
        auto time = my_clock::now() + std::chrono::milliseconds(ms);

        int ref = luaL_ref(ls, LUA_REGISTRYINDEX);
        lua_pop(ls, 1);

        self->sleep_queue.push_back({ time, ref });
        return 0;
    }, this);
}

mylua::~mylua()
{
    lua_close(ls);
}

void mylua::prelude()
{
    if (luaL_loadfile(ls, "prelude.lua") != 0)
    {
        throw std::runtime_error(fmt::format("prelude compile error: {}", lua_tostring(ls, -1)));
    }
    if (lua_pcall(ls, 0, 0, 0) != 0)
    {
        throw std::runtime_error(fmt::format("lua error: {}", lua_tostring(ls, -1)));
    }
}

mylua_ref mylua::start_sync()
{
    if (luaL_loadfile(ls, "code.lua") != 0)
    {
        throw std::runtime_error(fmt::format("code compile error: {}", lua_tostring(ls, -1)));
    }
    if (lua_pcall(ls, 0, 1, 0) != 0)
    {
        throw std::runtime_error(fmt::format("lua error: {}", lua_tostring(ls, -1)));
    }
    return mylua_ref(ls);
}

void mylua::start_async()
{
    lua_getglobal(ls, "__start");
    if (luaL_loadfile(ls, "code.lua") != 0)
    {
        throw std::runtime_error(fmt::format("code compile error: {}", lua_tostring(ls, -1)));
    }
    if (lua_pcall(ls, 1, 1, 0) != 0)
    {
        throw std::runtime_error(fmt::format("lua error: {}", lua_tostring(ls, -1)));
    }

    thread = lua_tothread(ls, -1);
    if (!thread)
    {
        throw std::runtime_error("entry() must return thread");
    }
    lua_pop(ls, 1);
}

void mylua::register_closure(const char* name, lua_CFunction fn, void* self)
{
    lua_pushlightuserdata(ls, self);
    lua_pushcclosure(ls, fn, 1);
    lua_setglobal(ls, name);
}

void mylua::register_ludata(const char* name, void* data)
{
    lua_pushlightuserdata(ls, data);
    lua_setglobal(ls, name);
}

bool mylua::run_async()
{
    if (lua_status(thread) == LUA_YIELD)
    {
        auto now = my_clock::now();

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
                ++it;
            }
        }

        return true;
    }
    else
    {
        if (lua_status(thread) != 0)
        {
            throw std::runtime_error(fmt::format("lua error: {}", lua_tostring(thread, -1)));
        }
        return false;
    }
}

void mylua::run_atexit()
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

mylua_ref mylua::global(const char* name)
{
    lua_getglobal(ls, name);
    return mylua_ref(ls);
}

class window final
{
public:
    window();
    ~window();

    SDL_Renderer* get_renderer() const
    {
        return renderer;
    }

    std::pair<int, int> get_size() const
    {
        int w, h;
        SDL_GetWindowSize(wnd, &w, &h);
        return { w, h };
    }

    bool poll(double deltaTime);
    void bind_input(lua_State* ls);

private:
    SDL_Window* wnd;
    SDL_Renderer* renderer;

    std::map<std::string, double> input_axis;
    struct {
        bool left = false, right = false, up = false, down = false;
        bool a = false, d = false, w = false, s = false;
    } keys;

    void onKey(SDL_KeyboardEvent& event);

    window(const window&) = delete;
    window& operator =(const window&) = delete;
};

window::window()
{
    wnd = SDL_CreateWindow("luatest", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, 0);
    if (!wnd)
    {
        throw std::runtime_error(fmt::format("failed to create window: {}", SDL_GetError()));
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    renderer = SDL_CreateRenderer(wnd, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        throw std::runtime_error(fmt::format("failed to create renderer: {}", SDL_GetError()));
    }

    input_axis.emplace("Horizontal", 0.0);
    input_axis.emplace("Vertical", 0.0);
}

window::~window()
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(wnd);
}

bool window::poll(double deltaTime)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                onKey(event.key);
                break;
            case SDL_QUIT:
                return false;
        }
    }

    input_axis["Horizontal"] = deltaTime * ((keys.left || keys.a ? -1 : 0) + (keys.right || keys.d ? 1 : 0));
    input_axis["Vertical"] = deltaTime * ((keys.up || keys.w ? 1 : 0) + (keys.down || keys.s ? -1 : 0));

    return true;
}

void window::bind_input(lua_State* ls)
{
    lua_getglobal(ls, "Input");
    lua_getfield(ls, -1, "axis");
    lua_getmetatable(ls, -1);
    lua_pushlightuserdata(ls, this);
    lua_pushcclosure(ls, [](lua_State* ls) {
        auto wnd = closure_upvalue<window*>(ls, 1);
        auto key = lua_tostring(ls, 2);
        if (auto it = wnd->input_axis.find(key); it != wnd->input_axis.end())
        {
            lua_pushnumber(ls, it->second);
            return 1;
        }
        else
        {
            return luaL_error(ls, "invalid input axis name");
        }
    }, 1);
    lua_setfield(ls, -2, "__index");
    lua_pop(ls, 3);
}

void window::onKey(SDL_KeyboardEvent& event)
{
    bool pressed = event.type == SDL_KEYDOWN;
    if (event.keysym.scancode == SDL_SCANCODE_LEFT)
    {
        keys.left = pressed;
    }
    if (event.keysym.scancode == SDL_SCANCODE_RIGHT)
    {
        keys.right = pressed;
    }
    if (event.keysym.scancode == SDL_SCANCODE_UP)
    {
        keys.up = pressed;
    }
    if (event.keysym.scancode == SDL_SCANCODE_DOWN)
    {
        keys.down = pressed;
    }
    if (event.keysym.scancode == SDL_SCANCODE_A)
    {
        keys.a = pressed;
    }
    if (event.keysym.scancode == SDL_SCANCODE_D)
    {
        keys.d = pressed;
    }
    if (event.keysym.scancode == SDL_SCANCODE_W)
    {
        keys.w = pressed;
    }
    if (event.keysym.scancode == SDL_SCANCODE_S)
    {
        keys.s = pressed;
    }
}

class entity final
{
public:
    explicit entity(mylua_ref s) : table(std::move(s)) {}

    mylua_ref get_component(const char* component);
    std::vector<std::pair<std::string, mylua_ref>> get_components();
    void traverse(std::invocable<entity&> auto f);

private:
    mylua_ref table;
};

mylua_ref entity::get_component(const char* component)
{
    auto ls = table.state();
    table.push();
    lua_pushstring(ls, "components");
    lua_rawget(ls, -2);
    lua_pushstring(ls, component);
    lua_rawget(ls, -2);
    mylua_ref ret(ls);
    lua_pop(ls, 2);
    return ret;
}

std::vector<std::pair<std::string, mylua_ref>> entity::get_components()
{
    std::vector<std::pair<std::string, mylua_ref>> v;

    auto ls = table.state();
    table.push();
    lua_pushstring(ls, "components");
    lua_rawget(ls, -2);
    lua_pushnil(ls);
    while (lua_next(ls, -2) != 0)
    {
        mylua_ref value(ls);
        if (lua_isstring(ls, -1))
        {
            v.push_back({ lua_tostring(ls, -1), std::move(value) });
        }
    }
    lua_pop(ls, 2);

    return v;
}

void entity::traverse(std::invocable<entity&> auto f)
{
    std::invoke(f, *this);

    auto ls = table.state();
    table.push();
    lua_pushnil(ls);
    while (lua_next(ls, -2))
    {
        if (lua_isstring(ls, -2) && std::strcmp(lua_tostring(ls, -2), "components") != 0)
        {
            entity child { mylua_ref(ls) };
            child.traverse(f);
        }
        else
        {
            lua_pop(ls, 1);
        }
    }
    lua_pop(ls, 1);
}

class scene final
{
public:
    explicit scene(mylua_ref s);
    void prepare(window& wnd);
    void present(window& wnd);

    entity& get_root() { return root; }

private:
    entity root;
};

scene::scene(mylua_ref s)
    : root(std::move(s))
{
}

struct transform
{
    glm::vec2 position, scale;
    float rotate;
    static transform identity()
    {
        return transform { .position = glm::vec2(0, 0), .scale = glm::vec2(1, 1), .rotate = 1.0f };
    }
};

using renderer_t = void (*)(window& wnd, const transform& tr, mylua_ref component);

void scene::prepare(window& wnd)
{
    auto renderer = wnd.get_renderer();

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    root.traverse([this, &wnd](entity& e) {
        mylua_ref rendererComponent = e.get_component("renderer");
        auto ls = rendererComponent.state();
        if (rendererComponent)
        {
            mylua_ref transformComponent = e.get_component("transform");
            auto tr = transform::identity();
            if (transformComponent)
            {
                transformComponent.push();
                lua_getfield(ls, -1, "_position");
                lua_getfield(ls, -1, "x");
                lua_getfield(ls, -2, "y");
                lua_getfield(ls, -4, "_scale");
                lua_getfield(ls, -1, "x");
                lua_getfield(ls, -2, "y");
                lua_getfield(ls, -7, "_rotate");
                tr.position = glm::vec2(lua_tonumber(ls, -6), lua_tonumber(ls, -5));
                tr.scale = glm::vec2(lua_tonumber(ls, -3), lua_tonumber(ls, -2));
                tr.rotate = lua_tonumber(ls, -1);
                lua_pop(ls, 8);
            }

            rendererComponent.push();
            lua_getfield(ls, -1, "__renderer");
            auto f = reinterpret_cast<renderer_t>(lua_touserdata(ls, -1));
            lua_pop(ls, 1);
            f(wnd, tr, mylua_ref(ls));
        }
    });
}

void scene::present(window& wnd)
{
    auto renderer = wnd.get_renderer();
    SDL_RenderPresent(renderer);
}

void rect_renderer(window& wnd, const transform& tr, mylua_ref component)
{
    auto ls = component.state();
    component.push();
    lua_getfield(ls, -1, "color");
    lua_getfield(ls, -1, "r");
    lua_getfield(ls, -2, "g");
    lua_getfield(ls, -3, "b");
    lua_getfield(ls, -4, "a");
    int r = int(lua_tonumber(ls, -4) * 255);
    int g = int(lua_tonumber(ls, -3) * 255);
    int b = int(lua_tonumber(ls, -2) * 255);
    int a = int(lua_tonumber(ls, -1) * 255);
    lua_pop(ls, 6);

    auto renderer = wnd.get_renderer();
    auto [w, h] = wnd.get_size();

    SDL_FRect rt {
        .x = w / 2.f + tr.position.x - tr.scale.x / 2,
        .y = h / 2.f - tr.position.y - tr.scale.y / 2,
        .w = tr.scale.x,
        .h = tr.scale.y
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderFillRectF(renderer, &rt);
}

int main(int argc, char** argv)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        throw std::runtime_error("SDL_Init error");
    }

    window wnd;
    mylua script;

    script.register_ludata("__rectRenderer", reinterpret_cast<void*>(rect_renderer));
    script.prelude();
    wnd.bind_input(script.state());

    scene sc(script.start_sync());
    auto prev = my_clock::now();

    while (true)
    {
        sc.prepare(wnd);

        auto now = my_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - prev);
        auto deltaTime = std::max(diff.count() / 1000.0, 0.0001);
        prev = now;

        auto ls = script.state();
        lua_getglobal(ls, "Time");
        lua_pushstring(ls, "deltaTime");
        lua_pushnumber(ls, deltaTime);
        lua_rawset(ls, -3);
        lua_pop(ls, 1);

        bool run = wnd.poll(deltaTime);
        if (run)
        {
            sc.present(wnd);
        }
        else
        {
            break;
        }

        SDL_Delay(16);

        sc.get_root().traverse([](entity& e) {
            if (auto scriptComponent = e.get_component("script"))
            {
                auto ls = scriptComponent.state();
                scriptComponent.push();
                lua_getfield(ls, -1, "onUpdate");
                if (lua_pcall(ls, 0, 0, 0) != 0)
                {
                    std::cout << fmt::format("onUpdate() error: {}\n", lua_tostring(ls, -1));
                }
                lua_pop(ls, 1);
            }
        });
    }
}
