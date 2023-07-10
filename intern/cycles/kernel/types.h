/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if (!defined(__KERNEL_GPU__) || (defined(__KERNEL_ONEAPI__) && defined(WITH_EMBREE_GPU))) && \
    defined(WITH_EMBREE)
#  if EMBREE_MAJOR_VERSION == 4
#    include <embree4/rtcore.h>
#    include <embree4/rtcore_scene.h>
#  else
#    include <embree3/rtcore.h>
#    include <embree3/rtcore_scene.h>
#  endif
#  define __EMBREE__
#endif

#include "util/math.h"
#include "util/math_fast.h"
#include "util/math_intersect.h"
#include "util/projection.h"
#include "util/static_assert.h"
#include "util/texture.h"
#include "util/transform.h"

#include "kernel/svm/types.h"

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

#define SHADER_NONE (~0)
#define OBJECT_NONE (~0)
#define PRIM_NONE (~0)
#define LAMP_NONE (~0)
#define ID_NONE (0.0f)
#define PASS_UNUSED (~0)
#define LIGHTGROUP_NONE (~0)

#define LIGHT_LINK_SET_MAX 64
#define LIGHT_LINK_MASK_ALL (~uint64_t(0))

#define INTEGRATOR_SHADOW_ISECT_SIZE_CPU 1024U
#define INTEGRATOR_SHADOW_ISECT_SIZE_GPU 4U

#ifdef __KERNEL_GPU__
#  define INTEGRATOR_SHADOW_ISECT_SIZE INTEGRATOR_SHADOW_ISECT_SIZE_GPU
#else
#  define INTEGRATOR_SHADOW_ISECT_SIZE INTEGRATOR_SHADOW_ISECT_SIZE_CPU
#endif

/* Kernel features */
#define __AO__
#define __CAUSTICS_TRICKS__
#define __CLAMP_SAMPLE__
#define __DENOISING_FEATURES__
#define __DPDU__
#define __HAIR__
#define __LIGHT_LINKING__
#define __SHADOW_LINKING__
#define __LIGHT_TREE__
#define __OBJECT_MOTION__
#define __PASSES__
#define __PATCH_EVAL__
#define __POINTCLOUD__
#define __RAY_DIFFERENTIALS__
#define __SHADER_RAYTRACE__
#define __SHADOW_CATCHER__
#define __SHADOW_RECORD_ALL__
#define __SUBSURFACE__
#define __SVM__
#define __TRANSPARENT_SHADOWS__
#define __VISIBILITY_FLAG__
#define __VOLUME__

/* Device specific features */
#ifdef WITH_OSL
#  define __OSL__
#  ifdef __KERNEL_OPTIX__
/* Kernels with OSL support are built separately in OptiX and don't need SVM. */
#    undef __SVM__
#  endif
#endif
#ifndef __KERNEL_GPU__
#  ifdef WITH_PATH_GUIDING
#    define __PATH_GUIDING__
#  endif
#  define __VOLUME_RECORD_ALL__
#endif /* !__KERNEL_GPU__ */

/* MNEE caused "Compute function exceeds available temporary registers" in macOS < 13 due to a bug
 * in spill buffer allocation sizing. */
#if !defined(__KERNEL_METAL__) || (__KERNEL_METAL_MACOS__ >= 13)
#  define __MNEE__
#endif

#if defined(__KERNEL_METAL_AMD__)
/* Disabled due to internal compiler perf issue and enable light tree on Metal/AMD. */
#  undef __LIGHT_TREE__
/* Disabled due to compiler crash on Metal/AMD. */
#  undef __MNEE__
#endif

/* Scene-based selective features compilation. */
#ifdef __KERNEL_FEATURES__
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_OBJECT_MOTION)
#    undef __OBJECT_MOTION__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_HAIR)
#    undef __HAIR__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_POINTCLOUD)
#    undef __POINTCLOUD__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_VOLUME)
#    undef __VOLUME__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_SUBSURFACE)
#    undef __SUBSURFACE__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_PATCH_EVALUATION)
#    undef __PATCH_EVAL__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_TRANSPARENT)
#    undef __TRANSPARENT_SHADOWS__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_SHADOW_CATCHER)
#    undef __SHADOW_CATCHER__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_DENOISING)
#    undef __DENOISING_FEATURES__
#  endif
#endif

#ifdef WITH_CYCLES_DEBUG_NAN
#  define __KERNEL_DEBUG_NAN__
#endif

/* Features that enable others */

#if defined(__SUBSURFACE__) || defined(__SHADER_RAYTRACE__)
#  define __BVH_LOCAL__
#endif

/* Sampling Patterns */

/* Unique numbers for sampling patterns in each bounce. */
enum PathTraceDimension {
  /* Init bounce */
  PRNG_FILTER = 0,
  PRNG_LENS_TIME = 1,

  /* Shade bounce */
  PRNG_TERMINATE = 0,
  PRNG_LIGHT = 1,
  PRNG_LIGHT_TERMINATE = 2,
  /* Surface */
  PRNG_SURFACE_BSDF = 3,
  PRNG_SURFACE_AO = 4,
  PRNG_SURFACE_BEVEL = 5,
  PRNG_SURFACE_BSDF_GUIDING = 6,

  /* Guiding RIS */
  PRNG_SURFACE_RIS_GUIDING_0 = 10,
  PRNG_SURFACE_RIS_GUIDING_1 = 11,

  /* Volume */
  PRNG_VOLUME_PHASE = 3,
  PRNG_VOLUME_PHASE_CHANNEL = 4,
  PRNG_VOLUME_SCATTER_DISTANCE = 5,
  PRNG_VOLUME_OFFSET = 6,
  PRNG_VOLUME_SHADE_OFFSET = 7,
  PRNG_VOLUME_PHASE_GUIDING_DISTANCE = 8,
  PRNG_VOLUME_PHASE_GUIDING_EQUIANGULAR = 9,

  /* Subsurface random walk bounces */
  PRNG_SUBSURFACE_BSDF = 0,
  PRNG_SUBSURFACE_PHASE_CHANNEL = 1,
  PRNG_SUBSURFACE_SCATTER_DISTANCE = 2,
  PRNG_SUBSURFACE_GUIDE_STRATEGY = 3,
  PRNG_SUBSURFACE_GUIDE_DIRECTION = 4,

  /* Subsurface disk bounce */
  PRNG_SUBSURFACE_DISK = 0,
  PRNG_SUBSURFACE_DISK_RESAMPLE = 1,

  /* High enough number so we don't need to change it when adding new dimensions,
   * low enough so there is no uint16_t overflow with many bounces. */
  PRNG_BOUNCE_NUM = 16,
};

enum SamplingPattern {
  SAMPLING_PATTERN_SOBOL_BURLEY = 0,
  SAMPLING_PATTERN_TABULATED_SOBOL = 1,

  SAMPLING_NUM_PATTERNS,
};

/* These flags values correspond to `raytypes` in `osl.cpp`, so keep them in sync! */

enum PathRayFlag : uint32_t {
  /* --------------------------------------------------------------------
   * Ray visibility.
   *
   * NOTE: Recalculated after a surface bounce.
   */

