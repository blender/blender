// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Konstantinos Margaritis <markos@codex.gr>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_PACKET_MATH_ALTIVEC_H
#define EIGEN_PACKET_MATH_ALTIVEC_H

#ifndef EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD
#define EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD 4
#endif

typedef __vector float          v4f;
typedef __vector int            v4i;
typedef __vector unsigned int   v4ui;
typedef __vector __bool int     v4bi;

// We don't want to write the same code all the time, but we need to reuse the constants
// and it doesn't really work to declare them global, so we define macros instead

#define USE_CONST_v0i     const v4i   v0i   = vec_splat_s32(0)
#define USE_CONST_v1i     const v4i   v1i   = vec_splat_s32(1)
#define USE_CONST_v16i_   const v4i   v16i_ = vec_splat_s32(-16)
#define USE_CONST_v0f     USE_CONST_v0i; const v4f v0f = (v4f) v0i
#define USE_CONST_v1f     USE_CONST_v1i; const v4f v1f = vec_ctf(v1i, 0)
#define USE_CONST_v1i_    const v4ui  v1i_  = vec_splat_u32(-1)
#define USE_CONST_v0f_    USE_CONST_v1i_; const v4f v0f_ = (v4f) vec_sl(v1i_, v1i_)

template<> struct ei_packet_traits<float>  { typedef v4f type; enum {size=4}; };
template<> struct ei_packet_traits<int>    { typedef v4i type; enum {size=4}; };

template<> struct ei_unpacket_traits<v4f>  { typedef float  type; enum {size=4}; };
template<> struct ei_unpacket_traits<v4i>  { typedef int    type; enum {size=4}; };

inline std::ostream & operator <<(std::ostream & s, const v4f & v)
{
  union {
    v4f   v;
    float n[4];
  } vt;
  vt.v = v;
  s << vt.n[0] << ", " << vt.n[1] << ", " << vt.n[2] << ", " << vt.n[3];
  return s;
}

inline std::ostream & operator <<(std::ostream & s, const v4i & v)
{
  union {
    v4i   v;
    int n[4];
  } vt;
  vt.v = v;
  s << vt.n[0] << ", " << vt.n[1] << ", " << vt.n[2] << ", " << vt.n[3];
  return s;
}

inline std::ostream & operator <<(std::ostream & s, const v4ui & v)
{
  union {
    v4ui   v;
    unsigned int n[4];
  } vt;
  vt.v = v;
  s << vt.n[0] << ", " << vt.n[1] << ", " << vt.n[2] << ", " << vt.n[3];
  return s;
}

inline std::ostream & operator <<(std::ostream & s, const v4bi & v)
{
  union {
    __vector __bool int v;
    unsigned int n[4];
  } vt;
  vt.v = v;
  s << vt.n[0] << ", " << vt.n[1] << ", " << vt.n[2] << ", " << vt.n[3];
  return s;
}

template<> inline v4f  ei_padd(const v4f&   a, const v4f&   b) { return vec_add(a,b); }
template<> inline v4i  ei_padd(const v4i&   a, const v4i&   b) { return vec_add(a,b); }

template<> inline v4f  ei_psub(const v4f&   a, const v4f&   b) { return vec_sub(a,b); }
template<> inline v4i  ei_psub(const v4i&   a, const v4i&   b) { return vec_sub(a,b); }

template<> inline v4f  ei_pmul(const v4f&   a, const v4f&   b) { USE_CONST_v0f; return vec_madd(a,b, v0f); }
template<> inline v4i  ei_pmul(const v4i&   a, const v4i&   b)
{
  // Detailed in: http://freevec.org/content/32bit_signed_integer_multiplication_altivec
  //Set up constants, variables
  v4i a1, b1, bswap, low_prod, high_prod, prod, prod_, v1sel;
  USE_CONST_v0i;
  USE_CONST_v1i;
  USE_CONST_v16i_;

  // Get the absolute values 
  a1  = vec_abs(a);
  b1  = vec_abs(b);

  // Get the signs using xor
  v4bi sgn = (v4bi) vec_cmplt(vec_xor(a, b), v0i);

  // Do the multiplication for the asbolute values.
  bswap = (v4i) vec_rl((v4ui) b1, (v4ui) v16i_ );
  low_prod = vec_mulo((__vector short)a1, (__vector short)b1);
  high_prod = vec_msum((__vector short)a1, (__vector short)bswap, v0i);
  high_prod = (v4i) vec_sl((v4ui) high_prod, (v4ui) v16i_);
  prod = vec_add( low_prod, high_prod );

  // NOR the product and select only the negative elements according to the sign mask
  prod_ = vec_nor(prod, prod);
  prod_ = vec_sel(v0i, prod_, sgn);

  // Add 1 to the result to get the negative numbers
  v1sel = vec_sel(v0i, v1i, sgn);
  prod_ = vec_add(prod_, v1sel);

  // Merge the results back to the final vector.
  prod = vec_sel(prod, prod_, sgn);

  return prod;
}

