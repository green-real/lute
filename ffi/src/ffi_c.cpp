#include "lute/ffi.h"
#include "lute/ffi/ctype.h"
#include "lute/ffi/utils.h"

#include "lute/runtime.h"

#include "lua.h"
#include "lualib.h"

#include "ffi.h"
#include <array>
#include <string>
#include <cstddef>
#include <memory>

namespace ffi
{

int lua_carray(lua_State* L)
{
    CType* ct = checkCType(L, 1);
    std::size_t size = luaL_checkinteger(L, 2);
    
    if (ct->kind == CTypeKind::FUNC)
        luaL_argerror(L, 1, "element type should not be a function type");
    if (ct->kind == CTypeKind::VOID)
        luaL_argerror(L, 1, "element type should not not be void");
    
    retainCType(L, 1);
    newCArrayType(L, ct, size, true);

    return 1;
}

int lua_cfunc(lua_State* L)
{
    CType* ret = checkCType(L, 1);
    if (ret->kind == CTypeKind::FUNC)
        luaL_argerror(L, 1, "return type should not be a function type");

    luaL_checktype(L, 2, LUA_TTABLE);

    std::size_t nargs = lua_objlen(L, 2);
    std::vector<CType*> args(nargs);
    for (std::size_t i = 0; i < nargs; ++i) {
        lua_rawgeti(L, 2, i + 1);
        args[i] = toCType(L, -1);
        if (args[i]->kind == CTypeKind::FUNC)
            luaL_argerrorf(L, 2, "at index %d, element type should not be a function type", i + 1);
        if (args[i]->kind == CTypeKind::VOID)
            luaL_argerrorf(L, 2, "at index %d, element type should not not be void", i + 1);
    }

    // now that we validated the arguments, we can safely retain them and make the CFuncType dependant 
    retainCType(L, 1);
    for (std::size_t i = 0; i < nargs; ++i) {
        retainCType(L, -1);
        lua_pop(L, 1);
    }
    
    newCFuncType(L, ret, std::move(args), FFI_DEFAULT_ABI, true);

    return 1;
}

int lua_cstruct(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    const char* debugname = luaL_optstring(L, 2, "");

    std::size_t nfields = lua_objlen(L, 1);
    std::vector<CType*> ftypes(nfields);
    std::vector<std::string> fnames(nfields);
    for (std::size_t i = 0; i < nfields; ++i) {
        lua_rawgeti(L, 1, i + 1);
        luaL_checktype(L, -1, LUA_TTABLE);
        
        lua_rawgeti(L, -1, 1);
        const char* name = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_rawgeti(L, -1, 2);
        CType* type = toCType(L, -1);

        if (name == nullptr)
            luaL_argerrorf(L, 1, "at index %d, field name must be a string", i + 1);

        if (type == nullptr)
            luaL_argerrorf(L, 1, "at index %d, field type must be a CType", i + 1);
        else if (type->kind == CTypeKind::FUNC)
            luaL_argerrorf(L, 1, "at index %d, field type should not be a function type", i + 1);
        else if (type->kind == CTypeKind::VOID)
            luaL_argerrorf(L, 1, "at index %d, field type should not be void", i + 1);

        fnames[i] = name;
        ftypes[i] = type;

        lua_remove(L, -2); // only remove the array, keep the CType on the stack
    }

    // now that we validated the arguments, we can safely retain them and make the CStructType dependant 
    for (std::size_t i = 0; i < nfields; ++i) {
        retainCType(L, -1);
        lua_pop(L, 1);
    }

    newCStructType(L, std::move(ftypes), std::move(fnames), debugname, true);

    return 1;
}

int lua_ctest(lua_State* L)
{
    lua_pushboolean(L, true);

    return 1;
}

int openCInterface(lua_State* L)
{
    initCArrayType(L);
    initCBaseType(L);
    initCFuncType(L);
    initCPointerType(L);
    initCStructType(L);

    lua_createtable(L, 0, std::size(clib) - 1 + std::size(cproperties) + (size_t)CBaseTypeKind::__COUNT__);
    luaL_register(L, nullptr, clib);
    
#pragma region CBaseTypes
#define ADD_TYPE(name, kind, ft) \
    newCBaseType(L, kind, &ft); \
    lua_setfield(L, -2, name);
#define ADD_TYPE_T(name, kind, type, isSigned) \
    newCBaseType(L, kind, getCIntFFIType(sizeof(type), isSigned)); \
    lua_setfield(L, -2, name);

    ADD_TYPE("bool", CBaseTypeKind::BOOL, ffi_type_sint8);

    ADD_TYPE("char", CBaseTypeKind::CHAR, ((char)-1 < 0 ? ffi_type_schar : ffi_type_uchar));
    ADD_TYPE("schar", CBaseTypeKind::SCHAR, ffi_type_schar);
    ADD_TYPE("uchar", CBaseTypeKind::UCHAR, ffi_type_uchar);

    ADD_TYPE("short", CBaseTypeKind::SHORT, ffi_type_sshort);
    ADD_TYPE("ushort", CBaseTypeKind::USHORT, ffi_type_ushort);

    ADD_TYPE("int", CBaseTypeKind::INT, ffi_type_sint);
    ADD_TYPE("uint", CBaseTypeKind::UINT, ffi_type_uint);

    ADD_TYPE("long", CBaseTypeKind::LONG, ffi_type_slong);
    ADD_TYPE("ulong", CBaseTypeKind::ULONG, ffi_type_ulong);

    ADD_TYPE_T("llong", CBaseTypeKind::LONGLONG, long long, true);
    ADD_TYPE_T("ullong", CBaseTypeKind::ULONGLONG, unsigned long long, false);

    ADD_TYPE_T("int8_t", CBaseTypeKind::INT8_T, int8_t, true);
    ADD_TYPE_T("int16_t", CBaseTypeKind::INT16_T, int16_t, true);
    ADD_TYPE_T("int32_t", CBaseTypeKind::INT32_T, int32_t, true);
    ADD_TYPE_T("int64_t", CBaseTypeKind::INT64_T, int64_t, true);
    ADD_TYPE_T("uint8_t", CBaseTypeKind::UINT8_T, uint8_t, false);
    ADD_TYPE_T("uint16_t", CBaseTypeKind::UINT16_T, uint16_t, false);
    ADD_TYPE_T("uint32_t", CBaseTypeKind::UINT32_T, uint32_t, false);
    ADD_TYPE_T("uint64_t", CBaseTypeKind::UINT64_T, uint64_t, false);

    ADD_TYPE_T("size_t", CBaseTypeKind::SIZE_T, size_t, false);
    ADD_TYPE_T("ssize_t", CBaseTypeKind::SSIZE_T, ssize_t, true);

    ADD_TYPE("float", CBaseTypeKind::FLOAT, ffi_type_float);
    ADD_TYPE("double", CBaseTypeKind::DOUBLE, ffi_type_double);

    ADD_TYPE("void", CBaseTypeKind::VOID, ffi_type_void);

#undef ADD_TYPE
#undef ADD_TYPE_T
#pragma endregion
    
    lua_setreadonly(L, -1, 1);

    return 1;
}

} // namespace ffi

