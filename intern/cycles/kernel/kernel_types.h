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
 * limitations under the License
 */

#ifndef __KERNEL_TYPES_H__
#define __KERNEL_TYPES_H__

#include "kernel_math.h"
#include "svm/svm_types.h"

#ifndef __KERNEL_GPU__
#define __KERNEL_CPU__
#endif

CCL_NAMESPACE_BEGIN

/* constants */
#define OBJECT_SIZE 		11
#define OBJECT_VECTOR_SIZE	6
#define LIGHT_SIZE			4
#define FILTER_TABLE_SIZE	256
#define RAMP_TABLE_SIZE		256
#define PARTICLE_SIZE 		5
#define TIME_INVALID		FLT_MAX

#define BSSRDF_MIN_RADIUS			1e-8f
#define BSSRDF_MAX_HITS				4

#define BB_DRAPPER				800.0f
#define BB_MAX_TABLE_RANGE		12000.0f
#define BB_TABLE_XPOWER			1.5f
#define BB_TABLE_YPOWER			5.0f
#define BB_TABLE_SPACING		2.0f

#define TEX_NUM_FLOAT_IMAGES	5

#define SHADER_NO_ID			-1

#define VOLUME_STACK_SIZE		16

/* device capabilities */
#ifdef __KERNEL_CPU__
#define __KERNEL_SHADING__
#define __KERNEL_ADV_SHADING__
#define __BRANCHED_PATH__
#ifdef WITH_OSL
#define __OSL__
#endif
#define __SUBSURFACE__
#define __CMJ__
#define __VOLUME__
#endif

#ifdef __KERNEL_CUDA__
#define __KERNEL_SHADING__
#define __KERNEL_ADV_SHADING__
#define __BRANCHED_PATH__
//#define __VOLUME__
#endif

#ifdef __KERNEL_OPENCL__

/* keep __KERNEL_ADV_SHADING__ in sync with opencl_kernel_use_advanced_shading! */

#ifdef __KERNEL_OPENCL_NVIDIA__
#define __KERNEL_SHADING__
#define __KERNEL_ADV_SHADING__
#endif

#ifdef __KERNEL_OPENCL_APPLE__
#define __KERNEL_SHADING__
//#define __KERNEL_ADV_SHADING__
#endif

#ifdef __KERNEL_OPENCL_AMD__
#define __SVM__
#define __EMISSION__
#define __IMAGE_TEXTURES__
#define __PROCEDURAL_TEXTURES__
#define __EXTRA_NODES__
#define __HOLDOUT__
#define __NORMAL_MAP__
//#define __BACKGROUND_MIS__
//#define __LAMP_MIS__
//#define __AO__
//#define __ANISOTROPIC__
//#define __CAMERA_MOTION__
//#define __OBJECT_MOTION__
//#define __HAIR__
//#define __MULTI_CLOSURE__
//#define __TRANSPARENT_SHADOWS__
//#define __PASSES__
#endif

#ifdef __KERNEL_OPENCL_INTEL_CPU__
#define __KERNEL_SHADING__
#define __KERNEL_ADV_SHADING__
#endif

#endif

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

#ifdef __KERNEL_SHADING__
#define __SVM__
#define __EMISSION__
#define __PROCEDURAL_TEXTURES__
#define __IMAGE_TEXTURES__
#define __EXTRA_NODES__
#define __HOLDOUT__
#define __NORMAL_MAP__
#endif

#ifdef __KERNEL_ADV_SHADING__
#define __MULTI_CLOSURE__
#define __TRANSPARENT_SHADOWS__
#define __PASSES__
#define __BACKGROUND_MIS__
#define __LAMP_MIS__
#define __AO__
#define __ANISOTROPIC__
#define __CAMERA_MOTION__
#define __OBJECT_MOTION__
#define __HAIR__
#endif

/* Sanity check */

#if defined(__KERNEL_OPENCL_NEED_ADVANCED_SHADING__) && !defined(__MULTI_CLOSURE__)
#error "OpenCL: mismatch between advanced shading flags in device_opencl.cpp and kernel_types.h"
#endif

/* Random Numbers */

typedef uint RNG;

/* Shader Evaluation */

