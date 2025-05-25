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
    
    const char* method = lua_namecallatom(L, nullptr);
    if (method == nullptr) {
        luaL_error(L, "attempt to namecall CPointerData with invalid method");
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
    CData* ct = static_cast<CData*>(ud);
    delete ct->ptrdata; // TODO: handle innermanaged
    handleCDataDtor(L, ct);
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