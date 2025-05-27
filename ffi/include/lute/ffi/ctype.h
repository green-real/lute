#pragma once

#include "lua.h"
#include "lualib.h"

#include "ffi.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>

namespace ffi
{

using CTypeKind = enum class CTypeKind;
using CBaseTypeKind = enum class CBaseTypeKind;
using CType = struct CType;
using CArrayType = struct CArrayType;
using CFuncType = struct CFuncType;
using CPointerType = struct CPointerType;
using CStructFieldType = struct CStructFieldType;
using CStructType = struct CStructType;

static const char kCType[] = "CType";
static const char kCArrayType[] = "CArrayType";
static const char kCBaseType[] = "CBaseType";
static const char kCFuncType[] = "CFuncType";
static const char kCPointerType[] = "CPointerType";
static const char kCStructType[] = "CStructType";

enum class CTypeKind {
    BOOL,

    CHAR,
    SCHAR,
    UCHAR,

    SHORT,
    USHORT,

    INT,
    UINT,

    LONG,
    ULONG,

    LONGLONG,
    ULONGLONG,

    INT8_T,
    UINT8_T,
    INT16_T,
    UINT16_T,
    INT32_T,
    UINT32_T,
    INT64_T,
    UINT64_T,

    SIZE_T,
    SSIZE_T,

    FLOAT,
    DOUBLE,

    VOID,

    ARRAY,
    FUNC,
    POINTER,
    STRUCT,
};

enum class CBaseTypeKind {
    BOOL,

    CHAR,
    SCHAR,
    UCHAR,

    SHORT,
    USHORT,

    INT,
    UINT,

    LONG,
    ULONG,

    LONGLONG,
    ULONGLONG,

    INT8_T,
    INT16_T,
    INT32_T,
    INT64_T,
    UINT8_T,
    UINT16_T,
    UINT32_T,
    UINT64_T,

    SIZE_T,
    SSIZE_T,

    FLOAT,
    DOUBLE,

    VOID,

    __COUNT__,
};

struct CArrayType {
    CType* elemtype;
    size_t size;
    ffi_type ft;
    bool releasectype; // if true, CArrayType will call releaseCType on inner when garbage collected

    void releaseDependencies(lua_State* L) const;
    ~CArrayType();
private:
    CArrayType(lua_State* L, CType* elemtype, size_t size, bool releasectype);
    friend CType* newCArrayType(lua_State* L, CType* elemtype, size_t size, bool releasectype);
};

struct CFuncType {
    CType* ret;
    std::vector<CType*> args;
    ffi_cif cif;
    ffi_abi abi;
    bool releasectype; // if true, CFunctionType will call releaseCType on ret and args when garbage collected
    std::string symbol; // symbol name for this function type, if any

    void releaseDependencies(lua_State* L) const;
    ~CFuncType();

private:
    CFuncType(lua_State* L, CType* ret, std::vector<CType*> args, ffi_abi abi, bool releasectype, std::string symbol, int& ffi_status);

    friend CType* newCFuncType(lua_State* L, CType* ret, std::vector<CType*> args, ffi_abi abi, bool releasectype, std::string symbol);
};

struct CPointerType {
    CType* innertype;
    bool releasectype; // if true, CPointerType will call releaseCType on inner when garbage collected

    void releaseDependencies(lua_State* L) const;
    ~CPointerType();

private:
    CPointerType(lua_State* L, CType* innertype, bool releasectype);
    friend CType* newCPointerType(lua_State* L, CType* innertype, bool releasectype);
};

struct CStructFieldType {
    CType* type;
    std::string name;
    size_t offset;

private:
    CStructFieldType(CType* type, std::string name, size_t offset, bool releasectype);

    friend CStructType;
};

struct CStructType {
    std::vector<CStructFieldType> fields;
    std::unordered_map<std::string, size_t> field_map;
    std::string debugname;
    ffi_type ft;
    bool releasectype; // if true, CStructType will call releaseCType on its struct field types when garbage collected

    void releaseDependencies(lua_State* L) const;
    ~CStructType();

private:
    CStructType(lua_State* L, std::vector<CType*> ftypes,std::vector<std::string> fnames, std::string debugname, bool releasectype, int& ffi_status);

    friend CType* newCStructType(lua_State* L, std::vector<CType*> ftypes,std::vector<std::string> fnames, std::string debugname, bool releasectype);
};

struct CType {
    CTypeKind kind;
    int refcount;
    int selfref;
    union {
        const struct CArrayType *array;
        const struct CFuncType *func;
        const struct CPointerType *ptr;
        const struct CStructType *struct_;
        const ffi_type *ft;
    };
};

CType* newCType(lua_State* L, CTypeKind kind, int utag);
CType* newCArrayType(lua_State* L, CType* elemtype, size_t size, bool releasectype);
CType* newCBaseType(lua_State* L, CBaseTypeKind kind, const ffi_type* ft);
CType* newCFuncType(lua_State* L, CType* ret, std::vector<CType*> args, ffi_abi abi, bool releasectype, std::string symbol);
CType* newCPointerType(lua_State* L, CType* innertype, bool releasectype);
CType* newCStructType(lua_State* L, std::vector<CType*> ftypes,std::vector<std::string> fnames, std::string debugname, bool releasectype);

CType* toCType(lua_State* L, int idx);
CType* toCArrayType(lua_State* L, int idx);
CType* toCBaseType(lua_State* L, int idx);
CType* toCFuncType(lua_State* L, int idx);
CType* toCPointerType(lua_State* L, int idx);
CType* toCStructType(lua_State* L, int idx);

CType* checkCType(lua_State* L, int idx);
CType* checkCType(lua_State* L, int idx, bool allowFunc, bool allowVoid);
CType* checkCArrayType(lua_State* L, int idx);
CType* checkCBaseType(lua_State* L, int idx);
CType* checkCFuncType(lua_State* L, int idx);
CType* checkCPointerType(lua_State* L, int idx);
CType* checkCStructType(lua_State* L, int idx);

void initCArrayType(lua_State* L);
void initCBaseType(lua_State* L);
void initCFuncType(lua_State* L);
void initCPointerType(lua_State* L);
void initCStructType(lua_State* L);

int handleCTypeNamecall(lua_State* L, CType* ct);
int handleCTypeToString(lua_State* L, CType* ct);

void retainCType(lua_State* L, int idx);
void retainCType(lua_State* L, CType* ct);
void releaseCType(lua_State* L, CType* ct);

bool pushCType(lua_State* L, CType* ctype);

const ffi_type* getCIntFFIType(std::size_t size, bool isSigned);

std::string toStringCType(CType* ct);
std::string getNameCType(CType* ct);
std::string getUDNameCType(CType* ct);
size_t sizeOfCType(CType* ct);
const ffi_type* getFFITypeOfCType(CType* ct);

}