template<> inline v4f  ei_pdiv(const v4f&   a, const v4f&   b) {
  v4f t, y_0, y_1, res;
  USE_CONST_v0f;
  USE_CONST_v1f;

  // Altivec does not offer a divide instruction, we have to do a reciprocal approximation
  y_0 = vec_re(b);
  
  // Do one Newton-Raphson iteration to get the needed accuracy
  t = vec_nmsub(y_0, b, v1f);
  y_1 = vec_madd(y_0, t, y_0);

  res = vec_madd(a, y_1, v0f);
  return res;
}

template<> inline v4f  ei_pmadd(const v4f&  a, const v4f&   b, const v4f&  c) { return vec_madd(a, b, c); }

template<> inline v4f  ei_pmin(const v4f&   a, const v4f&   b) { return vec_min(a,b); }
template<> inline v4i  ei_pmin(const v4i&   a, const v4i&   b) { return vec_min(a,b); }

template<> inline v4f  ei_pmax(const v4f&   a, const v4f&   b) { return vec_max(a,b); }
template<> inline v4i  ei_pmax(const v4i&   a, const v4i&   b) { return vec_max(a,b); }

template<> inline v4f  ei_pload(const float* from) { return vec_ld(0, from); }
template<> inline v4i  ei_pload(const int*   from) { return vec_ld(0, from); }

template<> inline v4f  ei_ploadu(const float*  from)
{
  // Taken from http://developer.apple.com/hardwaredrivers/ve/alignment.html
  __vector unsigned char MSQ, LSQ;
  __vector unsigned char mask;
  MSQ = vec_ld(0, (unsigned char *)from);          // most significant quadword
  LSQ = vec_ld(15, (unsigned char *)from);         // least significant quadword
  mask = vec_lvsl(0, from);                        // create the permute mask
  return (v4f) vec_perm(MSQ, LSQ, mask);           // align the data
}

template<> inline v4i    ei_ploadu(const int*    from)
{
  // Taken from http://developer.apple.com/hardwaredrivers/ve/alignment.html
  __vector unsigned char MSQ, LSQ;
  __vector unsigned char mask;
  MSQ = vec_ld(0, (unsigned char *)from);          // most significant quadword
  LSQ = vec_ld(15, (unsigned char *)from);         // least significant quadword
  mask = vec_lvsl(0, from);                        // create the permute mask
  return (v4i) vec_perm(MSQ, LSQ, mask);    // align the data
}

template<> inline v4f  ei_pset1(const float&  from)
{
  // Taken from http://developer.apple.com/hardwaredrivers/ve/alignment.html
  float __attribute__(aligned(16)) af[4];
  af[0] = from;
  v4f vc = vec_ld(0, af);
  vc = vec_splat(vc, 0);
  return vc;
}

template<> inline v4i    ei_pset1(const int&    from)
{
  int __attribute__(aligned(16)) ai[4];
  ai[0] = from;
  v4i vc = vec_ld(0, ai);
  vc = vec_splat(vc, 0);
  return vc;
}

template<> inline void ei_pstore(float*   to, const v4f&   from) { vec_st(from, 0, to); }
template<> inline void ei_pstore(int*     to, const v4i&   from) { vec_st(from, 0, to); }

template<> inline void ei_pstoreu(float*  to, const v4f&   from)
{
  // Taken from http://developer.apple.com/hardwaredrivers/ve/alignment.html
  // Warning: not thread safe!
  __vector unsigned char MSQ, LSQ, edges;
  __vector unsigned char edgeAlign, align;

  MSQ = vec_ld(0, (unsigned char *)to);                     // most significant quadword
  LSQ = vec_ld(15, (unsigned char *)to);                    // least significant quadword
  edgeAlign = vec_lvsl(0, to);                              // permute map to extract edges
  edges=vec_perm(LSQ,MSQ,edgeAlign);                        // extract the edges
  align = vec_lvsr( 0, to );                                // permute map to misalign data
  MSQ = vec_perm(edges,(__vector unsigned char)from,align);   // misalign the data (MSQ)
  LSQ = vec_perm((__vector unsigned char)from,edges,align);   // misalign the data (LSQ)
  vec_st( LSQ, 15, (unsigned char *)to );                   // Store the LSQ part first
  vec_st( MSQ, 0, (unsigned char *)to );                    // Store the MSQ part
}

