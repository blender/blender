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

#include "kernel_math.h"
#include "svm/svm_types.h"
#include "util_static_assert.h"

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

/* constants */
#define OBJECT_SIZE 		12
#define OBJECT_VECTOR_SIZE	6
#define LIGHT_SIZE		11
#define FILTER_TABLE_SIZE	1024
#define RAMP_TABLE_SIZE		256
#define SHUTTER_TABLE_SIZE		256
#define PARTICLE_SIZE 		5
#define SHADER_SIZE		5

#define BSSRDF_MIN_RADIUS			1e-8f
#define BSSRDF_MAX_HITS				4

#define BECKMANN_TABLE_SIZE		256

#define SHADER_NONE				(~0)
#define OBJECT_NONE				(~0)
#define PRIM_NONE				(~0)
#define LAMP_NONE				(~0)

#define VOLUME_STACK_SIZE		16

/* device capabilities */
#ifdef __KERNEL_CPU__
#  ifdef __KERNEL_SSE2__
#    define __QBVH__
#  endif
#  define __KERNEL_SHADING__
#  define __KERNEL_ADV_SHADING__
#  define __BRANCHED_PATH__
#  ifdef WITH_OSL
#    define __OSL__
#  endif
#  define __SUBSURFACE__
#  define __CMJ__
#  define __VOLUME__
#  define __VOLUME_DECOUPLED__
#  define __VOLUME_SCATTER__
#  define __SHADOW_RECORD_ALL__
#  define __VOLUME_RECORD_ALL__
#endif  /* __KERNEL_CPU__ */

#ifdef __KERNEL_CUDA__
#  define __KERNEL_SHADING__
#  define __KERNEL_ADV_SHADING__
#  define __BRANCHED_PATH__
#  define __VOLUME__
#  define __VOLUME_SCATTER__
#  define __SUBSURFACE__
#  define __CMJ__
#endif  /* __KERNEL_CUDA__ */

#ifdef __KERNEL_OPENCL__

/* keep __KERNEL_ADV_SHADING__ in sync with opencl_kernel_use_advanced_shading! */

#  ifdef __KERNEL_OPENCL_NVIDIA__
#    define __KERNEL_SHADING__
#    define __KERNEL_ADV_SHADING__
#    ifdef __KERNEL_EXPERIMENTAL__
#      define __CMJ__
#    endif
#  endif  /* __KERNEL_OPENCL_NVIDIA__ */

#  ifdef __KERNEL_OPENCL_APPLE__
#    define __KERNEL_SHADING__
#    define __KERNEL_ADV_SHADING__
/* TODO(sergey): Currently experimental section is ignored here,
 * this is because megakernel in device_opencl does not support
 * custom cflags depending on the scene features.
 */
#    ifdef __KERNEL_EXPERIMENTAL__
#      define __CMJ__
#    endif
#  endif  /* __KERNEL_OPENCL_NVIDIA__ */

#  ifdef __KERNEL_OPENCL_AMD__
#    define __CL_USE_NATIVE__
#    define __KERNEL_SHADING__
#    define __KERNEL_ADV_SHADING__
#  endif  /* __KERNEL_OPENCL_AMD__ */

#  ifdef __KERNEL_OPENCL_INTEL_CPU__
#    define __CL_USE_NATIVE__
#    define __KERNEL_SHADING__
#    define __KERNEL_ADV_SHADING__
#    ifdef __KERNEL_EXPERIMENTAL__
#      define __CMJ__
#    endif
#  endif  /* __KERNEL_OPENCL_INTEL_CPU__ */

#endif  /* __KERNEL_OPENCL__ */

/* kernel features */
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

#ifdef __KERNEL_SHADING__
#  define __SVM__
#  define __EMISSION__
#  define __TEXTURES__
#  define __EXTRA_NODES__
#  define __HOLDOUT__
#endif

#ifdef __KERNEL_ADV_SHADING__
#  define __MULTI_CLOSURE__
#  define __TRANSPARENT_SHADOWS__
#  define __PASSES__
#  define __BACKGROUND_MIS__
#  define __LAMP_MIS__
#  define __AO__
#  define __CAMERA_MOTION__
#  define __OBJECT_MOTION__
#  define __HAIR__
#  define __BAKING__
#endif

#ifdef WITH_CYCLES_DEBUG
#  define __KERNEL_DEBUG__
#endif

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

/* Random Numbers */