  PATH_RAY_CAMERA = (1U << 0U),
  PATH_RAY_REFLECT = (1U << 1U),
  PATH_RAY_TRANSMIT = (1U << 2U),
  PATH_RAY_DIFFUSE = (1U << 3U),
  PATH_RAY_GLOSSY = (1U << 4U),
  PATH_RAY_SINGULAR = (1U << 5U),
  PATH_RAY_TRANSPARENT = (1U << 6U),
  PATH_RAY_VOLUME_SCATTER = (1U << 7U),
  PATH_RAY_IMPORTANCE_BAKE = (1U << 8U),

  /* Shadow ray visibility. */
  PATH_RAY_SHADOW_OPAQUE = (1U << 9U),
  PATH_RAY_SHADOW_TRANSPARENT = (1U << 10U),
  PATH_RAY_SHADOW = (PATH_RAY_SHADOW_OPAQUE | PATH_RAY_SHADOW_TRANSPARENT),

  /* Subset of flags used for ray visibility for intersection.
   *
   * NOTE: SHADOW_CATCHER macros below assume there are no more than
   * 16 visibility bits. */
  PATH_RAY_ALL_VISIBILITY = ((1U << 11U) - 1U),

  /* Special flag to tag unaligned BVH nodes.
   * Only set and used in BVH nodes to distinguish how to interpret bounding box information stored
   * in the node (either it should be intersected as AABB or as OBBU).
   * So this can overlap with path flags. */
  PATH_RAY_NODE_UNALIGNED = (1U << 11U),

  /* --------------------------------------------------------------------
   * Path flags.
   */

  /* Surface had transmission component at previous bounce. Used for light tree
   * traversal and culling to be consistent with MIS PDF at the next bounce. */
  PATH_RAY_MIS_HAD_TRANSMISSION = (1U << 11U),

  /* Don't apply multiple importance sampling weights to emission from
   * lamp or surface hits, because they were not direct light sampled. */
  PATH_RAY_MIS_SKIP = (1U << 12U),

  /* Diffuse bounce earlier in the path, skip SSS to improve performance
   * and avoid branching twice with disk sampling SSS. */
  PATH_RAY_DIFFUSE_ANCESTOR = (1U << 13U),

  /* Single pass has been written. */
  PATH_RAY_SINGLE_PASS_DONE = (1U << 14U),

  /* Zero background alpha, for camera or transparent glass rays. */
  PATH_RAY_TRANSPARENT_BACKGROUND = (1U << 15U),

  /* Terminate ray immediately at next bounce. */
  PATH_RAY_TERMINATE_ON_NEXT_SURFACE = (1U << 16U),
  PATH_RAY_TERMINATE_IN_NEXT_VOLUME = (1U << 17U),

  /* Ray is to be terminated, but continue with transparent bounces and
   * emission as long as we encounter them. This is required to make the
   * MIS between direct and indirect light rays match, as shadow rays go
   * through transparent surfaces to reach emission too. */
  PATH_RAY_TERMINATE_AFTER_TRANSPARENT = (1U << 18U),

  /* Terminate ray immediately after volume shading. */
  PATH_RAY_TERMINATE_AFTER_VOLUME = (1U << 19U),

  /* Ray is to be terminated. */
  PATH_RAY_TERMINATE = (PATH_RAY_TERMINATE_ON_NEXT_SURFACE | PATH_RAY_TERMINATE_IN_NEXT_VOLUME |
                        PATH_RAY_TERMINATE_AFTER_TRANSPARENT | PATH_RAY_TERMINATE_AFTER_VOLUME),

  /* Path and shader is being evaluated for direct lighting emission. */
  PATH_RAY_EMISSION = (1U << 20U),

  /* Perform subsurface scattering. */
  PATH_RAY_SUBSURFACE_RANDOM_WALK = (1U << 21U),
  PATH_RAY_SUBSURFACE_DISK = (1U << 22U),
  PATH_RAY_SUBSURFACE_USE_FRESNEL = (1U << 23U),
  PATH_RAY_SUBSURFACE_BACKFACING = (1U << 24U),
  PATH_RAY_SUBSURFACE = (PATH_RAY_SUBSURFACE_RANDOM_WALK | PATH_RAY_SUBSURFACE_DISK |
                         PATH_RAY_SUBSURFACE_USE_FRESNEL | PATH_RAY_SUBSURFACE_BACKFACING),

  /* Contribute to denoising features. */
  PATH_RAY_DENOISING_FEATURES = (1U << 25U),

  /* Render pass categories. */
  PATH_RAY_SURFACE_PASS = (1U << 26U),
  PATH_RAY_VOLUME_PASS = (1U << 27U),
  PATH_RAY_ANY_PASS = (PATH_RAY_SURFACE_PASS | PATH_RAY_VOLUME_PASS),

  /* Shadow ray is for AO. */
  PATH_RAY_SHADOW_FOR_AO = (1U << 28U),

  /* A shadow catcher object was hit and the path was split into two. */
  PATH_RAY_SHADOW_CATCHER_HIT = (1U << 29U),

  /* A shadow catcher object was hit and this path traces only shadow catchers, writing them into
   * their dedicated pass for later division.
   *
   * NOTE: Is not covered with `PATH_RAY_ANY_PASS` because shadow catcher does special handling
   * which is separate from the light passes. */
  PATH_RAY_SHADOW_CATCHER_PASS = (1U << 30U),

  /* Path is evaluating background for an approximate shadow catcher with non-transparent film. */
  PATH_RAY_SHADOW_CATCHER_BACKGROUND = (1U << 31U),
};

// 8bit enum, just in case we need to move more variables in it
enum PathRayMNEE {
  PATH_MNEE_NONE = 0,

  PATH_MNEE_VALID = (1U << 0U),
  PATH_MNEE_RECEIVER_ANCESTOR = (1U << 1U),
  PATH_MNEE_CULL_LIGHT_CONNECTION = (1U << 2U),
};

/* Configure ray visibility bits for rays and objects respectively,
 * to make shadow catchers work.
 *
 * On shadow catcher paths we want to ignore any intersections with non-catchers,
 * whereas on regular paths we want to intersect all objects. */

#define SHADOW_CATCHER_VISIBILITY_SHIFT(visibility) ((visibility) << 16)

#define SHADOW_CATCHER_PATH_VISIBILITY(path_flag, visibility) \
  (((path_flag)&PATH_RAY_SHADOW_CATCHER_PASS) ? SHADOW_CATCHER_VISIBILITY_SHIFT(visibility) : \
                                                (visibility))

#define SHADOW_CATCHER_OBJECT_VISIBILITY(is_shadow_catcher, visibility) \
  (((is_shadow_catcher) ? SHADOW_CATCHER_VISIBILITY_SHIFT(visibility) : 0) | (visibility))

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
  LABEL_SUBSURFACE_SCATTER = 256,
} ClosureLabel;

/* Render Passes */

#define PASS_NAME_JOIN(a, b) a##_##b
#define PASSMASK(pass) (1 << ((PASS_NAME_JOIN(PASS, pass)) % 32))

