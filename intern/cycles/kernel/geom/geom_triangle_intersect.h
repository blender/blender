/*
 * Copyright 2014, Blender Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Triangle/Ray intersections.
 *
 * For BVH ray intersection we use a precomputed triangle storage to accelerate
 * intersection at the cost of more memory usage.
 */

CCL_NAMESPACE_BEGIN

ccl_device_inline bool triangle_intersect(KernelGlobals *kg,
                                          Intersection *isect,
                                          float3 P,
                                          float3 dir,
                                          uint visibility,
                                          int object,
                                          int prim_addr)
{
  const uint tri_vindex = kernel_tex_fetch(__prim_tri_index, prim_addr);
#if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
  const ssef *ssef_verts = (ssef *)&kg->__prim_tri_verts.data[tri_vindex];
#else
  const float4 tri_a = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 0),
               tri_b = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 1),
               tri_c = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 2);
#endif
  float t, u, v;
  if (ray_triangle_intersect(P,
                             dir,
                             isect->t,
#if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
                             ssef_verts,
#else
                             float4_to_float3(tri_a),
                             float4_to_float3(tri_b),
                             float4_to_float3(tri_c),
#endif
                             &u,
                             &v,
                             &t)) {
#ifdef __VISIBILITY_FLAG__
    /* Visibility flag test. we do it here under the assumption
     * that most triangles are culled by node flags.
     */
    if (kernel_tex_fetch(__prim_visibility, prim_addr) & visibility)
#endif
    {
      isect->prim = prim_addr;
      isect->object = object;
      isect->type = PRIMITIVE_TRIANGLE;
      isect->u = u;
      isect->v = v;
      isect->t = t;
      return true;
    }
  }
  return false;
}

