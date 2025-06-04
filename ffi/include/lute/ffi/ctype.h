#pragma once

#include "lua.h"
#include "lualib.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

namespace ffi
{

using CBindingFunction = int (*)(lua_State* L, void* func);

static const char kCTypeName[] = "CType";
static const char kCArrayTypeName[] = "CArrayType";
static const char kCBaseTypeName[] = "CBaseType";
static const char kCFunctionTypeName[] = "CFunctionType";
static const char kCPointerTypeName[] = "CPointerType";
static const char kCRecordTypeName[] = "CRecordType";

enum class CTypeKind
{
    Array,
    Base,
    Function,
    Pointer,
    Record
};

enum class CBaseTypeKind
{
    Void,

    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,

    Float,
    Double
};

struct CType
{
    CTypeKind kind;
    union
    {
        struct CArrayType* array;
        struct CBaseType* base;
        struct CFunctionType* func;
        struct CPointerType* ptr;
        struct CRecordType* record;
    };

    size_t size; // size of the type in bytes
    int refCount;
    int luaRef;
};

struct CArrayType
{
    CType* elementType;
    size_t elementCount;
    bool retainedCType; // whether the element type is retained and needs to be released when the CArrayType is destroyed
};

struct CBaseType
{
    CBaseTypeKind kind;
    std::string name;
};

struct CFunctionType
{
    std::vector<CType*> argumentTypes;
    CType* returnType;
    std::string symbol;
    CBindingFunction bindingFunction; // generated when this CFunctionType is used for the first time in
    bool retainedCTypes; // whether the argument and return types are retained and need to be released when the CFunctionType is destroyed
};

struct CPointerType
{
    CType* innerType;
    bool retainedCType; // whether the inner type is retained and needs to be released when the CPointerType is destroyed
};

struct CRecordType
{
    std::vector<CType*> fieldTypes;
    std::vector<std::string> fieldNames;
    std::vector<size_t> fieldOffsets;
    std::unordered_map<std::string_view, size_t> fieldNameToIndex;
    bool retainedCTypes; // whether the field types are retained and need to be released when the CRecordType is destroyed

    size_t alignment;
};

CType* newCArrayType(lua_State* L, CType* elementType, size_t elementCount, bool retainedCType);
CType* newCBaseType(lua_State* L, CBaseTypeKind kind, std::optional<std::string> name = std::nullopt);
CType* newCFunctionType(lua_State* L, std::vector<CType*> argumentTypes, CType* returnType, std::string symbol, bool retainedCTypes);
CType* newCPointerType(lua_State* L, CType* innerType, bool retainedCType);
CType* newCRecordType(lua_State* L, std::vector<CType*> fieldTypes, std::vector<std::string> fieldNames, bool retainedCTypes);

CType* toCType(lua_State* L, int idx);
CType* toCArrayType(lua_State* L, int idx);
CType* toCBaseType(lua_State* L, int idx);
CType* toCFunctionType(lua_State* L, int idx);
CType* toCPointerType(lua_State* L, int idx);
CType* toCRecordType(lua_State* L, int idx);

CType* checkCType(lua_State* L, int idx);
CType* checkCArrayType(lua_State* L, int idx);
CType* checkCBaseType(lua_State* L, int idx);
CType* checkCFunctionType(lua_State* L, int idx);
CType* checkCPointerType(lua_State* L, int idx);
CType* checkCRecordType(lua_State* L, int idx);

std::string toStringCType(CType* ctype);

void retainCType(lua_State* L, int idx);
void retainCType(lua_State* L, CType* ctype);
void releaseCType(lua_State* L, CType* ctype);

} // namespace ffi
