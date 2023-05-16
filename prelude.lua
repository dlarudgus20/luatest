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

Promise = {}
Promise.__index = Promise

function Promise.new(thunk)
    assert(type(thunk) == 'function', 'type error :: function is expected')
    return setmetatable({ thunk = thunk }, Promise)
end

function Promise.sleep(sec)
    return Promise.new(curry(__sleep)(sec))
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
            assert(getmetatable(result) == Promise, 'type error :: promise is expected')
            result.thunk(step)
        end
    end
    step(...)
    return thread
end

function Promise.async(func)
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
        local p = Promise.new(thunk)
        p.thread = pong(func, on_complete, ...)
        return p
    end
    return factory
end

function Promise.await(promise)
    assert(getmetatable(promise) == Promise, 'type error :: promise is expected')
    return coroutine.yield(promise)
end

function Promise.join(promises)
    local len = table.getn(promises)
    local done = 0
    local acc = {}
    local function thunk(callback)
        if len == 0 then
            return callback()
        end
        for i, tk in ipairs(promises) do
            assert(getmetatable(tk) == Promise, 'type error :: promise is expected')
            local function cb(...)
                acc[i] = {...}
                done = done + 1
                if done == len then
                    callback(unpack(acc))
                end
            end
            tk.thunk(cb)
        end
    end
    return Promise.new(thunk)
end

Util = {}

function Util.clamp(x, min, max)
    if x < min then return min end
    if x > max then return max end
    return x
end

Vec2 = { x = 0, y = 0 }
Vec2.__index = Vec2

function Vec2.new(x, y)
    return setmetatable({ x = x, y = y }, Vec2)
end

function Vec2.__add(a, b)
    return a:clone():add(b)
end

function Vec2.__sub(a, b)
    return a:clone():sub(b)
end

function Vec2.__mul(a, b)
    if type(a) == 'number' then
        return b:clone():mul1(a)
    elseif type(b) == 'number' then
        return a:clone():mul1(b)
    elseif getmetatable(a) == Vec2 and getmetatable(b) == Vec2 then
        return a:clone():mul(b)
    else
        error('invalid arguments')
    end
end

function Vec2.__div(a, b)
    if type(a) == 'number' then
        return b:clone():div1(a)
    elseif type(b) == 'number' then
        return a:clone():div1(b)
    elseif getmetatable(a) == Vec2 and getmetatable(b) == Vec2 then
        return a:clone():div(b)
    else
        error('invalid arguments')
    end
end

function Vec2.__unm(a)
    return a:clone():mul1(-1)
end

function Vec2.__eq(a, b)
    return a.x == b.x and a.y == b.y
end

function Vec2:assign(v)
    self.x = v.x
    self.y = v.y
end

function Vec2:clone()
    return Vec2.new(self.x, self.y)
end

function Vec2:add(v)
    self.x = self.x + v.x
    self.y = self.y + v.y
    return self
end

function Vec2:add2(x, y)
    self.x = self.x + x
    self.y = self.y + y
    return self
end

function Vec2:sub(v)
    self.x = self.x - v.x
    self.y = self.y - v.y
    return self
end

function Vec2:sub2(x, y)
    self.x = self.x - x
    self.y = self.y - y
    return self
end

function Vec2:mul(v)
    self.x = self.x * v.x
    self.y = self.y * v.y
    return self
end

function Vec2:mul1(t)
    self.x = self.x * t
    self.y = self.y * t
    return self
end

function Vec2:mul2(x, y)
    self.x = self.x * x
    self.y = self.y * y
    return self
end

function Vec2:div(v)
    self.x = self.x / v.x
    self.y = self.y / v.y
    return self
end

function Vec2:div1(t)
    self.x = self.x / t
    self.y = self.y / t
    return self
end

function Vec2:div2(x, y)
    self.x = self.x / x
    self.y = self.y / y
    return self
end

function Vec2:dot(v)
    return self.x * v.x + self.y * v.y
end

function Vec2:cross(v)
    return self.x * v.y - self.y * v.x
end

function Vec2:rotate(angle)
    local x = math.cos(angle) * self.x - math.sin(angle) * self.y
    local y = math.sin(angle) * self.x + math.cos(angle) * self.y
    self.x = x
    self.y = y
    return self
end

Mat2 = { m11 = 0, m12 = 0, m21 = 0, m22 = 0 }
Mat2.__index = Mat2

function Mat2.new(m11, m12, m21, m22)
    return setmetatable({ m11 = m11, m12 = m12, m21 = m21, m22 = m22 }, Mat2)
end

function Mat2.zero()
    return Mat2.new(0, 0, 0, 0)
end

function Mat2.identity()
    return Mat2.new(1, 0, 0, 1)
end

function Mat2.rotate(angle)
    return Mat2.new(math.cos(angle), -math.sin(angle), math.sin(angle), math.cos(angle))
end

function Mat2.__add(a, b)
    return a:clone():add(b)
end

function Mat2.__sub(a, b)
    return a:clone():sub(b)
end

function Mat2.__mul(a, b)
    if type(a) == 'number' then
        return b:clone():mul1(a)
    elseif type(b) == 'number' then
        return a:clone():mul1(b)
    elseif getmetatable(b) == Vec2 then
        return a:mul2(b)
    elseif getmetatable(a) == Mat2 and getmetatable(b) == Mat2 then
        return a:clone():mul(b)
    else
        error('invalid arguments')
    end
end

