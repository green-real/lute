#include "lute/ffi.h"

#include "lute/runtime.h"

namespace ffi
{

} // namespace ffi

int luaopen_ffi(lua_State* L)
{
    luaL_register(L, "ffi", ffi::lib);

    return 1;
}

int luteopen_ffi(lua_State* L)
{
    lua_createtable(L, 0, std::size(ffi::lib) + std::size(ffi::properties));

    for (auto& [name, func] : ffi::lib)
    {
        if (!name || !func)
            break;

        lua_pushcfunction(L, func, name);
        lua_setfield(L, -2, name);
    }

    lua_setreadonly(L, -1, 1);

    return 1;
}
