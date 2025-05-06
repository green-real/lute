#include "lute/ffi.h"

#include "lute/runtime.h"

#include "Luau/DenseHash.h"

#include "lua.h"
#include "lualib.h"

#include <ffi.h>

int add(int a, int b) {
    return a + b;
}

namespace ffi
{

int lua_test(lua_State* L)
{
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);

    ffi_cif cif;
    ffi_type *args[2];
    void *values[2];
    int result;

    args[0] = &ffi_type_sint;
    args[1] = &ffi_type_sint;
    values[0] = &x;
    values[1] = &y;

    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint, args) != FFI_OK) {
        luaL_errorL(L, "ffi_prep_cif failed");
        return 1;
    }

    ffi_call(&cif, FFI_FN(add), &result, values);

    lua_pushinteger(L, result);

    return 1;
}


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
