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

#ifndef __KERNEL_TYPES_H__
#define __KERNEL_TYPES_H__

#if !defined(__KERNEL_GPU__) && defined(WITH_EMBREE)
#  include <embree3/rtcore.h>
#  include <embree3/rtcore_scene.h>
#  define __EMBREE__
#endif

#include "kernel/kernel_math.h"
#include "kernel/svm/svm_types.h"
#include "util/util_static_assert.h"

#ifndef __KERNEL_GPU__
#  define __KERNEL_CPU__
#endif

/* TODO(sergey): This is only to make it possible to include this header
 * from outside of the kernel. but this could be done somewhat cleaner?
 */
#ifndef ccl_addr_space
#  define ccl_addr_space
#endif

CCL_NAMESPACE_BEGIN

/* Constants */
#define OBJECT_MOTION_PASS_SIZE 2
#define FILTER_TABLE_SIZE 1024
#define RAMP_TABLE_SIZE 256
#define SHUTTER_TABLE_SIZE 256

#define BSSRDF_MIN_RADIUS 1e-8f
#define BSSRDF_MAX_HITS 4
#define BSSRDF_MAX_BOUNCES 256
#define LOCAL_MAX_HITS 4

#define VOLUME_BOUNDS_MAX 1024

#define BECKMANN_TABLE_SIZE 256

#define SHADER_NONE (~0)
#define OBJECT_NONE (~0)
#define PRIM_NONE (~0)
#define LAMP_NONE (~0)
#define ID_NONE (0.0f)

#define VOLUME_STACK_SIZE 32

/* Split kernel constants */
#define WORK_POOL_SIZE_GPU 64
#define WORK_POOL_SIZE_CPU 1
#ifdef __KERNEL_GPU__
#  define WORK_POOL_SIZE WORK_POOL_SIZE_GPU
#else
#  define WORK_POOL_SIZE WORK_POOL_SIZE_CPU
#endif

#define SHADER_SORT_BLOCK_SIZE 2048

#ifdef __KERNEL_OPENCL__
#  define SHADER_SORT_LOCAL_SIZE 64
#elif defined(__KERNEL_CUDA__)
#  define SHADER_SORT_LOCAL_SIZE 32
#else
#  define SHADER_SORT_LOCAL_SIZE 1
#endif

/* Kernel features */
#define __SOBOL__
#define __INSTANCING__
#define __DPDU__
#define __UV__
#define __BACKGROUND__
#define __CAUSTICS_TRICKS__
#define __VISIBILITY_FLAG__
#define __RAY_DIFFERENTIALS__
#define __CAMERA_CLIPPING__
#define __INTERSECTION_REFINE__
#define __CLAMP_SAMPLE__
#define __PATCH_EVAL__
#define __SHADOW_TRICKS__
#define __DENOISING_FEATURES__
#define __SHADER_RAYTRACE__
#define __AO__
#define __PASSES__
#define __HAIR__

/* Without these we get an AO render, used by OpenCL preview kernel. */
#ifndef __KERNEL_AO_PREVIEW__
#  define __SVM__
#  define __EMISSION__
#  define __HOLDOUT__
#  define __MULTI_CLOSURE__
#  define __TRANSPARENT_SHADOWS__
#  define __BACKGROUND_MIS__
#  define __LAMP_MIS__
#  define __CAMERA_MOTION__
#  define __OBJECT_MOTION__
#  define __BAKING__
#  define __PRINCIPLED__
#  define __SUBSURFACE__
#  define __VOLUME__
#  define __VOLUME_SCATTER__
#  define __CMJ__
#  define __SHADOW_RECORD_ALL__
#  define __BRANCHED_PATH__
#endif

/* Device specific features */
#ifdef __KERNEL_CPU__
#  ifdef __KERNEL_SSE2__
#    define __QBVH__
#  endif
#  ifdef WITH_OSL
#    define __OSL__
#  endif
#  define __VOLUME_DECOUPLED__
#  define __VOLUME_RECORD_ALL__
#endif /* __KERNEL_CPU__ */

#ifdef __KERNEL_CUDA__
#  ifdef __SPLIT_KERNEL__
#    undef __BRANCHED_PATH__
#  endif
#endif /* __KERNEL_CUDA__ */

#ifdef __KERNEL_OPTIX__
#  undef __BAKING__
#  undef __BRANCHED_PATH__
/* TODO(pmours): Cannot use optixTrace in non-inlined functions */
#  undef __SHADER_RAYTRACE__
#endif /* __KERNEL_OPTIX__ */

#ifdef __KERNEL_OPENCL__
#endif /* __KERNEL_OPENCL__ */

/* Scene-based selective features compilation. */
#ifdef __NO_CAMERA_MOTION__
#  undef __CAMERA_MOTION__
#endif
#ifdef __NO_OBJECT_MOTION__
#  undef __OBJECT_MOTION__
#endif
#ifdef __NO_HAIR__
#  undef __HAIR__
#endif
#ifdef __NO_VOLUME__
#  undef __VOLUME__
#  undef __VOLUME_SCATTER__
#endif
#ifdef __NO_SUBSURFACE__
#  undef __SUBSURFACE__
#endif
#ifdef __NO_BAKING__
#  undef __BAKING__
#endif
#ifdef __NO_BRANCHED_PATH__
#  undef __BRANCHED_PATH__
#endif
#ifdef __NO_PATCH_EVAL__
#  undef __PATCH_EVAL__
#endif
#ifdef __NO_TRANSPARENT__
#  undef __TRANSPARENT_SHADOWS__
#endif
#ifdef __NO_SHADOW_TRICKS__
#  undef __SHADOW_TRICKS__
#endif
#ifdef __NO_PRINCIPLED__
#  undef __PRINCIPLED__
#endif
#ifdef __NO_DENOISING__
#  undef __DENOISING_FEATURES__
#endif
#ifdef __NO_SHADER_RAYTRACE__
#  undef __SHADER_RAYTRACE__
#endif

/* Features that enable others */
#ifdef WITH_CYCLES_DEBUG
#  define __KERNEL_DEBUG__
#endif

#if defined(__SUBSURFACE__) || defined(__SHADER_RAYTRACE__)
#  define __BVH_LOCAL__
#endif

/* Shader Evaluation */

typedef enum ShaderEvalType {
  SHADER_EVAL_DISPLACE,
  SHADER_EVAL_BACKGROUND,
  /* bake types */
  SHADER_EVAL_BAKE, /* no real shade, it's used in the code to
                     * differentiate the type of shader eval from the above
                     */
  /* data passes */
  SHADER_EVAL_NORMAL,
  SHADER_EVAL_UV,
  SHADER_EVAL_ROUGHNESS,
  SHADER_EVAL_DIFFUSE_COLOR,
  SHADER_EVAL_GLOSSY_COLOR,
  SHADER_EVAL_TRANSMISSION_COLOR,
  SHADER_EVAL_EMISSION,
  SHADER_EVAL_AOV_COLOR,
  SHADER_EVAL_AOV_VALUE,

  /* light passes */
  SHADER_EVAL_AO,
  SHADER_EVAL_COMBINED,
  SHADER_EVAL_SHADOW,
  SHADER_EVAL_DIFFUSE,
  SHADER_EVAL_GLOSSY,
  SHADER_EVAL_TRANSMISSION,

  /* extra */
  SHADER_EVAL_ENVIRONMENT,
} ShaderEvalType;

