local ffi = require("@lute/ffi") :: any
local ffi_c = ffi.c

local char = ffi_c.uchar
local size_t = ffi_c.size_t
local int = ffi_c.int

local zlibHandle = ffi.dlopen("libz.so")

local zlib = ffi_c.load(zlibHandle, {
    compress2 = ffi_c.func({char:ptr(), size_t:ptr(), char:ptr(), size_t, int}, int),
    uncompress = ffi_c.func({char:ptr(), size_t:ptr(), char:ptr(), size_t}, int),
    compressBound = ffi_c.func({size_t}, size_t),
})

local function compress(txt: string)
    local bound = zlib.compressBound(#txt):read()
    local compressed = ffi_c.new(ffi_c.array(char, bound + 1))
    local compressedSize = ffi_c.new(size_t, bound)
    local err = zlib.compress2(compressed, compressedSize:ptr(), txt, #txt, 9):read()
    if err ~= 0 then
        error("Compression failed with error code: " .. err)
    end

    return compressed:read()
end

local function decompress(compressed: string, originalSize: number)
    local decompressed = ffi_c.new(ffi_c.array(char, originalSize + 1))
    local decompressedSize = ffi_c.new(size_t, originalSize)
    local err = zlib.uncompress(decompressed, decompressedSize:ptr(), compressed, #compressed):read()
    if err ~= 0 then
        error("Decompression failed with error code: " .. err)
    end

    return decompressed:read()
end

local compressed = compress("Hello, World!")
local decompressed = decompress(compressed, #compressed)
print("Decompressed string: " .. decompressed)
