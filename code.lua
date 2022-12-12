function foo(name, ms, count)
    for i = 1, count do
        print(name .. ': ' .. i)
        await(sleep(ms))
    end
end

async(foo)('x', 1000, 4)
async(foo)('y', 500, 8)