typedef uint RNG;

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
	SHADER_EVAL_DIFFUSE_COLOR,
	SHADER_EVAL_GLOSSY_COLOR,
	SHADER_EVAL_TRANSMISSION_COLOR,
	SHADER_EVAL_SUBSURFACE_COLOR,
	SHADER_EVAL_EMISSION,

	/* light passes */
	SHADER_EVAL_AO,
	SHADER_EVAL_COMBINED,
	SHADER_EVAL_SHADOW,
	SHADER_EVAL_DIFFUSE,
	SHADER_EVAL_GLOSSY,
	SHADER_EVAL_TRANSMISSION,
	SHADER_EVAL_SUBSURFACE,

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
#ifdef __CAMERA_MOTION__
	PRNG_TIME = 4,
	PRNG_UNUSED_0 = 5,
	PRNG_UNUSED_1 = 6,	/* for some reason (6, 7) is a bad sobol pattern */
	PRNG_UNUSED_2 = 7,  /* with a low number of samples (< 64) */
#endif
	PRNG_BASE_NUM = 8,

	PRNG_BSDF_U = 0,
	PRNG_BSDF_V = 1,
	PRNG_BSDF = 2,
	PRNG_LIGHT = 3,
	PRNG_LIGHT_U = 4,
	PRNG_LIGHT_V = 5,
	PRNG_LIGHT_TERMINATE = 6,
	PRNG_TERMINATE = 7,

#ifdef __VOLUME__
	PRNG_PHASE_U = 8,
	PRNG_PHASE_V = 9,
	PRNG_PHASE = 10,
	PRNG_SCATTER_DISTANCE = 11,
#endif

	PRNG_BOUNCE_NUM = 12,
};

enum SamplingPattern {
	SAMPLING_PATTERN_SOBOL = 0,
	SAMPLING_PATTERN_CMJ = 1,

	SAMPLING_NUM_PATTERNS,
};

/* these flags values correspond to raytypes in osl.cpp, so keep them in sync! */

enum PathRayFlag {
	PATH_RAY_CAMERA = 1,
	PATH_RAY_REFLECT = 2,
	PATH_RAY_TRANSMIT = 4,
	PATH_RAY_DIFFUSE = 8,
	PATH_RAY_GLOSSY = 16,
	PATH_RAY_SINGULAR = 32,
	PATH_RAY_TRANSPARENT = 64,

	PATH_RAY_SHADOW_OPAQUE = 128,
	PATH_RAY_SHADOW_TRANSPARENT = 256,
	PATH_RAY_SHADOW = (PATH_RAY_SHADOW_OPAQUE|PATH_RAY_SHADOW_TRANSPARENT),

	PATH_RAY_CURVE = 512, /* visibility flag to define curve segments */
	PATH_RAY_VOLUME_SCATTER = 1024, /* volume scattering */

	/* Special flag to tag unaligned BVH nodes. */
	PATH_RAY_NODE_UNALIGNED = 2048,

	PATH_RAY_ALL_VISIBILITY = (1|2|4|8|16|32|64|128|256|512|1024|2048),

	PATH_RAY_MIS_SKIP = 4096,
	PATH_RAY_DIFFUSE_ANCESTOR = 8192,
	PATH_RAY_SINGLE_PASS_DONE = 16384,
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
} ClosureLabel;

/* Render Passes */

typedef enum PassType {
	PASS_NONE = 0,
	PASS_COMBINED = (1 << 0),
	PASS_DEPTH = (1 << 1),
	PASS_NORMAL = (1 << 2),
	PASS_UV = (1 << 3),
	PASS_OBJECT_ID = (1 << 4),
	PASS_MATERIAL_ID = (1 << 5),
	PASS_DIFFUSE_COLOR = (1 << 6),
	PASS_GLOSSY_COLOR = (1 << 7),
	PASS_TRANSMISSION_COLOR = (1 << 8),
	PASS_DIFFUSE_INDIRECT = (1 << 9),
	PASS_GLOSSY_INDIRECT = (1 << 10),
	PASS_TRANSMISSION_INDIRECT = (1 << 11),
	PASS_DIFFUSE_DIRECT = (1 << 12),
	PASS_GLOSSY_DIRECT = (1 << 13),
	PASS_TRANSMISSION_DIRECT = (1 << 14),
	PASS_EMISSION = (1 << 15),
	PASS_BACKGROUND = (1 << 16),
	PASS_AO = (1 << 17),
	PASS_SHADOW = (1 << 18),
	PASS_MOTION = (1 << 19),
	PASS_MOTION_WEIGHT = (1 << 20),
	PASS_MIST = (1 << 21),
	PASS_SUBSURFACE_DIRECT = (1 << 22),
	PASS_SUBSURFACE_INDIRECT = (1 << 23),
	PASS_SUBSURFACE_COLOR = (1 << 24),
	PASS_LIGHT = (1 << 25), /* no real pass, used to force use_light_pass */
#ifdef __KERNEL_DEBUG__
	PASS_BVH_TRAVERSED_NODES = (1 << 26),
	PASS_BVH_TRAVERSED_INSTANCES = (1 << 27),
	PASS_BVH_INTERSECTIONS = (1 << 28),
	PASS_RAY_BOUNCES = (1 << 29),
#endif
} PassType;

