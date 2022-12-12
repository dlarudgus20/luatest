function curry(f)
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

function pong(func, callback, ...)
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

function entry(main)
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
