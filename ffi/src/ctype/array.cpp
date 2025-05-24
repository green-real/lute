#include "lute/ffi/ctype.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include "Luau/Common.h"
#include "lua.h"
#include "lualib.h"

#include <cstddef>
#include <unordered_set>

namespace ffi
{

CArrayType::CArrayType(lua_State* L, CType* elemtype, std::size_t size, bool releasectype) : elemtype(elemtype), size(size), releasectype(releasectype)
{
    ft.type = FFI_TYPE_STRUCT;
    ft.size = sizeOfCType(elemtype) * size;
    ft.alignment = getFFITypeOfCType(elemtype)->alignment;
    ft.elements = nullptr;
}

CArrayType::~CArrayType() {}

void CArrayType::releaseDependencies(lua_State* L) const
{
    if (this->releasectype)
        releaseCType(L, this->elemtype);
}

// if releasectype, retainCType must have been called on inner before calling newCArrayType
CType* newCArrayType(lua_State* L, CType* elemtype, std::size_t size, bool releasectype)
{
    api_check(elemtype != nullptr);
    api_check(elemtype->kind != CTypeKind::FUNC);
    api_check(elemtype->kind != CTypeKind::VOID);
    api_check(!releasectype || elemtype->selfref != LUA_NOREF);

    CType* ct = newCType(L, CTypeKind::ARRAY, kFFICArrayTypeTag);
    ct->array = new CArrayType(L, elemtype, size, releasectype);
    
    return ct;
}

CType* toCArrayType(lua_State* L, int idx)
{
    return static_cast<CType*>(lua_touserdatatagged(L, idx, kFFICArrayTypeTag));
}

CType* checkCArrayType(lua_State* L, int idx)
{
    CType* ct = toCArrayType(L, idx);
    if (ct != nullptr)
        return ct;
    
    luaL_typeerror(L, idx, kCArrayType);
    return nullptr;
}

static int lua_tostring_CArrayType(lua_State* L)
{
    CType* ct = checkCArrayType(L, 1);
    return handleCTypeToString(L, ct);
}

static int lua_namecall_CArrayType(lua_State* L)
{
    CType* ct = checkCArrayType(L, 1);
    const char* method = lua_namecallatom(L, nullptr);
    if (method == nullptr)
        luaL_error(L, "attempt to namecall CArrayType with invalid method");
    
    return handleCTypeNamecall(L, ct);
}

void lua_dtor_CArrayType(lua_State* L, void* ud)
{
    CType* ct = static_cast<CType*>(ud);
    ct->array->releaseDependencies(L);
    delete ct->array;
}

void initCArrayType(lua_State* L)
{
    luaL_newmetatable(L, kCArrayType);

    lua_pushvalue(L, -1);
    lua_setuserdatametatable(L, kFFICArrayTypeTag);

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, lua_tostring_CArrayType, (std::string(kCArrayType) + "__tostring").c_str());
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lua_namecall_CArrayType, (std::string(kCArrayType) + "__namecall").c_str());
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_setuserdatadtor(L, kFFICArrayTypeTag, lua_dtor_CArrayType);
}

}