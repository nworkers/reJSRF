/* Decompiled JSRF functions: the MMATRIX matrix stack (section MMATRIX,
 * 0x001BA8A0..0x001BBAB0; issue #73, docs/design/20261016-i073-mmatrix.md).
 *
 * A stack of row-major 4x4 float matrices (64 bytes each, 16-byte aligned).
 * The guest dword at kStackCell holds a pointer to the cell that holds the
 * current matrix's address; push copies the current matrix to the next slot.
 * Vectors are row vectors: a point p maps to p.x*R0 + p.y*R1 + p.z*R2 + R3.
 *
 * The originals are short SSE sequences called very often; lifted, each call
 * pays more for the guest-register plumbing than for its arithmetic. These
 * versions compute the same lanes in plain C, in the same order and grouping
 * (IEEE single precision, so the results are bit-identical with the lifted
 * lane-wise C), and leave the general registers, the memory (including stack
 * slots below esp) and XMM0..7 as the originals do; rejsrf_decomp_check
 * compares all three. No intrinsics: this code builds for every target.
 * Prefetches are omitted. */

#include "recomp_types.h"
#include "rejsrf/jsrf_addresses.h"

#include <string.h>

extern void JSRF_DECOMP_001BA8A0_gen(void);
extern void JSRF_DECOMP_001BA8F0_gen(void);
extern void JSRF_DECOMP_001BA950_gen(void);
extern void JSRF_DECOMP_001BA9B0_gen(void);
extern void JSRF_DECOMP_001BA9C0_gen(void);
extern void JSRF_DECOMP_001BAA00_gen(void);
extern void JSRF_DECOMP_001BAA50_gen(void);
extern void JSRF_DECOMP_001BAA60_gen(void);
extern void JSRF_DECOMP_001BAAA0_gen(void);
extern void JSRF_DECOMP_001BAAE0_gen(void);
extern void JSRF_DECOMP_001BAB20_gen(void);
extern void JSRF_DECOMP_001BABA0_gen(void);
extern void JSRF_DECOMP_001BAC00_gen(void);
extern void JSRF_DECOMP_001BAC50_gen(void);
extern void JSRF_DECOMP_001BAC70_gen(void);
extern void JSRF_DECOMP_001BADB0_gen(void);
extern void JSRF_DECOMP_001BB270_gen(void);
extern void JSRF_DECOMP_001BB2B0_gen(void);
extern void JSRF_DECOMP_001BB2E0_gen(void);
extern void JSRF_DECOMP_001BB330_gen(void);
extern void JSRF_DECOMP_001BB380_gen(void);

/* Guest globals the library uses (per build: rejsrf/jsrf_addresses.h). */
#define kStackCell JSRF_MMATRIX_STACK_CELL      /* -> cell holding the current matrix address */
#define kSinCosTable JSRF_MMATRIX_SINCOS_TABLE  /* -> table of (a, b) float pairs by angle */
#define kWorkMatrix JSRF_MMATRIX_WORK_MATRIX    /* -> scratch matrix of 0x001BAC70 */
#define kWorkScalar JSRF_MMATRIX_WORK_SCALAR    /* float scratch of 0x001BAC70 */
#define kWorkVector JSRF_MMATRIX_WORK_VECTOR    /* -> vector read by 0x001BAC70 */

/* Host pointer to a guest address; the base is read once per function. */
#define GUEST_BASE() const uintptr_t base = (uintptr_t)g_xbox_mem_offset
#define PF(addr) ((void*)(base + (uint32_t)(addr)))

/* The four float lanes of an XMM register. */
typedef struct Lanes
{
    float f[4];
} Lanes;

/* movups / movaps load and store (bit patterns copied unchanged). */
static __inline Lanes Load4(uintptr_t base, uint32_t addr)
{
    Lanes r;
    memcpy(r.f, PF(addr), 16);
    return r;
}

static __inline void Store4(uintptr_t base, uint32_t addr, Lanes v)
{
    memcpy(PF(addr), v.f, 16);
}

/* movss xmm, [addr]: the float in lane 0, zeros above. */
static __inline Lanes Scalar(uintptr_t base, uint32_t addr)
{
    Lanes r = {{0.0f, 0.0f, 0.0f, 0.0f}};
    memcpy(&r.f[0], PF(addr), 4);
    return r;
}