// NOTE: Keep in sync with `Pass::get_type_enum()`.
typedef enum PassType {
  PASS_NONE = 0,

  /* Light Passes */
  PASS_COMBINED = 1,
  PASS_EMISSION,
  PASS_BACKGROUND,
  PASS_AO,
  PASS_DIFFUSE,
  PASS_DIFFUSE_DIRECT,
  PASS_DIFFUSE_INDIRECT,
  PASS_GLOSSY,
  PASS_GLOSSY_DIRECT,
  PASS_GLOSSY_INDIRECT,
  PASS_TRANSMISSION,
  PASS_TRANSMISSION_DIRECT,
  PASS_TRANSMISSION_INDIRECT,
  PASS_VOLUME,
  PASS_VOLUME_DIRECT,
  PASS_VOLUME_INDIRECT,
  PASS_CATEGORY_LIGHT_END = 31,

  /* Data passes */
  PASS_DEPTH = 32,
  PASS_POSITION,
  PASS_NORMAL,
  PASS_ROUGHNESS,
  PASS_UV,
  PASS_OBJECT_ID,
  PASS_MATERIAL_ID,
  PASS_MOTION,
  PASS_MOTION_WEIGHT,
  PASS_CRYPTOMATTE,
  PASS_AOV_COLOR,
  PASS_AOV_VALUE,
  PASS_ADAPTIVE_AUX_BUFFER,
  PASS_SAMPLE_COUNT,
  PASS_DIFFUSE_COLOR,
  PASS_GLOSSY_COLOR,
  PASS_TRANSMISSION_COLOR,
  /* No Scatter color since it's tricky to define what it would even mean. */
  PASS_MIST,
  PASS_DENOISING_NORMAL,
  PASS_DENOISING_ALBEDO,
  PASS_DENOISING_DEPTH,
  PASS_DENOISING_PREVIOUS,

  /* PASS_SHADOW_CATCHER accumulates contribution of shadow catcher object which is not affected by
   * any other object. The pass accessor will divide the combined pass by the shadow catcher. The
   * result of this division is then to be multiplied with the backdrop. The alpha channel of this
   * pass contains number of samples which contributed to the color components of the pass.
   *
   * PASS_SHADOW_CATCHER_SAMPLE_COUNT contains number of samples for which the path split
   * happened.
   *
   * PASS_SHADOW_CATCHER_MATTE contains pass which contains non-catcher objects. This pass is to be
   * alpha-overed onto the backdrop (after multiplication). */
  PASS_SHADOW_CATCHER,
  PASS_SHADOW_CATCHER_SAMPLE_COUNT,
  PASS_SHADOW_CATCHER_MATTE,

  /* Guiding related debug rendering passes */
  /* The estimated sample color from the PathSegmentStorage. If everything is integrated correctly
   * the output should be similar to PASS_COMBINED. */
  PASS_GUIDING_COLOR,
  /* The guiding probability at the first bounce. */
  PASS_GUIDING_PROBABILITY,
  /* The avg. roughness at the first bounce. */
  PASS_GUIDING_AVG_ROUGHNESS,
  PASS_CATEGORY_DATA_END = 63,

  PASS_BAKE_PRIMITIVE,
  PASS_BAKE_DIFFERENTIAL,
  PASS_CATEGORY_BAKE_END = 95,

  PASS_NUM,
} PassType;

#define PASS_ANY (~0)

typedef enum CryptomatteType {
  CRYPT_NONE = 0,
  CRYPT_OBJECT = (1 << 0),
  CRYPT_MATERIAL = (1 << 1),
  CRYPT_ASSET = (1 << 2),
  CRYPT_ACCURATE = (1 << 3),
} CryptomatteType;

typedef struct BsdfEval {
  Spectrum diffuse;
  Spectrum glossy;
  Spectrum sum;
} BsdfEval;

/* Closure Filter */

typedef enum FilterClosures {
  FILTER_CLOSURE_EMISSION = (1 << 0),
  FILTER_CLOSURE_DIFFUSE = (1 << 1),
  FILTER_CLOSURE_GLOSSY = (1 << 2),
  FILTER_CLOSURE_TRANSMISSION = (1 << 3),
  FILTER_CLOSURE_TRANSPARENT = (1 << 4),
  FILTER_CLOSURE_DIRECT_LIGHT = (1 << 5),
} FilterClosures;

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
  SHADER_EXCLUDE_SHADOW_CATCHER = (1 << 22),
  SHADER_EXCLUDE_ANY = (SHADER_EXCLUDE_DIFFUSE | SHADER_EXCLUDE_GLOSSY | SHADER_EXCLUDE_TRANSMIT |
                        SHADER_EXCLUDE_CAMERA | SHADER_EXCLUDE_SCATTER |
                        SHADER_EXCLUDE_SHADOW_CATCHER),

  SHADER_MASK = ~(SHADER_SMOOTH_NORMAL | SHADER_CAST_SHADOW | SHADER_AREA_LIGHT | SHADER_USE_MIS |
                  SHADER_EXCLUDE_ANY)
} ShaderFlag;

enum EmissionSampling {
  EMISSION_SAMPLING_NONE = 0,
  EMISSION_SAMPLING_AUTO = 1,
  EMISSION_SAMPLING_FRONT = 2,
  EMISSION_SAMPLING_BACK = 3,
  EMISSION_SAMPLING_FRONT_BACK = 4,

  EMISSION_SAMPLING_NUM
};

/* Light Type */

typedef enum LightType {
  LIGHT_POINT,
  LIGHT_DISTANT,
  LIGHT_BACKGROUND,
  LIGHT_AREA,
  LIGHT_SPOT,
  LIGHT_TRIANGLE
} LightType;

/* Guiding Distribution Type */

typedef enum GuidingDistributionType {
  GUIDING_TYPE_PARALLAX_AWARE_VMM = 0,
  GUIDING_TYPE_DIRECTIONAL_QUAD_TREE = 1,
  GUIDING_TYPE_VMM = 2,

  GUIDING_NUM_TYPES,
} GuidingDistributionType;

/* Guiding Directional Sampling Type */

typedef enum GuidingDirectionalSamplingType {
  GUIDING_DIRECTIONAL_SAMPLING_TYPE_PRODUCT_MIS = 0,
  GUIDING_DIRECTIONAL_SAMPLING_TYPE_RIS = 1,
  GUIDING_DIRECTIONAL_SAMPLING_TYPE_ROUGHNESS = 2,

  GUIDING_DIRECTIONAL_SAMPLING_NUM_TYPES,
} GuidingDirectionalSamplingType;

/* Camera Type */

enum CameraType { CAMERA_PERSPECTIVE, CAMERA_ORTHOGRAPHIC, CAMERA_PANORAMA };

/* Panorama Type */

enum PanoramaType {
  PANORAMA_EQUIRECTANGULAR = 0,
  PANORAMA_FISHEYE_EQUIDISTANT = 1,
  PANORAMA_FISHEYE_EQUISOLID = 2,
  PANORAMA_MIRRORBALL = 3,
  PANORAMA_FISHEYE_LENS_POLYNOMIAL = 4,
  PANORAMA_EQUIANGULAR_CUBEMAP_FACE = 5,

  PANORAMA_NUM_TYPES,
};

/* Specifies an offset for the shutter's time interval. */
enum MotionPosition {
  /* Shutter opens at the current frame. */
  MOTION_POSITION_START = 0,
  /* Shutter is fully open at the current frame. */
  MOTION_POSITION_CENTER = 1,
  /* Shutter closes at the current frame. */
  MOTION_POSITION_END = 2,

  MOTION_NUM_POSITIONS,
};

/* Direct Light Sampling */

