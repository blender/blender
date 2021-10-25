// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2010 Konstantinos Margaritis <markos@codex.gr>
// Heavily based on Gael's SSE version.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_PACKET_MATH_NEON_H
#define EIGEN_PACKET_MATH_NEON_H

namespace Eigen {

namespace internal {

#ifndef EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD
#define EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD 8
#endif

// FIXME NEON has 16 quad registers, but since the current register allocator
// is so bad, it is much better to reduce it to 8
#ifndef EIGEN_ARCH_DEFAULT_NUMBER_OF_REGISTERS
#define EIGEN_ARCH_DEFAULT_NUMBER_OF_REGISTERS 8
#endif

typedef float32x4_t Packet4f;
typedef int32x4_t   Packet4i;
typedef uint32x4_t  Packet4ui;

#define _EIGEN_DECLARE_CONST_Packet4f(NAME,X) \
  const Packet4f p4f_##NAME = pset1<Packet4f>(X)

#define _EIGEN_DECLARE_CONST_Packet4f_FROM_INT(NAME,X) \
  const Packet4f p4f_##NAME = vreinterpretq_f32_u32(pset1<int>(X))

#define _EIGEN_DECLARE_CONST_Packet4i(NAME,X) \
  const Packet4i p4i_##NAME = pset1<Packet4i>(X)

#if defined(__llvm__) && !defined(__clang__)
  //Special treatment for Apple's llvm-gcc, its NEON packet types are unions
  #define EIGEN_INIT_NEON_PACKET2(X, Y)       {{X, Y}}
  #define EIGEN_INIT_NEON_PACKET4(X, Y, Z, W) {{X, Y, Z, W}}
#else
  //Default initializer for packets
  #define EIGEN_INIT_NEON_PACKET2(X, Y)       {X, Y}
  #define EIGEN_INIT_NEON_PACKET4(X, Y, Z, W) {X, Y, Z, W}
#endif

// arm64 does have the pld instruction. If available, let's trust the __builtin_prefetch built-in function
// which available on LLVM and GCC (at least)
#if EIGEN_HAS_BUILTIN(__builtin_prefetch) || defined(__GNUC__)
  #define EIGEN_ARM_PREFETCH(ADDR) __builtin_prefetch(ADDR);
#elif defined __pld
  #define EIGEN_ARM_PREFETCH(ADDR) __pld(ADDR)
#elif !defined(__aarch64__)
  #define EIGEN_ARM_PREFETCH(ADDR) __asm__ __volatile__ ( "   pld [%[addr]]\n" :: [addr] "r" (ADDR) : "cc" );
#else
  // by default no explicit prefetching
  #define EIGEN_ARM_PREFETCH(ADDR)
#endif

template<> struct packet_traits<float>  : default_packet_traits
{
  typedef Packet4f type;
  enum {
    Vectorizable = 1,
    AlignedOnScalar = 1,
    size = 4,
   
    HasDiv  = 1,
    // FIXME check the Has*
    HasSin  = 0,
    HasCos  = 0,
    HasLog  = 0,
    HasExp  = 0,
    HasSqrt = 0
  };
};
template<> struct packet_traits<int>    : default_packet_traits
{
  typedef Packet4i type;
  enum {
    Vectorizable = 1,
    AlignedOnScalar = 1,
    size=4
    // FIXME check the Has*
  };
};

#if EIGEN_GNUC_AT_MOST(4,4) && !defined(__llvm__)
// workaround gcc 4.2, 4.3 and 4.4 compilatin issue
EIGEN_STRONG_INLINE float32x4_t vld1q_f32(const float* x) { return ::vld1q_f32((const float32_t*)x); }
EIGEN_STRONG_INLINE float32x2_t vld1_f32 (const float* x) { return ::vld1_f32 ((const float32_t*)x); }
EIGEN_STRONG_INLINE void        vst1q_f32(float* to, float32x4_t from) { ::vst1q_f32((float32_t*)to,from); }
EIGEN_STRONG_INLINE void        vst1_f32 (float* to, float32x2_t from) { ::vst1_f32 ((float32_t*)to,from); }
#endif

template<> struct unpacket_traits<Packet4f> { typedef float  type; enum {size=4}; };
template<> struct unpacket_traits<Packet4i> { typedef int    type; enum {size=4}; };

template<> EIGEN_STRONG_INLINE Packet4f pset1<Packet4f>(const float&  from) { return vdupq_n_f32(from); }
template<> EIGEN_STRONG_INLINE Packet4i pset1<Packet4i>(const int&    from)   { return vdupq_n_s32(from); }

template<> EIGEN_STRONG_INLINE Packet4f plset<float>(const float& a)
{
  Packet4f countdown = EIGEN_INIT_NEON_PACKET4(0, 1, 2, 3);
  return vaddq_f32(pset1<Packet4f>(a), countdown);
}
template<> EIGEN_STRONG_INLINE Packet4i plset<int>(const int& a)
{
  Packet4i countdown = EIGEN_INIT_NEON_PACKET4(0, 1, 2, 3);
  return vaddq_s32(pset1<Packet4i>(a), countdown);
}

template<> EIGEN_STRONG_INLINE Packet4f padd<Packet4f>(const Packet4f& a, const Packet4f& b) { return vaddq_f32(a,b); }
template<> EIGEN_STRONG_INLINE Packet4i padd<Packet4i>(const Packet4i& a, const Packet4i& b) { return vaddq_s32(a,b); }

template<> EIGEN_STRONG_INLINE Packet4f psub<Packet4f>(const Packet4f& a, const Packet4f& b) { return vsubq_f32(a,b); }
template<> EIGEN_STRONG_INLINE Packet4i psub<Packet4i>(const Packet4i& a, const Packet4i& b) { return vsubq_s32(a,b); }

template<> EIGEN_STRONG_INLINE Packet4f pnegate(const Packet4f& a) { return vnegq_f32(a); }
template<> EIGEN_STRONG_INLINE Packet4i pnegate(const Packet4i& a) { return vnegq_s32(a); }

template<> EIGEN_STRONG_INLINE Packet4f pconj(const Packet4f& a) { return a; }
template<> EIGEN_STRONG_INLINE Packet4i pconj(const Packet4i& a) { return a; }

template<> EIGEN_STRONG_INLINE Packet4f pmul<Packet4f>(const Packet4f& a, const Packet4f& b) { return vmulq_f32(a,b); }
template<> EIGEN_STRONG_INLINE Packet4i pmul<Packet4i>(const Packet4i& a, const Packet4i& b) { return vmulq_s32(a,b); }

template<> EIGEN_STRONG_INLINE Packet4f pdiv<Packet4f>(const Packet4f& a, const Packet4f& b)
{
  Packet4f inv, restep, div;

  // NEON does not offer a divide instruction, we have to do a reciprocal approximation
  // However NEON in contrast to other SIMD engines (AltiVec/SSE), offers
  // a reciprocal estimate AND a reciprocal step -which saves a few instructions
  // vrecpeq_f32() returns an estimate to 1/b, which we will finetune with
  // Newton-Raphson and vrecpsq_f32()
  inv = vrecpeq_f32(b);

  // This returns a differential, by which we will have to multiply inv to get a better
  // approximation of 1/b.
  restep = vrecpsq_f32(b, inv);
  inv = vmulq_f32(restep, inv);

  // Finally, multiply a by 1/b and get the wanted result of the division.
  div = vmulq_f32(a, inv);

  return div;
}
template<> EIGEN_STRONG_INLINE Packet4i pdiv<Packet4i>(const Packet4i& /*a*/, const Packet4i& /*b*/)
{ eigen_assert(false && "packet integer division are not supported by NEON");
  return pset1<Packet4i>(0);
}

// for some weird raisons, it has to be overloaded for packet of integers
template<> EIGEN_STRONG_INLINE Packet4f pmadd(const Packet4f& a, const Packet4f& b, const Packet4f& c) { return vmlaq_f32(c,a,b); }
template<> EIGEN_STRONG_INLINE Packet4i pmadd(const Packet4i& a, const Packet4i& b, const Packet4i& c) { return vmlaq_s32(c,a,b); }

template<> EIGEN_STRONG_INLINE Packet4f pmin<Packet4f>(const Packet4f& a, const Packet4f& b) { return vminq_f32(a,b); }
template<> EIGEN_STRONG_INLINE Packet4i pmin<Packet4i>(const Packet4i& a, const Packet4i& b) { return vminq_s32(a,b); }

template<> EIGEN_STRONG_INLINE Packet4f pmax<Packet4f>(const Packet4f& a, const Packet4f& b) { return vmaxq_f32(a,b); }
template<> EIGEN_STRONG_INLINE Packet4i pmax<Packet4i>(const Packet4i& a, const Packet4i& b) { return vmaxq_s32(a,b); }

// Logical Operations are not supported for float, so we have to reinterpret casts using NEON intrinsics
template<> EIGEN_STRONG_INLINE Packet4f pand<Packet4f>(const Packet4f& a, const Packet4f& b)
{
  return vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(a),vreinterpretq_u32_f32(b)));
}
template<> EIGEN_STRONG_INLINE Packet4i pand<Packet4i>(const Packet4i& a, const Packet4i& b) { return vandq_s32(a,b); }

