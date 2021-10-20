
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

/************************************ Path State *****************************/

KERNEL_STRUCT_BEGIN(path)
/* Index of a pixel within the device render buffer where this path will write its result.
 * To get an actual offset within the buffer the value needs to be multiplied by the
 * `kernel_data.film.pass_stride`.
 *
 * The multiplication is delayed for later, so that state can use 32bit integer. */
KERNEL_STRUCT_MEMBER(path, uint32_t, render_pixel_index, KERNEL_FEATURE_PATH_TRACING)
/* Current sample number. */
KERNEL_STRUCT_MEMBER(path, uint16_t, sample, KERNEL_FEATURE_PATH_TRACING)
/* Current ray bounce depth. */
KERNEL_STRUCT_MEMBER(path, uint16_t, bounce, KERNEL_FEATURE_PATH_TRACING)
/* Current transparent ray bounce depth. */
KERNEL_STRUCT_MEMBER(path, uint16_t, transparent_bounce, KERNEL_FEATURE_PATH_TRACING)
/* Current diffuse ray bounce depth. */
KERNEL_STRUCT_MEMBER(path, uint16_t, diffuse_bounce, KERNEL_FEATURE_PATH_TRACING)
/* Current glossy ray bounce depth. */
KERNEL_STRUCT_MEMBER(path, uint16_t, glossy_bounce, KERNEL_FEATURE_PATH_TRACING)
/* Current transmission ray bounce depth. */
KERNEL_STRUCT_MEMBER(path, uint16_t, transmission_bounce, KERNEL_FEATURE_PATH_TRACING)
/* Current volume ray bounce depth. */
KERNEL_STRUCT_MEMBER(path, uint16_t, volume_bounce, KERNEL_FEATURE_PATH_TRACING)
/* Current volume bounds ray bounce depth. */
KERNEL_STRUCT_MEMBER(path, uint16_t, volume_bounds_bounce, KERNEL_FEATURE_PATH_TRACING)
/* DeviceKernel bit indicating queued kernels. */
KERNEL_STRUCT_MEMBER(path, uint16_t, queued_kernel, KERNEL_FEATURE_PATH_TRACING)
/* Random number generator seed. */
KERNEL_STRUCT_MEMBER(path, uint32_t, rng_hash, KERNEL_FEATURE_PATH_TRACING)
/* Random number dimension offset. */
KERNEL_STRUCT_MEMBER(path, uint16_t, rng_offset, KERNEL_FEATURE_PATH_TRACING)
/* enum PathRayFlag */
KERNEL_STRUCT_MEMBER(path, uint32_t, flag, KERNEL_FEATURE_PATH_TRACING)
/* Multiple importance sampling
 * The PDF of BSDF sampling at the last scatter point, and distance to the
 * last scatter point minus the last ray segment. This distance lets us
 * compute the complete distance through transparent surfaces and volumes. */
KERNEL_STRUCT_MEMBER(path, float, mis_ray_pdf, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(path, float, mis_ray_t, KERNEL_FEATURE_PATH_TRACING)
/* Filter glossy. */
KERNEL_STRUCT_MEMBER(path, float, min_ray_pdf, KERNEL_FEATURE_PATH_TRACING)
/* Throughput. */
KERNEL_STRUCT_MEMBER(path, float3, throughput, KERNEL_FEATURE_PATH_TRACING)
/* Ratio of throughput to distinguish diffuse and glossy render passes. */
KERNEL_STRUCT_MEMBER(path, float3, diffuse_glossy_ratio, KERNEL_FEATURE_LIGHT_PASSES)
/* Denoising. */
KERNEL_STRUCT_MEMBER(path, float3, denoising_feature_throughput, KERNEL_FEATURE_DENOISING)
/* Shader sorting. */
/* TODO: compress as uint16? or leave out entirely and recompute key in sorting code? */
KERNEL_STRUCT_MEMBER(path, uint32_t, shader_sort_key, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_END(path)

/************************************** Ray ***********************************/

KERNEL_STRUCT_BEGIN(ray)
KERNEL_STRUCT_MEMBER(ray, float3, P, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(ray, float3, D, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(ray, float, t, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(ray, float, time, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(ray, float, dP, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(ray, float, dD, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_END(ray)

/*************************** Intersection result ******************************/

/* Result from scene intersection. */
KERNEL_STRUCT_BEGIN(isect)
KERNEL_STRUCT_MEMBER(isect, float, t, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(isect, float, u, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(isect, float, v, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(isect, int, prim, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(isect, int, object, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(isect, int, type, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_END(isect)

/*************** Subsurface closure state for subsurface kernel ***************/

KERNEL_STRUCT_BEGIN(subsurface)
KERNEL_STRUCT_MEMBER(subsurface, float3, albedo, KERNEL_FEATURE_SUBSURFACE)
KERNEL_STRUCT_MEMBER(subsurface, float3, radius, KERNEL_FEATURE_SUBSURFACE)
KERNEL_STRUCT_MEMBER(subsurface, float, anisotropy, KERNEL_FEATURE_SUBSURFACE)
KERNEL_STRUCT_MEMBER(subsurface, float3, Ng, KERNEL_FEATURE_SUBSURFACE)
KERNEL_STRUCT_END(subsurface)

/********************************** Volume Stack ******************************/

KERNEL_STRUCT_BEGIN(volume_stack)
KERNEL_STRUCT_ARRAY_MEMBER(volume_stack, int, object, KERNEL_FEATURE_VOLUME)
KERNEL_STRUCT_ARRAY_MEMBER(volume_stack, int, shader, KERNEL_FEATURE_VOLUME)
KERNEL_STRUCT_END_ARRAY(volume_stack,
                        KERNEL_STRUCT_VOLUME_STACK_SIZE,
                        KERNEL_STRUCT_VOLUME_STACK_SIZE)
