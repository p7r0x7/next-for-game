#include <cstddef>
#define LIB_NAME "Next"

#include <dmsdk/sdk.h>
#include <stdlib.h>

#define XXH_INLINE_ALL
#include "xxhash.h"
#include "zstd.h"
#include "rand.h"


/* ---------------------------------- rand --------------------------------- */

#define PUSH_FN(fn) do { lua_pushlightuserdata(L, (void*)&fn); lua_setfield(L, -2, #fn); } while (0)

static int Rand_FfiPtrs(lua_State* L) {
    lua_newtable(L);
    PUSH_FN(entropy);
    PUSH_FN(sfc32_seed); PUSH_FN(sfc32_doubles);
    PUSH_FN(pcg32_seed); PUSH_FN(pcg32_doubles);
    return 1;
}


/* ---------------------------------- zstd --------------------------------- */

#define NEXT_MAX_BYTES (2u << 20)                             // 2 MiB cap, each direction
#define NEXT_COMPRESS_CAP ZSTD_COMPRESSBOUND(NEXT_MAX_BYTES)  // worst-case compressed size

static ZSTD_CCtx* g_cctx = 0;
static ZSTD_DCtx* g_dctx = 0;
static char* g_cbuf = 0;   // scratch for compressed output, NEXT_COMPRESS_CAP bytes
static char* g_dbuf = 0;   // scratch for decompressed output, NEXT_MAX_BYTES bytes

static ZSTD_CCtx* GetCCtx() { if (!g_cctx) g_cctx = ZSTD_createCCtx(); return g_cctx; }
static ZSTD_DCtx* GetDCtx() { if (!g_dctx) g_dctx = ZSTD_createDCtx(); return g_dctx; }

// zstd.compress(str [, level=3]) -> str
static int Zstd_Compress(lua_State* L) {
    size_t len; const char* src = luaL_checklstring(L, 1, &len);
    if (len > NEXT_MAX_BYTES) return luaL_error(L, "zstd: input exceeds 2 MiB");
    int level = (int)luaL_optinteger(L, 2, 3);

    ZSTD_CCtx* cc = GetCCtx();
    if (!cc) return luaL_error(L, "out of memory");

    // Allocate the scratch buffer on first use, then reuse it forever.
    if (!g_cbuf) {
        g_cbuf = (char*)malloc(NEXT_COMPRESS_CAP);
        if (!g_cbuf) return luaL_error(L, "out of memory");
    }

    size_t n = ZSTD_compressCCtx(cc, g_cbuf, NEXT_COMPRESS_CAP, src, len, level);
    if (ZSTD_isError(n)) return luaL_error(L, "zstd: %s", ZSTD_getErrorName(n));
    lua_pushlstring(L, g_cbuf, n);  // Lua copies g_cbuf
    return 1;
}

// zstd.decompress(str) -> str
static int Zstd_Decompress(lua_State* L) {
    size_t len; const char* src = luaL_checklstring(L, 1, &len);

    // Ask the frame header how big the output will be BEFORE allocating or decoding.
    unsigned long long size = ZSTD_getFrameContentSize(src, len);
    if (size == ZSTD_CONTENTSIZE_ERROR) return luaL_error(L, "zstd: not a valid frame");
    if (size == ZSTD_CONTENTSIZE_UNKNOWN) return luaL_error(L, "zstd: frame has no content size");
    if (size > NEXT_MAX_BYTES) return luaL_error(L, "zstd: output exceeds 4 MiB");

    ZSTD_DCtx* dc = GetDCtx();
    if (!dc) return luaL_error(L, "out of memory");

    if (!g_dbuf) {
        g_dbuf = (char*)malloc(NEXT_MAX_BYTES);
        if (!g_dbuf) return luaL_error(L, "out of memory");
    }

    // Capacity is NEXT_MAX_BYTES, not `size`: zstd then refuses to write past the buffer
    // even if the frame header lied about its size.
    size_t n = ZSTD_decompressDCtx(dc, g_dbuf, NEXT_MAX_BYTES, src, len);
    if (ZSTD_isError(n)) return luaL_error(L, "zstd: %s", ZSTD_getErrorName(n));
    lua_pushlstring(L, g_dbuf, n);
    return 1;
}


/* --------------------------------- xxh32 --------------------------------- */

// xxh32.hash(str [, seed=0]) -> number (uint32)
static int Xxh32_Hash(lua_State* L) {
    size_t len; const char* s = luaL_checklstring(L, 1, &len);
    uint32_t seed = (uint32_t)luaL_optnumber(L, 2, 0);
    lua_pushnumber(L, (lua_Number)XXH32(s, len, seed));
    return 1;
}


/* ----------------------------- registration ------------------------------ */

static const luaL_reg Zstd_funcs[] = { {"compress", Zstd_Compress}, {"decompress", Zstd_Decompress}, {0,0} };
static const luaL_reg Xxh_funcs[] = { {"hash", Xxh32_Hash}, {0,0} };
static const luaL_reg Rand_funcs[] = { {"ffi_ptrs", Rand_FfiPtrs}, {0,0} };

static dmExtension::Result InitializeNext(dmExtension::Params* params) {
    lua_State* L = params->m_L;
    luaL_register(L, "rand", Rand_funcs); lua_pop(L, 1);
    luaL_register(L, "zstd", Zstd_funcs); lua_pop(L, 1);
    luaL_register(L, "xxh32", Xxh_funcs); lua_pop(L, 1);
    dmLogInfo("Next: registered rand, zstd, and xxh32 (lua_State %p)", (void*)L);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeNext(dmExtension::Params*) {
    if (g_cctx) { ZSTD_freeCCtx(g_cctx); g_cctx = 0; }
    if (g_dctx) { ZSTD_freeDCtx(g_dctx); g_dctx = 0; }
    free(g_cbuf); g_cbuf = 0;
    free(g_dbuf); g_dbuf = 0;
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(Next, LIB_NAME, 0, 0, InitializeNext, 0, 0, FinalizeNext)