/* Path Tracing
 * note we need to keep the u/v pairs at even values */

enum PathTraceDimension {
  PRNG_FILTER_U = 0,
  PRNG_FILTER_V = 1,
  PRNG_LENS_U = 2,
  PRNG_LENS_V = 3,
  PRNG_TIME = 4,
  PRNG_UNUSED_0 = 5,
  PRNG_UNUSED_1 = 6, /* for some reason (6, 7) is a bad sobol pattern */
  PRNG_UNUSED_2 = 7, /* with a low number of samples (< 64) */
  PRNG_BASE_NUM = 10,

  PRNG_BSDF_U = 0,
  PRNG_BSDF_V = 1,
  PRNG_LIGHT_U = 2,
  PRNG_LIGHT_V = 3,
  PRNG_LIGHT_TERMINATE = 4,
  PRNG_TERMINATE = 5,
  PRNG_PHASE_CHANNEL = 6,
  PRNG_SCATTER_DISTANCE = 7,
  PRNG_BOUNCE_NUM = 8,

  PRNG_BEVEL_U = 6, /* reuse volume dimension, correlation won't harm */
  PRNG_BEVEL_V = 7,
};

enum SamplingPattern {
  SAMPLING_PATTERN_SOBOL = 0,
  SAMPLING_PATTERN_CMJ = 1,
  SAMPLING_PATTERN_PMJ = 2,

  SAMPLING_NUM_PATTERNS,
};

/* these flags values correspond to raytypes in osl.cpp, so keep them in sync! */

enum PathRayFlag {
  /* Ray visibility. */
  PATH_RAY_CAMERA = (1 << 0),
  PATH_RAY_REFLECT = (1 << 1),
  PATH_RAY_TRANSMIT = (1 << 2),
  PATH_RAY_DIFFUSE = (1 << 3),
  PATH_RAY_GLOSSY = (1 << 4),
  PATH_RAY_SINGULAR = (1 << 5),
  PATH_RAY_TRANSPARENT = (1 << 6),

  /* Shadow ray visibility. */
  PATH_RAY_SHADOW_OPAQUE_NON_CATCHER = (1 << 7),
  PATH_RAY_SHADOW_OPAQUE_CATCHER = (1 << 8),
  PATH_RAY_SHADOW_OPAQUE = (PATH_RAY_SHADOW_OPAQUE_NON_CATCHER | PATH_RAY_SHADOW_OPAQUE_CATCHER),
  PATH_RAY_SHADOW_TRANSPARENT_NON_CATCHER = (1 << 9),
  PATH_RAY_SHADOW_TRANSPARENT_CATCHER = (1 << 10),
  PATH_RAY_SHADOW_TRANSPARENT = (PATH_RAY_SHADOW_TRANSPARENT_NON_CATCHER |
                                 PATH_RAY_SHADOW_TRANSPARENT_CATCHER),
  PATH_RAY_SHADOW_NON_CATCHER = (PATH_RAY_SHADOW_OPAQUE_NON_CATCHER |
                                 PATH_RAY_SHADOW_TRANSPARENT_NON_CATCHER),
  PATH_RAY_SHADOW = (PATH_RAY_SHADOW_OPAQUE | PATH_RAY_SHADOW_TRANSPARENT),

  /* Unused, free to reuse. */
  PATH_RAY_UNUSED = (1 << 11),

  /* Ray visibility for volume scattering. */
  PATH_RAY_VOLUME_SCATTER = (1 << 12),

  /* Special flag to tag unaligned BVH nodes. */
  PATH_RAY_NODE_UNALIGNED = (1 << 13),

  PATH_RAY_ALL_VISIBILITY = ((1 << 14) - 1),

  /* Don't apply multiple importance sampling weights to emission from
   * lamp or surface hits, because they were not direct light sampled. */
  PATH_RAY_MIS_SKIP = (1 << 14),
  /* Diffuse bounce earlier in the path, skip SSS to improve performance
   * and avoid branching twice with disk sampling SSS. */
  PATH_RAY_DIFFUSE_ANCESTOR = (1 << 15),
  /* Single pass has been written. */
  PATH_RAY_SINGLE_PASS_DONE = (1 << 16),
  /* Ray is behind a shadow catcher .*/
  PATH_RAY_SHADOW_CATCHER = (1 << 17),
  /* Store shadow data for shadow catcher or denoising. */
  PATH_RAY_STORE_SHADOW_INFO = (1 << 18),
  /* Zero background alpha, for camera or transparent glass rays. */
  PATH_RAY_TRANSPARENT_BACKGROUND = (1 << 19),
  /* Terminate ray immediately at next bounce. */
  PATH_RAY_TERMINATE_IMMEDIATE = (1 << 20),
  /* Ray is to be terminated, but continue with transparent bounces and
   * emission as long as we encounter them. This is required to make the
   * MIS between direct and indirect light rays match, as shadow rays go
   * through transparent surfaces to reach emission too. */
  PATH_RAY_TERMINATE_AFTER_TRANSPARENT = (1 << 21),
  /* Ray is to be terminated. */
  PATH_RAY_TERMINATE = (PATH_RAY_TERMINATE_IMMEDIATE | PATH_RAY_TERMINATE_AFTER_TRANSPARENT),
  /* Path and shader is being evaluated for direct lighting emission. */
  PATH_RAY_EMISSION = (1 << 22)
};

/* Closure Label */

typedef enum ClosureLabel {
  LABEL_NONE = 0,
  LABEL_TRANSMIT = 1,
  LABEL_REFLECT = 2,
  LABEL_DIFFUSE = 4,
  LABEL_GLOSSY = 8,
  LABEL_SINGULAR = 16,
  LABEL_TRANSPARENT = 32,
  LABEL_VOLUME_SCATTER = 64,
  LABEL_TRANSMIT_TRANSPARENT = 128,
} ClosureLabel;

/* Render Passes */

#define PASS_NAME_JOIN(a, b) a##_##b
#define PASSMASK(pass) (1 << ((PASS_NAME_JOIN(PASS, pass)) % 32))

#define PASSMASK_COMPONENT(comp) \
  (PASSMASK(PASS_NAME_JOIN(comp, DIRECT)) | PASSMASK(PASS_NAME_JOIN(comp, INDIRECT)) | \
   PASSMASK(PASS_NAME_JOIN(comp, COLOR)))

