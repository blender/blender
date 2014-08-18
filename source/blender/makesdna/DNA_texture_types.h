/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_texture_types.h
 *  \ingroup DNA
 *  \since mar-2001
 *  \author nzc
 */

#ifndef __DNA_TEXTURE_TYPES_H__
#define __DNA_TEXTURE_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_image_types.h" /* ImageUser */

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct Ipo;
struct ColorBand;
struct EnvMap;
struct Object;
struct Tex;
struct Image;
struct PreviewImage;
struct ImBuf;
struct Ocean;
struct CurveMapping;

typedef struct MTex {

	short texco, mapto, maptoneg, blendtype;
	struct Object *object;
	struct Tex *tex;
	char uvname[64];	/* MAX_CUSTOMDATA_LAYER_NAME */
	
	char projx, projy, projz, mapping;
	float ofs[3], size[3], rot;
	
	short texflag, colormodel, pmapto, pmaptoneg;
	short normapspace, which_output;
	char brush_map_mode, pad[7];
	float r, g, b, k;
	float def_var, rt;
	
	/* common */
	float colfac, varfac;
	
	/* material */
	float norfac, dispfac, warpfac;
	float colspecfac, mirrfac, alphafac;
	float difffac, specfac, emitfac, hardfac;
	float raymirrfac, translfac, ambfac;
	float colemitfac, colreflfac, coltransfac;
	float densfac, scatterfac, reflfac;

	/* particles */
	float timefac, lengthfac, clumpfac, dampfac;
	float kinkfac, roughfac, padensfac, gravityfac;
	float lifefac, sizefac, ivelfac, fieldfac;

	/* lamp */
	float shadowfac;

	/* world */
	float zenupfac, zendownfac, blendfac;
} MTex;

#ifndef DNA_USHORT_FIX
#define DNA_USHORT_FIX
/**
 * \deprecated This typedef serves to avoid badly typed functions when
 * \deprecated compiling while delivering a proper dna.c. Do not use
 * \deprecated it in any case.
 */
typedef unsigned short dna_ushort_fix;
#endif

typedef struct CBData {
	float r, g, b, a, pos;
	int cur;
} CBData;

/* 32 = MAXCOLORBAND */
/* note that this has to remain a single struct, for UserDef */
typedef struct ColorBand {
	short tot, cur;
	char ipotype, ipotype_hue;
	char color_mode;
	char pad[1];

	CBData data[32];
} ColorBand;

typedef struct EnvMap {
	struct Object *object;
	struct Image *ima;		/* type ENV_LOAD */
	struct ImBuf *cube[6];		/* these images are dynamic, not part of the main struct */
	float imat[4][4];
	float obimat[3][3];
	short type, stype;
	float clipsta, clipend;
	float viewscale;	/* viewscale is for planar envmaps to zoom in or out */
	unsigned int notlay;
	short cuberes, depth;
	int ok, lastframe;
	short recalc, lastsize;
} EnvMap;

typedef struct PointDensity {
	short flag;

	short falloff_type;
	float falloff_softness;
	float radius;
	short source;
	short color_source;
	int totpoints;
	
	int pdpad;

	struct Object *object;	/* for 'Object' or 'Particle system' type - source object */
	int psys;				/* index+1 in ob.particlesystem, non-ID pointer not allowed */
	short psys_cache_space;		/* cache points in worldspace, object space, ... ? */
	short ob_cache_space;		/* cache points in worldspace, object space, ... ? */
	
	void *point_tree;		/* the acceleration tree containing points */
	float *point_data;		/* dynamically allocated extra for extra information, like particle age */
	
	float noise_size;
	short noise_depth;
	short noise_influence;
	short noise_basis;
	short pdpad3[3];
	float noise_fac;
	
	float speed_scale, falloff_speed_scale, pdpad2;
	struct ColorBand *coba;	/* for time -> color */
	
	struct CurveMapping *falloff_curve; /* falloff density curve */
} PointDensity;

