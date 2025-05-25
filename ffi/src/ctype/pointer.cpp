#include "lute/ffi/ctype.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include "Luau/Common.h"
#include "lua.h"
#include "lualib.h"

#include <cstddef>

namespace ffi
{

CPointerType::CPointerType(lua_State* L, CType* innertype, bool releasectype) : innertype(innertype), releasectype(releasectype) {}

CPointerType::~CPointerType() {}

void CPointerType::releaseDependencies(lua_State* L) const
{
    if (this->releasectype)
        releaseCType(L, this->innertype);
}

// if releasectype, retainCType must have been called on inner before calling newCPointerType
CType* newCPointerType(lua_State* L, CType* innertype, bool releasectype)
{
    api_check(innertype != nullptr);
    api_check(!releasectype || innertype->selfref != LUA_NOREF);
    
    CType* ct = newCType(L, CTypeKind::POINTER, kFFICPointerTypeTag);
    ct->ptr = new CPointerType(L, innertype, releasectype);
    
    return ct;
}

CType* toCPointerType(lua_State* L, int idx)
{
    return static_cast<CType*>(lua_touserdatatagged(L, idx, kFFICPointerTypeTag));
}

CType* checkCPointerType(lua_State* L, int idx)
{
    CType* ct = toCPointerType(L, idx);
    if (ct != nullptr)
        return ct;
    
    luaL_typeerror(L, idx, kCPointerType);
    return nullptr;
}

static int lua_tostring_CPointerType(lua_State* L)
{
    CType* ct = checkCPointerType(L, 1);
    return handleCTypeToString(L, ct);
}

static int lua_namecall_CPointerType(lua_State* L)
{
    CType* ct = checkCPointerType(L, 1);
    const char* method = lua_namecallatom(L, nullptr);
    if (method == nullptr)
        luaL_error(L, "attempt to namecall CPointerType with invalid method");
    
    return handleCTypeNamecall(L, ct);
}

void lua_dtor_CPointerType(lua_State* L, void* ud)
{
    CType* ct = static_cast<CType*>(ud);

    ct->ptr->releaseDependencies(L);
    delete ct->ptr;
}

const luaL_Reg mt[] = {
    {"__tostring", lua_tostring_CPointerType},
    {"__namecall", lua_namecall_CPointerType},

    {nullptr, nullptr},
};

void initCPointerType(lua_State* L)
{
    luaL_newmetatable(L, kCPointerType);

    lua_pushvalue(L, -1);
    lua_setuserdatametatable(L, kFFICPointerTypeTag);

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, lua_tostring_CPointerType, "kCPointerType.__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lua_namecall_CPointerType, "kCPointerType.__namecall");
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_setuserdatadtor(L, kFFICPointerTypeTag, lua_dtor_CPointerType);
}

}