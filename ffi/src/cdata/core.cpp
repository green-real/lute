#include "lute/ffi.h"
#include "lute/ffi/state.h"
#include "lute/ffi/ctype.h"
#include "lute/ffi/cdata.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include "lua.h"
#include "lualib.h"

#include <cstring>
#include <string>
#include <sstream>

namespace ffi
{

CData* newCData(lua_State* L, CType* type, void* data, bool releasectype, bool managed, CData* dependent)
{
    int utag = getCDataUTagFromCType(type);

    CData* cd = static_cast<CData*>(lua_newuserdatataggedwithmetatable(L, sizeof(CData), utag));
    cd->kind = CDataKind::DATA; // may be changed to FUNC or POINTER by newCFuncData or newCPointerData
    cd->releasectype = releasectype;
    cd->managed = managed;
    cd->isvalid = true; // CData is valid by default, can be set to false later

    cd->refcount = 0;
    cd->selfref = LUA_NOREF;

    cd->type = type;
    cd->dependent = dependent;
    cd->data = data;
    
    lua_rawgetfield(L, LUA_REGISTRYINDEX, kFFIWeakRegistryKey);
    lua_pushlightuserdata(L, cd);
    lua_pushvalue(L, -3);
    lua_settable(L, -3);
    lua_pop(L, 1);

    return cd;
}

CData* toCData(lua_State* L, int idx)
{
    switch (lua_userdatatag(L, idx)) {
    case kFFICArrayDataTag:
    case kFFICBaseDataTag:
    case kFFICFuncDataTag:
    case kFFICPointerDataTag:
    case kFFICStructDataTag:
        break;
    default:
        return nullptr;
    }

    return static_cast<CData*>(lua_touserdata(L, idx));
}

CData* checkCData(lua_State* L, int idx)
{
    CData* cd = toCData(L, idx);
    if (cd != nullptr)
        return cd;

    luaL_typeerror(L, idx, kCData);
    return nullptr;
}

CData* checkCData(lua_State* L, int idx, CDataKind kind)
{
    CData* cd = toCData(L, idx);
    if (cd != nullptr)
        return cd;

    luaL_typeerror(L, idx, kCData);
    return nullptr;
}

void initCData(lua_State* L)
{
    initCArrayData(L);
    initCBaseData(L);
    initCFuncData(L);
    initCPointerData(L);
    initCStructData(L);
}

// increments the refcount and retains the CData
void retainCData(lua_State* L, int idx)
{
    CData* cd = toCData(L, idx);
    api_check(cd != nullptr);
    api_check(cd->refcount != 0 || cd->selfref == LUA_NOREF);

    cd->refcount++;
    if (cd->selfref == LUA_NOREF) {
        cd->selfref = lua_ref(L, idx);
    }
}

// retains a CData that is currently retained, so that it is not garbage collected until it is released
// this should only be used when the CData is already retained
void retainCData(lua_State* L, CData* cd)
{
    api_check(cd != nullptr);
    api_check(cd->refcount != 0);
    api_check(cd->selfref != LUA_NOREF);

    cd->refcount++;
}

// releases a CData previously retained by retainCData
void releaseCData(lua_State* L, CData* cd)
{
    api_check(cd != nullptr);
    api_check(cd->refcount > 0);
    api_check(cd->selfref != LUA_NOREF);

    cd->refcount--;
    if (cd->refcount == 0) {
        FFIState* ffiState = getFFIState(L);
        api_check(ffiState != nullptr);

        ffiState->addPendingUnref(cd->selfref);   
        cd->selfref = LUA_NOREF;
    }
}

// attempts to find the given CData in the weak registry and puts it on the stack
// returns true if the CData was found
bool pushCData(lua_State* L, CData* cd)
{
    lua_rawgetfield(L, LUA_REGISTRYINDEX, kFFIWeakRegistryKey);
    lua_pushlightuserdata(L, cd);
    int type = lua_rawget(L, -2);
    lua_remove(L, -2);

    return type != LUA_TNIL;
}

void writeLuaValueToCData(lua_State* L, int idx, void* data, CType* ct)
{
    size_t size = getFFITypeOfCType(ct)->size;

    CTypeKind kind = ct->kind;
    int ltype = lua_type(L, idx);

    switch (ltype) {
    case LUA_TBOOLEAN: {
        if (kind > CTypeKind::SSIZE_T) {
            luaL_argerrorf(L, idx, "cannot write boolean to %s<%s>", getUDNameCType(ct).c_str(), toStringCType(ct).c_str());
        }

        writeLuaNumberToCData(L, idx, data, ct);

        break;
    }
    case LUA_TNUMBER: {
        writeLuaNumberToCData(L, idx, data, ct);
        break;
    }
    case LUA_TBUFFER: {
        size_t len;
        void* src = lua_tobuffer(L, idx, &len);
        if (len > size) {
            luaL_argerrorf(L, idx, "buffer length %d is higher than expected size %d for %s<%s>", (int)len, (int)size, getUDNameCType(ct).c_str(), toStringCType(ct).c_str());
        }

        memcpy(data, src, len);
        break;
    }
    case LUA_TSTRING: {
        if (kind != CTypeKind::ARRAY || !(ct->array->elemtype->kind >= CTypeKind::CHAR && ct->array->elemtype->kind <= CTypeKind::UCHAR)) {
            luaL_argerrorf(L, idx, "cannot write string to %s<%s>", getUDNameCType(ct).c_str(), toStringCType(ct).c_str());
        }

        // TODO: is this length including the null terminator?
        size_t len;
        const char* str = lua_tolstring(L, idx, &len);

        if (len > size) {
            luaL_argerrorf(L, idx, "string length %d is higher than expected size %d for %s<%s>", (int)len, (int)size, getUDNameCType(ct).c_str(), toStringCType(ct).c_str());
        }

        memcpy(data, str, len);
        break;
    }
    case LUA_TTABLE: {
        switch (kind) {
        case CTypeKind::ARRAY: {
            writeLuaTableToCArray(L, idx, data, ct);
            break;
        }
        case CTypeKind::STRUCT: {
            writeLuaTableToCStruct(L, idx, data, ct);
            break;
        }
        default:
            luaL_argerrorf(L, idx, "cannot write table to %s<%s>", getUDNameCType(ct).c_str(), toStringCType(ct).c_str());
            break;
        }
        break;
    }
    case LUA_TUSERDATA: {
        CData* cd = toCData(L, idx);
        if (cd == nullptr) {
            luaL_argerrorf(L, idx, "cannot write %s to %s<%s>", luaL_typename(L, idx), getUDNameCType(ct).c_str(), toStringCType(ct).c_str());
        }
        
        size_t cdsize = getFFITypeOfCType(cd->type)->size;
        if (cdsize != size) {
            luaL_argerrorf(L, idx, "CData size %d is not equal to size %d for %s<%s>", (int)cdsize, (int)size, getUDNameCType(ct).c_str(), toStringCType(ct).c_str());
        }

        memcpy(data, cd->data, size);
        break;
    }
    case LUA_TNIL: {
        if (kind == CTypeKind::POINTER) {
            // writing nil to a pointer is allowed, it will set the pointer to nullptr
            memset(data, 0, size);
            return;
        }

        luaL_argerrorf(L, idx, "cannot write nil to %s<%s>", getUDNameCType(ct).c_str(), toStringCType(ct).c_str());
        break;
    };
    default:
        luaL_argerrorf(L, idx, "cannot write %s to %s<%s>", luaL_typename(L, idx), getUDNameCType(ct).c_str(), toStringCType(ct).c_str());
        break;
    }   
}

int pushLuaValueFromCData(lua_State* L, void* data, CType* ct, int cdataidx)
{
    size_t size = getFFITypeOfCType(ct)->size;

    if (ct->kind == CTypeKind::BOOL) {
        bool b = *static_cast<bool*>(data);
        lua_pushboolean(L, b);
        return 1;
    } else if (ct->kind <= CTypeKind::DOUBLE) {
        return pushLuaNumberFromCData(L, data, ct);
    } else if (ct->kind == CTypeKind::ARRAY) {
        CType* elemtype = ct->array->elemtype;
        size_t elemsize = getFFITypeOfCType(elemtype)->size;
        size_t numelems = size / elemsize;

        lua_createtable(L, static_cast<int>(numelems), 0);
        for (size_t i = 0; i < numelems; ++i) {
            void* elemdata = static_cast<char*>(data) + i * elemsize;
            retainCType(L, elemtype); // retain the element type

            CData* cd = nullptr;
            if (cdataidx) {
                retainCData(L, cdataidx);
                cd = toCData(L, cdataidx);

                api_check(cd != nullptr);
            }

            if (elemtype->kind == CTypeKind::POINTER) {
                newCPointerData(L, elemtype, elemdata, true, false, false, cd);
            } else {
                newCData(L, elemtype, elemdata, true, false, cd);
            }

            lua_rawseti(L, -2, static_cast<int>(i + 1));
        }

        return 1;
    } else if (ct->kind == CTypeKind::STRUCT) {
        lua_createtable(L, 0, static_cast<int>(ct->struct_->fields.size()));
        for (const auto& field : ct->struct_->fields) {
            void* fielddata = static_cast<char*>(data) + field.offset;
            retainCType(L, field.type); // retain the field type

            CData* cd = nullptr;
            if (cdataidx) {
                retainCData(L, cdataidx);
                cd = toCData(L, cdataidx);
                api_check(cd != nullptr);
            }

            if (field.type->kind == CTypeKind::POINTER) {
                newCPointerData(L, field.type, fielddata, true, false, false, cd);
            } else {
                newCData(L, field.type, fielddata, true, false, cd);
            }
            lua_setfield(L, -2, field.name.c_str());
        }
        return 1;
    } else if (ct->kind == CTypeKind::POINTER) {
        
        void** ptr = static_cast<void**>(data);
        void** buf = static_cast<void**>(lua_newbuffer(L, size));
        *buf = *ptr;

        return 1;
    } else {
        luaL_error(L, "cannot read %s<%s> as Lua value", getUDNameCType(ct).c_str(), toStringCType(ct).c_str());
    }

}

int handleCDataIndex(lua_State* L, CData* cd)
{
    api_check(cd != nullptr);
    api_check(cd->type != nullptr);

    luaL_error(L, "attempt to index %s with invalid index", getUDNameCData(cd).c_str());
    return 0;
}

int handleCDataNamecall(lua_State* L, CData* cd)
{
    api_check(cd != nullptr);
    api_check(cd->type != nullptr);

    const char* method = lua_namecallatom(L, nullptr);
    api_check(method != nullptr);

    CType* ct = cd->type;

    if (strcmp(method, "write") == 0) {
        if (lua_gettop(L) < 2) {
            luaL_argerror(L, 2, "expected value to write to CData");
        }

        size_t size = getFFITypeOfCType(ct)->size;
        if (size == 0) {
            luaL_argerror(L, 1, "cannot write to a type with size 0");
        }

        writeLuaValueToCData(L, 2, cd->data, ct);
        return 0;
    } else if (strcmp(method, "read") == 0) {
        if (lua_gettop(L) != 1) {
            luaL_argerror(L, 1, "expected no arguments to read from CData");
        }

        pushLuaValueFromCData(L, cd->data, ct, 1);
        return 1;
    } else if (strcmp(method, "ptr") == 0) {
        retainCData(L, 1);

        CType* ptrType = newCPointerType(L, ct, false);
        retainCType(L, -1); // retain the pointer type

        void** ptr = static_cast<void**>(malloc(sizeof(void*)));
        *ptr = cd->data; // set the pointer to the array data
        newCPointerData(L, ptrType, ptr, true, true, false, cd);

        return 1;
    } else if (strcmp(method, "type") == 0) {
        pushCType(L, ct);
        return 1;
    } else if (strcmp(method, "tostring") == 0) {
        return handleCDataToString(L, cd);
    }

    luaL_error(L, "attempt to namecall %s with unknown method '%s'", getUDNameCData(cd).c_str(), method);
    return 0;
}

int handleCDataToString(lua_State* L, CData* cd)
{
    lua_pushfstring(L, "%s(%p)", toStringCData(cd).c_str(), cd->data);
    return 1;
}

void handleCDataDtor(lua_State* L, CData* cd)
{
    if (cd->releasectype && cd->type != nullptr) {
        releaseCType(L, cd->type);
    }

    if (cd->managed && cd->data != nullptr) {
        free(cd->data);
    }
    
    if (cd->dependent != nullptr) {
        releaseCData(L, cd->dependent);
    }
}



int getCDataUTagFromCType(CType* ct)
{
    switch (ct->kind) {
    case CTypeKind::ARRAY:
        return kFFICArrayDataTag;
    case CTypeKind::FUNC:
        return kFFICFuncDataTag;
    case CTypeKind::POINTER:
        return kFFICPointerDataTag;
    case CTypeKind::STRUCT:
        return kFFICStructDataTag;
    default:
        return kFFICBaseDataTag;
    }
}

std::string getUDNameCData(CData* cd)
{
    std::string name;
    switch (cd->type->kind) {
    case CTypeKind::ARRAY:
        name = kCArrayData;
        break;
    case CTypeKind::FUNC:
        name = kCFuncData;
        break;
    case CTypeKind::POINTER:
        name = kCPointerData;
        break;
    case CTypeKind::STRUCT:
        name = kCStructData;
        break;
    default:
        name = kCBaseData;
        break;
    }
    return name;
}

std::string toStringCData(CData* cd)
{
    api_check(cd != nullptr);
    api_check(cd->type != nullptr);
    return getUDNameCData(cd) + '<' + toStringCType(cd->type) + ">";
}

} // namespace ffi