typedef enum PassType {
  PASS_NONE = 0,

  /* Main passes */
  PASS_COMBINED = 1,
  PASS_DEPTH,
  PASS_NORMAL,
  PASS_UV,
  PASS_OBJECT_ID,
  PASS_MATERIAL_ID,
  PASS_MOTION,
  PASS_MOTION_WEIGHT,
#ifdef __KERNEL_DEBUG__
  PASS_BVH_TRAVERSED_NODES,
  PASS_BVH_TRAVERSED_INSTANCES,
  PASS_BVH_INTERSECTIONS,
  PASS_RAY_BOUNCES,
#endif
  PASS_RENDER_TIME,
  PASS_CRYPTOMATTE,
  PASS_AOV_COLOR,
  PASS_AOV_VALUE,
  PASS_ADAPTIVE_AUX_BUFFER,
  PASS_SAMPLE_COUNT,
  PASS_CATEGORY_MAIN_END = 31,

  PASS_MIST = 32,
  PASS_EMISSION,
  PASS_BACKGROUND,
  PASS_AO,
  PASS_SHADOW,
  PASS_LIGHT, /* no real pass, used to force use_light_pass */
  PASS_DIFFUSE_DIRECT,
  PASS_DIFFUSE_INDIRECT,
  PASS_DIFFUSE_COLOR,
  PASS_GLOSSY_DIRECT,
  PASS_GLOSSY_INDIRECT,
  PASS_GLOSSY_COLOR,
  PASS_TRANSMISSION_DIRECT,
  PASS_TRANSMISSION_INDIRECT,
  PASS_TRANSMISSION_COLOR,
  PASS_VOLUME_DIRECT = 50,
  PASS_VOLUME_INDIRECT,
  /* No Scatter color since it's tricky to define what it would even mean. */
  PASS_CATEGORY_LIGHT_END = 63,

  PASS_BAKE_PRIMITIVE,
  PASS_BAKE_DIFFERENTIAL,
  PASS_CATEGORY_BAKE_END = 95
} PassType;

#define PASS_ANY (~0)

typedef enum CryptomatteType {
  CRYPT_NONE = 0,
  CRYPT_OBJECT = (1 << 0),
  CRYPT_MATERIAL = (1 << 1),
  CRYPT_ASSET = (1 << 2),
  CRYPT_ACCURATE = (1 << 3),
} CryptomatteType;

typedef enum DenoisingPassOffsets {
  DENOISING_PASS_NORMAL = 0,
  DENOISING_PASS_NORMAL_VAR = 3,
  DENOISING_PASS_ALBEDO = 6,
  DENOISING_PASS_ALBEDO_VAR = 9,
  DENOISING_PASS_DEPTH = 12,
  DENOISING_PASS_DEPTH_VAR = 13,
  DENOISING_PASS_SHADOW_A = 14,
  DENOISING_PASS_SHADOW_B = 17,
  DENOISING_PASS_COLOR = 20,
  DENOISING_PASS_COLOR_VAR = 23,
  DENOISING_PASS_CLEAN = 26,

  DENOISING_PASS_PREFILTERED_DEPTH = 0,
  DENOISING_PASS_PREFILTERED_NORMAL = 1,
  DENOISING_PASS_PREFILTERED_SHADOWING = 4,
  DENOISING_PASS_PREFILTERED_ALBEDO = 5,
  DENOISING_PASS_PREFILTERED_COLOR = 8,
  DENOISING_PASS_PREFILTERED_VARIANCE = 11,
  DENOISING_PASS_PREFILTERED_INTENSITY = 14,

  DENOISING_PASS_SIZE_BASE = 26,
  DENOISING_PASS_SIZE_CLEAN = 3,
  DENOISING_PASS_SIZE_PREFILTERED = 15,
} DenoisingPassOffsets;

typedef enum eBakePassFilter {
  BAKE_FILTER_NONE = 0,
  BAKE_FILTER_DIRECT = (1 << 0),
  BAKE_FILTER_INDIRECT = (1 << 1),
  BAKE_FILTER_COLOR = (1 << 2),
  BAKE_FILTER_DIFFUSE = (1 << 3),
  BAKE_FILTER_GLOSSY = (1 << 4),
  BAKE_FILTER_TRANSMISSION = (1 << 5),
  BAKE_FILTER_EMISSION = (1 << 6),
  BAKE_FILTER_AO = (1 << 7),
} eBakePassFilter;

typedef enum BakePassFilterCombos {
  BAKE_FILTER_COMBINED = (BAKE_FILTER_DIRECT | BAKE_FILTER_INDIRECT | BAKE_FILTER_DIFFUSE |
                          BAKE_FILTER_GLOSSY | BAKE_FILTER_TRANSMISSION | BAKE_FILTER_EMISSION |
                          BAKE_FILTER_AO),
  BAKE_FILTER_DIFFUSE_DIRECT = (BAKE_FILTER_DIRECT | BAKE_FILTER_DIFFUSE),
  BAKE_FILTER_GLOSSY_DIRECT = (BAKE_FILTER_DIRECT | BAKE_FILTER_GLOSSY),
  BAKE_FILTER_TRANSMISSION_DIRECT = (BAKE_FILTER_DIRECT | BAKE_FILTER_TRANSMISSION),
  BAKE_FILTER_DIFFUSE_INDIRECT = (BAKE_FILTER_INDIRECT | BAKE_FILTER_DIFFUSE),
  BAKE_FILTER_GLOSSY_INDIRECT = (BAKE_FILTER_INDIRECT | BAKE_FILTER_GLOSSY),
  BAKE_FILTER_TRANSMISSION_INDIRECT = (BAKE_FILTER_INDIRECT | BAKE_FILTER_TRANSMISSION),
} BakePassFilterCombos;

typedef enum DenoiseFlag {
  DENOISING_CLEAN_DIFFUSE_DIR = (1 << 0),
  DENOISING_CLEAN_DIFFUSE_IND = (1 << 1),
  DENOISING_CLEAN_GLOSSY_DIR = (1 << 2),
  DENOISING_CLEAN_GLOSSY_IND = (1 << 3),
  DENOISING_CLEAN_TRANSMISSION_DIR = (1 << 4),
  DENOISING_CLEAN_TRANSMISSION_IND = (1 << 5),
  DENOISING_CLEAN_ALL_PASSES = (1 << 6) - 1,
} DenoiseFlag;

#ifdef __KERNEL_DEBUG__
/* NOTE: This is a runtime-only struct, alignment is not
 * really important here.
 */
typedef struct DebugData {
  int num_bvh_traversed_nodes;
  int num_bvh_traversed_instances;
  int num_bvh_intersections;
  int num_ray_bounces;
} DebugData;
#endif

typedef ccl_addr_space struct PathRadianceState {
#ifdef __PASSES__
  float3 diffuse;
  float3 glossy;
  float3 transmission;
  float3 volume;

  float3 direct;
#endif
} PathRadianceState;

typedef ccl_addr_space struct PathRadiance {
#ifdef __PASSES__
  int use_light_pass;
#endif

  float transparent;
  float3 emission;
#ifdef __PASSES__
  float3 background;
  float3 ao;

  float3 indirect;
  float3 direct_emission;

  float3 color_diffuse;
  float3 color_glossy;
  float3 color_transmission;

  float3 direct_diffuse;
  float3 direct_glossy;
  float3 direct_transmission;
  float3 direct_volume;

  float3 indirect_diffuse;
  float3 indirect_glossy;
  float3 indirect_transmission;
  float3 indirect_volume;

  float4 shadow;
  float mist;
#endif

  struct PathRadianceState state;

#ifdef __SHADOW_TRICKS__
  /* Total light reachable across the path, ignoring shadow blocked queries. */
  float3 path_total;
  /* Total light reachable across the path with shadow blocked queries
   * applied here.
   *
   * Dividing this figure by path_total will give estimate of shadow pass.
   */
  float3 path_total_shaded;

  /* Color of the background on which shadow is alpha-overed. */
  float3 shadow_background_color;

  /* Path radiance sum and throughput at the moment when ray hits shadow
   * catcher object.
   */
  float shadow_throughput;

  /* Accumulated transparency along the path after shadow catcher bounce. */
  float shadow_transparency;

  /* Indicate if any shadow catcher data is set. */
  int has_shadow_catcher;
#endif

#ifdef __DENOISING_FEATURES__
  float3 denoising_normal;
  float3 denoising_albedo;
  float denoising_depth;
#endif /* __DENOISING_FEATURES__ */

#ifdef __KERNEL_DEBUG__
  DebugData debug_data;
#endif /* __KERNEL_DEBUG__ */
} PathRadiance;

