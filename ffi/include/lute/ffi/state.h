#pragma once

#include "lute/runtime.h"

#include "lua.h"

#include <unordered_map>
#include <vector>

namespace ffi
{

using FFIState = struct FFIState;

static std::unordered_map<lua_State*, FFIState*> GLToFFIStateMap;

struct FFIState {
    lua_State* GL;
    Runtime* runtime;

    // A vector for pending lua_unref calls
    std::vector<int> pendingUnrefs;

    bool unrefsScheduled;

    void addPendingUnref(int ref);

private:
    FFIState(lua_State* L);
    ~FFIState();

    void processPendingUnrefs();

    friend FFIState* newFFIState(lua_State* L);
    friend void lua_dtor_FFIStatePointer(lua_State* L, void* p);
};

FFIState* newFFIState(lua_State* L);
FFIState* getFFIState(lua_State* L);

}