#define PASS_ALL (~0)

typedef enum BakePassFilter {
	BAKE_FILTER_NONE = 0,
	BAKE_FILTER_DIRECT = (1 << 0),
	BAKE_FILTER_INDIRECT = (1 << 1),
	BAKE_FILTER_COLOR = (1 << 2),
	BAKE_FILTER_DIFFUSE = (1 << 3),
	BAKE_FILTER_GLOSSY = (1 << 4),
	BAKE_FILTER_TRANSMISSION = (1 << 5),
	BAKE_FILTER_SUBSURFACE = (1 << 6),
	BAKE_FILTER_EMISSION = (1 << 7),
	BAKE_FILTER_AO = (1 << 8),
} BakePassFilter;

typedef enum BakePassFilterCombos {
	BAKE_FILTER_COMBINED = (
	    BAKE_FILTER_DIRECT |
	    BAKE_FILTER_INDIRECT |
	    BAKE_FILTER_DIFFUSE |
	    BAKE_FILTER_GLOSSY |
	    BAKE_FILTER_TRANSMISSION |
	    BAKE_FILTER_SUBSURFACE |
	    BAKE_FILTER_EMISSION |
	    BAKE_FILTER_AO),
	BAKE_FILTER_DIFFUSE_DIRECT = (BAKE_FILTER_DIRECT | BAKE_FILTER_DIFFUSE),
	BAKE_FILTER_GLOSSY_DIRECT = (BAKE_FILTER_DIRECT | BAKE_FILTER_GLOSSY),
	BAKE_FILTER_TRANSMISSION_DIRECT = (BAKE_FILTER_DIRECT | BAKE_FILTER_TRANSMISSION),
	BAKE_FILTER_SUBSURFACE_DIRECT = (BAKE_FILTER_DIRECT | BAKE_FILTER_SUBSURFACE),
	BAKE_FILTER_DIFFUSE_INDIRECT = (BAKE_FILTER_INDIRECT | BAKE_FILTER_DIFFUSE),
	BAKE_FILTER_GLOSSY_INDIRECT = (BAKE_FILTER_INDIRECT | BAKE_FILTER_GLOSSY),
	BAKE_FILTER_TRANSMISSION_INDIRECT = (BAKE_FILTER_INDIRECT | BAKE_FILTER_TRANSMISSION),
	BAKE_FILTER_SUBSURFACE_INDIRECT = (BAKE_FILTER_INDIRECT | BAKE_FILTER_SUBSURFACE),
} BakePassFilterCombos;

typedef ccl_addr_space struct PathRadiance {
#ifdef __PASSES__
	int use_light_pass;
#endif

	float3 emission;
#ifdef __PASSES__
	float3 background;
	float3 ao;

	float3 indirect;
	float3 direct_throughput;
	float3 direct_emission;

	float3 color_diffuse;
	float3 color_glossy;
	float3 color_transmission;
	float3 color_subsurface;
	float3 color_scatter;

	float3 direct_diffuse;
	float3 direct_glossy;
	float3 direct_transmission;
	float3 direct_subsurface;
	float3 direct_scatter;

	float3 indirect_diffuse;
	float3 indirect_glossy;
	float3 indirect_transmission;
	float3 indirect_subsurface;
	float3 indirect_scatter;

	float3 path_diffuse;
	float3 path_glossy;
	float3 path_transmission;
	float3 path_subsurface;
	float3 path_scatter;

	float4 shadow;
	float mist;
#endif
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
	float3 subsurface;
	float3 scatter;
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
	SHADER_EXCLUDE_ANY = (SHADER_EXCLUDE_DIFFUSE|SHADER_EXCLUDE_GLOSSY|SHADER_EXCLUDE_TRANSMIT|SHADER_EXCLUDE_CAMERA|SHADER_EXCLUDE_SCATTER),

	SHADER_MASK = ~(SHADER_SMOOTH_NORMAL|SHADER_CAST_SHADOW|SHADER_AREA_LIGHT|SHADER_USE_MIS|SHADER_EXCLUDE_ANY)
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

enum CameraType {
	CAMERA_PERSPECTIVE,
	CAMERA_ORTHOGRAPHIC,
	CAMERA_PANORAMA
};

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
	float3 P;		/* origin */
	float3 D;		/* direction */

	float t;		/* length of the ray */
	float time;		/* time (for motion blur) */
#else
	float t;		/* length of the ray */
	float time;		/* time (for motion blur) */
	float3 P;		/* origin */
	float3 D;		/* direction */
#endif

#ifdef __RAY_DIFFERENTIALS__
	differential3 dP;
	differential3 dD;
#endif
} Ray;