/* movss [addr], xmm: lane 0. */
static __inline void StoreScalar(uintptr_t base, uint32_t addr, Lanes v)
{
    memcpy(PF(addr), &v.f[0], 4);
}

/* shufps x, x, lane * 0x55: every lane from one lane. */
static __inline Lanes Broadcast(Lanes v, int lane)
{
    const float x = v.f[lane];
    Lanes r = {{x, x, x, x}};
    return r;
}

/* movss xmm, [addr] then shufps xmm, xmm, 0. */
static __inline Lanes Splat(uintptr_t base, uint32_t addr)
{
    return Broadcast(Scalar(base, addr), 0);
}

/* mulps, addps, subps: lane by lane, first operand first. */
static __inline Lanes Mul(Lanes a, Lanes b)
{
    Lanes r = {{a.f[0] * b.f[0], a.f[1] * b.f[1], a.f[2] * b.f[2], a.f[3] * b.f[3]}};
    return r;
}

static __inline Lanes Add(Lanes a, Lanes b)
{
    Lanes r = {{a.f[0] + b.f[0], a.f[1] + b.f[1], a.f[2] + b.f[2], a.f[3] + b.f[3]}};
    return r;
}

static __inline Lanes Sub(Lanes a, Lanes b)
{
    Lanes r = {{a.f[0] - b.f[0], a.f[1] - b.f[1], a.f[2] - b.f[2], a.f[3] - b.f[3]}};
    return r;
}

/* addss, subss: lane 0 only, the rest from the first operand. */
static __inline Lanes AddScalar(Lanes a, Lanes b)
{
    a.f[0] = a.f[0] + b.f[0];
    return a;
}

static __inline Lanes SubScalar(Lanes a, Lanes b)
{
    a.f[0] = a.f[0] - b.f[0];
    return a;
}

static __inline uint32_t CurrentMatrix(uintptr_t base)
{
    const uint32_t cell = *(const uint32_t*)(base + kStackCell);
    return *(const uint32_t*)(base + cell);
}

#define SET_XMM(reg, v) memcpy(&(reg), (v).f, 16)

/* `ret`: pop the return address. */
#define RET() (g_esp += 4u)

/* 0x001BA8A0: M = identity. eax = M, ecx = 0, edx = 1.0f. */
void JSRF_DECOMP_001BA8A0(void)
{
    GUEST_BASE();
    const uint32_t m = CurrentMatrix(base);
    uint32_t* d = (uint32_t*)(base + m);
    for (int i = 0; i < 16; ++i)
    {
        d[i] = (i % 5 == 0) ? 0x3F800000u : 0u;
    }
    g_eax = m;
    g_ecx = 0;
    g_edx = 0x3F800000u;
    RET();
}

/* 0x001BA8F0, ecx = source: M = [ecx]. xmm0..3 = the rows. */
void JSRF_DECOMP_001BA8F0(void)
{
    GUEST_BASE();
    const uint32_t m = CurrentMatrix(base);
    const uint32_t src = g_ecx;
    const Lanes r0 = Load4(base, src);
    const Lanes r1 = Load4(base, src + 0x10);
    const Lanes r2 = Load4(base, src + 0x20);
    const Lanes r3 = Load4(base, src + 0x30);
    Store4(base, m, r0);
    Store4(base, m + 0x10, r1);
    Store4(base, m + 0x20, r2);
    Store4(base, m + 0x30, r3);
    SET_XMM(g_xmm0, r0);
    SET_XMM(g_xmm1, r1);
    SET_XMM(g_xmm2, r2);
    SET_XMM(g_xmm3, r3);
    g_eax = m;
    RET();
}

/* 0x001BA950, ecx = destination: [ecx] = M. */
void JSRF_DECOMP_001BA950(void)
{
    GUEST_BASE();
    const uint32_t m = CurrentMatrix(base);
    const uint32_t dst = g_ecx;
    const Lanes r0 = Load4(base, m);
    const Lanes r1 = Load4(base, m + 0x10);
    const Lanes r2 = Load4(base, m + 0x20);
    const Lanes r3 = Load4(base, m + 0x30);
    Store4(base, dst, r0);
    Store4(base, dst + 0x10, r1);
    Store4(base, dst + 0x20, r2);
    Store4(base, dst + 0x30, r3);
    SET_XMM(g_xmm0, r0);
    SET_XMM(g_xmm1, r1);
    SET_XMM(g_xmm2, r2);
    SET_XMM(g_xmm3, r3);
    g_eax = m;
    RET();
}

