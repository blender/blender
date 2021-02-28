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
#define PASS_UNUSED (~0)

/* Kernel features */
#define __SOBOL__
#define __DPDU__
#define __BACKGROUND__
#define __CAUSTICS_TRICKS__
#define __VISIBILITY_FLAG__
#define __RAY_DIFFERENTIALS__
#define __CAMERA_CLIPPING__
#define __INTERSECTION_REFINE__
#define __CLAMP_SAMPLE__
#define __PATCH_EVAL__
#define __SHADOW_CATCHER__
#define __DENOISING_FEATURES__
#define __SHADER_RAYTRACE__
#define __AO__
#define __PASSES__
#define __HAIR__
#define __SVM__
#define __EMISSION__
#define __HOLDOUT__
#define __TRANSPARENT_SHADOWS__
#define __BACKGROUND_MIS__
#define __LAMP_MIS__
#define __CAMERA_MOTION__
#define __OBJECT_MOTION__
#define __BAKING__
#define __PRINCIPLED__
#define __SUBSURFACE__
#define __VOLUME__
#define __CMJ__
#define __SHADOW_RECORD_ALL__
#define __BRANCHED_PATH__

/* Device specific features */
#ifdef __KERNEL_CPU__
#  ifdef WITH_OSL
#    define __OSL__
#  endif
#  define __VOLUME_RECORD_ALL__
#endif /* __KERNEL_CPU__ */

#ifdef __KERNEL_OPTIX__
#  undef __BAKING__
#endif /* __KERNEL_OPTIX__ */

/* Scene-based selective features compilation. */
#ifdef __KERNEL_FEATURES__
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_CAMERA_MOTION)
#    undef __CAMERA_MOTION__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_OBJECT_MOTION)
#    undef __OBJECT_MOTION__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_HAIR)
#    undef __HAIR__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_VOLUME)
#    undef __VOLUME__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_SUBSURFACE)
#    undef __SUBSURFACE__
#  endif
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_BAKING)
#    undef __BAKING__
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
#  if !(__KERNEL_FEATURES & KERNEL_FEATURE_PRINCIPLED)
#    undef __PRINCIPLED__
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
  SAMPLING_PATTERN_PMJ = 1,

  SAMPLING_NUM_PATTERNS,
};

/* These flags values correspond to `raytypes` in `osl.cpp`, so keep them in sync! */

enum PathRayFlag {
  /* --------------------------------------------------------------------
   * Ray visibility.
   *
   * NOTE: Recalculated after a surface bounce.
   */

  PATH_RAY_CAMERA = (1 << 0),
  PATH_RAY_REFLECT = (1 << 1),
  PATH_RAY_TRANSMIT = (1 << 2),
  PATH_RAY_DIFFUSE = (1 << 3),
  PATH_RAY_GLOSSY = (1 << 4),
  PATH_RAY_SINGULAR = (1 << 5),
  PATH_RAY_TRANSPARENT = (1 << 6),
  PATH_RAY_VOLUME_SCATTER = (1 << 7),

  /* Shadow ray visibility. */
  PATH_RAY_SHADOW_OPAQUE = (1 << 8),
  PATH_RAY_SHADOW_TRANSPARENT = (1 << 9),
  PATH_RAY_SHADOW = (PATH_RAY_SHADOW_OPAQUE | PATH_RAY_SHADOW_TRANSPARENT),

  /* Special flag to tag unaligned BVH nodes.
   * Only set and used in BVH nodes to distinguish how to interpret bounding box information stored
   * in the node (either it should be intersected as AABB or as OBB). */
  PATH_RAY_NODE_UNALIGNED = (1 << 10),

  /* Subset of flags used for ray visibility for intersection.
   *
   * NOTE: SHADOW_CATCHER macros below assume there are no more than
   * 16 visibility bits. */
  PATH_RAY_ALL_VISIBILITY = ((1 << 11) - 1),

  /* --------------------------------------------------------------------
   * Path flags.
   */

  /* Don't apply multiple importance sampling weights to emission from
   * lamp or surface hits, because they were not direct light sampled. */
  PATH_RAY_MIS_SKIP = (1 << 11),

  /* Diffuse bounce earlier in the path, skip SSS to improve performance
   * and avoid branching twice with disk sampling SSS. */
  PATH_RAY_DIFFUSE_ANCESTOR = (1 << 12),

