/**
 * blenlib/DNA_lamp_types.h (mar-2001 nzc)
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
#ifndef DNA_LAMP_TYPES_H
#define DNA_LAMP_TYPES_H

#include "DNA_ID.h"

#ifndef MAX_MTEX
#define MAX_MTEX	18
#endif

struct MTex;
struct CurveMapping;
struct AnimData;
struct Ipo;

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
	struct CurveMapping *curfalloff;
	short falloff_type;
	short pad2;
	
	float clipsta, clipend, shadspotsize;
	float bias, soft, compressthresh, pad5[3];
	short bufsize, samp, buffers, filtertype;
	char bufflag, buftype;
	
	short ray_samp, ray_sampy, ray_sampz;
	short ray_samp_type;
	short area_shape;
	float area_size, area_sizey, area_sizez;
	float adapt_thresh;
	short ray_samp_method;
	short pad1;
	
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
	short sky_colorspace, pad4;
	
	/* yafray: photonlight params */
	int YF_numphotons, YF_numsearch;
	short YF_phdepth, YF_useqmc, YF_bufsize, YF_pad;
	float YF_causticblur, YF_ltradius;
	/* yafray: glow params */
	float YF_glowint, YF_glowofs;
	short YF_glowtype, YF_pad2;
	
	struct Ipo *ipo;				// XXX depreceated... old animation system
	struct MTex *mtex[18];			/* MAX_MTEX */
	short pr_texture, pad[3];
	
	/* preview */
	struct PreviewImage *preview;
} Lamp;

/* **************** LAMP ********************* */

/* flag */
#define LA_DS_EXPAND	1

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
#define LA_QUAD			8	/* no longer used */
#define LA_NEG			16
#define LA_ONLYSHADOW	32
#define LA_SPHERE		64
#define LA_SQUARE		128
#define LA_TEXTURE		256
#define LA_OSATEX		512
/* #define LA_DEEP_SHADOW	1024 */ /* not used anywhere */
#define LA_NO_DIFF		2048
#define LA_NO_SPEC		4096
#define LA_SHAD_RAY		8192
/* yafray: lamp shadowbuffer flag, softlight */
/* Since it is used with LOCAL lamp, can't use LA_SHAD */
#define LA_YF_SOFT		16384
#define LA_LAYER_SHADOW	32768
#define LA_SHAD_TEX     (1<<16)
#define LA_SHOW_CONE    (1<<17)

/* layer_shadow */
#define LA_LAYER_SHADOW_BOTH	0
#define LA_LAYER_SHADOW_CAST	1
#define LA_LAYER_SHADOW_RECEIVE	2

/* sun effect type*/
#define LA_SUN_EFFECT_SKY			1
#define LA_SUN_EFFECT_AP			2

/* falloff_type */
#define LA_FALLOFF_CONSTANT		0
#define LA_FALLOFF_INVLINEAR		1
#define LA_FALLOFF_INVSQUARE	2
#define LA_FALLOFF_CURVE		3
#define LA_FALLOFF_SLIDERS		4


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
#define LA_SAMP_ROUND	1
#define LA_SAMP_UMBRA	2
#define LA_SAMP_DITHER	4
#define LA_SAMP_JITTER	8

/* mapto */
#define LAMAP_COL		1
#define LAMAP_SHAD		2


#endif /* DNA_LAMP_TYPES_H */