enum DirectLightSamplingType {
  DIRECT_LIGHT_SAMPLING_MIS = 0,
  DIRECT_LIGHT_SAMPLING_FORWARD = 1,
  DIRECT_LIGHT_SAMPLING_NEE = 2,

  DIRECT_LIGHT_SAMPLING_NUM,
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

typedef struct RaySelfPrimitives {
  int prim;         /* Primitive the ray is starting from */
  int object;       /* Instance prim is a part of */
  int light_prim;   /* Light primitive */
  int light_object; /* Light object */
  int light;        /* Light ID (the light the shadow ray is traced towards to) */
} RaySelfPrimitives;

typedef struct Ray {
  float3 P;   /* origin */
  float3 D;   /* direction */
  float tmin; /* start distance */
  float tmax; /* end distance */
  float time; /* time (for motion blur) */

  RaySelfPrimitives self;

#ifdef __RAY_DIFFERENTIALS__
  float dP;
  float dD;
#endif
} Ray;

/* Intersection */

typedef struct Intersection {
  float t, u, v;
  int prim;
  int object;
  int type;
} Intersection;

/* Primitives */

typedef enum PrimitiveType {
  PRIMITIVE_NONE = 0,
  PRIMITIVE_TRIANGLE = (1 << 0),
  PRIMITIVE_CURVE_THICK = (1 << 1),
  PRIMITIVE_CURVE_RIBBON = (1 << 2),
  PRIMITIVE_POINT = (1 << 3),
  PRIMITIVE_VOLUME = (1 << 4),
  PRIMITIVE_LAMP = (1 << 5),

  PRIMITIVE_MOTION = (1 << 6),
  PRIMITIVE_MOTION_TRIANGLE = (PRIMITIVE_TRIANGLE | PRIMITIVE_MOTION),
  PRIMITIVE_MOTION_CURVE_THICK = (PRIMITIVE_CURVE_THICK | PRIMITIVE_MOTION),
  PRIMITIVE_MOTION_CURVE_RIBBON = (PRIMITIVE_CURVE_RIBBON | PRIMITIVE_MOTION),
  PRIMITIVE_MOTION_POINT = (PRIMITIVE_POINT | PRIMITIVE_MOTION),

  PRIMITIVE_CURVE = (PRIMITIVE_CURVE_THICK | PRIMITIVE_CURVE_RIBBON),

  PRIMITIVE_ALL = (PRIMITIVE_TRIANGLE | PRIMITIVE_CURVE | PRIMITIVE_POINT | PRIMITIVE_VOLUME |
                   PRIMITIVE_LAMP | PRIMITIVE_MOTION),

  PRIMITIVE_NUM_SHAPES = 6,
  PRIMITIVE_NUM_BITS = PRIMITIVE_NUM_SHAPES + 1, /* All shapes + motion bit. */
  PRIMITIVE_NUM = PRIMITIVE_NUM_SHAPES * 2,      /* With and without motion. */
} PrimitiveType;

/* Convert type to index in range 0..PRIMITIVE_NUM-1. */
#define PRIMITIVE_INDEX(type) (bitscan((uint32_t)(type)) * 2 + (((type)&PRIMITIVE_MOTION) ? 1 : 0))

/* Pack segment into type value to save space. */
#define PRIMITIVE_PACK_SEGMENT(type, segment) ((segment << PRIMITIVE_NUM_BITS) | (type))
#define PRIMITIVE_UNPACK_SEGMENT(type) (type >> PRIMITIVE_NUM_BITS)

typedef enum CurveShapeType {
  CURVE_RIBBON = 0,
  CURVE_THICK = 1,

  CURVE_NUM_SHAPE_TYPES,
} CurveShapeType;

/* Attributes */

typedef enum AttributePrimitive {
  ATTR_PRIM_GEOMETRY = 0,
  ATTR_PRIM_SUBD,

  ATTR_PRIM_TYPES
} AttributePrimitive;

typedef enum AttributeElement {
  ATTR_ELEMENT_NONE = 0,
  ATTR_ELEMENT_OBJECT = (1 << 0),
  ATTR_ELEMENT_MESH = (1 << 1),
  ATTR_ELEMENT_FACE = (1 << 2),
  ATTR_ELEMENT_VERTEX = (1 << 3),
  ATTR_ELEMENT_VERTEX_MOTION = (1 << 4),
  ATTR_ELEMENT_CORNER = (1 << 5),
  ATTR_ELEMENT_CORNER_BYTE = (1 << 6),
  ATTR_ELEMENT_CURVE = (1 << 7),
  ATTR_ELEMENT_CURVE_KEY = (1 << 8),
  ATTR_ELEMENT_CURVE_KEY_MOTION = (1 << 9),
  ATTR_ELEMENT_VOXEL = (1 << 10)
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
  ATTR_STD_CURVE_LENGTH,
  ATTR_STD_CURVE_RANDOM,
  ATTR_STD_POINT_RANDOM,
  ATTR_STD_PTEX_FACE_ID,
  ATTR_STD_PTEX_UV,
  ATTR_STD_VOLUME_DENSITY,
  ATTR_STD_VOLUME_COLOR,
  ATTR_STD_VOLUME_FLAME,
  ATTR_STD_VOLUME_HEAT,
  ATTR_STD_VOLUME_TEMPERATURE,
  ATTR_STD_VOLUME_VELOCITY,
  ATTR_STD_VOLUME_VELOCITY_X,
  ATTR_STD_VOLUME_VELOCITY_Y,
  ATTR_STD_VOLUME_VELOCITY_Z,
  ATTR_STD_POINTINESS,
  ATTR_STD_RANDOM_PER_ISLAND,
  ATTR_STD_SHADOW_TRANSPARENCY,
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

/* For looking up attributes on objects and geometry. */
typedef struct AttributeMap {
  uint64_t id;      /* Global unique identifier. */
  int offset;       /* Offset into __attributes global arrays. */
  uint16_t element; /* AttributeElement. */
  uint8_t type;     /* NodeAttributeType. */
  uint8_t flags;    /* AttributeFlag. */
} AttributeMap;

/* Closure data */

#ifndef __MAX_CLOSURE__
#  define MAX_CLOSURE 64
#else
#  define MAX_CLOSURE __MAX_CLOSURE__
#endif

/* For manifold next event estimation, we need space to store and evaluate
 * 2 closures (with extra data) on the refractive interfaces, in addition
 * to keeping the full sd at the current shading point. We need 4 because a
 * refractive BSDF is instanced with a companion reflection BSDF, even though
 * we only need the refractive one, and each of them requires 2 slots. */
#ifndef __CAUSTICS_MAX_CLOSURE__
#  define CAUSTICS_MAX_CLOSURE 4
#else
#  define CAUSTICS_MAX_CLOSURE __CAUSTICS_MAX_CLOSURE__
#endif

#ifndef __MAX_VOLUME_STACK_SIZE__
#  define MAX_VOLUME_STACK_SIZE 32
#else
#  define MAX_VOLUME_STACK_SIZE __MAX_VOLUME_STACK_SIZE__
#endif

#define MAX_VOLUME_CLOSURE 8

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
  Spectrum weight; \
  ClosureType type; \
  float sample_weight; \
  float3 N

typedef struct ccl_align(16) ShaderClosure
{
  SHADER_CLOSURE_BASE;

  /* Extra space for closures to store data, somewhat arbitrary but closures
   * assert that their size fits. */
  char pad[sizeof(Spectrum) * 2 + sizeof(float) * 4];
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
  /* BSDF has a transmissive component. */
  SD_BSDF_HAS_TRANSMISSION = (1 << 11),

  SD_CLOSURE_FLAGS = (SD_EMISSION | SD_BSDF | SD_BSDF_HAS_EVAL | SD_BSSRDF | SD_HOLDOUT |
                      SD_EXTINCTION | SD_SCATTER | SD_BSDF_NEEDS_LCG | SD_BSDF_HAS_TRANSMISSION),

  /* Shader flags. */

  /* Use front side for direct light sampling. */
  SD_MIS_FRONT = (1 << 16),
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
  /* Shader has emission */
  SD_HAS_EMISSION = (1 << 29),
  /* Shader has ray-tracing. */
  SD_HAS_RAYTRACE = (1 << 30),
  /* Use back side for direct light sampling. */
  SD_MIS_BACK = (1 << 31),

  SD_SHADER_FLAGS = (SD_MIS_FRONT | SD_HAS_TRANSPARENT_SHADOW | SD_HAS_VOLUME |
                     SD_HAS_ONLY_VOLUME | SD_HETEROGENEOUS_VOLUME | SD_HAS_BSSRDF_BUMP |
                     SD_VOLUME_EQUIANGULAR | SD_VOLUME_MIS | SD_VOLUME_CUBIC | SD_HAS_BUMP |
                     SD_HAS_DISPLACEMENT | SD_HAS_CONSTANT_EMISSION | SD_NEED_VOLUME_ATTRIBUTES |
                     SD_HAS_EMISSION | SD_HAS_RAYTRACE | SD_MIS_BACK)
};

/* Object flags. */
enum ShaderDataObjectFlag {
  /* Holdout for camera rays. */
  SD_OBJECT_HOLDOUT_MASK = (1 << 0),
  /* Has object motion blur. */
  SD_OBJECT_MOTION = (1 << 1),
  /* Vertices have transform applied. */
  SD_OBJECT_TRANSFORM_APPLIED = (1 << 2),
  /* The object's transform applies a negative scale. */
  SD_OBJECT_NEGATIVE_SCALE = (1 << 3),
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
  /* object is caustics caster */
  SD_OBJECT_CAUSTICS_CASTER = (1 << 9),
  /* object is caustics receiver */
  SD_OBJECT_CAUSTICS_RECEIVER = (1 << 10),
  /* object has attribute for volume motion */
  SD_OBJECT_HAS_VOLUME_MOTION = (1 << 11),

  /* object is using caustics */
  SD_OBJECT_CAUSTICS = (SD_OBJECT_CAUSTICS_CASTER | SD_OBJECT_CAUSTICS_RECEIVER),

  SD_OBJECT_FLAGS = (SD_OBJECT_HOLDOUT_MASK | SD_OBJECT_MOTION | SD_OBJECT_TRANSFORM_APPLIED |
                     SD_OBJECT_NEGATIVE_SCALE | SD_OBJECT_HAS_VOLUME |
                     SD_OBJECT_INTERSECTS_VOLUME | SD_OBJECT_SHADOW_CATCHER |
                     SD_OBJECT_HAS_VOLUME_ATTRIBUTES | SD_OBJECT_CAUSTICS |
                     SD_OBJECT_HAS_VOLUME_MOTION)
};

typedef struct ccl_align(16) ShaderData
{
  /* position */
  float3 P;
  /* smooth normal for shading */
  float3 N;
  /* true geometric normal */
  float3 Ng;
  /* view/incoming direction */
  float3 wi;
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
  /* Radius of differential of P. */
  float dP;
  /* Radius of differential of wi. */
  float dI;
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
  /* Object <-> world space transformations for motion blur, cached to avoid
   * re-interpolating them constantly for shading. */
  Transform ob_tfm_motion;
  Transform ob_itfm_motion;
#endif

  /* ray start position, only set for backgrounds */
  float3 ray_P;
  float ray_dP;

#ifdef __OSL__
#  ifdef __KERNEL_GPU__
  ccl_private uint8_t *osl_closure_pool;
#  else
  const struct KernelGlobalsCPU *osl_globals;
  const struct IntegratorStateCPU *osl_path_state;
  const struct IntegratorShadowStateCPU *osl_shadow_path_state;
#  endif
#endif

  /* LCG state for closures that require additional random numbers. */
  uint lcg_state;

  /* Closure data, we store a fixed array of closures */
  int num_closure;
  int num_closure_left;
  Spectrum svm_closure_weight;

  /* Closure weights summed directly, so we can evaluate
   * emission and shadow transparency with MAX_CLOSURE 0. */
  Spectrum closure_emission_background;
  Spectrum closure_transparent_extinction;

  /* At the end so we can adjust size in ShaderDataTinyStorage. */
  struct ShaderClosure closure[MAX_CLOSURE];
}
ShaderData;

/* ShaderDataTinyStorage needs the same alignment as ShaderData, or else
 * the pointer cast in AS_SHADER_DATA invokes undefined behavior. */
typedef struct ccl_align(16) ShaderDataTinyStorage
{
  char pad[sizeof(ShaderData) - sizeof(ShaderClosure) * MAX_CLOSURE];
}
ShaderDataTinyStorage;

/* ShaderDataCausticsStorage needs the same alignment as ShaderData, or else
 * the pointer cast in AS_SHADER_DATA invokes undefined behavior. */
typedef struct ccl_align(16) ShaderDataCausticsStorage
{
  char pad[sizeof(ShaderData) - sizeof(ShaderClosure) * (MAX_CLOSURE - CAUSTICS_MAX_CLOSURE)];
}
ShaderDataCausticsStorage;

#define AS_SHADER_DATA(shader_data_tiny_storage) \
  ((ccl_private ShaderData *)shader_data_tiny_storage)

/* Compact volume closures storage.
 *
 * Used for decoupled direct/indirect light closure storage. */

typedef struct ShaderVolumeClosure {
  Spectrum weight;
  float sample_weight;
  float g;
} ShaderVolumeClosure;

typedef struct ShaderVolumePhases {
  ShaderVolumeClosure closure[MAX_VOLUME_CLOSURE];
  int num_closure;
} ShaderVolumePhases;

/* Volume Stack */

#ifdef __VOLUME__
typedef struct VolumeStack {
  int object;
  int shader;
} VolumeStack;
#endif

/* Struct to gather multiple nearby intersections. */
typedef struct LocalIntersection {
  int num_hits;
  struct Intersection hits[LOCAL_MAX_HITS];
  float3 Ng[LOCAL_MAX_HITS];
} LocalIntersection;

/* Constant Kernel Data
 *
 * These structs are passed from CPU to various devices, and the struct layout
 * must match exactly. Structs are padded to ensure 16 byte alignment, and we
 * do not use float3 because its size may not be the same on all devices. */

typedef struct KernelCamera {
  /* type */
  int type;
  int use_dof_or_motion_blur;

  /* depth of field */
  float aperturesize;
  float blades;
  float bladesrotation;
  float focaldistance;

  /* motion blur */
  float shuttertime;
  int num_motion_steps, have_perspective_motion;

  int pad1;
  int pad2;
  int pad3;

  /* panorama */
  int panorama_type;
  float fisheye_fov;
  float fisheye_lens;
  float fisheye_lens_polynomial_bias;
  float4 equirectangular_range;
  float4 fisheye_lens_polynomial_coefficients;

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

  /* clipping */
  float nearclip;
  float cliplength;

  /* sensor size */
  float sensorwidth;
  float sensorheight;

  /* render size */
  float width, height;

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

  int motion_position;
} KernelCamera;
static_assert_align(KernelCamera, 16);

typedef struct KernelFilmConvert {
  int pass_offset;
  int pass_stride;

  int pass_use_exposure;
  int pass_use_filter;

  int pass_divide;
  int pass_indirect;

  int pass_combined;
  int pass_sample_count;
  int pass_adaptive_aux_buffer;
  int pass_motion_weight;
  int pass_shadow_catcher;
  int pass_shadow_catcher_sample_count;
  int pass_shadow_catcher_matte;
  int pass_background;

  float scale;
  float exposure;
  float scale_exposure;

  int use_approximate_shadow_catcher;
  int use_approximate_shadow_catcher_background;
  int show_active_pixels;

  /* Number of components to write to. */
  int num_components;

  /* Number of floats per pixel. When zero is the same as `num_components`.
   * NOTE: Is ignored for half4 destination. */
  int pixel_stride;

  int is_denoised;

  /* Padding. */
  int pad1;
} KernelFilmConvert;
static_assert_align(KernelFilmConvert, 16);

typedef enum KernelBVHLayout {
  BVH_LAYOUT_NONE = 0,

  BVH_LAYOUT_BVH2 = (1 << 0),
  BVH_LAYOUT_EMBREE = (1 << 1),
  BVH_LAYOUT_OPTIX = (1 << 2),
  BVH_LAYOUT_MULTI_OPTIX = (1 << 3),
  BVH_LAYOUT_MULTI_OPTIX_EMBREE = (1 << 4),
  BVH_LAYOUT_METAL = (1 << 5),
  BVH_LAYOUT_MULTI_METAL = (1 << 6),
  BVH_LAYOUT_MULTI_METAL_EMBREE = (1 << 7),
  BVH_LAYOUT_HIPRT = (1 << 8),
  BVH_LAYOUT_MULTI_HIPRT = (1 << 9),
  BVH_LAYOUT_MULTI_HIPRT_EMBREE = (1 << 10),
  BVH_LAYOUT_EMBREEGPU = (1 << 11),
  BVH_LAYOUT_MULTI_EMBREEGPU = (1 << 12),
  BVH_LAYOUT_MULTI_EMBREEGPU_EMBREE = (1 << 13),

  /* Default BVH layout to use for CPU. */
  BVH_LAYOUT_AUTO = BVH_LAYOUT_EMBREE,
  BVH_LAYOUT_ALL = BVH_LAYOUT_BVH2 | BVH_LAYOUT_EMBREE | BVH_LAYOUT_OPTIX | BVH_LAYOUT_METAL |
                   BVH_LAYOUT_HIPRT | BVH_LAYOUT_MULTI_HIPRT | BVH_LAYOUT_MULTI_HIPRT_EMBREE |
                   BVH_LAYOUT_EMBREEGPU | BVH_LAYOUT_MULTI_EMBREEGPU |
                   BVH_LAYOUT_MULTI_EMBREEGPU_EMBREE,
} KernelBVHLayout;

/* Specialized struct that can become constants in dynamic compilation. */
#define KERNEL_STRUCT_BEGIN(name, parent) struct name {
#define KERNEL_STRUCT_END(name) \
  } \
  ; \
  static_assert_align(name, 16);

#ifdef __KERNEL_USE_DATA_CONSTANTS__
#  define KERNEL_STRUCT_MEMBER(parent, type, name) type __unused_##name;
#else
#  define KERNEL_STRUCT_MEMBER(parent, type, name) type name;
#endif

#include "kernel/data_template.h"

typedef struct KernelTables {
  int filter_table_offset;
  int ggx_E;
  int ggx_Eavg;
  int ggx_glass_E;
  int ggx_glass_Eavg;
  int ggx_glass_inv_E;
  int ggx_glass_inv_Eavg;
  int pad1;
} KernelTables;
static_assert_align(KernelTables, 16);

typedef struct KernelBake {
  int use;
  int object_index;
  int tri_offset;
  int use_camera;
} KernelBake;
static_assert_align(KernelBake, 16);

typedef struct KernelLightLinkSet {
  uint light_tree_root;
} KernelLightLinkSet;

typedef struct KernelData {
  /* Features and limits. */
  uint kernel_features;
  uint max_closures;
  uint max_shaders;
  uint volume_stack_size;

  /* Always dynamic data members. */
  KernelCamera cam;
  KernelBake bake;
  KernelTables tables;
  KernelLightLinkSet light_link_sets[LIGHT_LINK_SET_MAX];

  /* Potentially specialized data members. */
#define KERNEL_STRUCT_BEGIN(name, parent) name parent;
#include "kernel/data_template.h"

  /* Device specific BVH. */
#ifdef __KERNEL_OPTIX__
  OptixTraversableHandle device_bvh;
#elif defined __METALRT__
  metalrt_as_type device_bvh;
#elif defined(__HIPRT__)
  void *device_bvh;
#else
#  ifdef __EMBREE__
  RTCScene device_bvh;
#    ifndef __KERNEL_64_BIT__
  int pad1;
#    endif
#  else
  int device_bvh, pad1;
#  endif
#endif
  int pad2, pad3;
} KernelData;
static_assert_align(KernelData, 16);

/* Kernel data structures. */

typedef struct KernelObject {
  Transform tfm;
  Transform itfm;

  float volume_density;
  float pass_id;
  float random_number;
  float color[3];
  float alpha;
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

  float shadow_terminator_shading_offset;
  float shadow_terminator_geometry_offset;

  float ao_distance;

  int lightgroup;

  uint visibility;
  int primitive_type;

  /* Volume velocity scale. */
  float velocity_scale;

  /* TODO: separate array to avoid memory overhead when not used. */
  uint64_t light_set_membership;
  uint receiver_light_set;
  uint64_t shadow_set_membership;
  uint blocker_shadow_set;
} KernelObject;
static_assert_align(KernelObject, 16);

typedef struct KernelCurve {
  int shader_id;
  int first_key;
  int num_keys;
  int type;
} KernelCurve;
static_assert_align(KernelCurve, 16);

typedef struct KernelCurveSegment {
  int prim;
  int type;
} KernelCurveSegment;
static_assert_align(KernelCurveSegment, 8);

typedef struct KernelSpotLight {
  packed_float3 scaled_axis_u;
  float radius;
  packed_float3 scaled_axis_v;
  float eval_fac;
  packed_float3 dir;
  float cos_half_spot_angle;
  float half_cot_half_spot_angle;
  float inv_len_z;
  float spot_smooth;
  float pad;
} KernelSpotLight;

/* PointLight is SpotLight with only radius and invarea being used. */

typedef struct KernelAreaLight {
  packed_float3 axis_u;
  float len_u;
  packed_float3 axis_v;
  float len_v;
  packed_float3 dir;
  float invarea;
  float tan_half_spread;
  float normalize_spread;
  float pad[2];
} KernelAreaLight;

typedef struct KernelDistantLight {
  float angle;
  float one_minus_cosangle;
  float half_inv_sin_half_angle;
  float pdf;
  float eval_fac;
  float pad[3];
} KernelDistantLight;

typedef struct KernelLight {
  int type;
  packed_float3 co;
  int shader_id;
  float max_bounces;
  float random;
  float strength[3];
  int use_caustics;
  int lightgroup;
  Transform tfm;
  Transform itfm;
  union {
    KernelSpotLight spot;
    KernelAreaLight area;
    KernelDistantLight distant;
  };
  uint64_t light_set_membership;
  uint64_t shadow_set_membership;
} KernelLight;
static_assert_align(KernelLight, 16);

using MeshLight = struct MeshLight {
  int shader_flag;
  int object_id;
};

typedef struct KernelLightDistribution {
  float totarea;
  int prim;
  MeshLight mesh_light;
} KernelLightDistribution;
static_assert_align(KernelLightDistribution, 16);

/* Bounding box. */
using BoundingBox = struct BoundingBox {
  packed_float3 min;
  packed_float3 max;
};

using BoundingCone = struct BoundingCone {
  packed_float3 axis;
  float theta_o;
  float theta_e;
};

enum LightTreeNodeType : uint8_t {
  LIGHT_TREE_INSTANCE = (1 << 0),
  LIGHT_TREE_INNER = (1 << 1),
  LIGHT_TREE_LEAF = (1 << 2),
  LIGHT_TREE_DISTANT = (1 << 3),
};

typedef struct KernelLightTreeNode {
  /* Bounding box. */
  BoundingBox bbox;

  /* Bounding cone. */
  BoundingCone bcone;

  /* Energy. */
  float energy;

  LightTreeNodeType type;

  /* Leaf nodes need to know the number of emitters stored. */
  int num_emitters;

  union {
    struct {
      int first_emitter; /* The index of the first emitter. */
    } leaf;
    struct {
      /* Indices of the children. */
      int left_child;
      int right_child;
    } inner;
    struct {
      int reference; /* A reference to the node with the subtree. */
    } instance;
  };

  /* Bit trail. */
  uint bit_trail;

  /* Bits to skip in the bit trail, to skip nodes in for specialized trees. */
  uint8_t bit_skip;

  /* Padding. */
  uint8_t pad[11];
} KernelLightTreeNode;
static_assert_align(KernelLightTreeNode, 16);

typedef struct KernelLightTreeEmitter {
  /* Bounding cone. */
  float theta_o;
  float theta_e;

  /* Energy. */
  float energy;

  union {
    struct {
      int id; /* The location in the triangles array. */
      EmissionSampling emission_sampling;
    } triangle;

    struct {
      int id; /* The location in the lights array. */
    } light;

    struct {
      int object_id;
      int node_id;
    } mesh;
  };

  MeshLight mesh_light;

  /* Bit trail from root node to leaf node containing emitter. */
  int bit_trail;
} KernelLightTreeEmitter;
static_assert_align(KernelLightTreeEmitter, 16);

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

/* Patches */

#define PATCH_MAX_CONTROL_VERTS 16

/* Patch map node flags */

#define PATCH_MAP_NODE_IS_SET (1 << 30)
#define PATCH_MAP_NODE_IS_LEAF (1u << 31)
#define PATCH_MAP_NODE_INDEX_MASK (~(PATCH_MAP_NODE_IS_SET | PATCH_MAP_NODE_IS_LEAF))

/* Work Tiles */

typedef struct KernelWorkTile {
  uint x, y, w, h;

  uint start_sample;
  uint num_samples;
  uint sample_offset;

  int offset;
  uint stride;

  /* Precalculated parameters used by init_from_camera kernel on GPU. */
  int path_index_offset;
  int work_size;
} KernelWorkTile;

/* Shader Evaluation.
 *
 * Position on a primitive on an object at which we want to evaluate the
 * shader for e.g. mesh displacement or light importance map. */

typedef struct KernelShaderEvalInput {
  int object;
  int prim;
  float u, v;
} KernelShaderEvalInput;
static_assert_align(KernelShaderEvalInput, 16);

/* Pre-computed sample table sizes for the tabulated Sobol sampler.
 *
 * NOTE: min and max samples *must* be a power of two, and patterns
 * ideally should be as well.
 */
#define MIN_TAB_SOBOL_SAMPLES 256
#define MAX_TAB_SOBOL_SAMPLES 8192
#define NUM_TAB_SOBOL_DIMENSIONS 4
#define NUM_TAB_SOBOL_PATTERNS 256

/* Device kernels.
 *
 * Identifier for kernels that can be executed in device queues.
 *
 * Some implementation details.
 *
 * If the kernel uses shared CUDA memory, `CUDADeviceQueue::enqueue` is to be modified.
 * The path iteration kernels are handled in `PathTraceWorkGPU::enqueue_path_iteration`. */

typedef enum DeviceKernel : int {
  DEVICE_KERNEL_INTEGRATOR_INIT_FROM_CAMERA = 0,
  DEVICE_KERNEL_INTEGRATOR_INIT_FROM_BAKE,
  DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST,
  DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW,
  DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE,
  DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK,
  DEVICE_KERNEL_INTEGRATOR_INTERSECT_DEDICATED_LIGHT,
  DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND,
  DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT,
  DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE,
  DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE,
  DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE,
  DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME,
  DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW,
  DEVICE_KERNEL_INTEGRATOR_SHADE_DEDICATED_LIGHT,
  DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL,

  DEVICE_KERNEL_INTEGRATOR_QUEUED_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_QUEUED_SHADOW_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_ACTIVE_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_TERMINATED_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_SORTED_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_SORT_BUCKET_PASS,
  DEVICE_KERNEL_INTEGRATOR_SORT_WRITE_PASS,
  DEVICE_KERNEL_INTEGRATOR_COMPACT_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_COMPACT_STATES,
  DEVICE_KERNEL_INTEGRATOR_TERMINATED_SHADOW_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_STATES,
  DEVICE_KERNEL_INTEGRATOR_RESET,
  DEVICE_KERNEL_INTEGRATOR_SHADOW_CATCHER_COUNT_POSSIBLE_SPLITS,

  DEVICE_KERNEL_SHADER_EVAL_DISPLACE,
  DEVICE_KERNEL_SHADER_EVAL_BACKGROUND,
  DEVICE_KERNEL_SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY,

#define DECLARE_FILM_CONVERT_KERNEL(variant) \
  DEVICE_KERNEL_FILM_CONVERT_##variant, DEVICE_KERNEL_FILM_CONVERT_##variant##_HALF_RGBA

  DECLARE_FILM_CONVERT_KERNEL(DEPTH),
  DECLARE_FILM_CONVERT_KERNEL(MIST),
  DECLARE_FILM_CONVERT_KERNEL(SAMPLE_COUNT),
  DECLARE_FILM_CONVERT_KERNEL(FLOAT),
  DECLARE_FILM_CONVERT_KERNEL(LIGHT_PATH),
  DECLARE_FILM_CONVERT_KERNEL(FLOAT3),
  DECLARE_FILM_CONVERT_KERNEL(MOTION),
  DECLARE_FILM_CONVERT_KERNEL(CRYPTOMATTE),
  DECLARE_FILM_CONVERT_KERNEL(SHADOW_CATCHER),
  DECLARE_FILM_CONVERT_KERNEL(SHADOW_CATCHER_MATTE_WITH_SHADOW),
  DECLARE_FILM_CONVERT_KERNEL(COMBINED),
  DECLARE_FILM_CONVERT_KERNEL(FLOAT4),

#undef DECLARE_FILM_CONVERT_KERNEL

  DEVICE_KERNEL_ADAPTIVE_SAMPLING_CONVERGENCE_CHECK,
  DEVICE_KERNEL_ADAPTIVE_SAMPLING_CONVERGENCE_FILTER_X,
  DEVICE_KERNEL_ADAPTIVE_SAMPLING_CONVERGENCE_FILTER_Y,

  DEVICE_KERNEL_FILTER_GUIDING_PREPROCESS,
  DEVICE_KERNEL_FILTER_GUIDING_SET_FAKE_ALBEDO,
  DEVICE_KERNEL_FILTER_COLOR_PREPROCESS,
  DEVICE_KERNEL_FILTER_COLOR_POSTPROCESS,

  DEVICE_KERNEL_CRYPTOMATTE_POSTPROCESS,

  DEVICE_KERNEL_PREFIX_SUM,

  DEVICE_KERNEL_NUM,
} DeviceKernel;

enum {
  DEVICE_KERNEL_INTEGRATOR_NUM = DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL + 1,
};

/* Kernel Features */

enum KernelFeatureFlag : uint32_t {
  /* Shader nodes. */
  KERNEL_FEATURE_NODE_BSDF = (1U << 0U),
  KERNEL_FEATURE_NODE_EMISSION = (1U << 1U),
  KERNEL_FEATURE_NODE_VOLUME = (1U << 2U),
  KERNEL_FEATURE_NODE_BUMP = (1U << 3U),
  KERNEL_FEATURE_NODE_BUMP_STATE = (1U << 4U),
  KERNEL_FEATURE_NODE_VORONOI_EXTRA = (1U << 5U),
  KERNEL_FEATURE_NODE_RAYTRACE = (1U << 6U),
  KERNEL_FEATURE_NODE_AOV = (1U << 7U),
  KERNEL_FEATURE_NODE_LIGHT_PATH = (1U << 8U),