#ifdef __KERNEL_AVX2__
#  define cross256(A, B, C, D) _mm256_fmsub_ps(A, B, _mm256_mul_ps(C, D))
ccl_device_inline int ray_triangle_intersect8(KernelGlobals *kg,
                                              float3 ray_P,
                                              float3 ray_dir,
                                              Intersection **isect,
                                              uint visibility,
                                              int object,
                                              __m256 *triA,
                                              __m256 *triB,
                                              __m256 *triC,
                                              int prim_addr,
                                              int prim_num,
                                              uint *num_hits,
                                              uint max_hits,
                                              int *num_hits_in_instance,
                                              float isect_t)
{

  const unsigned char prim_num_mask = (1 << prim_num) - 1;

  const __m256i zero256 = _mm256_setzero_si256();

  const __m256 Px256 = _mm256_set1_ps(ray_P.x);
  const __m256 Py256 = _mm256_set1_ps(ray_P.y);
  const __m256 Pz256 = _mm256_set1_ps(ray_P.z);

  const __m256 dirx256 = _mm256_set1_ps(ray_dir.x);
  const __m256 diry256 = _mm256_set1_ps(ray_dir.y);
  const __m256 dirz256 = _mm256_set1_ps(ray_dir.z);

  /* Calculate vertices relative to ray origin. */
  __m256 v0_x_256 = _mm256_sub_ps(triC[0], Px256);
  __m256 v0_y_256 = _mm256_sub_ps(triC[1], Py256);
  __m256 v0_z_256 = _mm256_sub_ps(triC[2], Pz256);

  __m256 v1_x_256 = _mm256_sub_ps(triA[0], Px256);
  __m256 v1_y_256 = _mm256_sub_ps(triA[1], Py256);
  __m256 v1_z_256 = _mm256_sub_ps(triA[2], Pz256);

  __m256 v2_x_256 = _mm256_sub_ps(triB[0], Px256);
  __m256 v2_y_256 = _mm256_sub_ps(triB[1], Py256);
  __m256 v2_z_256 = _mm256_sub_ps(triB[2], Pz256);

  __m256 v0_v1_x_256 = _mm256_add_ps(v0_x_256, v1_x_256);
  __m256 v0_v1_y_256 = _mm256_add_ps(v0_y_256, v1_y_256);
  __m256 v0_v1_z_256 = _mm256_add_ps(v0_z_256, v1_z_256);

  __m256 v0_v2_x_256 = _mm256_add_ps(v0_x_256, v2_x_256);
  __m256 v0_v2_y_256 = _mm256_add_ps(v0_y_256, v2_y_256);
  __m256 v0_v2_z_256 = _mm256_add_ps(v0_z_256, v2_z_256);

  __m256 v1_v2_x_256 = _mm256_add_ps(v1_x_256, v2_x_256);
  __m256 v1_v2_y_256 = _mm256_add_ps(v1_y_256, v2_y_256);
  __m256 v1_v2_z_256 = _mm256_add_ps(v1_z_256, v2_z_256);

  /* Calculate triangle edges. */
  __m256 e0_x_256 = _mm256_sub_ps(v2_x_256, v0_x_256);
  __m256 e0_y_256 = _mm256_sub_ps(v2_y_256, v0_y_256);
  __m256 e0_z_256 = _mm256_sub_ps(v2_z_256, v0_z_256);

  __m256 e1_x_256 = _mm256_sub_ps(v0_x_256, v1_x_256);
  __m256 e1_y_256 = _mm256_sub_ps(v0_y_256, v1_y_256);
  __m256 e1_z_256 = _mm256_sub_ps(v0_z_256, v1_z_256);

  __m256 e2_x_256 = _mm256_sub_ps(v1_x_256, v2_x_256);
  __m256 e2_y_256 = _mm256_sub_ps(v1_y_256, v2_y_256);
  __m256 e2_z_256 = _mm256_sub_ps(v1_z_256, v2_z_256);

  /* Perform edge tests. */
  /* cross (AyBz - AzBy, AzBx -AxBz,  AxBy - AyBx) */
  __m256 U_x_256 = cross256(v0_v2_y_256, e0_z_256, v0_v2_z_256, e0_y_256);
  __m256 U_y_256 = cross256(v0_v2_z_256, e0_x_256, v0_v2_x_256, e0_z_256);
  __m256 U_z_256 = cross256(v0_v2_x_256, e0_y_256, v0_v2_y_256, e0_x_256);
  /* vertical dot */
  __m256 U_256 = _mm256_mul_ps(U_x_256, dirx256);
  U_256 = _mm256_fmadd_ps(U_y_256, diry256, U_256);
  U_256 = _mm256_fmadd_ps(U_z_256, dirz256, U_256);

  __m256 V_x_256 = cross256(v0_v1_y_256, e1_z_256, v0_v1_z_256, e1_y_256);
  __m256 V_y_256 = cross256(v0_v1_z_256, e1_x_256, v0_v1_x_256, e1_z_256);
  __m256 V_z_256 = cross256(v0_v1_x_256, e1_y_256, v0_v1_y_256, e1_x_256);
  /* vertical dot */
  __m256 V_256 = _mm256_mul_ps(V_x_256, dirx256);
  V_256 = _mm256_fmadd_ps(V_y_256, diry256, V_256);
  V_256 = _mm256_fmadd_ps(V_z_256, dirz256, V_256);

  __m256 W_x_256 = cross256(v1_v2_y_256, e2_z_256, v1_v2_z_256, e2_y_256);
  __m256 W_y_256 = cross256(v1_v2_z_256, e2_x_256, v1_v2_x_256, e2_z_256);
  __m256 W_z_256 = cross256(v1_v2_x_256, e2_y_256, v1_v2_y_256, e2_x_256);
  /* vertical dot */
  __m256 W_256 = _mm256_mul_ps(W_x_256, dirx256);
  W_256 = _mm256_fmadd_ps(W_y_256, diry256, W_256);
  W_256 = _mm256_fmadd_ps(W_z_256, dirz256, W_256);

  __m256i U_256_1 = _mm256_srli_epi32(_mm256_castps_si256(U_256), 31);
  __m256i V_256_1 = _mm256_srli_epi32(_mm256_castps_si256(V_256), 31);
  __m256i W_256_1 = _mm256_srli_epi32(_mm256_castps_si256(W_256), 31);
  __m256i UVW_256_1 = _mm256_add_epi32(_mm256_add_epi32(U_256_1, V_256_1), W_256_1);

  const __m256i one256 = _mm256_set1_epi32(1);
  const __m256i two256 = _mm256_set1_epi32(2);

  __m256i mask_minmaxUVW_256 = _mm256_or_si256(_mm256_cmpeq_epi32(one256, UVW_256_1),
                                               _mm256_cmpeq_epi32(two256, UVW_256_1));

  unsigned char mask_minmaxUVW_pos = _mm256_movemask_ps(_mm256_castsi256_ps(mask_minmaxUVW_256));
  if ((mask_minmaxUVW_pos & prim_num_mask) == prim_num_mask) {  // all bits set
    return false;
  }

  /* Calculate geometry normal and denominator. */
  __m256 Ng1_x_256 = cross256(e1_y_256, e0_z_256, e1_z_256, e0_y_256);
  __m256 Ng1_y_256 = cross256(e1_z_256, e0_x_256, e1_x_256, e0_z_256);
  __m256 Ng1_z_256 = cross256(e1_x_256, e0_y_256, e1_y_256, e0_x_256);

  Ng1_x_256 = _mm256_add_ps(Ng1_x_256, Ng1_x_256);
  Ng1_y_256 = _mm256_add_ps(Ng1_y_256, Ng1_y_256);
  Ng1_z_256 = _mm256_add_ps(Ng1_z_256, Ng1_z_256);

  /* vertical dot */
  __m256 den_256 = _mm256_mul_ps(Ng1_x_256, dirx256);
  den_256 = _mm256_fmadd_ps(Ng1_y_256, diry256, den_256);
  den_256 = _mm256_fmadd_ps(Ng1_z_256, dirz256, den_256);

  /* Perform depth test. */
  __m256 T_256 = _mm256_mul_ps(Ng1_x_256, v0_x_256);
  T_256 = _mm256_fmadd_ps(Ng1_y_256, v0_y_256, T_256);
  T_256 = _mm256_fmadd_ps(Ng1_z_256, v0_z_256, T_256);

  const __m256i c0x80000000 = _mm256_set1_epi32(0x80000000);
  __m256i sign_den_256 = _mm256_and_si256(_mm256_castps_si256(den_256), c0x80000000);

  __m256 sign_T_256 = _mm256_castsi256_ps(
      _mm256_xor_si256(_mm256_castps_si256(T_256), sign_den_256));

  unsigned char mask_sign_T = _mm256_movemask_ps(sign_T_256);
  if (((mask_minmaxUVW_pos | mask_sign_T) & prim_num_mask) == prim_num_mask) {
    return false;
  }

  __m256 xor_signmask_256 = _mm256_castsi256_ps(
      _mm256_xor_si256(_mm256_castps_si256(den_256), sign_den_256));

  ccl_align(32) float den8[8], U8[8], V8[8], T8[8], sign_T8[8], xor_signmask8[8];
  ccl_align(32) unsigned int mask_minmaxUVW8[8];

  if (visibility == PATH_RAY_SHADOW_OPAQUE) {
    __m256i mask_final_256 = _mm256_cmpeq_epi32(mask_minmaxUVW_256, zero256);
    __m256i maskden256 = _mm256_cmpeq_epi32(_mm256_castps_si256(den_256), zero256);
    __m256i mask0 = _mm256_cmpgt_epi32(zero256, _mm256_castps_si256(sign_T_256));
    __m256 rayt_256 = _mm256_set1_ps((*isect)->t);
    __m256i mask1 = _mm256_cmpgt_epi32(
        _mm256_castps_si256(sign_T_256),
        _mm256_castps_si256(_mm256_mul_ps(
            _mm256_castsi256_ps(_mm256_xor_si256(_mm256_castps_si256(den_256), sign_den_256)),
            rayt_256)));
    mask0 = _mm256_or_si256(mask1, mask0);
    mask_final_256 = _mm256_andnot_si256(mask0, mask_final_256);  //(~mask_minmaxUVW_pos) &(~mask)
    mask_final_256 = _mm256_andnot_si256(
        maskden256, mask_final_256);  //(~mask_minmaxUVW_pos) &(~mask) & (~maskden)
    int mask_final = _mm256_movemask_ps(_mm256_castsi256_ps(mask_final_256));
    if ((mask_final & prim_num_mask) == 0) {
      return false;
    }
    while (mask_final != 0) {
      const int i = __bscf(mask_final);
      if (i >= prim_num) {
        return false;
      }
#  ifdef __VISIBILITY_FLAG__
      if ((kernel_tex_fetch(__prim_visibility, (prim_addr + i)) & visibility) == 0) {
        continue;
      }
#  endif
      __m256 inv_den_256 = _mm256_rcp_ps(den_256);
      U_256 = _mm256_mul_ps(U_256, inv_den_256);
      V_256 = _mm256_mul_ps(V_256, inv_den_256);
      T_256 = _mm256_mul_ps(T_256, inv_den_256);
      _mm256_store_ps(U8, U_256);
      _mm256_store_ps(V8, V_256);
      _mm256_store_ps(T8, T_256);
      (*isect)->u = U8[i];
      (*isect)->v = V8[i];
      (*isect)->t = T8[i];
      (*isect)->prim = (prim_addr + i);
      (*isect)->object = object;
      (*isect)->type = PRIMITIVE_TRIANGLE;
      return true;
    }
    return false;
  }
  else {
    _mm256_store_ps(den8, den_256);
    _mm256_store_ps(U8, U_256);
    _mm256_store_ps(V8, V_256);
    _mm256_store_ps(T8, T_256);

    _mm256_store_ps(sign_T8, sign_T_256);
    _mm256_store_ps(xor_signmask8, xor_signmask_256);
    _mm256_store_si256((__m256i *)mask_minmaxUVW8, mask_minmaxUVW_256);

    int ret = false;

    if (visibility == PATH_RAY_SHADOW) {
      for (int i = 0; i < prim_num; i++) {
        if (mask_minmaxUVW8[i]) {
          continue;
        }
#  ifdef __VISIBILITY_FLAG__
        if ((kernel_tex_fetch(__prim_visibility, (prim_addr + i)) & visibility) == 0) {
          continue;
        }
#  endif
        if ((sign_T8[i] < 0.0f) || (sign_T8[i] > (*isect)->t * xor_signmask8[i])) {
          continue;
        }
        if (!den8[i]) {
          continue;
        }
        const float inv_den = 1.0f / den8[i];
        (*isect)->u = U8[i] * inv_den;
        (*isect)->v = V8[i] * inv_den;
        (*isect)->t = T8[i] * inv_den;
        (*isect)->prim = (prim_addr + i);
        (*isect)->object = object;
        (*isect)->type = PRIMITIVE_TRIANGLE;
        const int prim = kernel_tex_fetch(__prim_index, (*isect)->prim);
        int shader = 0;
#  ifdef __HAIR__
        if (kernel_tex_fetch(__prim_type, (*isect)->prim) & PRIMITIVE_ALL_TRIANGLE)
#  endif
        {
          shader = kernel_tex_fetch(__tri_shader, prim);
        }
#  ifdef __HAIR__
        else {
          float4 str = kernel_tex_fetch(__curves, prim);
          shader = __float_as_int(str.z);
        }
#  endif
        const int flag = kernel_tex_fetch(__shaders, (shader & SHADER_MASK)).flags;
        /* If no transparent shadows, all light is blocked. */
        if (!(flag & SD_HAS_TRANSPARENT_SHADOW)) {
          return 2;
        }
        /* If maximum number of hits reached, block all light. */
        else if (num_hits == NULL || *num_hits == max_hits) {
          return 2;
        }
        /* Move on to next entry in intersections array. */
        ret = true;
        (*isect)++;
        (*num_hits)++;
        (*num_hits_in_instance)++;
        (*isect)->t = isect_t;
      }
    }
    else {
      for (int i = 0; i < prim_num; i++) {
        if (mask_minmaxUVW8[i]) {
          continue;
        }
#  ifdef __VISIBILITY_FLAG__
        if ((kernel_tex_fetch(__prim_visibility, (prim_addr + i)) & visibility) == 0) {
          continue;
        }
#  endif
        if ((sign_T8[i] < 0.0f) || (sign_T8[i] > (*isect)->t * xor_signmask8[i])) {
          continue;
        }
        if (!den8[i]) {
          continue;
        }
        const float inv_den = 1.0f / den8[i];
        (*isect)->u = U8[i] * inv_den;
        (*isect)->v = V8[i] * inv_den;
        (*isect)->t = T8[i] * inv_den;
        (*isect)->prim = (prim_addr + i);
        (*isect)->object = object;
        (*isect)->type = PRIMITIVE_TRIANGLE;
        ret = true;
      }
    }
    return ret;
  }
}

