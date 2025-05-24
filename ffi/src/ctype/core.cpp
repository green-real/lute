#include "lute/ffi.h"
#include "lute/ffi/ctype.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include "Luau/Common.h"
#include "lua.h"
#include "lualib.h"

#include <cstring>
#include <string>
#include <sstream>
#include <functional>
#include <unordered_set>

namespace ffi
{

CType* newCType(lua_State* L, CTypeKind kind, int utag)
{
    CType* ct = static_cast<CType*>(lua_newuserdatataggedwithmetatable(L, sizeof(CType), utag));
    ct->kind = kind;
    ct->refcount = 0;
    ct->selfref = LUA_NOREF;

    lua_rawgetfield(L, LUA_REGISTRYINDEX, kFFIWeakRegistryKey);
    lua_pushlightuserdata(L, ct);
    lua_pushvalue(L, -3);
    lua_settable(L, -3);
    lua_pop(L, 1);

    return ct;
}

CType* toCType(lua_State* L, int idx)
{
    switch (lua_userdatatag(L, idx)) {
    case kFFICArrayTypeTag:
    case kFFICBaseTypeTag:
    case kFFICFuncTypeTag:
    case kFFICPointerTypeTag:
    case kFFICStructTypeTag:
        break;
    default:
        return nullptr;
    }

    return static_cast<CType*>(lua_touserdata(L, idx));
}

CType* checkCType(lua_State* L, int idx)
{
    CType* ct = toCType(L, idx);
    if (ct != nullptr)
        return ct;

    luaL_typeerror(L, idx, kCType);
    return nullptr;
}

int handleCTypeNamecall(lua_State* L, CType* ct)
{
    const char* method = lua_namecallatom(L, nullptr);

    if (strcmp(method, "ptr") == 0) {
        retainCType(L, 1);
        ffi::newCPointerType(L, ct, true);

        return 1;
    }

    luaL_error(L, "attempt to namecall %s<%s> with unknown method '%s'", getUDNameCType(ct).c_str(), toStringCType(ct).c_str(), method);
    return 0;
}

int handleCTypeToString(lua_State* L, CType* ct)
{
    std::string name = getUDNameCType(ct);
    std::string str = toStringCType(ct);
    str = name + "< " + str + " >";
    lua_pushstring(L, str.c_str());

    return 1;
}

// increments the refcount and retains the CType
void retainCType(lua_State* L, int idx)
{
    CType* ct = toCType(L, idx);
    api_check(ct != nullptr);
    api_check(ct->refcount != 0 || ct->selfref == LUA_NOREF);

    ct->refcount++;
    if (ct->selfref == LUA_NOREF)
        ct->selfref = lua_ref(L, idx); 
}

// retains a CType that is currently retained, so that it is not garbage collected until it is released
// this should only be used when the CType is already retained
void retainCType(lua_State* L, CType* ct)
{
    api_check(ct != nullptr);
    api_check(ct->refcount != 0);
    api_check(ct->selfref != LUA_NOREF);

    ct->refcount++;
}

// releases a CType previously retained by retainCType
void releaseCType(lua_State* L, CType* ct)
{
    api_check(ct != nullptr);
    api_check(ct->refcount > 0);
    api_check(ct->selfref != LUA_NOREF);

    ct->refcount--;
    if (ct->refcount == 0) {
        lua_unref(L, ct->selfref);
        ct->selfref = LUA_NOREF;
    }
}

// attempts to find the given CType in the weak registry and puts it on the stack
// returns true if the CType was found
bool pushCType(lua_State* L, CType* ct)
{
    lua_rawgetfield(L, LUA_REGISTRYINDEX, kFFIWeakRegistryKey);
    lua_pushlightuserdata(L, ct);
    int type = lua_rawget(L, -2);
    lua_remove(L, -2);

    return type != LUA_TNIL;
}

// releases all dependencies of a CType
std::string toStringCType(CType* ct)
{
    std::stringstream ss;
    bool first_pointer = true;

    std::function<void(CType*, int)> writeType = [&](CType* t, int depth) {
        ss << std::string(depth, ' ');

        switch (t->kind) {
        case CTypeKind::ARRAY:
            writeType(t->array->elemtype, depth);
            ss << '[' << t->array->size << ']';
            break;
        case CTypeKind::FUNC:
            writeType(t->func->ret, depth);
            ss << " (";
            for (std::size_t i = 0; i < t->func->args.size(); ++i) {
                writeType(t->func->args[i], depth);
                if (i < t->func->args.size() - 1)
                    ss << ", ";
            }
            ss << ')';
            break;
        case CTypeKind::POINTER:
            writeType(t->ptr->innertype, depth);
            if (first_pointer) {
                ss << ' ';
                first_pointer = false;
            }
            ss << '*';
            break;
        case CTypeKind::STRUCT:
            ss << "struct " << t->struct_->debugname;
            if (ct == t) {
                if (!t->struct_->debugname.empty()) ss << ' ';
                ss << "{\n";
                for (const CStructFieldType& f : t->struct_->fields) {
                    writeType(f.type, depth + 1);
                    ss << std::string(depth, ' ');
                    ss << ' ' << f.name << ";\n";
                }
                ss << std::string(depth, ' ');
                ss << '}';
            } else {
                ss << ";";
            }
            break;
        default:
            ss << ffi::getNameCType(t);
            break;
        }
    };

    writeType(ct, 0);
    return ss.str();
}

std::string getNameCType(CType* ct)
{
    switch (ct->kind) {
    case CTypeKind::BOOL:
        return "bool";
    case CTypeKind::CHAR:
        return "char";
    case CTypeKind::SCHAR:
        return "schar";
    case CTypeKind::UCHAR:
        return "uchar";
    case CTypeKind::SHORT:
        return "short";
    case CTypeKind::USHORT:
        return "ushort";
    case CTypeKind::INT:
        return "int";
    case CTypeKind::UINT:
        return "uint";
    case CTypeKind::LONG:
        return "long";
    case CTypeKind::ULONG:
        return "ulong";
    case CTypeKind::LONGLONG:
        return "longlong";
    case CTypeKind::ULONGLONG:
        return "ulonglong";
    
    case CTypeKind::INT8_T:
        return "int8_t";
    case CTypeKind::UINT8_T:
        return "uint8_t";
    case CTypeKind::INT16_T:
        return "int16_t";
    case CTypeKind::UINT16_T:
        return "uint16_t";
    case CTypeKind::INT32_T:
        return "int32_t";
    case CTypeKind::UINT32_T:
        return "uint32_t";
    case CTypeKind::INT64_T:
        return "int64_t";
    case CTypeKind::UINT64_T:
        return "uint64_t";

    case CTypeKind::SIZE_T:
        return "size_t";
    case CTypeKind::SSIZE_T:
        return "ssize_t";

    case CTypeKind::FLOAT:
        return "float";
    case CTypeKind::DOUBLE:
        return "double";

    case CTypeKind::VOID:
        return "void";

    case CTypeKind::ARRAY:
        return "array";
    case CTypeKind::FUNC:
        return "func";
    case CTypeKind::POINTER:
        return "pointer";
    case CTypeKind::STRUCT:
        return "struct";
    
    default:
        return "unknown";
    }
}

std::string getUDNameCType(CType* ct)
{
    std::string name;
    switch (ct->kind) {
    case CTypeKind::ARRAY:
        name = kCArrayType;
        break;
    case CTypeKind::FUNC:
        name = kCFuncType;
        break;
    case CTypeKind::POINTER:
        name = kCPointerType;
        break;
    case CTypeKind::STRUCT:
        name = kCStructType;
        break;
    default:
        name = kCBaseType;
        break;
    }

    return name;
}

size_t sizeOfCType(CType* ct)
{
    return getFFITypeOfCType(ct)->size;
}

const ffi_type* getFFITypeOfCType(CType* ct)
{
    switch (ct->kind) {
    case CTypeKind::ARRAY:
        return &ct->array->ft;
    case CTypeKind::POINTER:
        return &ffi_type_pointer;
    case CTypeKind::STRUCT:
        return &ct->struct_->ft;
    case CTypeKind::FUNC:
        return nullptr;
    default:
        return ct->ft;
    }
}


}