  /* Single pass has been written. */
  PATH_RAY_SINGLE_PASS_DONE = (1 << 13),

  /* Zero background alpha, for camera or transparent glass rays. */
  PATH_RAY_TRANSPARENT_BACKGROUND = (1 << 14),

  /* Terminate ray immediately at next bounce. */
  PATH_RAY_TERMINATE_ON_NEXT_SURFACE = (1 << 15),
  PATH_RAY_TERMINATE_IN_NEXT_VOLUME = (1 << 16),

  /* Ray is to be terminated, but continue with transparent bounces and
   * emission as long as we encounter them. This is required to make the
   * MIS between direct and indirect light rays match, as shadow rays go
   * through transparent surfaces to reach emission too. */
  PATH_RAY_TERMINATE_AFTER_TRANSPARENT = (1 << 17),

  /* Terminate ray immediately after volume shading. */
  PATH_RAY_TERMINATE_AFTER_VOLUME = (1 << 18),

  /* Ray is to be terminated. */
  PATH_RAY_TERMINATE = (PATH_RAY_TERMINATE_ON_NEXT_SURFACE | PATH_RAY_TERMINATE_IN_NEXT_VOLUME |
                        PATH_RAY_TERMINATE_AFTER_TRANSPARENT | PATH_RAY_TERMINATE_AFTER_VOLUME),

  /* Path and shader is being evaluated for direct lighting emission. */
  PATH_RAY_EMISSION = (1 << 19),

  /* Perform subsurface scattering. */
  PATH_RAY_SUBSURFACE = (1 << 20),

  /* Contribute to denoising features. */
  PATH_RAY_DENOISING_FEATURES = (1 << 21),

  /* Render pass categories. */
  PATH_RAY_REFLECT_PASS = (1 << 22),
  PATH_RAY_TRANSMISSION_PASS = (1 << 23),
  PATH_RAY_VOLUME_PASS = (1 << 24),
  PATH_RAY_ANY_PASS = (PATH_RAY_REFLECT_PASS | PATH_RAY_TRANSMISSION_PASS | PATH_RAY_VOLUME_PASS),

  /* Shadow ray is for a light or surface. */
  PATH_RAY_SHADOW_FOR_LIGHT = (1 << 25),

  /* A shadow catcher object was hit and the path was split into two. */
  PATH_RAY_SHADOW_CATCHER_HIT = (1 << 26),

  /* A shadow catcher object was hit and this path traces only shadow catchers, writing them into
   * their dedicated pass for later division.
   *
   * NOTE: Is not covered with `PATH_RAY_ANY_PASS` because shadow catcher does special handling
   * which is separate from the light passes. */
  PATH_RAY_SHADOW_CATCHER_PASS = (1 << 27),

  /* Path is evaluating background for an approximate shadow catcher with non-transparent film. */
  PATH_RAY_SHADOW_CATCHER_BACKGROUND = (1 << 28),
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
  PASS_SHADOW,
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
  float3 diffuse;
  float3 glossy;
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
  SHADER_EXCLUDE_SHADOW_CATCHER = (1 << 22),
  SHADER_EXCLUDE_ANY = (SHADER_EXCLUDE_DIFFUSE | SHADER_EXCLUDE_GLOSSY | SHADER_EXCLUDE_TRANSMIT |
                        SHADER_EXCLUDE_CAMERA | SHADER_EXCLUDE_SCATTER |
                        SHADER_EXCLUDE_SHADOW_CATCHER),

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
  float3 P;   /* origin */
  float3 D;   /* direction */
  float t;    /* length of the ray */
  float time; /* time (for motion blur) */

#ifdef __RAY_DIFFERENTIALS__
  float dP;
  float dD;
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
} Intersection;

/* Primitives */

