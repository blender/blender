/* texture.c
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MTC_matrixops.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "DNA_texture_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_image_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BKE_osa_types.h"
#include "BKE_plugin_types.h"
#include "BKE_utildefines.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BKE_library.h"
#include "BKE_image.h"
#include "BKE_texture.h"
#include "BKE_key.h"
#include "BKE_ipo.h"

#include "render.h"
#include "rendercore.h"
#include "envmap.h"

/* These vars form the texture channel */
float Tin, Tr, Tg, Tb, Ta, Txtra;
extern int Talpha;


/* ------------------------------------------------------------------------- */

void init_render_texture(Tex *tex)
{
	Image *ima;
	int imanr;
	unsigned short numlen;
	char name[FILE_MAXDIR+FILE_MAXFILE], head[FILE_MAXDIR+FILE_MAXFILE], tail[FILE_MAXDIR+FILE_MAXFILE];

	/* is also used as signal */
	tex->nor= NULL;

	/* imap test */
	if(tex->frames && tex->ima && tex->ima->name) {	/* frames */
		strcpy(name, tex->ima->name);
		
		imanr= calcimanr(G.scene->r.cfra, tex);
		
		if(tex->imaflag & TEX_ANIM5) {
			if(tex->ima->lastframe != imanr) {
				if(tex->ima->ibuf) IMB_freeImBuf(tex->ima->ibuf);
				tex->ima->ibuf= 0;
				tex->ima->lastframe= imanr;
			}
		}
		else {
				/* for patch field-ima rendering */
			tex->ima->lastframe= imanr;
			
			BLI_stringdec(name, head, tail, &numlen);
			BLI_stringenc(name, head, tail, numlen, imanr);
	
			ima= add_image(name);

			if(ima) {
				ima->flag |= IMA_FROMANIM;
				
				if(tex->ima) tex->ima->id.us--;
				tex->ima= ima;
				
				ima->ok= 1;
			}
		}
	}
	if(tex->imaflag & (TEX_ANTIALI+TEX_ANTISCALE)) {
		if(tex->ima && tex->ima->lastquality<R.osa) {
			if(tex->ima->ibuf) IMB_freeImBuf(tex->ima->ibuf);
			tex->ima->ibuf= 0;
		}
	}
	
	if(tex->type==TEX_PLUGIN) {
		if(tex->plugin && tex->plugin->doit) {
				if(tex->plugin->cfra) {
 					*(tex->plugin->cfra)= frame_to_float(G.scene->r.cfra); 
				}
		}
	}
	else if(tex->type==TEX_ENVMAP) {
		/* just in case */
		tex->imaflag= TEX_INTERPOL | TEX_MIPMAP;
		tex->extend= TEX_CLIP;
		
		if(tex->env) {
			if(R.flag & R_RENDERING) {
				if(tex->env->stype==ENV_ANIM) RE_free_envmapdata(tex->env);
			}
		}
	}
}

/* ------------------------------------------------------------------------- */

void init_render_textures()
{
	Tex *tex;
	
	tex= G.main->tex.first;
	while(tex) {
		if(tex->id.us) init_render_texture(tex);
		tex= tex->id.next;
	}
	
	free_unused_animimages();
}

/* ------------------------------------------------------------------------- */

void end_render_texture(Tex *tex)
{


}

/* ------------------------------------------------------------------------- */

void end_render_textures()
{
	Tex *tex;

	tex= G.main->tex.first;
	while(tex) {
		if(tex->id.us) end_render_texture(tex);
		tex= tex->id.next;
	}

}

/* ------------------------------------------------------------------------- */

static int blend(Tex *tex, float *texvec)
{
	float x, y, t;

	if(tex->flag & TEX_FLIPBLEND) {
		x= texvec[1];
		y= texvec[0];
	}
	else {
		x= texvec[0];
		y= texvec[1];
	}

	if(tex->stype==0) {	/* lin */
		Tin= (1.0+x)/2.0;
	}
	else if(tex->stype==1) {	/* quad */
		Tin= (1.0+x)/2.0;
		if(Tin<0.0) Tin= 0.0;
		else Tin*= Tin;
	}
	else if(tex->stype==2) {	/* ease */
		Tin= (1.0+x)/2.0;
		if(Tin<=.0) Tin= 0.0;
		else if(Tin>=1.0) Tin= 1.0;
		else {
			t= Tin*Tin;
			Tin= (3.0*t-2.0*t*Tin);
		}
	}
	else if(tex->stype==3) { /* diag */
		Tin= (2.0+x+y)/4.0;
	}
	else {  /* sphere */
		Tin= 1.0-sqrt(x*x+	y*y+texvec[2]*texvec[2]);
		if(Tin<0.0) Tin= 0.0;
		if(tex->stype==5) Tin*= Tin;  /* halo */
	}

	BRICON;
	if(tex->flag & TEX_COLORBAND)  return do_colorband(tex->coba);

	return 0;
}

/* ------------------------------------------------------------------------- */
/* ************************************************************************* */
/* clouds, wood & marble updated to do proper bumpmapping */
/* 0.025 seems reasonable value for offset */
#define B_OFFS 0.025

/* newnoise: all noisebased types now have different noisebases to choose from */

