#include "lute/ffi.h"
#include "lute/ffi/cdata.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include <string>
#include <cstring>

namespace ffi
{

CData* toCArrayData(lua_State* L, int idx)
{
    return static_cast<CData*>(lua_touserdatatagged(L, idx, kFFICArrayDataTag));
}

CData* checkCArrayData(lua_State* L, int idx)
{
    CData* cd = toCArrayData(L, idx);
    if (cd != nullptr)
        return cd;

    luaL_typeerror(L, idx, kCArrayData);
    return nullptr;
}

void writeLuaTableToCArray(lua_State* L, int idx, void* data, CType* ct)
{
    if (ct->kind != CTypeKind::ARRAY) {
        luaL_argerrorf(L, idx, "CType must be an array type to write a Lua table to it");
    }

    CType* elemtype = ct->array->elemtype;

    std::size_t size = getFFITypeOfCType(ct)->size;
    std::size_t elemsize = getFFITypeOfCType(elemtype)->size;
    std::size_t numelems = size / elemsize;

    for (std::size_t i = 0; i < numelems; ++i) {
        lua_rawgeti(L, idx, i + 1);
        if (lua_isnil(L, -1)) {
            luaL_argerrorf(L, idx, "table has fewer elements than expected (%d elements expected)", (int)numelems);
        }
        writeLuaValueToCData(L, -1, static_cast<char*>(data) + i * elemsize, ct->array->elemtype);
        lua_pop(L, 1);
    }
} 

static int lua_index_CArrayData(lua_State* L)
{
    CData* cd = checkCArrayData(L, 1);
    CType* ct = cd->type;

    if (lua_isnumber(L, 2)) {
        int idx = luaL_checkinteger(L, 2);
        if ((double)idx != lua_tonumber(L, 2)) {
            luaL_argerror(L, 2, "index must be an integer");
        } else if (idx < 0 || idx >= static_cast<int>(ct->array->size)) {
            luaL_argerrorf(L, 2, "index %d out of bounds for array of size %d", idx, (int)ct->array->size);
        }

        std::size_t elemsize = getFFITypeOfCType(ct->array->elemtype)->size;
        void* elemdata = static_cast<char*>(cd->data) + idx * elemsize;

        retainCData(L, 1);
        retainCType(L, ct->array->elemtype);
        if (ct->array->elemtype->kind == CTypeKind::POINTER) {
            newCPointerData(L, ct->array->elemtype, elemdata, true, false, false, cd);
        } else {
            newCData(L, ct->array->elemtype, elemdata, true, false, cd);
        }

        return 1;
    }

    return handleCDataIndex(L, cd);
}

static int lua_namecall_CArrayData(lua_State* L)
{
    CData* cd = checkCArrayData(L, 1);
    CType* ct = cd->type;

    const char* method = lua_namecallatom(L, nullptr);
    if (method == nullptr) {
        luaL_error(L, "attempt to namecall CArrayData with invalid method");
    }

    if (strcmp(method, "string") == 0) {
        CType* elemtype = ct->array->elemtype;
        if (elemtype->kind != CTypeKind::CHAR && elemtype->kind != CTypeKind::UCHAR && elemtype->kind != CTypeKind::SCHAR) {
            luaL_argerrorf(L, 2, "attempt to convert %s<%s> to string, but element type is not a character type",
                          getUDNameCType(ct).c_str(), toStringCType(ct).c_str());
        }

        const char* str = static_cast<const char*>(cd->data);
        std::size_t len = strlen(str);
        if (len > ct->array->size) {
            len = ct->array->size; // ensure we don't read beyond the array size
        }

        lua_pushlstring(L, str, len);
        return 1;
    }

    return handleCDataNamecall(L, cd);
}

static int lua_tostring_CArrayData(lua_State* L)
{
    CData* cd = checkCArrayData(L, 1);
    return handleCDataToString(L, cd);
}

void lua_dtor_CArrayData(lua_State* L, void* ud)
{
    CData* ct = static_cast<CData*>(ud);
    handleCDataDtor(L, ct);
}

void initCArrayData(lua_State* L)
{
    luaL_newmetatable(L, kCArrayData);

    lua_pushvalue(L, -1);
    lua_setuserdatametatable(L, kFFICArrayDataTag);

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, lua_index_CArrayData, "CArrayData.__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, lua_tostring_CArrayData, "kCArrayData.__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lua_namecall_CArrayData, "kCArrayData.__namecall");
    lua_setfield(L, -2, "__namecall");

    lua_pushstring(L, kCArrayData);
    lua_setfield(L, -2, "__type");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_setuserdatadtor(L, kFFICArrayDataTag, lua_dtor_CArrayData);
}

}