ccl_device_inline int triangle_intersect8(KernelGlobals *kg,
                                          Intersection **isect,
                                          float3 P,
                                          float3 dir,
                                          uint visibility,
                                          int object,
                                          int prim_addr,
                                          int prim_num,
                                          uint *num_hits,
                                          uint max_hits,
                                          int *num_hits_in_instance,
                                          float isect_t)
{
  __m128 tri_a[8], tri_b[8], tri_c[8];
  __m256 tritmp[12], tri[12];
  __m256 triA[3], triB[3], triC[3];

  int i, r;

  uint tri_vindex = kernel_tex_fetch(__prim_tri_index, prim_addr);
  for (i = 0; i < prim_num; i++) {
    tri_a[i] = *(__m128 *)&kg->__prim_tri_verts.data[tri_vindex++];
    tri_b[i] = *(__m128 *)&kg->__prim_tri_verts.data[tri_vindex++];
    tri_c[i] = *(__m128 *)&kg->__prim_tri_verts.data[tri_vindex++];
  }
  // create 9 or  12 placeholders
  tri[0] = _mm256_castps128_ps256(tri_a[0]);  //_mm256_zextps128_ps256
  tri[1] = _mm256_castps128_ps256(tri_b[0]);  //_mm256_zextps128_ps256
  tri[2] = _mm256_castps128_ps256(tri_c[0]);  //_mm256_zextps128_ps256

  tri[3] = _mm256_castps128_ps256(tri_a[1]);  //_mm256_zextps128_ps256
  tri[4] = _mm256_castps128_ps256(tri_b[1]);  //_mm256_zextps128_ps256
  tri[5] = _mm256_castps128_ps256(tri_c[1]);  //_mm256_zextps128_ps256

  tri[6] = _mm256_castps128_ps256(tri_a[2]);  //_mm256_zextps128_ps256
  tri[7] = _mm256_castps128_ps256(tri_b[2]);  //_mm256_zextps128_ps256
  tri[8] = _mm256_castps128_ps256(tri_c[2]);  //_mm256_zextps128_ps256

  if (prim_num > 3) {
    tri[9] = _mm256_castps128_ps256(tri_a[3]);   //_mm256_zextps128_ps256
    tri[10] = _mm256_castps128_ps256(tri_b[3]);  //_mm256_zextps128_ps256
    tri[11] = _mm256_castps128_ps256(tri_c[3]);  //_mm256_zextps128_ps256
  }

  for (i = 4, r = 0; i < prim_num; i++, r += 3) {
    tri[r] = _mm256_insertf128_ps(tri[r], tri_a[i], 1);
    tri[r + 1] = _mm256_insertf128_ps(tri[r + 1], tri_b[i], 1);
    tri[r + 2] = _mm256_insertf128_ps(tri[r + 2], tri_c[i], 1);
  }

  //------------------------------------------------
  // 0!  Xa0 Ya0 Za0 1 Xa4 Ya4 Za4  1
  // 1!  Xb0 Yb0 Zb0 1 Xb4 Yb4 Zb4 1
  // 2!  Xc0 Yc0 Zc0 1 Xc4 Yc4 Zc4 1

  // 3!  Xa1 Ya1 Za1 1 Xa5 Ya5 Za5 1
  // 4!  Xb1 Yb1 Zb1 1 Xb5 Yb5 Zb5  1
  // 5!  Xc1 Yc1 Zc1 1 Xc5 Yc5 Zc5 1

  // 6!  Xa2 Ya2 Za2 1 Xa6 Ya6 Za6 1
  // 7!  Xb2 Yb2 Zb2 1 Xb6 Yb6 Zb6  1
  // 8!  Xc2 Yc2 Zc2 1 Xc6 Yc6 Zc6 1

  // 9!  Xa3 Ya3 Za3 1 Xa7 Ya7 Za7  1
  // 10! Xb3 Yb3 Zb3 1 Xb7 Yb7 Zb7  1
  // 11! Xc3 Yc3 Zc3 1 Xc7 Yc7 Zc7  1

  //"transpose"
  tritmp[0] = _mm256_unpacklo_ps(tri[0], tri[3]);  // 0!  Xa0 Xa1 Ya0 Ya1 Xa4 Xa5 Ya4 Ya5
  tritmp[1] = _mm256_unpackhi_ps(tri[0], tri[3]);  // 1!  Za0 Za1 1   1   Za4 Za5  1   1

  tritmp[2] = _mm256_unpacklo_ps(tri[6], tri[9]);  // 2!  Xa2 Xa3 Ya2 Ya3 Xa6 Xa7 Ya6 Ya7
  tritmp[3] = _mm256_unpackhi_ps(tri[6], tri[9]);  // 3!  Za2 Za3  1   1  Za6 Za7  1   1

  tritmp[4] = _mm256_unpacklo_ps(tri[1], tri[4]);  // 4!  Xb0 Xb1 Yb0 Yb1 Xb4 Xb5 Yb4 Yb5
  tritmp[5] = _mm256_unpackhi_ps(tri[1], tri[4]);  // 5!  Zb0 Zb1  1  1   Zb4 Zb5  1   1

  tritmp[6] = _mm256_unpacklo_ps(tri[7], tri[10]);  // 6!  Xb2 Xb3 Yb2 Yb3 Xb6 Xb7 Yb6 Yb7
  tritmp[7] = _mm256_unpackhi_ps(tri[7], tri[10]);  // 7!  Zb2 Zb3  1    1 Zb6 Zb7  1   1

  tritmp[8] = _mm256_unpacklo_ps(tri[2], tri[5]);  // 8!  Xc0 Xc1 Yc0 Yc1 Xc4 Xc5 Yc4 Yc5
  tritmp[9] = _mm256_unpackhi_ps(tri[2], tri[5]);  // 9!  Zc0 Zc1  1   1  Zc4 Zc5  1   1

  tritmp[10] = _mm256_unpacklo_ps(tri[8], tri[11]);  // 10! Xc2 Xc3 Yc2 Yc3 Xc6 Xc7 Yc6 Yc7
  tritmp[11] = _mm256_unpackhi_ps(tri[8], tri[11]);  // 11! Zc2 Zc3  1   1  Zc6 Zc7  1   1

  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  triA[0] = _mm256_castpd_ps(
      _mm256_unpacklo_pd(_mm256_castps_pd(tritmp[0]),
                         _mm256_castps_pd(tritmp[2])));  //  Xa0 Xa1 Xa2 Xa3 Xa4 Xa5 Xa6 Xa7
  triA[1] = _mm256_castpd_ps(
      _mm256_unpackhi_pd(_mm256_castps_pd(tritmp[0]),
                         _mm256_castps_pd(tritmp[2])));  //  Ya0 Ya1 Ya2 Ya3 Ya4 Ya5 Ya6 Ya7
  triA[2] = _mm256_castpd_ps(
      _mm256_unpacklo_pd(_mm256_castps_pd(tritmp[1]),
                         _mm256_castps_pd(tritmp[3])));  //  Za0 Za1 Za2 Za3 Za4 Za5 Za6 Za7

  triB[0] = _mm256_castpd_ps(
      _mm256_unpacklo_pd(_mm256_castps_pd(tritmp[4]),
                         _mm256_castps_pd(tritmp[6])));  //  Xb0 Xb1  Xb2 Xb3 Xb4 Xb5 Xb5 Xb7
  triB[1] = _mm256_castpd_ps(
      _mm256_unpackhi_pd(_mm256_castps_pd(tritmp[4]),
                         _mm256_castps_pd(tritmp[6])));  //  Yb0 Yb1  Yb2 Yb3 Yb4 Yb5 Yb5 Yb7
  triB[2] = _mm256_castpd_ps(
      _mm256_unpacklo_pd(_mm256_castps_pd(tritmp[5]),
                         _mm256_castps_pd(tritmp[7])));  //    Zb0 Zb1  Zb2 Zb3 Zb4 Zb5 Zb5 Zb7

  triC[0] = _mm256_castpd_ps(
      _mm256_unpacklo_pd(_mm256_castps_pd(tritmp[8]),
                         _mm256_castps_pd(tritmp[10])));  // Xc0 Xc1 Xc2 Xc3 Xc4 Xc5 Xc6 Xc7
  triC[1] = _mm256_castpd_ps(
      _mm256_unpackhi_pd(_mm256_castps_pd(tritmp[8]),
                         _mm256_castps_pd(tritmp[10])));  // Yc0 Yc1 Yc2 Yc3 Yc4 Yc5 Yc6 Yc7
  triC[2] = _mm256_castpd_ps(
      _mm256_unpacklo_pd(_mm256_castps_pd(tritmp[9]),
                         _mm256_castps_pd(tritmp[11])));  // Zc0 Zc1 Zc2 Zc3 Zc4 Zc5 Zc6 Zc7

  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

  int result = ray_triangle_intersect8(kg,
                                       P,
                                       dir,
                                       isect,
                                       visibility,
                                       object,
                                       triA,
                                       triB,
                                       triC,
                                       prim_addr,
                                       prim_num,
                                       num_hits,
                                       max_hits,
                                       num_hits_in_instance,
                                       isect_t);
  return result;
}