/* 0x001BA9B0: eax = M. */
void JSRF_DECOMP_001BA9B0(void)
{
    GUEST_BASE();
    g_eax = CurrentMatrix(base);
    RET();
}

/* 0x001BA9C0: push a copy of M. edx = the stack cell, eax = old M,
 * ecx = new M. */
void JSRF_DECOMP_001BA9C0(void)
{
    GUEST_BASE();
    const uint32_t cell = MEM32(kStackCell);
    const uint32_t m = MEM32(cell);
    const uint32_t next = m + 0x40;
    MEM32(cell) = next;
    const Lanes r0 = Load4(base, m);
    const Lanes r1 = Load4(base, m + 0x10);
    const Lanes r2 = Load4(base, m + 0x20);
    const Lanes r3 = Load4(base, m + 0x30);
    Store4(base, next, r0);
    Store4(base, next + 0x10, r1);
    Store4(base, next + 0x20, r2);
    Store4(base, next + 0x30, r3);
    SET_XMM(g_xmm0, r0);
    SET_XMM(g_xmm1, r1);
    SET_XMM(g_xmm2, r2);
    SET_XMM(g_xmm3, r3);
    g_edx = cell;
    g_eax = m;
    g_ecx = next;
    RET();
}

/* 0x001BAA00: push an identity matrix. eax = new M, ecx = 0, edx = 1.0f. */
void JSRF_DECOMP_001BAA00(void)
{
    GUEST_BASE();
    const uint32_t cell = MEM32(kStackCell);
    const uint32_t next = MEM32(cell) + 0x40;
    MEM32(cell) = next;
    uint32_t* d = (uint32_t*)(base + next);
    for (int i = 0; i < 16; ++i)
    {
        d[i] = (i % 5 == 0) ? 0x3F800000u : 0u;
    }
    g_eax = next;
    g_ecx = 0;
    g_edx = 0x3F800000u;
    RET();
}

/* 0x001BAA50: pop. eax = the stack cell, ecx = new M. */
void JSRF_DECOMP_001BAA50(void)
{
    const uint32_t cell = MEM32(kStackCell);
    const uint32_t m = MEM32(cell) - 0x40;
    MEM32(cell) = m;
    g_eax = cell;
    g_ecx = m;
    RET();
}

/* Translate M by (x, y, z) at x_addr, y_addr, z_addr: R3 = (x*R0 + R3) +
 * (y*R1 + z*R2). Returns M; xmm0..2 as the original leaves them. */
static __inline uint32_t Translate(uintptr_t base, uint32_t x_addr, uint32_t y_addr, uint32_t z_addr)
{
    const uint32_t m = CurrentMatrix(base);
    Lanes x0 = Mul(Splat(base, x_addr), Load4(base, m));
    Lanes x1 = Mul(Splat(base, y_addr), Load4(base, m + 0x10));
    const Lanes x2 = Mul(Splat(base, z_addr), Load4(base, m + 0x20));
    x0 = Add(x0, Load4(base, m + 0x30));
    x1 = Add(x1, x2);
    x0 = Add(x0, x1);
    Store4(base, m + 0x30, x0);
    SET_XMM(g_xmm0, x0);
    SET_XMM(g_xmm1, x1);
    SET_XMM(g_xmm2, x2);
    return m;
}

/* 0x001BAA60, cdecl (float x, float y, float z): translate M. eax = M. */
void JSRF_DECOMP_001BAA60(void)
{
    GUEST_BASE();
    const uint32_t esp = g_esp;
    g_eax = Translate(base, esp + 4, esp + 8, esp + 12);
    RET();
}

/* 0x001BAAA0, ecx = const float v[3]: translate M by v. eax = M. */
void JSRF_DECOMP_001BAAA0(void)
{
    GUEST_BASE();
    const uint32_t v = g_ecx;
    g_eax = Translate(base, v, v + 4, v + 8);
    RET();
}

/* 0x001BAAE0, ecx = const float v[3]: R3 = (R3 - x*R0) - (y*R1 + z*R2).
 * eax = M. */
