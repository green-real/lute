#pragma once

#include "lute/ffi/ctype.h"

#include "Luau/Common.h"
#include "lua.h"
#include "lualib.h"

#include <cstddef>

#define api_check(x) LUAU_ASSERT(x)

namespace ffi
{

l_noret luaL_argerrorf(lua_State* L, int narg, const char* fmt, ...);

} // namespace ffi
