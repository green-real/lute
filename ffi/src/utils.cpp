#include "./utils.h"

#include "lute/ffi/ctype.h"

#include "lua.h"
#include "lualib.h"

#include <cstddef>
#include <string>

namespace ffi
{

l_noret luaL_argerrorf(lua_State* L, int narg, const char* fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    luaL_argerrorL(L, narg, lua_pushvfstring(L, fmt, argp));
    va_end(argp);
}

} // namespace ffi