template<> EIGEN_STRONG_INLINE Packet4f por<Packet4f>(const Packet4f& a, const Packet4f& b)
{
  return vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(a),vreinterpretq_u32_f32(b)));
}
template<> EIGEN_STRONG_INLINE Packet4i por<Packet4i>(const Packet4i& a, const Packet4i& b) { return vorrq_s32(a,b); }

template<> EIGEN_STRONG_INLINE Packet4f pxor<Packet4f>(const Packet4f& a, const Packet4f& b)
{
  return vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(a),vreinterpretq_u32_f32(b)));
}
template<> EIGEN_STRONG_INLINE Packet4i pxor<Packet4i>(const Packet4i& a, const Packet4i& b) { return veorq_s32(a,b); }

template<> EIGEN_STRONG_INLINE Packet4f pandnot<Packet4f>(const Packet4f& a, const Packet4f& b)
{
  return vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(a),vreinterpretq_u32_f32(b)));
}
template<> EIGEN_STRONG_INLINE Packet4i pandnot<Packet4i>(const Packet4i& a, const Packet4i& b) { return vbicq_s32(a,b); }

template<> EIGEN_STRONG_INLINE Packet4f pload<Packet4f>(const float* from) { EIGEN_DEBUG_ALIGNED_LOAD return vld1q_f32(from); }
template<> EIGEN_STRONG_INLINE Packet4i pload<Packet4i>(const int*   from) { EIGEN_DEBUG_ALIGNED_LOAD return vld1q_s32(from); }