typedef enum ShaderEvalType {
	SHADER_EVAL_DISPLACE,
	SHADER_EVAL_BACKGROUND
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
	PRNG_BASE_NUM = 8,
#else
	PRNG_BASE_NUM = 4,
#endif

	PRNG_BSDF_U = 0,
	PRNG_BSDF_V = 1,
	PRNG_BSDF = 2,
	PRNG_LIGHT = 3,
	PRNG_LIGHT_U = 4,
	PRNG_LIGHT_V = 5,
	PRNG_LIGHT_F = 6,
	PRNG_TERMINATE = 7,

#ifdef __VOLUME__
	PRNG_PHASE_U = 8,
	PRNG_PHASE_V = 9,
	PRNG_PHASE = 10,
	PRNG_SCATTER_DISTANCE = 11,
	PRNG_BOUNCE_NUM = 12,
#else
	PRNG_BOUNCE_NUM = 8,
#endif
};

enum SamplingPattern {
	SAMPLING_PATTERN_SOBOL = 0,
	SAMPLING_PATTERN_CMJ = 1
};

/* these flags values correspond to raytypes in osl.cpp, so keep them in sync!
 *
 * for ray visibility tests in BVH traversal, the upper 20 bits are used for
 * layer visibility tests. */

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

	PATH_RAY_CURVE = 512, /* visibility flag to define curve segments*/

	/* note that these can use maximum 12 bits, the other are for layers */
	PATH_RAY_ALL_VISIBILITY = (1|2|4|8|16|32|64|128|256|512),

	PATH_RAY_MIS_SKIP = 1024,
	PATH_RAY_DIFFUSE_ANCESTOR = 2048,
	PATH_RAY_GLOSSY_ANCESTOR = 4096,
	PATH_RAY_BSSRDF_ANCESTOR = 8192,
	PATH_RAY_SINGLE_PASS_DONE = 16384,
	PATH_RAY_VOLUME_SCATTER = 32768,

	/* we need layer member flags to be the 20 upper bits */
	PATH_RAY_LAYER_SHIFT = (32-20)
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
	PASS_COMBINED = 1,
	PASS_DEPTH = 2,
	PASS_NORMAL = 4,
	PASS_UV = 8,
	PASS_OBJECT_ID = 16,
	PASS_MATERIAL_ID = 32,
	PASS_DIFFUSE_COLOR = 64,
	PASS_GLOSSY_COLOR = 128,
	PASS_TRANSMISSION_COLOR = 256,
	PASS_DIFFUSE_INDIRECT = 512,
	PASS_GLOSSY_INDIRECT = 1024,
	PASS_TRANSMISSION_INDIRECT = 2048,
	PASS_DIFFUSE_DIRECT = 4096,
	PASS_GLOSSY_DIRECT = 8192,
	PASS_TRANSMISSION_DIRECT = 16384,
	PASS_EMISSION = 32768,
	PASS_BACKGROUND = 65536,
	PASS_AO = 131072,
	PASS_SHADOW = 262144,
	PASS_MOTION = 524288,
	PASS_MOTION_WEIGHT = 1048576,
	PASS_MIST = 2097152,
	PASS_SUBSURFACE_DIRECT = 4194304,
	PASS_SUBSURFACE_INDIRECT = 8388608,
	PASS_SUBSURFACE_COLOR = 16777216
} PassType;

#define PASS_ALL (~0)

#ifdef __PASSES__

typedef struct PathRadiance {
	int use_light_pass;

	float3 emission;
	float3 background;
	float3 ao;

	float3 indirect;
	float3 direct_throughput;
	float3 direct_emission;

	float3 color_diffuse;
	float3 color_glossy;
	float3 color_transmission;
	float3 color_subsurface;

	float3 direct_diffuse;
	float3 direct_glossy;
	float3 direct_transmission;
	float3 direct_subsurface;

	float3 indirect_diffuse;
	float3 indirect_glossy;
	float3 indirect_transmission;
	float3 indirect_subsurface;

	float3 path_diffuse;
	float3 path_glossy;
	float3 path_transmission;
	float3 path_subsurface;

	float4 shadow;
	float mist;
} PathRadiance;

typedef struct BsdfEval {
	int use_light_pass;

	float3 diffuse;
	float3 glossy;
	float3 transmission;
	float3 transparent;
	float3 subsurface;
} BsdfEval;

#else

typedef float3 PathRadiance;
typedef float3 BsdfEval;

