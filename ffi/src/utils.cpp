#include "lute/ffi/utils.h"

#include "lua.h"
#include "lualib.h"

#include <array>
#include <functional>

using CTypeKind = enum ffi::CTypeKind;

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