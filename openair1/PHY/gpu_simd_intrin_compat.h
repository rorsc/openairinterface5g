/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once
#include <stdint.h>

#if defined(__CUDACC__)
  #include <cuda_runtime.h>
#endif

/* IMPORTANT:
   nvcc compiles each TU in (at least) a host pass and a device pass.
   __CUDA_ARCH__ is NOT set in the host pass.
   If we gate __device__ on it, we end up with host-only definitions that
   can't be called from kernels.
*/
#if defined(__CUDACC__)
  #define GPUHD __host__ __device__ __forceinline__
#else
  #define GPUHD static inline
#endif

/* ---------- lane helpers ---------- */
typedef union {
  uint32_t u;
  int8_t   s8[4];
  uint8_t  u8[4];
  int16_t  s16[2];
  uint16_t u16[2];
} gpu_u32_lanes;

/* ---------- helper: signed rounded average for int8 ----------
   avg = (a+b + (a+b>=0)) >> 1
*/
/* ===================== Intrinsic equivalents ===================== */

GPUHD uint32_t gpu_vcmplts4(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vcmplts4(a, b);
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) R.u8[i] = ((int)A.s8[i] < (int)B.s8[i]) ? 0xFFu : 0x00u;
  return R.u;
#endif
}

GPUHD uint32_t gpu_vcmpeq4(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vcmpeq4(a, b);
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) R.u8[i] = (A.u8[i] == B.u8[i]) ? 0xFFu : 0x00u;
  return R.u;
#endif
}

GPUHD uint32_t gpu_vneg4(uint32_t a) {
#if defined(__CUDA_ARCH__)
  return __vneg4(a);
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) R.u8[i] = (uint8_t)(-(int)A.s8[i]);
  return R.u;
#endif
}

GPUHD uint32_t gpu_vabs4(uint32_t a) {
#if defined(__CUDA_ARCH__)
  return __vabs4(a);
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) {
    int t = (int)A.s8[i];
    if (t < 0) t = -t;
    R.u8[i] = (uint8_t)t;
  }
  return R.u;
#endif
}

GPUHD uint32_t gpu_vminu4(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vminu4(a, b);
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) { uint8_t x = A.u8[i], y = B.u8[i]; R.u8[i] = (x < y) ? x : y; }
  return R.u;
#endif
}

/* per-halfword signed min */
GPUHD uint32_t gpu_vmins2(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vmins2(a, b);
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 2; ++i) {
    int16_t x = A.s16[i], y = B.s16[i];
    R.s16[i] = (x < y) ? x : y;
  }
  return R.u;
#endif
}

GPUHD uint32_t gpu_vmaxu4(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vmaxu4(a, b);
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) { uint8_t x = A.u8[i], y = B.u8[i]; R.u8[i] = (x > y) ? x : y; }
  return R.u;
#endif
}

/* per-halfword signed max */
GPUHD uint32_t gpu_vmaxs2(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vmaxs2(a, b);
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 2; ++i) {
    int16_t x = A.s16[i], y = B.s16[i];
    R.s16[i] = (x > y) ? x : y;
  }
  return R.u;
#endif
}

/* per-byte signed saturating subtract: clamp(a - b) to [-128,127] */
GPUHD uint32_t gpu_vsubss4(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vsubss4(a, b);
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) {
    int t = (int)A.s8[i] - (int)B.s8[i];
    if (t >  127) t =  127;
    if (t < -128) t = -128;
    R.s8[i] = (int8_t)t;
  }
  return R.u;
#endif
}

GPUHD uint32_t gpu_vaddss2(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vaddss2(a, b);
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 2; ++i) {
    int t = (int)A.s16[i] + (int)B.s16[i];
    if (t >  32767) t =  32767;
    if (t < -32768) t = -32768;
    R.s16[i] = (int16_t)t;
  }
  return R.u;
#endif
}

/* per-halfword subtract (wrap-around): a - b */
GPUHD uint32_t gpu_vsub2(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vsub2(a, b);
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 2; ++i) {
   /* wrap-around in int16_t */
    R.s16[i] = (int16_t)((int)A.s16[i] - (int)B.s16[i]);
  }
  return R.u;
#endif
}
