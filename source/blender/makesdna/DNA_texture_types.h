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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
	int cuberes;
	short ok, lastframe;
} EnvMap;

typedef struct Tex {
	ID id;
	
	float noisesize, turbul;
	float bright, contrast, rfac, gfac, bfac;
	float filtersize;
	short noisedepth, noisetype;
	
	short imaflag, flag;
	short type, stype;
	
	float cropxmin, cropymin, cropxmax, cropymax;
	short xrepeat, yrepeat;
	short extend, len;
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
#define TEX_COLORBAND	1
#define TEX_FLIPBLEND	2
#define TEX_NEGALPHA	4

/* extend (begint bij 1 ivm backward comp.) */
#define TEX_EXTEND		1
#define TEX_CLIP		2
#define TEX_REPEAT		3
#define TEX_CLIPCUBE	4

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
#define MAP_XTRA		512

/* pr_type */
#define MA_FLAT			0
#define MA_SPHERE		1
#define MA_CUBE			2

/* pr_back */
#define MA_DARK			1

/* pr_lamp	*/

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

