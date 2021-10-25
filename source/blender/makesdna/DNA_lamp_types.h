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

/** \file DNA_lamp_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_LAMP_TYPES_H__
#define __DNA_LAMP_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"

#ifndef MAX_MTEX
#define MAX_MTEX	18
#endif

struct AnimData;
struct bNodeTree;
struct CurveMapping;
struct Ipo;
struct MTex;

typedef struct Lamp {
	ID id;
	struct AnimData *adt;	/* animation data (must be immediately after id for utilities to use it) */ 
	
	short type, flag;
	int mode;
	
	short colormodel, totex;
	float r, g, b, k;
	float shdwr, shdwg, shdwb, shdwpad;
	
	float energy, dist, spotsize, spotblend;
	float haint;
	
	
	float att1, att2;	/* Quad1 and Quad2 attenuation */
	float coeff_const, coeff_lin, coeff_quad, coeff_pad;
	struct CurveMapping *curfalloff;
	short falloff_type;
	short pad2;
	
	float clipsta, clipend;
	float bias, soft, compressthresh, bleedbias, pad5;
	short bufsize, samp, buffers, filtertype;
	char bufflag, buftype;
	
	short ray_samp, ray_sampy, ray_sampz;
	short ray_samp_type;
	short area_shape;
	float area_size, area_sizey, area_sizez;
	float adapt_thresh;
	short ray_samp_method;
	short shadowmap_type;
	
	/* texact is for buttons */
	short texact, shadhalostep;
	
	/* sun/sky */
	short sun_effect_type;
	short skyblendtype;
	float horizon_brightness;
	float spread;
	float sun_brightness;
	float sun_size;
	float backscattered_light;
	float sun_intensity;
	float atm_turbidity;
	float atm_inscattering_factor;
	float atm_extinction_factor;
	float atm_distance_factor;
	float skyblendfac;
	float sky_exposure;
	float shadow_frustum_size;		/* BGE Only */
	short sky_colorspace;
	char pad4[2];

	struct Ipo *ipo  DNA_DEPRECATED;  /* old animation system, deprecated for 2.5 */
	struct MTex *mtex[18];			/* MAX_MTEX */
	short pr_texture, use_nodes;
	char pad6[4];

	/* preview */
	struct PreviewImage *preview;

	/* nodes */
	struct bNodeTree *nodetree;
} Lamp;

/* **************** LAMP ********************* */

/* flag */
#define LA_DS_EXPAND	1
	/* NOTE: this must have the same value as MA_DS_SHOW_TEXS, 
	 * otherwise anim-editors will not read correctly
	 */
#define LA_DS_SHOW_TEXS	4

/* type */
#define LA_LOCAL		0
#define LA_SUN			1
#define LA_SPOT			2
#define LA_HEMI			3
#define LA_AREA			4

/* mode */
#define LA_SHAD_BUF		(1 << 0)
#define LA_HALO			(1 << 1)
#define LA_LAYER		(1 << 2)
#define LA_QUAD			(1 << 3) /* no longer used */
#define LA_NEG			(1 << 4)
#define LA_ONLYSHADOW	(1 << 5)
#define LA_SPHERE		(1 << 6)
#define LA_SQUARE		(1 << 7)
#define LA_TEXTURE		(1 << 8)
#define LA_OSATEX		(1 << 9)
/* #define LA_DEEP_SHADOW	(1 << 10) */ /* not used anywhere */
#define LA_NO_DIFF		(1 << 11)
#define LA_NO_SPEC		(1 << 12)
#define LA_SHAD_RAY		(1 << 13)
/* yafray: lamp shadowbuffer flag, softlight */
/* Since it is used with LOCAL lamp, can't use LA_SHAD */
/* #define LA_YF_SOFT		(1 << 14) */ /* no longer used */
#define LA_LAYER_SHADOW	(1 << 15)
#define LA_SHAD_TEX     (1 << 16)
#define LA_SHOW_CONE    (1 << 17)
#define LA_SHOW_SHADOW_BOX (1 << 18)

/* layer_shadow */
#define LA_LAYER_SHADOW_BOTH	0
#define LA_LAYER_SHADOW_CAST	1
#define LA_LAYER_SHADOW_RECEIVE	2

/* sun effect type*/
#define LA_SUN_EFFECT_SKY			1
#define LA_SUN_EFFECT_AP			2

/* falloff_type */
#define LA_FALLOFF_CONSTANT			0
#define LA_FALLOFF_INVLINEAR		1
#define LA_FALLOFF_INVSQUARE		2
#define LA_FALLOFF_CURVE			3
#define LA_FALLOFF_SLIDERS			4
#define LA_FALLOFF_INVCOEFFICIENTS	5


/* buftype, no flag */
#define LA_SHADBUF_REGULAR		0
#define LA_SHADBUF_IRREGULAR	1
#define LA_SHADBUF_HALFWAY		2
#define LA_SHADBUF_DEEP			3

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

/* ray_samp_method */
#define LA_SAMP_CONSTANT			0
#define LA_SAMP_HALTON				1
#define LA_SAMP_HAMMERSLEY			2


/* ray_samp_type */
// #define LA_SAMP_ROUND	1  // UNUSED
#define LA_SAMP_UMBRA	2
#define LA_SAMP_DITHER	4
#define LA_SAMP_JITTER	8

/* mapto */
#define LAMAP_COL		1
#define LAMAP_SHAD		2

/* shadowmap_type */
#define LA_SHADMAP_SIMPLE	0
#define LA_SHADMAP_VARIANCE	1

#endif /* __DNA_LAMP_TYPES_H__ */