#endif

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
	SHADER_EXCLUDE_ANY = (SHADER_EXCLUDE_DIFFUSE|SHADER_EXCLUDE_GLOSSY|SHADER_EXCLUDE_TRANSMIT|SHADER_EXCLUDE_CAMERA),

	SHADER_MASK = ~(SHADER_SMOOTH_NORMAL|SHADER_CAST_SHADOW|SHADER_AREA_LIGHT|SHADER_USE_MIS|SHADER_EXCLUDE_ANY)
} ShaderFlag;

/* Light Type */

typedef enum LightType {
	LIGHT_POINT,
	LIGHT_DISTANT,
	LIGHT_BACKGROUND,
	LIGHT_AREA,
	LIGHT_AO,
	LIGHT_SPOT,
	LIGHT_TRIANGLE,
	LIGHT_STRAND
} LightType;

/* Camera Type */

enum CameraType {
	CAMERA_PERSPECTIVE,
	CAMERA_ORTHOGRAPHIC,
	CAMERA_PANORAMA
};

/* Panorama Type */

enum PanoramaType {
	PANORAMA_EQUIRECTANGULAR,
	PANORAMA_FISHEYE_EQUIDISTANT,
	PANORAMA_FISHEYE_EQUISOLID
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
	float3 P;		/* origin */
	float3 D;		/* direction */
	float t;		/* length of the ray */
	float time;		/* time (for motion blur) */

#ifdef __RAY_DIFFERENTIALS__
	differential3 dP;
	differential3 dD;
#endif
} Ray;

/* Intersection */

typedef struct Intersection {
	float t, u, v;
	int prim;
	int object;
	int segment;
} Intersection;

/* Attributes */

#define ATTR_PRIM_TYPES		2
#define ATTR_PRIM_CURVE		1

typedef enum AttributeElement {
	ATTR_ELEMENT_NONE,
	ATTR_ELEMENT_OBJECT,
	ATTR_ELEMENT_MESH,
	ATTR_ELEMENT_FACE,
	ATTR_ELEMENT_VERTEX,
	ATTR_ELEMENT_CORNER,
	ATTR_ELEMENT_CURVE,
	ATTR_ELEMENT_CURVE_KEY
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
	ATTR_STD_MOTION_PRE,
	ATTR_STD_MOTION_POST,
	ATTR_STD_PARTICLE,
	ATTR_STD_CURVE_INTERCEPT,
	ATTR_STD_PTEX_FACE_ID,
	ATTR_STD_PTEX_UV,
	ATTR_STD_NUM,

	ATTR_STD_NOT_FOUND = ~0
} AttributeStandard;

/* Closure data */

#define MAX_CLOSURE 64

typedef struct ShaderClosure {
	ClosureType type;
	float3 weight;

#ifdef __MULTI_CLOSURE__
	float sample_weight;
#endif

	float data0;
	float data1;

	float3 N;
#if defined(__ANISOTROPIC__) || defined(__SUBSURFACE__) || defined(__HAIR__)
	float3 T;
#endif

#ifdef __HAIR__
	float offset;
#endif

#ifdef __OSL__
	void *prim;
#endif
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
	SD_BACKFACING = 1,		/* backside of surface? */
	SD_EMISSION = 2,		/* have emissive closure? */
	SD_BSDF = 4,			/* have bsdf closure? */
	SD_BSDF_HAS_EVAL = 8,	/* have non-singular bsdf closure? */
	SD_PHASE_HAS_EVAL = 8,	/* have non-singular phase closure? */
	SD_BSDF_GLOSSY = 16,	/* have glossy bsdf */
	SD_BSSRDF = 32,			/* have bssrdf */
	SD_HOLDOUT = 64,		/* have holdout closure? */
	SD_ABSORPTION = 128,	/* have volume absorption closure? */
	SD_SCATTER = 256,		/* have volume phase closure? */
	SD_AO = 512,			/* have ao closure? */
	SD_TRANSPARENT = 1024,	/* have transparent closure? */

	SD_CLOSURE_FLAGS = (SD_EMISSION|SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY|SD_BSSRDF|SD_HOLDOUT|SD_ABSORPTION|SD_SCATTER|SD_AO),

	/* shader flags */
	SD_USE_MIS = 2048,					/* direct light sample */
	SD_HAS_TRANSPARENT_SHADOW = 4096,	/* has transparent shadow */
	SD_HAS_VOLUME = 8192,				/* has volume shader */
	SD_HAS_ONLY_VOLUME = 16384,			/* has only volume shader, no surface */
	SD_HETEROGENEOUS_VOLUME = 32768,	/* has heterogeneous volume */
	SD_HAS_BSSRDF_BUMP = 65536,			/* bssrdf normal uses bump */

	SD_SHADER_FLAGS = (SD_USE_MIS|SD_HAS_TRANSPARENT_SHADOW|SD_HAS_VOLUME|SD_HAS_ONLY_VOLUME|SD_HETEROGENEOUS_VOLUME|SD_HAS_BSSRDF_BUMP),

	/* object flags */
	SD_HOLDOUT_MASK = 131072,			/* holdout for camera rays */
	SD_OBJECT_MOTION = 262144,			/* has object motion blur */
	SD_TRANSFORM_APPLIED = 524288, 		/* vertices have transform applied */

	SD_OBJECT_FLAGS = (SD_HOLDOUT_MASK|SD_OBJECT_MOTION|SD_TRANSFORM_APPLIED)
};

