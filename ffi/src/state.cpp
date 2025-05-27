#include "lute/ffi/state.h"
#include "lute/ffi/utils.h"
#include "lute/runtime.h"

#include "lute/userdatas.h"

#include "lua.h"

#include <unordered_map>

namespace ffi
{

FFIState::FFIState(lua_State* L) {
    this->GL = L;
    this->runtime = getRuntime(L);
    this->unrefsScheduled = false;
}

FFIState::~FFIState() {}

void FFIState::addPendingUnref(int ref)
{
    this->pendingUnrefs.push_back(ref);
    if (!this->unrefsScheduled) {
        this->runtime->schedule([this]() {
            this->processPendingUnrefs();
        });

        this->unrefsScheduled = true;
    }
}

void FFIState::processPendingUnrefs()
{
    for (int ref : this->pendingUnrefs) {
        lua_unref(this->GL, ref);
    }
    this->pendingUnrefs.clear();
    this->unrefsScheduled = false;
}

FFIState* newFFIState(lua_State* L)
{
    api_check(getFFIState(L) == nullptr);

    FFIState* state = new FFIState(L);
    FFIState** statePtr = static_cast<FFIState**>(lua_newuserdatatagged(L, sizeof(FFIState*), kFFIStatePointerTag));
    *statePtr = state;
    
    lua_pushlightuserdata(L, statePtr);
    lua_pushvalue(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_State* GL = lua_mainthread(L);
    GLToFFIStateMap[GL] = state;

    lua_pop(L, 1); // pop the FFIState pointer from the stack

    return state;
}

FFIState* getFFIState(lua_State* L)
{
    lua_State* GL = lua_mainthread(L);
    return GLToFFIStateMap.find(GL) != GLToFFIStateMap.end() ? GLToFFIStateMap[GL] : nullptr;
}

void lua_dtor_FFIStatePointer(lua_State* L, void* p)
{
    lua_State* GL = lua_mainthread(L);
    GLToFFIStateMap.erase(GL);

    FFIState** state = static_cast<FFIState**>(p);
    delete *state;
}

}