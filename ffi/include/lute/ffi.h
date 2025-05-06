#pragma once

#include "lua.h"
#include "lualib.h"
#include <string>

// open the library as a standard global luau library
int luaopen_ffi(lua_State* L);
// open the library as a table on top of the stack
int luteopen_ffi(lua_State* L);

namespace ffi
{

int lua_test(lua_State* L);

static const char kCInterfaceProperty[] = "c";

static const luaL_Reg lib[] = {
    {"test", lua_test},
    
    {nullptr, nullptr},
};

static const std::string properties[] = {
    kCInterfaceProperty
};

} // namespace ffi
