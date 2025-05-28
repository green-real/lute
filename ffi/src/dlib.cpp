#include "lute/ffi/dlib.h"
#include "lute/ffi.h"
#include "lute/ffi/state.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include "lua.h"
#include "lualib.h"

#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace ffi
{

FFIDLHandle* newFFIDLHandle(lua_State* L, const char* path)
{
    std::vector<std::string> possiblePaths = getPossibleDLPaths(path);
    std::string err;
    
    std::string resolved;
    void* lib = nullptr;
    for (const std::string& path : possiblePaths) {
        lib = openLibrary(path.c_str(), err);
        if (lib) {
            resolved = path;
            break;
        }
    }
    if (!lib) {
        luaL_error(L, "failed to open library '%s': %s", path, err.c_str());
    }

    FFIDLHandle* dl = static_cast<FFIDLHandle*>(lua_newuserdatataggedwithmetatable(L, sizeof(FFIDLHandle), kFFIDLHandleTag));
    dl->handle = lib;
    dl->refcount = 0;
    dl->selfref = LUA_NOREF;

    lua_rawgetfield(L, LUA_REGISTRYINDEX, kFFIWeakRegistryKey);
    lua_pushlightuserdata(L, dl);
    lua_pushstring(L, path);
    lua_rawset(L, -3);
    lua_pop(L, 1); // pop the weak registry table

    return dl;
}

FFIDLHandle* toFFIDLHandle(lua_State* L, int idx)
{
    return static_cast<FFIDLHandle*>(lua_touserdatatagged(L, idx, kFFIDLHandleTag));
}

FFIDLHandle* checkFFIDLHandle(lua_State* L, int idx)
{
    if (FFIDLHandle* handle = toFFIDLHandle(L, idx)) {
        if (handle->handle)
            return handle;
    }
    
    luaL_typeerror(L, idx, "valid FFIDLHandle expected");
}

// increments the refcount and retains the FFIDLHandle
void retainFFIDLHandle(lua_State* L, int idx)
{
    FFIDLHandle* dl = toFFIDLHandle(L, idx);
    api_check(dl != nullptr);
    api_check(dl->refcount == 0 || dl->selfref != LUA_NOREF);

    dl->refcount++;
    if (dl->selfref == LUA_NOREF)
        dl->selfref = lua_ref(L, idx); 
}

// retains a FFIDLHandle that is currently retained, so that it is not garbage collected until it is released
// this should only be used when the FFIDLHandle is already retained
void retainFFIDLHandle(lua_State* L, FFIDLHandle* dl)
{
    api_check(dl != nullptr);
    api_check(dl->refcount != 0);
    api_check(dl->selfref != LUA_NOREF);

    dl->refcount++;
}

// releases a FFIDLHandle previously retained by retainFFIDLHandle
void releaseFFIDLHandle(lua_State* L, FFIDLHandle* dl)
{
    api_check(dl != nullptr);
    api_check(dl->refcount > 0);
    api_check(dl->selfref != LUA_NOREF);

    dl->refcount--;
    if (dl->refcount == 0) {
        // TODO: lua_unref call is unsafe here, move it somewhere else
        FFIState* ffiState = getFFIState(L);
        api_check(ffiState != nullptr);

        ffiState->addPendingUnref(dl->selfref);   
        dl->selfref = LUA_NOREF;
    }
}

// gets the path to a FFIDLHandle
std::string getFFIDLHandlePath(lua_State* L, FFIDLHandle* dl)
{
    lua_rawgetfield(L, LUA_REGISTRYINDEX, kFFIWeakRegistryKey);
    lua_pushlightuserdata(L, dl);
    lua_rawget(L, -2);
    std::string path = lua_tostring(L, -1);
    lua_pop(L, 2); // pop the weak registry table and the path

    return path;
}

void lua_dtor_FFIDLHandle(lua_State* L, void* ud)
{
    FFIDLHandle* dl = static_cast<FFIDLHandle*>(ud);
    if (dl->handle) {
        closeLibrary(dl->handle);
        dl->handle = nullptr;
    }
}

void initFFIDLHandle(lua_State* L)
{
    luaL_newmetatable(L, kFFIDLHandle);

    lua_pushvalue(L, -1);
    lua_setuserdatametatable(L, kFFIDLHandleTag);

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_setuserdatadtor(L, kFFIDLHandleTag, lua_dtor_FFIDLHandle);
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

} // namespace ffi