void JSRF_DECOMP_001BAAE0(void)
{
    GUEST_BASE();
    const uint32_t m = CurrentMatrix(base);
    const uint32_t v = g_ecx;
    const Lanes x0 = Mul(Splat(base, v), Load4(base, m));
    Lanes x1 = Mul(Splat(base, v + 4), Load4(base, m + 0x10));
    const Lanes x2 = Mul(Splat(base, v + 8), Load4(base, m + 0x20));
    Lanes x3 = Load4(base, m + 0x30);
    x3 = Sub(x3, x0);
    x1 = Add(x1, x2);
    x3 = Sub(x3, x1);
    Store4(base, m + 0x30, x3);
    SET_XMM(g_xmm0, x0);
    SET_XMM(g_xmm1, x1);
    SET_XMM(g_xmm2, x2);
    SET_XMM(g_xmm3, x3);
    g_eax = m;
    RET();
}

/* 0x001BAB20, ecx = float out[3], edx = const float in[3] or NULL: out =
 * in transformed by M (with translation); a NULL in copies R3.xyz. eax = M. */
void JSRF_DECOMP_001BAB20(void)
{
    GUEST_BASE();
    const uint32_t m = CurrentMatrix(base);
    const uint32_t out = g_ecx;
    const uint32_t in = g_edx;
    g_eax = m;
    if (in == 0)
    {
        const Lanes x0 = Scalar(base, m + 0x30);
        const Lanes x1 = Scalar(base, m + 0x34);
        const Lanes x2 = Scalar(base, m + 0x38);
        StoreScalar(base, out, x0);
        StoreScalar(base, out + 4, x1);
        StoreScalar(base, out + 8, x2);
        SET_XMM(g_xmm0, x0);
        SET_XMM(g_xmm1, x1);
        SET_XMM(g_xmm2, x2);
        RET();
        return;
    }
    Lanes x0 = Mul(Splat(base, in), Load4(base, m));
    Lanes x1 = Mul(Splat(base, in + 4), Load4(base, m + 0x10));
    Lanes x2 = Mul(Splat(base, in + 8), Load4(base, m + 0x20));
    x0 = Add(x0, Load4(base, m + 0x30));
    x1 = Add(x1, x2);
    x0 = Add(x0, x1);
    x1 = Broadcast(x0, 1);
    x2 = Broadcast(x0, 2);
    StoreScalar(base, out, x0);
    StoreScalar(base, out + 4, x1);
    StoreScalar(base, out + 8, x2);
    SET_XMM(g_xmm0, x0);
    SET_XMM(g_xmm1, x1);
    SET_XMM(g_xmm2, x2);
    RET();
}

/* 0x001BABA0, ecx = const struct { void* dst; const float* src; int
 * dst_stride; int src_stride; }*, edx = count (do-while, so 0 means 2^32):
 * dst[i] (16 bytes) = src[i] transformed by M. ebx, esi and edi are pushed
 * and restored; eax = the dst after the last, ecx = src_stride, edx = 0. */
void JSRF_DECOMP_001BABA0(void)
{
    GUEST_BASE();
    const uint32_t args = g_ecx;
    uint32_t esp = g_esp;
    esp -= 4;
    MEM32(esp) = g_ebx;
    esp -= 4;
    MEM32(esp) = g_esi;
    esp -= 4;
    MEM32(esp) = g_edi;
    uint32_t dst = MEM32(args);
    uint32_t src = MEM32(args + 4);
    const uint32_t dst_stride = MEM32(args + 8);
    const uint32_t src_stride = MEM32(args + 12);
    const uint32_t m = CurrentMatrix(base);
    uint32_t count = g_edx;
    Lanes x0, x1, x2;
    do
    {
        /* The rows are memory operands in the loop: read them each time. */
        x0 = Mul(Splat(base, src), Load4(base, m));
        x1 = Mul(Splat(base, src + 4), Load4(base, m + 0x10));
        x2 = Mul(Splat(base, src + 8), Load4(base, m + 0x20));
        x0 = Add(x0, Load4(base, m + 0x30));
        x1 = Add(x1, x2);
        x0 = Add(x0, x1);
        Store4(base, dst, x0);
        src += src_stride;
        dst += dst_stride;
    } while (--count != 0);
    SET_XMM(g_xmm0, x0);
    SET_XMM(g_xmm1, x1);
    SET_XMM(g_xmm2, x2);
    g_eax = dst;
    g_ecx = src_stride;
    g_edx = 0;
    RET();
}

/* 0x001BAC00, ecx = float out[3], edx = const float in[3]: out = in rotated
 * by M without translation, ((x*R0 + y*R1) + z*R2). eax = M. */
