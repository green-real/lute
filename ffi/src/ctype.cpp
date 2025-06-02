#include "lute/ffi/ctype.h"

#include "./utils.h"

#include "lute/userdatas.h"

#include "Luau/Common.h"
#include "lua.h"
#include "lualib.h"

#include <algorithm>
#include <cstring>
#include <cstddef>
#include <functional>
#include <optional>
#include <sstream>

namespace ffi
{

static std::string getCBaseTypeKindName(CBaseTypeKind kind)
{
    switch (kind)
    {
    case CBaseTypeKind::Void:
        return "void";
    case CBaseTypeKind::Int8:
        return "int8_t";
    case CBaseTypeKind::UInt8:
        return "uint8_t";
    case CBaseTypeKind::Int16:
        return "int16_t";
    case CBaseTypeKind::UInt16:
        return "uint16_t";
    case CBaseTypeKind::Int32:
        return "int32_t";
    case CBaseTypeKind::UInt32:
        return "uint32_t";
    case CBaseTypeKind::Int64:
        return "int64_t";
    case CBaseTypeKind::UInt64:
        return "uint64_t";
    case CBaseTypeKind::Float:
        return "float";
    case CBaseTypeKind::Double:
        return "double";
    }

    LUAU_UNREACHABLE();
}

static size_t getCBaseTypeKindSize(CBaseTypeKind kind)
{
    switch (kind)
    {
    case CBaseTypeKind::Void:
        return 0;
    case CBaseTypeKind::Int8:
    case CBaseTypeKind::UInt8:
        return 1;
    case CBaseTypeKind::Int16:
    case CBaseTypeKind::UInt16:
        return 2;
    case CBaseTypeKind::Int32:
    case CBaseTypeKind::UInt32:
        return 4;
    case CBaseTypeKind::Int64:
    case CBaseTypeKind::UInt64:
        return 8;
    case CBaseTypeKind::Float:
        return sizeof(float);
    case CBaseTypeKind::Double:
        return sizeof(double);
    }

    LUAU_UNREACHABLE();
}

static size_t getCTypeAlignment(CType* ctype)
{
    switch (ctype->kind)
    {
    case CTypeKind::Array:
        return ctype->array->elementType->size;
    case CTypeKind::Base:
        return ctype->size;
    case CTypeKind::Function:
        return sizeof(void*);
    case CTypeKind::Pointer:
        return sizeof(void*);
    case CTypeKind::Record:
        return ctype->record->alignment;
    }

    LUAU_UNREACHABLE();
}

static CType* newCType(lua_State* L, CTypeKind kind)
{
    CType* ctype = static_cast<CType*>(lua_newuserdatataggedwithmetatable(L, sizeof(CType), kFFICTypeTag));
    ctype->kind = kind;
    ctype->size = 0; // size will be set later
    ctype->refCount = 0;
    ctype->luaRef = LUA_NOREF;

    return ctype;
}

CType* newCArrayType(lua_State* L, CType* elementType, size_t elementCount, bool retainedCType)
{
    CType* ctype = newCType(L, CTypeKind::Array);
    ctype->array = new CArrayType{
        .elementType = elementType,
        .elementCount = elementCount,
        .retainedCType = retainedCType,
    };
    ctype->size = elementType->size * elementCount;

    return ctype;
}

CType* newCBaseType(lua_State* L, CBaseTypeKind kind, std::optional<std::string> name)
{
    CType* ctype = newCType(L, CTypeKind::Base);
    ctype->base = new CBaseType{kind, name.value_or(getCBaseTypeKindName(kind))};
    ctype->size = getCBaseTypeKindSize(kind);

    return ctype;
}

CType* newCFunctionType(lua_State* L, std::vector<CType*> argumentTypes, CType* returnType, std::string symbol, bool retainedCTypes)
{
    CType* ctype = newCType(L, CTypeKind::Function);
    ctype->func = new CFunctionType{
        .argumentTypes = std::move(argumentTypes),
        .returnType = returnType,
        .symbol = std::move(symbol),
        .retainedCTypes = retainedCTypes,
    };
    ctype->size = sizeof(void*);

    return ctype;
}

CType* newCPointerType(lua_State* L, CType* innerType, bool retainedCType)
{
    CType* ctype = newCType(L, CTypeKind::Pointer);
    ctype->ptr = new CPointerType{
        .innerType = innerType,
        .retainedCType = retainedCType,
    };
    ctype->size = sizeof(void*);

    return ctype;
}

CType* newCRecordType(lua_State* L, std::vector<CType*> fieldTypes, std::vector<std::string> fieldNames, bool retainedCTypes)
{
    api_check(fieldTypes.size() == fieldNames.size());

    size_t nfields = fieldTypes.size();

    std::unordered_map<std::string_view, size_t> fieldNameToIndex(nfields);
    std::vector<size_t> fieldOffsets(nfields);

    size_t alignment = 1;
    size_t offset = 0;
    for (size_t i = 0; i < nfields; ++i)
    {
        CType* fieldType = fieldTypes[i];
        api_check(fieldType != nullptr);

        // Calculate the alignment of the field type
        size_t fieldAlignment = getCTypeAlignment(fieldType);
        if (fieldAlignment > alignment)
        {
            alignment = fieldAlignment;
        }

        // Align the offset to the field's alignment
        if (offset % fieldAlignment != 0)
        {
            offset += fieldAlignment - (offset % fieldAlignment);
        }

        // Store the offset for this field
        fieldOffsets[i] = offset;

        // Update the offset for the next field
        offset += fieldType->size;

        // Store the name to idx mapping
        fieldNameToIndex[fieldNames[i]] = i;
    }

    size_t recordSize = offset;
    if (recordSize % alignment != 0)
    {
        recordSize += alignment - (recordSize % alignment);
    }

    CType* ctype = newCType(L, CTypeKind::Record);
    ctype->record = new CRecordType{
        .fieldTypes = std::move(fieldTypes),
        .fieldNames = std::move(fieldNames),
        .fieldOffsets = std::move(fieldOffsets),
        .fieldNameToIndex = std::move(fieldNameToIndex),
        .retainedCTypes = retainedCTypes,
        .alignment = alignment,
    };
    ctype->size = recordSize;

    return ctype;
}

CType* toCType(lua_State* L, int idx)
{
    return static_cast<CType*>(lua_touserdatatagged(L, idx, kFFICTypeTag));
}

CType* toCArrayType(lua_State* L, int idx)
{
    CType* ctype = toCType(L, idx);
    return (ctype && ctype->kind == CTypeKind::Array) ? ctype : nullptr;
}

CType* toCBaseType(lua_State* L, int idx)
{
    CType* ctype = toCType(L, idx);
    return (ctype && ctype->kind == CTypeKind::Base) ? ctype : nullptr;
}

CType* toCFunctionType(lua_State* L, int idx)
{
    CType* ctype = toCType(L, idx);
    return (ctype && ctype->kind == CTypeKind::Function) ? ctype : nullptr;
}

CType* toCPointerType(lua_State* L, int idx)
{
    CType* ctype = toCType(L, idx);
    return (ctype && ctype->kind == CTypeKind::Pointer) ? ctype : nullptr;
}

CType* toCRecordType(lua_State* L, int idx)
{
    CType* ctype = toCType(L, idx);
    return (ctype && ctype->kind == CTypeKind::Record) ? ctype : nullptr;
}

CType* checkCType(lua_State* L, int idx)
{
    CType* ctype = toCType(L, idx);
    if (ctype != nullptr)
        return ctype;
    luaL_typeerror(L, idx, kCTypeName);
}

CType* checkCArrayType(lua_State* L, int idx)
{
    CType* ctype = toCArrayType(L, idx);
    if (ctype != nullptr)
        return ctype;
    luaL_typeerror(L, idx, kCArrayTypeName);
}

CType* checkCBaseType(lua_State* L, int idx)
{
    CType* ctype = toCBaseType(L, idx);
    if (ctype != nullptr)
        return ctype;
    luaL_typeerror(L, idx, kCBaseTypeName);
}

CType* checkCFunctionType(lua_State* L, int idx)
{
    CType* ctype = toCFunctionType(L, idx);
    if (ctype != nullptr)
        return ctype;
    luaL_typeerror(L, idx, kCFunctionTypeName);
}

CType* checkCPointerType(lua_State* L, int idx)
{
    CType* ctype = toCPointerType(L, idx);
    if (ctype != nullptr)
        return ctype;
    luaL_typeerror(L, idx, kCPointerTypeName);
}

CType* checkCRecordType(lua_State* L, int idx)
{
    CType* ctype = toCRecordType(L, idx);
    if (ctype != nullptr)
        return ctype;
    luaL_typeerror(L, idx, kCRecordTypeName);
}

std::string toStringCType(CType* ctype)
{
    std::stringstream ss;
    bool first_pointer = true;

    std::function<void(CType*, int)> writeType = [&](CType* t, int depth)
    {
        ss << std::string(depth, ' ');

        switch (t->kind)
        {
        case CTypeKind::Array:
            writeType(t->array->elementType, depth);
            ss << '[' << t->array->elementCount << ']';
            break;
        case CTypeKind::Base:
            ss << t->base->name;
            break;
        case CTypeKind::Function:
            writeType(t->func->returnType, depth);
            ss << " (";
            for (size_t i = 0; i < t->func->argumentTypes.size(); ++i)
            {
                writeType(t->func->argumentTypes[i], depth);
                if (i < t->func->argumentTypes.size() - 1)
                    ss << ", ";
            }
            ss << ')';
            break;
        case CTypeKind::Pointer:
            writeType(t->ptr->innerType, depth);
            if (first_pointer)
            {
                ss << ' ';
                first_pointer = false;
            }
            ss << '*';
            break;
        case CTypeKind::Record:
            ss << "struct ";
            ss << "{\n";
            for (size_t i = 0; i < t->record->fieldTypes.size(); ++i)
            {
                ss << std::string(depth + 2, ' ');
                writeType(t->record->fieldTypes[i], depth + 2);
                ss << ' ' << t->record->fieldNames[i];
                if (i < t->record->fieldTypes.size() - 1)
                    ss << ',';
                ss << '\n';
            }
            ss << std::string(depth, ' ');
            ss << '}';
            break;
        };
    };

    writeType(ctype, 0);
    return ss.str();
}

void retainCType(lua_State* L, int idx)
{
    CType* ctype = toCType(L, idx);
    api_check(ctype != nullptr);

    ctype->refCount++;
    if (ctype->luaRef == LUA_NOREF)
    {
        ctype->luaRef = lua_ref(L, idx);
    }
}

void retainCType(lua_State* L, CType* ctype)
{
    api_check(ctype->refCount > 0);
    api_check(ctype->luaRef != LUA_NOREF);

    ctype->refCount++;
}

void releaseCType(lua_State* L, CType* ctype)
{
    api_check(ctype->refCount > 0);
    api_check(ctype->luaRef != LUA_NOREF);

    ctype->refCount--;
    if (ctype->refCount == 0)
    {
        lua_unref(L, ctype->luaRef);
        ctype->luaRef = LUA_NOREF;
    }
}

static void handleCArrayTypeDestruction(lua_State* L, CType* ctype)
{
    api_check(ctype->kind == CTypeKind::Array);
    api_check(ctype->array != nullptr);

    CArrayType* arrayType = ctype->array;
    if (arrayType->retainedCType)
    {
        releaseCType(L, arrayType->elementType);
    }

    delete arrayType;
    ctype->array = nullptr;
}

static void handleCBaseTypeDestruction(lua_State* L, CType* ctype)
{
    api_check(ctype->kind == CTypeKind::Base);
    api_check(ctype->base != nullptr);

    CBaseType* baseType = ctype->base;

    delete baseType;
    ctype->base = nullptr;
}

static void handleCFunctionTypeDestruction(lua_State* L, CType* ctype)
{
    api_check(ctype->kind == CTypeKind::Function);
    api_check(ctype->func != nullptr);

    CFunctionType* funcType = ctype->func;
    if (funcType->retainedCTypes)
    {
        releaseCType(L, funcType->returnType);
        for (CType* argType : funcType->argumentTypes)
        {
            releaseCType(L, argType);
        }
    }

    delete funcType;
    ctype->func = nullptr;
}

static void handleCPointerTypeDestruction(lua_State* L, CType* ctype)
{
    api_check(ctype->kind == CTypeKind::Pointer);
    api_check(ctype->ptr != nullptr);

    CPointerType* ptrType = ctype->ptr;
    if (ptrType->retainedCType)
    {
        releaseCType(L, ptrType->innerType);
    }

    delete ptrType;
    ctype->ptr = nullptr;
}

static void handleCRecordTypeDestruction(lua_State* L, CType* ctype)
{
    api_check(ctype->kind == CTypeKind::Record);
    api_check(ctype->record != nullptr);

    CRecordType* recordType = ctype->record;
    if (recordType->retainedCTypes)
    {
        for (CType* fieldType : recordType->fieldTypes)
        {
            releaseCType(L, fieldType);
        }
    }

    delete recordType;
    ctype->record = nullptr;
}

static void handleCTypeDestruction(lua_State* L, void* ud)
{
    CType* ctype = static_cast<CType*>(ud);

    switch (ctype->kind)
    {
    case CTypeKind::Array:
        handleCArrayTypeDestruction(L, ctype);
        break;
    case CTypeKind::Base:
        handleCBaseTypeDestruction(L, ctype);
        break;
    case CTypeKind::Function:
        handleCFunctionTypeDestruction(L, ctype);
        break;
    case CTypeKind::Pointer:
        handleCPointerTypeDestruction(L, ctype);
        break;
    case CTypeKind::Record:
        handleCRecordTypeDestruction(L, ctype);
        break;
    }
}

static std::string getCTypeKindName(CTypeKind kind)
{
    switch (kind)
    {
    case CTypeKind::Array:
        return kCArrayTypeName;
    case CTypeKind::Base:
        return kCBaseTypeName;
    case CTypeKind::Function:
        return kCFunctionTypeName;
    case CTypeKind::Pointer:
        return kCPointerTypeName;
    case CTypeKind::Record:
        return kCRecordTypeName;
    default:
        abort(); // Invalid CTypeKind
    }
}

static int handleCTypeToString(lua_State* L)
{
    CType* ctype = checkCType(L, 1);

    lua_pushfstring(L, "%s< %s >", getCTypeKindName(ctype->kind).c_str(), toStringCType(ctype).c_str());
    return 1;
}

static int handleCTypeNamecall(lua_State* L)
{
    CType* ctype = checkCType(L, 1);

    const char* method = lua_namecallatom(L, nullptr);
    if (method == nullptr)
        luaL_error(L, "attempt to namecall %s with invalid method", getCTypeKindName(ctype->kind).c_str());

    if (strcmp(method, "kind") == 0)
    {
        lua_pushstring(L, getCTypeKindName(ctype->kind).c_str());
        return 1;
    }
    else if (strcmp(method, "size") == 0)
    {
        lua_pushinteger(L, ctype->size);
        return 1;
    }

    luaL_error(L, "attempt to namecall %s with invalid method '%s'", getCTypeKindName(ctype->kind).c_str(), method);
}

void initCType(lua_State* L)
{
    luaL_newmetatable(L, kCTypeName);

    lua_pushvalue(L, -1);
    lua_setuserdatametatable(L, kFFICTypeTag);

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, handleCTypeToString, "ctype__tostring");
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, handleCTypeNamecall, "ctype__namecall");
    lua_setfield(L, -2, "__namecall");

    lua_pushstring(L, kCTypeName);
    lua_setfield(L, -2, "__type");

    lua_setreadonly(L, -1, 1);
    lua_pop(L, 1);

    lua_setuserdatadtor(L, kFFICTypeTag, handleCTypeDestruction);
}

} // namespace ffi
