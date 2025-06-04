#include "lute/ffi/state.h"
#include "lute/runtime.h"

#include "lua.h"
#include "lualib.h"

#include "tcc/libtcc.h"

#include <iostream>
#include <cstdlib>
#include <unordered_map>

namespace ffi
{

FFIState::FFIState()
{
    tcc = tcc_new();
    if (!tcc)
        std::abort();


    if (tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY) < 0)
    {
        tcc_delete(tcc);
        tcc = nullptr;
        std::abort();
    }

    tcc_set_options(tcc, "-std=c11 -nostdlib -nostdinc -Wl,--export-all-symbols");
}

FFIState::~FFIState()
{
    tcc_delete(tcc);
}

FFIState* getFFIState(lua_State* L)
{
    Runtime* runtime = getRuntime(L);

    return &runtime->ffiState;
}

} // namespace ffi