void JSRF_DECOMP_001BAC00(void)
{
    GUEST_BASE();
    const uint32_t m = CurrentMatrix(base);
    const uint32_t out = g_ecx;
    const uint32_t in = g_edx;
    Lanes x0 = Mul(Splat(base, in), Load4(base, m));
    Lanes x1 = Mul(Splat(base, in + 4), Load4(base, m + 0x10));
    Lanes x2 = Mul(Splat(base, in + 8), Load4(base, m + 0x20));
    x0 = Add(x0, x1);
    x0 = Add(x0, x2);
    x1 = Broadcast(x0, 1);
    x2 = Broadcast(x0, 2);
    StoreScalar(base, out, x0);
    StoreScalar(base, out + 4, x1);
    StoreScalar(base, out + 8, x2);
    SET_XMM(g_xmm0, x0);
    SET_XMM(g_xmm1, x1);
    SET_XMM(g_xmm2, x2);
    g_eax = m;
    RET();
}

/* The angle rotations share their lookup: a 16-bit angle selects the (a, b)
 * pair at table + ((angle >> 1) & 0x7FF8) (arithmetic shift). */
static __inline uint32_t AngleEntry(uint32_t angle)
{
    return ((uint32_t)((int32_t)angle >> 1) & 0x7FF8u) + MEM32(kSinCosTable);
}

/* Rotation of rows (p, q) of M by the pair at the angle's entry, as one of
 * 0x001BB2E0 (rows 1, 2), 0x001BB330 (0, 2) and 0x001BB380 (0, 1):
 *   p' = b*Rp + sign_p * a*Rq,  q' = b*Rq + sign_q * a*Rp
 * with `p_add` choosing addps (p' = b*Rp + a*Rq, q' = b*Rq - a*Rp) or subps
 * (p' = b*Rp - a*Rq, q' = b*Rq + a*Rp). A zero angle changes nothing.
 * Updates eax and ecx like the originals (in the caller's state). */
typedef struct RotateState
{
    uint32_t eax;
    uint32_t ecx;
} RotateState;

static __inline void Rotate(uintptr_t base, RotateState* state, uint32_t row_p, uint32_t row_q, int p_add)
{
    if (state->ecx == 0)
    {
        return;
    }
    const uint32_t entry = AngleEntry(state->ecx);
    const uint32_t m = CurrentMatrix(base);
    const Lanes b = Splat(base, entry + 4);
    const Lanes a = Splat(base, entry);
    Lanes x0 = Mul(b, Load4(base, m + row_p));
    const Lanes x1 = Mul(a, Load4(base, m + row_q));
    Lanes x2 = Mul(b, Load4(base, m + row_q));
    const Lanes x3 = Mul(a, Load4(base, m + row_p));
    if (p_add)
    {
        x0 = Add(x0, x1);
        x2 = Sub(x2, x3);
    }
    else
    {
        x0 = Sub(x0, x1);
        x2 = Add(x2, x3);
    }
    Store4(base, m + row_p, x0);
    Store4(base, m + row_q, x2);
    SET_XMM(g_xmm0, x0);
    SET_XMM(g_xmm1, x1);
    SET_XMM(g_xmm2, x2);
    SET_XMM(g_xmm3, x3);
    state->eax = m;
    state->ecx = entry;
}

/* 0x001BB2E0, ecx = angle: rotate about X (rows 1 and 2). */
void JSRF_DECOMP_001BB2E0(void)
{
    GUEST_BASE();
    RotateState state = {g_eax, g_ecx};
    Rotate(base, &state, 0x10, 0x20, 1);
    g_eax = state.eax;
    g_ecx = state.ecx;
    RET();
}

/* 0x001BB330, ecx = angle: rotate about Y (rows 0 and 2). */
void JSRF_DECOMP_001BB330(void)
{
    GUEST_BASE();
    RotateState state = {g_eax, g_ecx};
    Rotate(base, &state, 0x00, 0x20, 0);
    g_eax = state.eax;
    g_ecx = state.ecx;
    RET();
}

/* 0x001BB380, ecx = angle: rotate about Z (rows 0 and 1). */
void JSRF_DECOMP_001BB380(void)
{
    GUEST_BASE();
    RotateState state = {g_eax, g_ecx};
    Rotate(base, &state, 0x00, 0x10, 1);
    g_eax = state.eax;
    g_ecx = state.ecx;
    RET();
}

