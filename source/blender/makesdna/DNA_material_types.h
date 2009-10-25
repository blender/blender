/**
 * blenlib/DNA_material_types.h (mar-2001 nzc)
 *
 * $Id$ 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#ifndef DNA_MATERIAL_TYPES_H
#define DNA_MATERIAL_TYPES_H

#include "DNA_ID.h"
#include "DNA_listBase.h"

#ifndef MAX_MTEX
#define MAX_MTEX	18
#endif

struct MTex;
struct ColorBand;
struct Group;
struct bNodeTree;
struct AnimData;
struct Ipo;

/* WATCH IT: change type? also make changes in ipo.h  */

typedef struct VolumeSettings {
	float density;
	float emission;
	float scattering;
	float reflection;

	float emission_col[3];
	float transmission_col[3];
	float reflection_col[3];

	float density_scale;
	float depth_cutoff;
	float asymmetry;
	
	short stepsize_type;
	short shadeflag;
	short shade_type;
	short precache_resolution;

	float stepsize;
	float ms_diff;
	float ms_intensity;
	int ms_steps;
} VolumeSettings;

typedef struct Material {
	ID id;
	struct AnimData *adt;	/* animation data (must be immediately after id for utilities to use it) */ 
	
	short material_type, flag;	
	/* note, keep this below synced with render_types.h */
	float r, g, b;
	float specr, specg, specb;
	float mirr, mirg, mirb;
	float ambr, ambb, ambg;
	float amb, emit, ang, spectra, ray_mirror;
	float alpha, ref, spec, zoffs, add;
	float translucency;
	/* end synced with render_types.h */
	
	struct VolumeSettings vol;

	float fresnel_mir, fresnel_mir_i;
	float fresnel_tra, fresnel_tra_i;
	float filter;		/* filter added, for raytrace transparency and transmissivity */
	float tx_limit, tx_falloff;
	short ray_depth, ray_depth_tra;
	short har;
	char seed1, seed2;
	
	float gloss_mir, gloss_tra;
	short samp_gloss_mir, samp_gloss_tra;
	float adapt_thresh_mir, adapt_thresh_tra;
	float aniso_gloss_mir;
	float dist_mir;
	short fadeto_mir;
	short shade_flag;		/* like Cubic interpolation */
		
	int mode, mode_l;		/* mode_l is the or-ed result of all layer modes */
	short flarec, starc, linec, ringc;
	float hasize, flaresize, subsize, flareboost;
	float strand_sta, strand_end, strand_ease, strand_surfnor;
	float strand_min, strand_widthfade;
	char strand_uvname[32];
	
	float sbias;			/* shadow bias to prevent terminator prob */
	float lbias;			/* factor to multiply lampbias with (0.0 = no mult) */
	float shad_alpha;		/* in use for irregular shadowbuffer */
	int	septex;
	
	/* for buttons and render*/
	char rgbsel, texact, pr_type, use_nodes;
	short pr_back, pr_lamp, pr_texture, ml_flag;	/* ml_flag is for disable base material */
	
	/* shaders */
	short diff_shader, spec_shader;
	float roughness, refrac;
	/* XXX param[4] needs review and improvement (shader system as whole anyway)
	   This is nasty reused variable for different goals and not easy to RNAify nicely. -jesterKing */
	float param[4];		/* size, smooth, size, smooth, for toonshader, 0 (fac) and 1 (fresnel) also for fresnel shader */
	float rms;
	float darkness;
	short texco, mapto;
	
	/* ramp colors */
	struct ColorBand *ramp_col;
	struct ColorBand *ramp_spec;
	char rampin_col, rampin_spec;
	char rampblend_col, rampblend_spec;
	short ramp_show, pad3;
	float rampfac_col, rampfac_spec;

	struct MTex *mtex[18];		/* MAX_MTEX */
	struct bNodeTree *nodetree;	
	struct Ipo *ipo;		// XXX depreceated... old animation system
	struct Group *group;	/* light group */
	struct PreviewImage * preview;

	/* dynamic properties */
	float friction, fh, reflect;
	float fhdist, xyfrict;
	short dynamode, pad2;

	/* subsurface scattering */
	float sss_radius[3], sss_col[3];
	float sss_error, sss_scale, sss_ior;
	float sss_colfac, sss_texfac;
	float sss_front, sss_back;
	short sss_flag, sss_preset;

	int mapto_textured;	/* render-time cache to optimise texture lookups */
	int pad4;

	ListBase gpumaterial;		/* runtime */
} Material;

