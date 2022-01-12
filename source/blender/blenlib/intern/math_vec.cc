/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bli
 */

#include "BLI_double2.hh"
#include "BLI_double3.hh"
#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_hash.hh"
#include "BLI_math_mpq.hh"
#include "BLI_mpq2.hh"
#include "BLI_mpq3.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

namespace blender {

float2::isect_result float2::isect_seg_seg(const float2 &v1,
                                           const float2 &v2,
                                           const float2 &v3,
                                           const float2 &v4)
{
  float2::isect_result ans;
  float div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
  if (div == 0.0f) {
    ans.lambda = 0.0f;
    ans.mu = 0.0f;
    ans.kind = float2::isect_result::LINE_LINE_COLINEAR;
  }
  else {
    ans.lambda = ((v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;
    ans.mu = ((v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;
    if (ans.lambda >= 0.0f && ans.lambda <= 1.0f && ans.mu >= 0.0f && ans.mu <= 1.0f) {
      if (ans.lambda == 0.0f || ans.lambda == 1.0f || ans.mu == 0.0f || ans.mu == 1.0f) {
        ans.kind = float2::isect_result::LINE_LINE_EXACT;
      }
      else {
        ans.kind = float2::isect_result::LINE_LINE_CROSS;
      }
    }
    else {
      ans.kind = float2::isect_result::LINE_LINE_NONE;
    }
  }
  return ans;
}

double2::isect_result double2::isect_seg_seg(const double2 &v1,
                                             const double2 &v2,
                                             const double2 &v3,
                                             const double2 &v4)
{
  double2::isect_result ans;
  double div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
  if (div == 0.0) {
    ans.lambda = 0.0;
    ans.kind = double2::isect_result::LINE_LINE_COLINEAR;
  }
  else {
    ans.lambda = ((v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;
    double mu = ((v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;
    if (ans.lambda >= 0.0 && ans.lambda <= 1.0 && mu >= 0.0 && mu <= 1.0) {
      if (ans.lambda == 0.0 || ans.lambda == 1.0 || mu == 0.0 || mu == 1.0) {
        ans.kind = double2::isect_result::LINE_LINE_EXACT;
      }
      else {
        ans.kind = double2::isect_result::LINE_LINE_CROSS;
      }
    }
    else {
      ans.kind = double2::isect_result::LINE_LINE_NONE;
    }
  }
  return ans;
}

#ifdef WITH_GMP
mpq2::isect_result mpq2::isect_seg_seg(const mpq2 &v1,
                                       const mpq2 &v2,
                                       const mpq2 &v3,
                                       const mpq2 &v4)
{
  mpq2::isect_result ans;
  mpq_class div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
  if (div == 0.0) {
    ans.lambda = 0.0;
    ans.kind = mpq2::isect_result::LINE_LINE_COLINEAR;
  }
  else {
    ans.lambda = ((v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;
    /* Avoid dividing mu by div: it is expensive in multi-precision. */
    mpq_class mudiv = ((v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1]));
    if (ans.lambda >= 0 && ans.lambda <= 1 &&
        ((div > 0 && mudiv >= 0 && mudiv <= div) || (div < 0 && mudiv <= 0 && mudiv >= div))) {
      if (ans.lambda == 0 || ans.lambda == 1 || mudiv == 0 || mudiv == div) {
        ans.kind = mpq2::isect_result::LINE_LINE_EXACT;
      }
      else {
        ans.kind = mpq2::isect_result::LINE_LINE_CROSS;
      }
    }
    else {
      ans.kind = mpq2::isect_result::LINE_LINE_NONE;
    }
  }
  return ans;
}
#endif

double3 double3::cross_poly(Span<double3> poly)
{
  /* Newell's Method. */
  int nv = static_cast<int>(poly.size());
  if (nv < 3) {
    return double3(0, 0, 0);
  }
  const double3 *v_prev = &poly[nv - 1];
  const double3 *v_curr = &poly[0];
  double3 n(0, 0, 0);
  for (int i = 0; i < nv;) {
    n[0] = n[0] + ((*v_prev)[1] - (*v_curr)[1]) * ((*v_prev)[2] + (*v_curr)[2]);
    n[1] = n[1] + ((*v_prev)[2] - (*v_curr)[2]) * ((*v_prev)[0] + (*v_curr)[0]);
    n[2] = n[2] + ((*v_prev)[0] - (*v_curr)[0]) * ((*v_prev)[1] + (*v_curr)[1]);
    v_prev = v_curr;
    ++i;
    if (i < nv) {
      v_curr = &poly[i];
    }
  }
  return n;
}

#ifdef WITH_GMP
mpq3 mpq3::cross_poly(Span<mpq3> poly)
{
  /* Newell's Method. */
  int nv = static_cast<int>(poly.size());
  if (nv < 3) {
    return mpq3(0);
  }
  const mpq3 *v_prev = &poly[nv - 1];
  const mpq3 *v_curr = &poly[0];
  mpq3 n(0);
  for (int i = 0; i < nv;) {
    n[0] = n[0] + ((*v_prev)[1] - (*v_curr)[1]) * ((*v_prev)[2] + (*v_curr)[2]);
    n[1] = n[1] + ((*v_prev)[2] - (*v_curr)[2]) * ((*v_prev)[0] + (*v_curr)[0]);
    n[2] = n[2] + ((*v_prev)[0] - (*v_curr)[0]) * ((*v_prev)[1] + (*v_curr)[1]);
    v_prev = v_curr;
    ++i;
    if (i < nv) {
      v_curr = &poly[i];
    }
  }
  return n;
}

uint64_t hash_mpq_class(const mpq_class &value)
{
  /* TODO: better/faster implementation of this. */
  return get_default_hash(static_cast<float>(value.get_d()));
}

uint64_t mpq2::hash() const
{
  uint64_t hashx = hash_mpq_class(this->x);
  uint64_t hashy = hash_mpq_class(this->y);
  return hashx ^ (hashy * 33);
}

uint64_t mpq3::hash() const
{
  uint64_t hashx = hash_mpq_class(this->x);
  uint64_t hashy = hash_mpq_class(this->y);
  uint64_t hashz = hash_mpq_class(this->z);
  return hashx ^ (hashy * 33) ^ (hashz * 33 * 37);
}
#endif

}  // namespace blender
