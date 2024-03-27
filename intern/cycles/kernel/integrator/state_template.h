/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/************************************ Path State *****************************/

KERNEL_STRUCT_BEGIN(path)
/* Index of a pixel within the device render buffer where this path will write its result.
 * To get an actual offset within the buffer the value needs to be multiplied by the
 * `kernel_data.film.pass_stride`.
 *
 * The multiplication is delayed for later, so that state can use 32bit integer. */
KERNEL_STRUCT_MEMBER(path, uint32_t, render_pixel_index, KERNEL_FEATURE_PATH_TRACING)
/* Current sample number. */
KERNEL_STRUCT_MEMBER(path, uint32_t, sample, KERNEL_FEATURE_PATH_TRACING)
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
/* enum PathRayMNEE */
KERNEL_STRUCT_MEMBER(path, uint8_t, mnee, KERNEL_FEATURE_PATH_TRACING)
/* Multiple importance sampling
 * The PDF of BSDF sampling at the last scatter point, which is at ray distance
 * zero and distance. Note that transparency and volume attenuation increase
 * the ray tmin but keep P unmodified so that this works. */
KERNEL_STRUCT_MEMBER(path, float, mis_ray_pdf, KERNEL_FEATURE_PATH_TRACING)
/* Object at last scatter point for light linking. */
KERNEL_STRUCT_MEMBER(path, int, mis_ray_object, KERNEL_FEATURE_LIGHT_LINKING)
/* Normal at last scatter point for light tree. */
KERNEL_STRUCT_MEMBER(path, packed_float3, mis_origin_n, KERNEL_FEATURE_PATH_TRACING)
/* Filter glossy. */
KERNEL_STRUCT_MEMBER(path, float, min_ray_pdf, KERNEL_FEATURE_PATH_TRACING)
/* Continuation probability for path termination. */
KERNEL_STRUCT_MEMBER(path, float, continuation_probability, KERNEL_FEATURE_PATH_TRACING)
/* Throughput. */
KERNEL_STRUCT_MEMBER(path, PackedSpectrum, throughput, KERNEL_FEATURE_PATH_TRACING)
/* Factor to multiple with throughput to get remove any guiding PDFS.
 * Such throughput without guiding PDFS is used for Russian roulette termination. */