typedef struct VoxelData {
	int resol[3];
	int interp_type;
	short file_format;
	short flag;
	short extend;
	short smoked_type;
	short data_type;
	short pad;
	int _pad;
	
	struct Object *object; /* for rendering smoke sims */
	float int_multiplier;
	int still_frame;
	char source_path[1024];  /* 1024 = FILE_MAX */

	/* temporary data */
	float *dataset;
	int cachedframe;
	int ok;
	
} VoxelData;

typedef struct OceanTex {
	struct Object *object;
	char oceanmod[64];
	
	int output;
	int pad;
	
} OceanTex;
	
typedef struct Tex {
	ID id;
	struct AnimData *adt;	/* animation data (must be immediately after id for utilities to use it) */ 
	
	float noisesize, turbul;
	float bright, contrast, saturation, rfac, gfac, bfac;
	float filtersize, pad2;

	/* newnoise: musgrave parameters */
	float mg_H, mg_lacunarity, mg_octaves, mg_offset, mg_gain;

	/* newnoise: distorted noise amount, musgrave & voronoi output scale */
	float dist_amount, ns_outscale;

	/* newnoise: voronoi nearest neighbor weights, minkovsky exponent, distance metric & color type */
	float vn_w1;
	float vn_w2;
	float vn_w3;
	float vn_w4;
	float vn_mexp;
	short vn_distm, vn_coltype;

	short noisedepth, noisetype; /* noisedepth MUST be <= 30 else we get floating point exceptions */

	/* newnoise: noisebasis type for clouds/marble/etc, noisebasis2 only used for distorted noise */
	short noisebasis, noisebasis2;

	short imaflag, flag;
	short type, stype;
	
	float cropxmin, cropymin, cropxmax, cropymax;
	int texfilter;
	int afmax;	// anisotropic filter maximum value, ewa -> max eccentricity, feline -> max probes
	short xrepeat, yrepeat;
	short extend;

	/* variables disabled, moved to struct iuser */
	short fie_ima;
	int len;
	int frames, offset, sfra;
	
	float checkerdist, nabla;
	float pad1;
	
	struct ImageUser iuser;
	
	struct bNodeTree *nodetree;
	struct Ipo *ipo  DNA_DEPRECATED;  /* old animation system, deprecated for 2.5 */
	struct Image *ima;
	struct ColorBand *coba;
	struct EnvMap *env;
	struct PreviewImage *preview;
	struct PointDensity *pd;
	struct VoxelData *vd;
	struct OceanTex *ot;
	
	char use_nodes;
	char pad[7];
	
} Tex;

/* used for mapping and texture nodes. note: rot is now in radians */

typedef struct TexMapping {
	float loc[3], rot[3], size[3];
	int flag;
	char projx, projy, projz, mapping;
	int type;
	
	float mat[4][4];
	float min[3], max[3];
	struct Object *ob;

} TexMapping;

typedef struct ColorMapping {
	struct ColorBand coba;

	float bright, contrast, saturation;
	int flag;

	float blend_color[3];
	float blend_factor;
	int blend_type, pad[3];
} ColorMapping;

/* texmap->flag */
#define TEXMAP_CLIP_MIN		1
#define TEXMAP_CLIP_MAX		2
#define TEXMAP_UNIT_MATRIX	4

/* texmap->type */
#define TEXMAP_TYPE_POINT		0
#define TEXMAP_TYPE_TEXTURE		1
#define TEXMAP_TYPE_VECTOR		2
#define TEXMAP_TYPE_NORMAL		3

/* colormap->flag */
#define COLORMAP_USE_RAMP 1

/* **************** TEX ********************* */

/* type */
#define TEX_CLOUDS		1
#define TEX_WOOD		2
#define TEX_MARBLE		3
#define TEX_MAGIC		4
#define TEX_BLEND		5
#define TEX_STUCCI		6
#define TEX_NOISE		7
#define TEX_IMAGE		8
//#define TEX_PLUGIN		9 /* Deprecated */
#define TEX_ENVMAP		10
#define TEX_MUSGRAVE	11
#define TEX_VORONOI		12
#define TEX_DISTNOISE	13
#define TEX_POINTDENSITY	14
#define TEX_VOXELDATA		15
#define TEX_OCEAN		16

