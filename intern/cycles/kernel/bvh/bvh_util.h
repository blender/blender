/*
 * Copyright 2011-2013 Blender Foundation
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

#pragma once

CCL_NAMESPACE_BEGIN

/* Ray offset to avoid self intersection.
 *
 * This function should be used to compute a modified ray start position for
 * rays leaving from a surface. */

ccl_device_inline float3 ray_offset(float3 P, float3 Ng)
{
#ifdef __INTERSECTION_REFINE__
  const float epsilon_f = 1e-5f;
  /* ideally this should match epsilon_f, but instancing and motion blur
   * precision makes it problematic */
  const float epsilon_test = 1.0f;
  const int epsilon_i = 32;

  float3 res;

  /* x component */
  if (fabsf(P.x) < epsilon_test) {
    res.x = P.x + Ng.x * epsilon_f;
  }
  else {
    uint ix = __float_as_uint(P.x);
    ix += ((ix ^ __float_as_uint(Ng.x)) >> 31) ? -epsilon_i : epsilon_i;
    res.x = __uint_as_float(ix);
  }

  /* y component */
  if (fabsf(P.y) < epsilon_test) {
    res.y = P.y + Ng.y * epsilon_f;
  }
  else {
    uint iy = __float_as_uint(P.y);
    iy += ((iy ^ __float_as_uint(Ng.y)) >> 31) ? -epsilon_i : epsilon_i;
    res.y = __uint_as_float(iy);
  }

  /* z component */
  if (fabsf(P.z) < epsilon_test) {
    res.z = P.z + Ng.z * epsilon_f;
  }
  else {
    uint iz = __float_as_uint(P.z);
    iz += ((iz ^ __float_as_uint(Ng.z)) >> 31) ? -epsilon_i : epsilon_i;
    res.z = __uint_as_float(iz);
  }

  return res;
#else
  const float epsilon_f = 1e-4f;
  return P + epsilon_f * Ng;
#endif
}

#if defined(__VOLUME_RECORD_ALL__) || (defined(__SHADOW_RECORD_ALL__) && defined(__KERNEL_CPU__))
/* ToDo: Move to another file? */
ccl_device int intersections_compare(const void *a, const void *b)
{
  const Intersection *isect_a = (const Intersection *)a;
  const Intersection *isect_b = (const Intersection *)b;

  if (isect_a->t < isect_b->t)
    return -1;
  else if (isect_a->t > isect_b->t)
    return 1;
  else
    return 0;
}
#endif

#if defined(__SHADOW_RECORD_ALL__)
ccl_device_inline void sort_intersections(Intersection *hits, uint num_hits)
{
  kernel_assert(num_hits > 0);

#  ifdef __KERNEL_GPU__
  /* Use bubble sort which has more friendly memory pattern on GPU. */
  bool swapped;
  do {
    swapped = false;
    for (int j = 0; j < num_hits - 1; ++j) {
      if (hits[j].t > hits[j + 1].t) {
        struct Intersection tmp = hits[j];
        hits[j] = hits[j + 1];
        hits[j + 1] = tmp;
        swapped = true;
      }
    }
    --num_hits;
  } while (swapped);
#  else
  qsort(hits, num_hits, sizeof(Intersection), intersections_compare);
#  endif
}
#endif /* __SHADOW_RECORD_ALL__ | __VOLUME_RECORD_ALL__ */

/* Utility to quickly get flags from an intersection. */

ccl_device_forceinline int intersection_get_shader_flags(const KernelGlobals *ccl_restrict kg,
                                                         const Intersection *ccl_restrict isect)
{
  const int prim = isect->prim;
  int shader = 0;

#ifdef __HAIR__
  if (isect->type & PRIMITIVE_ALL_TRIANGLE)
#endif
  {
    shader = kernel_tex_fetch(__tri_shader, prim);
  }
#ifdef __HAIR__
  else {
    shader = kernel_tex_fetch(__curves, prim).shader_id;
  }
#endif

  return kernel_tex_fetch(__shaders, (shader & SHADER_MASK)).flags;
}

ccl_device_forceinline int intersection_get_shader_from_isect_prim(
    const KernelGlobals *ccl_restrict kg, const int prim, const int isect_type)
{
  int shader = 0;

#ifdef __HAIR__
  if (isect_type & PRIMITIVE_ALL_TRIANGLE)
#endif
  {
    shader = kernel_tex_fetch(__tri_shader, prim);
  }
#ifdef __HAIR__
  else {
    shader = kernel_tex_fetch(__curves, prim).shader_id;
  }
#endif

  return shader & SHADER_MASK;
}

ccl_device_forceinline int intersection_get_shader(const KernelGlobals *ccl_restrict kg,
                                                   const Intersection *ccl_restrict isect)
{
  return intersection_get_shader_from_isect_prim(kg, isect->prim, isect->type);
}

ccl_device_forceinline int intersection_get_object_flags(const KernelGlobals *ccl_restrict kg,
                                                         const Intersection *ccl_restrict isect)
{
  return kernel_tex_fetch(__object_flag, isect->object);
}

CCL_NAMESPACE_END