/* Intersection */

typedef ccl_addr_space struct Intersection {
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
	PRIMITIVE_NONE            = 0,
	PRIMITIVE_TRIANGLE        = (1 << 0),
	PRIMITIVE_MOTION_TRIANGLE = (1 << 1),
	PRIMITIVE_CURVE           = (1 << 2),
	PRIMITIVE_MOTION_CURVE    = (1 << 3),
	/* Lamp primitive is not included below on purpose,
	 * since it is no real traceable primitive.
	 */
	PRIMITIVE_LAMP            = (1 << 4),

	PRIMITIVE_ALL_TRIANGLE = (PRIMITIVE_TRIANGLE|PRIMITIVE_MOTION_TRIANGLE),
	PRIMITIVE_ALL_CURVE = (PRIMITIVE_CURVE|PRIMITIVE_MOTION_CURVE),
	PRIMITIVE_ALL_MOTION = (PRIMITIVE_MOTION_TRIANGLE|PRIMITIVE_MOTION_CURVE),
	PRIMITIVE_ALL = (PRIMITIVE_ALL_TRIANGLE|PRIMITIVE_ALL_CURVE),

	/* Total number of different traceable primitives.
	 * NOTE: This is an actual value, not a bitflag.
	 */
	PRIMITIVE_NUM_TOTAL = 4,
} PrimitiveType;

#define PRIMITIVE_PACK_SEGMENT(type, segment) ((segment << PRIMITIVE_NUM_TOTAL) | (type))
#define PRIMITIVE_UNPACK_SEGMENT(type) (type >> PRIMITIVE_NUM_TOTAL)

/* Attributes */

