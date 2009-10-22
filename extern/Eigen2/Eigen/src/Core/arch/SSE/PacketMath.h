// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
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

#ifndef EIGEN_PACKET_MATH_SSE_H
#define EIGEN_PACKET_MATH_SSE_H

#ifndef EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD
#define EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD 16
#endif

template<> struct ei_packet_traits<float>  { typedef __m128  type; enum {size=4}; };
template<> struct ei_packet_traits<double> { typedef __m128d type; enum {size=2}; };
template<> struct ei_packet_traits<int>    { typedef __m128i type; enum {size=4}; };

template<> struct ei_unpacket_traits<__m128>  { typedef float  type; enum {size=4}; };
template<> struct ei_unpacket_traits<__m128d> { typedef double type; enum {size=2}; };
template<> struct ei_unpacket_traits<__m128i> { typedef int    type; enum {size=4}; };

template<> EIGEN_STRONG_INLINE __m128  ei_pset1<float>(const float&  from) { return _mm_set1_ps(from); }
template<> EIGEN_STRONG_INLINE __m128d ei_pset1<double>(const double& from) { return _mm_set1_pd(from); }
template<> EIGEN_STRONG_INLINE __m128i ei_pset1<int>(const int&    from) { return _mm_set1_epi32(from); }

template<> EIGEN_STRONG_INLINE __m128  ei_padd<__m128>(const __m128&  a, const __m128&  b) { return _mm_add_ps(a,b); }
template<> EIGEN_STRONG_INLINE __m128d ei_padd<__m128d>(const __m128d& a, const __m128d& b) { return _mm_add_pd(a,b); }
template<> EIGEN_STRONG_INLINE __m128i ei_padd<__m128i>(const __m128i& a, const __m128i& b) { return _mm_add_epi32(a,b); }

template<> EIGEN_STRONG_INLINE __m128  ei_psub<__m128>(const __m128&  a, const __m128&  b) { return _mm_sub_ps(a,b); }
template<> EIGEN_STRONG_INLINE __m128d ei_psub<__m128d>(const __m128d& a, const __m128d& b) { return _mm_sub_pd(a,b); }
template<> EIGEN_STRONG_INLINE __m128i ei_psub<__m128i>(const __m128i& a, const __m128i& b) { return _mm_sub_epi32(a,b); }

template<> EIGEN_STRONG_INLINE __m128  ei_pmul<__m128>(const __m128&  a, const __m128&  b) { return _mm_mul_ps(a,b); }
template<> EIGEN_STRONG_INLINE __m128d ei_pmul<__m128d>(const __m128d& a, const __m128d& b) { return _mm_mul_pd(a,b); }
template<> EIGEN_STRONG_INLINE __m128i ei_pmul<__m128i>(const __m128i& a, const __m128i& b)
{
  return _mm_or_si128(
    _mm_and_si128(
      _mm_mul_epu32(a,b),
      _mm_setr_epi32(0xffffffff,0,0xffffffff,0)),
    _mm_slli_si128(
      _mm_and_si128(
        _mm_mul_epu32(_mm_srli_si128(a,4),_mm_srli_si128(b,4)),
        _mm_setr_epi32(0xffffffff,0,0xffffffff,0)), 4));
}

template<> EIGEN_STRONG_INLINE __m128  ei_pdiv<__m128>(const __m128&  a, const __m128&  b) { return _mm_div_ps(a,b); }
template<> EIGEN_STRONG_INLINE __m128d ei_pdiv<__m128d>(const __m128d& a, const __m128d& b) { return _mm_div_pd(a,b); }
template<> EIGEN_STRONG_INLINE __m128i ei_pdiv<__m128i>(const __m128i& /*a*/, const __m128i& /*b*/)
{ ei_assert(false && "packet integer division are not supported by SSE");
  __m128i dummy = ei_pset1<int>(0);
  return dummy;
}

// for some weird raisons, it has to be overloaded for packet integer
template<> EIGEN_STRONG_INLINE __m128i ei_pmadd(const __m128i& a, const __m128i& b, const __m128i& c) { return ei_padd(ei_pmul(a,b), c); }