/* 0x001BAC50, ecx = const int32_t angles[3] (x, y, z): rotate about Z, then
 * Y, then X, through 0x001BB380, 0x001BB330 and 0x001BB2E0. The pushes and
 * return addresses it leaves below esp are written as the original does;
 * edx ends as the Y angle. */
void JSRF_DECOMP_001BAC50(void)
{
    GUEST_BASE();
    const uint32_t angles = g_ecx;
    const uint32_t ax = MEM32(angles);
    const uint32_t ay = MEM32(angles + 4);
    const uint32_t az = MEM32(angles + 8);
    const uint32_t esp = g_esp;
    MEM32(esp - 4) = ax;
    MEM32(esp - 8) = ay;
    MEM32(esp - 12) = 0x001BAC5Fu;
    RotateState state = {ax, az};
    Rotate(base, &state, 0x00, 0x10, 1);
    MEM32(esp - 8) = 0x001BAC65u;
    state.ecx = ay;
    Rotate(base, &state, 0x00, 0x20, 0);
    MEM32(esp - 4) = 0x001BAC6Bu;
    state.ecx = ax;
    Rotate(base, &state, 0x10, 0x20, 1);
    g_eax = state.eax;
    g_ecx = state.ecx;
    g_edx = ay;
    RET();
}

/* 0x001BAC70, ecx = const float p[4] (p[3] as an angle dword): builds a
 * scratch matrix W at [kWorkMatrix] from p, the angle's (a, b) and the vector
 * at [kWorkVector], then sets M's upper 3x3 rows to (rows of a matrix made
 * from W) times M. kWorkScalar keeps b. eax = M, ecx = W, edx = the vector. */
void JSRF_DECOMP_001BAC70(void)
{
    GUEST_BASE();
    const uint32_t p = g_ecx;
    const uint32_t vec = MEM32(kWorkVector);
    const uint32_t entry = AngleEntry(MEM32(p + 0xC));
    Lanes x0 = Scalar(base, entry + 4);
    Lanes x1 = Scalar(base, entry);
    const uint32_t w = MEM32(kWorkMatrix);
    StoreScalar(base, kWorkScalar, x0);
    Lanes x2 = Load4(base, vec);
    Lanes x3 = Load4(base, p);
    x0 = Broadcast(x0, 0);
    x1 = Broadcast(x1, 0);
    x2 = Sub(x2, x0);
    x1 = Mul(x1, x3);
    x2 = Mul(x2, x3);
    Store4(base, w + 0x30, x1);
    Lanes x4 = Broadcast(x3, 1);
    Lanes x5 = Broadcast(x3, 2);
    x3 = Broadcast(x3, 0);
    x3 = Mul(x3, x2);
    x4 = Mul(x4, x2);
    x5 = Mul(x5, x2);
    Store4(base, w, x3);
    Store4(base, w + 0x10, x4);
    Store4(base, w + 0x20, x5);

    const uint32_t m = CurrentMatrix(base);
    /* Scalar sums of W entries (re-read from memory, as the original does),
     * each splatted and multiplied by a row of M. */
#define SUM(a_off, op, b_addr) op(Scalar(base, w + (a_off)), Scalar(base, (b_addr)))
#define ROW(s0, s1, s2)                                                         \
    Add(Add(Mul(Broadcast(s0, 0), Load4(base, m)), \
                          Mul(Broadcast(s1, 0), Load4(base, m + 0x10))), \
               Mul(Broadcast(s2, 0), Load4(base, m + 0x20)))
    const Lanes a0 = SUM(0x00, AddScalar, kWorkScalar);
    const Lanes a1 = SUM(0x04, AddScalar, w + 0x38);
    const Lanes a2 = SUM(0x08, SubScalar, w + 0x34);
    x0 = ROW(a0, a1, a2);
    const Lanes b0 = SUM(0x10, SubScalar, w + 0x38);
    const Lanes b1 = SUM(0x14, AddScalar, kWorkScalar);
    const Lanes b2 = SUM(0x18, AddScalar, w + 0x30);
    x1 = ROW(b0, b1, b2);
    const Lanes c0 = SUM(0x20, AddScalar, w + 0x34);
    const Lanes c1 = SUM(0x24, SubScalar, w + 0x30);
    const Lanes c2 = SUM(0x28, AddScalar, kWorkScalar);
    x2 = ROW(c0, c1, c2);
    /* xmm3 and xmm4 end as the last row's second and third products. */
    x3 = Mul(Broadcast(c1, 0), Load4(base, m + 0x10));
    x4 = Mul(Broadcast(c2, 0), Load4(base, m + 0x20));
#undef ROW
#undef SUM
    Store4(base, m, x0);
    Store4(base, m + 0x10, x1);
    Store4(base, m + 0x20, x2);
    SET_XMM(g_xmm0, x0);
    SET_XMM(g_xmm1, x1);
    SET_XMM(g_xmm2, x2);
    SET_XMM(g_xmm3, x3);
    SET_XMM(g_xmm4, x4);
    SET_XMM(g_xmm5, x5);
    g_eax = m;
    g_ecx = w;
    g_edx = vec;
    RET();
}

