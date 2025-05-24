#include "lute/ffi.h"
#include "lute/ffi/state.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"
#include "lute/runtime.h"

#include "lua.h"
#include "lualib.h"

#include "ffi.h"
#include <array>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace ffi
{

int lua_dlopen(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::string resolved = resolveDLPath(path);
    std::string err;
    void* lib = openLibrary(resolved.c_str(), err);
    if (!lib) {
        luaL_error(L, "failed to open library '%s': %s", path, err.c_str());
    }

    FFIDLHandle* handle = static_cast<FFIDLHandle*>(lua_newuserdatatagged(L, sizeof(FFIDLHandle), kFFIDLHandleTag));
    handle->handle = lib;
    handle->path = path;
    new (&handle->symbols) std::unordered_map<std::string, void*>();

    return 1;
}

int lua_dlclose(lua_State* L)
{
    FFIDLHandle* handle = static_cast<FFIDLHandle*>(lua_touserdatatagged(L, 1, kFFIDLHandleTag));
    if (!handle) {
        luaL_typeerror(L, 1, "FFIDLHandle expected");
    }

    if (handle->handle) {
        int result = closeLibrary(handle->handle);
        if (result != 0) {
            luaL_error(L, "failed to close library '%s'", handle->path);
        }
        handle->handle = nullptr;
    }

    return 0;
}

FFIDLHandle* checkFFIDLHandle(lua_State* L, int idx)
{
    if (FFIDLHandle* handle = static_cast<FFIDLHandle*>(lua_touserdatatagged(L, idx, kFFIDLHandleTag))) {
        if (handle->handle)
            return handle;
    }
    
    luaL_typeerror(L, idx, "valid FFIDLHandle expected");
}

#ifdef _WIN32

void* openLibrary(const char* path, std::string& err)
{
    if (!path)
        return LoadLibraryA("ucrtbase.dll");

    void* lib = LoadLibraryA(path);
    if (!lib) {
        DWORD error = GetLastError();
        char buffer[256];
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, error, 0, buffer, sizeof(buffer), nullptr);
        err = buffer;
        return nullptr;
    }

    return lib;
}

int closeLibrary(void* lib)
{
    return FreeLibrary((HMODULE)lib);
}

void* getSymbol(void* lib, const std::string& symbol_name)
{
    return (void*)GetProcAddress((HMODULE)lib, symbol_name.c_str());
}

#else

void* openLibrary(const char* path, std::string& err)
{
    void* lib = dlopen(path, RTLD_LAZY);
    if (!lib) {
        err = dlerror();
        return nullptr;
    }

    return lib;
}

int closeLibrary(void* lib)
{
    return dlclose(lib);
}

void* getSymbol(void* lib, const std::string& symbol_name)
{
    return dlsym(lib, symbol_name.c_str());
}
#endif

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

    lua_setreadonly(L, -1, 1);

    return 1;
}
