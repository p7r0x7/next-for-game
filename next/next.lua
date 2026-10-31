-- LuaJIT-only fast path. Batches are written by C straight into Lua-owned
-- double[] memory; Lua reads them as plain doubles (JIT-compiled loads, no boxing,
-- no allocation, no GC). NOTE: FFI arrays are ZERO-based: buf[0] .. buf[n-1].
local ffi = package.preload.ffi()

pcall(ffi.cdef, [[
typedef struct { uint32_t a, b, c, d; } sfc32_t;
typedef struct { uint64_t state; } pcg32_t;
]])

local P = rand.ffi_ptrs()
local cast = ffi.cast
local sfc_seed   = cast("void(*)(sfc32_t*, uint32_t)", P.sfc32_seed)
local sfc_seed3  = cast("void(*)(sfc32_t*, uint32_t, uint32_t, uint32_t)", P.sfc32_seed3)
local sfc_raw    = cast("void(*)(sfc32_t*, double*, size_t)", P.sfc32_fill_f64)
local sfc_unit   = cast("void(*)(sfc32_t*, double*, size_t)", P.sfc32_fill_unit)
local pcg_seed   = cast("void(*)(pcg32_t*, uint64_t)", P.pcg32_seed)
local pcg_raw    = cast("void(*)(pcg32_t*, double*, size_t)", P.pcg32_fill_f64)
local pcg_unit   = cast("void(*)(pcg32_t*, double*, size_t)", P.pcg32_fill_unit)

local M = {}

-- Allocate a reusable batch buffer (zero-based double array). Do this once.
function M.buffer(n) return ffi.new("double[?]", n) end

-- State objects (1-element arrays so they decay to pointers with no extra work)
function M.sfc32(seed, s2, s3)
  local st = ffi.new("sfc32_t[1]")
  if s2 then sfc_seed3(st, seed, s2, s3) else sfc_seed(st, seed) end
  return st
end
function M.pcg32(seed)
  local st = ffi.new("pcg32_t[1]")
  pcg_seed(st, seed)
  return st
end

-- Fill buf[0..n-1]. "raw" = uint32 values as doubles; "unit" = [0,1) doubles.
M.sfc32_raw, M.sfc32_unit = sfc_raw, sfc_unit
M.pcg32_raw, M.pcg32_unit = pcg_raw, pcg_unit

return M
