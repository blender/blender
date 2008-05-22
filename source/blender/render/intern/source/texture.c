/*
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
 * Contributor(s): 2004-2006, Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MTC_matrixops.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "DNA_texture_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_material_types.h"
#include "DNA_image_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BKE_image.h"
#include "BKE_plugin_types.h"
#include "BKE_utildefines.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"

#include "BKE_library.h"
#include "BKE_image.h"
#include "BKE_texture.h"
#include "BKE_key.h"
#include "BKE_ipo.h"

#include "envmap.h"
#include "renderpipeline.h"
#include "render_types.h"
#include "rendercore.h"
#include "shading.h"
#include "texture.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */




/* note; this is called WITH RENDER IS NULL in src/drawview.c for animated 
   background image, option should move to kernel */
void init_render_texture(Render *re, Tex *tex)
{
	int cfra= G.scene->r.cfra;
	
	if(re) cfra= re->r.cfra;
	
	/* imap test */
	if(tex->ima && ELEM(tex->ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
		BKE_image_user_calc_imanr(&tex->iuser, cfra, re?re->flag & R_SEC_FIELD:0);
	}
	
	if(tex->type==TEX_PLUGIN) {
		if(tex->plugin && tex->plugin->doit) {
			if(tex->plugin->cfra) {
				*(tex->plugin->cfra)= frame_to_float(cfra); 
			}
		}
	}
	else if(tex->type==TEX_ENVMAP) {
		/* just in case */
		tex->imaflag |= TEX_INTERPOL | TEX_MIPMAP;
		tex->extend= TEX_CLIP;
		
		if(tex->env) {
			if(tex->env->type==ENV_PLANE)
				tex->extend= TEX_EXTEND;
			
			/* only free envmap when rendermode was set to render envmaps, for previewrender */
			if(G.rendering && re) {
				if (re->r.mode & R_ENVMAP)
					if(tex->env->stype==ENV_ANIM) 
						BKE_free_envmapdata(tex->env);
			}
		}
	}
}

/* ------------------------------------------------------------------------- */

void init_render_textures(Render *re)
{
	Tex *tex;
	
	tex= G.main->tex.first;
	while(tex) {
		if(tex->id.us) init_render_texture(re, tex);
		tex= tex->id.next;
	}
}

/* ------------------------------------------------------------------------- */


/* this allows colorbanded textures to control normals as well */
static void tex_normal_derivate(Tex *tex, TexResult *texres)
{
	if (tex->flag & TEX_COLORBAND) {
		float col[4];
		if (do_colorband(tex->coba, texres->tin, col)) {
			float fac0, fac1, fac2, fac3;
			
			fac0= (col[0]+col[1]+col[2]);
			do_colorband(tex->coba, texres->nor[0], col);
			fac1= (col[0]+col[1]+col[2]);
			do_colorband(tex->coba, texres->nor[1], col);
			fac2= (col[0]+col[1]+col[2]);
			do_colorband(tex->coba, texres->nor[2], col);
			fac3= (col[0]+col[1]+col[2]);
			
			texres->nor[0]= 0.3333*(fac0 - fac1);
			texres->nor[1]= 0.3333*(fac0 - fac2);
			texres->nor[2]= 0.3333*(fac0 - fac3);
			
			return;
		}
	}
	texres->nor[0]= texres->tin - texres->nor[0];
	texres->nor[1]= texres->tin - texres->nor[1];
	texres->nor[2]= texres->tin - texres->nor[2];
}



static int blend(Tex *tex, float *texvec, TexResult *texres)
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

	if(tex->stype==TEX_LIN) {	/* lin */
		texres->tin= (1.0+x)/2.0;
	}
	else if(tex->stype==TEX_QUAD) {	/* quad */
		texres->tin= (1.0+x)/2.0;
		if(texres->tin<0.0) texres->tin= 0.0;
		else texres->tin*= texres->tin;
	}
	else if(tex->stype==TEX_EASE) {	/* ease */
		texres->tin= (1.0+x)/2.0;
		if(texres->tin<=.0) texres->tin= 0.0;
		else if(texres->tin>=1.0) texres->tin= 1.0;
		else {
			t= texres->tin*texres->tin;
			texres->tin= (3.0*t-2.0*t*texres->tin);
		}
	}
	else if(tex->stype==TEX_DIAG) { /* diag */
		texres->tin= (2.0+x+y)/4.0;
	}
	else if(tex->stype==TEX_RAD) { /* radial */
		texres->tin= (atan2(y,x) / (2*M_PI) + 0.5);
	}
	else {  /* sphere TEX_SPHERE */
		texres->tin= 1.0-sqrt(x*x+	y*y+texvec[2]*texvec[2]);
		if(texres->tin<0.0) texres->tin= 0.0;
		if(tex->stype==TEX_HALO) texres->tin*= texres->tin;  /* halo */
	}

	BRICONT;

	return TEX_INT;
}

/* ------------------------------------------------------------------------- */
/* ************************************************************************* */

/* newnoise: all noisebased types now have different noisebases to choose from */