typedef struct BsdfEval {
#ifdef __PASSES__
  int use_light_pass;
#endif

  float3 diffuse;
#ifdef __PASSES__
  float3 glossy;
  float3 transmission;
  float3 transparent;
  float3 volume;
#endif
#ifdef __SHADOW_TRICKS__
  float3 sum_no_mis;
#endif
} BsdfEval;

/* Shader Flag */

typedef enum ShaderFlag {
  SHADER_SMOOTH_NORMAL = (1 << 31),
  SHADER_CAST_SHADOW = (1 << 30),
  SHADER_AREA_LIGHT = (1 << 29),
  SHADER_USE_MIS = (1 << 28),
  SHADER_EXCLUDE_DIFFUSE = (1 << 27),
  SHADER_EXCLUDE_GLOSSY = (1 << 26),
  SHADER_EXCLUDE_TRANSMIT = (1 << 25),
  SHADER_EXCLUDE_CAMERA = (1 << 24),
  SHADER_EXCLUDE_SCATTER = (1 << 23),
  SHADER_EXCLUDE_ANY = (SHADER_EXCLUDE_DIFFUSE | SHADER_EXCLUDE_GLOSSY | SHADER_EXCLUDE_TRANSMIT |
                        SHADER_EXCLUDE_CAMERA | SHADER_EXCLUDE_SCATTER),

  SHADER_MASK = ~(SHADER_SMOOTH_NORMAL | SHADER_CAST_SHADOW | SHADER_AREA_LIGHT | SHADER_USE_MIS |
                  SHADER_EXCLUDE_ANY)
} ShaderFlag;

/* Light Type */

typedef enum LightType {
  LIGHT_POINT,
  LIGHT_DISTANT,
  LIGHT_BACKGROUND,
  LIGHT_AREA,
  LIGHT_SPOT,
  LIGHT_TRIANGLE
} LightType;

/* Camera Type */

enum CameraType { CAMERA_PERSPECTIVE, CAMERA_ORTHOGRAPHIC, CAMERA_PANORAMA };

/* Panorama Type */

enum PanoramaType {
  PANORAMA_EQUIRECTANGULAR = 0,
  PANORAMA_FISHEYE_EQUIDISTANT = 1,
  PANORAMA_FISHEYE_EQUISOLID = 2,
  PANORAMA_MIRRORBALL = 3,

  PANORAMA_NUM_TYPES,
};

/* Differential */

typedef struct differential3 {
  float3 dx;
  float3 dy;
} differential3;

typedef struct differential {
  float dx;
  float dy;
} differential;

/* Ray */

typedef struct Ray {
/* TODO(sergey): This is only needed because current AMD
 * compiler has hard time building the kernel with this
 * reshuffle. And at the same time reshuffle will cause
 * less optimal CPU code in certain places.
 *
 * We'll get rid of this nasty exception once AMD compiler
 * is fixed.
 */
#ifndef __KERNEL_OPENCL_AMD__
  float3 P;   /* origin */
  float3 D;   /* direction */
  float t;    /* length of the ray */
  float time; /* time (for motion blur) */
#else
  float t;    /* length of the ray */
  float time; /* time (for motion blur) */
  float3 P;   /* origin */
  float3 D;   /* direction */
#endif

#ifdef __RAY_DIFFERENTIALS__
  differential3 dP;
  differential3 dD;
#endif
} Ray;

/* Intersection */

typedef struct Intersection {
#ifdef __EMBREE__
  float3 Ng;
#endif
  float t, u, v;
  int prim;
  int object;
  int type;

#ifdef __KERNEL_DEBUG__
  int num_traversed_nodes;
  int num_traversed_instances;
  int num_intersections;
#endif
} Intersection;

/* Primitives */

typedef enum PrimitiveType {
  PRIMITIVE_NONE = 0,
  PRIMITIVE_TRIANGLE = (1 << 0),
  PRIMITIVE_MOTION_TRIANGLE = (1 << 1),
  PRIMITIVE_CURVE = (1 << 2),
  PRIMITIVE_MOTION_CURVE = (1 << 3),
  /* Lamp primitive is not included below on purpose,
   * since it is no real traceable primitive.
   */
  PRIMITIVE_LAMP = (1 << 4),

  PRIMITIVE_ALL_TRIANGLE = (PRIMITIVE_TRIANGLE | PRIMITIVE_MOTION_TRIANGLE),
  PRIMITIVE_ALL_CURVE = (PRIMITIVE_CURVE | PRIMITIVE_MOTION_CURVE),
  PRIMITIVE_ALL_MOTION = (PRIMITIVE_MOTION_TRIANGLE | PRIMITIVE_MOTION_CURVE),
  PRIMITIVE_ALL = (PRIMITIVE_ALL_TRIANGLE | PRIMITIVE_ALL_CURVE),

  /* Total number of different traceable primitives.
   * NOTE: This is an actual value, not a bitflag.
   */
  PRIMITIVE_NUM_TOTAL = 4,
} PrimitiveType;

#define PRIMITIVE_PACK_SEGMENT(type, segment) ((segment << PRIMITIVE_NUM_TOTAL) | (type))
#define PRIMITIVE_UNPACK_SEGMENT(type) (type >> PRIMITIVE_NUM_TOTAL)

/* Attributes */

typedef enum AttributePrimitive {
  ATTR_PRIM_GEOMETRY = 0,
  ATTR_PRIM_SUBD,

  ATTR_PRIM_TYPES
} AttributePrimitive;

typedef enum AttributeElement {
  ATTR_ELEMENT_NONE,
  ATTR_ELEMENT_OBJECT,
  ATTR_ELEMENT_MESH,
  ATTR_ELEMENT_FACE,
  ATTR_ELEMENT_VERTEX,
  ATTR_ELEMENT_VERTEX_MOTION,
  ATTR_ELEMENT_CORNER,
  ATTR_ELEMENT_CORNER_BYTE,
  ATTR_ELEMENT_CURVE,
  ATTR_ELEMENT_CURVE_KEY,
  ATTR_ELEMENT_CURVE_KEY_MOTION,
  ATTR_ELEMENT_VOXEL
} AttributeElement;

typedef enum AttributeStandard {
  ATTR_STD_NONE = 0,
  ATTR_STD_VERTEX_NORMAL,
  ATTR_STD_FACE_NORMAL,
  ATTR_STD_UV,
  ATTR_STD_UV_TANGENT,
  ATTR_STD_UV_TANGENT_SIGN,
  ATTR_STD_VERTEX_COLOR,
  ATTR_STD_GENERATED,
  ATTR_STD_GENERATED_TRANSFORM,
  ATTR_STD_POSITION_UNDEFORMED,
  ATTR_STD_POSITION_UNDISPLACED,
  ATTR_STD_MOTION_VERTEX_POSITION,
  ATTR_STD_MOTION_VERTEX_NORMAL,
  ATTR_STD_PARTICLE,
  ATTR_STD_CURVE_INTERCEPT,
  ATTR_STD_CURVE_RANDOM,
  ATTR_STD_PTEX_FACE_ID,
  ATTR_STD_PTEX_UV,
  ATTR_STD_VOLUME_DENSITY,
  ATTR_STD_VOLUME_COLOR,
  ATTR_STD_VOLUME_FLAME,
  ATTR_STD_VOLUME_HEAT,
  ATTR_STD_VOLUME_TEMPERATURE,
  ATTR_STD_VOLUME_VELOCITY,
  ATTR_STD_POINTINESS,
  ATTR_STD_RANDOM_PER_ISLAND,
  ATTR_STD_NUM,

  ATTR_STD_NOT_FOUND = ~0
} AttributeStandard;