  /* Use denoising kernels and output denoising passes. */
  KERNEL_FEATURE_DENOISING = (1U << 9U),

  /* Use path tracing kernels. */
  KERNEL_FEATURE_PATH_TRACING = (1U << 10U),

  /* BVH/sampling kernel features. */
  KERNEL_FEATURE_POINTCLOUD = (1U << 11U),
  KERNEL_FEATURE_HAIR = (1U << 12U),
  KERNEL_FEATURE_HAIR_THICK = (1U << 13U),
  KERNEL_FEATURE_OBJECT_MOTION = (1U << 14U),

  /* Denotes whether baking functionality is needed. */
  KERNEL_FEATURE_BAKING = (1U << 15U),

  /* Use subsurface scattering materials. */
  KERNEL_FEATURE_SUBSURFACE = (1U << 16U),

  /* Use volume materials. */
  KERNEL_FEATURE_VOLUME = (1U << 17U),

  /* Use OpenSubdiv patch evaluation */
  KERNEL_FEATURE_PATCH_EVALUATION = (1U << 18U),

  /* Use Transparent shadows */
  KERNEL_FEATURE_TRANSPARENT = (1U << 19U),

  /* Use shadow catcher. */
  KERNEL_FEATURE_SHADOW_CATCHER = (1U << 29U),

