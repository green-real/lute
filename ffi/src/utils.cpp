#include "lute/ffi/utils.h"

#include "lua.h"
#include "lualib.h"

#include <array>
#include <functional>
#include <string>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

#ifdef _WIN32
#define LIB_PREFIX ""
#define ALT_LIB_PREFIX "lib"
#define LIB_SUFFIX ".dll"
#else
#define LIB_PREFIX "lib"
#define ALT_LIB_PREFIX ""
#define LIB_SUFFIX ".so"
#endif

std::string ffiStatusToString(int status)
{
    switch (status)
    {
    case FFI_OK:
        return "ok";
    case FFI_BAD_TYPEDEF:
        return "bad type definition";
    case FFI_BAD_ABI:
        return "bad abi";
    case FFI_BAD_ARGTYPE:
        return "bad argument type";
    default:
        return "unknown error";
    }
}

l_noret luaL_argerrorf(lua_State* L, int narg, const char* fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    luaL_argerrorL(L, narg, lua_pushvfstring(L, fmt, argp));
    va_end(argp);
}

std::vector<std::string> getPossibleDLPaths(const std::string& path)
{
    // extract filename and directory
    fs::path p(path);
    std::string filename = p.filename().string();
    std::string dir = p.parent_path().string();
    if (!dir.empty()) {
        dir = fs::absolute(dir).string();
        
        if (dir.back() != fs::path::preferred_separator) {
            dir += fs::path::preferred_separator;
        }
    }

    std::vector<std::string> possiblePaths = {
        dir + LIB_PREFIX + filename + LIB_SUFFIX,
        dir + ALT_LIB_PREFIX + filename + LIB_SUFFIX,
        dir + filename + LIB_SUFFIX,
        dir + LIB_PREFIX + filename,
        dir + ALT_LIB_PREFIX + filename,
        dir + filename
    };

    return possiblePaths;
}