template<> EIGEN_STRONG_INLINE __m128  ei_pmin<__m128>(const __m128&  a, const __m128&  b) { return _mm_min_ps(a,b); }
template<> EIGEN_STRONG_INLINE __m128d ei_pmin<__m128d>(const __m128d& a, const __m128d& b) { return _mm_min_pd(a,b); }
// FIXME this vectorized min operator is likely to be slower than the standard one
template<> EIGEN_STRONG_INLINE __m128i ei_pmin<__m128i>(const __m128i& a, const __m128i& b)
{
  __m128i mask = _mm_cmplt_epi32(a,b);
  return _mm_or_si128(_mm_and_si128(mask,a),_mm_andnot_si128(mask,b));
}

template<> EIGEN_STRONG_INLINE __m128  ei_pmax<__m128>(const __m128&  a, const __m128&  b) { return _mm_max_ps(a,b); }
template<> EIGEN_STRONG_INLINE __m128d ei_pmax<__m128d>(const __m128d& a, const __m128d& b) { return _mm_max_pd(a,b); }
// FIXME this vectorized max operator is likely to be slower than the standard one
template<> EIGEN_STRONG_INLINE __m128i ei_pmax<__m128i>(const __m128i& a, const __m128i& b)
{
  __m128i mask = _mm_cmpgt_epi32(a,b);
  return _mm_or_si128(_mm_and_si128(mask,a),_mm_andnot_si128(mask,b));
}

template<> EIGEN_STRONG_INLINE __m128  ei_pload<float>(const float*   from) { return _mm_load_ps(from); }
template<> EIGEN_STRONG_INLINE __m128d ei_pload<double>(const double*  from) { return _mm_load_pd(from); }
template<> EIGEN_STRONG_INLINE __m128i ei_pload<int>(const int* from) { return _mm_load_si128(reinterpret_cast<const __m128i*>(from)); }

template<> EIGEN_STRONG_INLINE __m128  ei_ploadu<float>(const float*   from) { return _mm_loadu_ps(from); }
// template<> EIGEN_STRONG_INLINE __m128  ei_ploadu(const float*   from) {
//   if (size_t(from)&0xF)
//     return _mm_loadu_ps(from);
//   else 
//     return _mm_loadu_ps(from);
// }
template<> EIGEN_STRONG_INLINE __m128d ei_ploadu<double>(const double*  from) { return _mm_loadu_pd(from); }
template<> EIGEN_STRONG_INLINE __m128i ei_ploadu<int>(const int* from) { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(from)); }

template<> EIGEN_STRONG_INLINE void ei_pstore<float>(float*  to, const __m128&  from) { _mm_store_ps(to, from); }
template<> EIGEN_STRONG_INLINE void ei_pstore<double>(double* to, const __m128d& from) { _mm_store_pd(to, from); }
template<> EIGEN_STRONG_INLINE void ei_pstore<int>(int*    to, const __m128i& from) { _mm_store_si128(reinterpret_cast<__m128i*>(to), from); }

template<> EIGEN_STRONG_INLINE void ei_pstoreu<float>(float*  to, const __m128&  from) { _mm_storeu_ps(to, from); }
template<> EIGEN_STRONG_INLINE void ei_pstoreu<double>(double* to, const __m128d& from) { _mm_storeu_pd(to, from); }
template<> EIGEN_STRONG_INLINE void ei_pstoreu<int>(int*    to, const __m128i& from) { _mm_storeu_si128(reinterpret_cast<__m128i*>(to), from); }

#ifdef _MSC_VER
// this fix internal compilation error
template<> EIGEN_STRONG_INLINE float  ei_pfirst<__m128>(const __m128&  a) { float x = _mm_cvtss_f32(a); return x; }
template<> EIGEN_STRONG_INLINE double ei_pfirst<__m128d>(const __m128d& a) { double x = _mm_cvtsd_f64(a); return x; }
template<> EIGEN_STRONG_INLINE int    ei_pfirst<__m128i>(const __m128i& a) { int x = _mm_cvtsi128_si32(a); return x; }
#else
template<> EIGEN_STRONG_INLINE float  ei_pfirst<__m128>(const __m128&  a) { return _mm_cvtss_f32(a); }
template<> EIGEN_STRONG_INLINE double ei_pfirst<__m128d>(const __m128d& a) { return _mm_cvtsd_f64(a); }
template<> EIGEN_STRONG_INLINE int    ei_pfirst<__m128i>(const __m128i& a) { return _mm_cvtsi128_si32(a); }
#endif

