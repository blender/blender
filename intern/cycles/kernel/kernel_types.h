/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
#define OBJECT_SIZE 		16
#define LIGHT_SIZE			4
#define FILTER_TABLE_SIZE	256
#define RAMP_TABLE_SIZE		256
#define TIME_INVALID		FLT_MAX

/* device capabilities */
#ifdef __KERNEL_CPU__
#define __KERNEL_SHADING__
#define __KERNEL_ADV_SHADING__
#endif

#ifdef __KERNEL_CUDA__
#define __KERNEL_SHADING__
#if __CUDA_ARCH__ >= 200
#define __KERNEL_ADV_SHADING__
#endif
#endif

#ifdef __KERNEL_OPENCL__
//#define __KERNEL_SHADING__
//#define __KERNEL_ADV_SHADING__
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
#define __TEXTURES__
#define __HOLDOUT__
#endif

#ifdef __KERNEL_ADV_SHADING__
#define __MULTI_CLOSURE__
#define __TRANSPARENT_SHADOWS__
#define __PASSES__
#define __BACKGROUND_MIS__
#define __AO__
//#define __MOTION__
#endif

//#define __MULTI_LIGHT__
//#define __OSL__
//#define __SOBOL_FULL_SCREEN__
//#define __MODIFY_TP__
//#define __QBVH__

/* Shader Evaluation */

enum ShaderEvalType {
	SHADER_EVAL_DISPLACE,
	SHADER_EVAL_BACKGROUND
};

/* Path Tracing
 * note we need to keep the u/v pairs at even values */

enum PathTraceDimension {
	PRNG_FILTER_U = 0,
	PRNG_FILTER_V = 1,
	PRNG_LENS_U = 2,
	PRNG_LENS_V = 3,
#ifdef __MOTION__
	PRNG_TIME = 4,
	PRNG_UNUSED = 5,
	PRNG_BASE_NUM = 6,
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
	PRNG_BOUNCE_NUM = 8
};

/* these flag values correspond exactly to OSL defaults, so be careful not to
 * change this, or if you do, set the "raytypes" shading system attribute with
 * your own new ray types and bitflag values.
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

	PATH_RAY_MIS_SKIP = 512,

	PATH_RAY_ALL = (1|2|4|8|16|32|64|128|256|512),

	PATH_RAY_LAYER_SHIFT = (32-20)
};

/* Closure Label */

typedef enum ClosureLabel {
	LABEL_NONE = 0,
	LABEL_CAMERA = 1,
	LABEL_LIGHT = 2,
	LABEL_BACKGROUND = 4,
	LABEL_TRANSMIT = 8,
	LABEL_REFLECT = 16,
	LABEL_VOLUME = 32,
	LABEL_OBJECT = 64,
	LABEL_DIFFUSE = 128,
	LABEL_GLOSSY = 256,
	LABEL_SINGULAR = 512,
	LABEL_TRANSPARENT = 1024,
	LABEL_STOP = 2048
} ClosureLabel;

/* Render Passes */

typedef enum PassType {
	PASS_NONE = 0,
	PASS_COMBINED = 1,
	PASS_DEPTH = 2,
	PASS_NORMAL = 8,
	PASS_UV = 16,
	PASS_OBJECT_ID = 32,
	PASS_MATERIAL_ID = 64,
	PASS_DIFFUSE_COLOR = 128,
	PASS_GLOSSY_COLOR = 256,
	PASS_TRANSMISSION_COLOR = 512,
	PASS_DIFFUSE_INDIRECT = 1024,
	PASS_GLOSSY_INDIRECT = 2048,
	PASS_TRANSMISSION_INDIRECT = 4096,
	PASS_DIFFUSE_DIRECT = 8192,
	PASS_GLOSSY_DIRECT = 16384,
	PASS_TRANSMISSION_DIRECT = 32768,
	PASS_EMISSION = 65536,
	PASS_BACKGROUND = 131072,
	PASS_AO = 262144,
	PASS_SHADOW = 524288,
	PASS_MOTION = 1048576,
	PASS_MOTION_WEIGHT = 2097152
} PassType;

#define PASS_ALL (~0)

#ifdef __PASSES__

typedef float3 PathThroughput;

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

	float3 direct_diffuse;
	float3 direct_glossy;
	float3 direct_transmission;

	float3 indirect_diffuse;
	float3 indirect_glossy;
	float3 indirect_transmission;

	float4 shadow;
} PathRadiance;

typedef struct BsdfEval {
	int use_light_pass;

	float3 diffuse;
	float3 glossy;
	float3 transmission;
	float3 transparent;
} BsdfEval;

#else

typedef float3 PathThroughput;
typedef float3 PathRadiance;
typedef float3 BsdfEval;

#endif

/* Shader Flag */

typedef enum ShaderFlag {
	SHADER_SMOOTH_NORMAL = (1 << 31),
	SHADER_CAST_SHADOW = (1 << 30),
	SHADER_AREA_LIGHT = (1 << 29),

	SHADER_MASK = ~(SHADER_SMOOTH_NORMAL|SHADER_CAST_SHADOW|SHADER_AREA_LIGHT)
} ShaderFlag;

/* Light Type */

typedef enum LightType {
	LIGHT_POINT,
	LIGHT_DISTANT,
	LIGHT_BACKGROUND,
	LIGHT_AREA,
	LIGHT_AO
} LightType;