typedef enum AttributePrimitive {
	ATTR_PRIM_TRIANGLE = 0,
	ATTR_PRIM_CURVE,
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
	ATTR_STD_GENERATED,
	ATTR_STD_GENERATED_TRANSFORM,
	ATTR_STD_POSITION_UNDEFORMED,
	ATTR_STD_POSITION_UNDISPLACED,
	ATTR_STD_MOTION_VERTEX_POSITION,
	ATTR_STD_MOTION_VERTEX_NORMAL,
	ATTR_STD_PARTICLE,
	ATTR_STD_CURVE_INTERCEPT,
	ATTR_STD_PTEX_FACE_ID,
	ATTR_STD_PTEX_UV,
	ATTR_STD_VOLUME_DENSITY,
	ATTR_STD_VOLUME_COLOR,
	ATTR_STD_VOLUME_FLAME,
	ATTR_STD_VOLUME_HEAT,
	ATTR_STD_VOLUME_VELOCITY,
	ATTR_STD_POINTINESS,
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
#  ifndef __MAX_CLOSURE__
#     define MAX_CLOSURE 64
#  else
#    define MAX_CLOSURE __MAX_CLOSURE__
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
 * We pad the struct to 80 bytes and ensure it is aligned to 16 bytes, which
 * we assume to be the maximum required alignment for any struct. */

#define SHADER_CLOSURE_BASE \
	float3 weight; \
	ClosureType type; \
	float sample_weight \

typedef ccl_addr_space struct ccl_align(16) ShaderClosure {
	SHADER_CLOSURE_BASE;

	float data[14]; /* pad to 80 bytes */
} ShaderClosure;

/* Shader Context
 *
 * For OSL we recycle a fixed number of contexts for speed */

typedef enum ShaderContext {
	SHADER_CONTEXT_MAIN = 0,
	SHADER_CONTEXT_INDIRECT = 1,
	SHADER_CONTEXT_EMISSION = 2,
	SHADER_CONTEXT_SHADOW = 3,
	SHADER_CONTEXT_SSS = 4,
	SHADER_CONTEXT_VOLUME = 5,
	SHADER_CONTEXT_NUM = 6
} ShaderContext;

/* Shader Data
 *
 * Main shader state at a point on the surface or in a volume. All coordinates
 * are in world space. */

enum ShaderDataFlag {
	/* runtime flags */
	SD_BACKFACING      = (1 << 0),   /* backside of surface? */
	SD_EMISSION        = (1 << 1),   /* have emissive closure? */
	SD_BSDF            = (1 << 2),   /* have bsdf closure? */
	SD_BSDF_HAS_EVAL   = (1 << 3),   /* have non-singular bsdf closure? */
	SD_BSSRDF          = (1 << 4),   /* have bssrdf */
	SD_HOLDOUT         = (1 << 5),   /* have holdout closure? */
	SD_ABSORPTION      = (1 << 6),   /* have volume absorption closure? */
	SD_SCATTER         = (1 << 7),   /* have volume phase closure? */
	SD_AO              = (1 << 8),   /* have ao closure? */
	SD_TRANSPARENT     = (1 << 9),  /* have transparent closure? */
	SD_BSDF_NEEDS_LCG  = (1 << 10),

	SD_CLOSURE_FLAGS = (SD_EMISSION|SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSSRDF|
	                    SD_HOLDOUT|SD_ABSORPTION|SD_SCATTER|SD_AO|
	                    SD_BSDF_NEEDS_LCG),

	/* shader flags */
	SD_USE_MIS                = (1 << 12),  /* direct light sample */
	SD_HAS_TRANSPARENT_SHADOW = (1 << 13),  /* has transparent shadow */
	SD_HAS_VOLUME             = (1 << 14),  /* has volume shader */
	SD_HAS_ONLY_VOLUME        = (1 << 15),  /* has only volume shader, no surface */
	SD_HETEROGENEOUS_VOLUME   = (1 << 16),  /* has heterogeneous volume */
	SD_HAS_BSSRDF_BUMP        = (1 << 17),  /* bssrdf normal uses bump */
	SD_VOLUME_EQUIANGULAR     = (1 << 18),  /* use equiangular sampling */
	SD_VOLUME_MIS             = (1 << 19),  /* use multiple importance sampling */
	SD_VOLUME_CUBIC           = (1 << 20),  /* use cubic interpolation for voxels */
	SD_HAS_BUMP               = (1 << 21),  /* has data connected to the displacement input */
	SD_HAS_DISPLACEMENT       = (1 << 22),  /* has true displacement */
	SD_HAS_CONSTANT_EMISSION  = (1 << 23),  /* has constant emission (value stored in __shader_flag) */

	SD_SHADER_FLAGS = (SD_USE_MIS|SD_HAS_TRANSPARENT_SHADOW|SD_HAS_VOLUME|
	                   SD_HAS_ONLY_VOLUME|SD_HETEROGENEOUS_VOLUME|
	                   SD_HAS_BSSRDF_BUMP|SD_VOLUME_EQUIANGULAR|SD_VOLUME_MIS|
	                   SD_VOLUME_CUBIC|SD_HAS_BUMP|SD_HAS_DISPLACEMENT|SD_HAS_CONSTANT_EMISSION),

	/* object flags */
	SD_HOLDOUT_MASK             = (1 << 24),  /* holdout for camera rays */
	SD_OBJECT_MOTION            = (1 << 25),  /* has object motion blur */
	SD_TRANSFORM_APPLIED        = (1 << 26),  /* vertices have transform applied */
	SD_NEGATIVE_SCALE_APPLIED   = (1 << 27),  /* vertices have negative scale applied */
	SD_OBJECT_HAS_VOLUME        = (1 << 28),  /* object has a volume shader */
	SD_OBJECT_INTERSECTS_VOLUME = (1 << 29),  /* object intersects AABB of an object with volume shader */
	SD_OBJECT_HAS_VERTEX_MOTION = (1 << 30),  /* has position for motion vertices */

	SD_OBJECT_FLAGS = (SD_HOLDOUT_MASK|SD_OBJECT_MOTION|SD_TRANSFORM_APPLIED|
	                   SD_NEGATIVE_SCALE_APPLIED|SD_OBJECT_HAS_VOLUME|
	                   SD_OBJECT_INTERSECTS_VOLUME)
};

#ifdef __SPLIT_KERNEL__
#  define SD_THREAD (get_global_id(1) * get_global_size(0) + get_global_id(0))
#  if !defined(__SPLIT_KERNEL_SOA__)
     /* ShaderData is stored as an Array-of-Structures */
#    define ccl_soa_member(type, name) type soa_##name
#    define ccl_fetch(s, t) (s[SD_THREAD].soa_##t)
#    define ccl_fetch_array(s, t, index) (&s[SD_THREAD].soa_##t[index])
#  else
     /* ShaderData is stored as an Structure-of-Arrays */
#    define SD_GLOBAL_SIZE (get_global_size(0) * get_global_size(1))
#    define SD_FIELD_SIZE(t) sizeof(((struct ShaderData*)0)->t)
#    define SD_OFFSETOF(t) ((char*)(&((struct ShaderData*)0)->t) - (char*)0)
#    define ccl_soa_member(type, name) type soa_##name
#    define ccl_fetch(s, t) (((ShaderData*)((ccl_addr_space char*)s + SD_GLOBAL_SIZE * SD_OFFSETOF(soa_##t) +  SD_FIELD_SIZE(soa_##t) * SD_THREAD - SD_OFFSETOF(soa_##t)))->soa_##t)
#    define ccl_fetch_array(s, t, index) (&ccl_fetch(s, t)[index])
#  endif
#else
#  define ccl_soa_member(type, name) type name
#  define ccl_fetch(s, t) (s->t)
#  define ccl_fetch_array(s, t, index) (&s->t[index])
#endif

typedef ccl_addr_space struct ShaderData {
	/* position */
	ccl_soa_member(float3, P);
	/* smooth normal for shading */
	ccl_soa_member(float3, N);
	/* true geometric normal */
	ccl_soa_member(float3, Ng);
	/* view/incoming direction */
	ccl_soa_member(float3, I);
	/* shader id */
	ccl_soa_member(int, shader);
	/* booleans describing shader, see ShaderDataFlag */
	ccl_soa_member(int, flag);

	/* primitive id if there is one, ~0 otherwise */
	ccl_soa_member(int, prim);

	/* combined type and curve segment for hair */
	ccl_soa_member(int, type);

	/* parametric coordinates
	 * - barycentric weights for triangles */
	ccl_soa_member(float, u);
	ccl_soa_member(float, v);
	/* object id if there is one, ~0 otherwise */
	ccl_soa_member(int, object);

	/* motion blur sample time */
	ccl_soa_member(float, time);

	/* length of the ray being shaded */
	ccl_soa_member(float, ray_length);

#ifdef __RAY_DIFFERENTIALS__
	/* differential of P. these are orthogonal to Ng, not N */
	ccl_soa_member(differential3, dP);
	/* differential of I */
	ccl_soa_member(differential3, dI);
	/* differential of u, v */
	ccl_soa_member(differential, du);
	ccl_soa_member(differential, dv);
#endif
#ifdef __DPDU__
	/* differential of P w.r.t. parametric coordinates. note that dPdu is
	 * not readily suitable as a tangent for shading on triangles. */
	ccl_soa_member(float3, dPdu);
	ccl_soa_member(float3, dPdv);
#endif

#ifdef __OBJECT_MOTION__
	/* object <-> world space transformations, cached to avoid
	 * re-interpolating them constantly for shading */
	ccl_soa_member(Transform, ob_tfm);
	ccl_soa_member(Transform, ob_itfm);
#endif

	/* Closure data, we store a fixed array of closures */
	ccl_soa_member(struct ShaderClosure, closure[MAX_CLOSURE]);
	ccl_soa_member(int, num_closure);
	ccl_soa_member(int, num_closure_extra);
	ccl_soa_member(float, randb_closure);
	ccl_soa_member(float3, svm_closure_weight);

	/* LCG state for closures that require additional random numbers. */
	ccl_soa_member(uint, lcg_state);

	/* ray start position, only set for backgrounds */
	ccl_soa_member(float3, ray_P);
	ccl_soa_member(differential3, ray_dP);

#ifdef __OSL__
	struct KernelGlobals *osl_globals;
	struct PathState *osl_path_state;
#endif
} ShaderData;

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
	int rng_offset;    		/* dimension offset */
	int sample;        		/* path sample number */
	int num_samples;		/* total number of times this path will be sampled */

	/* bounce counting */
	int bounce;
	int diffuse_bounce;
	int glossy_bounce;
	int transmission_bounce;
	int transparent_bounce;

	/* multiple importance sampling */
	float min_ray_pdf; /* smallest bounce pdf over entire path up to now */
	float ray_pdf;     /* last bounce pdf */
#ifdef __LAMP_MIS__
	float ray_t;       /* accumulated distance through transparent surfaces */
#endif

	/* volume rendering */
#ifdef __VOLUME__
	int volume_bounce;
	RNG rng_congruential;
	VolumeStack volume_stack[VOLUME_STACK_SIZE];
#endif
} PathState;

