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

    int idx = luaL_checkinteger(L, 2);
    if (idx < 0 || idx >= static_cast<int>(ct->array->size)) {
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

static int lua_namecall_CArrayData(lua_State* L)
{
    CData* cd = checkCArrayData(L, 1);
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

    lua_pushcfunction(L, lua_index_CArrayData, (std::string(kCArrayData) + "__index").c_str());
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, lua_tostring_CArrayData, (std::string(kCArrayData) + "__tostring").c_str());
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lua_namecall_CArrayData, (std::string(kCArrayData) + "__namecall").c_str());
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_setuserdatadtor(L, kFFICArrayDataTag, lua_dtor_CArrayData);
}

}