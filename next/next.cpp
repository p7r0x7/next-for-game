#define LIB_NAME "Next"

#include <dmsdk/sdk.h>
#include <stdlib.h>

#define XXH_INLINE_ALL
#include "xxhash.h"
#include "zstd.h"
#include "rand.h"

/* ============ zstd ============ */

// Contexts are reused across calls (plain ZSTD_compress allocates one per call).
static ZSTD_CCtx* g_cctx = 0;
static ZSTD_DCtx* g_dctx = 0;
static ZSTD_CCtx* GetCCtx() { if (!g_cctx) g_cctx = ZSTD_createCCtx(); return g_cctx; }
static ZSTD_DCtx* GetDCtx() { if (!g_dctx) g_dctx = ZSTD_createDCtx(); return g_dctx; }

// zstd.compress(str [, level=3]) -> str
static int Zstd_Compress(lua_State* L)
{
    size_t len; const char* src = luaL_checklstring(L, 1, &len);
    int level = (int)luaL_optinteger(L, 2, 3);
    ZSTD_CCtx* cc = GetCCtx();
    if (!cc) return luaL_error(L, "out of memory");
    size_t cap = ZSTD_compressBound(len);
    void* dst = malloc(cap ? cap : 1);
    if (!dst) return luaL_error(L, "out of memory");
    size_t n = ZSTD_compressCCtx(cc, dst, cap, src, len, level);
    if (ZSTD_isError(n)) { free(dst); return luaL_error(L, "zstd: %s", ZSTD_getErrorName(n)); }
    lua_pushlstring(L, (const char*)dst, n);
    free(dst);
    return 1;
}

// zstd.decompress(str [, max_size=256MB]) -> str
static int Zstd_Decompress(lua_State* L)
{
    size_t len; const char* src = luaL_checklstring(L, 1, &len);
    unsigned long long maxs = (unsigned long long)luaL_optnumber(L, 2, 256.0 * 1024 * 1024);
    unsigned long long size = ZSTD_getFrameContentSize(src, len);
    if (size == ZSTD_CONTENTSIZE_ERROR)   return luaL_error(L, "zstd: not a valid frame");
    if (size == ZSTD_CONTENTSIZE_UNKNOWN) return luaL_error(L, "zstd: frame has no content size");
    if (size > maxs)                      return luaL_error(L, "zstd: output exceeds max_size");
    ZSTD_DCtx* dc = GetDCtx();
    if (!dc) return luaL_error(L, "out of memory");
    void* dst = malloc(size ? (size_t)size : 1);
    if (!dst) return luaL_error(L, "out of memory");
    size_t n = ZSTD_decompressDCtx(dc, dst, (size_t)size, src, len);
    if (ZSTD_isError(n)) { free(dst); return luaL_error(L, "zstd: %s", ZSTD_getErrorName(n)); }
    lua_pushlstring(L, (const char*)dst, n);
    free(dst);
    return 1;
}

/* ============ xxh32 ============ */

// xxh32.hash(str [, seed=0]) -> number (uint32)
static int Xxh32_Hash(lua_State* L)
{
    size_t len; const char* s = luaL_checklstring(L, 1, &len);
    uint32_t seed = (uint32_t)luaL_optnumber(L, 2, 0);
    lua_pushnumber(L, (lua_Number)XXH32(s, len, seed));
    return 1;
}

/* ============ rand (LuaJIT FFI entry points) ============
 * Addresses are handed to Lua as lightuserdata; lua/bytekit_rand.lua casts and
 * calls them through the FFI. State structs and output buffers live in Lua FFI memory. */
extern "C" {
static void bk_sfc32_seed(sfc32_t* s, uint32_t seed)                        { sfc32_seed(s, seed); }
static void bk_sfc32_seed3(sfc32_t* s, uint32_t a, uint32_t b, uint32_t c)  { sfc32_seed3(s, a, b, c); }
static void bk_sfc32_fill_f64(sfc32_t* s, double* o, size_t n)              { sfc32_fill_f64(s, o, n); }
static void bk_sfc32_fill_unit(sfc32_t* s, double* o, size_t n)             { sfc32_fill_unit(s, o, n); }
static void bk_pcg32_seed(pcg32_t* p, uint64_t seed)                        { pcg32_seed(p, seed); }
static void bk_pcg32_fill_f64(pcg32_t* p, double* o, size_t n)              { pcg32_fill_f64(p, o, n); }
static void bk_pcg32_fill_unit(pcg32_t* p, double* o, size_t n)             { pcg32_fill_unit(p, o, n); }
}

#define PUSH_FN(name) do { lua_pushlightuserdata(L, (void*)&bk_##name); lua_setfield(L, -2, #name); } while (0)

// rand.ffi_ptrs() -> { sfc32_seed=<ptr>, ... }
static int Rand_FfiPtrs(lua_State* L)
{
    lua_newtable(L);
    PUSH_FN(sfc32_seed); PUSH_FN(sfc32_seed3); PUSH_FN(sfc32_fill_f64); PUSH_FN(sfc32_fill_unit);
    PUSH_FN(pcg32_seed); PUSH_FN(pcg32_fill_f64); PUSH_FN(pcg32_fill_unit);
    return 1;
}

/* ============ registration ============ */

static const luaL_reg Zstd_funcs[] = { {"compress", Zstd_Compress}, {"decompress", Zstd_Decompress}, {0,0} };
static const luaL_reg Xxh_funcs[]  = { {"hash", Xxh32_Hash}, {0,0} };
static const luaL_reg Rand_funcs[] = { {"ffi_ptrs", Rand_FfiPtrs}, {0,0} };

static dmExtension::Result InitializeNext(dmExtension::Params* params)
{
    lua_State* L = params->m_L;
    luaL_register(L, "zstd",  Zstd_funcs); lua_pop(L, 1);
    luaL_register(L, "xxh32", Xxh_funcs);  lua_pop(L, 1);
    luaL_register(L, "rand",  Rand_funcs); lua_pop(L, 1);
    dmLogInfo("Next: registered zstd, xxh32, rand (lua_State %p)", (void*)L);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeNext(dmExtension::Params*)
{
    if (g_cctx) { ZSTD_freeCCtx(g_cctx); g_cctx = 0; }
    if (g_dctx) { ZSTD_freeDCtx(g_dctx); g_dctx = 0; }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(Next, LIB_NAME, 0, 0, InitializeNext, 0, 0, FinalizeNext)