KERNEL_STRUCT_MEMBER(path, float, unguided_throughput, KERNEL_FEATURE_PATH_GUIDING)
/* Ratio of throughput to distinguish diffuse / glossy / transmission render passes. */
KERNEL_STRUCT_MEMBER(path, PackedSpectrum, pass_diffuse_weight, KERNEL_FEATURE_LIGHT_PASSES)
KERNEL_STRUCT_MEMBER(path, PackedSpectrum, pass_glossy_weight, KERNEL_FEATURE_LIGHT_PASSES)
/* Denoising. */
KERNEL_STRUCT_MEMBER(path, PackedSpectrum, denoising_feature_throughput, KERNEL_FEATURE_DENOISING)
/* Shader sorting. */
/* TODO: compress as uint16? or leave out entirely and recompute key in sorting code? */
KERNEL_STRUCT_MEMBER(path, uint32_t, shader_sort_key, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_END(path)

/************************************** Ray ***********************************/

KERNEL_STRUCT_BEGIN(ray)
KERNEL_STRUCT_MEMBER(ray, packed_float3, P, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(ray, packed_float3, D, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(ray, float, tmin, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(ray, float, tmax, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(ray, float, time, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(ray, float, dP, KERNEL_FEATURE_PATH_TRACING)
KERNEL_STRUCT_MEMBER(ray, float, dD, KERNEL_FEATURE_PATH_TRACING)
#ifdef __LIGHT_TREE__
KERNEL_STRUCT_MEMBER(ray, float, previous_dt, KERNEL_FEATURE_PATH_TRACING)
#endif
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
KERNEL_STRUCT_MEMBER(subsurface, PackedSpectrum, albedo, KERNEL_FEATURE_SUBSURFACE)
KERNEL_STRUCT_MEMBER(subsurface, PackedSpectrum, radius, KERNEL_FEATURE_SUBSURFACE)
KERNEL_STRUCT_MEMBER(subsurface, float, anisotropy, KERNEL_FEATURE_SUBSURFACE)
KERNEL_STRUCT_MEMBER(subsurface, packed_float3, N, KERNEL_FEATURE_SUBSURFACE)
KERNEL_STRUCT_END(subsurface)

/********************************** Volume Stack ******************************/

KERNEL_STRUCT_BEGIN(volume_stack)
KERNEL_STRUCT_ARRAY_MEMBER(volume_stack, int, object, KERNEL_FEATURE_VOLUME)
KERNEL_STRUCT_ARRAY_MEMBER(volume_stack, int, shader, KERNEL_FEATURE_VOLUME)
KERNEL_STRUCT_END_ARRAY(volume_stack,
                        KERNEL_STRUCT_VOLUME_STACK_SIZE,
                        KERNEL_STRUCT_VOLUME_STACK_SIZE)

/************************************ Path Guiding *****************************/
KERNEL_STRUCT_BEGIN(guiding)
#ifdef __PATH_GUIDING__
/* Current path segment of the random walk/path. */
KERNEL_STRUCT_MEMBER(guiding,
                     openpgl::cpp::PathSegment *,
                     path_segment,
                     KERNEL_FEATURE_PATH_GUIDING)
#else
/* Current path segment of the random walk/path. */
KERNEL_STRUCT_MEMBER(guiding, uint64_t, path_segment, KERNEL_FEATURE_PATH_GUIDING)
#endif
/* If surface guiding is enabled */
KERNEL_STRUCT_MEMBER(guiding, bool, use_surface_guiding, KERNEL_FEATURE_PATH_GUIDING)
/* Random number used for additional guiding decisions (e.g., cache query, selection to use guiding
 * or BSDF sampling) */
KERNEL_STRUCT_MEMBER(guiding, float, sample_surface_guiding_rand, KERNEL_FEATURE_PATH_GUIDING)
/* The probability to use surface guiding (i.e., diffuse sampling prob * guiding prob)*/
KERNEL_STRUCT_MEMBER(guiding, float, surface_guiding_sampling_prob, KERNEL_FEATURE_PATH_GUIDING)
/* Probability of sampling a BSSRDF closure instead of a BSDF closure. */
KERNEL_STRUCT_MEMBER(guiding, float, bssrdf_sampling_prob, KERNEL_FEATURE_PATH_GUIDING)
/* If volume guiding is enabled */
KERNEL_STRUCT_MEMBER(guiding, bool, use_volume_guiding, KERNEL_FEATURE_PATH_GUIDING)
/* Random number used for additional guiding decisions (e.g., cache query, selection to use guiding
 * or BSDF sampling) */
KERNEL_STRUCT_MEMBER(guiding, float, sample_volume_guiding_rand, KERNEL_FEATURE_PATH_GUIDING)
/* The probability to use surface guiding (i.e., diffuse sampling prob * guiding prob). */
KERNEL_STRUCT_MEMBER(guiding, float, volume_guiding_sampling_prob, KERNEL_FEATURE_PATH_GUIDING)
KERNEL_STRUCT_END(guiding)

/******************************* Shadow linking *******************************/

KERNEL_STRUCT_BEGIN(shadow_link)
KERNEL_STRUCT_MEMBER(shadow_link, float, dedicated_light_weight, KERNEL_FEATURE_SHADOW_LINKING)
/* Copy of primitive and object from the last main path intersection. */
KERNEL_STRUCT_MEMBER(shadow_link, int, last_isect_prim, KERNEL_FEATURE_SHADOW_LINKING)
KERNEL_STRUCT_MEMBER(shadow_link, int, last_isect_object, KERNEL_FEATURE_SHADOW_LINKING)
KERNEL_STRUCT_END(shadow_link)
