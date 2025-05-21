#include "lute/ffi/ctype.h"
#include "lute/ffi/utils.h"
#include "lute/userdatas.h"

#include "Luau/Common.h"

#include "lua.h"
#include "lualib.h"

#include "ffi.h"

#include <cstddef>
#include <memory>
#include <cstring>

namespace ffi
{

static ffi_type* getCStructFieldFFIType(lua_State* L, CType* ct, bool dependant)
{
    api_check(!dependant || ct->selfref != LUA_NOREF); // if dependant, ct must have a selfref

    // const_cast is safe, because it will never be modified past this point
    return const_cast<ffi_type*>(getFFITypeOfCType(ct));
}

CStructFieldType::CStructFieldType(CType* type, std::string name, std::size_t offset, bool dependant) : type(type), name(std::move(name)), offset(offset)
{
    api_check(type != nullptr);
    api_check(type->kind != CTypeKind::FUNC);
    api_check(type->kind != CTypeKind::VOID);
    api_check(!dependant || type->selfref != LUA_NOREF);
}

CStructType::CStructType(lua_State* L, std::vector<CType*> ftypes, std::vector<std::string> fnames, std::string debugname, bool dependant, int& ffi_status)
    : debugname(std::move(debugname)), dependant(dependant)
{
    this->ft.type = FFI_TYPE_STRUCT;
    this->ft.size = 0;
    this->ft.alignment = 0;

    std::size_t nfields = ftypes.size();
    api_check(nfields == fnames.size());
    if (nfields == 0) return;

    // build ffi_type elements
    this->ft.elements = new ffi_type*[nfields + 1];
    for (std::size_t i = 0; i < nfields; ++i) {
        this->ft.elements[i] = getCStructFieldFFIType(L, ftypes[i], dependant);
    }
    this->ft.elements[nfields] = nullptr;

    // get offsets, deleted after this->fields is built
    std::size_t* offsets = new std::size_t[nfields];
    ffi_status = ffi_get_struct_offsets(FFI_DEFAULT_ABI, &this->ft, offsets);

    // build fields
    this->fields.reserve(nfields);
    for (std::size_t i = 0, j = 0; i < nfields; ++i) {
        std::size_t offset;

        if (ftypes[i]->kind == CTypeKind::ARRAY && ftypes[i]->array->size == 0) {
            if (i == 0) continue;
                
            offset = this->fields[i - 1].offset + sizeOfCType(this->fields[i - 1].type);
        } else {
            offset = offsets[j++];
        }

        this->fields.push_back(CStructFieldType(ftypes[i], std::move(fnames[i]), offset, dependant));
    }
    delete[] offsets;

    // build field_map
    this->field_map.reserve(nfields);
    for (std::size_t i = 0; i < nfields; ++i) {
        this->field_map[this->fields[i].name] = i;
    }
}

CStructType::~CStructType()
{
    if (this->ft.elements != nullptr) {
        delete[] this->ft.elements;
    }
}

void CStructType::releaseDependencies(lua_State* L) const
{
    if (!this->dependant) return;

    for (CStructFieldType field : this->fields) {
        releaseCType(L, field.type);
    }
}

// if dependant is true, retainCType must have been called on each field type before calling newCStructType
CType* newCStructType(lua_State* L, std::vector<CType*> ftypes,std::vector<std::string> fnames, std::string debugname, bool dependant)
{
    int ffi_status;

    CType* ct = newCType(L, CTypeKind::STRUCT, kFFICStructTypeTag);
    ct->struct_ = new CStructType(L, ftypes, fnames, debugname, dependant, ffi_status);
    
    if (LUAU_UNLIKELY(ffi_status != FFI_OK)) {
        luaL_errorL(L, "ffi_struct_offsets fail: %s", ffiStatusToString(ffi_status).c_str());
    }

    return ct;
}

CType* toCStructType(lua_State* L, int idx)
{
    return static_cast<CType*>(lua_touserdatatagged(L, idx, kFFICStructTypeTag));
}

CType* checkCStructType(lua_State* L, int idx)
{
    CType* ct = toCStructType(L, idx);
    if (ct != nullptr)
        return ct;
    
    luaL_typeerror(L, idx, kCStructType);
    return nullptr;
}

int lua_tostring_CStructType(lua_State* L)
{
    CType* ct = checkCStructType(L, 1);
    return handleCTypeToString(L, ct);
}

int lua_namecall_CStructType(lua_State* L)
{
    CType* ct = checkCStructType(L, 1);
    const char* method = lua_namecallatom(L, nullptr);
    if (method == nullptr)
        luaL_error(L, "attempt to namecall CStructType with invalid method");

    return handleCTypeNamecall(L, ct);
}

void lua_dtor_CStructType(lua_State* L, void* ud)
{
    CType* ct = static_cast<CType*>(ud);
    ct->struct_->releaseDependencies(L);
    delete ct->struct_;
}

const luaL_Reg mt[] = {
    {"__tostring", lua_tostring_CStructType},
    {"__namecall", lua_namecall_CStructType},

    {nullptr, nullptr},
};

void initCStructType(lua_State* L)
{
    luaL_newmetatable(L, kCStructType);

    lua_pushvalue(L, -1);
    lua_setuserdatametatable(L, kFFICStructTypeTag);

    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");

    lua_pushcfunction(L, lua_tostring_CStructType, (std::string(kCStructType) + "__tostring").c_str());
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lua_namecall_CStructType, (std::string(kCStructType) + "__namecall").c_str());
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_setuserdatadtor(L, kFFICStructTypeTag, lua_dtor_CStructType);
}

}