typedef enum AttributeFlag {
  ATTR_FINAL_SIZE = (1 << 0),
  ATTR_SUBDIVIDED = (1 << 1),
} AttributeFlag;

typedef struct AttributeDescriptor {
  AttributeElement element;
  NodeAttributeType type;
  uint flags; /* see enum AttributeFlag */
  int offset;
} AttributeDescriptor;

/* Closure data */

#ifdef __MULTI_CLOSURE__
#  ifdef __SPLIT_KERNEL__
#    define MAX_CLOSURE 1
#  else
#    ifndef __MAX_CLOSURE__
#      define MAX_CLOSURE 64
#    else
#      define MAX_CLOSURE __MAX_CLOSURE__
#    endif
#  endif
#else
#  define MAX_CLOSURE 1
#endif

/* This struct is the base class for all closures. The common members are
 * duplicated in all derived classes since we don't have C++ in the kernel
 * yet, and because it lets us lay out the members to minimize padding. The
 * weight member is located at the beginning of the struct for this reason.
 *
 * ShaderClosure has a fixed size, and any extra space must be allocated
 * with closure_alloc_extra().
 *
 * We pad the struct to align to 16 bytes. All shader closures are assumed
 * to fit in this struct size. CPU sizes are a bit larger because float3 is
 * padded to be 16 bytes, while it's only 12 bytes on the GPU. */

#define SHADER_CLOSURE_BASE \
  float3 weight; \
  ClosureType type; \
  float sample_weight; \
  float3 N

typedef ccl_addr_space struct ccl_align(16) ShaderClosure
{
  SHADER_CLOSURE_BASE;

#ifdef __KERNEL_CPU__
  float pad[2];
#endif
  float data[10];
}
ShaderClosure;

/* Shader Data
 *
 * Main shader state at a point on the surface or in a volume. All coordinates
 * are in world space.
 */

enum ShaderDataFlag {
  /* Runtime flags. */

  /* Set when ray hits backside of surface. */
  SD_BACKFACING = (1 << 0),
  /* Shader has non-zero emission. */
  SD_EMISSION = (1 << 1),
  /* Shader has BSDF closure. */
  SD_BSDF = (1 << 2),
  /* Shader has non-singular BSDF closure. */
  SD_BSDF_HAS_EVAL = (1 << 3),
  /* Shader has BSSRDF closure. */
  SD_BSSRDF = (1 << 4),
  /* Shader has holdout closure. */
  SD_HOLDOUT = (1 << 5),
  /* Shader has non-zero volume extinction. */
  SD_EXTINCTION = (1 << 6),
  /* Shader has have volume phase (scatter) closure. */
  SD_SCATTER = (1 << 7),
  /* Shader has transparent closure. */
  SD_TRANSPARENT = (1 << 9),
  /* BSDF requires LCG for evaluation. */
  SD_BSDF_NEEDS_LCG = (1 << 10),

  SD_CLOSURE_FLAGS = (SD_EMISSION | SD_BSDF | SD_BSDF_HAS_EVAL | SD_BSSRDF | SD_HOLDOUT |
                      SD_EXTINCTION | SD_SCATTER | SD_BSDF_NEEDS_LCG),

  /* Shader flags. */

  /* direct light sample */
  SD_USE_MIS = (1 << 16),
  /* Has transparent shadow. */
  SD_HAS_TRANSPARENT_SHADOW = (1 << 17),
  /* Has volume shader. */
  SD_HAS_VOLUME = (1 << 18),
  /* Has only volume shader, no surface. */
  SD_HAS_ONLY_VOLUME = (1 << 19),
  /* Has heterogeneous volume. */
  SD_HETEROGENEOUS_VOLUME = (1 << 20),
  /* BSSRDF normal uses bump. */
  SD_HAS_BSSRDF_BUMP = (1 << 21),
  /* Use equiangular volume sampling */
  SD_VOLUME_EQUIANGULAR = (1 << 22),
  /* Use multiple importance volume sampling. */
  SD_VOLUME_MIS = (1 << 23),
  /* Use cubic interpolation for voxels. */
  SD_VOLUME_CUBIC = (1 << 24),
  /* Has data connected to the displacement input or uses bump map. */
  SD_HAS_BUMP = (1 << 25),
  /* Has true displacement. */
  SD_HAS_DISPLACEMENT = (1 << 26),
  /* Has constant emission (value stored in __shaders) */
  SD_HAS_CONSTANT_EMISSION = (1 << 27),
  /* Needs to access attributes for volume rendering */
  SD_NEED_VOLUME_ATTRIBUTES = (1 << 28),

  SD_SHADER_FLAGS = (SD_USE_MIS | SD_HAS_TRANSPARENT_SHADOW | SD_HAS_VOLUME | SD_HAS_ONLY_VOLUME |
                     SD_HETEROGENEOUS_VOLUME | SD_HAS_BSSRDF_BUMP | SD_VOLUME_EQUIANGULAR |
                     SD_VOLUME_MIS | SD_VOLUME_CUBIC | SD_HAS_BUMP | SD_HAS_DISPLACEMENT |
                     SD_HAS_CONSTANT_EMISSION | SD_NEED_VOLUME_ATTRIBUTES)
};

/* Object flags. */
enum ShaderDataObjectFlag {
  /* Holdout for camera rays. */
  SD_OBJECT_HOLDOUT_MASK = (1 << 0),
  /* Has object motion blur. */
  SD_OBJECT_MOTION = (1 << 1),
  /* Vertices have transform applied. */
  SD_OBJECT_TRANSFORM_APPLIED = (1 << 2),
  /* Vertices have negative scale applied. */
  SD_OBJECT_NEGATIVE_SCALE_APPLIED = (1 << 3),
  /* Object has a volume shader. */
  SD_OBJECT_HAS_VOLUME = (1 << 4),
  /* Object intersects AABB of an object with volume shader. */
  SD_OBJECT_INTERSECTS_VOLUME = (1 << 5),
  /* Has position for motion vertices. */
  SD_OBJECT_HAS_VERTEX_MOTION = (1 << 6),
  /* object is used to catch shadows */
  SD_OBJECT_SHADOW_CATCHER = (1 << 7),
  /* object has volume attributes */
  SD_OBJECT_HAS_VOLUME_ATTRIBUTES = (1 << 8),

  SD_OBJECT_FLAGS = (SD_OBJECT_HOLDOUT_MASK | SD_OBJECT_MOTION | SD_OBJECT_TRANSFORM_APPLIED |
                     SD_OBJECT_NEGATIVE_SCALE_APPLIED | SD_OBJECT_HAS_VOLUME |
                     SD_OBJECT_INTERSECTS_VOLUME | SD_OBJECT_SHADOW_CATCHER |
                     SD_OBJECT_HAS_VOLUME_ATTRIBUTES)
};