/* **************** MATERIAL ********************* */

/* maximum number of materials per material array.
 * (on object, mesh, lamp, etc.). limited by
 * short mat_nr in verts, faces. */
#define MAXMAT			32767

/* material_type */
#define MA_TYPE_SURFACE	0
#define MA_TYPE_HALO	1
#define MA_TYPE_VOLUME	2
#define MA_TYPE_WIRE	3

/* flag */
		/* for render */
#define MA_IS_USED		1
		/* for dopesheet */
#define MA_DS_EXPAND	2

/* mode (is int) */
#define MA_TRACEBLE		1
#define MA_SHADOW		2
#define MA_SHLESS		4
#define MA_WIRE			8			/* deprecated */
#define MA_VERTEXCOL	16
#define MA_HALO_SOFT	16
#define MA_HALO			32			/* deprecated */
#define MA_ZTRANSP		64
#define MA_VERTEXCOLP	128
#define MA_ZINV			256
#define MA_HALO_RINGS	256
#define MA_ENV			512
#define MA_HALO_LINES	512
#define MA_ONLYSHADOW	1024
#define MA_HALO_XALPHA	1024
#define MA_STAR			0x800
#define MA_FACETEXTURE	0x800
#define MA_HALOTEX		0x1000
#define MA_HALOPUNO		0x2000
#define MA_ONLYCAST		0x2000
#define MA_NOMIST		0x4000
#define MA_HALO_SHADE	0x4000
#define MA_HALO_FLARE	0x8000
#define MA_TRANSP		0x10000
#define MA_RAYTRANSP	0x20000
#define MA_RAYMIRROR	0x40000
#define MA_SHADOW_TRA	0x80000
#define MA_RAMP_COL		0x100000
#define MA_RAMP_SPEC	0x200000
#define MA_RAYBIAS		0x400000
#define MA_FULL_OSA		0x800000
#define MA_TANGENT_STR	0x1000000
#define MA_SHADBUF		0x2000000
		/* note; we drop MA_TANGENT_STR later to become tangent_u */
#define MA_TANGENT_V	0x4000000
/* qdn: a bit clumsy this, tangents needed for normal maps separated from shading */
#define MA_NORMAP_TANG	0x8000000
#define MA_GROUP_NOLAY	0x10000000
#define MA_FACETEXTURE_ALPHA	0x20000000
#define MA_STR_B_UNITS	0x40000000
#define MA_STR_SURFDIFF 0x80000000

#define	MA_MODE_MASK	0x6fffffff	/* all valid mode bits */

/* ray mirror fadeout */
#define MA_RAYMIR_FADETOSKY	0
#define MA_RAYMIR_FADETOMAT	1

/* shade_flag */
#define MA_CUBIC			1
#define MA_OBCOLOR			2

/* diff_shader */
#define MA_DIFF_LAMBERT		0
#define MA_DIFF_ORENNAYAR	1
#define MA_DIFF_TOON		2
#define MA_DIFF_MINNAERT    3
#define MA_DIFF_FRESNEL     4

/* spec_shader */
#define MA_SPEC_COOKTORR	0
#define MA_SPEC_PHONG		1
#define MA_SPEC_BLINN		2
#define MA_SPEC_TOON		3
#define MA_SPEC_WARDISO		4

/* dynamode */
#define MA_DRAW_DYNABUTS    1		/* deprecated */
#define MA_FH_NOR	        2

/* ramps */
#define MA_RAMP_IN_SHADER	0
#define MA_RAMP_IN_ENERGY	1
#define MA_RAMP_IN_NOR		2
#define MA_RAMP_IN_RESULT	3

