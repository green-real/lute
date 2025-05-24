#include "lute/ffi/utils.h"

#include "lua.h"
#include "lualib.h"

#include <array>
#include <functional>
#include <string>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

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

#ifdef _WIN32
#define LIB_PREFIX ""
#define ALT_LIB_PREFIX "lib"
#define LIB_SUFFIX ".dll"
#else
#define LIB_PREFIX "lib"
#define ALT_LIB_PREFIX ""
#define LIB_SUFFIX ".so"
#endif

std::string resolveDLPath(const std::string& path)
{
    if (fs::exists(path)) {
        return fs::absolute(path).string();
    }

    // extract filename and directory
    fs::path p(path);
    std::string filename = p.filename().string();
    std::string dir = p.parent_path().string();
    if (!dir.empty()) {
        dir += fs::path::preferred_separator;
    }

    // generate possibly library names
    std::vector<std::string> possibleNames = {
        LIB_PREFIX + filename,
        ALT_LIB_PREFIX + filename,
        LIB_PREFIX + filename + LIB_SUFFIX,
        ALT_LIB_PREFIX + filename + LIB_SUFFIX,
        filename + LIB_SUFFIX,
        filename
    };

    // attempt to find the library in the directory
    for (const auto& name : possibleNames) {
        std::string fullPath = dir + name;
        if (fs::exists(fullPath)) {
            return fs::absolute(fullPath).string();
        }
    }

    return path; // return the original path if no library was found
}