/* musgrave stype */
#define TEX_MFRACTAL		0
#define TEX_RIDGEDMF		1
#define TEX_HYBRIDMF		2
#define TEX_FBM				3
#define TEX_HTERRAIN		4

/* newnoise: noisebasis 1 & 2 */
#define TEX_BLENDER			0
#define TEX_STDPERLIN		1
#define TEX_NEWPERLIN		2
#define TEX_VORONOI_F1		3
#define TEX_VORONOI_F2		4
#define TEX_VORONOI_F3		5
#define TEX_VORONOI_F4		6
#define TEX_VORONOI_F2F1	7
#define TEX_VORONOI_CRACKLE		8
#define TEX_CELLNOISE		14

/* newnoise: Voronoi distance metrics, vn_distm */
#define TEX_DISTANCE		0
#define TEX_DISTANCE_SQUARED		1
#define TEX_MANHATTAN		2
#define TEX_CHEBYCHEV		3
#define TEX_MINKOVSKY_HALF		4
#define TEX_MINKOVSKY_FOUR		5
#define TEX_MINKOVSKY		6

/* imaflag */
#define TEX_INTERPOL	1
#define TEX_USEALPHA	2
#define TEX_MIPMAP		4
#define TEX_IMAROT		16
#define TEX_CALCALPHA	32
#define TEX_NORMALMAP	2048
#define TEX_GAUSS_MIP	4096
#define TEX_FILTER_MIN	8192
#define TEX_DERIVATIVEMAP	16384

/* texfilter */
// TXF_BOX -> blender's old texture filtering method
#define TXF_BOX			0
#define TXF_EWA			1
#define TXF_FELINE		2
#define TXF_AREA		3

/* imaflag unused, only for version check */
#define TEX_FIELDS_		8
#define TEX_ANIMCYCLIC_	64
#define TEX_ANIM5_		128
#define TEX_ANTIALI_	256
#define TEX_ANTISCALE_	512
#define TEX_STD_FIELD_	1024

/* flag */
#define TEX_COLORBAND		1
#define TEX_FLIPBLEND		2
#define TEX_NEGALPHA		4
#define TEX_CHECKER_ODD		8
#define TEX_CHECKER_EVEN	16
#define TEX_PRV_ALPHA		32
#define TEX_PRV_NOR			64
#define TEX_REPEAT_XMIR		128
#define TEX_REPEAT_YMIR		256
#define TEX_FLAG_MASK		( TEX_COLORBAND | TEX_FLIPBLEND | TEX_NEGALPHA | TEX_CHECKER_ODD | TEX_CHECKER_EVEN | TEX_PRV_ALPHA | TEX_PRV_NOR | TEX_REPEAT_XMIR | TEX_REPEAT_YMIR ) 
#define TEX_DS_EXPAND		512
#define TEX_NO_CLAMP		1024

/* extend (starts with 1 because of backward comp.) */
#define TEX_EXTEND		1
#define TEX_CLIP		2
#define TEX_REPEAT		3
#define TEX_CLIPCUBE	4
#define TEX_CHECKER		5

/* noisetype */
#define TEX_NOISESOFT	0
#define TEX_NOISEPERL	1

/* tex->noisebasis2 in texture.c - wood waveforms */
#define TEX_SIN			0
#define TEX_SAW			1
#define TEX_TRI			2

/* tex->stype in texture.c - wood types */
#define TEX_BAND		0
#define TEX_RING		1
#define TEX_BANDNOISE	2
#define TEX_RINGNOISE	3

/* tex->stype in texture.c - cloud types */
#define TEX_DEFAULT		0
#define TEX_COLOR		1