static int clouds(Tex *tex, float *texvec)
{
	int rv=0;	/* return value, int:0, col:1, nor:2, everything:3 */
	Tin = BLI_gTurbulence(tex->noisesize, texvec[0], texvec[1], texvec[2], tex->noisedepth, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);

	if (tex->nor!=NULL) {
		// calculate bumpnormal
		tex->nor[0] = Tin - BLI_gTurbulence(tex->noisesize, texvec[0] + B_OFFS, texvec[1], texvec[2], tex->noisedepth,  (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		tex->nor[1] = Tin - BLI_gTurbulence(tex->noisesize, texvec[0], texvec[1] + B_OFFS, texvec[2], tex->noisedepth,  (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		tex->nor[2] = Tin - BLI_gTurbulence(tex->noisesize, texvec[0], texvec[1], texvec[2] + B_OFFS, tex->noisedepth,  (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		rv += 2;
	}

	if (tex->stype==1) {
		// in this case, int. value should really be computed from color,
		// and bumpnormal from that, would be too slow, looks ok as is
		Tr = Tin;
		Tg = BLI_gTurbulence(tex->noisesize, texvec[1], texvec[0], texvec[2], tex->noisedepth, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		Tb = BLI_gTurbulence(tex->noisesize, texvec[1], texvec[2], texvec[0], tex->noisedepth, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		BRICONRGB;
		Ta = 1.0;
		return (rv+1);
	}

	BRICON;

	if (tex->flag & TEX_COLORBAND)  return (rv + do_colorband(tex->coba));

	return rv;

}

/* computes basic wood intensity value at x,y,z */
static float wood_int(Tex *tex, float x, float y, float z)
{
	float wi=0;

	if (tex->stype==0)
		wi = 0.5 + 0.5*sin((x + y + z)*10.0);
	else if (tex->stype==1)
		wi = 0.5 + 0.5*sin(sqrt(x*x + y*y + z*z)*20.0);
	else if (tex->stype==2) {
		wi = BLI_gNoise(tex->noisesize, x, y, z, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		wi = 0.5 + 0.5*sin(tex->turbul*wi + (x + y + z)*10.0);
	}
	else if (tex->stype==3) {
		wi = BLI_gNoise(tex->noisesize, x, y, z, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		wi = 0.5 + 0.5*sin(tex->turbul*wi + (sqrt(x*x + y*y + z*z))*20.0);
	}

	return wi;
}

static int wood(Tex *tex, float *texvec)
{
	int rv=0;	/* return value, int:0, col:1, nor:2, everything:3 */

	Tin = wood_int(tex, texvec[0], texvec[1], texvec[2]);
	if (tex->nor!=NULL) {
		/* calculate bumpnormal */
		tex->nor[0] = Tin - wood_int(tex, texvec[0] + B_OFFS, texvec[1], texvec[2]);
		tex->nor[1] = Tin - wood_int(tex, texvec[0], texvec[1] + B_OFFS, texvec[2]);
		tex->nor[2] = Tin - wood_int(tex, texvec[0], texvec[1], texvec[2] + B_OFFS);
		rv += 2;
	}

	BRICON;
	if (tex->flag & TEX_COLORBAND)  return (rv + do_colorband(tex->coba));

	return rv;
}

/* computes basic marble intensity at x,y,z */
static float marble_int(Tex *tex, float x, float y, float z)
{
	float n, mi;

	n = 5.0 * (x + y + z);

	mi = 0.5 + 0.5 * sin(n + tex->turbul * BLI_gTurbulence(tex->noisesize, x, y, z, tex->noisedepth, (tex->noisetype!=TEX_NOISESOFT),  tex->noisebasis));
	if (tex->stype>=1) {
		mi = sqrt(mi);
		if (tex->stype==2) mi = sqrt(mi);
	}

	return mi;
}

static int marble(Tex *tex, float *texvec)
{
	int rv=0;	/* return value, int:0, col:1, nor:2, everything:3 */

	Tin = marble_int(tex, texvec[0], texvec[1], texvec[2]);

	if (tex->nor!=NULL) {
		/* calculate bumpnormal */
		tex->nor[0] = Tin - marble_int(tex, texvec[0] + B_OFFS, texvec[1], texvec[2]);
		tex->nor[1] = Tin - marble_int(tex, texvec[0], texvec[1] + B_OFFS, texvec[2]);
		tex->nor[2] = Tin - marble_int(tex, texvec[0], texvec[1], texvec[2] + B_OFFS);
		rv += 2;
	}

	BRICON;
	if (tex->flag & TEX_COLORBAND)  return (rv + do_colorband(tex->coba));

	return rv;
}

/* ------------------------------------------------------------------------- */

static int magic(Tex *tex, float *texvec)
{
	float x, y, z, turb=1.0;
	int n;

	n= tex->noisedepth;
	turb= tex->turbul/5.0;

	x=  sin( ( texvec[0]+texvec[1]+texvec[2])*5.0 );
	y=  cos( (-texvec[0]+texvec[1]-texvec[2])*5.0 );
	z= -cos( (-texvec[0]-texvec[1]+texvec[2])*5.0 );
	if(n>0) {
		x*= turb;
		y*= turb;
		z*= turb;
		y= -cos(x-y+z);
		y*= turb;
		if(n>1) {
			x= cos(x-y-z);
			x*= turb;
			if(n>2) {
				z= sin(-x-y-z);
				z*= turb;
				if(n>3) {
					x= -cos(-x+y-z);
					x*= turb;
					if(n>4) {
						y= -sin(-x+y+z);
						y*= turb;
						if(n>5) {
							y= -cos(-x+y+z);
							y*= turb;
							if(n>6) {
								x= cos(x+y+z);
								x*= turb;
								if(n>7) {
									z= sin(x+y-z);
									z*= turb;
									if(n>8) {
										x= -cos(-x-y+z);
										x*= turb;
										if(n>9) {
											y= -sin(x-y+z);
											y*= turb;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if(turb!=0.0) {
		turb*= 2.0;
		x/= turb; 
		y/= turb; 
		z/= turb;
	}
	Tr= 0.5-x;
	Tg= 0.5-y;
	Tb= 0.5-z;

	BRICONRGB;
	Ta= 1.0;
	
	return 1;
}

/* ------------------------------------------------------------------------- */

/* newnoise: stucci also modified to use different noisebasis */
static int stucci(Tex *tex, float *texvec)
{
	float b2, vec[3], ofs;

	if(tex->nor == NULL) return 0;

	ofs= tex->turbul/200.0;

	b2= BLI_gNoise(tex->noisesize, texvec[0], texvec[1], texvec[2], (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
	if(tex->stype) ofs*=(b2*b2);
	vec[0] = b2 - BLI_gNoise(tex->noisesize, texvec[0]+ofs, texvec[1], texvec[2], (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
	vec[1] = b2 - BLI_gNoise(tex->noisesize, texvec[0], texvec[1]+ofs, texvec[2], (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);	
	vec[2] = b2 - BLI_gNoise(tex->noisesize, texvec[0], texvec[1], texvec[2]+ofs, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);

	if(tex->stype==1) {
		tex->nor[0]= vec[0];
		tex->nor[1]= vec[1];
		tex->nor[2]= vec[2];
	}
	else {
		tex->nor[0]= -vec[0];
		tex->nor[1]= -vec[1];
		tex->nor[2]= -vec[2];
	}

	return 2;
}

/* ------------------------------------------------------------------------- */
/* newnoise: musgrave terrain noise types */

static float mg_mFractalOrfBmTex(Tex *tex, float *texvec)
{
	int rv=0;	/* return value, int:0, col:1, nor:2, everything:3 */
	float (*mgravefunc)(float, float, float, float, float, float, int);

	if (tex->stype==TEX_MFRACTAL)
		mgravefunc = mg_MultiFractal;
	else
		mgravefunc = mg_fBm;

	Tin = mgravefunc(texvec[0], texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->noisebasis);

	if (tex->nor!=NULL) {
		/* calculate bumpnormal */
		tex->nor[0] = Tin - mgravefunc(texvec[0] + B_OFFS, texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->noisebasis);
		tex->nor[1] = Tin - mgravefunc(texvec[0], texvec[1] + B_OFFS, texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->noisebasis);
		tex->nor[2] = Tin - mgravefunc(texvec[0], texvec[1], texvec[2] + B_OFFS, tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->noisebasis);
		rv += 2;
	}

	Tin *= tex->ns_outscale;

	BRICON;

	if (tex->flag & TEX_COLORBAND)  return (rv + do_colorband(tex->coba));

	return rv;

}

static float mg_ridgedOrHybridMFTex(Tex *tex, float *texvec)
{
	int rv=0;	/* return value, int:0, col:1, nor:2, everything:3 */
	float (*mgravefunc)(float, float, float, float, float, float, float, float, int);

	if (tex->stype==TEX_RIDGEDMF)
		mgravefunc = mg_RidgedMultiFractal;
	else
		mgravefunc = mg_HybridMultiFractal;

	Tin = mgravefunc(texvec[0], texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->mg_gain, tex->noisebasis);

	if (tex->nor!=NULL) {
		/* calculate bumpnormal */
		tex->nor[0] = Tin - mgravefunc(texvec[0] + B_OFFS, texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->mg_gain, tex->noisebasis);
		tex->nor[1] = Tin - mgravefunc(texvec[0], texvec[1] + B_OFFS, texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->mg_gain, tex->noisebasis);
		tex->nor[2] = Tin - mgravefunc(texvec[0], texvec[1], texvec[2] + B_OFFS, tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->mg_gain, tex->noisebasis);
		rv += 2;
	}

	Tin *= tex->ns_outscale;

	BRICON;

	if (tex->flag & TEX_COLORBAND)  return (rv + do_colorband(tex->coba));

	return rv;

}


static float mg_HTerrainTex(Tex *tex, float *texvec)
{
	int rv=0;	/* return value, int:0, col:1, nor:2, everything:3 */

	Tin = mg_HeteroTerrain(texvec[0], texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->noisebasis);

	if (tex->nor!=NULL) {
		/* calculate bumpnormal */
		tex->nor[0] = Tin - mg_HeteroTerrain(texvec[0] + B_OFFS, texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->noisebasis);
		tex->nor[1] = Tin - mg_HeteroTerrain(texvec[0], texvec[1] + B_OFFS, texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->noisebasis);
		tex->nor[2] = Tin - mg_HeteroTerrain(texvec[0], texvec[1], texvec[2] + B_OFFS, tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->noisebasis);
		rv += 2;
	}

	Tin *= tex->ns_outscale;

	BRICON;

	if (tex->flag & TEX_COLORBAND)  return (rv + do_colorband(tex->coba));

	return rv;

}


static float mg_distNoiseTex(Tex *tex, float *texvec)
{
	int rv=0;	/* return value, int:0, col:1, nor:2, everything:3 */

	Tin = mg_VLNoise(texvec[0], texvec[1], texvec[2], tex->dist_amount, tex->noisebasis, tex->noisebasis2);

	if (tex->nor!=NULL) {
		/* calculate bumpnormal */
		tex->nor[0] = Tin - mg_VLNoise(texvec[0] + B_OFFS, texvec[1], texvec[2], tex->dist_amount, tex->noisebasis, tex->noisebasis2);
		tex->nor[1] = Tin - mg_VLNoise(texvec[0], texvec[1] + B_OFFS, texvec[2], tex->dist_amount, tex->noisebasis, tex->noisebasis2);
		tex->nor[2] = Tin - mg_VLNoise(texvec[0], texvec[1], texvec[2] + B_OFFS, tex->dist_amount, tex->noisebasis, tex->noisebasis2);
		rv += 2;
	}

	BRICON;

	if (tex->flag & TEX_COLORBAND)  return (rv + do_colorband(tex->coba));

	return rv;

}


/* ------------------------------------------------------------------------- */
/* newnoise: Voronoi texture type, probably the slowest, especially with minkovsky, bumpmapping, could be done another way */

static float voronoiTex(Tex *tex, float *texvec)
{
	int rv=0;	/* return value, int:0, col:1, nor:2, everything:3 */
	float da[4], pa[12];	/* distance and point coordinate arrays of 4 nearest neighbours */
	float aw1 = fabs(tex->vn_w1);
	float aw2 = fabs(tex->vn_w2);
	float aw3 = fabs(tex->vn_w3);
	float aw4 = fabs(tex->vn_w4);
	float sc = (aw1 + aw2 + aw3 + aw4);
	if (sc!=0.f) sc =  tex->ns_outscale/sc;

	voronoi(texvec[0], texvec[1], texvec[2], da, pa, tex->vn_mexp, tex->vn_distm);
	Tin = sc * fabs(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);

	if (tex->vn_coltype) {
		float ca[3];	/* cell color */
		cellNoiseV(pa[0], pa[1], pa[2], ca);
		Tr = aw1*ca[0];
		Tg = aw1*ca[1];
		Tb = aw1*ca[2];
		cellNoiseV(pa[3], pa[4], pa[5], ca);
		Tr += aw2*ca[0];
		Tg += aw2*ca[1];
		Tb += aw2*ca[2];
		cellNoiseV(pa[6], pa[7], pa[8], ca);
		Tr += aw3*ca[0];
		Tg += aw3*ca[1];
		Tb += aw3*ca[2];
		cellNoiseV(pa[9], pa[10], pa[11], ca);
		Tr += aw4*ca[0];
		Tg += aw4*ca[1];
		Tb += aw4*ca[2];
		if (tex->vn_coltype>=2) {
			float t1 = (da[1]-da[0])*10;
			if (t1>1) t1=1;
			if (tex->vn_coltype==3) t1*=Tin; else t1*=sc;
			Tr *= t1;
			Tg *= t1;
			Tb *= t1;
		}
		else {
			Tr *= sc;
			Tg *= sc;
			Tb *= sc;
		}
	}

	if (tex->nor!=NULL) {
		/* calculate bumpnormal */
		voronoi(texvec[0] + B_OFFS, texvec[1], texvec[2], da, pa, tex->vn_mexp,  tex->vn_distm);
		tex->nor[0] = Tin - sc * fabs(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);
		voronoi(texvec[0], texvec[1] + B_OFFS, texvec[2], da, pa, tex->vn_mexp,  tex->vn_distm);
		tex->nor[1] = Tin - sc * fabs(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);
		voronoi(texvec[0], texvec[1], texvec[2] + B_OFFS, da, pa, tex->vn_mexp,  tex->vn_distm);
		tex->nor[2] = Tin - sc * fabs(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);
		rv += 2;
	}

	if (tex->vn_coltype) {
		BRICONRGB;
		Ta = 1.0;
		return (rv+1);
	}
	
	BRICON;

	if (tex->flag & TEX_COLORBAND)  return (rv + do_colorband(tex->coba));

	return rv;

}


/* ------------------------------------------------------------------------- */

static int texnoise(Tex *tex)
{
	float div=3.0;
	int val, ran, loop;
	
	ran= BLI_rand();
	val= (ran & 3);
	
	loop= tex->noisedepth;
	while(loop--) {
		ran= (ran>>2);
		val*= (ran & 3);
		div*= 3.0;
	}
	
	Tin= ((float)val)/div;;

	BRICON;
	if(tex->flag & TEX_COLORBAND)  return do_colorband(tex->coba);
	
	return 0;
}

/* ------------------------------------------------------------------------- */

static int plugintex(Tex *tex, float *texvec, float *dxt, float *dyt, int osatex)
{
	PluginTex *pit;
	int rgbnor=0;

	Tin= 0.0;

	pit= tex->plugin;
	if(pit && pit->doit) {
		if(tex->nor) {
			VECCOPY(pit->result+5, tex->nor);
		}
		if(osatex) rgbnor= ((TexDoit)pit->doit)(tex->stype, pit->data, texvec, dxt, dyt);
		else rgbnor= ((TexDoit)pit->doit)(tex->stype, pit->data, texvec, 0, 0);

		Tin= pit->result[0];

		if(rgbnor & TEX_NOR) {
			if(tex->nor) {
				VECCOPY(tex->nor, pit->result+5);
			}
		}
		
		if(rgbnor & TEX_RGB) {
			Tr= pit->result[1];
			Tg= pit->result[2];
			Tb= pit->result[3];
			Ta= pit->result[4];

			BRICONRGB;
		}
		
		BRICON;
		if(tex->flag & TEX_COLORBAND)  rgbnor |= do_colorband(tex->coba);
	}

	return rgbnor;
}

/* *************** PROJECTIONS ******************* */

void tubemap(float x, float y, float z, float *adr1, float *adr2)
{
	float len;

	*adr2 = (z + 1.0) / 2.0;

	len= sqrt(x*x+y*y);
	if(len>0) {
		*adr1 = (1.0 - (atan2(x/len,y/len) / M_PI)) / 2.0;
	}
}

/* ------------------------------------------------------------------------- */

void spheremap(float x, float y, float z, float *adr1, float *adr2)
{
	float len;

	len= sqrt(x*x+y*y+z*z);
	if(len>0.0) {
		
		if(x==0.0 && y==0.0) *adr1= 0.0;	/* othwise domain error */
		else *adr1 = (1.0 - atan2(x,y)/M_PI )/2.0;

		z/=len;
		*adr2 = 1.0- saacos(z)/M_PI;
	}
}

/* ------------------------------------------------------------------------- */

static int cubemap_glob(MTex *mtex, VlakRen *vlr, float x, float y, float z, float *adr1, float *adr2)
{
	float x1, y1, z1, nor[3];
	int ret;
	
	if(vlr==NULL) {
		nor[0]= x; nor[1]= y; nor[2]= z;	// use local render coord
	}
	else {
		VECCOPY(nor, vlr->n);
	}
	MTC_Mat4Mul3Vecfl(R.viewinv, nor);

	x1= fabs(nor[0]);
	y1= fabs(nor[1]);
	z1= fabs(nor[2]);
	
	if(z1>=x1 && z1>=y1) {
		*adr1 = (x + 1.0) / 2.0;
		*adr2 = (y + 1.0) / 2.0;
		ret= 0;
	}
	else if(y1>=x1 && y1>=z1) {
		*adr1 = (x + 1.0) / 2.0;
		*adr2 = (z + 1.0) / 2.0;
		ret= 1;
	}
	else {
		*adr1 = (y + 1.0) / 2.0;
		*adr2 = (z + 1.0) / 2.0;
		ret= 2;		
	}
	return ret;
}

/* ------------------------------------------------------------------------- */

static int cubemap(MTex *mtex, VlakRen *vlr, float x, float y, float z, float *adr1, float *adr2)
{
	int proj[4], ret= 0;
	
	if(vlr) {
		int index;
		
		/* Mesh vertices have such flags, for others we calculate it once based on orco */
		if((vlr->puno & (ME_PROJXY|ME_PROJXZ|ME_PROJYZ))==0) {
			if(vlr->v1->orco) {
				float nor[3];
				CalcNormFloat(vlr->v1->orco, vlr->v2->orco, vlr->v3->orco, nor);
				
				if( fabs(nor[0])<fabs(nor[2]) && fabs(nor[1])<fabs(nor[2]) ) vlr->puno |= ME_PROJXY;
				else if( fabs(nor[0])<fabs(nor[1]) && fabs(nor[2])<fabs(nor[1]) ) vlr->puno |= ME_PROJXZ;
				else vlr->puno |= ME_PROJYZ;
			}
			else return cubemap_glob(mtex, vlr, x, y, z, adr1, adr2);
		}
		
		/* the mtex->proj{xyz} have type char. maybe this should be wider? */
		/* casting to int ensures that the index type is right.            */
		index = (int) mtex->projx;
		proj[index]= ME_PROJXY;

		index = (int) mtex->projy;
		proj[index]= ME_PROJXZ;

		index = (int) mtex->projz;
		proj[index]= ME_PROJYZ;
		
		if(vlr->puno & proj[1]) {
			*adr1 = (x + 1.0) / 2.0;
			*adr2 = (y + 1.0) / 2.0;	
		}
		else if(vlr->puno & proj[2]) {
			*adr1 = (x + 1.0) / 2.0;
			*adr2 = (z + 1.0) / 2.0;
			ret= 1;
		}
		else {
			*adr1 = (y + 1.0) / 2.0;
			*adr2 = (z + 1.0) / 2.0;
			ret= 2;
		}		
	} 
	else {
		return cubemap_glob(mtex, vlr, x, y, z, adr1, adr2);
	}
	
	return ret;
}

/* ------------------------------------------------------------------------- */

static int cubemap_ob(MTex *mtex, VlakRen *vlr, float x, float y, float z, float *adr1, float *adr2)
{
	float x1, y1, z1, nor[3];
	int ret;
	
	if(vlr==NULL) return 0;
	
	VECCOPY(nor, vlr->n);
	if(mtex->object) MTC_Mat4Mul3Vecfl(mtex->object->imat, nor);
	
	x1= fabs(nor[0]);
	y1= fabs(nor[1]);
	z1= fabs(nor[2]);
	
	if(z1>=x1 && z1>=y1) {
		*adr1 = (x + 1.0) / 2.0;
		*adr2 = (y + 1.0) / 2.0;
		ret= 0;
	}
	else if(y1>=x1 && y1>=z1) {
		*adr1 = (x + 1.0) / 2.0;
		*adr2 = (z + 1.0) / 2.0;
		ret= 1;
	}
	else {
		*adr1 = (y + 1.0) / 2.0;
		*adr2 = (z + 1.0) / 2.0;
		ret= 2;		
	}
	return ret;
}

/* ------------------------------------------------------------------------- */

static void do_2d_mapping(MTex *mtex, float *t, VlakRen *vlr, float *dxt, float *dyt)
{
	Tex *tex;
	float fx, fy, fac1, area[8];
	int ok, proj, areaflag= 0, wrap;
	
	wrap= mtex->mapping;
	tex= mtex->tex;

	if(R.osa==0) {
		
		if(wrap==MTEX_FLAT) {
			fx = (t[0] + 1.0) / 2.0;
			fy = (t[1] + 1.0) / 2.0;
		}
		else if(wrap==MTEX_TUBE) tubemap(t[0], t[1], t[2], &fx, &fy);
		else if(wrap==MTEX_SPHERE) spheremap(t[0], t[1], t[2], &fx, &fy);
		else {
			if(mtex->texco==TEXCO_OBJECT) cubemap_ob(mtex, vlr, t[0], t[1], t[2], &fx, &fy);
			else if(mtex->texco==TEXCO_GLOB) cubemap_glob(mtex, vlr, t[0], t[1], t[2], &fx, &fy);
			else cubemap(mtex, vlr, t[0], t[1], t[2], &fx, &fy);
		}
		
		/* repeat */
		if(tex->xrepeat>1) {
			fx *= tex->xrepeat;
			if(fx>1.0) fx -= (int)(fx);
			else if(fx<0.0) fx+= 1-(int)(fx);
		}
		if(tex->yrepeat>1) {
			fy *= tex->yrepeat;
			if(fy>1.0) fy -= (int)(fy);
			else if(fy<0.0) fy+= 1-(int)(fy);
		}
		
		/* crop */
		if(tex->cropxmin!=0.0 || tex->cropxmax!=1.0) {
			fac1= tex->cropxmax - tex->cropxmin;
			fx= tex->cropxmin+ fx*fac1;
		}
		if(tex->cropymin!=0.0 || tex->cropymax!=1.0) {
			fac1= tex->cropymax - tex->cropymin;
			fy= tex->cropymin+ fy*fac1;
		}

		t[0]= fx;
		t[1]= fy;
	}
	else {
		
		if(wrap==MTEX_FLAT) {
			fx= (t[0] + 1.0) / 2.0;
			fy= (t[1] + 1.0) / 2.0;
			dxt[0]/= 2.0; 
			dxt[1]/= 2.0;
			dyt[0]/= 2.0; 
			dyt[1]/= 2.0;
		}
		else if ELEM(wrap, MTEX_TUBE, MTEX_SPHERE) {
			/* exception: the seam behind (y<0.0) */
			ok= 1;
			if(t[1]<=0.0) {
				fx= t[0]+dxt[0];
				fy= t[0]+dyt[0];
				if(fx>=0.0 && fy>=0.0 && t[0]>=0.0);
				else if(fx<=0.0 && fy<=0.0 && t[0]<=0.0);
				else ok= 0;
			}
			if(ok) {
				if(wrap==MTEX_TUBE) {
					tubemap(t[0], t[1], t[2], area, area+1);
					tubemap(t[0]+dxt[0], t[1]+dxt[1], t[2]+dxt[2], area+2, area+3);
					tubemap(t[0]+dyt[0], t[1]+dyt[1], t[2]+dyt[2], area+4, area+5);
				}
				else { 
					spheremap(t[0], t[1], t[2],area,area+1);
					spheremap(t[0]+dxt[0], t[1]+dxt[1], t[2]+dxt[2], area+2, area+3);
					spheremap(t[0]+dyt[0], t[1]+dyt[1], t[2]+dyt[2], area+4, area+5);
				}
				areaflag= 1;
			}
			else {
				if(wrap==MTEX_TUBE) tubemap(t[0], t[1], t[2], &fx, &fy);
				else spheremap(t[0], t[1], t[2], &fx, &fy);
				dxt[0]/= 2.0; 
				dxt[1]/= 2.0;
				dyt[0]/= 2.0; 
				dyt[1]/= 2.0;
			}
		}
		else {

			if(mtex->texco==TEXCO_OBJECT) proj = cubemap_ob(mtex, vlr, t[0], t[1], t[2], &fx, &fy);
			else if (mtex->texco==TEXCO_GLOB) proj = cubemap_glob(mtex, vlr, t[0], t[1], t[2], &fx, &fy);
			else proj = cubemap(mtex, vlr, t[0], t[1], t[2], &fx, &fy);

			if(proj==1) {
				dxt[1]= dxt[2];
				dyt[1]= dyt[2];
			}
			else if(proj==2) {
				dxt[0]= dxt[1];
				dyt[0]= dyt[1];
				dxt[1]= dxt[2];
				dyt[1]= dyt[2];
			}
			dxt[0]/= 2.0; 
			dxt[1]/= 2.0;
			dyt[0]/= 2.0; 
			dyt[1]/= 2.0;
		}
		
		/* if area, then reacalculate dxt[] and dyt[] */
		if(areaflag) {
			fx= area[0]; 
			fy= area[1];
			dxt[0]= area[2]-fx;
			dxt[1]= area[3]-fy;
			dyt[0]= area[4]-fx;
			dyt[1]= area[5]-fy;
		}
		
		/* repeat */
		if(tex->xrepeat>1) {
			fx *= tex->xrepeat;
			dxt[0]*= tex->xrepeat;
			dyt[0]*= tex->xrepeat;
			if(fx>1.0) fx -= (int)(fx);
			else if(fx<0.0) fx+= 1-(int)(fx);
		}
		if(tex->yrepeat>1) {
			fy *= tex->yrepeat;
			dxt[1]*= tex->yrepeat;
			dyt[1]*= tex->yrepeat;
			if(fy>1.0) fy -= (int)(fy);
			else if(fy<0.0) fy+= 1-(int)(fy);
		}
		
		/* crop */
		if(tex->cropxmin!=0.0 || tex->cropxmax!=1.0) {
			fac1= tex->cropxmax - tex->cropxmin;
			fx= tex->cropxmin+ fx*fac1;
			dxt[0]*= fac1;
			dyt[0]*= fac1;
		}
		if(tex->cropymin!=0.0 || tex->cropymax!=1.0) {
			fac1= tex->cropymax - tex->cropymin;
			fy= tex->cropymin+ fy*fac1;
			dxt[1]*= fac1;
			dyt[1]*= fac1;
		}
		
		t[0]= fx;
		t[1]= fy;

	}
}


/* ************************************** */

int multitex(Tex *tex, float *texvec, float *dxt, float *dyt, int osatex)
{

	switch(tex->type) {
	
	case 0:
		Tin= 0.0;
		return 0;
	case TEX_CLOUDS:
		return clouds(tex, texvec);
	case TEX_WOOD:
		return wood(tex, texvec); 
	case TEX_MARBLE:
		return marble(tex, texvec); 
	case TEX_MAGIC:
		return magic(tex, texvec); 
	case TEX_BLEND:
		return blend(tex, texvec);
	case TEX_STUCCI:
		Tin= 0.0;
		return stucci(tex, texvec); 
	case TEX_NOISE:
		return texnoise(tex); 
	case TEX_IMAGE:
		if(osatex) return imagewraposa(tex, texvec, dxt, dyt); 
		else return imagewrap(tex, texvec); 
	case TEX_PLUGIN:
		return plugintex(tex, texvec, dxt, dyt, osatex);
	case TEX_ENVMAP:
		return envmaptex(tex, texvec, dxt, dyt, osatex);
	case TEX_MUSGRAVE:
		/* newnoise: musgrave types */
		
		/* ton: added this, for Blender convention reason. scaling texvec here is so-so... */
		VecMulf(texvec, 1.0/tex->noisesize);
		
		switch(tex->stype) {
		case TEX_MFRACTAL:
		case TEX_FBM:
			return mg_mFractalOrfBmTex(tex, texvec);
		case TEX_RIDGEDMF:
		case TEX_HYBRIDMF:
			return mg_ridgedOrHybridMFTex(tex, texvec);
		case TEX_HTERRAIN:
			return mg_HTerrainTex(tex, texvec);
		}
		break;
	/* newnoise: voronoi type */
	case TEX_VORONOI:
		/* ton: added this, for Blender convention reason. scaling texvec here is so-so... */
		VecMulf(texvec, 1.0/tex->noisesize);
		
		return voronoiTex(tex, texvec);
	case TEX_DISTNOISE:
		/* ton: added this, for Blender convention reason. scaling texvec here is so-so... */
		VecMulf(texvec, 1.0/tex->noisesize);
		
		return mg_distNoiseTex(tex, texvec);
	}
	return 0;
}

/* ------------------------------------------------------------------------- */

void do_material_tex(ShadeInput *shi)
{
	Object *ob;
	Material *mat_col, *mat_colspec, *mat_colmir, *mat_ref, *mat_amb;
	Material *mat_spec, *mat_har, *mat_emit, *mat_alpha, *mat_ray_mirr, *mat_translu;
	MTex *mtex;
	Tex *tex;
	float *co = NULL, *dx = NULL, *dy = NULL, fact, 
		facm, factt, facmm, facmul = 0.0, stencilTin=1.0;
	float texvec[3], dxt[3], dyt[3], tempvec[3], norvec[3], Tnor=1.0;
	int tex_nr, rgbnor= 0;

	/* here: test flag if there's a tex (todo) */
	
	mat_col=mat_colspec=mat_colmir=mat_ref=mat_spec=mat_har=mat_emit=mat_alpha=mat_ray_mirr=mat_translu=mat_amb= shi->mat;
	
	for(tex_nr=0; tex_nr<8; tex_nr++) {
		
		/* separate tex switching */
		if(shi->mat->septex & (1<<tex_nr)) continue;
		
		if(shi->mat->mtex[tex_nr]) {
			mtex= shi->mat->mtex[tex_nr];
			
			tex= mtex->tex;
			if(tex==0) continue;
			
			/* which coords */
			if(mtex->texco==TEXCO_ORCO) {
				co= shi->lo; dx= O.dxlo; dy= O.dylo;
			}
			else if(mtex->texco==TEXCO_STICKY) {
				co= shi->sticky; dx= O.dxsticky; dy= O.dysticky;
			}
			else if(mtex->texco==TEXCO_OBJECT) {
				ob= mtex->object;
				if(ob) {
					co= tempvec;
					dx= dxt;
					dy= dyt;
					VECCOPY(tempvec, shi->co);
					MTC_Mat4MulVecfl(ob->imat, tempvec);
					if(shi->osatex) {
						VECCOPY(dxt, O.dxco);
						VECCOPY(dyt, O.dyco);
						MTC_Mat4Mul3Vecfl(ob->imat, dxt);
						MTC_Mat4Mul3Vecfl(ob->imat, dyt);
					}
				}
				else {
					/* if object doesn't exist, do not use orcos (not initialized) */
					co= shi->co;
					dx= O.dxco; dy= O.dyco;
				}
			}
			else if(mtex->texco==TEXCO_REFL) {
				co= shi->ref; dx= O.dxref; dy= O.dyref;
			}
			else if(mtex->texco==TEXCO_NORM) {
				co= shi->orn; dx= O.dxno; dy= O.dyno;
			}
			else if(mtex->texco==TEXCO_GLOB) {
				co= shi->gl; dx= O.dxco; dy= O.dyco;
			}
			else if(mtex->texco==TEXCO_UV) {
				co= shi->uv; dx= O.dxuv; dy= O.dyuv; 
			}
			else if(mtex->texco==TEXCO_WINDOW) {
				co= shi->winco; dx= O.dxwin; dy= O.dywin;
			}
			
			/* de pointer defines if bumping happens */
			if(mtex->mapto & (MAP_NORM|MAP_DISPLACE)) {
				tex->nor= norvec;
				norvec[0]= norvec[1]= norvec[2]= 0.0;
			}
			else tex->nor= NULL;

			if(tex->type==TEX_IMAGE) {

				/* new: first swap coords, then map, then trans/scale */

				/* placement */
				if(mtex->projx) texvec[0]= co[mtex->projx-1];
				else texvec[0]= 0.0;
				if(mtex->projy) texvec[1]= co[mtex->projy-1];
				else texvec[1]= 0.0;
				if(mtex->projz) texvec[2]= co[mtex->projz-1];
				else texvec[2]= 0.0;

				if(shi->osatex) {

					if(mtex->projx) {
						dxt[0]= dx[mtex->projx-1];
						dyt[0]= dy[mtex->projx-1];
					}
					else dxt[0]= 0.0;
					if(mtex->projy) {
						dxt[1]= dx[mtex->projy-1];
						dyt[1]= dy[mtex->projy-1];
					}
					else dxt[1]= 0.0;
					if(mtex->projx) {
						dxt[2]= dx[mtex->projz-1];
						dyt[2]= dy[mtex->projz-1];
					}
					else dxt[2]= 0.0;
				}

				do_2d_mapping(mtex, texvec, shi->vlr, dxt, dyt);

				/* translate and scale */
				texvec[0]= mtex->size[0]*(texvec[0]-0.5) +mtex->ofs[0]+0.5;
				texvec[1]= mtex->size[1]*(texvec[1]-0.5) +mtex->ofs[1]+0.5;
				if(shi->osatex) {
					dxt[0]= mtex->size[0]*dxt[0];
					dxt[1]= mtex->size[1]*dxt[1];
					dyt[0]= mtex->size[0]*dyt[0];
					dyt[1]= mtex->size[1]*dyt[1];
				}
			}
			else {

				/* placement */
				if(mtex->projx) texvec[0]= mtex->size[0]*(co[mtex->projx-1]+mtex->ofs[0]);
				else texvec[0]= mtex->size[0]*(mtex->ofs[0]);

				if(mtex->projy) texvec[1]= mtex->size[1]*(co[mtex->projy-1]+mtex->ofs[1]);
				else texvec[1]= mtex->size[1]*(mtex->ofs[1]);

				if(mtex->projz) texvec[2]= mtex->size[2]*(co[mtex->projz-1]+mtex->ofs[2]);
				else texvec[2]= mtex->size[2]*(mtex->ofs[2]);

				if(shi->osatex) {
					if(mtex->projx) {
						dxt[0]= mtex->size[0]*dx[mtex->projx-1];
						dyt[0]= mtex->size[0]*dy[mtex->projx-1];
					}
					else dxt[0]= 0.0;
					if(mtex->projy) {
						dxt[1]= mtex->size[1]*dx[mtex->projy-1];
						dyt[1]= mtex->size[1]*dy[mtex->projy-1];
					}
					else dxt[1]= 0.0;
					if(mtex->projx) {
						dxt[2]= mtex->size[2]*dx[mtex->projz-1];
						dyt[2]= mtex->size[2]*dy[mtex->projz-1];
					}
					else dxt[2]= 0.0;
				}
			}

			rgbnor= multitex(tex, texvec, dxt, dyt, shi->osatex);

			/* texture output */

			if( (rgbnor & TEX_RGB) && (mtex->texflag & MTEX_RGBTOINT)) {
				Tin= (0.35*Tr+0.45*Tg+0.2*Tb);
				rgbnor-= 1;
			}
			if(mtex->texflag & MTEX_NEGATIVE) {
				if(rgbnor & TEX_RGB) {
					Tr= 1.0-Tr;
					Tg= 1.0-Tg;
					Tb= 1.0-Tb;
				}
				Tin= 1.0-Tin;
			}
			if(mtex->texflag & MTEX_STENCIL) {
				if(rgbnor & TEX_RGB) {
					fact= Ta;
					Ta*= stencilTin;
					stencilTin*= fact;
				}
				else {
					fact= Tin;
					Tin*= stencilTin;
					stencilTin*= fact;
				}
			}
			else {
				Ta*= stencilTin;
				Tnor*= stencilTin;
				Tin*= stencilTin;
			}

			if(tex->nor && (rgbnor & TEX_NOR)==0) {
				/* make our own normal */
				if(rgbnor & TEX_RGB) {
					tex->nor[0]= Tr;
					tex->nor[1]= Tg;
					tex->nor[2]= Tb;
				}
				else {
					float co= 0.5*cos(Tin-0.5);
					float si= 0.5*sin(Tin-0.5);
					float f1, f2;

					f1= shi->vn[0];
					f2= shi->vn[1];
					tex->nor[0]= f1*co+f2*si;
					tex->nor[1]= f2*co-f1*si;
					f1= shi->vn[1];
					f2= shi->vn[2];
					tex->nor[1]= f1*co+f2*si;
					tex->nor[2]= f2*co-f1*si;
				}
			}


			/* mapping */
			if(mtex->mapto & (MAP_COL+MAP_COLSPEC+MAP_COLMIR)) {

				if((rgbnor & TEX_RGB)==0) {
					Tr= mtex->r;
					Tg= mtex->g;
					Tb= mtex->b;
				}
				else if(mtex->mapto & MAP_ALPHA) {
					if(mtex->texflag & MTEX_ALPHAMIX) Tin= Ta;
					else Tin= stencilTin;
				}
				else Tin= Ta;

				fact= Tin*mtex->colfac;
				facm= 1.0-fact;
				if(mtex->blendtype==MTEX_MUL) facm= 1.0-mtex->colfac;
				if(mtex->blendtype==MTEX_SUB) fact= -fact;

				if(mtex->mapto & MAP_COL) {
					if(mtex->blendtype==MTEX_BLEND) {
						shi->matren->r= (fact*Tr + facm*mat_col->r);
						shi->matren->g= (fact*Tg + facm*mat_col->g);
						shi->matren->b= (fact*Tb + facm*mat_col->b);
					}
					else if(mtex->blendtype==MTEX_MUL) {
						shi->matren->r= (facm+fact*Tr)*mat_col->r;
						shi->matren->g= (facm+fact*Tg)*mat_col->g;
						shi->matren->b= (facm+fact*Tb)*mat_col->b;
					}
					else {
						shi->matren->r= (fact*Tr + mat_col->r);
						shi->matren->g= (fact*Tg + mat_col->g);
						shi->matren->b= (fact*Tb + mat_col->b);
					}
					mat_col= shi->matren;
				}
				if(mtex->mapto & MAP_COLSPEC) {
					if(mtex->blendtype==MTEX_BLEND) {
						shi->matren->specr= (fact*Tr + facm*mat_colspec->specr);
						shi->matren->specg= (fact*Tg + facm*mat_colspec->specg);
						shi->matren->specb= (fact*Tb + facm*mat_colspec->specb);
					}
					else if(mtex->blendtype==MTEX_MUL) {
						shi->matren->specr= (facm+fact*Tr)*mat_colspec->specr;
						shi->matren->specg= (facm+fact*Tg)*mat_colspec->specg;
						shi->matren->specb= (facm+fact*Tb)*mat_colspec->specb;
					}
					else {
						shi->matren->specr= (fact*Tr + mat_colspec->specr);
						shi->matren->specg= (fact*Tg + mat_colspec->specg);
						shi->matren->specb= (fact*Tb + mat_colspec->specb);
					}
					mat_colspec= shi->matren;
				}
				if(mtex->mapto & MAP_COLMIR) {
					if(mtex->blendtype==MTEX_BLEND) {
						// exception for envmap only
						if(tex->type==TEX_ENVMAP) {
							shi->refcol[0]= fact + facm*shi->refcol[0];
							shi->refcol[1]= fact*Tr + facm*shi->refcol[1];
							shi->refcol[2]= fact*Tg + facm*shi->refcol[2];
							shi->refcol[3]= fact*Tb + facm*shi->refcol[3];
						} else {
							shi->matren->mirr= fact*Tr + facm*mat_colmir->mirr;
							shi->matren->mirg= fact*Tg + facm*mat_colmir->mirg;
							shi->matren->mirb= fact*Tb + facm*mat_colmir->mirb;
						}
					}
					else if(mtex->blendtype==MTEX_MUL) {
						shi->matren->mirr= (facm+fact*Tr)*mat_colmir->mirr;
						shi->matren->mirg= (facm+fact*Tg)*mat_colmir->mirg;
						shi->matren->mirb= (facm+fact*Tb)*mat_colmir->mirb;
					}
					else {
						shi->matren->mirr= (fact*Tr + mat_colmir->mirr);
						shi->matren->mirg= (fact*Tg + mat_colmir->mirg);
						shi->matren->mirb= (fact*Tb + mat_colmir->mirb);
					}
					mat_colmir= shi->matren;
				}
			}
			if( (mtex->mapto & MAP_NORM) ) {
				if(tex->nor) {
					
					if(mtex->maptoneg & MAP_NORM) tex->norfac= -mtex->norfac;
					else tex->norfac= mtex->norfac;

					shi->vn[0]+= Tnor*tex->norfac*tex->nor[0];
					shi->vn[1]+= Tnor*tex->norfac*tex->nor[1];
					shi->vn[2]+= Tnor*tex->norfac*tex->nor[2];
					
					Normalise(shi->vn);
					
					/* this makes sure the bump is passed on to the next texture */
					shi->orn[0]= shi->vn[0];
					shi->orn[1]= shi->vn[1];
					shi->orn[2]= shi->vn[2];
					
					/* reflection vector */
					calc_R_ref(shi);
				}
			}

			if( mtex->mapto & MAP_DISPLACE ) {
				/* Now that most textures offer both Nor and Intensity, allow  */
				/* both to work, and let user select with slider.   */
				if(tex->nor) {
					if(mtex->maptoneg & MAP_DISPLACE) tex->norfac= -mtex->norfac;
					else tex->norfac= mtex->norfac;

					shi->displace[0]+= 0.2f*Tnor*tex->norfac*tex->nor[0];
					shi->displace[1]+= 0.2f*Tnor*tex->norfac*tex->nor[1];
					shi->displace[2]+= 0.2f*Tnor*tex->norfac*tex->nor[2];
				}
				
				if(rgbnor & TEX_RGB) {
					if(Talpha) Tin= Ta;
					else Tin= (0.35*Tr+0.45*Tg+0.2*Tb);
				}

				if(mtex->maptoneg & MAP_DISPLACE) {
					factt= (0.5-Tin)*mtex->dispfac; facmm= 1.0-factt;
				}
				else {
					factt= (Tin-0.5)*mtex->dispfac; facmm= 1.0-factt;
				}

				if(mtex->blendtype==MTEX_BLEND) {
					shi->displace[0]= factt*shi->vn[0] + facmm*shi->displace[0];
					shi->displace[1]= factt*shi->vn[1] + facmm*shi->displace[1];
					shi->displace[2]= factt*shi->vn[2] + facmm*shi->displace[2];
				}
				else if(mtex->blendtype==MTEX_MUL) {
					shi->displace[0]*= factt*shi->vn[0];
					shi->displace[1]*= factt*shi->vn[1];
					shi->displace[2]*= factt*shi->vn[2];
				}
				else { /* add or sub */
					if(mtex->blendtype==MTEX_SUB) factt= -factt;
					else factt= factt;
					shi->displace[0]+= factt*shi->vn[0];
					shi->displace[1]+= factt*shi->vn[1];
					shi->displace[2]+= factt*shi->vn[2];
				}
			}

			if(mtex->mapto & MAP_VARS) {
				if(rgbnor & TEX_RGB) {
					if(Talpha) Tin= Ta;
					else Tin= (0.35*Tr+0.45*Tg+0.2*Tb);
				}

				fact= Tin*mtex->varfac;
				facm= 1.0-fact;
				if(mtex->blendtype==MTEX_MUL) facmul= 1.0-mtex->varfac;
				if(mtex->blendtype==MTEX_SUB) fact= -fact;

				if(mtex->mapto & MAP_REF) {
					if(mtex->maptoneg & MAP_REF) {factt= facm; facmm= fact;}
					else {factt= fact; facmm= facm;}

					if(mtex->blendtype==MTEX_BLEND)
						shi->matren->ref= factt*mtex->def_var+ facmm*mat_ref->ref;
					else if(mtex->blendtype==MTEX_MUL)
						shi->matren->ref= (facmul+factt)*mat_ref->ref;
					else {
						shi->matren->ref= factt+mat_ref->ref;
						if(shi->matren->ref<0.0) shi->matren->ref= 0.0;
					}
					mat_ref= shi->matren;
				}
				if(mtex->mapto & MAP_SPEC) {
					if(mtex->maptoneg & MAP_SPEC) {factt= facm; facmm= fact;}
					else {factt= fact; facmm= facm;}

					if(mtex->blendtype==MTEX_BLEND)
						shi->matren->spec= factt*mtex->def_var+ facmm*mat_spec->spec;
					else if(mtex->blendtype==MTEX_MUL)
						shi->matren->spec= (facmul+factt)*mat_spec->spec;
					else {
						shi->matren->spec= factt+mat_spec->spec;
						if(shi->matren->spec<0.0) shi->matren->spec= 0.0;
					}
					mat_spec= shi->matren;
				}
				if(mtex->mapto & MAP_EMIT) {
					if(mtex->maptoneg & MAP_EMIT) {factt= facm; facmm= fact;}
					else {factt= fact; facmm= facm;}

					if(mtex->blendtype==MTEX_BLEND)
						shi->matren->emit= factt*mtex->def_var+ facmm*mat_emit->emit;
					else if(mtex->blendtype==MTEX_MUL)
						shi->matren->emit= (facmul+factt)*mat_emit->emit;
					else {
						shi->matren->emit= factt+mat_emit->emit;
						if(shi->matren->emit<0.0) shi->matren->emit= 0.0;
					}
					mat_emit= shi->matren;
				}
				if(mtex->mapto & MAP_ALPHA) {
					if(mtex->maptoneg & MAP_ALPHA) {factt= facm; facmm= fact;}
					else {factt= fact; facmm= facm;}

					if(mtex->blendtype==MTEX_BLEND)
						shi->matren->alpha= factt*mtex->def_var+ facmm*mat_alpha->alpha;
					else if(mtex->blendtype==MTEX_MUL)
						shi->matren->alpha= (facmul+factt)*mat_alpha->alpha;
					else {
						shi->matren->alpha= factt+mat_alpha->alpha;
						if(shi->matren->alpha<0.0) shi->matren->alpha= 0.0;
						else if(shi->matren->alpha>1.0) shi->matren->alpha= 1.0;
					}
					mat_alpha= shi->matren;
				}
				if(mtex->mapto & MAP_HAR) {
					if(mtex->maptoneg & MAP_HAR) {factt= facm; facmm= fact;}
					else {factt= fact; facmm= facm;}

					if(mtex->blendtype==MTEX_BLEND) {
						shi->matren->har= 128.0*factt*mtex->def_var+ facmm*mat_har->har;
					} else if(mtex->blendtype==MTEX_MUL) {
						shi->matren->har= (facmul+factt)*mat_har->har;
					} else {
						shi->matren->har= 128.0*factt+mat_har->har;
						if(shi->matren->har<1) shi->matren->har= 1;
					}
					mat_har= shi->matren;
				}
				if(mtex->mapto & MAP_RAYMIRR) {
					if(mtex->maptoneg & MAP_RAYMIRR) {factt= facm; facmm= fact;}
					else {factt= fact; facmm= facm;}

					if(mtex->blendtype==MTEX_BLEND)
						shi->matren->ray_mirror= factt*mtex->def_var+ facmm*mat_ray_mirr->ray_mirror;
					else if(mtex->blendtype==MTEX_MUL)
						shi->matren->ray_mirror= (facmul+factt)*mat_ray_mirr->ray_mirror;
					else {
						shi->matren->ray_mirror= factt+mat_ray_mirr->ray_mirror;
						if(shi->matren->ray_mirror<0.0) shi->matren->ray_mirror= 0.0;
						else if(shi->matren->ray_mirror>1.0) shi->matren->ray_mirror= 1.0;
					}
					mat_ray_mirr= shi->matren;
				}
				if(mtex->mapto & MAP_TRANSLU) {
					if(mtex->maptoneg & MAP_TRANSLU) {factt= facm; facmm= fact;}
					else {factt= fact; facmm= facm;}

					if(mtex->blendtype==MTEX_BLEND)
						shi->matren->translucency= factt*mtex->def_var+ facmm*mat_translu->translucency;
					else if(mtex->blendtype==MTEX_MUL)
						shi->matren->translucency= (facmul+factt)*mat_translu->translucency;
					else {
						shi->matren->translucency= factt+mat_translu->translucency;
						if(shi->matren->translucency<0.0) shi->matren->translucency= 0.0;
						else if(shi->matren->translucency>1.0) shi->matren->translucency= 1.0;
					}
					mat_translu= shi->matren;
				}
				if(mtex->mapto & MAP_AMB) {
					if(mtex->maptoneg & MAP_AMB) {factt= facm; facmm= fact;}
					else {factt= fact; facmm= facm;}

					if(mtex->blendtype==MTEX_BLEND)
						shi->matren->amb= factt*mtex->def_var+ facmm*mat_translu->amb;
					else if(mtex->blendtype==MTEX_MUL)
						shi->matren->amb= (facmul+factt)*mat_translu->amb;
					else {
						shi->matren->amb= factt+mat_translu->amb;
						if(shi->matren->amb<0.0) shi->matren->amb= 0.0;
						else if(shi->matren->amb>1.0) shi->matren->amb= 1.0;
					}
					mat_amb= shi->matren;
				}
			}
		}
	}
}

/* ------------------------------------------------------------------------- */

void do_halo_tex(HaloRen *har, float xn, float yn, float *colf)
{
	MTex *mtex;
	float texvec[3], dxt[3], dyt[3], fact, facm, dx;
	int rgb, osatex;
	
	mtex= har->mat->mtex[0];
	if(mtex->tex==0) return;
	/* no normal mapping */
	mtex->tex->nor= NULL;
		
	texvec[0]= xn/har->rad;
	texvec[1]= yn/har->rad;
	texvec[2]= 0.0;
	
	osatex= (har->mat->texco & TEXCO_OSA);

	/* placement */
	if(mtex->projx) texvec[0]= mtex->size[0]*(texvec[mtex->projx-1]+mtex->ofs[0]);
	else texvec[0]= mtex->size[0]*(mtex->ofs[0]);
	
	if(mtex->projy) texvec[1]= mtex->size[1]*(texvec[mtex->projy-1]+mtex->ofs[1]);
	else texvec[1]= mtex->size[1]*(mtex->ofs[1]);
	
	if(mtex->projz) texvec[2]= mtex->size[2]*(texvec[mtex->projz-1]+mtex->ofs[2]);
	else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
	
	if(osatex) {
	
		dx= 1.0/har->rad;
	
		if(mtex->projx) {
			dxt[0]= mtex->size[0]*dx;
			dyt[0]= mtex->size[0]*dx;
		}
		else dxt[0]= 0.0;
		if(mtex->projy) {
			dxt[1]= mtex->size[1]*dx;
			dyt[1]= mtex->size[1]*dx;
		}
		else dxt[1]= 0.0;
		if(mtex->projz) {
			dxt[2]= 0.0;
			dyt[2]= 0.0;
		}
		else dxt[2]= 0.0;

	}

	if(mtex->tex->type==TEX_IMAGE) do_2d_mapping(mtex, texvec, NULL, dxt, dyt);
	
	rgb= multitex(mtex->tex, texvec, dxt, dyt, osatex);

	/* texture output */
	if(rgb && (mtex->texflag & MTEX_RGBTOINT)) {
		Tin= (0.35*Tr+0.45*Tg+0.2*Tb);
		rgb= 0;
	}
	if(mtex->texflag & MTEX_NEGATIVE) {
		if(rgb) {
			Tr= 1.0-Tr;
			Tg= 1.0-Tg;
			Tb= 1.0-Tb;
		}
		else Tin= 1.0-Tin;
	}

	/* mapping */
	if(mtex->mapto & MAP_COL) {
		
		if(rgb==0) {
			Tr= mtex->r;
			Tg= mtex->g;
			Tb= mtex->b;
		}
		else if(mtex->mapto & MAP_ALPHA) {
			if(mtex->texflag & MTEX_ALPHAMIX) Tin= Ta; 
			else Tin= 1.0;
		}
		else Tin= Ta;

		fact= Tin*mtex->colfac;
		facm= 1.0-fact;
		
		if(mtex->blendtype==MTEX_MUL) {
			facm= 1.0-mtex->colfac;
		}
		else fact*= 256;

		if(mtex->blendtype==MTEX_SUB) fact= -fact;

		if(mtex->blendtype==MTEX_BLEND) {
			colf[0]= (fact*Tr + facm*har->r);
			colf[1]= (fact*Tg + facm*har->g);
			colf[2]= (fact*Tb + facm*har->b);
		}
		else if(mtex->blendtype==MTEX_MUL) {
			colf[0]= (facm+fact*Tr)*har->r;
			colf[1]= (facm+fact*Tg)*har->g;
			colf[2]= (facm+fact*Tb)*har->b;
		}
		else {
			colf[0]= (fact*Tr + har->r);
			colf[1]= (fact*Tg + har->g);
			colf[2]= (fact*Tb + har->b);
			
			CLAMP(colf[0], 0.0, 1.0);
			CLAMP(colf[1], 0.0, 1.0);
			CLAMP(colf[2], 0.0, 1.0);
		}
	}
	if(mtex->mapto & MAP_ALPHA) {
		if(rgb) {
			if(Talpha) Tin= Ta;
			else Tin= (0.35*Tr+0.45*Tg+0.2*Tb);
		}
				
		colf[3]*= Tin;
	}
}

/* ------------------------------------------------------------------------- */

void do_sky_tex(float *lo)
{
	World *wrld_hor, *wrld_zen;
	MTex *mtex;
	float *co, fact, facm, factt, facmm, facmul = 0.0, stencilTin=1.0;
	float tempvec[3], texvec[3], dxt[3], dyt[3];
	int tex_nr, rgb= 0, ok;
	

	/* todo: add flag to test if there's a tex */
	
	wrld_hor= wrld_zen= G.scene->world;

	/* The 6 here is the max amount of channels for a world */
	for(tex_nr=0; tex_nr<6; tex_nr++) {
		if(R.wrld.mtex[tex_nr]) {
			mtex= R.wrld.mtex[tex_nr];
			
			if(mtex->tex==0) continue;
			/* if(mtex->mapto==0) continue; */
			
			/* which coords */
			co= lo;
			
			/* dxt dyt just from 1 value */
			dxt[0]= dxt[1]= dxt[2]= O.dxview;
			dyt[0]= dyt[1]= dyt[2]= O.dyview;
			
			/* Grab the mapping settings for this texture */
			if(mtex->texco==TEXCO_ANGMAP) {
				
				fact= (1.0/M_PI)*acos(lo[2])/(sqrt(lo[0]*lo[0] + lo[1]*lo[1])); 
				tempvec[0]= lo[0]*fact;
				tempvec[1]= lo[1]*fact;
				tempvec[2]= 0.0;
				co= tempvec;
				
			}
			else if(mtex->texco==TEXCO_OBJECT) {
				Object *ob= mtex->object;
				if(ob) {
					VECCOPY(tempvec, lo);
					MTC_Mat4MulVecfl(ob->imat, tempvec);
					co= tempvec;
				}
			}
			
			/* placement */			
			if(mtex->projx) texvec[0]= mtex->size[0]*(co[mtex->projx-1]+mtex->ofs[0]);
			else texvec[0]= mtex->size[0]*(mtex->ofs[0]);
			
			if(mtex->projy) texvec[1]= mtex->size[1]*(co[mtex->projy-1]+mtex->ofs[1]);
			else texvec[1]= mtex->size[1]*(mtex->ofs[1]);
			
			if(mtex->projz) texvec[2]= mtex->size[2]*(co[mtex->projz-1]+mtex->ofs[2]);
			else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
			
			/* texture */
			if(mtex->tex->type==TEX_IMAGE) do_2d_mapping(mtex, texvec, NULL, dxt, dyt);
			
			rgb= multitex(mtex->tex, texvec, dxt, dyt, R.osa);
			
			/* texture output */
			if(rgb && (mtex->texflag & MTEX_RGBTOINT)) {
				Tin= (0.35*Tr+0.45*Tg+0.2*Tb);
				rgb= 0;
			}
			if(mtex->texflag & MTEX_NEGATIVE) {
				if(rgb) {
					Tr= 1.0-Tr;
					Tg= 1.0-Tg;
					Tb= 1.0-Tb;
				}
				else Tin= 1.0-Tin;
			}
			if(mtex->texflag & MTEX_STENCIL) {
				if(rgb) {
					
				}
				else {
					fact= Tin;
					Tin*= stencilTin;
					stencilTin*= fact;
				}
			}
			else {
				if(rgb) ;
				else Tin*= stencilTin;
			}
			
			/* colour mapping */
			if(mtex->mapto & (WOMAP_HORIZ+WOMAP_ZENUP+WOMAP_ZENDOWN)) {
				
				if(rgb==0) {
					Tr= mtex->r;
					Tg= mtex->g;
					Tb= mtex->b;
				}
				else Tin= 1.0;

				fact= Tin*mtex->colfac;
				facm= 1.0-fact;
				if(mtex->blendtype==MTEX_MUL) facm= 1.0-mtex->colfac;
				if(mtex->blendtype==MTEX_SUB) fact= -fact;

				if(mtex->mapto & WOMAP_HORIZ) {
					if(mtex->blendtype==MTEX_BLEND) {
						R.wrld.horr= (fact*Tr + facm*wrld_hor->horr);
						R.wrld.horg= (fact*Tg + facm*wrld_hor->horg);
						R.wrld.horb= (fact*Tb + facm*wrld_hor->horb);
					}
					else if(mtex->blendtype==MTEX_MUL) {
						R.wrld.horr= (facm+fact*Tr)*wrld_hor->horr;
						R.wrld.horg= (facm+fact*Tg)*wrld_hor->horg;
						R.wrld.horb= (facm+fact*Tb)*wrld_hor->horb;
					}
					else {
						R.wrld.horr= (fact*Tr + wrld_hor->horr);
						R.wrld.horg= (fact*Tg + wrld_hor->horg);
						R.wrld.horb= (fact*Tb + wrld_hor->horb);
					}
					wrld_hor= &R.wrld;
				}
				if(mtex->mapto & (WOMAP_ZENUP+WOMAP_ZENDOWN)) {
					ok= 0;
					if(R.wrld.skytype & WO_SKYREAL) {
						if((R.wrld.skytype & WO_ZENUP)) {
							if(mtex->mapto & WOMAP_ZENUP) ok= 1;
						}
						else if(mtex->mapto & WOMAP_ZENDOWN) ok= 1;
					}
					else ok= 1;
					
					if(ok) {
					
						if(mtex->blendtype==MTEX_BLEND) {
							R.wrld.zenr= (fact*Tr + facm*wrld_zen->zenr);
							R.wrld.zeng= (fact*Tg + facm*wrld_zen->zeng);
							R.wrld.zenb= (fact*Tb + facm*wrld_zen->zenb);
						}
						else if(mtex->blendtype==MTEX_MUL) {
							R.wrld.zenr= (facm+fact*Tr)*wrld_zen->zenr;
							R.wrld.zeng= (facm+fact*Tg)*wrld_zen->zeng;
							R.wrld.zenb= (facm+fact*Tb)*wrld_zen->zenb;
						}
						else {
							R.wrld.zenr= (fact*Tr + wrld_zen->zenr);
							R.wrld.zeng= (fact*Tg + wrld_zen->zeng);
							R.wrld.zenb= (fact*Tb + wrld_zen->zenb);
						}
						wrld_zen= &R.wrld;
					}
					else {
						/* otherwise zenRGB undefined */
						R.wrld.zenr= wrld_zen->zenr;
						R.wrld.zeng= wrld_zen->zeng;
						R.wrld.zenb= wrld_zen->zenb;
					}
				}
			}
			if(mtex->mapto & WOMAP_BLEND) {
				if(rgb) Tin= (0.35*Tr+0.45*Tg+0.2*Tb);
				
				fact= Tin*mtex->varfac;
				facm= 1.0-fact;
				if(mtex->blendtype==MTEX_MUL) facmul= 1.0-mtex->varfac;
				if(mtex->blendtype==MTEX_SUB) fact= -fact;

				factt= fact; facmm= facm;
				
				if(mtex->blendtype==MTEX_BLEND)
					R.inprz= factt*mtex->def_var+ facmm*R.inprz;
				else if(mtex->blendtype==MTEX_MUL)
					R.inprz= (facmul+factt)*R.inprz;
				else {
					R.inprz= factt+R.inprz;
				}
			}
		}
	}
}

/* ------------------------------------------------------------------------- */
/* explicit lampren stuff should be factored out! or rather, the
   texturing stuff might need to go...*/
void do_lamp_tex(LampRen *la, float *lavec, ShadeInput *shi)
{
	Object *ob;
	LampRen *la_col;
	MTex *mtex;
	Tex *tex;
	float *co = NULL, *dx = NULL, *dy = NULL, fact, facm, stencilTin=1.0;
	float texvec[3], dxt[3], dyt[3], tempvec[3];
	int tex_nr, rgb= 0;
	
	la_col= la->org;
	
	tex_nr= 0;
	
	for(; tex_nr<6; tex_nr++) {
		
		if(la->mtex[tex_nr]) {
			mtex= la->mtex[tex_nr];
			
			tex= mtex->tex;
			if(tex==0) continue;
			tex->nor= NULL;
			
			/* which coords */
			if(mtex->texco==TEXCO_OBJECT) {
				ob= mtex->object;
				if(ob) {
					co= tempvec;
					dx= dxt;
					dy= dyt;
					VECCOPY(tempvec, shi->co);
					MTC_Mat4MulVecfl(ob->imat, tempvec);
					if(shi->osatex) {
						VECCOPY(dxt, O.dxco);
						VECCOPY(dyt, O.dyco);
						MTC_Mat4Mul3Vecfl(ob->imat, dxt);
						MTC_Mat4Mul3Vecfl(ob->imat, dyt);
					}
				}
				else {
					co= shi->co;
					dx= O.dxco; dy= O.dyco;
				}
			}
			else if(mtex->texco==TEXCO_GLOB) {
				co= shi->gl; dx= O.dxco; dy= O.dyco;
				VECCOPY(shi->gl, shi->co);
				MTC_Mat4MulVecfl(R.viewinv, shi->gl);
			}
			else if(mtex->texco==TEXCO_VIEW) {
				
				VECCOPY(tempvec, lavec);
				MTC_Mat3MulVecfl(la->imat, tempvec);
				
				tempvec[0]*= la->spottexfac;
				tempvec[1]*= la->spottexfac;
				co= tempvec; 
				
				dx= dxt; dy= dyt;	
				if(shi->osatex) {
					VECCOPY(dxt, O.dxlv);
					VECCOPY(dyt, O.dylv);
					/* need some matrix conversion here? la->imat is a [3][3]  matrix!!! **/
					MTC_Mat3MulVecfl(la->imat, dxt);
					MTC_Mat3MulVecfl(la->imat, dyt);
					
					VecMulf(dxt, la->spottexfac);
					VecMulf(dyt, la->spottexfac);
				}
			}
			
			
			/* placement */
			if(mtex->projx) texvec[0]= mtex->size[0]*(co[mtex->projx-1]+mtex->ofs[0]);
			else texvec[0]= mtex->size[0]*(mtex->ofs[0]);
			
			if(mtex->projy) texvec[1]= mtex->size[1]*(co[mtex->projy-1]+mtex->ofs[1]);
			else texvec[1]= mtex->size[1]*(mtex->ofs[1]);
			
			if(mtex->projz) texvec[2]= mtex->size[2]*(co[mtex->projz-1]+mtex->ofs[2]);
			else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
			
			if(shi->osatex) {
				if(mtex->projx) {
					dxt[0]= mtex->size[0]*dx[mtex->projx-1];
					dyt[0]= mtex->size[0]*dy[mtex->projx-1];
				}
				else dxt[0]= 0.0;
				if(mtex->projy) {
					dxt[1]= mtex->size[1]*dx[mtex->projy-1];
					dyt[1]= mtex->size[1]*dy[mtex->projy-1];
				}
				else dxt[1]= 0.0;
				if(mtex->projx) {
					dxt[2]= mtex->size[2]*dx[mtex->projz-1];
					dyt[2]= mtex->size[2]*dy[mtex->projz-1];
				}
				else dxt[2]= 0.0;
			}
			
			/* texture */
			if(tex->type==TEX_IMAGE) {
				do_2d_mapping(mtex, texvec, NULL, dxt, dyt);
			}
			
			rgb= multitex(tex, texvec, dxt, dyt, shi->osatex);

			/* texture output */
			if(rgb && (mtex->texflag & MTEX_RGBTOINT)) {
				Tin= (0.35*Tr+0.45*Tg+0.2*Tb);
				rgb= 0;
			}
			if(mtex->texflag & MTEX_NEGATIVE) {
				if(rgb) {
					Tr= 1.0-Tr;
					Tg= 1.0-Tg;
					Tb= 1.0-Tb;
				}
				else Tin= 1.0-Tin;
			}
			if(mtex->texflag & MTEX_STENCIL) {
				if(rgb) {
					fact= Ta;
					Ta*= stencilTin;
					stencilTin*= fact;
				}
				else {
					fact= Tin;
					Tin*= stencilTin;
					stencilTin*= fact;
				}
			}
			else {
				if(rgb) Ta*= stencilTin;
				else Tin*= stencilTin;
			}
			
			/* mapping */
			if(mtex->mapto & LAMAP_COL) {
				
				if(rgb==0) {
					Tr= mtex->r;
					Tg= mtex->g;
					Tb= mtex->b;
				}
				else if(mtex->mapto & MAP_ALPHA) {
					if(mtex->texflag & MTEX_ALPHAMIX) Tin= Ta;
					else Tin= stencilTin;
				}
				else Tin= Ta;

				Tr*= la->energy;
				Tg*= la->energy;
				Tb*= la->energy;

				fact= Tin*mtex->colfac;
				facm= 1.0-fact;
				if(mtex->blendtype==MTEX_MUL) facm= 1.0-mtex->colfac;
				if(mtex->blendtype==MTEX_SUB) fact= -fact;

				if(mtex->blendtype==MTEX_BLEND) {
					la->r= (fact*Tr + facm*la_col->r);
					la->g= (fact*Tg + facm*la_col->g);
					la->b= (fact*Tb + facm*la_col->b);
				}
				else if(mtex->blendtype==MTEX_MUL) {
					la->r= (facm+fact*Tr)*la_col->r;
					la->g= (facm+fact*Tg)*la_col->g;
					la->b= (facm+fact*Tb)*la_col->b;
				}
				else {
					la->r= (fact*Tr + la_col->r);
					la->g= (fact*Tg + la_col->g);
					la->b= (fact*Tb + la_col->b);
				}
				la_col= la; /* makes sure first run uses la->org, then la */
			}
			
		}
	}
}

/* ------------------------------------------------------------------------- */

void externtex(MTex *mtex, float *vec)
{
	Tex *tex;
	float dxt[3], dyt[3], texvec[3], dummy[3];
	int rgb;
	
	tex= mtex->tex;
	if(tex==0) return;
	
	/* placement */
	if(mtex->projx) texvec[0]= mtex->size[0]*(vec[mtex->projx-1]+mtex->ofs[0]);
	else texvec[0]= mtex->size[0]*(mtex->ofs[0]);
	
	if(mtex->projy) texvec[1]= mtex->size[1]*(vec[mtex->projy-1]+mtex->ofs[1]);
	else texvec[1]= mtex->size[1]*(mtex->ofs[1]);
	
	if(mtex->projz) texvec[2]= mtex->size[2]*(vec[mtex->projz-1]+mtex->ofs[2]);
	else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
	
	/* texture */
	if(tex->type==TEX_IMAGE) {
		do_2d_mapping(mtex, texvec, NULL, dxt, dyt);
		
		if(mtex->mapto & MAP_NORM) {
			/* the pointer defines if there's bump */
			tex->nor= dummy;
			if(mtex->maptoneg & MAP_NORM) tex->norfac= -mtex->norfac;
			else tex->norfac= mtex->norfac;
		}
		else tex->nor= NULL;
	}
	
	rgb= multitex(tex, texvec, dxt, dyt, 0);
	
	if(rgb) {
		Tin= (0.35*Tr+0.45*Tg+0.2*Tb);
	}
	else {
		Tr= mtex->r;
		Tg= mtex->g;
		Tb= mtex->b;
	}
}

/* ------------------------------------------------------------------------- */

void externtexcol(MTex *mtex, float *orco, char *col)
{
	int temp;
	float b1;

	if(mtex->tex==0) return;
	
	externtex(mtex, orco);

	b1= 1.0-Tin;

	temp= 255*(Tin*Tr)+b1*col[0];
	if(temp>255) col[0]= 255; else col[0]= temp;
	temp= 255*(Tin*Tg)+b1*col[1];
	if(temp>255) col[1]= 255; else col[1]= temp;
	temp= 255*(Tin*Tb)+b1*col[2];
	if(temp>255) col[2]= 255; else col[2]= temp;
	
}

/* ------------------------------------------------------------------------- */

void render_realtime_texture(ShadeInput *shi)
{
	static Tex tex;
	static int firsttime= 1;
	float texvec[2], dx[2], dy[2];
	
	if(firsttime) {
		default_tex(&tex);
		tex.type= TEX_IMAGE;
		firsttime= 0;
	}
	
	tex.ima = shi->vlr->tface->tpage;
	if(tex.ima) {
		
		texvec[0]= 0.5+0.5*shi->uv[0];
		texvec[1]= 0.5+0.5*shi->uv[1];
		if(shi->osatex) {
			dx[0]= 0.5*O.dxuv[0];
			dx[1]= 0.5*O.dxuv[1];
			dy[0]= 0.5*O.dyuv[0];
			dy[1]= 0.5*O.dyuv[1];
		}
		
		if(shi->osatex) imagewraposa(&tex, texvec, dx, dy); 
		else imagewrap(&tex, texvec); 
		
		shi->vcol[0]*= Tr;
		shi->vcol[1]*= Tg;
		shi->vcol[2]*= Tb;
	}
	
	
}

/* eof */
