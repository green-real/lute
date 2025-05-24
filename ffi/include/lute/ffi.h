#pragma once

#include "lua.h"
#include "lualib.h"

#include <string>
#include <unordered_map>

// open the library as a standard global luau library
int luaopen_ffi(lua_State* L);
// open the library as a table on top of the stack
int luteopen_ffi(lua_State* L);

static const char kFFIWeakRegistryKey[] = "LUTE_FFI_WEAK_REGISTRY";

namespace ffi
{

int lua_dlopen(lua_State* L);

int lua_dlclose(lua_State* L);

static const char kCInterfaceProperty[] = "c";

static const luaL_Reg lib[] = {
    {"dlopen", lua_dlopen},
    {"dlclose", lua_dlclose},
    
    {nullptr, nullptr},
};

static const std::string properties[] = {
    kCInterfaceProperty
};

}  // namespace ffi

namespace ffi
{

struct FFIDLHandle
{
    void* handle;
    const char* path;

    std::unordered_map<std::string, void*> symbols;
};

FFIDLHandle* checkFFIDLHandle(lua_State* L, int idx);


void* openLibrary(const char* path, std::string& err);

int closeLibrary(void* lib);

void* getSymbol(void* lib, const std::string& symbol_name);

}

namespace ffi
{

int lua_carray(lua_State* L);

int lua_cfunc(lua_State* L);

int lua_cstruct(lua_State* L);

int lua_cnew(lua_State* L);

int lua_csizeof(lua_State* L);

int lua_cload(lua_State* L);

int lua_ccast(lua_State* L);

static const luaL_Reg clib[] = {
    {"array", lua_carray},
    {"func", lua_cfunc},
    {"struct", lua_cstruct},
    {"new", lua_cnew},
    {"sizeof", lua_csizeof},
    {"load", lua_cload},
    {"cast", lua_ccast},

    {nullptr, nullptr},
};

static const std::string cproperties[] = {
    kCInterfaceProperty
};

int openCInterface(lua_State* L);

} // namespace ffi
