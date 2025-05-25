local ffi = require("@lute/ffi") :: any
local c = ffi.c

local char = c.char
local int = c.int
local double = c.double
local void = c.void

local libc = c.load(ffi.libc, {
    printf = c.func({char:ptr()}, int, "printf");
    printf_str = c.func({char:ptr(), char:ptr()}, int, "printf");
    printf_int = c.func({char:ptr(), int}, int, "printf");
    printf_ptr = c.func({char:ptr(), void:ptr()}, int, "printf");
})

local bufptr = buffer.create(8)
buffer.writeu32(bufptr, 0, 0x89ABCDEF)
buffer.writeu32(bufptr, 4, 0x01234567)
local ptr: any = c.new(void:ptr(), bufptr)

libc.printf("Test\n")
libc.printf_str("%s\n", "Test")
libc.printf_int("Number: %d\n", 42)
libc.printf_ptr("Pointer: %p\n", ptr)

local function printf(fmt: string, ...)
    local args = {...}
    local arg_types = table.create(#args + 1)
    arg_types[1] = char:ptr()
    for i = 1, #args do
        if typeof(args[i]) == "string" then
            arg_types[i + 1] = char:ptr()
        elseif typeof(args[i]) == "number" then
            arg_types[i + 1] = double
        elseif typeof(args[i]) == "CPointerData" then
            arg_types[i + 1] = void:ptr()
        else
            error("Unsupported argument type: " .. typeof(args[i]))
        end
    end

    local func_t = c.func(arg_types, int, "printf")
    local printf = c.load(ffi.libc, func_t)

    printf(fmt, ...)
end

printf("string: %s, number: %f, pointer: %p\n", "Hello", 123.52, ptr)