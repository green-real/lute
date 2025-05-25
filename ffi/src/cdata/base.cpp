#include "lute/ffi.h"
#include "lute/ffi/cdata.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include <string>
#include <cstring>

namespace ffi
{

CData* toCBaseData(lua_State* L, int idx)
{
    return static_cast<CData*>(lua_touserdatatagged(L, idx, kFFICBaseDataTag));
}

CData* checkCBaseData(lua_State* L, int idx)
{
    CData* cd = toCBaseData(L, idx);
    if (cd != nullptr)
        return cd;

    luaL_typeerror(L, idx, kCBaseData);
    return nullptr;
}

void writeLuaNumberToCData(lua_State* L, int idx, void* data, CType* ct)
{
    #define WRITE(value, type) \
        *static_cast<type*>(data) = static_cast<type>(value);

    if (ct->kind == CTypeKind::BOOL) {
        int b = lua_toboolean(L, idx);
        WRITE(b, bool);
        return;
    }

    double num = lua_tonumber(L, idx);

    switch (ct->kind) {
    case CTypeKind::CHAR:
        WRITE(num, char);
        break;
    case CTypeKind::SCHAR:
        WRITE(num, signed char);
        break;
    case CTypeKind::UCHAR:
        WRITE(num, unsigned char);
        break;

    case CTypeKind::SHORT:
        WRITE(num, short);
        break;
    case CTypeKind::USHORT:
        WRITE(num, unsigned short);
        break;

    case CTypeKind::INT:
        WRITE(num, int);
        break;
    case CTypeKind::UINT:
        WRITE(num, unsigned int);
        break;

    case CTypeKind::LONG:
        WRITE(num, long);
        break;
    case CTypeKind::ULONG:
        WRITE(num, unsigned long);
        break;

    case CTypeKind::LONGLONG:
        WRITE(num, long long);
        break;
    case CTypeKind::ULONGLONG:
        WRITE(num, unsigned long long);
        break;

    case CTypeKind::INT8_T:
        WRITE(num, int8_t);
        break;
    case CTypeKind::UINT8_T:
        WRITE(num, uint8_t);
        break;
    case CTypeKind::INT16_T:
        WRITE(num, int16_t);
        break;
    case CTypeKind::UINT16_T:
        WRITE(num, uint16_t);
        break;
    case CTypeKind::INT32_T:
        WRITE(num, int32_t);
        break;
    case CTypeKind::UINT32_T:
        WRITE(num, uint32_t);
        break;
    case CTypeKind::INT64_T:
        WRITE(num, int64_t);
        break;
    case CTypeKind::UINT64_T:
        WRITE(num, uint64_t);
        break;

    case CTypeKind::SIZE_T:
        WRITE(num, size_t);
        break;
    case CTypeKind::SSIZE_T:
        WRITE(num, std::make_signed_t<size_t>);
        break;

    case CTypeKind::FLOAT:
        WRITE(num, float);
        break;
    case CTypeKind::DOUBLE:
        WRITE(num, double);
        break;

    default:
        std::abort();
        break;
    }

    #undef WRITE
}

int pushLuaNumberFromCData(lua_State* L, void* data, CType* ct)
{
    #define PUSHNUM(type) \
        lua_pushnumber(L, static_cast<lua_Number>(*reinterpret_cast<type*>(data)));

    switch (ct->kind) {
    case CTypeKind::CHAR: PUSHNUM(char); break;
    case CTypeKind::SCHAR: PUSHNUM(signed char); break;
    case CTypeKind::UCHAR: PUSHNUM(unsigned char); break;
    case CTypeKind::SHORT: PUSHNUM(short); break;
    case CTypeKind::USHORT: PUSHNUM(unsigned short); break;
    case CTypeKind::INT: PUSHNUM(int); break;
    case CTypeKind::UINT: PUSHNUM(unsigned int); break;
    case CTypeKind::LONG: PUSHNUM(long); break;
    case CTypeKind::ULONG: PUSHNUM(unsigned long); break;
    case CTypeKind::LONGLONG: PUSHNUM(long long); break;
    case CTypeKind::ULONGLONG: PUSHNUM(unsigned long long); break;
    case CTypeKind::INT8_T: PUSHNUM(std::int8_t); break;
    case CTypeKind::UINT8_T: PUSHNUM(std::uint8_t); break;
    case CTypeKind::INT16_T: PUSHNUM(std::int16_t); break;
    case CTypeKind::UINT16_T: PUSHNUM(std::uint16_t); break;
    case CTypeKind::INT32_T: PUSHNUM(std::int32_t); break;
    case CTypeKind::UINT32_T: PUSHNUM(std::uint32_t); break;
    case CTypeKind::INT64_T: PUSHNUM(std::int64_t); break;
    case CTypeKind::UINT64_T: PUSHNUM(std::uint64_t); break;

    case CTypeKind::SIZE_T: PUSHNUM(std::size_t); break;
    case CTypeKind::SSIZE_T: PUSHNUM(std::make_signed_t<std::size_t>); break;

    case CTypeKind::FLOAT: PUSHNUM(float); break;
    case CTypeKind::DOUBLE: PUSHNUM(double); break;

    default:
        std::abort();
        break;
    }

    #undef PUSHNUM
    return 1;
}

static int lua_namecall_CBaseData(lua_State* L)
{
    CData* cd = checkCBaseData(L, 1);
    
    const char* method = lua_namecallatom(L, nullptr);
    if (method == nullptr) {
        luaL_error(L, "attempt to namecall CBaseData with invalid method");
    }

    return handleCDataNamecall(L, cd);
}

static int lua_tostring_CBaseData(lua_State* L)
{
    CData* cd = checkCBaseData(L, 1);
    return handleCDataToString(L, cd);
}

void lua_dtor_CBaseData(lua_State* L, void* ud)
{
    CData* ct = static_cast<CData*>(ud);
    handleCDataDtor(L, ct);
}

void initCBaseData(lua_State* L)
{
    luaL_newmetatable(L, kCBaseData);

    lua_pushvalue(L, -1);
    lua_setuserdatametatable(L, kFFICBaseDataTag);

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, lua_tostring_CBaseData, "kCBaseData.__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lua_namecall_CBaseData, "kCBaseData.__namecall");
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_setuserdatadtor(L, kFFICBaseDataTag, lua_dtor_CBaseData);
}

}