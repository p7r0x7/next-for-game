local M = {}

local ffi = package.preload.ffi()

pcall(ffi.cdef, [[
    typedef struct { uint32_t a, b, c, d; } sfc32_t;
    typedef struct { uint64_t state; } pcg32_t;
]])

local ptrs = rand.ffi_ptrs()
local entropy_fn = ffi.cast("void(*)(char*, size_t)", ptrs.entropy)
local sfc_seed_fn = ffi.cast("void(*)(sfc32_t*, const char*)", ptrs.sfc32_seed)
local sfc_fill_fn = ffi.cast("void(*)(sfc32_t*, double*, int)", ptrs.sfc32_doubles)
local pcg_seed_fn = ffi.cast("void(*)(pcg32_t*, const char*)", ptrs.pcg32_seed)
local pcg_fill_fn = ffi.cast("void(*)(pcg32_t*, double*, int)", ptrs.pcg32_doubles)

local BUF_SIZE = 1024

function M.entropy(n)
    assert(n <= 256, "entropy(): n must be <= 256")
    local buf = ffi.new("char[?]", n)
    entropy_fn(buf, n)
    return buf
end

local pcg_state = ffi.new("pcg32_t[1]")
local pcg_buf = ffi.new("double[?]", BUF_SIZE)
local pcg_pos = BUF_SIZE
local pcg_seeded = false

function M.pcg_seed(seed)
    if type(seed) == "string" then
        assert(#seed >= 8, "pcg_seed(): seed needs 8 bytes")
    end
    pcg_seed_fn(pcg_state, seed)
    pcg_pos = BUF_SIZE
    pcg_seeded = true
end

function M.pcg_next()
    assert(pcg_seeded, "pcg_next(): call pcg_seed() to seed first", 2)
    if pcg_pos >= BUF_SIZE then
        pcg_fill_fn(pcg_state, pcg_buf, BUF_SIZE)
        pcg_pos = 0
    end
    local v = pcg_buf[pcg_pos]
    pcg_pos = pcg_pos + 1
    return v
end

local sfc_state = ffi.new("sfc32_t[1]")
local sfc_buf = ffi.new("double[?]", BUF_SIZE)
local sfc_pos = BUF_SIZE
sfc_seed_fn(sfc_state, M.entropy(12))

function M.sfc_next()
    if sfc_pos >= BUF_SIZE then
        sfc_fill_fn(sfc_state, sfc_buf, BUF_SIZE)
        sfc_pos = 0
    end
    local v = sfc_buf[sfc_pos]
    sfc_pos = sfc_pos + 1
    return v
end

return M