typedef ccl_addr_space struct ccl_align(16) ShaderData
{
  /* position */
  float3 P;
  /* smooth normal for shading */
  float3 N;
  /* true geometric normal */
  float3 Ng;
  /* view/incoming direction */
  float3 I;
  /* shader id */
  int shader;
  /* booleans describing shader, see ShaderDataFlag */
  int flag;
  /* booleans describing object of the shader, see ShaderDataObjectFlag */
  int object_flag;

  /* primitive id if there is one, ~0 otherwise */
  int prim;

  /* combined type and curve segment for hair */
  int type;

  /* parametric coordinates
   * - barycentric weights for triangles */
  float u;
  float v;
  /* object id if there is one, ~0 otherwise */
  int object;
  /* lamp id if there is one, ~0 otherwise */
  int lamp;

  /* motion blur sample time */
  float time;

  /* length of the ray being shaded */
  float ray_length;

#ifdef __RAY_DIFFERENTIALS__
  /* differential of P. these are orthogonal to Ng, not N */
  differential3 dP;
  /* differential of I */
  differential3 dI;
  /* differential of u, v */
  differential du;
  differential dv;
#endif
#ifdef __DPDU__
  /* differential of P w.r.t. parametric coordinates. note that dPdu is
   * not readily suitable as a tangent for shading on triangles. */
  float3 dPdu;
  float3 dPdv;
#endif

#ifdef __OBJECT_MOTION__
  /* object <-> world space transformations, cached to avoid
   * re-interpolating them constantly for shading */
  Transform ob_tfm;
  Transform ob_itfm;
#endif

  /* ray start position, only set for backgrounds */
  float3 ray_P;
  differential3 ray_dP;

#ifdef __OSL__
  struct KernelGlobals *osl_globals;
  struct PathState *osl_path_state;
#endif

  /* LCG state for closures that require additional random numbers. */
  uint lcg_state;

  /* Closure data, we store a fixed array of closures */
  int num_closure;
  int num_closure_left;
  float randb_closure;
  float3 svm_closure_weight;

  /* Closure weights summed directly, so we can evaluate
   * emission and shadow transparency with MAX_CLOSURE 0. */
  float3 closure_emission_background;
  float3 closure_transparent_extinction;

  /* At the end so we can adjust size in ShaderDataTinyStorage. */
  struct ShaderClosure closure[MAX_CLOSURE];
}
ShaderData;

/* ShaderDataTinyStorage needs the same alignment as ShaderData, or else
 * the pointer cast in AS_SHADER_DATA invokes undefined behavior. */
typedef ccl_addr_space struct ccl_align(16) ShaderDataTinyStorage
{
  char pad[sizeof(ShaderData) - sizeof(ShaderClosure) * MAX_CLOSURE];
}
ShaderDataTinyStorage;
#define AS_SHADER_DATA(shader_data_tiny_storage) ((ShaderData *)shader_data_tiny_storage)

/* Path State */

#ifdef __VOLUME__
typedef struct VolumeStack {
  int object;
  int shader;
} VolumeStack;
#endif

typedef struct PathState {
  /* see enum PathRayFlag */
  int flag;

  /* random number generator state */
  uint rng_hash;       /* per pixel hash */
  int rng_offset;      /* dimension offset */
  int sample;          /* path sample number */
  int num_samples;     /* total number of times this path will be sampled */
  float branch_factor; /* number of branches in indirect paths */

  /* bounce counting */
  int bounce;
  int diffuse_bounce;
  int glossy_bounce;
  int transmission_bounce;
  int transparent_bounce;

#ifdef __DENOISING_FEATURES__
  float denoising_feature_weight;
  float3 denoising_feature_throughput;
#endif /* __DENOISING_FEATURES__ */

  /* multiple importance sampling */
  float min_ray_pdf; /* smallest bounce pdf over entire path up to now */
  float ray_pdf;     /* last bounce pdf */
#ifdef __LAMP_MIS__
  float ray_t; /* accumulated distance through transparent surfaces */
#endif

  /* volume rendering */
#ifdef __VOLUME__
  int volume_bounce;
  int volume_bounds_bounce;
  VolumeStack volume_stack[VOLUME_STACK_SIZE];
#endif
} PathState;

#ifdef __VOLUME__
typedef struct VolumeState {
#  ifdef __SPLIT_KERNEL__
#  else
  PathState ps;
#  endif
} VolumeState;
#endif

/* Struct to gather multiple nearby intersections. */
typedef struct LocalIntersection {
  Ray ray;
  float3 weight[LOCAL_MAX_HITS];

  int num_hits;
  struct Intersection hits[LOCAL_MAX_HITS];
  float3 Ng[LOCAL_MAX_HITS];
} LocalIntersection;

/* Subsurface */

/* Struct to gather SSS indirect rays and delay tracing them. */
typedef struct SubsurfaceIndirectRays {
  PathState state[BSSRDF_MAX_HITS];

  int num_rays;

  struct Ray rays[BSSRDF_MAX_HITS];
  float3 throughputs[BSSRDF_MAX_HITS];
  struct PathRadianceState L_state[BSSRDF_MAX_HITS];
} SubsurfaceIndirectRays;
static_assert(BSSRDF_MAX_HITS <= LOCAL_MAX_HITS, "BSSRDF hits too high.");

/* Constant Kernel Data
 *
 * These structs are passed from CPU to various devices, and the struct layout
 * must match exactly. Structs are padded to ensure 16 byte alignment, and we
 * do not use float3 because its size may not be the same on all devices. */

typedef struct KernelCamera {
  /* type */
  int type;

  /* panorama */
  int panorama_type;
  float fisheye_fov;
  float fisheye_lens;
  float4 equirectangular_range;

  /* stereo */
  float interocular_offset;
  float convergence_distance;
  float pole_merge_angle_from;
  float pole_merge_angle_to;

  /* matrices */
  Transform cameratoworld;
  ProjectionTransform rastertocamera;

  /* differentials */
  float4 dx;
  float4 dy;

  /* depth of field */
  float aperturesize;
  float blades;
  float bladesrotation;
  float focaldistance;

  /* motion blur */
  float shuttertime;
  int num_motion_steps, have_perspective_motion;

  /* clipping */
  float nearclip;
  float cliplength;

  /* sensor size */
  float sensorwidth;
  float sensorheight;

  /* render size */
  float width, height;
  int resolution;

  /* anamorphic lens bokeh */
  float inv_aperture_ratio;

  int is_inside_volume;

  /* more matrices */
  ProjectionTransform screentoworld;
  ProjectionTransform rastertoworld;
  ProjectionTransform ndctoworld;
  ProjectionTransform worldtoscreen;
  ProjectionTransform worldtoraster;
  ProjectionTransform worldtondc;
  Transform worldtocamera;

  /* Stores changes in the projection matrix. Use for camera zoom motion
   * blur and motion pass output for perspective camera. */
  ProjectionTransform perspective_pre;
  ProjectionTransform perspective_post;

  /* Transforms for motion pass. */
  Transform motion_pass_pre;
  Transform motion_pass_post;

  int shutter_table_offset;

  /* Rolling shutter */
  int rolling_shutter_type;
  float rolling_shutter_duration;

  int pad;
} KernelCamera;
static_assert_align(KernelCamera, 16);

