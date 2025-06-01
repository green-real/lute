#pragma once

#include "lua.h"
#include "lualib.h"

namespace ffi
{

static const luaL_Reg clib[] = {
    
    {nullptr, nullptr}
};

int openCInterface(lua_State* L);

} // namespace ffi
