#include "lute/ffi.h"
#include "lute/ffi/cdata.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include <string>
#include <cstring>

namespace ffi
{

CData* newCPointerData(lua_State* L, CType* type, void* data, bool releasectype, bool managed, bool innermanaged, CData* dependent)
{
    api_check(type->kind == CTypeKind::POINTER);

    CData* cd = newCData(L, type, data, releasectype, managed, dependent);
    cd->kind = CDataKind::POINTER;
    cd->ptrdata = new CPointerData;
    cd->ptrdata->innermanaged = innermanaged;

    return cd;
}

CData* toCPointerData(lua_State* L, int idx)
{
    return static_cast<CData*>(lua_touserdatatagged(L, idx, kFFICPointerDataTag));
}

CData* checkCPointerData(lua_State* L, int idx)
{
    CData* cd = toCPointerData(L, idx);
    if (cd != nullptr)
        return cd;

    luaL_typeerror(L, idx, kCPointerData);
    return nullptr;
}

static int lua_index_CPointerData(lua_State* L)
{
    CData* cd = checkCPointerData(L, 1);
    CType* ct = cd->type;
    CType* innerct = ct->ptr->innertype;
    if (innerct->kind == CTypeKind::FUNC) {
        luaL_error(L, "cannot index a function pointer");
    } else if (innerct->kind == CTypeKind::VOID) {
        luaL_error(L, "cannot index a void pointer");
    }

    void* ptr = *static_cast<void**>(cd->data);

    int idx = luaL_checkinteger(L, 2);
    std::size_t elemsize = getFFITypeOfCType(innerct)->size;
    void* elemdata = static_cast<char*>(ptr) + idx * elemsize;

    retainCData(L, 1);
    retainCType(L, innerct);
    if (innerct->kind == CTypeKind::POINTER) {
        newCPointerData(L, innerct, elemdata, true, false, false, cd);
    } else {
        newCData(L, innerct, elemdata, true, false, cd);
    }

    return 1;
}

static int lua_namecall_CPointerData(lua_State* L)
{
    CData* cd = checkCPointerData(L, 1);
    CType* ct = cd->type;
    
    const char* method = lua_namecallatom(L, nullptr);
    if (method == nullptr) {
        luaL_error(L, "attempt to namecall CPointerData with invalid method");
    }

    if (strcmp(method, "deref") == 0) {
        int index = luaL_optinteger(L, 2, 0);
        if (index < 0) {
            luaL_argerror(L, 2, "index must be a non-negative integer");
        }

        CType* innerct = ct->ptr->innertype;
        if (innerct->kind == CTypeKind::VOID) {
            luaL_error(L, "cannot dereference a void pointer");
        }

        void* ptr = *static_cast<void**>(cd->data);
        std::size_t elemsize = innerct->kind == CTypeKind::FUNC ? 0 : getFFITypeOfCType(innerct)->size;
        void* elemdata = static_cast<char*>(ptr) + index * elemsize;

        retainCType(L, innerct);
        retainCData(L, 1);
        if (innerct->kind == CTypeKind::POINTER) {
            newCPointerData(L, innerct, elemdata, true, false, false, cd);
        } else if (innerct->kind == CTypeKind::FUNC) {
            newCFuncData(L, innerct, elemdata, true);
        } else {
            newCData(L, innerct, elemdata, true, false, cd);
        }

        return 1;
    } else if (strcmp(method, "string") == 0) {
        CType* innerct = ct->ptr->innertype;
        if (innerct->kind != CTypeKind::CHAR && innerct->kind != CTypeKind::UCHAR && innerct->kind != CTypeKind::SCHAR) {
            luaL_argerrorf(L, 2, "attempt to convert %s<%s> to string, but inner type is not a character type",
                           getUDNameCType(ct).c_str(), toStringCType(ct).c_str());
        }

        const char* str = *static_cast<const char**>(cd->data);
        std::size_t len = strlen(str);

        lua_pushlstring(L, str, len);
        return 1;
    } else if (strcmp(method, "isnull") == 0) {
        void* ptr = *static_cast<void**>(cd->data);
        lua_pushboolean(L, ptr == nullptr);
        return 1;
    }

    return handleCDataNamecall(L, cd);
}

static int lua_tostring_CPointerData(lua_State* L)
{
    CData* cd = checkCPointerData(L, 1);
    return handleCDataToString(L, cd);
}

void lua_dtor_CPointerData(lua_State* L, void* ud)
{
    CData* cd = static_cast<CData*>(ud);
    api_check(cd->kind == CDataKind::POINTER);

    if (cd->ptrdata->innermanaged) {
        void* ptr = *static_cast<void**>(cd->data);
        if (ptr != nullptr) {
            free(ptr); // free the inner pointer if it is managed
        }
    }

    delete cd->ptrdata; // TODO: handle innermanaged
    handleCDataDtor(L, cd);
}

void initCPointerData(lua_State* L)
{
    luaL_newmetatable(L, kCPointerData);

    lua_pushvalue(L, -1);
    lua_setuserdatametatable(L, kFFICPointerDataTag);

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, lua_index_CPointerData, "kCPointerData.__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, lua_tostring_CPointerData, "kCPointerData.__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lua_namecall_CPointerData, "kCPointerData.__namecall");
    lua_setfield(L, -2, "__namecall");

    lua_pushstring(L, kCPointerData);
    lua_setfield(L, -2, "__type");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_setuserdatadtor(L, kFFICPointerDataTag, lua_dtor_CPointerData);
}

}