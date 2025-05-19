#pragma once

#include "lute/ffi/ctype.h"

#include "lua.h"
#include "lualib.h"

#include "ffi.h"
#include <cstdint>
#include <cstddef>

namespace ffi
{

using CData = struct CData;
using CFuncData = struct CFuncData;

struct CData
{
    CType* type;
    int refcount;
    int selfref;
    union {
        const CFuncData* func;
        void* data;
    };
};

struct CFuncData
{
    const void* func;
    void** args; // preallocated argument values
};

}