static int clouds(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;
	
	texres->tin = BLI_gTurbulence(tex->noisesize, texvec[0], texvec[1], texvec[2], tex->noisedepth, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);

	if (texres->nor!=NULL) {
		// calculate bumpnormal
		texres->nor[0] = BLI_gTurbulence(tex->noisesize, texvec[0] + tex->nabla, texvec[1], texvec[2], tex->noisedepth,  (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		texres->nor[1] = BLI_gTurbulence(tex->noisesize, texvec[0], texvec[1] + tex->nabla, texvec[2], tex->noisedepth,  (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		texres->nor[2] = BLI_gTurbulence(tex->noisesize, texvec[0], texvec[1], texvec[2] + tex->nabla, tex->noisedepth,  (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		
		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	if (tex->stype==TEX_COLOR) {
		// in this case, int. value should really be computed from color,
		// and bumpnormal from that, would be too slow, looks ok as is
		texres->tr = texres->tin;
		texres->tg = BLI_gTurbulence(tex->noisesize, texvec[1], texvec[0], texvec[2], tex->noisedepth, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		texres->tb = BLI_gTurbulence(tex->noisesize, texvec[1], texvec[2], texvec[0], tex->noisedepth, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		BRICONTRGB;
		texres->ta = 1.0;
		return (rv | TEX_RGB);
	}

	BRICONT;

	return rv;

}

/* creates a sine wave */
static float tex_sin(float a)
{
	a = 0.5 + 0.5*sin(a);
		
	return a;
}

/* creates a saw wave */
static float tex_saw(float a)
{
	const float b = 2*M_PI;
	
	int n = (int)(a / b);
	a -= n*b;
	if (a < 0) a += b;
	return a / b;
}

/* creates a triangle wave */
static float tex_tri(float a)
{
	const float b = 2*M_PI;
	const float rmax = 1.0;
	
	a = rmax - 2.0*fabs(floor((a*(1.0/b))+0.5) - (a*(1.0/b)));
	
	return a;
}

/* computes basic wood intensity value at x,y,z */
static float wood_int(Tex *tex, float x, float y, float z)
{
	float wi=0;						
	short wf = tex->noisebasis2;	/* wave form:	TEX_SIN=0,  TEX_SAW=1,  TEX_TRI=2						 */
	short wt = tex->stype;			/* wood type:	TEX_BAND=0, TEX_RING=1, TEX_BANDNOISE=2, TEX_RINGNOISE=3 */

	float (*waveform[3])(float);	/* create array of pointers to waveform functions */
	waveform[0] = tex_sin;			/* assign address of tex_sin() function to pointer array */
	waveform[1] = tex_saw;
	waveform[2] = tex_tri;
	
	if ((wf>TEX_TRI) || (wf<TEX_SIN)) wf=0; /* check to be sure noisebasis2 is initialized ahead of time */
		
	if (wt==TEX_BAND) {
		wi = waveform[wf]((x + y + z)*10.0);
	}
	else if (wt==TEX_RING) {
		wi = waveform[wf](sqrt(x*x + y*y + z*z)*20.0);
	}
	else if (wt==TEX_BANDNOISE) {
		wi = tex->turbul*BLI_gNoise(tex->noisesize, x, y, z, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		wi = waveform[wf]((x + y + z)*10.0 + wi);
	}
	else if (wt==TEX_RINGNOISE) {
		wi = tex->turbul*BLI_gNoise(tex->noisesize, x, y, z, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		wi = waveform[wf](sqrt(x*x + y*y + z*z)*20.0 + wi);
	}
	
	return wi;
}

static int wood(Tex *tex, float *texvec, TexResult *texres)
{
	int rv=TEX_INT;

	texres->tin = wood_int(tex, texvec[0], texvec[1], texvec[2]);
	if (texres->nor!=NULL) {
		/* calculate bumpnormal */
		texres->nor[0] = wood_int(tex, texvec[0] + tex->nabla, texvec[1], texvec[2]);
		texres->nor[1] = wood_int(tex, texvec[0], texvec[1] + tex->nabla, texvec[2]);
		texres->nor[2] = wood_int(tex, texvec[0], texvec[1], texvec[2] + tex->nabla);
		
		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	BRICONT;

	return rv;
}

/* computes basic marble intensity at x,y,z */
static float marble_int(Tex *tex, float x, float y, float z)
{
	float n, mi;
	short wf = tex->noisebasis2;	/* wave form:	TEX_SIN=0,  TEX_SAW=1,  TEX_TRI=2						*/
	short mt = tex->stype;			/* marble type:	TEX_SOFT=0,	TEX_SHARP=1,TEX_SHAPER=2 					*/
	
	float (*waveform[3])(float);	/* create array of pointers to waveform functions */
	waveform[0] = tex_sin;			/* assign address of tex_sin() function to pointer array */
	waveform[1] = tex_saw;
	waveform[2] = tex_tri;
	
	if ((wf>TEX_TRI) || (wf<TEX_SIN)) wf=0; /* check to be sure noisebasis2 isn't initialized ahead of time */
	
	n = 5.0 * (x + y + z);
	
	mi = n + tex->turbul * BLI_gTurbulence(tex->noisesize, x, y, z, tex->noisedepth, (tex->noisetype!=TEX_NOISESOFT),  tex->noisebasis);

	if (mt>=TEX_SOFT) {  /* TEX_SOFT always true */
		mi = waveform[wf](mi);
		if (mt==TEX_SHARP) {
			mi = sqrt(mi);
		} 
		else if (mt==TEX_SHARPER) {
			mi = sqrt(sqrt(mi));
		}
	}

	return mi;
}

static int marble(Tex *tex, float *texvec, TexResult *texres)
{
	int rv=TEX_INT;

	texres->tin = marble_int(tex, texvec[0], texvec[1], texvec[2]);

	if (texres->nor!=NULL) {
		/* calculate bumpnormal */
		texres->nor[0] = marble_int(tex, texvec[0] + tex->nabla, texvec[1], texvec[2]);
		texres->nor[1] = marble_int(tex, texvec[0], texvec[1] + tex->nabla, texvec[2]);
		texres->nor[2] = marble_int(tex, texvec[0], texvec[1], texvec[2] + tex->nabla);
		
		tex_normal_derivate(tex, texres);
		
		rv |= TEX_NOR;
	}

	BRICONT;

	return rv;
}

/* ------------------------------------------------------------------------- */

static int magic(Tex *tex, float *texvec, TexResult *texres)
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
	texres->tr= 0.5-x;
	texres->tg= 0.5-y;
	texres->tb= 0.5-z;

	texres->tin= 0.3333*(texres->tr+texres->tg+texres->tb);
	
	BRICONTRGB;
	texres->ta= 1.0;
	
	return TEX_RGB;
}

/* ------------------------------------------------------------------------- */

/* newnoise: stucci also modified to use different noisebasis */
static int stucci(Tex *tex, float *texvec, TexResult *texres)
{
	float nor[3], b2, ofs;
	int retval= TEX_INT;
	
	b2= BLI_gNoise(tex->noisesize, texvec[0], texvec[1], texvec[2], (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
	
	ofs= tex->turbul/200.0;

	if(tex->stype) ofs*=(b2*b2);
	nor[0] = BLI_gNoise(tex->noisesize, texvec[0]+ofs, texvec[1], texvec[2], (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
	nor[1] = BLI_gNoise(tex->noisesize, texvec[0], texvec[1]+ofs, texvec[2], (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);	
	nor[2] = BLI_gNoise(tex->noisesize, texvec[0], texvec[1], texvec[2]+ofs, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);

	texres->tin= nor[2];
	
	if(texres->nor) { 
		
		VECCOPY(texres->nor, nor);
		tex_normal_derivate(tex, texres);
		
		if(tex->stype==TEX_WALLOUT) {
			texres->nor[0]= -texres->nor[0];
			texres->nor[1]= -texres->nor[1];
			texres->nor[2]= -texres->nor[2];
		}
		
		retval |= TEX_NOR;
	}
	
	if(tex->stype==TEX_WALLOUT) 
		texres->tin= 1.0f-texres->tin;
	
	if(texres->tin<0.0f)
		texres->tin= 0.0f;
	
	return retval;
}

/* ------------------------------------------------------------------------- */
/* newnoise: musgrave terrain noise types */

static float mg_mFractalOrfBmTex(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;
	float (*mgravefunc)(float, float, float, float, float, float, int);

	if (tex->stype==TEX_MFRACTAL)
		mgravefunc = mg_MultiFractal;
	else
		mgravefunc = mg_fBm;

	texres->tin = tex->ns_outscale*mgravefunc(texvec[0], texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->noisebasis);

	if (texres->nor!=NULL) {
		float offs= tex->nabla/tex->noisesize;	// also scaling of texvec
		
		/* calculate bumpnormal */
		texres->nor[0] = tex->ns_outscale*mgravefunc(texvec[0] + offs, texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->noisebasis);
		texres->nor[1] = tex->ns_outscale*mgravefunc(texvec[0], texvec[1] + offs, texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->noisebasis);
		texres->nor[2] = tex->ns_outscale*mgravefunc(texvec[0], texvec[1], texvec[2] + offs, tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->noisebasis);
		
		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	BRICONT;

	return rv;

}

static float mg_ridgedOrHybridMFTex(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;
	float (*mgravefunc)(float, float, float, float, float, float, float, float, int);

	if (tex->stype==TEX_RIDGEDMF)
		mgravefunc = mg_RidgedMultiFractal;
	else
		mgravefunc = mg_HybridMultiFractal;

	texres->tin = tex->ns_outscale*mgravefunc(texvec[0], texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->mg_gain, tex->noisebasis);

	if (texres->nor!=NULL) {
		float offs= tex->nabla/tex->noisesize;	// also scaling of texvec
		
		/* calculate bumpnormal */
		texres->nor[0] = tex->ns_outscale*mgravefunc(texvec[0] + offs, texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->mg_gain, tex->noisebasis);
		texres->nor[1] = tex->ns_outscale*mgravefunc(texvec[0], texvec[1] + offs, texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->mg_gain, tex->noisebasis);
		texres->nor[2] = tex->ns_outscale*mgravefunc(texvec[0], texvec[1], texvec[2] + offs, tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->mg_gain, tex->noisebasis);
		
		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	BRICONT;

	return rv;

}


static float mg_HTerrainTex(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;

	texres->tin = tex->ns_outscale*mg_HeteroTerrain(texvec[0], texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->noisebasis);

	if (texres->nor!=NULL) {
		float offs= tex->nabla/tex->noisesize;	// also scaling of texvec
		
		/* calculate bumpnormal */
		texres->nor[0] = tex->ns_outscale*mg_HeteroTerrain(texvec[0] + offs, texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->noisebasis);
		texres->nor[1] = tex->ns_outscale*mg_HeteroTerrain(texvec[0], texvec[1] + offs, texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->noisebasis);
		texres->nor[2] = tex->ns_outscale*mg_HeteroTerrain(texvec[0], texvec[1], texvec[2] + offs, tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->noisebasis);
		
		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	BRICONT;

	return rv;

}


static float mg_distNoiseTex(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;

	texres->tin = mg_VLNoise(texvec[0], texvec[1], texvec[2], tex->dist_amount, tex->noisebasis, tex->noisebasis2);

	if (texres->nor!=NULL) {
		float offs= tex->nabla/tex->noisesize;	// also scaling of texvec
		
		/* calculate bumpnormal */
		texres->nor[0] = mg_VLNoise(texvec[0] + offs, texvec[1], texvec[2], tex->dist_amount, tex->noisebasis, tex->noisebasis2);
		texres->nor[1] = mg_VLNoise(texvec[0], texvec[1] + offs, texvec[2], tex->dist_amount, tex->noisebasis, tex->noisebasis2);
		texres->nor[2] = mg_VLNoise(texvec[0], texvec[1], texvec[2] + offs, tex->dist_amount, tex->noisebasis, tex->noisebasis2);

		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	BRICONT;


	return rv;

}


/* ------------------------------------------------------------------------- */
/* newnoise: Voronoi texture type, probably the slowest, especially with minkovsky, bumpmapping, could be done another way */

static float voronoiTex(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;
	float da[4], pa[12];	/* distance and point coordinate arrays of 4 nearest neighbours */
	float aw1 = fabs(tex->vn_w1);
	float aw2 = fabs(tex->vn_w2);
	float aw3 = fabs(tex->vn_w3);
	float aw4 = fabs(tex->vn_w4);
	float sc = (aw1 + aw2 + aw3 + aw4);
	if (sc!=0.f) sc =  tex->ns_outscale/sc;

	voronoi(texvec[0], texvec[1], texvec[2], da, pa, tex->vn_mexp, tex->vn_distm);
	texres->tin = sc * fabs(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);

	if (tex->vn_coltype) {
		float ca[3];	/* cell color */
		cellNoiseV(pa[0], pa[1], pa[2], ca);
		texres->tr = aw1*ca[0];
		texres->tg = aw1*ca[1];
		texres->tb = aw1*ca[2];
		cellNoiseV(pa[3], pa[4], pa[5], ca);
		texres->tr += aw2*ca[0];
		texres->tg += aw2*ca[1];
		texres->tb += aw2*ca[2];
		cellNoiseV(pa[6], pa[7], pa[8], ca);
		texres->tr += aw3*ca[0];
		texres->tg += aw3*ca[1];
		texres->tb += aw3*ca[2];
		cellNoiseV(pa[9], pa[10], pa[11], ca);
		texres->tr += aw4*ca[0];
		texres->tg += aw4*ca[1];
		texres->tb += aw4*ca[2];
		if (tex->vn_coltype>=2) {
			float t1 = (da[1]-da[0])*10;
			if (t1>1) t1=1;
			if (tex->vn_coltype==3) t1*=texres->tin; else t1*=sc;
			texres->tr *= t1;
			texres->tg *= t1;
			texres->tb *= t1;
		}
		else {
			texres->tr *= sc;
			texres->tg *= sc;
			texres->tb *= sc;
		}
	}

	if (texres->nor!=NULL) {
		float offs= tex->nabla/tex->noisesize;	// also scaling of texvec

		/* calculate bumpnormal */
		voronoi(texvec[0] + offs, texvec[1], texvec[2], da, pa, tex->vn_mexp,  tex->vn_distm);
		texres->nor[0] = sc * fabs(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);
		voronoi(texvec[0], texvec[1] + offs, texvec[2], da, pa, tex->vn_mexp,  tex->vn_distm);
		texres->nor[1] = sc * fabs(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);
		voronoi(texvec[0], texvec[1], texvec[2] + offs, da, pa, tex->vn_mexp,  tex->vn_distm);
		texres->nor[2] = sc * fabs(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);
		
		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	if (tex->vn_coltype) {
		BRICONTRGB;
		texres->ta = 1.0;
		return (rv | TEX_RGB);
	}
	
	BRICONT;

	return rv;

}


/* ------------------------------------------------------------------------- */

static int texnoise(Tex *tex, TexResult *texres)
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
	
	texres->tin= ((float)val)/div;;

	BRICONT;
	return TEX_INT;
}

/* ------------------------------------------------------------------------- */

static int plugintex(Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres)
{
	PluginTex *pit;
	int rgbnor=0;
	float result[ 8 ];

	texres->tin= 0.0;

	pit= tex->plugin;
	if(pit && pit->doit) {
		if(texres->nor) {
			if (pit->version < 6) {
				VECCOPY(pit->result+5, texres->nor);
			} else {
				VECCOPY(result+5, texres->nor);
			}
		}
		if (pit->version < 6) {
			if(osatex) rgbnor= ((TexDoitold)pit->doit)(tex->stype, 
				pit->data, texvec, dxt, dyt);
			else rgbnor= ((TexDoitold)pit->doit)(tex->stype, 
				pit->data, texvec, 0, 0);
		} else {
			if(osatex) rgbnor= ((TexDoit)pit->doit)(tex->stype, 
				pit->data, texvec, dxt, dyt, result);
			else rgbnor= ((TexDoit)pit->doit)(tex->stype, 
				pit->data, texvec, 0, 0, result);
		}

		if (pit->version < 6) {
			texres->tin = pit->result[0];
		} else {
			texres->tin = result[0];
		}

		if(rgbnor & TEX_NOR) {
			if(texres->nor) {
				if (pit->version < 6) {
					VECCOPY(texres->nor, pit->result+5);
				} else {
					VECCOPY(texres->nor, result+5);
				}
			}
		}
		
		if(rgbnor & TEX_RGB) {
			if (pit->version < 6) {
				texres->tr = pit->result[1];
				texres->tg = pit->result[2];
				texres->tb = pit->result[3];
				texres->ta = pit->result[4];
			} else {
				texres->tr = result[1];
				texres->tg = result[2];
				texres->tb = result[3];
				texres->ta = result[4];
			}

			BRICONTRGB;
		}
		
		BRICONT;
	}

	return rgbnor;
}


static int cubemap_glob(float *n, float x, float y, float z, float *adr1, float *adr2)
{
	float x1, y1, z1, nor[3];
	int ret;
	
	if(n==NULL) {
		nor[0]= x; nor[1]= y; nor[2]= z;	// use local render coord
	}
	else {
		VECCOPY(nor, n);
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

/* mtex argument only for projection switches */
static int cubemap(MTex *mtex, VlakRen *vlr, float *n, float x, float y, float z, float *adr1, float *adr2)
{
	int proj[4]={0, ME_PROJXY, ME_PROJXZ, ME_PROJYZ}, ret= 0;
	
	if(vlr) {
		int index;
		
		/* Mesh vertices have such flags, for others we calculate it once based on orco */
		if((vlr->puno & (ME_PROJXY|ME_PROJXZ|ME_PROJYZ))==0) {
			/* test for v1, vlr can be faked for baking */
			if(vlr->v1 && vlr->v1->orco) {
				float nor[3];
				CalcNormFloat(vlr->v1->orco, vlr->v2->orco, vlr->v3->orco, nor);
				
				if( fabs(nor[0])<fabs(nor[2]) && fabs(nor[1])<fabs(nor[2]) ) vlr->puno |= ME_PROJXY;
				else if( fabs(nor[0])<fabs(nor[1]) && fabs(nor[2])<fabs(nor[1]) ) vlr->puno |= ME_PROJXZ;
				else vlr->puno |= ME_PROJYZ;
			}
			else return cubemap_glob(n, x, y, z, adr1, adr2);
		}
		
		if(mtex) {
			/* the mtex->proj{xyz} have type char. maybe this should be wider? */
			/* casting to int ensures that the index type is right.            */
			index = (int) mtex->projx;
			proj[index]= ME_PROJXY;

			index = (int) mtex->projy;
			proj[index]= ME_PROJXZ;

			index = (int) mtex->projz;
			proj[index]= ME_PROJYZ;
		}
		
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
		return cubemap_glob(n, x, y, z, adr1, adr2);
	}
	
	return ret;
}

/* ------------------------------------------------------------------------- */

static int cubemap_ob(Object *ob, float *n, float x, float y, float z, float *adr1, float *adr2)
{
	float x1, y1, z1, nor[3];
	int ret;
	
	if(n==NULL) return 0;
	
	VECCOPY(nor, n);
	if(ob) MTC_Mat4Mul3Vecfl(ob->imat, nor);
	
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

static void do_2d_mapping(MTex *mtex, float *t, VlakRen *vlr, float *n, float *dxt, float *dyt)
{
	Tex *tex;
	Object *ob= NULL;
	float fx, fy, fac1, area[8];
	int ok, proj, areaflag= 0, wrap, texco;
	
	/* mtex variables localized, only cubemap doesn't cooperate yet... */
	wrap= mtex->mapping;
	tex= mtex->tex;
	ob= mtex->object;
	texco= mtex->texco;

	if(R.osa==0) {
		
		if(wrap==MTEX_FLAT) {
			fx = (t[0] + 1.0) / 2.0;
			fy = (t[1] + 1.0) / 2.0;
		}
		else if(wrap==MTEX_TUBE) tubemap(t[0], t[1], t[2], &fx, &fy);
		else if(wrap==MTEX_SPHERE) spheremap(t[0], t[1], t[2], &fx, &fy);
		else {
			if(texco==TEXCO_OBJECT) cubemap_ob(ob, n, t[0], t[1], t[2], &fx, &fy);
			else if(texco==TEXCO_GLOB) cubemap_glob(n, t[0], t[1], t[2], &fx, &fy);
			else cubemap(mtex, vlr, n, t[0], t[1], t[2], &fx, &fy);
		}
		
		/* repeat */
		if(tex->extend==TEX_REPEAT) {
			if(tex->xrepeat>1) {
				float origf= fx *= tex->xrepeat;
				
				if(fx>1.0) fx -= (int)(fx);
				else if(fx<0.0) fx+= 1-(int)(fx);
				
				if(tex->flag & TEX_REPEAT_XMIR) {
					int orig= (int)floor(origf);
					if(orig & 1) 
						fx= 1.0-fx;
				}
			}
			if(tex->yrepeat>1) {
				float origf= fy *= tex->yrepeat;
				
				if(fy>1.0) fy -= (int)(fy);
				else if(fy<0.0) fy+= 1-(int)(fy);
				
				if(tex->flag & TEX_REPEAT_YMIR) {
					int orig= (int)floor(origf);
					if(orig & 1) 
						fy= 1.0-fy;
				}
			}
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
			dxt[2]/= 2.0;
			dyt[0]/= 2.0; 
			dyt[1]/= 2.0;
			dyt[2]/= 2.0;
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

			if(texco==TEXCO_OBJECT) proj = cubemap_ob(ob, n, t[0], t[1], t[2], &fx, &fy);
			else if (texco==TEXCO_GLOB) proj = cubemap_glob(n, t[0], t[1], t[2], &fx, &fy);
			else proj = cubemap(mtex, vlr, n, t[0], t[1], t[2], &fx, &fy);

			if(proj==1) {
				SWAP(float, dxt[1], dxt[2]);
				SWAP(float, dyt[1], dyt[2]);
			}
			else if(proj==2) {
				float f1= dxt[0], f2= dyt[0];
				dxt[0]= dxt[1];
				dyt[0]= dyt[1];
				dxt[1]= dxt[2];
				dyt[1]= dyt[2];
				dxt[2]= f1;
				dyt[2]= f2;
			}
			dxt[0]/= 2.0; 
			dxt[1]/= 2.0;
			dxt[2]/= 2.0;
			
			dyt[0]/= 2.0; 
			dyt[1]/= 2.0;
			dyt[2]/= 2.0;
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
		if(tex->extend==TEX_REPEAT) {
			float max= 1.0f;
			if(tex->xrepeat>1) {
				float origf= fx *= tex->xrepeat;
				
				if(fx>1.0f) fx -= (int)(fx);
				else if(fx<0.0f) fx+= 1-(int)(fx);
				
				if(tex->flag & TEX_REPEAT_XMIR) {
					int orig= (int)floor(origf);
					if(orig & 1) 
						fx= 1.0f-fx;
				}
				
				max= tex->xrepeat;
				
				dxt[0]*= tex->xrepeat;
				dyt[0]*= tex->xrepeat;
			}
			if(tex->yrepeat>1) {
				float origf= fy *= tex->yrepeat;
				
				if(fy>1.0f) fy -= (int)(fy);
				else if(fy<0.0f) fy+= 1-(int)(fy);
				
				if(tex->flag & TEX_REPEAT_YMIR) {
					int orig= (int)floor(origf);
					if(orig & 1) 
						fy= 1.0f-fy;
				}
				
				if(max<tex->yrepeat)
					max= tex->yrepeat;

				dxt[1]*= tex->yrepeat;
				dyt[1]*= tex->yrepeat;
			}
			if(max!=1.0f) {
				dxt[2]*= max;
				dyt[2]*= max;
			}
			
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

static int multitex(Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres)
{
	float tmpvec[3];
	int retval=0; /* return value, int:0, col:1, nor:2, everything:3 */

	texres->talpha= 0;	/* is set when image texture returns alpha (considered premul) */
	
	switch(tex->type) {
	
	case 0:
		texres->tin= 0.0f;
		return 0;
	case TEX_CLOUDS:
		retval= clouds(tex, texvec, texres);
		break;
	case TEX_WOOD:
		retval= wood(tex, texvec, texres); 
		break;
	case TEX_MARBLE:
		retval= marble(tex, texvec, texres); 
		break;
	case TEX_MAGIC:
		retval= magic(tex, texvec, texres); 
		break;
	case TEX_BLEND:
		retval= blend(tex, texvec, texres);
		break;
	case TEX_STUCCI:
		retval= stucci(tex, texvec, texres); 
		break;
	case TEX_NOISE:
		retval= texnoise(tex, texres); 
		break;
	case TEX_IMAGE:
		if(osatex) retval= imagewraposa(tex, tex->ima, NULL, texvec, dxt, dyt, texres); 
		else retval= imagewrap(tex, tex->ima, NULL, texvec, texres); 
		tag_image_time(tex->ima); /* tag image as having being used */
		break;
	case TEX_PLUGIN:
		retval= plugintex(tex, texvec, dxt, dyt, osatex, texres);
		break;
	case TEX_ENVMAP:
		retval= envmaptex(tex, texvec, dxt, dyt, osatex, texres);
		break;
	case TEX_MUSGRAVE:
		/* newnoise: musgrave types */
		
		/* ton: added this, for Blender convention reason. 
		 * artificer: added the use of tmpvec to avoid scaling texvec
		 */
		VECCOPY(tmpvec, texvec);
		VecMulf(tmpvec, 1.0/tex->noisesize);
		
		switch(tex->stype) {
		case TEX_MFRACTAL:
		case TEX_FBM:
			retval= mg_mFractalOrfBmTex(tex, tmpvec, texres);
			break;
		case TEX_RIDGEDMF:
		case TEX_HYBRIDMF:
			retval= mg_ridgedOrHybridMFTex(tex, tmpvec, texres);
			break;
		case TEX_HTERRAIN:
			retval= mg_HTerrainTex(tex, tmpvec, texres);
			break;
		}
		break;
	/* newnoise: voronoi type */
	case TEX_VORONOI:
		/* ton: added this, for Blender convention reason.
		 * artificer: added the use of tmpvec to avoid scaling texvec
		 */
		VECCOPY(tmpvec, texvec);
		VecMulf(tmpvec, 1.0/tex->noisesize);
		
		retval= voronoiTex(tex, tmpvec, texres);
		break;
	case TEX_DISTNOISE:
		/* ton: added this, for Blender convention reason.
		 * artificer: added the use of tmpvec to avoid scaling texvec
		 */
		VECCOPY(tmpvec, texvec);
		VecMulf(tmpvec, 1.0/tex->noisesize);
		
		retval= mg_distNoiseTex(tex, tmpvec, texres);
		break;
	}

	if (tex->flag & TEX_COLORBAND) {
		float col[4];
		if (do_colorband(tex->coba, texres->tin, col)) {
			texres->talpha= 1;
			texres->tr= col[0];
			texres->tg= col[1];
			texres->tb= col[2];
			texres->ta= col[3];
			retval |= 1;
		}
	}
	return retval;
}

/* Warning, if the texres's values are not declared zero, check the return value to be sure
 * the color values are set before using the r/g/b values, otherwise you may use uninitialized values - Campbell */
int multitex_ext(Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres)
{
	
	if(tex==NULL) {
		memset(texres, 0, sizeof(TexResult));
		return 0;
	}
	
	/* Image requires 2d mapping conversion */
	if(tex->type==TEX_IMAGE) {
		MTex mtex;
		float texvec_l[3], dxt_l[3], dyt_l[3];
		
		mtex.mapping= MTEX_FLAT;
		mtex.tex= tex;
		mtex.object= NULL;
		mtex.texco= TEXCO_ORCO;
		
		VECCOPY(texvec_l, texvec);
		if(dxt && dyt) {
			VECCOPY(dxt_l, dxt);
			VECCOPY(dyt_l, dyt);
		}
		else {
			dxt_l[0]= dxt_l[1]= dxt_l[2]= 0.0f;
			dyt_l[0]= dyt_l[1]= dyt_l[2]= 0.0f;
		}
		
		do_2d_mapping(&mtex, texvec_l, NULL, NULL, dxt_l, dyt_l);

		return multitex(tex, texvec_l, dxt_l, dyt_l, osatex, texres);
	}
	else
		return multitex(tex, texvec, dxt, dyt, osatex, texres);
}

/* ------------------------------------------------------------------------- */

/* in = destination, tex = texture, out = previous color */
/* fact = texture strength, facg = button strength value */
void texture_rgb_blend(float *in, float *tex, float *out, float fact, float facg, int blendtype)
{
	float facm, col;
	
	switch(blendtype) {
	case MTEX_BLEND:
		fact*= facg;
		facm= 1.0-fact;

		in[0]= (fact*tex[0] + facm*out[0]);
		in[1]= (fact*tex[1] + facm*out[1]);
		in[2]= (fact*tex[2] + facm*out[2]);
		break;
		
	case MTEX_MUL:
		fact*= facg;
		facm= 1.0-facg;
		in[0]= (facm+fact*tex[0])*out[0];
		in[1]= (facm+fact*tex[1])*out[1];
		in[2]= (facm+fact*tex[2])*out[2];
		break;

	case MTEX_SCREEN:
		fact*= facg;
		facm= 1.0-facg;
		in[0]= 1.0 - (facm+fact*(1.0-tex[0])) * (1.0-out[0]);
		in[1]= 1.0 - (facm+fact*(1.0-tex[1])) * (1.0-out[1]);
		in[2]= 1.0 - (facm+fact*(1.0-tex[2])) * (1.0-out[2]);
		break;

	case MTEX_OVERLAY:
		fact*= facg;
		facm= 1.0-facg;
		
		if(out[0] < 0.5f)
			in[0] = out[0] * (facm + 2.0f*fact*tex[0]);
		else
			in[0] = 1.0f - (facm + 2.0f*fact*(1.0 - tex[0])) * (1.0 - out[0]);
		if(out[1] < 0.5f)
			in[1] = out[1] * (facm + 2.0f*fact*tex[1]);
		else
			in[1] = 1.0f - (facm + 2.0f*fact*(1.0 - tex[1])) * (1.0 - out[1]);
		if(out[2] < 0.5f)
			in[2] = out[2] * (facm + 2.0f*fact*tex[2]);
		else
			in[2] = 1.0f - (facm + 2.0f*fact*(1.0 - tex[2])) * (1.0 - out[2]);
		break;
		
	case MTEX_SUB:
		fact= -fact;
	case MTEX_ADD:
		fact*= facg;
		in[0]= (fact*tex[0] + out[0]);
		in[1]= (fact*tex[1] + out[1]);
		in[2]= (fact*tex[2] + out[2]);
		break;

	case MTEX_DIV:
		fact*= facg;
		facm= 1.0-fact;
		
		if(tex[0]!=0.0)
			in[0]= facm*out[0] + fact*out[0]/tex[0];
		if(tex[1]!=0.0)
			in[1]= facm*out[1] + fact*out[1]/tex[1];
		if(tex[2]!=0.0)
			in[2]= facm*out[2] + fact*out[2]/tex[2];

		break;

	case MTEX_DIFF:
		fact*= facg;
		facm= 1.0-fact;
		in[0]= facm*out[0] + fact*fabs(tex[0]-out[0]);
		in[1]= facm*out[1] + fact*fabs(tex[1]-out[1]);
		in[2]= facm*out[2] + fact*fabs(tex[2]-out[2]);
		break;

	case MTEX_DARK:
		fact*= facg;
		facm= 1.0-fact;
		
		col= fact*tex[0];
		if(col < out[0]) in[0]= col; else in[0]= out[0];
		col= fact*tex[1];
		if(col < out[1]) in[1]= col; else in[1]= out[1];
		col= fact*tex[2];
		if(col < out[2]) in[2]= col; else in[2]= out[2];
		break;

	case MTEX_LIGHT:
		fact*= facg;
		facm= 1.0-fact;
		
		col= fact*tex[0];
		if(col > out[0]) in[0]= col; else in[0]= out[0];
		col= fact*tex[1];
		if(col > out[1]) in[1]= col; else in[1]= out[1];
		col= fact*tex[2];
		if(col > out[2]) in[2]= col; else in[2]= out[2];
		break;
		
	case MTEX_BLEND_HUE:
		fact*= facg;
		VECCOPY(in, out);
		ramp_blend(MA_RAMP_HUE, in, in+1, in+2, fact, tex);
		break;
	case MTEX_BLEND_SAT:
		fact*= facg;
		VECCOPY(in, out);
		ramp_blend(MA_RAMP_SAT, in, in+1, in+2, fact, tex);
		break;
	case MTEX_BLEND_VAL:
		fact*= facg;
		VECCOPY(in, out);
		ramp_blend(MA_RAMP_VAL, in, in+1, in+2, fact, tex);
		break;
	case MTEX_BLEND_COLOR:
		fact*= facg;
		VECCOPY(in, out);
		ramp_blend(MA_RAMP_COLOR, in, in+1, in+2, fact, tex);
		break;
	}
}

float texture_value_blend(float tex, float out, float fact, float facg, int blendtype, int flip)
{
	float in=0.0, facm, col;
	
	fact*= facg;
	facm= 1.0-fact;
	if(flip) SWAP(float, fact, facm);

	switch(blendtype) {
	case MTEX_BLEND:
		in= fact*tex + facm*out;
		break;

	case MTEX_MUL:
		facm= 1.0-facg;
		in= (facm+fact*tex)*out;
		break;

	case MTEX_SCREEN:
		facm= 1.0-facg;
		in= 1.0-(facm+fact*(1.0-tex))*(1.0-out);
		break;

	case MTEX_SUB:
		fact= -fact;
	case MTEX_ADD:
		in= fact*tex + out;
		break;

	case MTEX_DIV:
		if(tex!=0.0)
			in= facm*out + fact*out/tex;
		break;

	case MTEX_DIFF:
		in= facm*out + fact*fabs(tex-out);
		break;

	case MTEX_DARK:
		col= fact*tex;
		if(col < out) in= col; else in= out;
		break;

	case MTEX_LIGHT:
		col= fact*tex;
		if(col > out) in= col; else in= out;
		break;
	}
	
	return in;
}


void do_material_tex(ShadeInput *shi)
{
	MTex *mtex;
	Tex *tex;
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float *co = NULL, *dx = NULL, *dy = NULL;
	float fact, facm, factt, facmm, stencilTin=1.0;
	float texvec[3], dxt[3], dyt[3], tempvec[3], norvec[3], warpvec[3]={0.0f, 0.0f, 0.0f}, Tnor=1.0;
	int tex_nr, rgbnor= 0, warpdone=0;

	if (R.r.scemode & R_NO_TEX) return;
	/* here: test flag if there's a tex (todo) */
	
	for(tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
		
		/* separate tex switching */
		if(shi->mat->septex & (1<<tex_nr)) continue;
		
		if(shi->mat->mtex[tex_nr]) {
			mtex= shi->mat->mtex[tex_nr];
			
			tex= mtex->tex;
			if(tex==0) continue;

			/* which coords */
			if(mtex->texco==TEXCO_ORCO) {
				if(mtex->texflag & MTEX_DUPLI_MAPTO) {
					co= shi->duplilo; dx= dxt; dy= dyt;
					dxt[0]= dxt[1]= dxt[2]= 0.0f;
					dyt[0]= dyt[1]= dyt[2]= 0.0f;
				}
				else {
					co= shi->lo; dx= shi->dxlo; dy= shi->dylo;
				}
			}
			else if(mtex->texco==TEXCO_STICKY) {
				co= shi->sticky; dx= shi->dxsticky; dy= shi->dysticky;
			}
			else if(mtex->texco==TEXCO_OBJECT) {
				Object *ob= mtex->object;
				if(ob) {
					co= tempvec;
					dx= dxt;
					dy= dyt;
					VECCOPY(tempvec, shi->co);
					if(mtex->texflag & MTEX_OB_DUPLI_ORIG)
						if(shi->obi && shi->obi->duplitexmat)
							MTC_Mat4MulVecfl(shi->obi->duplitexmat, tempvec);
					MTC_Mat4MulVecfl(ob->imat, tempvec);
					if(shi->osatex) {
						VECCOPY(dxt, shi->dxco);
						VECCOPY(dyt, shi->dyco);
						MTC_Mat4Mul3Vecfl(ob->imat, dxt);
						MTC_Mat4Mul3Vecfl(ob->imat, dyt);
					}
				}
				else {
					/* if object doesn't exist, do not use orcos (not initialized) */
					co= shi->co;
					dx= shi->dxco; dy= shi->dyco;
				}
			}
			else if(mtex->texco==TEXCO_REFL) {
				co= shi->ref; dx= shi->dxref; dy= shi->dyref;
			}
			else if(mtex->texco==TEXCO_NORM) {
				co= shi->orn; dx= shi->dxno; dy= shi->dyno;
			}
			else if(mtex->texco==TEXCO_TANGENT) {
				co= shi->tang; dx= shi->dxno; dy= shi->dyno;
			}
			else if(mtex->texco==TEXCO_GLOB) {
				co= shi->gl; dx= shi->dxco; dy= shi->dyco;
			}
			else if(mtex->texco==TEXCO_UV) {
				if(mtex->texflag & MTEX_DUPLI_MAPTO) {
					co= shi->dupliuv; dx= dxt; dy= dyt;
					dxt[0]= dxt[1]= dxt[2]= 0.0f;
					dyt[0]= dyt[1]= dyt[2]= 0.0f;
				}
				else {
					ShadeInputUV *suv= &shi->uv[shi->actuv];
					int i;

					if(mtex->uvname[0] != 0) {
						for(i = 0; i < shi->totuv; i++) {
							if(strcmp(shi->uv[i].name, mtex->uvname)==0) {
								suv= &shi->uv[i];
								break;
							}
						}
					}

					co= suv->uv;
					dx= suv->dxuv;
					dy= suv->dyuv; 
				}
			}
			else if(mtex->texco==TEXCO_WINDOW) {
				co= shi->winco; dx= shi->dxwin; dy= shi->dywin;
			}
			else if(mtex->texco==TEXCO_STRAND) {
				co= tempvec; dx= dxt; dy= dyt;
				co[0]= shi->strandco;
				co[1]= co[2]= 0.0f;
				dx[0]= shi->dxstrand;
				dx[1]= dx[2]= 0.0f;
				dy[0]= shi->dystrand;
				dy[1]= dy[2]= 0.0f;
			}
			else if(mtex->texco==TEXCO_STRESS) {
				co= tempvec; dx= dxt; dy= dyt;
				co[0]= shi->stress;
				co[1]= co[2]= 0.0f;
				dx[0]= 0.0f;
				dx[1]= dx[2]= 0.0f;
				dy[0]= 0.0f;
				dy[1]= dy[2]= 0.0f;
			}
			else continue;	// can happen when texco defines disappear and it renders old files

			/* de pointer defines if bumping happens */
			if(mtex->mapto & (MAP_NORM|MAP_DISPLACE|MAP_WARP)) {
				texres.nor= norvec;
				norvec[0]= norvec[1]= norvec[2]= 0.0;
			}
			else texres.nor= NULL;
			
			if(warpdone) {
				VECADD(tempvec, co, warpvec);
				co= tempvec;
			}
			
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
					else dxt[0]= dyt[0]= 0.0f;
					
					if(mtex->projy) {
						dxt[1]= dx[mtex->projy-1];
						dyt[1]= dy[mtex->projy-1];
					}
					else dxt[1]= dyt[1]= 0.0f;
					if(mtex->projz) {
						dxt[2]= dx[mtex->projz-1];
						dyt[2]= dy[mtex->projz-1];
					}
					else dxt[2]= dyt[2]= 0.0;
				}
				
				do_2d_mapping(mtex, texvec, shi->vlr, shi->facenor, dxt, dyt);

				/* translate and scale */
				texvec[0]= mtex->size[0]*(texvec[0]-0.5) +mtex->ofs[0]+0.5;
				texvec[1]= mtex->size[1]*(texvec[1]-0.5) +mtex->ofs[1]+0.5;
				if(shi->osatex) {
					dxt[0]= mtex->size[0]*dxt[0];
					dxt[1]= mtex->size[1]*dxt[1];
					dyt[0]= mtex->size[0]*dyt[0];
					dyt[1]= mtex->size[1]*dyt[1];
				}
				
				/* problem: repeat-mirror is not a 'repeat' but 'extend' in imagetexture.c */
				if(tex->flag & TEX_REPEAT_XMIR) {
					if(texvec[0]>1.0f) texvec[0] -= (int)(texvec[0]);
					else if(texvec[0]<0.0f) texvec[0]+= 1-(int)(texvec[0]);
				}
				if(tex->flag & TEX_REPEAT_YMIR) {
					if(texvec[1]>1.0f) texvec[1] -= (int)(texvec[1]);
					else if(texvec[1]<0.0f) texvec[1]+= 1-(int)(texvec[1]);
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
					if(mtex->projz) {
						dxt[2]= mtex->size[2]*dx[mtex->projz-1];
						dyt[2]= mtex->size[2]*dy[mtex->projz-1];
					}
					else dxt[2]= 0.0;
				}
			}

			rgbnor= multitex(tex, texvec, dxt, dyt, shi->osatex, &texres);

			/* texture output */

			if( (rgbnor & TEX_RGB) && (mtex->texflag & MTEX_RGBTOINT)) {
				texres.tin= (0.35*texres.tr+0.45*texres.tg+0.2*texres.tb);
				rgbnor-= TEX_RGB;
			}
			if(mtex->texflag & MTEX_NEGATIVE) {
				if(rgbnor & TEX_RGB) {
					texres.tr= 1.0-texres.tr;
					texres.tg= 1.0-texres.tg;
					texres.tb= 1.0-texres.tb;
				}
				texres.tin= 1.0-texres.tin;
			}
			if(mtex->texflag & MTEX_STENCIL) {
				if(rgbnor & TEX_RGB) {
					fact= texres.ta;
					texres.ta*= stencilTin;
					stencilTin*= fact;
				}
				else {
					fact= texres.tin;
					texres.tin*= stencilTin;
					stencilTin*= fact;
				}
			}
			else {
				Tnor*= stencilTin;
			}
			
			if(texres.nor) {
				if((rgbnor & TEX_NOR)==0) {
					/* make our own normal */
					if(rgbnor & TEX_RGB) {
						texres.nor[0]= texres.tr;
						texres.nor[1]= texres.tg;
						texres.nor[2]= texres.tb;
					}
					else {
						float co= 0.5*cos(texres.tin-0.5);
						float si= 0.5*sin(texres.tin-0.5);
						float f1, f2;

						f1= shi->vn[0];
						f2= shi->vn[1];
						texres.nor[0]= f1*co+f2*si;
						texres.nor[1]= f2*co-f1*si;
						f1= shi->vn[1];
						f2= shi->vn[2];
						texres.nor[1]= f1*co+f2*si;
						texres.nor[2]= f2*co-f1*si;
					}
				}
				// warping, local space
				if(mtex->mapto & MAP_WARP) {
					warpvec[0]= mtex->warpfac*texres.nor[0];
					warpvec[1]= mtex->warpfac*texres.nor[1];
					warpvec[2]= mtex->warpfac*texres.nor[2];
					warpdone= 1;
				}
#if 0				
				if(mtex->texflag & MTEX_VIEWSPACE) {
					// rotate to global coords
					if(mtex->texco==TEXCO_ORCO || mtex->texco==TEXCO_UV) {
						if(shi->vlr && shi->obr->ob) {
							float len= Normalize(texres.nor);
							// can be optimized... (ton)
							Mat4Mul3Vecfl(shi->obr->ob->obmat, texres.nor);
							Mat4Mul3Vecfl(R.viewmat, texres.nor);
							Normalize(texres.nor);
							VecMulf(texres.nor, len);
						}
					}
				}
#endif				
			}

			/* mapping */
			if(mtex->mapto & (MAP_COL+MAP_COLSPEC+MAP_COLMIR)) {
				float tcol[3], colfac;
				
				/* stencil maps on the texture control slider, not texture intensity value */
				colfac= mtex->colfac*stencilTin;
				
				tcol[0]=texres.tr; tcol[1]=texres.tg; tcol[2]=texres.tb;
				
				if((rgbnor & TEX_RGB)==0) {
					tcol[0]= mtex->r;
					tcol[1]= mtex->g;
					tcol[2]= mtex->b;
				}
				else if(mtex->mapto & MAP_ALPHA) {
					texres.tin= stencilTin;
				}
				else texres.tin= texres.ta;
				
				if(mtex->mapto & MAP_COL) {
					texture_rgb_blend(&shi->r, tcol, &shi->r, texres.tin, colfac, mtex->blendtype);
				}
				if(mtex->mapto & MAP_COLSPEC) {
					texture_rgb_blend(&shi->specr, tcol, &shi->specr, texres.tin, colfac, mtex->blendtype);
				}
				if(mtex->mapto & MAP_COLMIR) {
					// exception for envmap only
					if(tex->type==TEX_ENVMAP && mtex->blendtype==MTEX_BLEND) {
						fact= texres.tin*colfac;
						facm= 1.0- fact;
						shi->refcol[0]= fact + facm*shi->refcol[0];
						shi->refcol[1]= fact*tcol[0] + facm*shi->refcol[1];
						shi->refcol[2]= fact*tcol[1] + facm*shi->refcol[2];
						shi->refcol[3]= fact*tcol[2] + facm*shi->refcol[3];
					}
					else {
						texture_rgb_blend(&shi->mirr, tcol, &shi->mirr, texres.tin, colfac, mtex->blendtype);
					}
				}
			}
			if( (mtex->mapto & MAP_NORM) ) {
				if(texres.nor) {
					if(mtex->maptoneg & MAP_NORM) tex->norfac= -mtex->norfac;
					else tex->norfac= mtex->norfac;
					
					/* we need to code blending modes for normals too once.. now 1 exception hardcoded */
					
					if ((tex->type==TEX_IMAGE) && (tex->imaflag & TEX_NORMALMAP)) {
						/* qdn: for normalmaps, to invert the normalmap vector,
						   it is better to negate x & y instead of subtracting the vector as was done before */
						tex->norfac = mtex->norfac;
						if (mtex->maptoneg & MAP_NORM) {
							texres.nor[0] = -texres.nor[0];
							texres.nor[1] = -texres.nor[1];
						}
						fact = Tnor*tex->norfac;
						if (fact>1.f) fact = 1.f;
						facm = 1.f-fact;
						if(mtex->normapspace == MTEX_NSPACE_TANGENT) {
							/* qdn: tangent space */
							float B[3], tv[3];
							Crossf(B, shi->vn, shi->nmaptang);	/* bitangent */
							/* transform norvec from tangent space to object surface in camera space */
							tv[0] = texres.nor[0]*shi->nmaptang[0] + texres.nor[1]*B[0] + texres.nor[2]*shi->vn[0];
							tv[1] = texres.nor[0]*shi->nmaptang[1] + texres.nor[1]*B[1] + texres.nor[2]*shi->vn[1];
							tv[2] = texres.nor[0]*shi->nmaptang[2] + texres.nor[1]*B[2] + texres.nor[2]*shi->vn[2];
							shi->vn[0]= facm*shi->vn[0] + fact*tv[0];
							shi->vn[1]= facm*shi->vn[1] + fact*tv[1];
							shi->vn[2]= facm*shi->vn[2] + fact*tv[2];
						}
						else {
							float nor[3];

							VECCOPY(nor, texres.nor);

							if(mtex->normapspace == MTEX_NSPACE_CAMERA);
							else if(mtex->normapspace == MTEX_NSPACE_WORLD) {
								Mat4Mul3Vecfl(R.viewmat, nor);
							}
							else if(mtex->normapspace == MTEX_NSPACE_OBJECT) {
								if(shi->obr && shi->obr->ob)
									Mat4Mul3Vecfl(shi->obr->ob->obmat, nor);
								Mat4Mul3Vecfl(R.viewmat, nor);
							}

							Normalize(nor);

							/* qdn: worldspace */
							shi->vn[0]= facm*shi->vn[0] + fact*nor[0];
							shi->vn[1]= facm*shi->vn[1] + fact*nor[1];
							shi->vn[2]= facm*shi->vn[2] + fact*nor[2];
						}
					}
					else {
						float nor[3], dot;

						if(shi->mat->mode & MA_TANGENT_V) {
							shi->tang[0]+= Tnor*tex->norfac*texres.nor[0];
							shi->tang[1]+= Tnor*tex->norfac*texres.nor[1];
							shi->tang[2]+= Tnor*tex->norfac*texres.nor[2];
						}

						/* prevent bump to become negative normal */
						nor[0]= Tnor*tex->norfac*texres.nor[0];
						nor[1]= Tnor*tex->norfac*texres.nor[1];
						nor[2]= Tnor*tex->norfac*texres.nor[2];
						
						dot= 0.5f + 0.5f*INPR(nor, shi->vn);
						
						shi->vn[0]+= dot*nor[0];
						shi->vn[1]+= dot*nor[1];
						shi->vn[2]+= dot*nor[2];
					}					
					Normalize(shi->vn);
					
					/* this makes sure the bump is passed on to the next texture */
					shi->orn[0]= -shi->vn[0];
					shi->orn[1]= -shi->vn[1];
					shi->orn[2]= -shi->vn[2];
					
					/* reflection vector */
					calc_R_ref(shi);
				}
			}

			if( mtex->mapto & MAP_DISPLACE ) {
				/* Now that most textures offer both Nor and Intensity, allow  */
				/* both to work, and let user select with slider.   */
				if(texres.nor) {
					if(mtex->maptoneg & MAP_DISPLACE) tex->norfac= -mtex->norfac;
					else tex->norfac= mtex->norfac;

					shi->displace[0]+= 0.2f*Tnor*tex->norfac*texres.nor[0];
					shi->displace[1]+= 0.2f*Tnor*tex->norfac*texres.nor[1];
					shi->displace[2]+= 0.2f*Tnor*tex->norfac*texres.nor[2];
				}
				
				if(rgbnor & TEX_RGB) {
					if(texres.talpha) texres.tin= texres.ta;
					else texres.tin= (0.35f*texres.tr+0.45f*texres.tg+0.2f*texres.tb);
				}

				if(mtex->maptoneg & MAP_DISPLACE) {
					factt= (texres.tin-0.5f)*mtex->dispfac*stencilTin; facmm= 1.0f-factt;
				}
				else {
					factt= (0.5f-texres.tin)*mtex->dispfac*stencilTin; facmm= 1.0f-factt;
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
				/* stencil maps on the texture control slider, not texture intensity value */
				float varfac= mtex->varfac*stencilTin;
				
				if(rgbnor & TEX_RGB) {
					if(texres.talpha) texres.tin= texres.ta;
					else texres.tin= (0.35*texres.tr+0.45*texres.tg+0.2*texres.tb);
				}

				if(mtex->mapto & MAP_REF) {
					int flip= mtex->maptoneg & MAP_REF;

					shi->refl= texture_value_blend(mtex->def_var, shi->refl, texres.tin, varfac, mtex->blendtype, flip);
					if(shi->refl<0.0) shi->refl= 0.0;
				}
				if(mtex->mapto & MAP_SPEC) {
					int flip= mtex->maptoneg & MAP_SPEC;
					
					shi->spec= texture_value_blend(mtex->def_var, shi->spec, texres.tin, varfac, mtex->blendtype, flip);
					if(shi->spec<0.0) shi->spec= 0.0;
				}
				if(mtex->mapto & MAP_EMIT) {
					int flip= mtex->maptoneg & MAP_EMIT;

					shi->emit= texture_value_blend(mtex->def_var, shi->emit, texres.tin, varfac, mtex->blendtype, flip);
					if(shi->emit<0.0) shi->emit= 0.0;
				}
				if(mtex->mapto & MAP_ALPHA) {
					int flip= mtex->maptoneg & MAP_ALPHA;

					shi->alpha= texture_value_blend(mtex->def_var, shi->alpha, texres.tin, varfac, mtex->blendtype, flip);
					if(shi->alpha<0.0) shi->alpha= 0.0;
					else if(shi->alpha>1.0) shi->alpha= 1.0;
				}
				if(mtex->mapto & MAP_HAR) {
					int flip= mtex->maptoneg & MAP_HAR;
					float har;  // have to map to 0-1
					
					har= ((float)shi->har)/128.0;
					har= 128.0*texture_value_blend(mtex->def_var, har, texres.tin, varfac, mtex->blendtype, flip);
					
					if(har<1.0) shi->har= 1; 
					else if(har>511.0) shi->har= 511;
					else shi->har= (int)har;
				}
				if(mtex->mapto & MAP_RAYMIRR) {
					int flip= mtex->maptoneg & MAP_RAYMIRR;

					shi->ray_mirror= texture_value_blend(mtex->def_var, shi->ray_mirror, texres.tin, varfac, mtex->blendtype, flip);
					if(shi->ray_mirror<0.0) shi->ray_mirror= 0.0;
					else if(shi->ray_mirror>1.0) shi->ray_mirror= 1.0;
				}
				if(mtex->mapto & MAP_TRANSLU) {
					int flip= mtex->maptoneg & MAP_TRANSLU;

					shi->translucency= texture_value_blend(mtex->def_var, shi->translucency, texres.tin, varfac, mtex->blendtype, flip);
					if(shi->translucency<0.0) shi->translucency= 0.0;
					else if(shi->translucency>1.0) shi->translucency= 1.0;
				}
				if(mtex->mapto & MAP_LAYER) {
					int flip= mtex->maptoneg & MAP_LAYER;
					
					shi->layerfac= texture_value_blend(mtex->def_var, shi->layerfac, texres.tin, varfac, mtex->blendtype, flip);
					if(shi->layerfac<0.0) shi->layerfac= 0.0;
					else if(shi->layerfac>1.0) shi->layerfac= 1.0;
				}
				if(mtex->mapto & MAP_AMB) {
					int flip= mtex->maptoneg & MAP_AMB;

					shi->amb= texture_value_blend(mtex->def_var, shi->amb, texres.tin, varfac, mtex->blendtype, flip);
					if(shi->amb<0.0) shi->amb= 0.0;
					else if(shi->amb>1.0) shi->amb= 1.0;
				}
			}
		}
	}
}

/* ------------------------------------------------------------------------- */

void do_halo_tex(HaloRen *har, float xn, float yn, float *colf)
{
	MTex *mtex;
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float texvec[3], dxt[3], dyt[3], fact, facm, dx;
	int rgb, osatex;

	if (R.r.scemode & R_NO_TEX) return;
	
	mtex= har->mat->mtex[0];
	if(mtex->tex==NULL) return;
	
	/* no normal mapping */
	texres.nor= NULL;
		
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
		else dxt[0]= dyt[0]= 0.0;
		
		if(mtex->projy) {
			dxt[1]= mtex->size[1]*dx;
			dyt[1]= mtex->size[1]*dx;
		}
		else dxt[1]= dyt[1]= 0.0;
		
		if(mtex->projz) {
			dxt[2]= 0.0;
			dyt[2]= 0.0;
		}
		else dxt[2]= dyt[2]= 0.0;

	}

	if(mtex->tex->type==TEX_IMAGE) do_2d_mapping(mtex, texvec, NULL, NULL, dxt, dyt);
	
	rgb= multitex(mtex->tex, texvec, dxt, dyt, osatex, &texres);

	/* texture output */
	if(rgb && (mtex->texflag & MTEX_RGBTOINT)) {
		texres.tin= (0.35*texres.tr+0.45*texres.tg+0.2*texres.tb);
		rgb= 0;
	}
	if(mtex->texflag & MTEX_NEGATIVE) {
		if(rgb) {
			texres.tr= 1.0-texres.tr;
			texres.tg= 1.0-texres.tg;
			texres.tb= 1.0-texres.tb;
		}
		else texres.tin= 1.0-texres.tin;
	}

	/* mapping */
	if(mtex->mapto & MAP_COL) {
		
		if(rgb==0) {
			texres.tr= mtex->r;
			texres.tg= mtex->g;
			texres.tb= mtex->b;
		}
		else if(mtex->mapto & MAP_ALPHA) {
			texres.tin= 1.0;
		}
		else texres.tin= texres.ta;

		fact= texres.tin*mtex->colfac;
		facm= 1.0-fact;
		
		if(mtex->blendtype==MTEX_MUL) {
			facm= 1.0-mtex->colfac;
		}
		
		if(mtex->blendtype==MTEX_SUB) fact= -fact;

		if(mtex->blendtype==MTEX_BLEND) {
			colf[0]= (fact*texres.tr + facm*har->r);
			colf[1]= (fact*texres.tg + facm*har->g);
			colf[2]= (fact*texres.tb + facm*har->b);
		}
		else if(mtex->blendtype==MTEX_MUL) {
			colf[0]= (facm+fact*texres.tr)*har->r;
			colf[1]= (facm+fact*texres.tg)*har->g;
			colf[2]= (facm+fact*texres.tb)*har->b;
		}
		else {
			colf[0]= (fact*texres.tr + har->r);
			colf[1]= (fact*texres.tg + har->g);
			colf[2]= (fact*texres.tb + har->b);
			
			CLAMP(colf[0], 0.0, 1.0);
			CLAMP(colf[1], 0.0, 1.0);
			CLAMP(colf[2], 0.0, 1.0);
		}
	}
	if(mtex->mapto & MAP_ALPHA) {
		if(rgb) {
			if(texres.talpha) texres.tin= texres.ta;
			else texres.tin= (0.35*texres.tr+0.45*texres.tg+0.2*texres.tb);
		}
				
		colf[3]*= texres.tin;
	}
}

/* ------------------------------------------------------------------------- */

/* hor and zen are RGB vectors, blend is 1 float, should all be initialized */
void do_sky_tex(float *rco, float *lo, float *dxyview, float *hor, float *zen, float *blend, int skyflag)
{
	MTex *mtex;
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float *co, fact, stencilTin=1.0;
	float tempvec[3], texvec[3], dxt[3], dyt[3];
	int tex_nr, rgb= 0, ok;
	
	if (R.r.scemode & R_NO_TEX) return;
	/* todo: add flag to test if there's a tex */
	texres.nor= NULL;
	
	for(tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
		if(R.wrld.mtex[tex_nr]) {
			mtex= R.wrld.mtex[tex_nr];
			
			if(mtex->tex==0) continue;
			/* if(mtex->mapto==0) continue; */
			
			/* which coords */
			co= lo;
			
			/* dxt dyt just from 1 value */
			if(dxyview) {
				dxt[0]= dxt[1]= dxt[2]= dxyview[0];
				dyt[0]= dyt[1]= dyt[2]= dxyview[1];
			}
			else {
				dxt[0]= dxt[1]= dxt[2]= 0.0;
				dyt[0]= dyt[1]= dyt[2]= 0.0;
			}
			
			/* Grab the mapping settings for this texture */
			switch(mtex->texco) {
			case TEXCO_ANGMAP:
				/* only works with texture being "real" */
				fact= (1.0/M_PI)*acos(lo[2])/(sqrt(lo[0]*lo[0] + lo[1]*lo[1])); 
				tempvec[0]= lo[0]*fact;
				tempvec[1]= lo[1]*fact;
				tempvec[2]= 0.0;
				co= tempvec;
				break;
				
			case TEXCO_H_SPHEREMAP:
			case TEXCO_H_TUBEMAP:
				if(skyflag & WO_ZENUP) {
					if(mtex->texco==TEXCO_H_TUBEMAP) tubemap(lo[0], lo[2], lo[1], tempvec, tempvec+1);
					else spheremap(lo[0], lo[2], lo[1], tempvec, tempvec+1);
					/* tube/spheremap maps for outside view, not inside */
					tempvec[0]= 1.0-tempvec[0];
					/* only top half */
					tempvec[1]= 2.0*tempvec[1]-1.0;
					tempvec[2]= 0.0;
					/* and correction for do_2d_mapping */
					tempvec[0]= 2.0*tempvec[0]-1.0;
					tempvec[1]= 2.0*tempvec[1]-1.0;
					co= tempvec;
				}
				else {
					/* potentially dangerous... check with multitex! */
					continue;
				}
				break;
			case TEXCO_OBJECT:
				if(mtex->object) {
					VECCOPY(tempvec, lo);
					MTC_Mat4MulVecfl(mtex->object->imat, tempvec);
					co= tempvec;
				}
				break;
				
			case TEXCO_GLOB:
				if(rco) {
					VECCOPY(tempvec, rco);
					MTC_Mat4MulVecfl(R.viewinv, tempvec);
					co= tempvec;
				}
				else
					co= lo;
				
//				VECCOPY(shi->dxgl, shi->dxco);
//				MTC_Mat3MulVecfl(R.imat, shi->dxco);
//				VECCOPY(shi->dygl, shi->dyco);
//				MTC_Mat3MulVecfl(R.imat, shi->dyco);
				break;
			}
			
			/* placement */			
			if(mtex->projx) texvec[0]= mtex->size[0]*(co[mtex->projx-1]+mtex->ofs[0]);
			else texvec[0]= mtex->size[0]*(mtex->ofs[0]);
			
			if(mtex->projy) texvec[1]= mtex->size[1]*(co[mtex->projy-1]+mtex->ofs[1]);
			else texvec[1]= mtex->size[1]*(mtex->ofs[1]);
			
			if(mtex->projz) texvec[2]= mtex->size[2]*(co[mtex->projz-1]+mtex->ofs[2]);
			else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
			
			/* texture */
			if(mtex->tex->type==TEX_IMAGE) do_2d_mapping(mtex, texvec, NULL, NULL, dxt, dyt);
		
			rgb= multitex(mtex->tex, texvec, dxt, dyt, R.osa, &texres);
			
			/* texture output */
			if(rgb && (mtex->texflag & MTEX_RGBTOINT)) {
				texres.tin= (0.35*texres.tr+0.45*texres.tg+0.2*texres.tb);
				rgb= 0;
			}
			if(mtex->texflag & MTEX_NEGATIVE) {
				if(rgb) {
					texres.tr= 1.0-texres.tr;
					texres.tg= 1.0-texres.tg;
					texres.tb= 1.0-texres.tb;
				}
				else texres.tin= 1.0-texres.tin;
			}
			if(mtex->texflag & MTEX_STENCIL) {
				if(rgb) {
					fact= texres.ta;
					texres.ta*= stencilTin;
					stencilTin*= fact;
				}
				else {
					fact= texres.tin;
					texres.tin*= stencilTin;
					stencilTin*= fact;
				}
			}
			else {
				if(rgb) texres.ta *= stencilTin;
				else texres.tin*= stencilTin;
			}
			
			/* color mapping */
			if(mtex->mapto & (WOMAP_HORIZ+WOMAP_ZENUP+WOMAP_ZENDOWN)) {
				float tcol[3];
				
				if(rgb==0) {
					texres.tr= mtex->r;
					texres.tg= mtex->g;
					texres.tb= mtex->b;
				}
				else texres.tin= texres.ta;
				
				tcol[0]= texres.tr; tcol[1]= texres.tg; tcol[2]= texres.tb;

				if(mtex->mapto & WOMAP_HORIZ) {
					texture_rgb_blend(hor, tcol, hor, texres.tin, mtex->colfac, mtex->blendtype);
				}
				if(mtex->mapto & (WOMAP_ZENUP+WOMAP_ZENDOWN)) {
					ok= 0;
					if(R.wrld.skytype & WO_SKYREAL) {
						if((skyflag & WO_ZENUP)) {
							if(mtex->mapto & WOMAP_ZENUP) ok= 1;
						}
						else if(mtex->mapto & WOMAP_ZENDOWN) ok= 1;
					}
					else ok= 1;
					
					if(ok) {
						texture_rgb_blend(zen, tcol, zen, texres.tin, mtex->colfac, mtex->blendtype);
					}
				}
			}
			if(mtex->mapto & WOMAP_BLEND) {
				if(rgb) texres.tin= (0.35*texres.tr+0.45*texres.tg+0.2*texres.tb);
				
				*blend= texture_value_blend(mtex->def_var, *blend, texres.tin, mtex->varfac, mtex->blendtype, 0);
			}
		}
	}
}

/* ------------------------------------------------------------------------- */
/* colf supposed to be initialized with la->r,g,b */

void do_lamp_tex(LampRen *la, float *lavec, ShadeInput *shi, float *colf)
{
	Object *ob;
	MTex *mtex;
	Tex *tex;
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float *co = NULL, *dx = NULL, *dy = NULL, fact, stencilTin=1.0;
	float texvec[3], dxt[3], dyt[3], tempvec[3];
	int tex_nr, rgb= 0;
	
	if (R.r.scemode & R_NO_TEX) return;
	tex_nr= 0;
	
	for(; tex_nr<MAX_MTEX; tex_nr++) {
		
		if(la->mtex[tex_nr]) {
			mtex= la->mtex[tex_nr];
			
			tex= mtex->tex;
			if(tex==NULL) continue;
			texres.nor= NULL;
			
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
						VECCOPY(dxt, shi->dxco);
						VECCOPY(dyt, shi->dyco);
						MTC_Mat4Mul3Vecfl(ob->imat, dxt);
						MTC_Mat4Mul3Vecfl(ob->imat, dyt);
					}
				}
				else {
					co= shi->co;
					dx= shi->dxco; dy= shi->dyco;
				}
			}
			else if(mtex->texco==TEXCO_GLOB) {
				co= shi->gl; dx= shi->dxco; dy= shi->dyco;
				VECCOPY(shi->gl, shi->co);
				MTC_Mat4MulVecfl(R.viewinv, shi->gl);
			}
			else if(mtex->texco==TEXCO_VIEW) {
				
				VECCOPY(tempvec, lavec);
				MTC_Mat3MulVecfl(la->imat, tempvec);
				
				if(la->type==LA_SPOT) {
					tempvec[0]*= la->spottexfac;
					tempvec[1]*= la->spottexfac;
				}
				co= tempvec; 
				
				dx= dxt; dy= dyt;	
				if(shi->osatex) {
					VECCOPY(dxt, shi->dxlv);
					VECCOPY(dyt, shi->dylv);
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
				do_2d_mapping(mtex, texvec, NULL, NULL, dxt, dyt);
			}
			
			rgb= multitex(tex, texvec, dxt, dyt, shi->osatex, &texres);

			/* texture output */
			if(rgb && (mtex->texflag & MTEX_RGBTOINT)) {
				texres.tin= (0.35*texres.tr+0.45*texres.tg+0.2*texres.tb);
				rgb= 0;
			}
			if(mtex->texflag & MTEX_NEGATIVE) {
				if(rgb) {
					texres.tr= 1.0-texres.tr;
					texres.tg= 1.0-texres.tg;
					texres.tb= 1.0-texres.tb;
				}
				else texres.tin= 1.0-texres.tin;
			}
			if(mtex->texflag & MTEX_STENCIL) {
				if(rgb) {
					fact= texres.ta;
					texres.ta*= stencilTin;
					stencilTin*= fact;
				}
				else {
					fact= texres.tin;
					texres.tin*= stencilTin;
					stencilTin*= fact;
				}
			}
			else {
				if(rgb) texres.ta*= stencilTin;
				else texres.tin*= stencilTin;
			}
			
			/* mapping */
			if(mtex->mapto & LAMAP_COL) {
				float col[3];
				
				if(rgb==0) {
					texres.tr= mtex->r;
					texres.tg= mtex->g;
					texres.tb= mtex->b;
				}
				else if(mtex->mapto & MAP_ALPHA) {
					texres.tin= stencilTin;
				}
				else texres.tin= texres.ta;

				/* lamp colors were premultiplied with this */
				col[0]= texres.tr*la->energy;
				col[1]= texres.tg*la->energy;
				col[2]= texres.tb*la->energy;
				
				texture_rgb_blend(colf, col, colf, texres.tin, mtex->colfac, mtex->blendtype);
			}
		}
	}
}

/* ------------------------------------------------------------------------- */

int externtex(MTex *mtex, float *vec, float *tin, float *tr, float *tg, float *tb, float *ta)
{
	Tex *tex;
	TexResult texr;
	float dxt[3], dyt[3], texvec[3];
	int rgb;
	
	tex= mtex->tex;
	if(tex==NULL) return 0;
	texr.nor= NULL;
	
	/* placement */
	if(mtex->projx) texvec[0]= mtex->size[0]*(vec[mtex->projx-1]+mtex->ofs[0]);
	else texvec[0]= mtex->size[0]*(mtex->ofs[0]);
	
	if(mtex->projy) texvec[1]= mtex->size[1]*(vec[mtex->projy-1]+mtex->ofs[1]);
	else texvec[1]= mtex->size[1]*(mtex->ofs[1]);
	
	if(mtex->projz) texvec[2]= mtex->size[2]*(vec[mtex->projz-1]+mtex->ofs[2]);
	else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
	
	/* texture */
	if(tex->type==TEX_IMAGE) {
		do_2d_mapping(mtex, texvec, NULL, NULL, dxt, dyt);
	}
	
	rgb= multitex(tex, texvec, dxt, dyt, 0, &texr);
	
	if(rgb) {
		texr.tin= (0.35*texr.tr+0.45*texr.tg+0.2*texr.tb);
	}
	else {
		texr.tr= mtex->r;
		texr.tg= mtex->g;
		texr.tb= mtex->b;
	}
	
	*tin= texr.tin;
	*tr= texr.tr;
	*tg= texr.tg;
	*tb= texr.tb;
	*ta= texr.ta;

	return (rgb != 0);
}


/* ------------------------------------------------------------------------- */

void render_realtime_texture(ShadeInput *shi, Image *ima)
{
	TexResult texr;
	static Tex imatex[BLENDER_MAX_THREADS];	// threadsafe
	static int firsttime= 1;
	Tex *tex;
	float texvec[3], dx[2], dy[2];
	ShadeInputUV *suv= &shi->uv[shi->actuv];
	int a;

	if(R.r.scemode & R_NO_TEX) return;

	if(firsttime) {
		BLI_lock_thread(LOCK_IMAGE);
		if(firsttime) {
			for(a=0; a<BLENDER_MAX_THREADS; a++) {
				memset(&imatex[a], 0, sizeof(Tex));
				default_tex(&imatex[a]);
				imatex[a].type= TEX_IMAGE;
			}

			firsttime= 0;
		}
		BLI_unlock_thread(LOCK_IMAGE);
	}
	
	tex= &imatex[shi->thread];
	tex->iuser.ok= ima->ok;
	
	texvec[0]= 0.5+0.5*suv->uv[0];
	texvec[1]= 0.5+0.5*suv->uv[1];
	texvec[2] = 0;  // initalize it because imagewrap looks at it.
	if(shi->osatex) {
		dx[0]= 0.5*suv->dxuv[0];
		dx[1]= 0.5*suv->dxuv[1];
		dy[0]= 0.5*suv->dyuv[0];
		dy[1]= 0.5*suv->dyuv[1];
	}
	
	texr.nor= NULL;
	
	if(shi->osatex) imagewraposa(tex, ima, NULL, texvec, dx, dy, &texr); 
	else imagewrap(tex, ima, NULL, texvec, &texr); 

	shi->vcol[0]*= texr.tr;
	shi->vcol[1]*= texr.tg;
	shi->vcol[2]*= texr.tb;
	shi->vcol[3]*= texr.ta;
}

/* eof */