typedef enum PrimitiveType {
  PRIMITIVE_NONE = 0,
  PRIMITIVE_TRIANGLE = (1 << 0),
  PRIMITIVE_MOTION_TRIANGLE = (1 << 1),
  PRIMITIVE_CURVE_THICK = (1 << 2),
  PRIMITIVE_MOTION_CURVE_THICK = (1 << 3),
  PRIMITIVE_CURVE_RIBBON = (1 << 4),
  PRIMITIVE_MOTION_CURVE_RIBBON = (1 << 5),
  PRIMITIVE_VOLUME = (1 << 6),
  PRIMITIVE_LAMP = (1 << 7),

  PRIMITIVE_ALL_TRIANGLE = (PRIMITIVE_TRIANGLE | PRIMITIVE_MOTION_TRIANGLE),
  PRIMITIVE_ALL_CURVE = (PRIMITIVE_CURVE_THICK | PRIMITIVE_MOTION_CURVE_THICK |
                         PRIMITIVE_CURVE_RIBBON | PRIMITIVE_MOTION_CURVE_RIBBON),
  PRIMITIVE_ALL_VOLUME = (PRIMITIVE_VOLUME),
  PRIMITIVE_ALL_MOTION = (PRIMITIVE_MOTION_TRIANGLE | PRIMITIVE_MOTION_CURVE_THICK |
                          PRIMITIVE_MOTION_CURVE_RIBBON),
  PRIMITIVE_ALL = (PRIMITIVE_ALL_TRIANGLE | PRIMITIVE_ALL_CURVE | PRIMITIVE_ALL_VOLUME |
                   PRIMITIVE_LAMP),

  PRIMITIVE_NUM = 8,
} PrimitiveType;

#define PRIMITIVE_PACK_SEGMENT(type, segment) ((segment << PRIMITIVE_NUM) | (type))
#define PRIMITIVE_UNPACK_SEGMENT(type) (type >> PRIMITIVE_NUM)

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

#ifndef __MAX_CLOSURE__
#  define MAX_CLOSURE 64
#else
#  define MAX_CLOSURE __MAX_CLOSURE__
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
  /* Shader has emission */
  SD_HAS_EMISSION = (1 << 29),
  /* Shader has raytracing */
  SD_HAS_RAYTRACE = (1 << 30),

  SD_SHADER_FLAGS = (SD_USE_MIS | SD_HAS_TRANSPARENT_SHADOW | SD_HAS_VOLUME | SD_HAS_ONLY_VOLUME |
                     SD_HETEROGENEOUS_VOLUME | SD_HAS_BSSRDF_BUMP | SD_VOLUME_EQUIANGULAR |
                     SD_VOLUME_MIS | SD_VOLUME_CUBIC | SD_HAS_BUMP | SD_HAS_DISPLACEMENT |
                     SD_HAS_CONSTANT_EMISSION | SD_NEED_VOLUME_ATTRIBUTES | SD_HAS_EMISSION |
                     SD_HAS_RAYTRACE)
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
  /* Object <-> world space transformations for motion blur, cached to avoid
   * re-interpolating them constantly for shading. */
  Transform ob_tfm_motion;
  Transform ob_itfm_motion;
#endif

  /* ray start position, only set for backgrounds */
  float3 ray_P;
  float ray_dP;

#ifdef __OSL__
  const struct KernelGlobals *osl_globals;
  const struct IntegratorStateCPU *osl_path_state;
#endif

  /* LCG state for closures that require additional random numbers. */
  uint lcg_state;

  /* Closure data, we store a fixed array of closures */
  int num_closure;
  int num_closure_left;
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

/* Compact volume closures storage.
 *
 * Used for decoupled direct/indirect light closure storage. */

ccl_addr_space struct ShaderVolumeClosure {
  float3 weight;
  float sample_weight;
  float g;
};

ccl_addr_space struct ShaderVolumePhases {
  ShaderVolumeClosure closure[MAX_VOLUME_CLOSURE];
  int num_closure;
};

/* Volume Stack */

#ifdef __VOLUME__
typedef struct VolumeStack {
  int object;
  int shader;
} VolumeStack;
#endif

/* Struct to gather multiple nearby intersections. */
typedef struct LocalIntersection {
  Ray ray;
  float3 weight[LOCAL_MAX_HITS];

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
  int pad1;

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

  int pass_combined;
  int pass_depth;
  int pass_position;
  int pass_normal;
  int pass_roughness;
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

  int pass_shadow_catcher;
  int pass_shadow_catcher_sample_count;
  int pass_shadow_catcher_matte;

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

  int pass_denoising_normal;
  int pass_denoising_albedo;

  int pass_aov_color;
  int pass_aov_value;

  /* XYZ to rendering color space transform. float4 instead of float3 to
   * ensure consistent padding/alignment across devices. */
  float4 xyz_to_r;
  float4 xyz_to_g;
  float4 xyz_to_b;
  float4 rgb_to_y;

  int pass_bake_primitive;
  int pass_bake_differential;

  int use_approximate_shadow_catcher;

  int pad1, pad2, pad3;
} KernelFilm;
static_assert_align(KernelFilm, 16);

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

