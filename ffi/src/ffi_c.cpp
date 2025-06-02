#include "./ffi_c.h"
#include "./utils.h"

#include "lute/ffi/ctype.h"


#include "lua.h"
#include "lualib.h"

#include <array>

namespace ffi
{

static int lua_carray(lua_State* L)
{
    CType* elementType = checkCType(L, 1);
    size_t elementCount = luaL_optinteger(L, 2, 0);

    if (elementType->kind == CTypeKind::Function)
        luaL_argerror(L, 1, "element type cannot be a function type");
    else if (elementType->kind == CTypeKind::Base && elementType->base->kind == CBaseTypeKind::Void)
        luaL_argerror(L, 1, "element type cannot be void");
    else if (elementType->kind == CTypeKind::Array && elementType->array->elementCount == 0)
        luaL_argerror(L, 1, "element type cannot be an incomplete array type");

    retainCType(L, 1);
    newCArrayType(L, elementType, elementCount, true);

    return 1;
}

static int lua_cfunc(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    CType* returnType = checkCType(L, 2);
    if (returnType->kind == CTypeKind::Function)
        luaL_argerror(L, 2, "return type cannot be a function type");
    else if (returnType->kind == CTypeKind::Array)
        luaL_argerror(L, 2, "return type cannot be an array type");

    const char* symbol = luaL_optstring(L, 3, "");

    size_t argCount = lua_objlen(L, 1);
    std::vector<CType*> argumentTypes(argCount);
    for (size_t i = 0; i < argCount; ++i)
    {
        lua_rawgeti(L, 1, i + 1);
        argumentTypes[i] = checkCType(L, -1);
        if (argumentTypes[i]->kind == CTypeKind::Function)
            luaL_argerrorf(L, 1, "argument type #%u cannot be a function type", i + 1);
        else if (argumentTypes[i]->kind == CTypeKind::Base && argumentTypes[i]->base->kind == CBaseTypeKind::Void)
            luaL_argerrorf(L, 1, "argument type #%u cannot be void", i + 1);
    }

    retainCType(L, 2);
    for (size_t i = 0; i < argCount; ++i)
    {
        retainCType(L, -1);
        lua_pop(L, 1);
    }

    newCFunctionType(L, std::move(argumentTypes), returnType, symbol, true);

    return 1;
}

static int lua_cstruct(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    size_t fieldCount = lua_objlen(L, 1);
    std::vector<CType*> fieldTypes(fieldCount);
    std::vector<std::string> fieldNames(fieldCount);
    for (size_t i = 0; i < fieldCount; ++i)
    {
        lua_rawgeti(L, 1, i + 1);
        if (!lua_istable(L, -1))
            luaL_argerrorf(L, 1, "field #%u is not a table", i + 1);

        lua_pushnil(L);
        lua_next(L, -2);
        if (!lua_isstring(L, -2))
            luaL_argerrorf(L, 1, "field name #%u is not a string", i + 1);

        const char* fieldName = lua_tostring(L, -2);

        CType* fieldType = toCType(L, -1);
        if (!fieldType)
            luaL_argerrorf(L, 1, "field type #%u is not a valid CType", i + 1);
        else if (fieldType->kind == CTypeKind::Function)
            luaL_argerrorf(L, 1, "field type #%u cannot be a function type", i + 1);
        else if (fieldType->kind == CTypeKind::Base && fieldType->base->kind == CBaseTypeKind::Void)
            luaL_argerrorf(L, 1, "field type #%u cannot be void", i + 1);
        else if (fieldType->kind == CTypeKind::Array && fieldType->array->elementCount == 0 && i + 1 < fieldCount)
            luaL_argerrorf(L, 1, "field type #%u cannot be an incomplete array type unless it is the last field", i + 1);

        fieldTypes[i] = fieldType;
        fieldNames[i] = fieldName;

        lua_remove(L, -2);
        lua_remove(L, -2);
    }

    for (size_t i = 0; i < fieldCount; ++i)
    {
        retainCType(L, -1);
        lua_pop(L, 1);
    }

    newCRecordType(L, std::move(fieldTypes), std::move(fieldNames), true);
    return 1;
}

static CBaseTypeKind getCBaseTypeKind(size_t size, bool sign)
{
    switch (size)
    {
    case 1:
        return sign ? CBaseTypeKind::Int8 : CBaseTypeKind::UInt8;
    case 2:
        return sign ? CBaseTypeKind::Int16 : CBaseTypeKind::UInt16;
    case 4:
        return sign ? CBaseTypeKind::Int32 : CBaseTypeKind::UInt32;
    case 8:
        return sign ? CBaseTypeKind::Int64 : CBaseTypeKind::UInt64;
    default:
        abort(); // Unsupported size for C base type
    }
}

static void registerCBaseTypes(lua_State* L)
{

#define ADD_TYPE(kind, name) \
    newCBaseType(L, kind, name); \
    lua_setfield(L, -2, name);

#define ADD_CTYPE(type, field, name) \
    newCBaseType(L, getCBaseTypeKind(sizeof(type), std::is_signed_v<type>), name); \
    lua_setfield(L, -2, name);

    ADD_TYPE(CBaseTypeKind::Void, "void");

    ADD_TYPE(CBaseTypeKind::Int8, "int8_t");
    ADD_TYPE(CBaseTypeKind::UInt8, "uint8_t");
    ADD_TYPE(CBaseTypeKind::Int16, "int16_t");
    ADD_TYPE(CBaseTypeKind::UInt16, "uint16_t");
    ADD_TYPE(CBaseTypeKind::Int32, "int32_t");
    ADD_TYPE(CBaseTypeKind::UInt32, "uint32_t");
    ADD_TYPE(CBaseTypeKind::Int64, "int64_t");
    ADD_TYPE(CBaseTypeKind::UInt64, "uint64_t");

    ADD_TYPE(CBaseTypeKind::Float, "float");
    ADD_TYPE(CBaseTypeKind::Double, "double");

    ADD_CTYPE(char, "char", "char");
    ADD_CTYPE(signed char, "schar", "signed char");
    ADD_CTYPE(unsigned char, "uchar", "unsigned char");
    ADD_CTYPE(short, "short", "short");
    ADD_CTYPE(unsigned short, "ushort", "unsigned short");
    ADD_CTYPE(int, "int", "int");
    ADD_CTYPE(unsigned int, "uint", "unsigned int");
    ADD_CTYPE(long, "long", "long");
    ADD_CTYPE(unsigned long, "ulong", "unsigned long");
    ADD_CTYPE(long long, "longlong", "long long");
    ADD_CTYPE(unsigned long long, "ulonglong", "unsigned long long");

#undef ADD_TYPE
#undef ADD_CTYPE
}

static const luaL_Reg clib[] = {
    {"array", lua_carray},
    {"func", lua_cfunc},
    {"struct", lua_cstruct},

    {nullptr, nullptr}
};

int openCInterface(lua_State* L)
{
    initCType(L);

    lua_createtable(L, 0, std::size(clib) - 1);
    luaL_register(L, nullptr, clib);
    registerCBaseTypes(L);

    lua_setreadonly(L, -1, 1);

    return 1;
}

} // namespace ffi