/* tex->stype in texture.c - marble types */
#define TEX_SOFT		0
#define TEX_SHARP		1
#define TEX_SHARPER		2

/* tex->stype in texture.c - blend types */
#define TEX_LIN			0
#define TEX_QUAD		1
#define TEX_EASE		2
#define TEX_DIAG		3
#define TEX_SPHERE		4
#define TEX_HALO		5
#define TEX_RAD			6

/* tex->stype in texture.c - stucci types */
#define TEX_PLASTIC		0
#define TEX_WALLIN		1
#define TEX_WALLOUT		2

/* tex->stype in texture.c - voronoi types */
#define TEX_INTENSITY	0
#define TEX_COL1		1
#define TEX_COL2		2
#define TEX_COL3		3

/* mtex->normapspace */
#define MTEX_NSPACE_CAMERA	0
#define MTEX_NSPACE_WORLD	1
#define MTEX_NSPACE_OBJECT	2
#define MTEX_NSPACE_TANGENT	3

/* wrap */
#define MTEX_FLAT		0
#define MTEX_CUBE		1
#define MTEX_TUBE		2
#define MTEX_SPHERE		3

/* return value */
#define TEX_INT		0
#define TEX_RGB		1
#define TEX_NOR		2

/* pr_texture in material, world, lamp, */
#define TEX_PR_TEXTURE	0
#define TEX_PR_OTHER	1
#define TEX_PR_BOTH		2

/* **************** MTEX ********************* */

/* proj */
#define PROJ_N			0
#define PROJ_X			1
#define PROJ_Y			2
#define PROJ_Z			3

/* texflag */
#define MTEX_RGBTOINT		1
#define MTEX_STENCIL		2
#define MTEX_NEGATIVE		4
#define MTEX_ALPHAMIX		8
#define MTEX_VIEWSPACE		16
#define MTEX_DUPLI_MAPTO	32
#define MTEX_OB_DUPLI_ORIG	64
#define MTEX_COMPAT_BUMP	128
#define MTEX_3TAP_BUMP		256
#define MTEX_5TAP_BUMP		512
#define MTEX_BUMP_OBJECTSPACE	1024
#define MTEX_BUMP_TEXTURESPACE	2048
/* #define MTEX_BUMP_FLIPPED 	4096 */ /* UNUSED */
#define MTEX_TIPS				4096  /* should use with_freestyle flag?  */
#define MTEX_BICUBIC_BUMP		8192
#define MTEX_MAPTO_BOUNDS		16384

/* blendtype */
#define MTEX_BLEND		0
#define MTEX_MUL		1
#define MTEX_ADD		2
#define MTEX_SUB		3
#define MTEX_DIV		4
#define MTEX_DARK		5
#define MTEX_DIFF		6
#define MTEX_LIGHT		7
#define MTEX_SCREEN		8
#define MTEX_OVERLAY	9
#define MTEX_BLEND_HUE		10
#define MTEX_BLEND_SAT		11
#define MTEX_BLEND_VAL		12
#define MTEX_BLEND_COLOR	13
#define MTEX_SOFT_LIGHT     15 
#define MTEX_LIN_LIGHT      16

/* brush_map_mode */
#define MTEX_MAP_MODE_VIEW     0
#define MTEX_MAP_MODE_TILED    1
#define MTEX_MAP_MODE_3D       2
#define MTEX_MAP_MODE_AREA     3
#define MTEX_MAP_MODE_RANDOM   4
#define MTEX_MAP_MODE_STENCIL  5

/* **************** ColorBand ********************* */

/* colormode */
enum {
	COLBAND_BLEND_RGB   = 0,
	COLBAND_BLEND_HSV   = 1,
	COLBAND_BLEND_HSL   = 2,
};

/* interpolation */
enum {
	COLBAND_INTERP_LINEAR       = 0,
	COLBAND_INTERP_EASE         = 1,
	COLBAND_INTERP_B_SPLINE     = 2,
	COLBAND_INTERP_CARDINAL     = 3,
	COLBAND_INTERP_CONSTANT     = 4,
};