template<> EIGEN_STRONG_INLINE Packet4f ploadu<Packet4f>(const float* from) { EIGEN_DEBUG_UNALIGNED_LOAD return vld1q_f32(from); }
template<> EIGEN_STRONG_INLINE Packet4i ploadu<Packet4i>(const int* from)   { EIGEN_DEBUG_UNALIGNED_LOAD return vld1q_s32(from); }

template<> EIGEN_STRONG_INLINE Packet4f ploaddup<Packet4f>(const float*   from)
{
  float32x2_t lo, hi;
  lo = vld1_dup_f32(from);
  hi = vld1_dup_f32(from+1);
  return vcombine_f32(lo, hi);
}
template<> EIGEN_STRONG_INLINE Packet4i ploaddup<Packet4i>(const int*     from)
{
  int32x2_t lo, hi;
  lo = vld1_dup_s32(from);
  hi = vld1_dup_s32(from+1);
  return vcombine_s32(lo, hi);
}

template<> EIGEN_STRONG_INLINE void pstore<float>(float*   to, const Packet4f& from) { EIGEN_DEBUG_ALIGNED_STORE vst1q_f32(to, from); }
template<> EIGEN_STRONG_INLINE void pstore<int>(int*       to, const Packet4i& from) { EIGEN_DEBUG_ALIGNED_STORE vst1q_s32(to, from); }

template<> EIGEN_STRONG_INLINE void pstoreu<float>(float*  to, const Packet4f& from) { EIGEN_DEBUG_UNALIGNED_STORE vst1q_f32(to, from); }
template<> EIGEN_STRONG_INLINE void pstoreu<int>(int*      to, const Packet4i& from) { EIGEN_DEBUG_UNALIGNED_STORE vst1q_s32(to, from); }