/* 0x001BADB0, ecx = const float A[16]: M = A * M (row 3 included:
 * (a30*R0 + a31*R1) + (a32*R2 + a33*R3)). The rows of M are read first; A's
 * rows are read as each result row is stored, as the original does. eax = M,
 * xmm4..7 = the old rows. */
void JSRF_DECOMP_001BADB0(void)
{
    GUEST_BASE();
    const uint32_t m = CurrentMatrix(base);
    const uint32_t a = g_ecx;
    const Lanes r0 = Load4(base, m);
    const Lanes r1 = Load4(base, m + 0x10);
    const Lanes r2 = Load4(base, m + 0x20);
    const Lanes r3 = Load4(base, m + 0x30);
    Lanes x0, x1, x2, x3;
    for (uint32_t row = 0; row < 3; ++row)
    {
        const uint32_t ar = a + row * 0x10;
        x0 = Mul(Splat(base, ar), r0);
        x1 = Mul(Splat(base, ar + 4), r1);
        x2 = Mul(Splat(base, ar + 8), r2);
        x0 = Add(x0, x1);
        x0 = Add(x0, x2);
        Store4(base, m + row * 0x10, x0);
    }
    x0 = Mul(Splat(base, a + 0x30), r0);
    x1 = Mul(Splat(base, a + 0x34), r1);
    x2 = Mul(Splat(base, a + 0x38), r2);
    x3 = Mul(Splat(base, a + 0x3C), r3);
    x0 = Add(x0, x1);
    x2 = Add(x2, x3);
    x0 = Add(x0, x2);
    Store4(base, m + 0x30, x0);
    SET_XMM(g_xmm0, x0);
    SET_XMM(g_xmm1, x1);
    SET_XMM(g_xmm2, x2);
    SET_XMM(g_xmm3, x3);
    SET_XMM(g_xmm4, r0);
    SET_XMM(g_xmm5, r1);
    SET_XMM(g_xmm6, r2);
    SET_XMM(g_xmm7, r3);
    g_eax = m;
    RET();
}

/* 0x001BB270, ecx = const float s[3]: R0..R2 scaled by s.x, s.y, s.z.
 * eax = M. */
void JSRF_DECOMP_001BB270(void)
{
    GUEST_BASE();
    const uint32_t m = CurrentMatrix(base);
    const uint32_t s = g_ecx;
    const Lanes x0 = Mul(Splat(base, s), Load4(base, m));
    const Lanes x1 = Mul(Splat(base, s + 4), Load4(base, m + 0x10));
    const Lanes x2 = Mul(Splat(base, s + 8), Load4(base, m + 0x20));
    Store4(base, m, x0);
    Store4(base, m + 0x10, x1);
    Store4(base, m + 0x20, x2);
    SET_XMM(g_xmm0, x0);
    SET_XMM(g_xmm1, x1);
    SET_XMM(g_xmm2, x2);
    g_eax = m;
    RET();
}

/* 0x001BB2B0, cdecl (float s): R0..R2 scaled by s. eax = M. */
void JSRF_DECOMP_001BB2B0(void)
{
    GUEST_BASE();
    const uint32_t m = CurrentMatrix(base);
    const Lanes s = Splat(base, g_esp + 4);
    const Lanes x0 = Mul(s, Load4(base, m));
    const Lanes x1 = Mul(s, Load4(base, m + 0x10));
    const Lanes x2 = Mul(s, Load4(base, m + 0x20));
    Store4(base, m, x0);
    Store4(base, m + 0x10, x1);
    Store4(base, m + 0x20, x2);
    SET_XMM(g_xmm0, x0);
    SET_XMM(g_xmm1, x1);
    SET_XMM(g_xmm2, x2);
    g_eax = m;
    RET();
}