typedef struct KernelBackground {
  /* only shader index */
  int surface_shader;
  int volume_shader;
  float volume_step_size;
  int transparent;
  float transparent_roughness_squared_threshold;

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

  /* Padding */
  int pad1, pad2, pad3;
} KernelBackground;
static_assert_align(KernelBackground, 16);

typedef struct KernelIntegrator {
  /* emission */
  int use_direct_light;
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

  /* AO bounces */
  int ao_bounces;
  float ao_bounces_distance;
  float ao_bounces_factor;

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

  /* mis */
  int use_lamp_mis;

  /* sampler */
  int sampling_pattern;

  /* volume render */
  int use_volumes;
  int volume_max_steps;
  float volume_step_rate;

  int has_shadow_catcher;

  /* padding */
  int pad1, pad2;
} KernelIntegrator;
static_assert_align(KernelIntegrator, 16);

typedef enum KernelBVHLayout {
  BVH_LAYOUT_NONE = 0,

  BVH_LAYOUT_BVH2 = (1 << 0),
  BVH_LAYOUT_EMBREE = (1 << 1),
  BVH_LAYOUT_OPTIX = (1 << 2),
  BVH_LAYOUT_MULTI_OPTIX = (1 << 3),
  BVH_LAYOUT_MULTI_OPTIX_EMBREE = (1 << 4),

  /* Default BVH layout to use for CPU. */
  BVH_LAYOUT_AUTO = BVH_LAYOUT_EMBREE,
  BVH_LAYOUT_ALL = BVH_LAYOUT_BVH2 | BVH_LAYOUT_EMBREE | BVH_LAYOUT_OPTIX,
} KernelBVHLayout;

