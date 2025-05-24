#include "lute/ffi/ctype.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include "lua.h"
#include "lualib.h"

namespace ffi
{

CType* newCBaseType(lua_State* L, CBaseTypeKind kind, const ffi_type* ft)
{
    api_check(kind < CBaseTypeKind::__COUNT__);
    api_check(ft != nullptr);

    CType* ct = newCType(L, static_cast<CTypeKind>(kind), kFFICBaseTypeTag);
    ct->ft = ft;
    return ct;
}

CType* toCBaseType(lua_State* L, int idx)
{
    return static_cast<CType*>(lua_touserdatatagged(L, idx, kFFICBaseTypeTag));
}

CType* checkCBaseType(lua_State* L, int idx)
{
    CType* ct = toCBaseType(L, idx);
    if (ct != nullptr)
        return ct;
    
    luaL_typeerror(L, idx, kCBaseType);
    return nullptr;
}

static int lua_tostring_CBaseType(lua_State* L)
{
    CType* ct = checkCBaseType(L, 1);
    return handleCTypeToString(L, ct);
}

static int lua_namecall_CBaseType(lua_State* L)
{
    CType* ct = checkCBaseType(L, 1);
    const char* method = lua_namecallatom(L, nullptr);
    if (method == nullptr)
        luaL_error(L, "attempt to namecall CBaseType with invalid method");

    return handleCTypeNamecall(L, ct);
}

const luaL_Reg mt[] = {
    {"__tostring", lua_tostring_CBaseType},
    {"__namecall", lua_namecall_CBaseType},

    {nullptr, nullptr},
};

void initCBaseType(lua_State* L)
{
    luaL_newmetatable(L, kCBaseType);

    lua_pushvalue(L, -1);
    lua_setuserdatametatable(L, kFFICBaseTypeTag);

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, lua_tostring_CBaseType, (std::string(kCBaseType) + "__tostring").c_str());
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lua_namecall_CBaseType, (std::string(kCBaseType) + "__namecall").c_str());
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);
}

const ffi_type* getCIntFFIType(std::size_t size, bool isSigned)
{
    switch (size)
    {
    case 8:
        return isSigned ? &ffi_type_sint64 : &ffi_type_uint64;
    case 4:
        return isSigned ? &ffi_type_sint32 : &ffi_type_uint32;
    case 2:
        return isSigned ? &ffi_type_sint16 : &ffi_type_uint16;
    default:
        return isSigned ? &ffi_type_sint8 : &ffi_type_uint8;
    }
}

}