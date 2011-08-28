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

CCL_NAMESPACE_BEGIN

#define OBJECT_SIZE 16

#define __SOBOL__
#define __INSTANCING__
#define __DPDU__
#define __UV__
#define __BACKGROUND__
#define __EMISSION__
#define __CAUSTICS_TRICKS__
#ifndef __KERNEL_OPENCL__
#define __SVM__
#define __TEXTURES__
#define __HOLDOUT__
#endif
#define __RAY_DIFFERENTIALS__
#define __CAMERA_CLIPPING__
#define __INTERSECTION_REFINE__
//#define __SOBOL_FULL_SCREEN__
//#define __MODIFY_TP__
//#define __QBVH__

/* Path Tracing */

enum PathTraceDimension {
	PRNG_FILTER_U = 0,
	PRNG_FILTER_V = 1,
	PRNG_LENS_U = 2,
	PRNG_LENS_V = 3,
	PRNG_BASE_NUM = 4,

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
 * your own new ray types and bitflag values */
enum PathRayFlag {
	PATH_RAY_CAMERA = 1,
	PATH_RAY_SHADOW = 2,
	PATH_RAY_REFLECT = 4,
	PATH_RAY_TRANSMIT = 8,
	PATH_RAY_DIFFUSE = 16,
	PATH_RAY_GLOSSY = 32,
	PATH_RAY_SINGULAR = 64
};

/* Bidirectional Path Tracing */

enum BidirTraceDimension {
	BRNG_FILTER_U = 0,
	BRNG_FILTER_V = 1,
	BRNG_LENS_U = 2,
	BRNG_LENS_V = 3,
	BRNG_LIGHT_U = 4,
	BRNG_LIGHT_V = 5,
	BRNG_LIGHT = 6,
	BRNG_LIGHT_F = 7,
	BRNG_EMISSIVE_U = 8,
	BRNG_EMISSIVE_V = 9,
	BRNG_BASE_NUM = 10,

	BRNG_BSDF_U = 0,
	BRNG_BSDF_V = 1,
	BRNG_BSDF = 2,
	BRNG_TERMINATE = 3,
	BRNG_BOUNCE_NUM = 4
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
	LABEL_STRAIGHT = 1024,
	LABEL_STOP = 2048
} ClosureLabel;

/* Ray Type */

typedef enum RayType {
	RayTypeCamera = 1,
	RayTypeShadow = 2,
	RayTypeReflection = 4,
	RayTypeRefraction = 8,
	RayTypeDiffuse = 16,
	RayTypeGlossy = 32
} RayType;

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

/* OSL data */

#if !defined(__KERNEL_GPU__) && defined(WITH_OSL)

#define MAX_OSL_CLOSURE 8

struct FlatClosure {
	void *prim;
	float3 weight;
	float sample_weight;
};

#endif

/* Shader Data
 *
 * Main shader state at a point on the surface or in a volume. All coordinates
 * are in world space. */

enum ShaderDataFlag {
	SD_BACKFACING = 1,		/* backside of surface? */
	SD_EMISSION = 2,		/* have emissive closure? */
	SD_BSDF = 4,			/* have bsdf closure? */
	SD_BSDF_HAS_EVAL = 8,	/* have non-singular bsdf closure? */
	SD_BSDF_GLOSSY = 16,	/* have glossy bsdf */
	SD_HOLDOUT = 32			/* have holdout closure? */
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

	/* SVM closure data. we always sample a single closure, to get fixed
	 * memory usage, svm_closure_data contains closure parameters. */
	ClosureType svm_closure;
	float3 svm_closure_weight;
	float svm_closure_data[3]; /* CUDA gives compile error if out of bounds */

#if !defined(__KERNEL_GPU__) && defined(WITH_OSL)
	/* OSL closure data and context. we store all closures flattened into
	 * lists per type, different from SVM. */
	struct {
		FlatClosure bsdf[MAX_OSL_CLOSURE];
		FlatClosure emissive[MAX_OSL_CLOSURE];
		FlatClosure volume[MAX_OSL_CLOSURE];
		
		int num_bsdf;
		int num_emissive;
		int num_volume;

		float bsdf_sample_sum;
		float emissive_sample_sum;
		float volume_sample_sum;

		float3 holdout_weight;

		float randb;
	} osl_closure;

	void *osl_ctx;
#endif
} ShaderData;

/* Constrant Kernel Data */

typedef struct KernelCamera {
	/* type */
	int ortho;
	int pad;

	/* size */
	int width, height;

	/* matrices */
	Transform cameratoworld;
	Transform rastertocamera;

	/* depth of field */
	float lensradius;
	float focaldistance;

	/* motion blur */
	float shutteropen;
	float shutterclose;

	/* differentials */
	float3 dx;
#ifndef WITH_OPENCL
	float pad1;
#endif
	float3 dy;
#ifndef WITH_OPENCL
	float pad2;
#endif

	/* clipping */
	float nearclip;
	float cliplength;
	float pad3, pad4;

	/* more matrices */
	Transform screentoworld;
	Transform rastertoworld;
	Transform ndctoworld;
	Transform worldtoscreen;
	Transform worldtoraster;
	Transform worldtondc;
	Transform worldtocamera;
} KernelCamera;

typedef struct KernelFilm {
	float exposure;
	int pad1, pad2, pad3;
} KernelFilm;

typedef struct KernelBackground {
	/* only shader index */
	int shader;
	int transparent;
	int pad1, pad2;
} KernelBackground;

typedef struct KernelSunSky {
	/* sun direction in spherical and cartesian */
	float theta, phi, pad3, pad4;
	float3 dir;
#ifndef WITH_OPENCL
	float pad;
#endif

	/* perez function parameters */
	float zenith_Y, zenith_x, zenith_y, pad2;
	float perez_Y[5], perez_x[5], perez_y[5];
	float pad5;
} KernelSunSky;

typedef struct KernelIntegrator {
	/* emission */
	int use_emission;
	int num_triangles;
	int num_distribution;
	int num_lights;
	float pdf_triangles;
	float pdf_lights;

	/* path tracing */
	int minbounce;
	int maxbounce;

	/* caustics */
	int no_caustics;
	float blur_caustics;

	/* padding */
	int pad[2];
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

