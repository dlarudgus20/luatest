package.path = './libs/?.lua;./libs/?/init.lua'
package.cpath = ''

local function curry(f)
    local function g(...)
        local args = {...}
        local function h(x)
            table.insert(args, x)
            return f(unpack(args))
        end
        return h
    end
    return g
end

local function pong(func, callback, ...)
    assert(type(func) == 'function', 'type error :: function is expected')
    assert(type(callback) == 'function', 'type error :: function is expected')
    local thread = coroutine.create(func)
    local function step(...)
        local status, result = coroutine.resume(thread, ...)
        assert(status, result)
        if coroutine.status(thread) == 'dead' then
            callback(result)
        else
            assert(type(result) == 'function', 'type error :: function is expected')
            result(step)
        end
    end
    step(...)
    return thread
end

function __start(main)
    return pong(function ()
        main()
        coroutine.yield(__atexit)
    end, function () end)
end

function async(func)
    assert(type(func) == 'function', 'type error :: function is expected')
    local function factory(...)
        local cb, rs
        local stat = 0
        local function call()
            if cb then
                __sleep(0, function () cb(rs) end)
            end
        end
        local function on_complete(result)
            rs = result
            if stat < 0 then
                call()
            else
                stat = 1
            end
        end
        local function thunk(callback)
            cb = callback
            if stat > 0 then
                call()
            end
            stat = -1
        end
        pong(func, on_complete, ...)
        return thunk
    end
    return factory
end

function await(thunk)
    assert(type(thunk) == 'function', 'type error :: function is expected')
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

sleep = curry(__sleep)

Vec2 = { x = 0, y = 0 }
Vec2.__index = Vec2

function Vec2.new(x, y)
    return setmetatable({ x = x, y = y }, Vec2)
end

function Vec2:add(x, y)
    self.x = self.x + x
    self.y = self.y + y
    return self
end

function Vec2:mul(x, y)
    self.x = self.x * x
    self.y = self.y * y
    return self
end

Color = { r = 0, g = 0, b = 0, a = 0 }
Color.__index = Color

function Color.new(r, g, b, a)
    return setmetatable({ r = r, g = g, b = b, a = a }, Color)
end

Entity = {}
Entity.__index = Entity

function Entity.new()
    local o = {}
    o.components = {}
    setmetatable(o, Entity)
    return o
end

function Entity:addComponent(name, component)
    self.components[name] = component
end

Transform = {}
Transform.__index = Transform

function Transform.new()
    local o = {}
    o._position = Vec2.new(0, 0)
    o._scale = Vec2.new(1, 1)
    o._rotate = 0
    setmetatable(o, Transform)
    return o
end

function Transform:move(x, y)
    self._position:add(x, y)
    return self
end

function Transform:scale(x, y)
    self._scale:mul(x, y)
    return self
end

function Transform:rotate(angle)
    self._rotate = self._rotate * angle
    return self
end

local function setReadonly(o)
    return setmetatable(o, { __newindex = function(t, k, v) error('readonly table') end })
end

Time = setReadonly({ deltaTime = 0 })

Input = setReadonly({ axis = setReadonly({}) })

Script = {}
Script.__index = Script

function Script.new()
    return setmetatable({}, Script)
end

RectRenderer = { __renderer = __rectRenderer }
RectRenderer.__index = RectRenderer

function RectRenderer.new()
    return setmetatable({ color = Color.new(0.375, 0.5, 1, 1) }, RectRenderer)
end

function RectRenderer:setColor(r, g, b, a)
    self.color.r = r
    self.color.g = g
    self.color.b = b
    self.color.a = a
    return self
end
