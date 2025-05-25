#include "lute/ffi.h"
#include "lute/ffi/cdata.h"
#include "lute/ffi/dlib.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include "ffi.h"

#include <string>
#include <cstring>

namespace ffi
{

CData* newCFuncData(lua_State* L, CType* type, void* data, bool releasectype, FFIDLHandle* dlib)
{
    api_check(dlib == nullptr || dlib->selfref != LUA_NOREF); // dlib must be a valid FFIDLHandle which was retained before this call

    CData* cd = newCData(L, type, data, releasectype, false, nullptr);
    cd->kind = CDataKind::FUNC;
    cd->funcdata = new CFuncData;
    cd->funcdata->args = static_cast<void**>(malloc(sizeof(void*) * type->func->args.size()));
    cd->funcdata->argstorage = static_cast<void**>(calloc(type->func->args.size(), sizeof(void*)));
    cd->funcdata->retcd = nullptr;
    cd->funcdata->dlib = dlib;

    return cd;
}

CData* toCFuncData(lua_State* L, int idx)
{
    return static_cast<CData*>(lua_touserdatatagged(L, idx, kFFICFuncDataTag));
}

CData* checkCFuncData(lua_State* L, int idx)
{
    CData* cd = toCFuncData(L, idx);
    if (cd != nullptr)
        return cd;

    luaL_typeerror(L, idx, kCFuncData);
    return nullptr;
}
    
static int lua_call_CFuncData(lua_State* L)
{
    CData* cd = checkCFuncData(L, 1);
    CType* ct = cd->type;

    void** args = cd->funcdata->args;
    void** argstorage = cd->funcdata->argstorage;

    for (size_t i = 0; i < ct->func->args.size(); ++i) {
        CType* argtype = ct->func->args[i];

        if (lua_isuserdata(L, i + 2)) {
            CData* arg = toCData(L, i + 2);
            if (arg == nullptr) {
                luaL_argerrorf(L, i + 2, "expected CData at argument %d", (int)i + 2);
            }

            if (argtype->kind == CTypeKind::POINTER && arg->type->kind == CTypeKind::ARRAY) {
                args[i] = &arg->data; // for arrays, we pass the address of the data
            } else {
                args[i] = arg->data;
            }
        } else {
            if (lua_isnoneornil(L, i + 2)) {
                luaL_argerrorf(L, i + 2, "expected CData or value convertible to CData at argument %d", (int)i + 2);
            }

            // it should only be empty the first time we call the function without a CData as argument
            if (argstorage[i] == nullptr) {
                argstorage[i] = malloc(getFFITypeOfCType(argtype)->size);
            }

            if (lua_type(L, i + 2) == LUA_TSTRING && argtype->kind == CTypeKind::POINTER) {
                const char* str = lua_tostring(L, i + 2);
                if (str == nullptr) {
                    luaL_argerrorf(L, i + 2, "expected string at argument %d", (int)i + 2);
                }

                *(char**)argstorage[i] = const_cast<char*>(str);
            } else {
                writeLuaValueToCData(L, i + 2, argstorage[i], argtype);
            }

            args[i] = argstorage[i];
        }
    }
    
    void* ret_data = nullptr;
    CData* retcd = cd->funcdata->retcd;
    CType* ret_type = ct->func->ret;

    if (retcd != nullptr) {
        ret_data = retcd->data;
        // pushCData(L, retcd);
    } else if (ret_type->kind != CTypeKind::VOID) {
        ret_data = malloc(getFFITypeOfCType(ct->func->ret)->size);

        retainCType(L, ret_type);
        newCData(L, ct->func->ret, ret_data, true, true);
    }
    
    ffi_call(const_cast<ffi_cif*>(&ct->func->cif), FFI_FN(cd->data), ret_data, cd->funcdata->args);
    return (retcd == nullptr && ret_data) ? 1 : 0;
}

static int lua_namecall_CFuncData(lua_State* L)
{
    CData* cd = checkCFuncData(L, 1);

    const char* method = lua_namecallatom(L, nullptr);
    if (strcmp(method, "setret") == 0) {
        CData* ret = checkCData(L, 2);
        CType* rettype = ret->type;
        if (rettype->kind != cd->type->func->ret->kind) {
            luaL_argerror(L, 2, "given CData's type does not match the function's return type");
        }

        retainCData(L, 2);
        cd->funcdata->retcd = ret;
        return 0;
    } else if (strcmp(method, "getret") == 0) {
        if (cd->funcdata->retcd == nullptr) {
            lua_pushnil(L);
            return 1;
        }
        pushCData(L, cd->funcdata->retcd);
        return 1;
    }

    return handleCDataNamecall(L, cd);
}

static int lua_tostring_CFuncData(lua_State* L)
{
    CData* cd = checkCData(L, 1);
    return handleCDataToString(L, cd);
}

void lua_dtor_CFuncData(lua_State* L, void* ud)
{
    CData* ct = static_cast<CData*>(ud);
    if (ct->funcdata->retcd) {
        releaseCData(L, ct->funcdata->retcd);
    }
    
    free(ct->funcdata->args);
    for (size_t i = 0; i < ct->type->func->args.size(); ++i) {
        if (ct->funcdata->argstorage[i] != nullptr) {
            free(ct->funcdata->argstorage[i]);
        }
    }
    free(ct->funcdata->argstorage);

    if (ct->funcdata->dlib) {
        releaseFFIDLHandle(L, ct->funcdata->dlib);
    }

    delete ct->funcdata;
    handleCDataDtor(L, ct);
}

void initCFuncData(lua_State* L)
{
    luaL_newmetatable(L, kCFuncData);

    lua_pushvalue(L, -1);
    lua_setuserdatametatable(L, kFFICFuncDataTag);

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, lua_call_CFuncData, "kCFuncData.__call");
    lua_setfield(L, -2, "__call");

    lua_pushcfunction(L, lua_tostring_CFuncData, "kCFuncData.__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lua_namecall_CFuncData, "kCFuncData.__namecall");
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_setuserdatadtor(L, kFFICFuncDataTag, lua_dtor_CFuncData);
}

}