typedef struct KernelFilm {
  float exposure;
  int pass_flag;

  int light_pass_flag;
  int pass_stride;
  int use_light_pass;

  int pass_combined;
  int pass_depth;
  int pass_normal;
  int pass_motion;

  int pass_motion_weight;
  int pass_uv;
  int pass_object_id;
  int pass_material_id;

  int pass_diffuse_color;
  int pass_glossy_color;
  int pass_transmission_color;

  int pass_diffuse_indirect;
  int pass_glossy_indirect;
  int pass_transmission_indirect;
  int pass_volume_indirect;

  int pass_diffuse_direct;
  int pass_glossy_direct;
  int pass_transmission_direct;
  int pass_volume_direct;

  int pass_emission;
  int pass_background;
  int pass_ao;
  float pass_alpha_threshold;

  int pass_shadow;
  float pass_shadow_scale;
  int filter_table_offset;
  int cryptomatte_passes;
  int cryptomatte_depth;
  int pass_cryptomatte;

  int pass_adaptive_aux_buffer;
  int pass_sample_count;

  int pass_mist;
  float mist_start;
  float mist_inv_depth;
  float mist_falloff;

  int pass_denoising_data;
  int pass_denoising_clean;
  int denoising_flags;

  int pass_aov_color;
  int pass_aov_value;
  int pass_aov_color_num;
  int pass_aov_value_num;
  int pad1, pad2, pad3;

  /* XYZ to rendering color space transform. float4 instead of float3 to
   * ensure consistent padding/alignment across devices. */
  float4 xyz_to_r;
  float4 xyz_to_g;
  float4 xyz_to_b;
  float4 rgb_to_y;

  int pass_bake_primitive;
  int pass_bake_differential;
  int pad;

#ifdef __KERNEL_DEBUG__
  int pass_bvh_traversed_nodes;
  int pass_bvh_traversed_instances;
  int pass_bvh_intersections;
  int pass_ray_bounces;
#endif

  /* viewport rendering options */
  int display_pass_stride;
  int display_pass_components;
  int display_divide_pass_stride;
  int use_display_exposure;
  int use_display_pass_alpha;

  int pad4, pad5, pad6;
} KernelFilm;
static_assert_align(KernelFilm, 16);

typedef struct KernelBackground {
  /* only shader index */
  int surface_shader;
  int volume_shader;
  float volume_step_size;
  int transparent;
  float transparent_roughness_squared_threshold;

  /* ambient occlusion */
  float ao_factor;
  float ao_distance;
  float ao_bounces_factor;

  /* portal sampling */
  float portal_weight;
  int num_portals;
  int portal_offset;

  /* sun sampling */
  float sun_weight;
  /* xyz store direction, w the angle. float4 instead of float3 is used
   * to ensure consistent padding/alignment across devices. */
  float4 sun;

  /* map sampling */
  float map_weight;
  int map_res_x;
  int map_res_y;

  int use_mis;
} KernelBackground;
static_assert_align(KernelBackground, 16);

typedef struct KernelIntegrator {
  /* emission */
  int use_direct_light;
  int use_ambient_occlusion;
  int num_distribution;
  int num_all_lights;
  float pdf_triangles;
  float pdf_lights;
  float light_inv_rr_threshold;

  /* bounces */
  int min_bounce;
  int max_bounce;

  int max_diffuse_bounce;
  int max_glossy_bounce;
  int max_transmission_bounce;
  int max_volume_bounce;

  int ao_bounces;

  /* transparent */
  int transparent_min_bounce;
  int transparent_max_bounce;
  int transparent_shadows;

  /* caustics */
  int caustics_reflective;
  int caustics_refractive;
  float filter_glossy;

  /* seed */
  int seed;

  /* clamp */
  float sample_clamp_direct;
  float sample_clamp_indirect;

  /* branched path */
  int branched;
  int volume_decoupled;
  int diffuse_samples;
  int glossy_samples;
  int transmission_samples;
  int ao_samples;
  int mesh_light_samples;
  int subsurface_samples;
  int sample_all_lights_direct;
  int sample_all_lights_indirect;

  /* mis */
  int use_lamp_mis;

  /* sampler */
  int sampling_pattern;
  int aa_samples;
  int adaptive_min_samples;
  int adaptive_step;
  int adaptive_stop_per_sample;
  float adaptive_threshold;

  /* volume render */
  int use_volumes;
  int volume_max_steps;
  float volume_step_rate;
  int volume_samples;

  int start_sample;

  int max_closures;

  int pad1, pad2;
} KernelIntegrator;
static_assert_align(KernelIntegrator, 16);

typedef enum KernelBVHLayout {
  BVH_LAYOUT_NONE = 0,

  BVH_LAYOUT_BVH2 = (1 << 0),
  BVH_LAYOUT_BVH4 = (1 << 1),
  BVH_LAYOUT_BVH8 = (1 << 2),

  BVH_LAYOUT_EMBREE = (1 << 3),
  BVH_LAYOUT_OPTIX = (1 << 4),

  BVH_LAYOUT_DEFAULT = BVH_LAYOUT_BVH8,
  BVH_LAYOUT_ALL = (unsigned int)(~0u),
} KernelBVHLayout;

typedef struct KernelBVH {
  /* Own BVH */
  int root;
  int have_motion;
  int have_curves;
  int have_instancing;
  int bvh_layout;
  int use_bvh_steps;

  /* Custom BVH */
#ifdef __KERNEL_OPTIX__
  OptixTraversableHandle scene;
#else
#  ifdef __EMBREE__
  RTCScene scene;
#    ifndef __KERNEL_64_BIT__
  int pad2;
#    endif
#  else
  int scene, pad2;
#  endif
#endif
} KernelBVH;
static_assert_align(KernelBVH, 16);

typedef enum CurveFlag {
  /* runtime flags */
  CURVE_KN_RIBBONS = 1, /* use flat curve ribbons */
} CurveFlag;

typedef struct KernelCurves {
  int curveflags;
  int subdivisions;

  int pad1, pad2;
} KernelCurves;
static_assert_align(KernelCurves, 16);

typedef struct KernelTables {
  int beckmann_offset;
  int pad1, pad2, pad3;
} KernelTables;
static_assert_align(KernelTables, 16);

typedef struct KernelBake {
  int object_index;
  int tri_offset;
  int type;
  int pass_filter;
} KernelBake;
static_assert_align(KernelBake, 16);

typedef struct KernelData {
  KernelCamera cam;
  KernelFilm film;
  KernelBackground background;
  KernelIntegrator integrator;
  KernelBVH bvh;
  KernelCurves curve;
  KernelTables tables;
  KernelBake bake;
} KernelData;
static_assert_align(KernelData, 16);

/* Kernel data structures. */

typedef struct KernelObject {
  Transform tfm;
  Transform itfm;

  float surface_area;
  float pass_id;
  float random_number;
  float color[3];
  int particle_index;

  float dupli_generated[3];
  float dupli_uv[2];

  int numkeys;
  int numsteps;
  int numverts;

  uint patch_map_offset;
  uint attribute_map_offset;
  uint motion_offset;

  float cryptomatte_object;
  float cryptomatte_asset;

  float shadow_terminator_offset;
  float pad1, pad2, pad3;
} KernelObject;
static_assert_align(KernelObject, 16);