typedef struct KernelBVH {
  /* Own BVH */
  int root;
  int have_motion;
  int have_curves;
  int bvh_layout;
  int use_bvh_steps;
  int curve_subdivisions;

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

typedef struct KernelTables {
  int beckmann_offset;
  int pad1, pad2, pad3;
} KernelTables;
static_assert_align(KernelTables, 16);

typedef struct KernelBake {
  int use;
  int object_index;
  int tri_offset;
  int pad1;
} KernelBake;
static_assert_align(KernelBake, 16);

typedef struct KernelData {
  uint kernel_features;
  uint max_closures;
  uint max_shaders;
  uint volume_stack_size;

  KernelCamera cam;
  KernelFilm film;
  KernelBackground background;
  KernelIntegrator integrator;
  KernelBVH bvh;
  KernelTables tables;
  KernelBake bake;
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

  uint visibility;
  int primitive_type;
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
  float tan_spread;
  float dir[3];
  float normalize_spread;
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
  float max_bounces;
  float random;
  float strength[3];
  float pad1, pad2;
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

/* Pre-computed sample table sizes for PMJ02 sampler. */
#define NUM_PMJ_DIVISIONS 32
#define NUM_PMJ_SAMPLES ((NUM_PMJ_DIVISIONS) * (NUM_PMJ_DIVISIONS))
#define NUM_PMJ_PATTERNS 1

/* Device kernels.
 *
 * Identifier for kernels that can be executed in device queues.
 *
 * Some implementation details.
 *
 * If the kernel uses shared CUDA memory, `CUDADeviceQueue::enqueue` is to be modified.
 * The path iteration kernels are handled in `PathTraceWorkGPU::enqueue_path_iteration`. */

typedef enum DeviceKernel {
  DEVICE_KERNEL_INTEGRATOR_INIT_FROM_CAMERA = 0,
  DEVICE_KERNEL_INTEGRATOR_INIT_FROM_BAKE,
  DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST,
  DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW,
  DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE,
  DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK,
  DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND,
  DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT,
  DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE,
  DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE,
  DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME,
  DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW,
  DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL,

  DEVICE_KERNEL_INTEGRATOR_QUEUED_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_QUEUED_SHADOW_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_ACTIVE_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_TERMINATED_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_SORTED_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_COMPACT_PATHS_ARRAY,
  DEVICE_KERNEL_INTEGRATOR_COMPACT_STATES,
  DEVICE_KERNEL_INTEGRATOR_RESET,
  DEVICE_KERNEL_INTEGRATOR_SHADOW_CATCHER_COUNT_POSSIBLE_SPLITS,

  DEVICE_KERNEL_SHADER_EVAL_DISPLACE,
  DEVICE_KERNEL_SHADER_EVAL_BACKGROUND,

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

enum KernelFeatureFlag : unsigned int {
  /* Shader nodes. */
  KERNEL_FEATURE_NODE_BSDF = (1U << 0U),
  KERNEL_FEATURE_NODE_EMISSION = (1U << 1U),
  KERNEL_FEATURE_NODE_VOLUME = (1U << 2U),
  KERNEL_FEATURE_NODE_HAIR = (1U << 3U),
  KERNEL_FEATURE_NODE_BUMP = (1U << 4U),
  KERNEL_FEATURE_NODE_BUMP_STATE = (1U << 5U),
  KERNEL_FEATURE_NODE_VORONOI_EXTRA = (1U << 6U),
  KERNEL_FEATURE_NODE_RAYTRACE = (1U << 7U),

  /* Use denoising kernels and output denoising passes. */
  KERNEL_FEATURE_DENOISING = (1U << 8U),

  /* Use path tracing kernels. */
  KERNEL_FEATURE_PATH_TRACING = (1U << 9U),

  /* BVH/sampling kernel features. */
  KERNEL_FEATURE_HAIR = (1U << 10U),
  KERNEL_FEATURE_HAIR_THICK = (1U << 11U),
  KERNEL_FEATURE_OBJECT_MOTION = (1U << 12U),
  KERNEL_FEATURE_CAMERA_MOTION = (1U << 13U),

  /* Denotes whether baking functionality is needed. */
  KERNEL_FEATURE_BAKING = (1U << 14U),

  /* Use subsurface scattering materials. */
  KERNEL_FEATURE_SUBSURFACE = (1U << 15U),

  /* Use volume materials. */
  KERNEL_FEATURE_VOLUME = (1U << 16U),

  /* Use OpenSubdiv patch evaluation */
  KERNEL_FEATURE_PATCH_EVALUATION = (1U << 17U),

  /* Use Transparent shadows */
  KERNEL_FEATURE_TRANSPARENT = (1U << 18U),

  /* Use shadow catcher. */
  KERNEL_FEATURE_SHADOW_CATCHER = (1U << 19U),

  /* Per-uber shader usage flags. */
  KERNEL_FEATURE_PRINCIPLED = (1U << 20U),

  /* Light render passes. */
  KERNEL_FEATURE_LIGHT_PASSES = (1U << 21U),

  /* Shadow render pass. */
  KERNEL_FEATURE_SHADOW_PASS = (1U << 22U),
};

/* Shader node feature mask, to specialize shader evaluation for kernels. */

#define KERNEL_FEATURE_NODE_MASK_SURFACE_LIGHT \
  (KERNEL_FEATURE_NODE_EMISSION | KERNEL_FEATURE_NODE_VORONOI_EXTRA)
#define KERNEL_FEATURE_NODE_MASK_SURFACE_SHADOW \
  (KERNEL_FEATURE_NODE_BSDF | KERNEL_FEATURE_NODE_EMISSION | KERNEL_FEATURE_NODE_VOLUME | \
   KERNEL_FEATURE_NODE_HAIR | KERNEL_FEATURE_NODE_BUMP | KERNEL_FEATURE_NODE_BUMP_STATE | \
   KERNEL_FEATURE_NODE_VORONOI_EXTRA)
#define KERNEL_FEATURE_NODE_MASK_SURFACE \
  (KERNEL_FEATURE_NODE_MASK_SURFACE_SHADOW | KERNEL_FEATURE_NODE_RAYTRACE)
#define KERNEL_FEATURE_NODE_MASK_VOLUME \
  (KERNEL_FEATURE_NODE_EMISSION | KERNEL_FEATURE_NODE_VOLUME | KERNEL_FEATURE_NODE_VORONOI_EXTRA)
#define KERNEL_FEATURE_NODE_MASK_DISPLACEMENT \
  (KERNEL_FEATURE_NODE_VORONOI_EXTRA | KERNEL_FEATURE_NODE_BUMP | KERNEL_FEATURE_NODE_BUMP_STATE)
#define KERNEL_FEATURE_NODE_MASK_BUMP KERNEL_FEATURE_NODE_MASK_DISPLACEMENT

#define KERNEL_NODES_FEATURE(feature) ((node_feature_mask & (KERNEL_FEATURE_NODE_##feature)) != 0U)

CCL_NAMESPACE_END
