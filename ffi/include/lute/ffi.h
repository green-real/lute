#pragma once

#include "lua.h"
#include "lualib.h"

#include <string>

// open the library as a standard global luau library
int luaopen_ffi(lua_State* L);
// open the library as a table on top of the stack
int luteopen_ffi(lua_State* L);

static const char kFFIWeakRegistryKey[] = "LUTE_FFI_WEAK_REGISTRY";

namespace ffi
{

static const char kCInterfaceProperty[] = "c";

static const luaL_Reg lib[] = {
    
    {nullptr, nullptr},
};

static const std::string properties[] = {
    kCInterfaceProperty
};

}  // namespace ffi

namespace ffi
{

int lua_carray(lua_State* L);

int lua_cfunc(lua_State* L);

int lua_cstruct(lua_State* L);

int lua_ctest(lua_State* L);

static const luaL_Reg clib[] = {
    {"array", lua_carray},
    {"func", lua_cfunc},
    {"struct", lua_cstruct},
    {"test", lua_ctest},

    {nullptr, nullptr},
};

static const std::string cproperties[] = {
    kCInterfaceProperty
};

int openCInterface(lua_State* L);

} // namespace ffi
