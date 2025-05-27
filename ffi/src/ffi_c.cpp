#include "lute/ffi.h"
#include "lute/ffi/ctype.h"
#include "lute/ffi/cdata.h"
#include "lute/ffi/dlib.h"
#include "lute/ffi/utils.h"

#include "lute/runtime.h"

#include "lua.h"
#include "lualib.h"

#include "ffi.h"
#include <array>
#include <string>
#include <cstddef>
#include <memory>
#include <cstring>

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
    luaL_checktype(L, 1, LUA_TTABLE);

    CType* ret = checkCType(L, 2);
    if (ret->kind == CTypeKind::FUNC)
        luaL_argerror(L, 2, "return type should not be a function type");
    
    const char* symbol = luaL_optstring(L, 3, nullptr);

    std::size_t nargs = lua_objlen(L, 1);
    std::vector<CType*> args(nargs);
    for (std::size_t i = 0; i < nargs; ++i) {
        lua_rawgeti(L, 1, i + 1);
        args[i] = toCType(L, -1);
        if (args[i]->kind == CTypeKind::FUNC)
            luaL_argerrorf(L, 1, "at index %d, element type should not be a function type", i + 1);
        if (args[i]->kind == CTypeKind::VOID)
            luaL_argerrorf(L, 1, "at index %d, element type should not not be void", i + 1);
    }

    // now that we validated the arguments, we can safely retain them and make the CFuncType releasectype 
    retainCType(L, 2);
    for (std::size_t i = 0; i < nargs; ++i) {
        retainCType(L, -1);
        lua_pop(L, 1);
    }

    std::string symbol_str;
    if (symbol != nullptr) {
        symbol_str = symbol;
    }

    newCFuncType(L, ret, std::move(args), FFI_DEFAULT_ABI, true, std::move(symbol_str));

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

    // now that we validated the arguments, we can safely retain them and make the CStructType releasectype 
    for (std::size_t i = 0; i < nfields; ++i) {
        retainCType(L, -1);
        lua_pop(L, 1);
    }

    newCStructType(L, std::move(ftypes), std::move(fnames), debugname, true);

    return 1;
}

int lua_cnew(lua_State* L)
{
    CType* ct = checkCType(L, 1);
    if (ct->kind == CTypeKind::FUNC) {
        luaL_argerror(L, 1, "CType cannot be a function type");
    } else if (ct->kind == CTypeKind::VOID) {
        luaL_argerror(L, 1, "CType cannot be void");
    }

    const ffi_type* ft = getFFITypeOfCType(ct);
    std::size_t size = ft->size;
    if (size == 0) {
        luaL_argerror(L, 1, "CType cannot be a zero-sized type");
    }

    retainCType(L, 1);

    void* data = malloc(size);
    memset(data, 0, size);
    
    if (!lua_isnoneornil(L, 2)) {
        writeLuaValueToCData(L, 2, data, ct);
    }

    switch (ct->kind) {
        case CTypeKind::POINTER:
            newCPointerData(L, ct, data, true, true, false, nullptr);
            break;
        default:
            newCData(L, ct, data, true, true);
            break;
    }

    return 1;
}

int lua_csizeof(lua_State* L)
{
    CType* ct = checkCType(L, 1);
    if (ct->kind == CTypeKind::FUNC) {
        luaL_argerror(L, 1, "CFunctionType does not have a size");
    }

    const ffi_type* ft = getFFITypeOfCType(ct);
    lua_pushinteger(L, ft->size);

    return 1;
}

int lua_cload(lua_State* L)
{
    FFIDLHandle* handle = checkFFIDLHandle(L, 1);

    if (lua_istable(L, 2)) {
        lua_newtable(L); // create a new table for the loaded symbols

        lua_pushnil(L); // start iterating over the symbol table
        while (lua_next(L, 2) != 0) {
            const char* field_name = lua_tostring(L, -2);
            if (field_name == nullptr) {
                luaL_argerror(L, 2, "index must be a string");
            }

            CType* ct = checkCType(L, -1);
            if (ct->kind != CTypeKind::FUNC) {
                luaL_argerror(L, 2, "value should be a CFuncType");
            }

            const char* symbol = field_name;
            if (!ct->func->symbol.empty()) {
                symbol = ct->func->symbol.c_str(); // use the symbol from the CFuncType if it exists
            }

            void* addr = getSymbol(handle->handle, symbol);
            if (addr == nullptr) {
                lua_pushnil(L);
            } else {
                retainCType(L, -1);
                retainFFIDLHandle(L, 1);
                newCFuncData(L, ct, addr, true, handle);
            }

            lua_setfield(L, -4, field_name);
            lua_pop(L, 1); // pop the value, keep the key for the next iteration
        }

        return 1;
    } else if (lua_isuserdata(L, 2)) {
        // if the second argument is a CFuncType, we can load the symbol directly
        CType* ct = checkCType(L, 2);
        if (ct->kind != CTypeKind::FUNC) {
            luaL_argerror(L, 2, "value should be a CFuncType");
        }

        const char* symbol = ct->func->symbol.c_str();
        if (symbol == nullptr || *symbol == '\0') {
            luaL_argerror(L, 2, "CFuncType does not have a symbol");
        }

        void* addr = getSymbol(handle->handle, symbol);
        if (addr == nullptr) {
            lua_pushnil(L);
        } else {
            retainCType(L, 2);
            retainFFIDLHandle(L, 1);
            newCFuncData(L, ct, addr, true, handle);
        }

        return 1;
    }

    luaL_typeerror(L, 2, "expected a table or CFuncType");
}