/* color interpolation */
enum {
	COLBAND_HUE_NEAR    = 0,
	COLBAND_HUE_FAR     = 1,
	COLBAND_HUE_CW      = 2,
	COLBAND_HUE_CCW     = 3,
};

/* **************** EnvMap ********************* */

/* type */
#define ENV_CUBE	0
#define ENV_PLANE	1
#define ENV_SPHERE	2

/* stype */
#define ENV_STATIC	0
#define ENV_ANIM	1
#define ENV_LOAD	2

/* ok */
#define ENV_NORMAL	1
#define ENV_OSA		2

/* **************** PointDensity ********************* */

/* source */
#define TEX_PD_PSYS			0
#define TEX_PD_OBJECT		1
#define TEX_PD_FILE			2

/* falloff_type */
#define TEX_PD_FALLOFF_STD		0
#define TEX_PD_FALLOFF_SMOOTH	1
#define TEX_PD_FALLOFF_SOFT		2
#define TEX_PD_FALLOFF_CONSTANT	3
#define TEX_PD_FALLOFF_ROOT		4
#define TEX_PD_FALLOFF_PARTICLE_AGE 5
#define TEX_PD_FALLOFF_PARTICLE_VEL 6

/* psys_cache_space */
#define TEX_PD_OBJECTLOC	0
#define TEX_PD_OBJECTSPACE	1
#define TEX_PD_WORLDSPACE	2

/* flag */
#define TEX_PD_TURBULENCE		1
#define TEX_PD_FALLOFF_CURVE	2

/* noise_influence */
#define TEX_PD_NOISE_STATIC		0
#define TEX_PD_NOISE_VEL		1
#define TEX_PD_NOISE_AGE		2
#define TEX_PD_NOISE_TIME		3

/* color_source */
#define TEX_PD_COLOR_CONSTANT	0
#define TEX_PD_COLOR_PARTAGE	1
#define TEX_PD_COLOR_PARTSPEED	2
#define TEX_PD_COLOR_PARTVEL	3

#define POINT_DATA_VEL		1
#define POINT_DATA_LIFE		2

/******************** Voxel Data *****************************/
/* flag */
#define TEX_VD_STILL			1

/* interpolation */
#define TEX_VD_NEARESTNEIGHBOR		0
#define TEX_VD_LINEAR				1
#define TEX_VD_QUADRATIC        2
#define TEX_VD_TRICUBIC_CATROM  3
#define TEX_VD_TRICUBIC_BSPLINE 4
#define TEX_VD_TRICUBIC_SLOW    5

/* file format */
#define TEX_VD_BLENDERVOXEL		0
#define TEX_VD_RAW_8BIT			1
#define TEX_VD_RAW_16BIT		2
#define TEX_VD_IMAGE_SEQUENCE	3
#define TEX_VD_SMOKE			4
/* for voxels which use VoxelData->source_path */
#define TEX_VD_IS_SOURCE_PATH(_format) (ELEM(_format, TEX_VD_BLENDERVOXEL, TEX_VD_RAW_8BIT, TEX_VD_RAW_16BIT))

/* smoke data types */
#define TEX_VD_SMOKEDENSITY		0
#define TEX_VD_SMOKEHEAT		1
#define TEX_VD_SMOKEVEL			2
#define TEX_VD_SMOKEFLAME		3

/* data_type */
#define TEX_VD_INTENSITY		0
#define TEX_VD_RGBA_PREMUL		1

/******************** Ocean *****************************/
/* output */
#define TEX_OCN_DISPLACEMENT	1
#define TEX_OCN_FOAM			2
#define TEX_OCN_JPLUS			3
#define TEX_OCN_EMINUS			4	
#define TEX_OCN_EPLUS			5

/* flag */
#define TEX_OCN_GENERATE_NORMALS	1	
#define TEX_OCN_XZ				2	
	
#ifdef __cplusplus
}
#endif

#endif