template<> inline void ei_pstoreu(int*    to , const v4i&    from )
{
  // Taken from http://developer.apple.com/hardwaredrivers/ve/alignment.html
  // Warning: not thread safe!
  __vector unsigned char MSQ, LSQ, edges;
  __vector unsigned char edgeAlign, align;

  MSQ = vec_ld(0, (unsigned char *)to);                     // most significant quadword
  LSQ = vec_ld(15, (unsigned char *)to);                    // least significant quadword
  edgeAlign = vec_lvsl(0, to);                              // permute map to extract edges
  edges=vec_perm(LSQ,MSQ,edgeAlign);                        // extract the edges
  align = vec_lvsr( 0, to );                                // permute map to misalign data
  MSQ = vec_perm(edges,(__vector unsigned char)from,align);   // misalign the data (MSQ)
  LSQ = vec_perm((__vector unsigned char)from,edges,align);   // misalign the data (LSQ)
  vec_st( LSQ, 15, (unsigned char *)to );                   // Store the LSQ part first
  vec_st( MSQ, 0, (unsigned char *)to );                    // Store the MSQ part
}

template<> inline float  ei_pfirst(const v4f&  a)
{
  float __attribute__(aligned(16)) af[4];
  vec_st(a, 0, af);
  return af[0];
}

template<> inline int    ei_pfirst(const v4i&  a)
{
  int __attribute__(aligned(16)) ai[4];
  vec_st(a, 0, ai);
  return ai[0];
}

inline v4f ei_preduxp(const v4f* vecs)
{
  v4f v[4], sum[4];

  // It's easier and faster to transpose then add as columns
  // Check: http://www.freevec.org/function/matrix_4x4_transpose_floats for explanation
  // Do the transpose, first set of moves
  v[0] = vec_mergeh(vecs[0], vecs[2]);
  v[1] = vec_mergel(vecs[0], vecs[2]);
  v[2] = vec_mergeh(vecs[1], vecs[3]);
  v[3] = vec_mergel(vecs[1], vecs[3]);
  // Get the resulting vectors
  sum[0] = vec_mergeh(v[0], v[2]);
  sum[1] = vec_mergel(v[0], v[2]);
  sum[2] = vec_mergeh(v[1], v[3]);
  sum[3] = vec_mergel(v[1], v[3]);

  // Now do the summation:
  // Lines 0+1
  sum[0] = vec_add(sum[0], sum[1]);
  // Lines 2+3
  sum[1] = vec_add(sum[2], sum[3]);
  // Add the results
  sum[0] = vec_add(sum[0], sum[1]);
  return sum[0];
}

inline float ei_predux(const v4f& a)
{
  v4f b, sum;
  b = (v4f)vec_sld(a, a, 8);
  sum = vec_add(a, b);
  b = (v4f)vec_sld(sum, sum, 4);
  sum = vec_add(sum, b);
  return ei_pfirst(sum);
}

inline v4i  ei_preduxp(const v4i* vecs)
{
  v4i v[4], sum[4];

  // It's easier and faster to transpose then add as columns
  // Check: http://www.freevec.org/function/matrix_4x4_transpose_floats for explanation
  // Do the transpose, first set of moves
  v[0] = vec_mergeh(vecs[0], vecs[2]);
  v[1] = vec_mergel(vecs[0], vecs[2]);
  v[2] = vec_mergeh(vecs[1], vecs[3]);
  v[3] = vec_mergel(vecs[1], vecs[3]);
  // Get the resulting vectors
  sum[0] = vec_mergeh(v[0], v[2]);
  sum[1] = vec_mergel(v[0], v[2]);
  sum[2] = vec_mergeh(v[1], v[3]);
  sum[3] = vec_mergel(v[1], v[3]);

  // Now do the summation:
  // Lines 0+1
  sum[0] = vec_add(sum[0], sum[1]);
  // Lines 2+3
  sum[1] = vec_add(sum[2], sum[3]);
  // Add the results
  sum[0] = vec_add(sum[0], sum[1]);
  return sum[0];
}

inline int ei_predux(const v4i& a)
{
  USE_CONST_v0i;
  v4i sum;
  sum = vec_sums(a, v0i);
  sum = vec_sld(sum, v0i, 12);
  return ei_pfirst(sum);
}

template<int Offset>
struct ei_palign_impl<Offset, v4f>
{
  inline static void run(v4f& first, const v4f& second)
  {
    first = vec_sld(first, second, Offset*4);
  }
};

template<int Offset>
struct ei_palign_impl<Offset, v4i>
{
  inline static void run(v4i& first, const v4i& second)
  {
    first = vec_sld(first, second, Offset*4);
  }
};

#endif // EIGEN_PACKET_MATH_ALTIVEC_H