/* Camera Type */

enum CameraType {
	CAMERA_PERSPECTIVE,
	CAMERA_ORTHOGRAPHIC,
	CAMERA_ENVIRONMENT
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
	float3 P;
	float3 D;
	float t;
	float time;

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
} Intersection;

/* Attributes */

typedef enum AttributeElement {
	ATTR_ELEMENT_FACE,
	ATTR_ELEMENT_VERTEX,
	ATTR_ELEMENT_CORNER,
	ATTR_ELEMENT_VALUE,
	ATTR_ELEMENT_NONE
} AttributeElement;

typedef enum AttributeStandard {
	ATTR_STD_NONE = 0,
	ATTR_STD_VERTEX_NORMAL,
	ATTR_STD_FACE_NORMAL,
	ATTR_STD_UV,
	ATTR_STD_GENERATED,
	ATTR_STD_POSITION_UNDEFORMED,
	ATTR_STD_POSITION_UNDISPLACED,
	ATTR_STD_MOTION_PRE,
	ATTR_STD_MOTION_POST,
	ATTR_STD_NUM,

	ATTR_STD_NOT_FOUND = ~0
} AttributeStandard;

/* Closure data */

#define MAX_CLOSURE 8

typedef struct ShaderClosure {
	ClosureType type;
	float3 weight;

#ifdef __MULTI_CLOSURE__
	float sample_weight;
#endif

#ifdef __OSL__
	void *prim;
#else
	float data0;
	float data1;
#endif

} ShaderClosure;

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
	SD_BSDF_GLOSSY = 16,	/* have glossy bsdf */
	SD_HOLDOUT = 32,		/* have holdout closure? */
	SD_VOLUME = 64,			/* have volume closure? */

	/* shader flags */
	SD_SAMPLE_AS_LIGHT = 128,			/* direct light sample */
	SD_HAS_SURFACE_TRANSPARENT = 256,	/* has surface transparency */
	SD_HAS_VOLUME = 512,				/* has volume shader */
	SD_HOMOGENEOUS_VOLUME = 1024		/* has homogeneous volume */
};

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
	/* parametric coordinates
	 * - barycentric weights for triangles */
	float u, v;
	/* object id if there is one, ~0 otherwise */
	int object;

	/* motion blur sample time */
	float time;

#ifdef __MOTION__
	/* object <-> world space transformations, cached to avoid
	 * re-interpolating them constantly for shading */
	Transform ob_tfm;
	Transform ob_itfm;
#endif

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

#ifdef __MULTI_CLOSURE__
	/* Closure data, we store a fixed array of closures */
	ShaderClosure closure[MAX_CLOSURE];
	int num_closure;
	float randb_closure;
#else
	/* Closure data, with a single sampled closure for low memory usage */
	ShaderClosure closure;
#endif

#ifdef __OSL__
	/* OSL context */
	void *osl_ctx;
#endif
} ShaderData;

/* Constrant Kernel Data
 *
 * These structs are passed from CPU to various devices, and the struct layout
 * must match exactly. Structs are padded to ensure 16 byte alignment, and we
 * do not use float3 because its size may not be the same on all devices. */

typedef struct KernelCamera {
	/* type */
	int type;
	int pad1, pad2, pad3;

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
	float pad;

	/* clipping */
	float nearclip;
	float cliplength;

	/* more matrices */
	Transform screentoworld;
	Transform rastertoworld;
	Transform ndctoworld;
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
	int pass_diffuse_indirect;

	int pass_glossy_indirect;
	int pass_transmission_indirect;
	int pass_diffuse_direct;
	int pass_glossy_direct;

	int pass_transmission_direct;
	int pass_emission;
	int pass_background;
	int pass_ao;

	int pass_shadow;
	int pass_pad1;
	int pass_pad2;
	int pass_pad3;
} KernelFilm;

typedef struct KernelBackground {
	/* only shader index */
	int shader;
	int transparent;

	/* ambient occlusion */
	float ao_factor;
	float ao_distance;
} KernelBackground;

typedef struct KernelSunSky {
	/* sun direction in spherical and cartesian */
	float theta, phi, pad3, pad4;

	/* perez function parameters */
	float zenith_Y, zenith_x, zenith_y, pad2;
	float perez_Y[5], perez_x[5], perez_y[5];
	float pad5;
} KernelSunSky;

typedef struct KernelIntegrator {
	/* emission */
	int use_direct_light;
	int use_ambient_occlusion;
	int num_distribution;
	int num_all_lights;
	float pdf_triangles;
	float pdf_lights;
	int pdf_background_res;

	/* bounces */
	int min_bounce;
	int max_bounce;

	int max_diffuse_bounce;
	int max_glossy_bounce;
	int max_transmission_bounce;

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
	float sample_clamp;
} KernelIntegrator;

typedef struct KernelBVH {
	/* root node */
	int root;
	int attributes_map_stride;
	int pad1, pad2;
} KernelBVH;

typedef struct KernelData {
	KernelCamera cam;
	KernelFilm film;
	KernelBackground background;
	KernelSunSky sunsky;
	KernelIntegrator integrator;
	KernelBVH bvh;
} KernelData;

CCL_NAMESPACE_END

#endif /*  __KERNEL_TYPES_H__ */