#endif /* __KERNEL_AVX2__ */

/* Special ray intersection routines for subsurface scattering. In that case we
 * only want to intersect with primitives in the same object, and if case of
 * multiple hits we pick a single random primitive as the intersection point.
 * Returns whether traversal should be stopped.
 */

#ifdef __BVH_LOCAL__
ccl_device_inline bool triangle_intersect_local(KernelGlobals *kg,
                                                LocalIntersection *local_isect,
                                                float3 P,
                                                float3 dir,
                                                int object,
                                                int local_object,
                                                int prim_addr,
                                                float tmax,
                                                uint *lcg_state,
                                                int max_hits)
{
  /* Only intersect with matching object, for instanced objects we
   * already know we are only intersecting the right object. */
  if (object == OBJECT_NONE) {
    if (kernel_tex_fetch(__prim_object, prim_addr) != local_object) {
      return false;
    }
  }

  const uint tri_vindex = kernel_tex_fetch(__prim_tri_index, prim_addr);
#  if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
  const ssef *ssef_verts = (ssef *)&kg->__prim_tri_verts.data[tri_vindex];
#  else
  const float3 tri_a = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 0)),
               tri_b = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 1)),
               tri_c = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 2));
#  endif
  float t, u, v;
  if (!ray_triangle_intersect(P,
                              dir,
                              tmax,
#  if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
                              ssef_verts,
#  else
                              tri_a,
                              tri_b,
                              tri_c,
#  endif
                              &u,
                              &v,
                              &t)) {
    return false;
  }

  /* If no actual hit information is requested, just return here. */
  if (max_hits == 0) {
    return true;
  }

  int hit;
  if (lcg_state) {
    /* Record up to max_hits intersections. */
    for (int i = min(max_hits, local_isect->num_hits) - 1; i >= 0; --i) {
      if (local_isect->hits[i].t == t) {
        return false;
      }
    }

    local_isect->num_hits++;

    if (local_isect->num_hits <= max_hits) {
      hit = local_isect->num_hits - 1;
    }
    else {
      /* reservoir sampling: if we are at the maximum number of
       * hits, randomly replace element or skip it */
      hit = lcg_step_uint(lcg_state) % local_isect->num_hits;

      if (hit >= max_hits)
        return false;
    }
  }
  else {
    /* Record closest intersection only. */
    if (local_isect->num_hits && t > local_isect->hits[0].t) {
      return false;
    }

    hit = 0;
    local_isect->num_hits = 1;
  }

  /* Record intersection. */
  Intersection *isect = &local_isect->hits[hit];
  isect->prim = prim_addr;
  isect->object = object;
  isect->type = PRIMITIVE_TRIANGLE;
  isect->u = u;
  isect->v = v;
  isect->t = t;

  /* Record geometric normal. */
