local ffi = require("@lute/ffi") :: any
local c = ffi.c

local char = c.uchar
local size_t = c.size_t
local int = c.int

local zlib = c.load(ffi.dlopen("libz.so"), {
    compress2 = c.func({char:ptr(), size_t:ptr(), char:ptr(), size_t, int}, int),
    uncompress = c.func({char:ptr(), size_t:ptr(), char:ptr(), size_t}, int),
    compressBound = c.func({size_t}, size_t),
})

local function compress(txt: string)
    local bound = zlib.compressBound(#txt):read()
    local compressed = c.new(c.array(char, bound + 1))
    local compressedSize = c.new(size_t, bound)
    local err = zlib.compress2(compressed, compressedSize:ptr(), txt, #txt, 9):read()
    if err ~= 0 then
        error("Compression failed with error code: " .. err)
    end

    return compressed:read()
end

local function decompress(compressed: string, originalSize: number)
    local decompressed = c.new(c.array(char, originalSize + 1))
    local decompressedSize = c.new(size_t, originalSize)
    local err = zlib.uncompress(decompressed, decompressedSize:ptr(), compressed, #compressed):read()
    if err ~= 0 then
        error("Decompression failed with error code: " .. err)
    end

    return decompressed:read()
end

local compressed = compress("Hello, World!")
local decompressed = decompress(compressed, #compressed)
print("Decompressed string: " .. decompressed)