template<> EIGEN_STRONG_INLINE void prefetch<float>(const float* addr) { EIGEN_ARM_PREFETCH(addr); }
template<> EIGEN_STRONG_INLINE void prefetch<int>(const int*     addr) { EIGEN_ARM_PREFETCH(addr); }

// FIXME only store the 2 first elements ?
template<> EIGEN_STRONG_INLINE float  pfirst<Packet4f>(const Packet4f& a) { float EIGEN_ALIGN16 x[4]; vst1q_f32(x, a); return x[0]; }
template<> EIGEN_STRONG_INLINE int    pfirst<Packet4i>(const Packet4i& a) { int   EIGEN_ALIGN16 x[4]; vst1q_s32(x, a); return x[0]; }

template<> EIGEN_STRONG_INLINE Packet4f preverse(const Packet4f& a) {
  float32x2_t a_lo, a_hi;
  Packet4f a_r64;

  a_r64 = vrev64q_f32(a);
  a_lo = vget_low_f32(a_r64);
  a_hi = vget_high_f32(a_r64);
  return vcombine_f32(a_hi, a_lo);
}
template<> EIGEN_STRONG_INLINE Packet4i preverse(const Packet4i& a) {
  int32x2_t a_lo, a_hi;
  Packet4i a_r64;

  a_r64 = vrev64q_s32(a);
  a_lo = vget_low_s32(a_r64);
  a_hi = vget_high_s32(a_r64);
  return vcombine_s32(a_hi, a_lo);
}
template<> EIGEN_STRONG_INLINE Packet4f pabs(const Packet4f& a) { return vabsq_f32(a); }
template<> EIGEN_STRONG_INLINE Packet4i pabs(const Packet4i& a) { return vabsq_s32(a); }

template<> EIGEN_STRONG_INLINE float predux<Packet4f>(const Packet4f& a)
{
  float32x2_t a_lo, a_hi, sum;

  a_lo = vget_low_f32(a);
  a_hi = vget_high_f32(a);
  sum = vpadd_f32(a_lo, a_hi);
  sum = vpadd_f32(sum, sum);
  return vget_lane_f32(sum, 0);
}

template<> EIGEN_STRONG_INLINE Packet4f preduxp<Packet4f>(const Packet4f* vecs)
{
  float32x4x2_t vtrn1, vtrn2, res1, res2;
  Packet4f sum1, sum2, sum;

  // NEON zip performs interleaving of the supplied vectors.
  // We perform two interleaves in a row to acquire the transposed vector
  vtrn1 = vzipq_f32(vecs[0], vecs[2]);
  vtrn2 = vzipq_f32(vecs[1], vecs[3]);
  res1 = vzipq_f32(vtrn1.val[0], vtrn2.val[0]);
  res2 = vzipq_f32(vtrn1.val[1], vtrn2.val[1]);

  // Do the addition of the resulting vectors
  sum1 = vaddq_f32(res1.val[0], res1.val[1]);
  sum2 = vaddq_f32(res2.val[0], res2.val[1]);
  sum = vaddq_f32(sum1, sum2);

  return sum;
}

template<> EIGEN_STRONG_INLINE int predux<Packet4i>(const Packet4i& a)
{
  int32x2_t a_lo, a_hi, sum;

  a_lo = vget_low_s32(a);
  a_hi = vget_high_s32(a);
  sum = vpadd_s32(a_lo, a_hi);
  sum = vpadd_s32(sum, sum);
  return vget_lane_s32(sum, 0);
}

template<> EIGEN_STRONG_INLINE Packet4i preduxp<Packet4i>(const Packet4i* vecs)
{
  int32x4x2_t vtrn1, vtrn2, res1, res2;
  Packet4i sum1, sum2, sum;

  // NEON zip performs interleaving of the supplied vectors.
  // We perform two interleaves in a row to acquire the transposed vector
  vtrn1 = vzipq_s32(vecs[0], vecs[2]);
  vtrn2 = vzipq_s32(vecs[1], vecs[3]);
  res1 = vzipq_s32(vtrn1.val[0], vtrn2.val[0]);
  res2 = vzipq_s32(vtrn1.val[1], vtrn2.val[1]);

  // Do the addition of the resulting vectors
  sum1 = vaddq_s32(res1.val[0], res1.val[1]);
  sum2 = vaddq_s32(res2.val[0], res2.val[1]);
  sum = vaddq_s32(sum1, sum2);

  return sum;
}

