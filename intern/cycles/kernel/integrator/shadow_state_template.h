/*
 * Copyright 2011-2021 Blender Foundation
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

/********************************* Shadow Path State **************************/

KERNEL_STRUCT_BEGIN(shadow_path)
/* Index of a pixel within the device render buffer. */
KERNEL_STRUCT_MEMBER(shadow_path, uint32_t, render_pixel_index, KERNEL_FEATURE_PATH_TRACING)
/* Current sample number. */
KERNEL_STRUCT_MEMBER(shadow_path, uint32_t, sample, KERNEL_FEATURE_PATH_TRACING)
/* Random number generator seed. */
KERNEL_STRUCT_MEMBER(shadow_path, uint32_t, rng_hash, KERNEL_FEATURE_PATH_TRACING)
/* Random number dimension offset. */
KERNEL_STRUCT_MEMBER(shadow_path, uint16_t, rng_offset, KERNEL_FEATURE_PATH_TRACING)
/* Current ray bounce depth. */
KERNEL_STRUCT_MEMBER(shadow_path, uint16_t, bounce, KERNEL_FEATURE_PATH_TRACING)
/* Current transparent ray bounce depth. */
KERNEL_STRUCT_MEMBER(shadow_path, uint16_t, transparent_bounce, KERNEL_FEATURE_PATH_TRACING)
/* Current diffuse ray bounce depth. */
KERNEL_STRUCT_MEMBER(shadow_path, uint16_t, diffuse_bounce, KERNEL_FEATURE_PATH_TRACING)
/* Current glossy ray bounce depth. */
KERNEL_STRUCT_MEMBER(shadow_path, uint16_t, glossy_bounce, KERNEL_FEATURE_PATH_TRACING)
/* Current transmission ray bounce depth. */
KERNEL_STRUCT_MEMBER(shadow_path, uint16_t, transmission_bounce, KERNEL_FEATURE_PATH_TRACING)
/* DeviceKernel bit indicating queued kernels. */
KERNEL_STRUCT_MEMBER(shadow_path, uint16_t, queued_kernel, KERNEL_FEATURE_PATH_TRACING)
/* enum PathRayFlag */
KERNEL_STRUCT_MEMBER(shadow_path, uint32_t, flag, KERNEL_FEATURE_PATH_TRACING)
/* Throughput. */
KERNEL_STRUCT_MEMBER(shadow_path, float3, throughput, KERNEL_FEATURE_PATH_TRACING)
/* Throughput for shadow pass. */
KERNEL_STRUCT_MEMBER(shadow_path,
                     float3,
                     unshadowed_throughput,
                     KERNEL_FEATURE_SHADOW_PASS | KERNEL_FEATURE_AO_ADDITIVE)
/* Ratio of throughput to distinguish diffuse and glossy render passes. */
KERNEL_STRUCT_MEMBER(shadow_path, float3, diffuse_glossy_ratio, KERNEL_FEATURE_LIGHT_PASSES)
/* Number of intersections found by ray-tracing. */
KERNEL_STRUCT_MEMBER(shadow_path, uint16_t, num_hits, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_END(shadow_path)

/********************************** Shadow Ray *******************************/

KERNEL_STRUCT_BEGIN(shadow_ray)
KERNEL_STRUCT_MEMBER(shadow_ray, float3, P, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(shadow_ray, float3, D, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(shadow_ray, float, t, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(shadow_ray, float, time, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(shadow_ray, float, dP, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_END(shadow_ray)

/*********************** Shadow Intersection result **************************/

/* Result from scene intersection. */
KERNEL_STRUCT_BEGIN(shadow_isect)
KERNEL_STRUCT_ARRAY_MEMBER(shadow_isect, float, t, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_ARRAY_MEMBER(shadow_isect, float, u, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_ARRAY_MEMBER(shadow_isect, float, v, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_ARRAY_MEMBER(shadow_isect, int, prim, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_ARRAY_MEMBER(shadow_isect, int, object, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_ARRAY_MEMBER(shadow_isect, int, type, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_END_ARRAY(shadow_isect,
                        INTEGRATOR_SHADOW_ISECT_SIZE_CPU,
                        INTEGRATOR_SHADOW_ISECT_SIZE_GPU)

/**************************** Shadow Volume Stack *****************************/

KERNEL_STRUCT_BEGIN(shadow_volume_stack)
KERNEL_STRUCT_ARRAY_MEMBER(shadow_volume_stack, int, object, KERNEL_FEATURE_VOLUME)
KERNEL_STRUCT_ARRAY_MEMBER(shadow_volume_stack, int, shader, KERNEL_FEATURE_VOLUME)
KERNEL_STRUCT_END_ARRAY(shadow_volume_stack,
                        KERNEL_STRUCT_VOLUME_STACK_SIZE,
                        KERNEL_STRUCT_VOLUME_STACK_SIZE)
