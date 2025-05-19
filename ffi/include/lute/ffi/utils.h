#pragma once

#include "lute/ffi/ctype.h"

#include "Luau/Common.h"

#include "lua.h"
#include "lualib.h"

#include <string>

using CType = struct ffi::CType;

#define api_check(e) LUAU_ASSERT(e)


std::string ffiStatusToString(int status);

l_noret luaL_argerrorf(lua_State* L, int narg, const char* fmt, ...);