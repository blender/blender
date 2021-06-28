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

/* This function should be used to compute a modified ray start position for
 * rays leaving from a surface. The algorithm slightly distorts flat surface
 * of a triangle. Surface is lifted by amount h along normal n in the incident
 * point. */

ccl_device_inline float3 smooth_surface_offset(KernelGlobals *kg, ShaderData *sd, float3 Ng)
{
  float3 V[3], N[3];
  triangle_vertices_and_normals(kg, sd->prim, V, N);

  const float u = sd->u, v = sd->v;
  const float w = 1 - u - v;
  float3 P = V[0] * u + V[1] * v + V[2] * w; /* Local space */
  float3 n = N[0] * u + N[1] * v + N[2] * w; /* We get away without normalization */
  n = transform_direction(&(sd->ob_tfm), n); /* Normal x scale, world space */

  /* Parabolic approximation */
  float a = dot(N[2] - N[0], V[0] - V[2]);
  float b = dot(N[2] - N[1], V[1] - V[2]);
  float c = dot(N[1] - N[0], V[1] - V[0]);
  float h = a * u * (u - 1) + (a + b + c) * u * v + b * v * (v - 1);

  /* Check flipped normals */
  if (dot(n, Ng) > 0) {
    /* Local linear envelope */
    float h0 = max(max(dot(V[1] - V[0], N[0]), dot(V[2] - V[0], N[0])), 0.0f);
    float h1 = max(max(dot(V[0] - V[1], N[1]), dot(V[2] - V[1], N[1])), 0.0f);
    float h2 = max(max(dot(V[0] - V[2], N[2]), dot(V[1] - V[2], N[2])), 0.0f);
    h0 = max(dot(V[0] - P, N[0]) + h0, 0.0f);
    h1 = max(dot(V[1] - P, N[1]) + h1, 0.0f);
    h2 = max(dot(V[2] - P, N[2]) + h2, 0.0f);
    h = max(min(min(h0, h1), h2), h * 0.5f);
  }
  else {
    float h0 = max(max(dot(V[0] - V[1], N[0]), dot(V[0] - V[2], N[0])), 0.0f);
    float h1 = max(max(dot(V[1] - V[0], N[1]), dot(V[1] - V[2], N[1])), 0.0f);
    float h2 = max(max(dot(V[2] - V[0], N[2]), dot(V[2] - V[1], N[2])), 0.0f);
    h0 = max(dot(P - V[0], N[0]) + h0, 0.0f);
    h1 = max(dot(P - V[1], N[1]) + h1, 0.0f);
    h2 = max(dot(P - V[2], N[2]) + h2, 0.0f);
    h = min(-min(min(h0, h1), h2), h * 0.5f);
  }

  return n * h;
}

/* Ray offset to avoid shadow terminator artifact. */

ccl_device_inline float3 ray_offset_shadow(KernelGlobals *kg, ShaderData *sd, float3 L)
{
  float NL = dot(sd->N, L);
  bool transmit = (NL < 0.0f);
  float3 Ng = (transmit ? -sd->Ng : sd->Ng);
  float3 P = ray_offset(sd->P, Ng);

  if ((sd->type & PRIMITIVE_ALL_TRIANGLE) && (sd->shader & SHADER_SMOOTH_NORMAL)) {
    const float offset_cutoff =
        kernel_tex_fetch(__objects, sd->object).shadow_terminator_geometry_offset;
    /* Do ray offset (heavy stuff) only for close to be terminated triangles:
     * offset_cutoff = 0.1f means that 10-20% of rays will be affected. Also
     * make a smooth transition near the threshold. */
    if (offset_cutoff > 0.0f) {
      float NgL = dot(Ng, L);
      float offset_amount = 0.0f;
      if (NL < offset_cutoff) {
        offset_amount = clamp(2.0f - (NgL + NL) / offset_cutoff, 0.0f, 1.0f);
      }
      else {
        offset_amount = clamp(1.0f - NgL / offset_cutoff, 0.0f, 1.0f);
      }
      if (offset_amount > 0.0f) {
        P += smooth_surface_offset(kg, sd, Ng) * offset_amount;
      }
    }
  }

  return P;
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

/* Utility to quickly get a shader flags from an intersection. */

ccl_device_forceinline int intersection_get_shader_flags(KernelGlobals *ccl_restrict kg,
                                                         const Intersection *isect)
{
  const int prim = kernel_tex_fetch(__prim_index, isect->prim);
  int shader = 0;

#ifdef __HAIR__
  if (kernel_tex_fetch(__prim_type, isect->prim) & PRIMITIVE_ALL_TRIANGLE)
#endif
  {
    shader = kernel_tex_fetch(__tri_shader, prim);
  }
#ifdef __HAIR__
  else {
    float4 str = kernel_tex_fetch(__curves, prim);
    shader = __float_as_int(str.z);
  }
#endif

  return kernel_tex_fetch(__shaders, (shader & SHADER_MASK)).flags;
}

ccl_device_forceinline int intersection_get_shader(KernelGlobals *ccl_restrict kg,
                                                   const Intersection *isect)
{
  const int prim = kernel_tex_fetch(__prim_index, isect->prim);
  int shader = 0;

#ifdef __HAIR__
  if (kernel_tex_fetch(__prim_type, isect->prim) & PRIMITIVE_ALL_TRIANGLE)
#endif
  {
    shader = kernel_tex_fetch(__tri_shader, prim);
  }
#ifdef __HAIR__
  else {
    float4 str = kernel_tex_fetch(__curves, prim);
    shader = __float_as_int(str.z);
  }
#endif

  return shader & SHADER_MASK;
}

CCL_NAMESPACE_END
