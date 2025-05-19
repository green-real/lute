#pragma once

#include "lua.h"

#include <unordered_map>

namespace ffi
{

using FFIState = struct FFIState;

static std::unordered_map<lua_State*, FFIState*> GLToFFIStateMap;

struct FFIState {


private:
    FFIState(lua_State* L);
    ~FFIState();

    friend FFIState* newFFIState(lua_State* L);
    friend void lua_dtor_FFIStatePointer(lua_State* L, void* p);
};

FFIState* newFFIState(lua_State* L);
FFIState* getFFIState(lua_State* L);

}