  /* Light render passes. */
  KERNEL_FEATURE_LIGHT_PASSES = (1U << 21U),

  /* AO. */
  KERNEL_FEATURE_AO_PASS = (1U << 22U),
  KERNEL_FEATURE_AO_ADDITIVE = (1U << 23U),
  KERNEL_FEATURE_AO = (KERNEL_FEATURE_AO_PASS | KERNEL_FEATURE_AO_ADDITIVE),

  /* MNEE. */
  KERNEL_FEATURE_MNEE = (1U << 24U),

  /* Path guiding. */
  KERNEL_FEATURE_PATH_GUIDING = (1U << 25U),

  /* OSL. */
  KERNEL_FEATURE_OSL = (1U << 26U),

  /* Light and shadow linking. */
  KERNEL_FEATURE_LIGHT_LINKING = (1U << 27U),
  KERNEL_FEATURE_SHADOW_LINKING = (1U << 28U),
};

/* Shader node feature mask, to specialize shader evaluation for kernels. */

#define KERNEL_FEATURE_NODE_MASK_SURFACE_LIGHT \
  (KERNEL_FEATURE_NODE_EMISSION | KERNEL_FEATURE_NODE_VORONOI_EXTRA | \
   KERNEL_FEATURE_NODE_LIGHT_PATH)
#define KERNEL_FEATURE_NODE_MASK_SURFACE_BACKGROUND \
  (KERNEL_FEATURE_NODE_MASK_SURFACE_LIGHT | KERNEL_FEATURE_NODE_AOV)
#define KERNEL_FEATURE_NODE_MASK_SURFACE_SHADOW \
  (KERNEL_FEATURE_NODE_BSDF | KERNEL_FEATURE_NODE_EMISSION | KERNEL_FEATURE_NODE_BUMP | \
   KERNEL_FEATURE_NODE_BUMP_STATE | KERNEL_FEATURE_NODE_VORONOI_EXTRA | \
   KERNEL_FEATURE_NODE_LIGHT_PATH)
#define KERNEL_FEATURE_NODE_MASK_SURFACE \
  (KERNEL_FEATURE_NODE_MASK_SURFACE_SHADOW | KERNEL_FEATURE_NODE_RAYTRACE | \
   KERNEL_FEATURE_NODE_AOV | KERNEL_FEATURE_NODE_LIGHT_PATH)
#define KERNEL_FEATURE_NODE_MASK_VOLUME \
  (KERNEL_FEATURE_NODE_EMISSION | KERNEL_FEATURE_NODE_VOLUME | \
   KERNEL_FEATURE_NODE_VORONOI_EXTRA | KERNEL_FEATURE_NODE_LIGHT_PATH)
#define KERNEL_FEATURE_NODE_MASK_DISPLACEMENT \
  (KERNEL_FEATURE_NODE_VORONOI_EXTRA | KERNEL_FEATURE_NODE_BUMP | KERNEL_FEATURE_NODE_BUMP_STATE)
#define KERNEL_FEATURE_NODE_MASK_BUMP KERNEL_FEATURE_NODE_MASK_DISPLACEMENT

/* Must be constexpr on the CPU to avoid compile errors because the state types
 * are different depending on the main, shadow or null path. For GPU we don't have
 * C++17 everywhere so need to check it. */
#if __cplusplus < 201703L
#  define IF_KERNEL_FEATURE(feature) if ((node_feature_mask & (KERNEL_FEATURE_##feature)) != 0U)
#  define IF_KERNEL_NODES_FEATURE(feature) \
    if ((node_feature_mask & (KERNEL_FEATURE_NODE_##feature)) != 0U)
#else
#  define IF_KERNEL_FEATURE(feature) \
    if constexpr ((node_feature_mask & (KERNEL_FEATURE_##feature)) != 0U)
#  define IF_KERNEL_NODES_FEATURE(feature) \
    if constexpr ((node_feature_mask & (KERNEL_FEATURE_NODE_##feature)) != 0U)
#endif

CCL_NAMESPACE_END