/* Subsurface */

/* Struct to gather multiple SSS hits. */
struct SubsurfaceIntersection
{
	Ray ray;
	float3 weight[BSSRDF_MAX_HITS];

	int num_hits;
	struct Intersection hits[BSSRDF_MAX_HITS];
	float3 Ng[BSSRDF_MAX_HITS];
};

/* Struct to gather SSS indirect rays and delay tracing them. */
struct SubsurfaceIndirectRays
{
	bool need_update_volume_stack;
	bool tracing;
	PathState state[BSSRDF_MAX_HITS];
	struct PathRadiance direct_L;

	int num_rays;
	struct Ray rays[BSSRDF_MAX_HITS];
	float3 throughputs[BSSRDF_MAX_HITS];
	struct PathRadiance L[BSSRDF_MAX_HITS];
};

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
	Transform rastertocamera;

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
	int have_motion, have_perspective_motion;

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
	Transform screentoworld;
	Transform rastertoworld;
	/* work around cuda sm 2.0 crash, this seems to
	 * cross some limit in combination with motion 
	 * Transform ndctoworld; */
	Transform worldtoscreen;
	Transform worldtoraster;
	Transform worldtondc;
	Transform worldtocamera;

	MotionTransform motion;

	/* Denotes changes in the projective matrix, namely in rastertocamera.
	 * Used for camera zoom motion blur,
	 */
	PerspectiveMotionTransform perspective_motion;

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
	int pass_subsurface_color;
	
	int pass_diffuse_indirect;
	int pass_glossy_indirect;
	int pass_transmission_indirect;
	int pass_subsurface_indirect;
	
	int pass_diffuse_direct;
	int pass_glossy_direct;
	int pass_transmission_direct;
	int pass_subsurface_direct;
	
	int pass_emission;
	int pass_background;
	int pass_ao;
	float pass_alpha_threshold;

	int pass_shadow;
	float pass_shadow_scale;
	int filter_table_offset;
	int pass_pad2;

	int pass_mist;
	float mist_start;
	float mist_inv_depth;
	float mist_falloff;