// Other reduction functions:
// mul
template<> EIGEN_STRONG_INLINE float predux_mul<Packet4f>(const Packet4f& a)
{
  float32x2_t a_lo, a_hi, prod;

  // Get a_lo = |a1|a2| and a_hi = |a3|a4|
  a_lo = vget_low_f32(a);
  a_hi = vget_high_f32(a);
  // Get the product of a_lo * a_hi -> |a1*a3|a2*a4|
  prod = vmul_f32(a_lo, a_hi);
  // Multiply prod with its swapped value |a2*a4|a1*a3|
  prod = vmul_f32(prod, vrev64_f32(prod));

  return vget_lane_f32(prod, 0);
}
template<> EIGEN_STRONG_INLINE int predux_mul<Packet4i>(const Packet4i& a)
{
  int32x2_t a_lo, a_hi, prod;

  // Get a_lo = |a1|a2| and a_hi = |a3|a4|
  a_lo = vget_low_s32(a);
  a_hi = vget_high_s32(a);
  // Get the product of a_lo * a_hi -> |a1*a3|a2*a4|
  prod = vmul_s32(a_lo, a_hi);
  // Multiply prod with its swapped value |a2*a4|a1*a3|
  prod = vmul_s32(prod, vrev64_s32(prod));

  return vget_lane_s32(prod, 0);
}

// min
template<> EIGEN_STRONG_INLINE float predux_min<Packet4f>(const Packet4f& a)
{
  float32x2_t a_lo, a_hi, min;

  a_lo = vget_low_f32(a);
  a_hi = vget_high_f32(a);
  min = vpmin_f32(a_lo, a_hi);
  min = vpmin_f32(min, min);

  return vget_lane_f32(min, 0);
}

template<> EIGEN_STRONG_INLINE int predux_min<Packet4i>(const Packet4i& a)
{
  int32x2_t a_lo, a_hi, min;

  a_lo = vget_low_s32(a);
  a_hi = vget_high_s32(a);
  min = vpmin_s32(a_lo, a_hi);
  min = vpmin_s32(min, min);
  
  return vget_lane_s32(min, 0);
}

// max
template<> EIGEN_STRONG_INLINE float predux_max<Packet4f>(const Packet4f& a)
{
  float32x2_t a_lo, a_hi, max;

  a_lo = vget_low_f32(a);
  a_hi = vget_high_f32(a);
  max = vpmax_f32(a_lo, a_hi);
  max = vpmax_f32(max, max);

  return vget_lane_f32(max, 0);
}

template<> EIGEN_STRONG_INLINE int predux_max<Packet4i>(const Packet4i& a)
{
  int32x2_t a_lo, a_hi, max;

  a_lo = vget_low_s32(a);
  a_hi = vget_high_s32(a);
  max = vpmax_s32(a_lo, a_hi);
  max = vpmax_s32(max, max);

  return vget_lane_s32(max, 0);
}

// this PALIGN_NEON business is to work around a bug in LLVM Clang 3.0 causing incorrect compilation errors,
// see bug 347 and this LLVM bug: http://llvm.org/bugs/show_bug.cgi?id=11074
#define PALIGN_NEON(Offset,Type,Command) \
template<>\
struct palign_impl<Offset,Type>\
{\
    EIGEN_STRONG_INLINE static void run(Type& first, const Type& second)\
    {\
        if (Offset!=0)\
            first = Command(first, second, Offset);\
    }\
};\

PALIGN_NEON(0,Packet4f,vextq_f32)
PALIGN_NEON(1,Packet4f,vextq_f32)
PALIGN_NEON(2,Packet4f,vextq_f32)
PALIGN_NEON(3,Packet4f,vextq_f32)
PALIGN_NEON(0,Packet4i,vextq_s32)
PALIGN_NEON(1,Packet4i,vextq_s32)
PALIGN_NEON(2,Packet4i,vextq_s32)
PALIGN_NEON(3,Packet4i,vextq_s32)
    
#undef PALIGN_NEON

} // end namespace internal

} // end namespace Eigen

#endif // EIGEN_PACKET_MATH_NEON_H