#  if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
  const float3 tri_a = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 0)),
               tri_b = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 1)),
               tri_c = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex + 2));
#  endif
  local_isect->Ng[hit] = normalize(cross(tri_b - tri_a, tri_c - tri_a));

  return false;
}
#endif /* __BVH_LOCAL__ */

/* Refine triangle intersection to more precise hit point. For rays that travel
 * far the precision is often not so good, this reintersects the primitive from
 * a closer distance. */

/* Reintersections uses the paper:
 *
 * Tomas Moeller
 * Fast, minimum storage ray/triangle intersection
 * http://www.cs.virginia.edu/~gfx/Courses/2003/ImageSynthesis/papers/Acceleration/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf
 */

ccl_device_inline float3 triangle_refine(KernelGlobals *kg,
                                         ShaderData *sd,
                                         const Intersection *isect,
                                         const Ray *ray)
{
  float3 P = ray->P;
  float3 D = ray->D;
  float t = isect->t;

#ifdef __INTERSECTION_REFINE__
  if (isect->object != OBJECT_NONE) {
    if (UNLIKELY(t == 0.0f)) {
      return P;
    }
#  ifdef __OBJECT_MOTION__
    Transform tfm = sd->ob_itfm;
#  else
    Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
#  endif

    P = transform_point(&tfm, P);
    D = transform_direction(&tfm, D * t);
    D = normalize_len(D, &t);
  }

  P = P + D * t;

  const uint tri_vindex = kernel_tex_fetch(__prim_tri_index, isect->prim);
  const float4 tri_a = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 0),
               tri_b = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 1),
               tri_c = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 2);
  float3 edge1 = make_float3(tri_a.x - tri_c.x, tri_a.y - tri_c.y, tri_a.z - tri_c.z);
  float3 edge2 = make_float3(tri_b.x - tri_c.x, tri_b.y - tri_c.y, tri_b.z - tri_c.z);
  float3 tvec = make_float3(P.x - tri_c.x, P.y - tri_c.y, P.z - tri_c.z);
  float3 qvec = cross(tvec, edge1);
  float3 pvec = cross(D, edge2);
  float det = dot(edge1, pvec);
  if (det != 0.0f) {
    /* If determinant is zero it means ray lies in the plane of
     * the triangle. It is possible in theory due to watertight
     * nature of triangle intersection. For such cases we simply
     * don't refine intersection hoping it'll go all fine.
     */
    float rt = dot(edge2, qvec) / det;
    P = P + D * rt;
  }

  if (isect->object != OBJECT_NONE) {
#  ifdef __OBJECT_MOTION__
    Transform tfm = sd->ob_tfm;
#  else
    Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#  endif

    P = transform_point(&tfm, P);
  }

  return P;
