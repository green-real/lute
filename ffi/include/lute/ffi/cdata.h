#pragma once

#include "lute/ffi/ctype.h"
#include "lute/ffi/dlib.h"

#include "lua.h"
#include "lualib.h"

#include "ffi.h"
#include <cstdint>
#include <cstddef>

namespace ffi
{

using CDataKind = enum class CDataKind;
using CData = struct CData;
using CFuncData = struct CFuncData;
using CPointerData = struct CPointerData;

static const char kCData[] = "CData";
static const char kCArrayData[] = "CArrayData";
static const char kCBaseData[] = "CBaseData";
static const char kCFuncData[] = "CFuncData";
static const char kCPointerData[] = "CPointerData";
static const char kCStructData[] = "CStructData";

enum class CDataKind
{
    DATA,
    FUNC,
    POINTER
};

struct CData
{
    CDataKind kind;
    bool releasectype; // if true, CData will call releaseCType on type when garbage collected
    bool managed; // if true, CData will free data when garbage collected
    bool isvalid = true; // if true, CData is valid and can be used. CData will be invalidated when CData:free() is called

    int refcount;
    int selfref;

    CType* type;
    CData* dependent; // if not null, this CData will be released when dependent is released
    void* data;
    union {
        CFuncData* funcdata;
        CPointerData* ptrdata;
    };
};

struct CFuncData
{
    void** args; // preallocated argument values
    void** argstorage; // storage for arguments, used to avoid reallocating memory for each call if not using CData as argument
    CData* retcd; // if not null, this is the CData that will be returned as the result of the function call
    FFIDLHandle* dlib; // if not null, this is the FFIDLHandle that this CFuncData is associated with, which will be released when the CFuncData is garbage collected
};

struct CPointerData
{
    bool innermanaged; // if true, CPointerData will free *ptr when garbage collected
    CData* refto; // if this is not null, CPointerData will call releaseCData on refto when garbage collected
};

CData* newCData(lua_State* L, CType* type, void* data, bool releasectype, bool managed, CData* dependent = nullptr);
CData* newCFuncData(lua_State* L, CType* type, void* data, bool releasectype, FFIDLHandle* dlib = nullptr);
CData* newCPointerData(lua_State* L, CType* type, void* data, bool releasectype, bool managed, bool innermanaged, CData* dependent = nullptr);

CData* toCData(lua_State* L, int idx);
CData* toCArrayData(lua_State* L, int idx);
CData* toCBaseData(lua_State* L, int idx);
CData* toCFuncData(lua_State* L, int idx);
CData* toCPointerData(lua_State* L, int idx);
CData* toCStructData(lua_State* L, int idx);

CData* checkCData(lua_State* L, int idx);
CData* checkCArrayData(lua_State* L, int idx);
CData* checkCBaseData(lua_State* L, int idx);
CData* checkCFuncData(lua_State* L, int idx);
CData* checkCPointerData(lua_State* L, int idx);
CData* checkCStructData(lua_State* L, int idx);

bool pushCData(lua_State* L, CData* cd);

void writeLuaValueToCData(lua_State* L, int idx, void* data, CType* ct);
void writeLuaNumberToCData(lua_State* L, int idx, void* data, CType* ct);
void writeLuaTableToCArray(lua_State* L, int idx, void* data, CType* ct);
void writeLuaTableToCStruct(lua_State* L, int idx, void* data, CType* ct);

int pushLuaValueFromCData(lua_State* L, void* data, CType* ct, int cdataidx = 0);
int pushLuaNumberFromCData(lua_State* L, void* data, CType* ct);

int handleCDataNamecall(lua_State* L, CData* cd);
int handleCDataToString(lua_State* L, CData* cd);
void handleCDataDtor(lua_State* L, CData* cd);

void initCArrayData(lua_State* L);
void initCBaseData(lua_State* L);
void initCFuncData(lua_State* L);
void initCPointerData(lua_State* L);
void initCStructData(lua_State* L);

void retainCData(lua_State* L, int idx);
void retainCData(lua_State* L, CData* cd);
void releaseCData(lua_State* L, CData* cd);

int getCDataUTagFromCType(CType* ct);

std::string getUDNameCData(CData* cd);
std::string toStringCData(CData* cd);

}

