#pragma once

#include "lua.h"

#include <unordered_map>
#include <string>

namespace ffi
{

using FFIDLHandle = struct FFIDLHandle;

static const char kFFIDLHandle[] = "FFIDLHandle";

#ifdef _WIN32
static const char kLibCDL[] = "ucrtbase.dll";
#else
static const char kLibCDL[] = "libc.so.6";
#endif

struct FFIDLHandle
{
    void* handle;

    int refcount;
    int selfref;
};

FFIDLHandle* newFFIDLHandle(lua_State* L, const char* path);
FFIDLHandle* toFFIDLHandle(lua_State* L, int idx);
FFIDLHandle* checkFFIDLHandle(lua_State* L, int idx);

std::string getFFIDLHandlePath(lua_State* L, FFIDLHandle* dl);

void retainFFIDLHandle(lua_State* L, int idx);
void retainFFIDLHandle(lua_State* L, FFIDLHandle* dl);
void releaseFFIDLHandle(lua_State* L, FFIDLHandle* dl);

void initFFIDLHandle(lua_State* L);

void* openLibrary(const char* path, std::string& err);

int closeLibrary(void* lib);

void* getSymbol(void* lib, const std::string& symbol_name);

}