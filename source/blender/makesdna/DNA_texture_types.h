/**
 * blenlib/DNA_texture_types.h (mar-2001 nzc)
 *
 * $Id$ 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef DNA_TEXTURE_TYPES_H
#define DNA_TEXTURE_TYPES_H

#include "DNA_ID.h"

struct Ipo;
struct PluginTex;
struct ColorBand;
struct EnvMap;
struct Object;
struct Tex;
struct Image;

typedef struct MTex {

	short texco, mapto, maptoneg, blendtype;
	struct Object *object;
	struct Tex *tex;
	
	char projx, projy, projz, mapping;
	float ofs[3], size[3];
	
	short texflag, colormodel;
	float r, g, b, k;
	float def_var;
	
	float colfac, norfac, varfac;
	float dispfac; 
	short pad1, pad2; 
	
} MTex;

#ifndef DNA_USHORT_FIX
#define DNA_USHORT_FIX
/**
 * @deprecated This typedef serves to avoid badly typed functions when
 * @deprecated compiling while delivering a proper dna.c. Do not use
 * @deprecated it in any case.
 */
typedef unsigned short dna_ushort_fix;
#endif

typedef struct PluginTex {
	char name[160];
	void *handle;
	
	char *pname;
	char *stnames;

	int stypes;
	int vars;
	void *varstr;
	float *result;
	float *cfra;
	
	float data[32];

	int (*doit)(void);

	/* should be void (*)(unsigned short)... patched */	
	void (*callback)(dna_ushort_fix);
	
	int version, pad;
} PluginTex;

typedef struct CBData {
	float r, g, b, a, pos;
	int cur;
} CBData;

typedef struct ColorBand {
	short flag, tot, cur, ipotype;
	CBData data[16];
	
} ColorBand;

typedef struct EnvMap {
	struct Object *object;
	struct Image *ima;		/* type ENV_LOAD */
	struct Image *cube[6];		/* these images are dynamic, not part of the main struct */
	float imat[4][4];
	short type, stype;
	float clipsta, clipend;
	unsigned int notlay;
	short cuberes, depth;
	short ok, lastframe;
	short recalc, lastsize;
	int pad1;
} EnvMap;

typedef struct Tex {
	ID id;
	
	float noisesize, turbul;
	float bright, contrast, rfac, gfac, bfac;
	float filtersize;

	/* newnoise: musgrave parameters */
	float mg_H, mg_lacunarity, mg_octaves, mg_offset, mg_gain;

	/* newnoise: distorted noise amount, musgrave & voronoi ouput scale */
	float dist_amount, ns_outscale;

	/* newnoise: voronoi nearest neighbour weights, minkovsky exponent, distance metric & color type */
	float vn_w1, vn_w2, vn_w3, vn_w4, vn_mexp;
	short vn_distm, vn_coltype;

	short noisedepth, noisetype;

	/* newnoise: noisebasis type for clouds/marble/etc, noisebasis2 only used for distorted noise */
	short noisebasis, noisebasis2;
	
	short imaflag, flag;
	short type, stype;
	
	float cropxmin, cropymin, cropxmax, cropymax;
	short xrepeat, yrepeat;
	short extend, len;
	float checkerdist, pad;
	short frames, offset, sfra, fie_ima;
	float norfac, *nor;
	
	struct Ipo *ipo;
	struct Image *ima;
	struct PluginTex *plugin;
	struct ColorBand *coba;
	struct EnvMap *env;
	
	short fradur[4][2];
	
} Tex;

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
#define TEX_PLUGIN		9
#define TEX_ENVMAP		10
#define TEX_MUSGRAVE	11
#define TEX_VORONOI		12
#define TEX_DISTNOISE	13

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
#define TEX_FIELDS		8
#define TEX_IMAROT		16
#define TEX_CALCALPHA	32
#define TEX_ANIMCYCLIC	64
#define TEX_ANIM5		128
#define TEX_ANTIALI		256
#define TEX_ANTISCALE	512
#define TEX_STD_FIELD	1024

#define TEX_LASOPPATCH	8192
#define TEX_MORKPATCH	16384

/* flag */
#define TEX_COLORBAND		1
#define TEX_FLIPBLEND		2
#define TEX_NEGALPHA		4
#define TEX_CHECKER_ODD		8
#define TEX_CHECKER_EVEN	16

/* extend (starts with 1 because of backward comp.) */
#define TEX_EXTEND		1
#define TEX_CLIP		2
#define TEX_REPEAT		3
#define TEX_CLIPCUBE	4
#define TEX_CHECKER		5

/* noisetype */
#define TEX_NOISESOFT	0
#define TEX_NOISEPERL	1

/* wrap */
#define MTEX_FLAT		0
#define MTEX_CUBE		1
#define MTEX_TUBE		2
#define MTEX_SPHERE		3

/* return value */
#define TEX_INT		0
#define TEX_RGB		1
#define TEX_NOR		2

/* **************** MTEX ********************* */

/* proj */
#define PROJ_N			0
#define PROJ_X			1
#define PROJ_Y			2
#define PROJ_Z			3

/* texflag */
#define MTEX_RGBTOINT	1
#define MTEX_STENCIL	2
#define MTEX_NEGATIVE	4
#define MTEX_ALPHAMIX	8

/* blendtype */
#define MTEX_BLEND		0
#define MTEX_MUL		1
#define MTEX_ADD		2
#define MTEX_SUB		3

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

#endif

