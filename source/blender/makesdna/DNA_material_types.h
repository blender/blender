/**
 * blenlib/DNA_material_types.h (mar-2001 nzc)
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
#ifndef DNA_MATERIAL_TYPES_H
#define DNA_MATERIAL_TYPES_H

/*  #include "BLI_listBase.h" */

#include "DNA_ID.h"
#include "DNA_scriptlink_types.h"

struct MTex;
struct Ipo;
struct Material;

/* WATCH IT: change type? also make changes in ipo.h  */

typedef struct Material {
	ID id;
	
	short colormodel, lay;		/* lay: for dynamics (old engine, until 2.04) */
	float r, g, b;
	float specr, specg, specb;
	float mirr, mirg, mirb;
	float ambr, ambb, ambg;
	
	float amb, emit, ang, spectra, ray_mirror;
	float alpha, ref, spec, zoffs, add;
	float translucency;
	float fresnel_mir, fresnel_mir_i;
	float fresnel_tra, fresnel_tra_i;
	short ray_depth, ray_depth_tra;
	short har;
	char seed1, seed2;
	
	int mode; 
	int mode2; /* even more material settings :) */
	short flarec, starc, linec, ringc;
	float hasize, flaresize, subsize, flareboost;
	
	/* for buttons and render*/
	char rgbsel, texact, pr_type, septex;
	short pr_back, pr_lamp;

	/* shaders */
	short diff_shader, spec_shader;
	float roughness, refrac;
	float param[4];		/* size, smooth, size, smooth, for toonshader */
	short texco, mapto;
	
	struct MTex *mtex[8];
	struct Ipo *ipo;
	struct Material *ren;
	
	/* dynamic properties */
	float friction, fh, reflect;
	float fhdist, xyfrict;
	short dynamode, pad2;
	
	ScriptLink scriptlink;
} Material;

/* **************** MATERIAL ********************* */

	/* maximum number of materials per material array
	 * (on object, mesh, lamp, etc.)
	 */
#define MAXMAT			16

/* colormodel */
#define MA_RGB			0
#define MA_CMYK			1
#define MA_YUV			2
#define MA_HSV			3

/* mode (is int) */
#define MA_TRACEBLE		1
#define MA_SHADOW		2
#define MA_SHLESS		4
#define MA_WIRE			8
#define MA_VERTEXCOL	16
#define MA_HALO			32
#define MA_ZTRA			64
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
#define MA_NOMIST		0x4000
#define MA_HALO_SHADE	0x4000
#define MA_HALO_FLARE	0x8000
#define MA_RADIO		0x10000
#define MA_RAYTRANSP	0x20000
#define MA_RAYMIRROR	0x40000
#define MA_SHADOW_TRA	0x80000

/* diff_shader */
#define MA_DIFF_LAMBERT		0
#define MA_DIFF_ORENNAYAR	1
#define MA_DIFF_TOON		2

/* spec_shader */
#define MA_SPEC_COOKTORR	0
#define MA_SPEC_PHONG		1
#define MA_SPEC_BLINN		2
#define MA_SPEC_TOON		3

/* dynamode */
#define MA_DRAW_DYNABUTS    1
#define MA_FH_NOR	        2

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
#define MAP_RAYMIRR		512
#define MAP_TRANSLU		1024
#define MAP_AMB			2048
#define MAP_DISPLACE	4096

/* pr_type */
#define MA_FLAT			0
#define MA_SPHERE		1
#define MA_CUBE			2

/* pr_back */
#define MA_DARK			1

#endif