#ifdef __KERNEL_DEBUG__
	int pass_bvh_traversed_nodes;
	int pass_bvh_traversed_instances;
	int pass_bvh_intersections;
	int pass_ray_bounces;
#endif
} KernelFilm;
static_assert_align(KernelFilm, 16);

typedef struct KernelBackground {
	/* only shader index */
	int surface_shader;
	int volume_shader;
	int transparent;
	int pad;

	/* ambient occlusion */
	float ao_factor;
	float ao_distance;
	float ao_pad1, ao_pad2;
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
	float inv_pdf_lights;
	int pdf_background_res;

	/* light portals */
	float portal_pdf;
	int num_portals;
	int portal_offset;

	/* bounces */
	int min_bounce;
	int max_bounce;

	int max_diffuse_bounce;
	int max_glossy_bounce;
	int max_transmission_bounce;
	int max_volume_bounce;

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

	/* volume render */
	int use_volumes;
	int volume_max_steps;
	float volume_step_size;
	int volume_samples;

	float light_inv_rr_threshold;

	int start_sample;
} KernelIntegrator;
static_assert_align(KernelIntegrator, 16);

typedef struct KernelBVH {
	/* root node */
	int root;
	int attributes_map_stride;
	int have_motion;
	int have_curves;
	int have_instancing;
	int use_qbvh;
	int use_bvh_steps;
	int pad1;
} KernelBVH;
static_assert_align(KernelBVH, 16);

typedef enum CurveFlag {
	/* runtime flags */
	CURVE_KN_BACKFACING = 1,				/* backside of cylinder? */
	CURVE_KN_ENCLOSEFILTER = 2,				/* don't consider strands surrounding start point? */
	CURVE_KN_INTERPOLATE = 4,				/* render as a curve? */
	CURVE_KN_ACCURATE = 8,					/* use accurate intersections test? */
	CURVE_KN_INTERSECTCORRECTION = 16,		/* correct for width after determing closest midpoint? */
	CURVE_KN_TRUETANGENTGNORMAL = 32,		/* use tangent normal for geometry? */
	CURVE_KN_RIBBONS = 64,					/* use flat curve ribbons */
} CurveFlag;

