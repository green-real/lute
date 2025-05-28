#include "lute/ffi.h"
#include "lute/ffi/cdata.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include <string>
#include <cstring>

namespace ffi
{

CData* toCStructData(lua_State* L, int idx)
{
    return static_cast<CData*>(lua_touserdatatagged(L, idx, kFFICStructDataTag));
}

CData* checkCStructData(lua_State* L, int idx)
{
    CData* cd = toCStructData(L, idx);
    if (cd != nullptr)
        return cd;

    luaL_typeerror(L, idx, kCStructData);
    return nullptr;
}

void writeLuaTableToCStruct(lua_State* L, int idx, void* data, CType* ct)
{
    size_t size = getFFITypeOfCType(ct)->size;

    api_check(ct->kind == CTypeKind::STRUCT);
    api_check(ct->struct_->ft.size == size);

    size_t offset = 0;

    // check if the table is an array or a dictionary
    int objlen = lua_objlen(L, idx);
    if (objlen == 0) {
        for (const auto& field : ct->struct_->fields) {
            size_t field_size = getFFITypeOfCType(field.type)->size;

            lua_getfield(L, idx, field.name.c_str());
            if (lua_isnil(L, -1)) {
                luaL_argerrorf(L, idx, "table is missing field '%s'", field.name.c_str());
            }
            writeLuaValueToCData(L, -1, static_cast<char*>(data) + offset, field.type);
            lua_pop(L, 1);
            offset += field_size;
        }
    } else {
        if (objlen != (int)ct->struct_->fields.size()) {
            luaL_argerrorf(L, idx, "table has incorrect number of fields (%d fields expected)", (int)ct->struct_->fields.size());
        }

        for (int i = 0; i < objlen; ++i) {
            CType* fieldtype = ct->struct_->fields[i].type;
            size_t field_size = getFFITypeOfCType(fieldtype)->size;
            lua_rawgeti(L, idx, i + 1);
            if (lua_isnil(L, -1)) {
                luaL_argerrorf(L, idx, "table is missing field at index %d", i + 1);
            }

            writeLuaValueToCData(L, -1, static_cast<char*>(data) + offset, fieldtype);
            lua_pop(L, 1);
            offset += field_size;
        }
    }

}

static int lua_index_CStructData(lua_State* L)
{
    CData* cd = checkCStructData(L, 1);
    CType* ct = cd->type;

    if (lua_type(L, 2) == LUA_TSTRING) {
        const char* field_name = lua_tostring(L, 2);
        if (field_name == nullptr) {
            luaL_argerror(L, 2, "field name must be a string");
        }

        auto it = ct->struct_->field_map.find(field_name);
        if (it != ct->struct_->field_map.end()) {
            size_t offset = ct->struct_->fields[it->second].offset;
            CType* field_type = ct->struct_->fields[it->second].type;

            void* field_data = static_cast<char*>(cd->data) + offset;

            retainCData(L, 1);
            retainCType(L, field_type);
            if (field_type->kind == CTypeKind::POINTER) {
                newCPointerData(L, field_type, field_data, true, false, false, cd);
            } else {
                newCData(L, field_type, field_data, true, false, cd);
            }

            return 1;
        }        
    }

    return 0;
}

static int lua_namecall_CStructData(lua_State* L)
{
    CData* cd = checkCStructData(L, 1);

    const char* method = lua_namecallatom(L, nullptr);
    if (method == nullptr) {
        luaL_error(L, "attempt to namecall CStructData with invalid method");
    }

    return handleCDataNamecall(L, cd);
}

static int lua_tostring_CStructData(lua_State* L)
{
    CData* cd = checkCStructData(L, 1);
    return handleCDataToString(L, cd);
}

void lua_dtor_CStructData(lua_State* L, void* ud)
{
    CData* ct = static_cast<CData*>(ud);
    handleCDataDtor(L, ct);
}

void initCStructData(lua_State* L)
{
    luaL_newmetatable(L, kCStructData);

    lua_pushvalue(L, -1);
    lua_setuserdatametatable(L, kFFICStructDataTag);

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, lua_index_CStructData, "kCStructData.__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, lua_namecall_CStructData, "kCStructData.__namecall");
    lua_setfield(L, -2, "__namecall");

    lua_pushcfunction(L, lua_tostring_CStructData, "kCStructData.__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pushstring(L, kCStructData);
    lua_setfield(L, -2, "__type");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_setuserdatadtor(L, kFFICStructDataTag, lua_dtor_CStructData);
}

}