#define MA_RAMP_BLEND		0
#define MA_RAMP_ADD			1
#define MA_RAMP_MULT		2
#define MA_RAMP_SUB			3
#define MA_RAMP_SCREEN		4
#define MA_RAMP_DIV			5
#define MA_RAMP_DIFF		6
#define MA_RAMP_DARK		7
#define MA_RAMP_LIGHT		8
#define MA_RAMP_OVERLAY		9
#define MA_RAMP_DODGE		10
#define MA_RAMP_BURN		11
#define MA_RAMP_HUE			12
#define MA_RAMP_SAT			13
#define MA_RAMP_VAL			14
#define MA_RAMP_COLOR		15
#define MA_RAMP_SOFT        16 
#define MA_RAMP_LINEAR      17 

/* texco */
#define TEXCO_ORCO		1
#define TEXCO_REFL		2
#define TEXCO_NORM		4
#define TEXCO_GLOB		8
#define TEXCO_UV		16
#define TEXCO_OBJECT	32
#define TEXCO_LAVECTOR	64
#define TEXCO_VIEW		128
#define TEXCO_STICKY	256
#define TEXCO_OSA		512
#define TEXCO_WINDOW	1024
#define NEED_UV			2048
#define TEXCO_TANGENT	4096
	/* still stored in vertex->accum, 1 D */
#define TEXCO_STRAND	8192
#define TEXCO_STRESS	16384
#define TEXCO_SPEED		32768

/* mapto */
#define MAP_COL			1
#define MAP_NORM		2
#define MAP_COLSPEC		4
#define MAP_COLMIR		8
#define MAP_VARS		(0xFFF0)
#define MAP_REF			16
#define MAP_SPEC		32
#define MAP_EMIT		64
#define MAP_ALPHA		128
#define MAP_HAR			256
#define MAP_RAYMIRR		512
#define MAP_TRANSLU		1024
#define MAP_AMB			2048
#define MAP_DISPLACE	4096
#define MAP_WARP		8192
#define MAP_LAYER		16384		/* unused */

/* volume mapto - reuse definitions for now - a bit naughty! */
#define MAP_DENSITY				128
#define MAP_EMISSION			64
#define MAP_EMISSION_COL		1
#define MAP_SCATTERING			16
#define MAP_TRANSMISSION_COL	8
#define MAP_REFLECTION_COL		4
#define MAP_REFLECTION			32


/* mapto for halo */
//#define MAP_HA_COL		1
//#define MAP_HA_ALPHA	128
//#define MAP_HA_HAR		256
//#define MAP_HA_SIZE		2
//#define MAP_HA_ADD		64

/* pmapto */
/* init */
#define MAP_PA_INIT		31
#define MAP_PA_TIME		1
#define MAP_PA_LIFE		2
#define MAP_PA_DENS		4
#define MAP_PA_SIZE		8
#define MAP_PA_LENGTH	16
/* reset */
#define MAP_PA_IVEL		32
/* physics */
#define MAP_PA_PVEL		64
/* path cache */
#define MAP_PA_CACHE	912
#define MAP_PA_CLUMP	128
#define MAP_PA_KINK		256
#define MAP_PA_ROUGH	512

/* pr_type */
#define MA_FLAT			0
#define MA_SPHERE		1
#define MA_CUBE			2
#define MA_MONKEY		3
#define MA_SPHERE_A		4
#define MA_TEXTURE		5
#define MA_LAMP			6
#define MA_SKY			7
#define MA_HAIR			10
#define MA_ATMOS		11

/* pr_back */
#define MA_DARK			1

/* sss_flag */
#define MA_DIFF_SSS		1

/* vol_stepsize_type */
#define MA_VOL_STEP_RANDOMIZED	0
#define MA_VOL_STEP_CONSTANT	1
#define MA_VOL_STEP_ADAPTIVE	2

/* vol_shadeflag */
#define MA_VOL_RECV_EXT_SHADOW	1
#define MA_VOL_PRECACHESHADING	8

/* vol_shading_type */
#define MA_VOL_SHADE_SHADELESS					0
#define MA_VOL_SHADE_SHADOWED					2
#define MA_VOL_SHADE_SHADED						1
#define MA_VOL_SHADE_MULTIPLE					3
#define MA_VOL_SHADE_SHADEDPLUSMULTIPLE			4

#endif