typedef struct KernelCurves {
	int curveflags;
	int subdivisions;

	float minimum_width;
	float maximum_width;
} KernelCurves;
static_assert_align(KernelCurves, 16);

typedef struct KernelTables {
	int beckmann_offset;
	int pad1, pad2, pad3;
} KernelTables;
static_assert_align(KernelTables, 16);

typedef struct KernelData {
	KernelCamera cam;
	KernelFilm film;
	KernelBackground background;
	KernelIntegrator integrator;
	KernelBVH bvh;
	KernelCurves curve;
	KernelTables tables;
} KernelData;
static_assert_align(KernelData, 16);

#ifdef __KERNEL_DEBUG__
/* NOTE: This is a runtime-only struct, alignment is not
 * really important here.
 */
typedef ccl_addr_space struct DebugData {
	int num_bvh_traversed_nodes;
	int num_bvh_traversed_instances;
	int num_bvh_intersections;
	int num_ray_bounces;
} DebugData;
#endif

/* Declarations required for split kernel */

/* Macro for queues */
/* Value marking queue's empty slot */
#define QUEUE_EMPTY_SLOT -1

/*
 * Queue 1 - Active rays
 * Queue 2 - Background queue
 * Queue 3 - Shadow ray cast kernel - AO
 * Queeu 4 - Shadow ray cast kernel - direct lighting
 */
#define NUM_QUEUES 4

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
	QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS = 1,

	/* All rays for which a shadow ray should be cast to determine radiance
	 * contribution for AO are enqueued here.
	 */
	QUEUE_SHADOW_RAY_CAST_AO_RAYS = 2,

	/* All rays for which a shadow ray should be cast to determine radiance
	 * contributing for direct lighting are enqueued here.
	 */
	QUEUE_SHADOW_RAY_CAST_DL_RAYS = 3,
};

/* We use RAY_STATE_MASK to get ray_state (enums 0 to 5) */
#define RAY_STATE_MASK 0x007
#define RAY_FLAG_MASK 0x0F8
enum RayState {
	/* Denotes ray is actively involved in path-iteration. */
	RAY_ACTIVE = 0,
	/* Denotes ray has completed processing all samples and is inactive. */
	RAY_INACTIVE = 1,
	/* Denoted ray has exited path-iteration and needs to update output buffer. */
	RAY_UPDATE_BUFFER = 2,
	/* Donotes ray has hit background */
	RAY_HIT_BACKGROUND = 3,
	/* Denotes ray has to be regenerated */
	RAY_TO_REGENERATE = 4,
	/* Denotes ray has been regenerated */
	RAY_REGENERATED = 5,
	/* Denotes ray should skip direct lighting */
	RAY_SKIP_DL = 6,
	/* Flag's ray has to execute shadow blocked function in AO part */
	RAY_SHADOW_RAY_CAST_AO = 16,
	/* Flag's ray has to execute shadow blocked function in direct lighting part. */
	RAY_SHADOW_RAY_CAST_DL = 32,
};

#define ASSIGN_RAY_STATE(ray_state, ray_index, state) (ray_state[ray_index] = ((ray_state[ray_index] & RAY_FLAG_MASK) | state))
#define IS_STATE(ray_state, ray_index, state) ((ray_state[ray_index] & RAY_STATE_MASK) == state)
#define ADD_RAY_FLAG(ray_state, ray_index, flag) (ray_state[ray_index] = (ray_state[ray_index] | flag))
#define REMOVE_RAY_FLAG(ray_state, ray_index, flag) (ray_state[ray_index] = (ray_state[ray_index] & (~flag)))
#define IS_FLAG(ray_state, ray_index, flag) (ray_state[ray_index] & flag)

/* Patches */

#define PATCH_MAX_CONTROL_VERTS 16

/* Patch map node flags */

#define PATCH_MAP_NODE_IS_SET (1 << 30)
#define PATCH_MAP_NODE_IS_LEAF (1u << 31)
#define PATCH_MAP_NODE_INDEX_MASK (~(PATCH_MAP_NODE_IS_SET | PATCH_MAP_NODE_IS_LEAF))

CCL_NAMESPACE_END

#endif /*  __KERNEL_TYPES_H__ */