struct KernelGlobals;

typedef struct ShaderData {
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

	/* primitive id if there is one, ~0 otherwise */
	int prim;

#ifdef __HAIR__
	/* for curves, segment number in curve, ~0 for triangles */
	int segment;
	/* variables for minimum hair width using transparency bsdf */
	/*float curve_transparency; */
	/*float curve_radius; */
#endif
	/* parametric coordinates
	 * - barycentric weights for triangles */
	float u, v;
	/* object id if there is one, ~0 otherwise */
	int object;

	/* motion blur sample time */
	float time;
	
	/* length of the ray being shaded */
	float ray_length;
	
	/* ray bounce depth */
	int ray_depth;

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
	float3 dPdu, dPdv;
#endif

#ifdef __OBJECT_MOTION__
	/* object <-> world space transformations, cached to avoid
	 * re-interpolating them constantly for shading */
	Transform ob_tfm;
	Transform ob_itfm;
#endif

#ifdef __MULTI_CLOSURE__
	/* Closure data, we store a fixed array of closures */
	ShaderClosure closure[MAX_CLOSURE];
	int num_closure;
	float randb_closure;
#else
	/* Closure data, with a single sampled closure for low memory usage */
	ShaderClosure closure;
#endif

	/* ray start position, only set for backgrounds */
	float3 ray_P;
	differential3 ray_dP;

#ifdef __OSL__
	struct KernelGlobals *osl_globals;
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
	int rng_offset;    /* dimension offset */
	int sample;        /* path sample number */
	int num_samples;   /* total number of times this path will be sampled */

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
	int have_motion;

	/* clipping */
	float nearclip;
	float cliplength;

	/* sensor size */
	float sensorwidth;
	float sensorheight;

	/* render size */
	float width, height;
	int resolution;
	int pad1;
	int pad2;
	int pad3;

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
} KernelCamera;

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
} KernelFilm;

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
	int no_caustics;
	float filter_glossy;

	/* seed */
	int seed;

	/* render layer */
	int layer_flag;

	/* clamp */
	float sample_clamp_direct;
	float sample_clamp_indirect;

	/* branched path */
	int branched;
	int aa_samples;
	int diffuse_samples;
	int glossy_samples;
	int transmission_samples;
	int ao_samples;
	int mesh_light_samples;
	int subsurface_samples;
	
	/* mis */
	int use_lamp_mis;

	/* sampler */
	int sampling_pattern;

	/* volume render */
	int volume_homogeneous_sampling;
	int use_volumes;
	int volume_max_steps;
	float volume_step_size;
	int volume_samples;
	int pad1, pad2;
} KernelIntegrator;

typedef struct KernelBVH {
	/* root node */
	int root;
	int attributes_map_stride;
	int have_motion;
	int have_curves;
	int have_instancing;

	int pad1, pad2, pad3;
} KernelBVH;

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
	/* strand intersect and normal parameters - many can be changed to flags */
	int curveflags;
	int subdivisions;

	float minimum_width;
	float maximum_width;
} KernelCurves;

typedef struct KernelBlackbody {
	int table_offset;
	int pad1, pad2, pad3;
} KernelBlackbody;


typedef struct KernelData {
	KernelCamera cam;
	KernelFilm film;
	KernelBackground background;
	KernelIntegrator integrator;
	KernelBVH bvh;
	KernelCurves curve;
	KernelBlackbody blackbody;
} KernelData;

CCL_NAMESPACE_END

#endif /*  __KERNEL_TYPES_H__ */

