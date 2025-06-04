#pragma once

#include "lua.h"

struct TCCState;

namespace ffi
{

struct FFIState
{
    TCCState* tcc;

    FFIState();
    ~FFIState();
};

FFIState* getFFIState(lua_State* L);

} // namespace ffi