#ifdef __SSE3__
// TODO implement SSE2 versions as well as integer versions
template<> EIGEN_STRONG_INLINE __m128 ei_preduxp<__m128>(const __m128* vecs)
{
  return _mm_hadd_ps(_mm_hadd_ps(vecs[0], vecs[1]),_mm_hadd_ps(vecs[2], vecs[3]));
}
template<> EIGEN_STRONG_INLINE __m128d ei_preduxp<__m128d>(const __m128d* vecs)
{
  return _mm_hadd_pd(vecs[0], vecs[1]);
}
// SSSE3 version:
// EIGEN_STRONG_INLINE __m128i ei_preduxp(const __m128i* vecs)
// {
//   return _mm_hadd_epi32(_mm_hadd_epi32(vecs[0], vecs[1]),_mm_hadd_epi32(vecs[2], vecs[3]));
// }

template<> EIGEN_STRONG_INLINE float ei_predux<__m128>(const __m128& a)
{
  __m128 tmp0 = _mm_hadd_ps(a,a);
  return ei_pfirst(_mm_hadd_ps(tmp0, tmp0));
}

template<> EIGEN_STRONG_INLINE double ei_predux<__m128d>(const __m128d& a) { return ei_pfirst(_mm_hadd_pd(a, a)); }

// SSSE3 version:
// EIGEN_STRONG_INLINE float ei_predux(const __m128i& a)
// {
//   __m128i tmp0 = _mm_hadd_epi32(a,a);
//   return ei_pfirst(_mm_hadd_epi32(tmp0, tmp0));
// }
#else
// SSE2 versions
template<> EIGEN_STRONG_INLINE float ei_predux<__m128>(const __m128& a)
{
  __m128 tmp = _mm_add_ps(a, _mm_movehl_ps(a,a));
  return ei_pfirst(_mm_add_ss(tmp, _mm_shuffle_ps(tmp,tmp, 1)));
}
template<> EIGEN_STRONG_INLINE double ei_predux<__m128d>(const __m128d& a)
{
  return ei_pfirst(_mm_add_sd(a, _mm_unpackhi_pd(a,a)));
}

template<> EIGEN_STRONG_INLINE __m128 ei_preduxp<__m128>(const __m128* vecs)
{
  __m128 tmp0, tmp1, tmp2;
  tmp0 = _mm_unpacklo_ps(vecs[0], vecs[1]);
  tmp1 = _mm_unpackhi_ps(vecs[0], vecs[1]);
  tmp2 = _mm_unpackhi_ps(vecs[2], vecs[3]);
  tmp0 = _mm_add_ps(tmp0, tmp1);
  tmp1 = _mm_unpacklo_ps(vecs[2], vecs[3]);
  tmp1 = _mm_add_ps(tmp1, tmp2);
  tmp2 = _mm_movehl_ps(tmp1, tmp0);
  tmp0 = _mm_movelh_ps(tmp0, tmp1);
  return _mm_add_ps(tmp0, tmp2);
}

template<> EIGEN_STRONG_INLINE __m128d ei_preduxp<__m128d>(const __m128d* vecs)
{
  return _mm_add_pd(_mm_unpacklo_pd(vecs[0], vecs[1]), _mm_unpackhi_pd(vecs[0], vecs[1]));
}
#endif  // SSE3

template<> EIGEN_STRONG_INLINE int ei_predux<__m128i>(const __m128i& a)
{
  __m128i tmp = _mm_add_epi32(a, _mm_unpackhi_epi64(a,a));
  return ei_pfirst(tmp) + ei_pfirst(_mm_shuffle_epi32(tmp, 1));
}

template<> EIGEN_STRONG_INLINE __m128i ei_preduxp<__m128i>(const __m128i* vecs)
{
  __m128i tmp0, tmp1, tmp2;
  tmp0 = _mm_unpacklo_epi32(vecs[0], vecs[1]);
  tmp1 = _mm_unpackhi_epi32(vecs[0], vecs[1]);
  tmp2 = _mm_unpackhi_epi32(vecs[2], vecs[3]);
  tmp0 = _mm_add_epi32(tmp0, tmp1);
  tmp1 = _mm_unpacklo_epi32(vecs[2], vecs[3]);
  tmp1 = _mm_add_epi32(tmp1, tmp2);
  tmp2 = _mm_unpacklo_epi64(tmp0, tmp1);
  tmp0 = _mm_unpackhi_epi64(tmp0, tmp1);
  return _mm_add_epi32(tmp0, tmp2);
}

