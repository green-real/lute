#include "lute/ffi.h"

#include "lua.h"
#include "lualib.h"

#include <array>

namespace ffi
{

} // namespace ffi

int luaopen_ffi(lua_State* L)
{
    luteopen_ffi(L);
    lua_setglobal(L, "ffi");

    return 1;
}

int luteopen_ffi(lua_State* L)
{
    luaL_register(L, nullptr, ffi::lib);

    lua_setreadonly(L, -1, 1);

    return 1;
}
