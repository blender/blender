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

CCL_NAMESPACE_BEGIN

#ifdef __SHADER_RAYTRACE__

/* Bevel shader averaging normals from nearby surfaces.
 *
 * Sampling strategy from: BSSRDF Importance Sampling, SIGGRAPH 2013
 * http://library.imageworks.com/pdfs/imageworks-library-BSSRDF-sampling.pdf
 */

ccl_device_noinline float3 svm_bevel(KernelGlobals *kg,
                                     ShaderData *sd,
                                     ccl_addr_space PathState *state,
                                     float radius,
                                     int num_samples)
{
  /* Early out if no sampling needed. */
  if (radius <= 0.0f || num_samples < 1 || sd->object == OBJECT_NONE) {
    return sd->N;
  }

  /* Can't raytrace from shaders like displacement, before BVH exists. */
  if (kernel_data.bvh.bvh_layout == BVH_LAYOUT_NONE) {
    return sd->N;
  }

  /* Don't bevel for blurry indirect rays. */
  if (state->min_ray_pdf < 8.0f) {
    return sd->N;
  }

  /* Setup for multi intersection. */
  LocalIntersection isect;
  uint lcg_state = lcg_state_init_addrspace(state, 0x64c6a40e);

  /* Sample normals from surrounding points on surface. */
  float3 sum_N = make_float3(0.0f, 0.0f, 0.0f);

  for (int sample = 0; sample < num_samples; sample++) {
    float disk_u, disk_v;
    path_branched_rng_2D(
        kg, state->rng_hash, state, sample, num_samples, PRNG_BEVEL_U, &disk_u, &disk_v);

    /* Pick random axis in local frame and point on disk. */
    float3 disk_N, disk_T, disk_B;
    float pick_pdf_N, pick_pdf_T, pick_pdf_B;

    disk_N = sd->Ng;
    make_orthonormals(disk_N, &disk_T, &disk_B);

    float axisu = disk_u;

    if (axisu < 0.5f) {
      pick_pdf_N = 0.5f;
      pick_pdf_T = 0.25f;
      pick_pdf_B = 0.25f;
      disk_u *= 2.0f;
    }
    else if (axisu < 0.75f) {
      float3 tmp = disk_N;
      disk_N = disk_T;
      disk_T = tmp;
      pick_pdf_N = 0.25f;
      pick_pdf_T = 0.5f;
      pick_pdf_B = 0.25f;
      disk_u = (disk_u - 0.5f) * 4.0f;
    }
    else {
      float3 tmp = disk_N;
      disk_N = disk_B;
      disk_B = tmp;
      pick_pdf_N = 0.25f;
      pick_pdf_T = 0.25f;
      pick_pdf_B = 0.5f;
      disk_u = (disk_u - 0.75f) * 4.0f;
    }

    /* Sample point on disk. */
    float phi = M_2PI_F * disk_u;
    float disk_r = disk_v;
    float disk_height;

    /* Perhaps find something better than Cubic BSSRDF, but happens to work well. */
    bssrdf_cubic_sample(radius, 0.0f, disk_r, &disk_r, &disk_height);

    float3 disk_P = (disk_r * cosf(phi)) * disk_T + (disk_r * sinf(phi)) * disk_B;

    /* Create ray. */
    Ray *ray = &isect.ray;
    ray->P = sd->P + disk_N * disk_height + disk_P;
    ray->D = -disk_N;
    ray->t = 2.0f * disk_height;
    ray->dP = sd->dP;
    ray->dD = differential3_zero();
    ray->time = sd->time;

    /* Intersect with the same object. if multiple intersections are found it
     * will use at most LOCAL_MAX_HITS hits, a random subset of all hits. */
    scene_intersect_local(kg, *ray, &isect, sd->object, &lcg_state, LOCAL_MAX_HITS);

    int num_eval_hits = min(isect.num_hits, LOCAL_MAX_HITS);

    for (int hit = 0; hit < num_eval_hits; hit++) {
      /* Quickly retrieve P and Ng without setting up ShaderData. */
      float3 hit_P;
      if (sd->type & PRIMITIVE_TRIANGLE) {
        hit_P = triangle_refine_local(kg, sd, &isect.hits[hit], ray);
      }
#  ifdef __OBJECT_MOTION__
      else if (sd->type & PRIMITIVE_MOTION_TRIANGLE) {
        float3 verts[3];
        motion_triangle_vertices(
            kg, sd->object, kernel_tex_fetch(__prim_index, isect.hits[hit].prim), sd->time, verts);
        hit_P = motion_triangle_refine_local(kg, sd, &isect.hits[hit], ray, verts);
      }
#  endif /* __OBJECT_MOTION__ */

      /* Get geometric normal. */
      float3 hit_Ng = isect.Ng[hit];
      int object = (isect.hits[hit].object == OBJECT_NONE) ?
                       kernel_tex_fetch(__prim_object, isect.hits[hit].prim) :
                       isect.hits[hit].object;
      int object_flag = kernel_tex_fetch(__object_flag, object);
      if (object_flag & SD_OBJECT_NEGATIVE_SCALE_APPLIED) {
        hit_Ng = -hit_Ng;
      }

      /* Compute smooth normal. */
      float3 N = hit_Ng;
      int prim = kernel_tex_fetch(__prim_index, isect.hits[hit].prim);
      int shader = kernel_tex_fetch(__tri_shader, prim);

      if (shader & SHADER_SMOOTH_NORMAL) {
        float u = isect.hits[hit].u;
        float v = isect.hits[hit].v;

        if (sd->type & PRIMITIVE_TRIANGLE) {
          N = triangle_smooth_normal(kg, N, prim, u, v);
        }
#  ifdef __OBJECT_MOTION__
        else if (sd->type & PRIMITIVE_MOTION_TRIANGLE) {
          N = motion_triangle_smooth_normal(kg, N, sd->object, prim, u, v, sd->time);
        }
#  endif /* __OBJECT_MOTION__ */
      }

      /* Transform normals to world space. */
      if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
        object_normal_transform(kg, sd, &N);
        object_normal_transform(kg, sd, &hit_Ng);
      }

      /* Probability densities for local frame axes. */
      float pdf_N = pick_pdf_N * fabsf(dot(disk_N, hit_Ng));
      float pdf_T = pick_pdf_T * fabsf(dot(disk_T, hit_Ng));
      float pdf_B = pick_pdf_B * fabsf(dot(disk_B, hit_Ng));

      /* Multiple importance sample between 3 axes, power heuristic
       * found to be slightly better than balance heuristic. pdf_N
       * in the MIS weight and denominator cancelled out. */
      float w = pdf_N / (sqr(pdf_N) + sqr(pdf_T) + sqr(pdf_B));
      if (isect.num_hits > LOCAL_MAX_HITS) {
        w *= isect.num_hits / (float)LOCAL_MAX_HITS;
      }

      /* Real distance to sampled point. */
      float r = len(hit_P - sd->P);

      /* Compute weight. */
      float pdf = bssrdf_cubic_pdf(radius, 0.0f, r);
      float disk_pdf = bssrdf_cubic_pdf(radius, 0.0f, disk_r);

      w *= pdf / disk_pdf;

      /* Sum normal and weight. */
      sum_N += w * N;
    }
  }

  /* Normalize. */
  float3 N = safe_normalize(sum_N);
  return is_zero(N) ? sd->N : (sd->flag & SD_BACKFACING) ? -N : N;
}

ccl_device void svm_node_bevel(
    KernelGlobals *kg, ShaderData *sd, ccl_addr_space PathState *state, float *stack, uint4 node)
{
  uint num_samples, radius_offset, normal_offset, out_offset;
  decode_node_uchar4(node.y, &num_samples, &radius_offset, &normal_offset, &out_offset);

  float radius = stack_load_float(stack, radius_offset);
  float3 bevel_N = svm_bevel(kg, sd, state, radius, num_samples);

  if (stack_valid(normal_offset)) {
    /* Preserve input normal. */
    float3 ref_N = stack_load_float3(stack, normal_offset);
    bevel_N = normalize(ref_N + (bevel_N - sd->N));
  }

  stack_store_float3(stack, out_offset, bevel_N);
}

#endif /* __SHADER_RAYTRACE__ */

CCL_NAMESPACE_END
