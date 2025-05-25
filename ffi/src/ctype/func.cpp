#include "lute/ffi/ctype.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include "Luau/Common.h"

#include "lua.h"
#include "lualib.h"

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

namespace ffi
{

static ffi_type* getCFuncArgFFIType(lua_State* L, CType* ct, bool releasectype, bool ret = false)
{
    api_check(ct != nullptr);
    api_check(ct->kind != CTypeKind::FUNC);
    api_check(ct->kind != CTypeKind::VOID || ret);
    api_check(!releasectype || ct->selfref != LUA_NOREF);

    if (ct->kind == CTypeKind::ARRAY) 
        return &ffi_type_pointer;

    // const_cast is safe, because it will never be modified past this point
    return const_cast<ffi_type*>(getFFITypeOfCType(ct));
}

CFuncType::CFuncType(lua_State* L, CType* ret, std::vector<CType*> args, ffi_abi abi, bool releasectype, std::string symbol, int& ffi_status)
    : ret(ret), args(std::move(args)), abi(abi), releasectype(releasectype), symbol(std::move(symbol))
{
    int nargs = this->args.size();

    // validate types and build cif.arg_types
    ffi_type* ret_ft = getCFuncArgFFIType(L, this->ret, this->releasectype, true);
    ffi_type** arg_ftypes = new ffi_type*[nargs];
    for (std::uint8_t i = 0; i < nargs; ++i) {
        arg_ftypes[i] = getCFuncArgFFIType(L, this->args[i], this->releasectype);
    }

    // this sets this->cif.arg_types to arg_ftypes, which is freed by ~CFuncType
    ffi_status = ffi_prep_cif(&this->cif, this->abi, nargs, ret_ft, arg_ftypes);
}

CFuncType::~CFuncType()
{
    delete[] this->cif.arg_types;
}

void CFuncType::releaseDependencies(lua_State* L) const
{
    if (!this->releasectype) return;

    releaseCType(L, this->ret);
    for (CType* arg : this->args) {
        releaseCType(L, arg);
    }
}

// if releasectype, retainCType must have been called on ret and args before calling newCFuncType
CType* newCFuncType(lua_State* L, CType* ret, std::vector<CType*> args, ffi_abi abi, bool releasectype, std::string symbol)
{
    int ffi_status;

    CType* ct = newCType(L, CTypeKind::FUNC, kFFICFuncTypeTag);
    ct->func = new CFuncType(L, ret, std::move(args), abi, releasectype, std::move(symbol), ffi_status);

    if (ffi_status != FFI_OK) {
        luaL_errorL(L, "ffi_prep_cif fail: %s", ffiStatusToString(ffi_status).c_str());
    }

    return ct;
}

CType* toCFuncType(lua_State* L, int idx)
{
    return static_cast<CType*>(lua_touserdatatagged(L, idx, kFFICFuncTypeTag));
}

CType* checkCFuncType(lua_State* L, int idx)
{
    CType* ct = toCFuncType(L, idx);
    if (ct != nullptr)
        return ct;

    luaL_typeerror(L, idx, kCFuncType);
    return nullptr;
}

static int lua_tostring_CFuncType(lua_State* L)
{
    CType* ct = checkCFuncType(L, 1);
    return handleCTypeToString(L, ct);
}

static int lua_namecall_CFuncType(lua_State* L)
{
    CType* ct = checkCFuncType(L, 1);

    const char* method = lua_namecallatom(L, nullptr);
    if (method == nullptr) {
        luaL_error(L, "attempt to namecall CFuncType with invalid method");
    }

    return handleCTypeNamecall(L, ct);
}

void lua_dtor_CFuncType(lua_State* L, void* ud)
{
    CType* ct = static_cast<CType*>(ud);

    ct->func->releaseDependencies(L);
    delete ct->func;
}

const luaL_Reg mt[] = {
    {"__tostring", lua_tostring_CFuncType},
    {"__namecall", lua_namecall_CFuncType},

    {nullptr, nullptr},
};

void initCFuncType(lua_State* L)
{
    luaL_newmetatable(L, kCFuncType);

    lua_pushvalue(L, -1);
    lua_setuserdatametatable(L, kFFICFuncTypeTag);

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, lua_tostring_CFuncType, "kCFuncType.__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lua_namecall_CFuncType, "kCFuncType.__namecall");
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_setuserdatadtor(L, kFFICFuncTypeTag, lua_dtor_CFuncType);
}

}
