#include "lute/ffi.h"
#include "lute/ffi/state.h"
#include "lute/ffi/dlib.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"
#include "lute/runtime.h"

#include "lua.h"
#include "lualib.h"

#include "ffi.h"

#include <array>

namespace ffi
{

int lua_dlopen(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    newFFIDLHandle(L, path);

    return 1;
}

static void makeRegistry(lua_State* L)
{
    // weak registry
    lua_newtable(L);

    lua_createtable(L, 0, 1);
    lua_pushliteral(L, "v");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);

    lua_rawsetfield(L, LUA_REGISTRYINDEX, kFFIWeakRegistryKey);
}

} // namespace ffi

int luaopen_ffi(lua_State* L)
{
    luteopen_ffi(L);
    lua_setglobal(L, "ffi");

    return 1;
}

int luteopen_ffi(lua_State* L)
{
    ffi::newFFIState(L);
    ffi::makeRegistry(L);

    lua_createtable(L, 0, std::size(ffi::lib) - 1 + std::size(ffi::properties));

    for (auto& [name, func] : ffi::lib)
    {
        if (!name || !func)
            break;

        lua_pushcfunction(L, func, name);
        lua_setfield(L, -2, name);
    }

    ffi::openCInterface(L);
    lua_setfield(L, -2, ffi::kCInterfaceProperty);

#ifdef _WIN32
    const char* kLibCDL = "msvcrt.dll";
    HMODULE ucrtbase = LoadLibraryA("ucrtbase.dll");
    if (ucrtbase) {
        kLibCDL = "ucrtbase.dll";
        FreeLibrary(ucrtbase);
    }
#else
    const char* kLibCDL = "libc.so.6";
#endif

    ffi::newFFIDLHandle(L, kLibCDL);
    lua_setfield(L, -2, ffi::kLibCDLProperty);

    lua_setreadonly(L, -1, 1);

    return 1;
}