function Mat2.__unm(a)
    return a:clone():mul1(-1)
end

function Mat2.__eq(a, b)
    return a.m11 == b.m11 and a.m12 == b.m12 and a.m21 == b.m21 and a.m22 == b.m22
end

function Mat2:clone()
    return Mat2.new(self.m11, self.m12, self.m21, self.m22)
end

function Mat2:add(m)
    self.m11 = self.m11 + m.m11
    self.m12 = self.m12 + m.m12
    self.m21 = self.m21 + m.m21
    self.m22 = self.m22 + m.m22
    return self
end

function Mat2:sub(m)
    self.m11 = self.m11 - m.m11
    self.m12 = self.m12 - m.m12
    self.m21 = self.m21 - m.m21
    self.m22 = self.m22 - m.m22
    return self
end

function Mat2:mul1(t)
    self.m11 = self.m11 * t
    self.m12 = self.m12 * t
    self.m21 = self.m21 * t
    self.m22 = self.m22 * t
    return self
end

function Mat2:mul2(v)
    return Vec2.new(
        self.m11 * v.x + self.m12 * v.y,
        self.m21 * v.x + self.m22 * v.y)
end

function Mat2:mul(m)
    local t11 = self.m11 * m.m11 + self.m12 * m.m21
    local t12 = self.m11 * m.m12 + self.m12 * m.m22
    local t21 = self.m21 * m.m11 + self.m22 * m.m21
    local t22 = self.m21 * m.m12 + self.m22 * m.m22
    self.m11 = t11
    self.m12 = t12
    self.m21 = t21
    self.m22 = t22
end

Color = {}
Color.__index = Color

function Color.new(r, g, b, a)
    return setmetatable({ r = r, g = g, b = b, a = a }, Color)
end

Transform = {}
Transform.__index = Transform

function Transform.new()
    return setmetatable({
        position = Vec2.new(0, 0),
        scale = Vec2.new(1, 1),
        rotate = 0,
    }, Transform)
end

function Transform.new3(p, s, r)
    return setmetatable({ position = p, scale = s, rotate = r }, Transform)
end

function Transform:moveBy(x, y)
    self.position:add2(x, y)
    return self
end

function Transform:scaleBy(x, y)
    self.scale:mul2(x, y)
    return self
end

function Transform:rotateBy(angle)
    self.rotate = self.rotate * angle
    return self
end

function Transform:worldpos()
    local parent = self.parent
    local pos = self.position:clone()
    while parent ~= nil do
        pos:mul(parent.scale):rotate(parent.rotate):add(parent.position)
        parent = parent.parent
    end
    return pos
end

function Transform:setworldpos(pos)
    local function recur(tr, p)
        if tr.parent ~= nil then
            p = recur(tr.parent, p)
        end
        return p:sub(tr.position):rotate(-tr.rotate):div(tr.scale)
    end
    self.position:assign(pos)
    if self.parent ~= nil then
        self.position = recur(self.parent, self.position)
    end
end

Entity = {}

function Entity:__index(k)
    return Entity[k] or self.children[k]
end

function Entity:__newindex(k, v)
    if k == 'tag' then
        rawset(self, 'tag', v)
    elseif getmetatable(v) == Entity then
        assert(type(k) == "string", 'invalid key')
        v.components.transform.parent = self.components.transform
        self.children[k] = v
    elseif v == nil then
        assert(type(k) == "string", 'invalid key')
        self.children[k].components.transform.parent = nil
        self.children[k] = nil
    else
        error('value is not an Entity')
    end
end

function Entity.new()
    local o = { children = {}, components = { transform = Transform.new() } }
    return setmetatable(o, Entity)
end

function Entity:setTag(tag)
    rawset(self, 'tag', tag)
    return self
end

function Entity:addComponent(name, component)
    assert(self.components[name] == nil, 'component already exists')
    self.components[name] = component
end

function Entity:pushChild(entity)
    entity.components.transform.parent = self.components.transform
    table.insert(self.children, entity)
    return #self.children - 1
end

function Entity:removeChild(index)
    assert(self.children[index] ~= nil, 'invalid index')
    self.children[index].components.transform.parent = nil
    table.remove(self.children, index)
end

local function setReadonly(o)
    return setmetatable(o, { __newindex = function(t, k, v) error('readonly table') end })
end

Time = { time = 0, deltaTime = 0.001 }

Input = setReadonly({
    axis = setReadonly({}),
    button = setReadonly({}),
})

Script = {}
Script.__index = Script

function Script.new()
    return setmetatable({ __threads = {} }, Script)
end

RectRenderer = { __renderer = __rectRenderer }
RectRenderer.__index = RectRenderer

function RectRenderer.new()
    return setmetatable({ color = Color.new(0.375, 0.5, 1, 1), fill = true }, RectRenderer)
end

function RectRenderer:setColor(r, g, b, a)
    self.color.r = r
    self.color.g = g
    self.color.b = b
    self.color.a = a
    return self
end

function RectRenderer:setFill(fill)
    self.fill = fill
    return self
end

RigidBody = { velocity = Vec2.new(0, 0) }
RigidBody.__index = RigidBody

function RigidBody.new()
    return setmetatable({}, RigidBody)
end

function RigidBody:setVelocity(x, y)
    self.velocity = Vec2.new(x, y)
    return self
end

RectCollider = {}
RectCollider.__index = RectCollider

function RectCollider.new()
    return setmetatable({}, RectCollider)
end
