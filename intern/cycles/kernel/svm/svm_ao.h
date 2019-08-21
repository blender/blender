/*
 * Copyright 2011-2018 Blender Foundation
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

ccl_device_noinline float svm_ao(KernelGlobals *kg,
                                 ShaderData *sd,
                                 float3 N,
                                 ccl_addr_space PathState *state,
                                 float max_dist,
                                 int num_samples,
                                 int flags)
{
  if (flags & NODE_AO_GLOBAL_RADIUS) {
    max_dist = kernel_data.background.ao_distance;
  }

  /* Early out if no sampling needed. */
  if (max_dist <= 0.0f || num_samples < 1 || sd->object == OBJECT_NONE) {
    return 1.0f;
  }

  /* Can't raytrace from shaders like displacement, before BVH exists. */
  if (kernel_data.bvh.bvh_layout == BVH_LAYOUT_NONE) {
    return 1.0f;
  }

  if (flags & NODE_AO_INSIDE) {
    N = -N;
  }

  float3 T, B;
  make_orthonormals(N, &T, &B);

  int unoccluded = 0;
  for (int sample = 0; sample < num_samples; sample++) {
    float disk_u, disk_v;
    path_branched_rng_2D(
        kg, state->rng_hash, state, sample, num_samples, PRNG_BEVEL_U, &disk_u, &disk_v);

    float2 d = concentric_sample_disk(disk_u, disk_v);
    float3 D = make_float3(d.x, d.y, safe_sqrtf(1.0f - dot(d, d)));

    /* Create ray. */
    Ray ray;
    ray.P = ray_offset(sd->P, N);
    ray.D = D.x * T + D.y * B + D.z * N;
    ray.t = max_dist;
    ray.time = sd->time;
    ray.dP = sd->dP;
    ray.dD = differential3_zero();

    if (flags & NODE_AO_ONLY_LOCAL) {
      if (!scene_intersect_local(kg, &ray, NULL, sd->object, NULL, 0)) {
        unoccluded++;
      }
    }
    else {
      Intersection isect;
      if (!scene_intersect(kg, &ray, PATH_RAY_SHADOW_OPAQUE, &isect)) {
        unoccluded++;
      }
    }
  }

  return ((float)unoccluded) / num_samples;
}

ccl_device void svm_node_ao(
    KernelGlobals *kg, ShaderData *sd, ccl_addr_space PathState *state, float *stack, uint4 node)
{
  uint flags, dist_offset, normal_offset, out_ao_offset;
  decode_node_uchar4(node.y, &flags, &dist_offset, &normal_offset, &out_ao_offset);

  uint color_offset, out_color_offset, samples;
  decode_node_uchar4(node.z, &color_offset, &out_color_offset, &samples, NULL);

  float dist = stack_load_float_default(stack, dist_offset, node.w);
  float3 normal = stack_valid(normal_offset) ? stack_load_float3(stack, normal_offset) : sd->N;
  float ao = svm_ao(kg, sd, normal, state, dist, samples, flags);

  if (stack_valid(out_ao_offset)) {
    stack_store_float(stack, out_ao_offset, ao);
  }

  if (stack_valid(out_color_offset)) {
    float3 color = stack_load_float3(stack, color_offset);
    stack_store_float3(stack, out_color_offset, ao * color);
  }
}

#endif /* __SHADER_RAYTRACE__ */

CCL_NAMESPACE_END
