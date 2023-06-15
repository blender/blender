/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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
KERNEL_STRUCT_MEMBER(shadow_path, PackedSpectrum, throughput, KERNEL_FEATURE_PATH_TRACING)
/* Throughput for shadow pass. */
KERNEL_STRUCT_MEMBER(shadow_path,
                     PackedSpectrum,
                     unshadowed_throughput,
                     KERNEL_FEATURE_AO_ADDITIVE)
/* Ratio of throughput to distinguish diffuse / glossy / transmission render passes. */
KERNEL_STRUCT_MEMBER(shadow_path, PackedSpectrum, pass_diffuse_weight, KERNEL_FEATURE_LIGHT_PASSES)
KERNEL_STRUCT_MEMBER(shadow_path, PackedSpectrum, pass_glossy_weight, KERNEL_FEATURE_LIGHT_PASSES)
/* Number of intersections found by ray-tracing. */
KERNEL_STRUCT_MEMBER(shadow_path, uint16_t, num_hits, KERNEL_FEATURE_PATH_TRACING)
/* Light group. */
KERNEL_STRUCT_MEMBER(shadow_path, uint8_t, lightgroup, KERNEL_FEATURE_PATH_TRACING)
/* Path guiding. */
KERNEL_STRUCT_MEMBER(shadow_path, PackedSpectrum, unlit_throughput, KERNEL_FEATURE_PATH_GUIDING)
#ifdef __PATH_GUIDING__
KERNEL_STRUCT_MEMBER(shadow_path,
                     openpgl::cpp::PathSegment *,
                     path_segment,
                     KERNEL_FEATURE_PATH_GUIDING)
#else
KERNEL_STRUCT_MEMBER(shadow_path, uint64_t, path_segment, KERNEL_FEATURE_PATH_GUIDING)
#endif
KERNEL_STRUCT_MEMBER(shadow_path, float, guiding_mis_weight, KERNEL_FEATURE_PATH_GUIDING)
KERNEL_STRUCT_END(shadow_path)

/********************************** Shadow Ray *******************************/

KERNEL_STRUCT_BEGIN(shadow_ray)
KERNEL_STRUCT_MEMBER(shadow_ray, packed_float3, P, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(shadow_ray, packed_float3, D, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(shadow_ray, float, tmin, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(shadow_ray, float, tmax, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(shadow_ray, float, time, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(shadow_ray, float, dP, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(shadow_ray, int, self_light, KERNEL_FEATURE_SHADOW_LINKING)
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
