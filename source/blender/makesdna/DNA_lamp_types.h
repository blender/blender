/**
 * blenlib/DNA_lamp_types.h (mar-2001 nzc)
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
#ifndef DNA_LAMP_TYPES_H
#define DNA_LAMP_TYPES_H

#include "DNA_ID.h"
#include "DNA_scriptlink_types.h"

#ifndef MAX_MTEX
#define MAX_MTEX	10
#endif

struct MTex;
struct Ipo;

typedef struct Lamp {
	ID id;
	
	short type, mode;
	
	short colormodel, totex;
	float r, g, b, k;
	
	float energy, dist, spotsize, spotblend;
	float haint;
	float att1, att2;	/* Quad1 and Quad2 attenuation */
	
	float clipsta, clipend, shadspotsize;
	float bias, soft;
	short bufsize, samp, buffers, filtertype;
	char bufflag, buftype;
	
	short ray_samp, ray_sampy, ray_sampz, ray_samp_type;
	short area_shape;
	float area_size, area_sizey, area_sizez;
	
	/* texact is for buttons */
	short texact, shadhalostep;
	
	/* yafray: photonlight params */
	int YF_numphotons, YF_numsearch;
	short YF_phdepth, YF_useqmc, YF_bufsize, YF_pad;
	float YF_causticblur, YF_ltradius;
	/* yafray: glow params */
	float YF_glowint, YF_glowofs;
	short YF_glowtype, YF_pad2;
	
	struct MTex *mtex[10];
	struct Ipo *ipo;
	
	ScriptLink scriptlink;
} Lamp;

/* **************** LAMP ********************* */

/* type */
#define LA_LOCAL		0
#define LA_SUN			1
#define LA_SPOT			2
#define LA_HEMI			3
#define LA_AREA			4
/* yafray: extra lamp type used for caustic photonmap */
#define LA_YF_PHOTON	5

/* mode */
#define LA_SHAD_BUF		1
#define LA_HALO			2
#define LA_LAYER		4
#define LA_QUAD			8
#define LA_NEG			16
#define LA_ONLYSHADOW	32
#define LA_SPHERE		64
#define LA_SQUARE		128
#define LA_TEXTURE		256
#define LA_OSATEX		512
#define LA_DEEP_SHADOW	1024
#define LA_NO_DIFF		2048
#define LA_NO_SPEC		4096
#define LA_SHAD_RAY		8192
/* yafray: lamp shadowbuffer flag, softlight */
/* Since it is used with LOCAL lamp, can't use LA_SHAD */
#define LA_YF_SOFT		16384

/* buftype, no flag */
#define LA_SHADBUF_REGULAR		0
#define LA_SHADBUF_IRREGULAR	1
#define LA_SHADBUF_HALFWAY		2

/* bufflag, auto clipping */
#define LA_SHADBUF_AUTO_START	1
#define LA_SHADBUF_AUTO_END		2

/* filtertype */
#define LA_SHADBUF_BOX		0
#define LA_SHADBUF_TENT		1
#define LA_SHADBUF_GAUSS	2

/* area shape */
#define LA_AREA_SQUARE	0
#define LA_AREA_RECT	1
#define LA_AREA_CUBE	2
#define LA_AREA_BOX		3

/* ray_samp_type */
#define LA_SAMP_ROUND	1
#define LA_SAMP_UMBRA	2
#define LA_SAMP_DITHER	4
#define LA_SAMP_JITTER	8

/* mapto */
#define LAMAP_COL		1


#endif /* DNA_LAMP_TYPES_H */