#else
  return P + D * t;
#endif
}

/* Same as above, except that isect->t is assumed to be in object space for
 * instancing.
 */
ccl_device_inline float3 triangle_refine_local(KernelGlobals *kg,
                                               ShaderData *sd,
                                               const Intersection *isect,
                                               const Ray *ray)
{
  float3 P = ray->P;
  float3 D = ray->D;
  float t = isect->t;

  if (isect->object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
    Transform tfm = sd->ob_itfm;
#else
    Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
#endif

    P = transform_point(&tfm, P);
    D = transform_direction(&tfm, D);
    D = normalize(D);
  }

  P = P + D * t;

#ifdef __INTERSECTION_REFINE__
  const uint tri_vindex = kernel_tex_fetch(__prim_tri_index, isect->prim);
  const float4 tri_a = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 0),
               tri_b = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 1),
               tri_c = kernel_tex_fetch(__prim_tri_verts, tri_vindex + 2);
  float3 edge1 = make_float3(tri_a.x - tri_c.x, tri_a.y - tri_c.y, tri_a.z - tri_c.z);
  float3 edge2 = make_float3(tri_b.x - tri_c.x, tri_b.y - tri_c.y, tri_b.z - tri_c.z);
  float3 tvec = make_float3(P.x - tri_c.x, P.y - tri_c.y, P.z - tri_c.z);
  float3 qvec = cross(tvec, edge1);
  float3 pvec = cross(D, edge2);
  float det = dot(edge1, pvec);
  if (det != 0.0f) {
    /* If determinant is zero it means ray lies in the plane of
     * the triangle. It is possible in theory due to watertight
     * nature of triangle intersection. For such cases we simply
     * don't refine intersection hoping it'll go all fine.
     */
    float rt = dot(edge2, qvec) / det;
    P = P + D * rt;
  }
#endif /* __INTERSECTION_REFINE__ */

  if (isect->object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
    Transform tfm = sd->ob_tfm;
#else
    Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#endif

    P = transform_point(&tfm, P);
  }

  return P;
}

CCL_NAMESPACE_END