int lua_ccast(lua_State* L)
{
    CData* cd = checkCData(L, 1);
    CType* ct = checkCType(L, 2);

    switch (cd->type->kind) {
        case CTypeKind::FUNC:
            luaL_argerror(L, 1, "cannot cast CFuncData");
            break;
        case CTypeKind::VOID:
            luaL_argerror(L, 1, "cannot cast CData of void type");
            break;
        case CTypeKind::ARRAY:
            if (ct->kind == CTypeKind::ARRAY) {
                retainCType(L, 2);
                retainCData(L, 1);
                newCData(L, ct, cd->data, true, false, cd);
            } else if (ct->kind == CTypeKind::POINTER) {
                retainCType(L, 2);
                retainCData(L, 1);
                newCPointerData(L, ct, &cd->data, true, false, false, cd);
            } else {
                luaL_argerror(L, 2, "cannot cast array to non-array/pointer type");
            }

            break;
        case CTypeKind::STRUCT:
            luaL_argerror(L, 1, "cannot cast CStructData to another type");
            break;
        case CTypeKind::POINTER:
            if (ct->kind == CTypeKind::POINTER) {
                retainCType(L, 2);
                retainCData(L, 1);
                newCPointerData(L, ct, cd->data, true, false, false, cd);
            } else if (ct->kind == CTypeKind::ARRAY) {
                retainCType(L, 2);
                retainCData(L, 1);
                newCData(L, ct, *static_cast<void**>(cd->data), true, false, cd);
            } else {
                luaL_argerror(L, 2, "cannot cast pointer to non-pointer/array type");
            }

            break;
        default:
            // TODO: add support for casting numeric types to other numeric types
            break;
    }

    return 1;
}

int lua_cstring(lua_State* L)
{
    std::size_t len = 0;
    const char* str = luaL_checklstring(L, 1, &len);


    CType* innerct = nullptr;

    if (lua_isboolean(L, 2)) {
        bool sign = lua_toboolean(L, 2);
        if (sign) {
            innerct = newCBaseType(L, CBaseTypeKind::CHAR, &ffi_type_schar);
        } else {
            innerct = newCBaseType(L, CBaseTypeKind::UCHAR, &ffi_type_uchar);
        }
    } else {
        char c = -1;
        innerct = newCBaseType(L, CBaseTypeKind::CHAR, &(c < 0 ? ffi_type_schar : ffi_type_uchar));
    }

    retainCType(L, -1);
    CType* ct = newCPointerType(L, innerct, true);

    void* data = malloc(len + 1);
    memcpy(data, str, len + 1);

    void* ptr = malloc(sizeof(void*));
    *static_cast<void**>(ptr) = data; // store the string data in a pointer

    retainCType(L, -1); // retain the CType
    newCPointerData(L, ct, ptr, true, true, true, nullptr); // create a CPointerData that holds the string data

    return 1;
}

int openCInterface(lua_State* L)
{
    initCArrayType(L);
    initCBaseType(L);
    initCFuncType(L);
    initCPointerType(L);
    initCStructType(L);

    initCArrayData(L);
    initCBaseData(L);
    initCFuncData(L);
    initCPointerData(L);
    initCStructData(L);

    initFFIDLHandle(L);

    lua_createtable(L, 0, std::size(clib) - 1 + std::size(cproperties) + static_cast<std::size_t>(CBaseTypeKind::__COUNT__));
    luaL_register(L, nullptr, clib);
    
#pragma region CBaseTypes
#define ADD_TYPE(name, kind, ft) \
    newCBaseType(L, kind, &ft); \
    lua_setfield(L, -2, name);
#define ADD_TYPE_T(name, kind, type, isSigned) \
    newCBaseType(L, kind, getCIntFFIType(sizeof(type), isSigned)); \
    lua_setfield(L, -2, name);

    ADD_TYPE("bool", CBaseTypeKind::BOOL, ffi_type_sint8);

    char c = -1;
    ADD_TYPE("char", CBaseTypeKind::CHAR, (c < 0 ? ffi_type_schar : ffi_type_uchar));
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

    ADD_TYPE_T("int8_t", CBaseTypeKind::INT8_T, std::int8_t, true);
    ADD_TYPE_T("int16_t", CBaseTypeKind::INT16_T, std::int16_t, true);
    ADD_TYPE_T("int32_t", CBaseTypeKind::INT32_T, std::int32_t, true);
    ADD_TYPE_T("int64_t", CBaseTypeKind::INT64_T, std::int64_t, true);
    ADD_TYPE_T("uint8_t", CBaseTypeKind::UINT8_T, std::uint8_t, false);
    ADD_TYPE_T("uint16_t", CBaseTypeKind::UINT16_T, std::uint16_t, false);
    ADD_TYPE_T("uint32_t", CBaseTypeKind::UINT32_T, std::uint32_t, false);
    ADD_TYPE_T("uint64_t", CBaseTypeKind::UINT64_T, std::uint64_t, false);

    ADD_TYPE_T("size_t", CBaseTypeKind::SIZE_T, std::size_t, false);
    ADD_TYPE_T("ssize_t", CBaseTypeKind::SSIZE_T, std::make_signed_t<std::size_t>, true);

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