typedef struct KernelSpotLight {
  float radius;
  float invarea;
  float spot_angle;
  float spot_smooth;
  float dir[3];
  float pad;
} KernelSpotLight;

/* PointLight is SpotLight with only radius and invarea being used. */

typedef struct KernelAreaLight {
  float axisu[3];
  float invarea;
  float axisv[3];
  float pad1;
  float dir[3];
  float pad2;
} KernelAreaLight;

typedef struct KernelDistantLight {
  float radius;
  float cosangle;
  float invarea;
  float pad;
} KernelDistantLight;

typedef struct KernelLight {
  int type;
  float co[3];
  int shader_id;
  int samples;
  float max_bounces;
  float random;
  float strength[3];
  float pad1;
  Transform tfm;
  Transform itfm;
  union {
    KernelSpotLight spot;
    KernelAreaLight area;
    KernelDistantLight distant;
  };
} KernelLight;
static_assert_align(KernelLight, 16);

typedef struct KernelLightDistribution {
  float totarea;
  int prim;
  union {
    struct {
      int shader_flag;
      int object_id;
    } mesh_light;
    struct {
      float pad;
      float size;
    } lamp;
  };
} KernelLightDistribution;
static_assert_align(KernelLightDistribution, 16);

typedef struct KernelParticle {
  int index;
  float age;
  float lifetime;
  float size;
  float4 rotation;
  /* Only xyz are used of the following. float4 instead of float3 are used
   * to ensure consistent padding/alignment across devices. */
  float4 location;
  float4 velocity;
  float4 angular_velocity;
} KernelParticle;
static_assert_align(KernelParticle, 16);

typedef struct KernelShader {
  float constant_emission[3];
  float cryptomatte_id;
  int flags;
  int pass_id;
  int pad2, pad3;
} KernelShader;
static_assert_align(KernelShader, 16);

/* Declarations required for split kernel */

/* Macro for queues */
/* Value marking queue's empty slot */
#define QUEUE_EMPTY_SLOT -1

/*
 * Queue 1 - Active rays
 * Queue 2 - Background queue
 * Queue 3 - Shadow ray cast kernel - AO
 * Queue 4 - Shadow ray cast kernel - direct lighting
 */

/* Queue names */
enum QueueNumber {
  /* All active rays and regenerated rays are enqueued here. */
  QUEUE_ACTIVE_AND_REGENERATED_RAYS = 0,

  /* All
   * 1. Background-hit rays,
   * 2. Rays that has exited path-iteration but needs to update output buffer
   * 3. Rays to be regenerated
   * are enqueued here.
   */
  QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,

  /* All rays for which a shadow ray should be cast to determine radiance
   * contribution for AO are enqueued here.
   */
  QUEUE_SHADOW_RAY_CAST_AO_RAYS,

  /* All rays for which a shadow ray should be cast to determine radiance
   * contributing for direct lighting are enqueued here.
   */
  QUEUE_SHADOW_RAY_CAST_DL_RAYS,

  /* Rays sorted according to shader->id */
  QUEUE_SHADER_SORTED_RAYS,

#ifdef __BRANCHED_PATH__
  /* All rays moving to next iteration of the indirect loop for light */
  QUEUE_LIGHT_INDIRECT_ITER,
  /* Queue of all inactive rays. These are candidates for sharing work of indirect loops */
  QUEUE_INACTIVE_RAYS,
#  ifdef __VOLUME__
  /* All rays moving to next iteration of the indirect loop for volumes */
  QUEUE_VOLUME_INDIRECT_ITER,
#  endif
#  ifdef __SUBSURFACE__
  /* All rays moving to next iteration of the indirect loop for subsurface */
  QUEUE_SUBSURFACE_INDIRECT_ITER,
#  endif
#endif /* __BRANCHED_PATH__ */

  NUM_QUEUES
};

/* We use RAY_STATE_MASK to get ray_state */
#define RAY_STATE_MASK 0x0F
#define RAY_FLAG_MASK 0xF0
enum RayState {
  RAY_INVALID = 0,
  /* Denotes ray is actively involved in path-iteration. */
  RAY_ACTIVE,
  /* Denotes ray has completed processing all samples and is inactive. */
  RAY_INACTIVE,
  /* Denotes ray has exited path-iteration and needs to update output buffer. */
  RAY_UPDATE_BUFFER,
  /* Denotes ray needs to skip most surface shader work. */
  RAY_HAS_ONLY_VOLUME,
  /* Donotes ray has hit background */
  RAY_HIT_BACKGROUND,
  /* Denotes ray has to be regenerated */
  RAY_TO_REGENERATE,
  /* Denotes ray has been regenerated */
  RAY_REGENERATED,
  /* Denotes ray is moving to next iteration of the branched indirect loop */
  RAY_LIGHT_INDIRECT_NEXT_ITER,
  RAY_VOLUME_INDIRECT_NEXT_ITER,
  RAY_SUBSURFACE_INDIRECT_NEXT_ITER,

  /* Ray flags */

  /* Flags to denote that the ray is currently evaluating the branched indirect loop */
  RAY_BRANCHED_LIGHT_INDIRECT = (1 << 4),
  RAY_BRANCHED_VOLUME_INDIRECT = (1 << 5),
  RAY_BRANCHED_SUBSURFACE_INDIRECT = (1 << 6),
  RAY_BRANCHED_INDIRECT = (RAY_BRANCHED_LIGHT_INDIRECT | RAY_BRANCHED_VOLUME_INDIRECT |
                           RAY_BRANCHED_SUBSURFACE_INDIRECT),

  /* Ray is evaluating an iteration of an indirect loop for another thread */
  RAY_BRANCHED_INDIRECT_SHARED = (1 << 7),
};

#define ASSIGN_RAY_STATE(ray_state, ray_index, state) \
  (ray_state[ray_index] = ((ray_state[ray_index] & RAY_FLAG_MASK) | state))
#define IS_STATE(ray_state, ray_index, state) \
  ((ray_index) != QUEUE_EMPTY_SLOT && ((ray_state)[(ray_index)] & RAY_STATE_MASK) == (state))
#define ADD_RAY_FLAG(ray_state, ray_index, flag) \
  (ray_state[ray_index] = (ray_state[ray_index] | flag))
#define REMOVE_RAY_FLAG(ray_state, ray_index, flag) \
  (ray_state[ray_index] = (ray_state[ray_index] & (~flag)))
#define IS_FLAG(ray_state, ray_index, flag) (ray_state[ray_index] & flag)

/* Patches */

#define PATCH_MAX_CONTROL_VERTS 16

/* Patch map node flags */

#define PATCH_MAP_NODE_IS_SET (1 << 30)
#define PATCH_MAP_NODE_IS_LEAF (1u << 31)
#define PATCH_MAP_NODE_INDEX_MASK (~(PATCH_MAP_NODE_IS_SET | PATCH_MAP_NODE_IS_LEAF))

/* Work Tiles */

typedef struct WorkTile {
  uint x, y, w, h;

  uint start_sample;
  uint num_samples;

  int offset;
  uint stride;

  ccl_global float *buffer;
} WorkTile;

/* Precoumputed sample table sizes for PMJ02 sampler. */
#define NUM_PMJ_SAMPLES 64 * 64
#define NUM_PMJ_PATTERNS 48

CCL_NAMESPACE_END

#endif /*  __KERNEL_TYPES_H__ */