#if (defined __GNUC__)
// template <> EIGEN_STRONG_INLINE __m128 ei_pmadd(const __m128&  a, const __m128&  b, const __m128&  c)
// {
//   __m128 res = b;
//   asm("mulps %[a], %[b] \n\taddps %[c], %[b]" : [b] "+x" (res) : [a] "x" (a), [c] "x" (c));
//   return res;
// }
// EIGEN_STRONG_INLINE __m128i _mm_alignr_epi8(const __m128i&  a, const __m128i&  b, const int i)
// {
//   __m128i res = a;
//   asm("palignr %[i], %[a], %[b] " : [b] "+x" (res) : [a] "x" (a), [i] "i" (i));
//   return res;
// }
#endif

#ifdef __SSSE3__
// SSSE3 versions
template<int Offset>
struct ei_palign_impl<Offset,__m128>
{
  EIGEN_STRONG_INLINE static void run(__m128& first, const __m128& second)
  {
    if (Offset!=0)
      first = _mm_castsi128_ps(_mm_alignr_epi8(_mm_castps_si128(second), _mm_castps_si128(first), Offset*4));
  }
};

template<int Offset>
struct ei_palign_impl<Offset,__m128i>
{
  EIGEN_STRONG_INLINE static void run(__m128i& first, const __m128i& second)
  {
    if (Offset!=0)
      first = _mm_alignr_epi8(second,first, Offset*4);
  }
};

template<int Offset>
struct ei_palign_impl<Offset,__m128d>
{
  EIGEN_STRONG_INLINE static void run(__m128d& first, const __m128d& second)
  {
    if (Offset==1)
      first = _mm_castsi128_pd(_mm_alignr_epi8(_mm_castpd_si128(second), _mm_castpd_si128(first), 8));
  }
};
#else
// SSE2 versions
template<int Offset>
struct ei_palign_impl<Offset,__m128>
{
  EIGEN_STRONG_INLINE static void run(__m128& first, const __m128& second)
  {
    if (Offset==1)
    {
      first = _mm_move_ss(first,second);
      first = _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(first),0x39));
    }
    else if (Offset==2)
    {
      first = _mm_movehl_ps(first,first);
      first = _mm_movelh_ps(first,second);
    }
    else if (Offset==3)
    {
      first = _mm_move_ss(first,second);
      first = _mm_shuffle_ps(first,second,0x93);
    }
  }
};

template<int Offset>
struct ei_palign_impl<Offset,__m128i>
{
  EIGEN_STRONG_INLINE static void run(__m128i& first, const __m128i& second)
  {
    if (Offset==1)
    {
      first = _mm_castps_si128(_mm_move_ss(_mm_castsi128_ps(first),_mm_castsi128_ps(second)));
      first = _mm_shuffle_epi32(first,0x39);
    }
    else if (Offset==2)
    {
      first = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(first),_mm_castsi128_ps(first)));
      first = _mm_castps_si128(_mm_movelh_ps(_mm_castsi128_ps(first),_mm_castsi128_ps(second)));
    }
    else if (Offset==3)
    {
      first = _mm_castps_si128(_mm_move_ss(_mm_castsi128_ps(first),_mm_castsi128_ps(second)));
      first = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(first),_mm_castsi128_ps(second),0x93));
    }
  }
};

template<int Offset>
struct ei_palign_impl<Offset,__m128d>
{
  EIGEN_STRONG_INLINE static void run(__m128d& first, const __m128d& second)
  {
    if (Offset==1)
    {
      first = _mm_castps_pd(_mm_movehl_ps(_mm_castpd_ps(first),_mm_castpd_ps(first)));
      first = _mm_castps_pd(_mm_movelh_ps(_mm_castpd_ps(first),_mm_castpd_ps(second)));
    }
  }
};
#endif

#define ei_vec4f_swizzle1(v,p,q,r,s) \
  (_mm_castsi128_ps(_mm_shuffle_epi32( _mm_castps_si128(v), ((s)<<6|(r)<<4|(q)<<2|(p)))))

#endif // EIGEN_PACKET_MATH_SSE_H
