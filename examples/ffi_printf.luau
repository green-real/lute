local ffi = require("@lute/ffi") :: any
local c = ffi.c

local char = c.char
local int = c.int
local double = c.double
local void = c.void

local charptr = char:ptr()
local voidptr = void:ptr()

local libc = c.load(ffi.libc, {
    printf = c.func({charptr}, int, "printf");
    printf_str = c.func({charptr, charptr}, int, "printf");
    printf_int = c.func({charptr, int}, int, "printf");
    printf_ptr = c.func({charptr, voidptr}, int, "printf");
})

local bufptr = buffer.create(8)
buffer.writeu32(bufptr, 0, 0x89ABCDEF)
buffer.writeu32(bufptr, 4, 0x01234567)
local ptr = c.new(voidptr, bufptr)

libc.printf("Test\n")
libc.printf_str("%s\n", "Test")
libc.printf_int("Number: %d\n", 42)
libc.printf_ptr("Pointer: %p\n", ptr)

local printf_ret = c.new(int)
local printfcache = {}
local function printf(fmt: string, ...)
    local args = {...}
    local arg_types = table.create(#args)
    for i,v in args do arg_types[i] = typeof(v) end

    local key = table.concat(arg_types, "|")
    if printfcache[key] then
        printfcache[key](fmt, ...)
        return
    end

    local arg_ctypes = table.create(#args + 1)
    table.insert(arg_ctypes, charptr)
    for i, v in args do
        local ty = arg_types[i]
        if ty == "string" then
            table.insert(arg_ctypes, charptr)
        elseif ty == "number" then
            table.insert(arg_ctypes, double)
        elseif ty == "CPointerData" then
            table.insert(arg_ctypes, voidptr)
        else
            error("Unsupported argument type: " .. typeof(args[i]))
        end
    end

    local func_t = c.func(arg_ctypes, int, "printf")
    local printf = c.load(ffi.libc, func_t)
    printf:setret(printf_ret)

    printfcache[key] = printf

    printf(fmt, ...)
end

printf("string: %s, number: %f, pointer: %p\n", "Hello", 123.52, ptr)
print("printf resulting length: " .. printf_ret:read())