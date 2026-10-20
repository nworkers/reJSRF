/* rejsrf: the title's data addresses that differ between the builds.
 *
 * The US and Japanese builds have the same code at the same addresses, so
 * function names (sub_XXXXXXXX) are shared; their data moved a little
 * (docs/analysis/language.md section 10). Each pair below was found by
 * reading the operand of the instruction at the same address in both XBEs
 * (scripts/analysis/language/variant_addresses.py). The build picks one set
 * with REJSRF_VARIANT (CMake), which defines REJSRF_VARIANT_JP for the
 * Japanese build (docs/design/20261020-i108-japanese-variant.md).
 *
 * Plain C: the decompiled sources include it next to the generated code. */
#ifndef REJSRF_JSRF_ADDRESSES_H
#define REJSRF_JSRF_ADDRESSES_H

#if defined(REJSRF_VARIANT_JP)

/* The language index (getter sub_00043700, 191 reads); bss here. */
#define JSRF_LANGUAGE_INDEX 0x00251F58u
/* MMATRIX globals (decomp/mmatrix.c). */
#define JSRF_MMATRIX_STACK_CELL 0x00264BECu
#define JSRF_MMATRIX_SINCOS_TABLE 0x00264B8Cu
#define JSRF_MMATRIX_WORK_MATRIX 0x00264BF0u
#define JSRF_MMATRIX_WORK_SCALAR 0x00264BC0u
#define JSRF_MMATRIX_WORK_VECTOR 0x0022E590u

#else

#define JSRF_LANGUAGE_INDEX 0x001FC724u
#define JSRF_MMATRIX_STACK_CELL 0x00264C04u
#define JSRF_MMATRIX_SINCOS_TABLE 0x00264BA4u
#define JSRF_MMATRIX_WORK_MATRIX 0x00264C08u
#define JSRF_MMATRIX_WORK_SCALAR 0x00264BD8u
#define JSRF_MMATRIX_WORK_VECTOR 0x0022E5B0u

#endif

/* The camera's frustum values (decomp/camera_frustum.c): the same address in
 * both builds. */
#define JSRF_FRUSTUM_VSECANT_A 0x00264E78u /* 1 / cos(half vertical angle) */
#define JSRF_FRUSTUM_VTAN_NEG 0x00264E7Cu  /* +/- tan(half vertical angle) */
#define JSRF_FRUSTUM_HSECANT_A 0x00264E80u /* 1 / cos(half horizontal angle) */
#define JSRF_FRUSTUM_HTAN_NEG 0x00264E84u  /* -tan(half horizontal angle) */
#define JSRF_FRUSTUM_HSECANT_B 0x00264E88u /* 1 / cos(half horizontal angle) */
#define JSRF_FRUSTUM_HTAN_POS 0x00264E8Cu  /* tan(half horizontal angle) */
#define JSRF_FRUSTUM_VSECANT_B 0x00264E90u /* 1 / cos(half vertical angle) */
#define JSRF_FRUSTUM_VTAN_POS 0x00264E94u  /* +/- tan(half vertical angle) */

#endif /* REJSRF_JSRF_ADDRESSES_H */
