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
 * Contributor(s): 2004-2006, Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/render_texture.c
 *  \ingroup render
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_texture_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_material_types.h"
#include "DNA_image_types.h"
#include "DNA_node_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BKE_colortools.h"
#include "BKE_image.h"
#include "BKE_node.h"
#include "BKE_plugin_types.h"

#include "BKE_animsys.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_scene.h"

#include "BKE_library.h"
#include "BKE_image.h"
#include "BKE_texture.h"
#include "BKE_key.h"
#include "BKE_ipo.h"

#include "MEM_guardedalloc.h"

#include "envmap.h"
#include "pointdensity.h"
#include "voxeldata.h"
#include "renderpipeline.h"
#include "render_types.h"
#include "rendercore.h"
#include "shading.h"
#include "texture.h"
#include "texture_ocean.h"

#include "renderdatabase.h" /* needed for UV */

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */




static void init_render_texture(Render *re, Tex *tex)
{
	int cfra= re->scene->r.cfra;
	
	if (re) cfra= re->r.cfra;
	
	/* imap test */
	if (tex->ima && ELEM(tex->ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
		BKE_image_user_calc_frame(&tex->iuser, cfra, re?re->flag & R_SEC_FIELD:0);
	}
	
	if (tex->type==TEX_PLUGIN) {
		if (tex->plugin && tex->plugin->doit) {
			if (tex->plugin->cfra) {
				*(tex->plugin->cfra)= (float)cfra; //BKE_curframe(re->scene); // XXX old animsys - timing stuff to be fixed 
			}
		}
	}
	else if (tex->type==TEX_ENVMAP) {
		/* just in case */
		tex->imaflag |= TEX_INTERPOL | TEX_MIPMAP;
		tex->extend= TEX_CLIP;
		
		if (tex->env) {
			if (tex->env->type==ENV_PLANE)
				tex->extend= TEX_EXTEND;
			
			/* only free envmap when rendermode was set to render envmaps, for previewrender */
			if (G.rendering && re) {
				if (re->r.mode & R_ENVMAP)
					if (tex->env->stype==ENV_ANIM)
						BKE_free_envmapdata(tex->env);
			}
		}
	}
	
	if (tex->nodetree && tex->use_nodes) {
		ntreeTexBeginExecTree(tex->nodetree, 1); /* has internal flag to detect it only does it once */
	}
}

/* ------------------------------------------------------------------------- */

void init_render_textures(Render *re)
{
	Tex *tex;
	
	tex= re->main->tex.first;
	while (tex) {
		if (tex->id.us) init_render_texture(re, tex);
		tex= tex->id.next;
	}
}

static void end_render_texture(Tex *tex)
{
	if (tex && tex->use_nodes && tex->nodetree && tex->nodetree->execdata)
		ntreeTexEndExecTree(tex->nodetree->execdata, 1);
}

void end_render_textures(Render *re)
{
	Tex *tex;
	for (tex= re->main->tex.first; tex; tex= tex->id.next)
		if (tex->id.us)
			end_render_texture(tex);
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
			
			texres->nor[0]= 0.3333f*(fac0 - fac1);
			texres->nor[1]= 0.3333f*(fac0 - fac2);
			texres->nor[2]= 0.3333f*(fac0 - fac3);
			
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

	if (tex->flag & TEX_FLIPBLEND) {
		x= texvec[1];
		y= texvec[0];
	}
	else {
		x= texvec[0];
		y= texvec[1];
	}

	if (tex->stype==TEX_LIN) {	/* lin */
		texres->tin= (1.0f+x)/2.0f;
	}
	else if (tex->stype==TEX_QUAD) {	/* quad */
		texres->tin= (1.0f+x)/2.0f;
		if (texres->tin<0.0f) texres->tin= 0.0f;
		else texres->tin*= texres->tin;
	}
	else if (tex->stype==TEX_EASE) {	/* ease */
		texres->tin= (1.0f+x)/2.0f;
		if (texres->tin<=0.0f) texres->tin= 0.0f;
		else if (texres->tin>=1.0f) texres->tin= 1.0f;
		else {
			t= texres->tin*texres->tin;
			texres->tin= (3.0f*t-2.0f*t*texres->tin);
		}
	}
	else if (tex->stype==TEX_DIAG) { /* diag */
		texres->tin= (2.0f+x+y)/4.0f;
	}
	else if (tex->stype==TEX_RAD) { /* radial */
		texres->tin= (atan2(y, x) / (2*M_PI) + 0.5);
	}
	else {  /* sphere TEX_SPHERE */
		texres->tin= 1.0-sqrt(x*x+	y*y+texvec[2]*texvec[2]);
		if (texres->tin<0.0f) texres->tin= 0.0f;
		if (tex->stype==TEX_HALO) texres->tin*= texres->tin;  /* halo */
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
	
	a = rmax - 2.0f*fabsf(floorf((a*(1.0f/b))+0.5f) - (a*(1.0f/b)));
	
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
		wi = waveform[wf]((x + y + z)*10.0f);
	}
	else if (wt==TEX_RING) {
		wi = waveform[wf](sqrtf(x*x + y*y + z*z)*20.0f);
	}
	else if (wt==TEX_BANDNOISE) {
		wi = tex->turbul*BLI_gNoise(tex->noisesize, x, y, z, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		wi = waveform[wf]((x + y + z)*10.0f + wi);
	}
	else if (wt==TEX_RINGNOISE) {
		wi = tex->turbul*BLI_gNoise(tex->noisesize, x, y, z, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		wi = waveform[wf](sqrtf(x*x + y*y + z*z)*20.0f + wi);
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
	
	n = 5.0f * (x + y + z);
	
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
	turb= tex->turbul/5.0f;

	x=  sin( ( texvec[0]+texvec[1]+texvec[2])*5.0f );
	y=  cos( (-texvec[0]+texvec[1]-texvec[2])*5.0f );
	z= -cos( (-texvec[0]-texvec[1]+texvec[2])*5.0f );
	if (n>0) {
		x*= turb;
		y*= turb;
		z*= turb;
		y= -cos(x-y+z);
		y*= turb;
		if (n>1) {
			x= cos(x-y-z);
			x*= turb;
			if (n>2) {
				z= sin(-x-y-z);
				z*= turb;
				if (n>3) {
					x= -cos(-x+y-z);
					x*= turb;
					if (n>4) {
						y= -sin(-x+y+z);
						y*= turb;
						if (n>5) {
							y= -cos(-x+y+z);
							y*= turb;
							if (n>6) {
								x= cos(x+y+z);
								x*= turb;
								if (n>7) {
									z= sin(x+y-z);
									z*= turb;
									if (n>8) {
										x= -cos(-x-y+z);
										x*= turb;
										if (n>9) {
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

	if (turb!=0.0f) {
		turb*= 2.0f;
		x/= turb; 
		y/= turb; 
		z/= turb;
	}
	texres->tr= 0.5f-x;
	texres->tg= 0.5f-y;
	texres->tb= 0.5f-z;

	texres->tin= 0.3333f*(texres->tr+texres->tg+texres->tb);
	
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
	
	ofs= tex->turbul/200.0f;

	if (tex->stype) ofs*=(b2*b2);
	nor[0] = BLI_gNoise(tex->noisesize, texvec[0]+ofs, texvec[1], texvec[2], (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
	nor[1] = BLI_gNoise(tex->noisesize, texvec[0], texvec[1]+ofs, texvec[2], (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);	
	nor[2] = BLI_gNoise(tex->noisesize, texvec[0], texvec[1], texvec[2]+ofs, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);

	texres->tin= nor[2];
	
	if (texres->nor) {
		
		copy_v3_v3(texres->nor, nor);
		tex_normal_derivate(tex, texres);
		
		if (tex->stype==TEX_WALLOUT) {
			texres->nor[0]= -texres->nor[0];
			texres->nor[1]= -texres->nor[1];
			texres->nor[2]= -texres->nor[2];
		}
		
		retval |= TEX_NOR;
	}
	
	if (tex->stype==TEX_WALLOUT)
		texres->tin= 1.0f-texres->tin;
	
	if (texres->tin<0.0f)
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
	float da[4], pa[12];	/* distance and point coordinate arrays of 4 nearest neighbors */
	float aw1 = fabs(tex->vn_w1);
	float aw2 = fabs(tex->vn_w2);
	float aw3 = fabs(tex->vn_w3);
	float aw4 = fabs(tex->vn_w4);
	float sc = (aw1 + aw2 + aw3 + aw4);
	if (sc!=0.f) sc =  tex->ns_outscale/sc;

	voronoi(texvec[0], texvec[1], texvec[2], da, pa, tex->vn_mexp, tex->vn_distm);
	texres->tin = sc * fabsf(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);

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
		texres->nor[0] = sc * fabsf(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);
		voronoi(texvec[0], texvec[1] + offs, texvec[2], da, pa, tex->vn_mexp,  tex->vn_distm);
		texres->nor[1] = sc * fabsf(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);
		voronoi(texvec[0], texvec[1], texvec[2] + offs, da, pa, tex->vn_mexp,  tex->vn_distm);
		texres->nor[2] = sc * fabsf(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);
		
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
	while (loop--) {
		ran= (ran>>2);
		val*= (ran & 3);
		div*= 3.0f;
	}
	
	texres->tin= ((float)val)/div;

	BRICONT;
	return TEX_INT;
}

/* ------------------------------------------------------------------------- */

static int plugintex(Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres)
{
	PluginTex *pit;
	int rgbnor=0;
	float result[8]= {0.0f};

	texres->tin= 0.0;

	pit= tex->plugin;
	if (pit && pit->doit) {
		if (texres->nor) {
			if (pit->version < 6) {
				copy_v3_v3(pit->result+5, texres->nor);
			}
			else {
				copy_v3_v3(result+5, texres->nor);
			}
		}
		if (pit->version < 6) {
			if (osatex) rgbnor= ((TexDoitold)pit->doit)(tex->stype,
				pit->data, texvec, dxt, dyt);
			else rgbnor= ((TexDoitold)pit->doit)(tex->stype, 
				pit->data, texvec, NULL, NULL);
		}
		else {
			if (osatex) rgbnor= ((TexDoit)pit->doit)(tex->stype,
				pit->data, texvec, dxt, dyt, result);
			else rgbnor= ((TexDoit)pit->doit)(tex->stype, 
				pit->data, texvec, NULL, NULL, result);
		}

		if (pit->version < 6) {
			texres->tin = pit->result[0];
		}
		else {
			texres->tin = result[0]; /* XXX, assigning garbage value, fixme! */
		}

		if (rgbnor & TEX_NOR) {
			if (texres->nor) {
				if (pit->version < 6) {
					copy_v3_v3(texres->nor, pit->result+5);
				}
				else {
					copy_v3_v3(texres->nor, result+5);
				}
			}
		}
		
		if (rgbnor & TEX_RGB) {
			if (pit->version < 6) {
				texres->tr = pit->result[1];
				texres->tg = pit->result[2];
				texres->tb = pit->result[3];
				texres->ta = pit->result[4];
			}
			else {
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
	
	if (n==NULL) {
		nor[0]= x; nor[1]= y; nor[2]= z;	// use local render coord
	}
	else {
		copy_v3_v3(nor, n);
	}
	mul_mat3_m4_v3(R.viewinv, nor);

	x1= fabs(nor[0]);
	y1= fabs(nor[1]);
	z1= fabs(nor[2]);
	
	if (z1>=x1 && z1>=y1) {
		*adr1 = (x + 1.0f) / 2.0f;
		*adr2 = (y + 1.0f) / 2.0f;
		ret= 0;
	}
	else if (y1>=x1 && y1>=z1) {
		*adr1 = (x + 1.0f) / 2.0f;
		*adr2 = (z + 1.0f) / 2.0f;
		ret= 1;
	}
	else {
		*adr1 = (y + 1.0f) / 2.0f;
		*adr2 = (z + 1.0f) / 2.0f;
		ret= 2;		
	}
	return ret;
}

/* ------------------------------------------------------------------------- */

/* mtex argument only for projection switches */
static int cubemap(MTex *mtex, VlakRen *vlr, float *n, float x, float y, float z, float *adr1, float *adr2)
{
	int proj[4]={0, ME_PROJXY, ME_PROJXZ, ME_PROJYZ}, ret= 0;
	
	if (vlr) {
		int index;
		
		/* Mesh vertices have such flags, for others we calculate it once based on orco */
		if ((vlr->puno & (ME_PROJXY|ME_PROJXZ|ME_PROJYZ))==0) {
			/* test for v1, vlr can be faked for baking */
			if (vlr->v1 && vlr->v1->orco) {
				float nor[3];
				normal_tri_v3(nor, vlr->v1->orco, vlr->v2->orco, vlr->v3->orco);
				
				if ( fabs(nor[0])<fabs(nor[2]) && fabs(nor[1])<fabs(nor[2]) ) vlr->puno |= ME_PROJXY;
				else if ( fabs(nor[0])<fabs(nor[1]) && fabs(nor[2])<fabs(nor[1]) ) vlr->puno |= ME_PROJXZ;
				else vlr->puno |= ME_PROJYZ;
			}
			else return cubemap_glob(n, x, y, z, adr1, adr2);
		}
		
		if (mtex) {
			/* the mtex->proj{xyz} have type char. maybe this should be wider? */
			/* casting to int ensures that the index type is right.            */
			index = (int) mtex->projx;
			proj[index]= ME_PROJXY;

			index = (int) mtex->projy;
			proj[index]= ME_PROJXZ;

			index = (int) mtex->projz;
			proj[index]= ME_PROJYZ;
		}
		
		if (vlr->puno & proj[1]) {
			*adr1 = (x + 1.0f) / 2.0f;
			*adr2 = (y + 1.0f) / 2.0f;
		}
		else if (vlr->puno & proj[2]) {
			*adr1 = (x + 1.0f) / 2.0f;
			*adr2 = (z + 1.0f) / 2.0f;
			ret= 1;
		}
		else {
			*adr1 = (y + 1.0f) / 2.0f;
			*adr2 = (z + 1.0f) / 2.0f;
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
	
	if (n==NULL) return 0;
	
	copy_v3_v3(nor, n);
	if (ob) mul_mat3_m4_v3(ob->imat, nor);
	
	x1= fabs(nor[0]);
	y1= fabs(nor[1]);
	z1= fabs(nor[2]);
	
	if (z1>=x1 && z1>=y1) {
		*adr1 = (x + 1.0f) / 2.0f;
		*adr2 = (y + 1.0f) / 2.0f;
		ret= 0;
	}
	else if (y1>=x1 && y1>=z1) {
		*adr1 = (x + 1.0f) / 2.0f;
		*adr2 = (z + 1.0f) / 2.0f;
		ret= 1;
	}
	else {
		*adr1 = (y + 1.0f) / 2.0f;
		*adr2 = (z + 1.0f) / 2.0f;
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

	if (R.osa==0) {
		
		if (wrap==MTEX_FLAT) {
			fx = (t[0] + 1.0f) / 2.0f;
			fy = (t[1] + 1.0f) / 2.0f;
		}
		else if (wrap==MTEX_TUBE) map_to_tube( &fx, &fy, t[0], t[1], t[2]);
		else if (wrap==MTEX_SPHERE) map_to_sphere( &fx, &fy, t[0], t[1], t[2]);
		else {
			if (texco==TEXCO_OBJECT) cubemap_ob(ob, n, t[0], t[1], t[2], &fx, &fy);
			else if (texco==TEXCO_GLOB) cubemap_glob(n, t[0], t[1], t[2], &fx, &fy);
			else cubemap(mtex, vlr, n, t[0], t[1], t[2], &fx, &fy);
		}
		
		/* repeat */
		if (tex->extend==TEX_REPEAT) {
			if (tex->xrepeat>1) {
				float origf= fx *= tex->xrepeat;
				
				if (fx>1.0f) fx -= (int)(fx);
				else if (fx<0.0f) fx+= 1-(int)(fx);
				
				if (tex->flag & TEX_REPEAT_XMIR) {
					int orig= (int)floor(origf);
					if (orig & 1)
						fx= 1.0f-fx;
				}
			}
			if (tex->yrepeat>1) {
				float origf= fy *= tex->yrepeat;
				
				if (fy>1.0f) fy -= (int)(fy);
				else if (fy<0.0f) fy+= 1-(int)(fy);
				
				if (tex->flag & TEX_REPEAT_YMIR) {
					int orig= (int)floor(origf);
					if (orig & 1)
						fy= 1.0f-fy;
				}
			}
		}
		/* crop */
		if (tex->cropxmin!=0.0f || tex->cropxmax!=1.0f) {
			fac1= tex->cropxmax - tex->cropxmin;
			fx= tex->cropxmin+ fx*fac1;
		}
		if (tex->cropymin!=0.0f || tex->cropymax!=1.0f) {
			fac1= tex->cropymax - tex->cropymin;
			fy= tex->cropymin+ fy*fac1;
		}

		t[0]= fx;
		t[1]= fy;
	}
	else {
		
		if (wrap==MTEX_FLAT) {
			fx= (t[0] + 1.0f) / 2.0f;
			fy= (t[1] + 1.0f) / 2.0f;
			dxt[0]/= 2.0f;
			dxt[1]/= 2.0f;
			dxt[2]/= 2.0f;
			dyt[0]/= 2.0f;
			dyt[1]/= 2.0f;
			dyt[2]/= 2.0f;
		}
		else if (ELEM(wrap, MTEX_TUBE, MTEX_SPHERE)) {
			/* exception: the seam behind (y<0.0) */
			ok= 1;
			if (t[1]<=0.0f) {
				fx= t[0]+dxt[0];
				fy= t[0]+dyt[0];
				if (fx>=0.0f && fy>=0.0f && t[0]>=0.0f);
				else if (fx<=0.0f && fy<=0.0f && t[0]<=0.0f);
				else ok= 0;
			}
			if (ok) {
				if (wrap==MTEX_TUBE) {
					map_to_tube( area, area+1, t[0], t[1], t[2]);
					map_to_tube( area+2, area+3, t[0]+dxt[0], t[1]+dxt[1], t[2]+dxt[2]);
					map_to_tube( area+4, area+5, t[0]+dyt[0], t[1]+dyt[1], t[2]+dyt[2]);
				}
				else { 
					map_to_sphere(area, area+1, t[0], t[1], t[2]);
					map_to_sphere( area+2, area+3, t[0]+dxt[0], t[1]+dxt[1], t[2]+dxt[2]);
					map_to_sphere( area+4, area+5, t[0]+dyt[0], t[1]+dyt[1], t[2]+dyt[2]);
				}
				areaflag= 1;
			}
			else {
				if (wrap==MTEX_TUBE) map_to_tube( &fx, &fy, t[0], t[1], t[2]);
				else map_to_sphere( &fx, &fy, t[0], t[1], t[2]);
				dxt[0]/= 2.0f;
				dxt[1]/= 2.0f;
				dyt[0]/= 2.0f;
				dyt[1]/= 2.0f;
			}
		}
		else {

			if (texco==TEXCO_OBJECT) proj = cubemap_ob(ob, n, t[0], t[1], t[2], &fx, &fy);
			else if (texco==TEXCO_GLOB) proj = cubemap_glob(n, t[0], t[1], t[2], &fx, &fy);
			else proj = cubemap(mtex, vlr, n, t[0], t[1], t[2], &fx, &fy);

			if (proj==1) {
				SWAP(float, dxt[1], dxt[2]);
				SWAP(float, dyt[1], dyt[2]);
			}
			else if (proj==2) {
				float f1= dxt[0], f2= dyt[0];
				dxt[0]= dxt[1];
				dyt[0]= dyt[1];
				dxt[1]= dxt[2];
				dyt[1]= dyt[2];
				dxt[2]= f1;
				dyt[2]= f2;
			}

			dxt[0] *= 0.5f;
			dxt[1] *= 0.5f;
			dxt[2] *= 0.5f;

			dyt[0] *= 0.5f;
			dyt[1] *= 0.5f;
			dyt[2] *= 0.5f;

		}
		
		/* if area, then reacalculate dxt[] and dyt[] */
		if (areaflag) {
			fx= area[0]; 
			fy= area[1];
			dxt[0]= area[2]-fx;
			dxt[1]= area[3]-fy;
			dyt[0]= area[4]-fx;
			dyt[1]= area[5]-fy;
		}
		
		/* repeat */
		if (tex->extend==TEX_REPEAT) {
			float max= 1.0f;
			if (tex->xrepeat>1) {
				float origf= fx *= tex->xrepeat;
				
				// TXF: omit mirror here, see comments in do_material_tex() after do_2d_mapping() call
				if (tex->texfilter == TXF_BOX) {
					if (fx>1.0f) fx -= (int)(fx);
					else if (fx<0.0f) fx+= 1-(int)(fx);
				
					if (tex->flag & TEX_REPEAT_XMIR) {
						int orig= (int)floor(origf);
						if (orig & 1)
							fx= 1.0f-fx;
					}
				}
				
				max= tex->xrepeat;
				
				dxt[0]*= tex->xrepeat;
				dyt[0]*= tex->xrepeat;
			}
			if (tex->yrepeat>1) {
				float origf= fy *= tex->yrepeat;
				
				// TXF: omit mirror here, see comments in do_material_tex() after do_2d_mapping() call
				if (tex->texfilter == TXF_BOX) {
					if (fy>1.0f) fy -= (int)(fy);
					else if (fy<0.0f) fy+= 1-(int)(fy);
				
					if (tex->flag & TEX_REPEAT_YMIR) {
						int orig= (int)floor(origf);
						if (orig & 1)
							fy= 1.0f-fy;
					}
				}
				
				if (max<tex->yrepeat)
					max= tex->yrepeat;

				dxt[1]*= tex->yrepeat;
				dyt[1]*= tex->yrepeat;
			}
			if (max!=1.0f) {
				dxt[2]*= max;
				dyt[2]*= max;
			}
			
		}
		/* crop */
		if (tex->cropxmin!=0.0f || tex->cropxmax!=1.0f) {
			fac1= tex->cropxmax - tex->cropxmin;
			fx= tex->cropxmin+ fx*fac1;
			dxt[0]*= fac1;
			dyt[0]*= fac1;
		}
		if (tex->cropymin!=0.0f || tex->cropymax!=1.0f) {
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

static int multitex(Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres, short thread, short which_output)
{
	float tmpvec[3];
	int retval=0; /* return value, int:0, col:1, nor:2, everything:3 */

	texres->talpha= 0;	/* is set when image texture returns alpha (considered premul) */
	
	if (tex->use_nodes && tex->nodetree) {
		retval = ntreeTexExecTree(tex->nodetree, texres, texvec, dxt, dyt, osatex, thread,
			tex, which_output, R.r.cfra, (R.r.scemode & R_TEXNODE_PREVIEW) != 0, NULL, NULL);
	}
	else
	switch (tex->type) {
	
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
		if (osatex) retval= imagewraposa(tex, tex->ima, NULL, texvec, dxt, dyt, texres);
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
		copy_v3_v3(tmpvec, texvec);
		mul_v3_fl(tmpvec, 1.0f/tex->noisesize);
		
		switch (tex->stype) {
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
		copy_v3_v3(tmpvec, texvec);
		mul_v3_fl(tmpvec, 1.0f/tex->noisesize);
		
		retval= voronoiTex(tex, tmpvec, texres);
		break;
	case TEX_DISTNOISE:
		/* ton: added this, for Blender convention reason.
		 * artificer: added the use of tmpvec to avoid scaling texvec
		 */
		copy_v3_v3(tmpvec, texvec);
		mul_v3_fl(tmpvec, 1.0f/tex->noisesize);
		
		retval= mg_distNoiseTex(tex, tmpvec, texres);
		break;
	case TEX_POINTDENSITY:
		retval= pointdensitytex(tex, texvec, texres);
		break;
	case TEX_VOXELDATA:
		retval= voxeldatatex(tex, texvec, texres);  
		break;
	case TEX_OCEAN:
		retval= ocean_texture(tex, texvec, texres);  
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
			retval |= TEX_RGB;
		}
	}
	return retval;
}

/* this is called from the shader and texture nodes */
int multitex_nodes(Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres, short thread, short which_output, ShadeInput *shi, MTex *mtex)
{
	if (tex==NULL) {
		memset(texres, 0, sizeof(TexResult));
		return 0;
	}

	if (mtex)
		which_output= mtex->which_output;
	
	if (tex->type==TEX_IMAGE) {
		int rgbnor;

		if (mtex) {
			/* we have mtex, use it for 2d mapping images only */
			do_2d_mapping(mtex, texvec, shi->vlr, shi->facenor, dxt, dyt);
			rgbnor= multitex(tex, texvec, dxt, dyt, osatex, texres, thread, which_output);

			if (mtex->mapto & (MAP_COL+MAP_COLSPEC+MAP_COLMIR)) {
				ImBuf *ibuf = BKE_image_get_ibuf(tex->ima, &tex->iuser);
				
				/* don't linearize float buffers, assumed to be linear */
				if (ibuf && !(ibuf->rect_float) && R.r.color_mgt_flag & R_COLOR_MANAGEMENT)
					srgb_to_linearrgb_v3_v3(&texres->tr, &texres->tr);
			}
		}
		else {
			/* we don't have mtex, do default flat 2d projection */
			MTex localmtex;
			float texvec_l[3], dxt_l[3], dyt_l[3];
			
			localmtex.mapping= MTEX_FLAT;
			localmtex.tex= tex;
			localmtex.object= NULL;
			localmtex.texco= TEXCO_ORCO;
			
			copy_v3_v3(texvec_l, texvec);
			if (dxt && dyt) {
				copy_v3_v3(dxt_l, dxt);
				copy_v3_v3(dyt_l, dyt);
			}
			else {
				zero_v3(dxt_l);
				zero_v3(dyt_l);
			}
			
			do_2d_mapping(&localmtex, texvec_l, NULL, NULL, dxt_l, dyt_l);
			rgbnor= multitex(tex, texvec_l, dxt_l, dyt_l, osatex, texres, thread, which_output);
		}

		return rgbnor;
	}
	else
		return multitex(tex, texvec, dxt, dyt, osatex, texres, thread, which_output);
}

/* this is called for surface shading */
int multitex_mtex(ShadeInput *shi, MTex *mtex, float *texvec, float *dxt, float *dyt, TexResult *texres)
{
	Tex *tex= mtex->tex;

	if (tex->use_nodes && tex->nodetree) {
		/* stupid exception here .. but we have to pass shi and mtex to
		 * textures nodes for 2d mapping and color management for images */
		return ntreeTexExecTree(tex->nodetree, texres, texvec, dxt, dyt, shi->osatex, shi->thread,
			tex, mtex->which_output, R.r.cfra, (R.r.scemode & R_TEXNODE_PREVIEW) != 0, shi, mtex);
	}
	else
		return multitex(mtex->tex, texvec, dxt, dyt, shi->osatex, texres, shi->thread, mtex->which_output);
}

/* Warning, if the texres's values are not declared zero, check the return value to be sure
 * the color values are set before using the r/g/b values, otherwise you may use uninitialized values - Campbell */
int multitex_ext(Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres)
{
	return multitex_nodes(tex, texvec, dxt, dyt, osatex, texres, 0, 0, NULL, NULL);
}

/* extern-tex doesn't support nodes (ntreeBeginExec() can't be called when rendering is going on) */
int multitex_ext_safe(Tex *tex, float *texvec, TexResult *texres)
{
	int use_nodes= tex->use_nodes, retval;
	
	tex->use_nodes= 0;
	retval= multitex_nodes(tex, texvec, NULL, NULL, 0, texres, 0, 0, NULL, NULL);
	tex->use_nodes= use_nodes;
	
	return retval;
}


/* ------------------------------------------------------------------------- */

/* in = destination, tex = texture, out = previous color */
/* fact = texture strength, facg = button strength value */
void texture_rgb_blend(float in[3], const float tex[3], const float out[3], float fact, float facg, int blendtype)
{
	float facm, col;
	
	switch (blendtype) {
	case MTEX_BLEND:
		fact*= facg;
		facm= 1.0f-fact;

		in[0]= (fact*tex[0] + facm*out[0]);
		in[1]= (fact*tex[1] + facm*out[1]);
		in[2]= (fact*tex[2] + facm*out[2]);
		break;
		
	case MTEX_MUL:
		fact*= facg;
		facm= 1.0f-facg;
		in[0]= (facm+fact*tex[0])*out[0];
		in[1]= (facm+fact*tex[1])*out[1];
		in[2]= (facm+fact*tex[2])*out[2];
		break;

	case MTEX_SCREEN:
		fact*= facg;
		facm= 1.0f-facg;
		in[0]= 1.0f - (facm+fact*(1.0f-tex[0])) * (1.0f-out[0]);
		in[1]= 1.0f - (facm+fact*(1.0f-tex[1])) * (1.0f-out[1]);
		in[2]= 1.0f - (facm+fact*(1.0f-tex[2])) * (1.0f-out[2]);
		break;

	case MTEX_OVERLAY:
		fact*= facg;
		facm= 1.0f-facg;
		
		if (out[0] < 0.5f)
			in[0] = out[0] * (facm + 2.0f*fact*tex[0]);
		else
			in[0] = 1.0f - (facm + 2.0f*fact*(1.0f - tex[0])) * (1.0f - out[0]);
		if (out[1] < 0.5f)
			in[1] = out[1] * (facm + 2.0f*fact*tex[1]);
		else
			in[1] = 1.0f - (facm + 2.0f*fact*(1.0f - tex[1])) * (1.0f - out[1]);
		if (out[2] < 0.5f)
			in[2] = out[2] * (facm + 2.0f*fact*tex[2]);
		else
			in[2] = 1.0f - (facm + 2.0f*fact*(1.0f - tex[2])) * (1.0f - out[2]);
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
		facm= 1.0f-fact;
		
		if (tex[0]!=0.0f)
			in[0]= facm*out[0] + fact*out[0]/tex[0];
		if (tex[1]!=0.0f)
			in[1]= facm*out[1] + fact*out[1]/tex[1];
		if (tex[2]!=0.0f)
			in[2]= facm*out[2] + fact*out[2]/tex[2];

		break;

	case MTEX_DIFF:
		fact*= facg;
		facm= 1.0f-fact;
		in[0]= facm*out[0] + fact*fabsf(tex[0]-out[0]);
		in[1]= facm*out[1] + fact*fabsf(tex[1]-out[1]);
		in[2]= facm*out[2] + fact*fabsf(tex[2]-out[2]);
		break;

	case MTEX_DARK:
		fact*= facg;
		facm= 1.0f-fact;
		
		col= tex[0]+((1-tex[0])*facm);
		if (col < out[0]) in[0]= col; else in[0]= out[0];
		col= tex[1]+((1-tex[1])*facm);
		if (col < out[1]) in[1]= col; else in[1]= out[1];
		col= tex[2]+((1-tex[2])*facm);
		if (col < out[2]) in[2]= col; else in[2]= out[2];
		break;

	case MTEX_LIGHT:
		fact*= facg;
		
		col= fact*tex[0];
		if (col > out[0]) in[0]= col; else in[0]= out[0];
		col= fact*tex[1];
		if (col > out[1]) in[1]= col; else in[1]= out[1];
		col= fact*tex[2];
		if (col > out[2]) in[2]= col; else in[2]= out[2];
		break;
		
	case MTEX_BLEND_HUE:
		fact*= facg;
		copy_v3_v3(in, out);
		ramp_blend(MA_RAMP_HUE, in, fact, tex);
		break;
	case MTEX_BLEND_SAT:
		fact*= facg;
		copy_v3_v3(in, out);
		ramp_blend(MA_RAMP_SAT, in, fact, tex);
		break;
	case MTEX_BLEND_VAL:
		fact*= facg;
		copy_v3_v3(in, out);
		ramp_blend(MA_RAMP_VAL, in, fact, tex);
		break;
	case MTEX_BLEND_COLOR:
		fact*= facg;
		copy_v3_v3(in, out);
		ramp_blend(MA_RAMP_COLOR, in, fact, tex);
		break;
	case MTEX_SOFT_LIGHT: 
		fact*= facg; 
		copy_v3_v3(in, out);
		ramp_blend(MA_RAMP_SOFT, in, fact, tex);
		break; 
	case MTEX_LIN_LIGHT: 
		fact*= facg; 
		copy_v3_v3(in, out);
		ramp_blend(MA_RAMP_LINEAR, in, fact, tex);
		break; 
	}
}

float texture_value_blend(float tex, float out, float fact, float facg, int blendtype)
{
	float in=0.0, facm, col, scf;
	int flip= (facg < 0.0f);

	facg= fabsf(facg);
	
	fact*= facg;
	facm= 1.0f-fact;
	if (flip) SWAP(float, fact, facm);

	switch (blendtype) {
	case MTEX_BLEND:
		in= fact*tex + facm*out;
		break;

	case MTEX_MUL:
		facm= 1.0f-facg;
		in= (facm+fact*tex)*out;
		break;

	case MTEX_SCREEN:
		facm= 1.0f-facg;
		in= 1.0f-(facm+fact*(1.0f-tex))*(1.0f-out);
		break;

	case MTEX_OVERLAY:
		facm= 1.0f-facg;
		if (out < 0.5f)
			in = out * (facm + 2.0f*fact*tex);
		else
			in = 1.0f - (facm + 2.0f*fact*(1.0f - tex)) * (1.0f - out);
		break;

	case MTEX_SUB:
		fact= -fact;
	case MTEX_ADD:
		in= fact*tex + out;
		break;

	case MTEX_DIV:
		if (tex!=0.0f)
			in= facm*out + fact*out/tex;
		break;

	case MTEX_DIFF:
		in= facm*out + fact*fabsf(tex-out);
		break;

	case MTEX_DARK:
		col= fact*tex;
		if (col < out) in= col; else in= out;
		break;

	case MTEX_LIGHT:
		col= fact*tex;
		if (col > out) in= col; else in= out;
		break;

	case MTEX_SOFT_LIGHT: 
		scf=1.0f - (1.0f - tex) * (1.0f - out);
		in= facm*out + fact * ((1.0f - out) * tex * out) + (out * scf);
		break;       

	case MTEX_LIN_LIGHT: 
		if (tex > 0.5f)
			in = out + fact*(2.0f*(tex - 0.5f));
		else 
			in = out + fact*(2.0f*tex - 1.0f);
		break;
	}
	
	return in;
}

static void texco_mapping(ShadeInput* shi, Tex* tex, MTex* mtex, float* co, float* dx, float* dy, float* texvec, float* dxt, float* dyt)
{
	// new: first swap coords, then map, then trans/scale
	if (tex->type == TEX_IMAGE) {
		// placement
		texvec[0] = mtex->projx ? co[mtex->projx - 1] : 0.f;
		texvec[1] = mtex->projy ? co[mtex->projy - 1] : 0.f;
		texvec[2] = mtex->projz ? co[mtex->projz - 1] : 0.f;

		if (shi->osatex) {
			if (mtex->projx) {
				dxt[0] = dx[mtex->projx - 1];
				dyt[0] = dy[mtex->projx - 1];
			}
			else dxt[0] = dyt[0] = 0.f;
			if (mtex->projy) {
				dxt[1] = dx[mtex->projy - 1];
				dyt[1] = dy[mtex->projy - 1];
			}
			else dxt[1] = dyt[1] = 0.f;
			if (mtex->projz) {
				dxt[2] = dx[mtex->projz - 1];
				dyt[2] = dy[mtex->projz - 1];
			}
			else dxt[2] = dyt[2] = 0.f;
		}
		do_2d_mapping(mtex, texvec, shi->vlr, shi->facenor, dxt, dyt);

		// translate and scale
		texvec[0] = mtex->size[0]*(texvec[0] - 0.5f) + mtex->ofs[0] + 0.5f;
		texvec[1] = mtex->size[1]*(texvec[1] - 0.5f) + mtex->ofs[1] + 0.5f;
		if (shi->osatex) {
			dxt[0] = mtex->size[0]*dxt[0];
			dxt[1] = mtex->size[1]*dxt[1];
			dyt[0] = mtex->size[0]*dyt[0];
			dyt[1] = mtex->size[1]*dyt[1];
		}
		
		/* problem: repeat-mirror is not a 'repeat' but 'extend' in imagetexture.c */
		// TXF: bug was here, only modify texvec when repeat mode set, old code affected other modes too.
		// New texfilters solve mirroring differently so that it also works correctly when
		// textures are scaled (sizeXYZ) as well as repeated. See also modification in do_2d_mapping().
		// (since currently only done in osa mode, results will look incorrect without osa TODO) 
		if (tex->extend == TEX_REPEAT && (tex->flag & TEX_REPEAT_XMIR)) {
			if (tex->texfilter == TXF_BOX)
				texvec[0] -= floorf(texvec[0]);	// this line equivalent to old code, same below
			else if (texvec[0] < 0.f || texvec[0] > 1.f) {
				const float tx = 0.5f*texvec[0];
				texvec[0] = 2.f*(tx - floorf(tx));
				if (texvec[0] > 1.f) texvec[0] = 2.f - texvec[0];
			}
		}
		if (tex->extend == TEX_REPEAT && (tex->flag & TEX_REPEAT_YMIR)) {
			if (tex->texfilter == TXF_BOX)
				texvec[1] -= floorf(texvec[1]);
			else if (texvec[1] < 0.f || texvec[1] > 1.f) {
				const float ty = 0.5f*texvec[1];
				texvec[1] = 2.f*(ty - floorf(ty));
				if (texvec[1] > 1.f) texvec[1] = 2.f - texvec[1];
			}
		}
		
	}
	else {	// procedural
		// placement
		texvec[0] = mtex->size[0]*(mtex->projx ? (co[mtex->projx - 1] + mtex->ofs[0]) : mtex->ofs[0]);
		texvec[1] = mtex->size[1]*(mtex->projy ? (co[mtex->projy - 1] + mtex->ofs[1]) : mtex->ofs[1]);
		texvec[2] = mtex->size[2]*(mtex->projz ? (co[mtex->projz - 1] + mtex->ofs[2]) : mtex->ofs[2]);

		if (shi->osatex) {
			if (mtex->projx) {
				dxt[0] = mtex->size[0]*dx[mtex->projx - 1];
				dyt[0] = mtex->size[0]*dy[mtex->projx - 1];
			}
			else dxt[0] = dyt[0] = 0.f;
			if (mtex->projy) {
				dxt[1] = mtex->size[1]*dx[mtex->projy - 1];
				dyt[1] = mtex->size[1]*dy[mtex->projy - 1];
			}
			else dxt[1] = dyt[1] = 0.f;
			if (mtex->projz) {
				dxt[2] = mtex->size[2]*dx[mtex->projz - 1];
				dyt[2] = mtex->size[2]*dy[mtex->projz - 1];
			}
			else dxt[2]= dyt[2] = 0.f;
		}
	}
}

/* Bump code from 2.5 development cycle, has a number of bugs, but here for compatibility */

typedef struct CompatibleBump {
	float nu[3], nv[3], nn[3];
	float dudnu, dudnv, dvdnu, dvdnv;
	int nunvdone;
} CompatibleBump;

static void compatible_bump_init(CompatibleBump *compat_bump)
{
	memset(compat_bump, 0, sizeof(*compat_bump));

	compat_bump->dudnu = 1.0f;
	compat_bump->dvdnv = 1.0f;
}

static void compatible_bump_uv_derivs(CompatibleBump *compat_bump, ShadeInput *shi, MTex *mtex, int i)
{
	// uvmapping only, calculation of normal tangent u/v partial derivatives
	// (should not be here, dudnu, dudnv, dvdnu & dvdnv should probably be part of ShadeInputUV struct,
	//  nu/nv in ShadeInput and this calculation should then move to shadeinput.c, shade_input_set_shade_texco() func.)
	// NOTE: test for shi->obr->ob here, since vlr/obr/obi can be 'fake' when called from fastshade(), another reason to move it..
	// NOTE: shi->v1 is NULL when called from displace_render_vert, assigning verts in this case is not trivial because the shi quad face side is not know.
	if ((mtex->texflag & MTEX_COMPAT_BUMP) && shi->obr && shi->obr->ob && shi->v1) {
		if (mtex->mapto & (MAP_NORM|MAP_WARP) && !((mtex->tex->type==TEX_IMAGE) && (mtex->tex->imaflag & TEX_NORMALMAP))) {
			MTFace* tf = RE_vlakren_get_tface(shi->obr, shi->vlr, i, NULL, 0);
			int j1 = shi->i1, j2 = shi->i2, j3 = shi->i3;

			vlr_set_uv_indices(shi->vlr, &j1, &j2, &j3);

			// compute ortho basis around normal
			if (!compat_bump->nunvdone) {
				// render normal is negated
				compat_bump->nn[0] = -shi->vn[0];
				compat_bump->nn[1] = -shi->vn[1];
				compat_bump->nn[2] = -shi->vn[2];
				ortho_basis_v3v3_v3(compat_bump->nu, compat_bump->nv, compat_bump->nn);
				compat_bump->nunvdone= 1;
			}

			if (tf) {
				float *uv1 = tf->uv[j1], *uv2 = tf->uv[j2], *uv3 = tf->uv[j3];
				const float an[3] = {fabsf(compat_bump->nn[0]), fabsf(compat_bump->nn[1]), fabsf(compat_bump->nn[2])};
				const int a1 = (an[0] > an[1] && an[0] > an[2]) ? 1 : 0;
				const int a2 = (an[2] > an[0] && an[2] > an[1]) ? 1 : 2;
				const float dp1_a1 = shi->v1->co[a1] - shi->v3->co[a1];
				const float dp1_a2 = shi->v1->co[a2] - shi->v3->co[a2];
				const float dp2_a1 = shi->v2->co[a1] - shi->v3->co[a1];
				const float dp2_a2 = shi->v2->co[a2] - shi->v3->co[a2];
				const float du1 = uv1[0] - uv3[0], du2 = uv2[0] - uv3[0];
				const float dv1 = uv1[1] - uv3[1], dv2 = uv2[1] - uv3[1];
				const float dpdu_a1 = dv2*dp1_a1 - dv1*dp2_a1;
				const float dpdu_a2 = dv2*dp1_a2 - dv1*dp2_a2;
				const float dpdv_a1 = du1*dp2_a1 - du2*dp1_a1;
				const float dpdv_a2 = du1*dp2_a2 - du2*dp1_a2;
				float d = dpdu_a1*dpdv_a2 - dpdv_a1*dpdu_a2;
				float uvd = du1*dv2 - dv1*du2;

				if (uvd == 0.f) uvd = 1e-5f;
				if (d == 0.f) d = 1e-5f;
				d = uvd / d;

				compat_bump->dudnu = (dpdv_a2*compat_bump->nu[a1] - dpdv_a1*compat_bump->nu[a2])*d;
				compat_bump->dvdnu = (dpdu_a1*compat_bump->nu[a2] - dpdu_a2*compat_bump->nu[a1])*d;
				compat_bump->dudnv = (dpdv_a2*compat_bump->nv[a1] - dpdv_a1*compat_bump->nv[a2])*d;
				compat_bump->dvdnv = (dpdu_a1*compat_bump->nv[a2] - dpdu_a2*compat_bump->nv[a1])*d;
			}
		}
	}
}

static int compatible_bump_compute(CompatibleBump *compat_bump, ShadeInput *shi, MTex *mtex, Tex *tex, TexResult *texres, float Tnor, float *co, float *dx, float *dy, float *texvec, float *dxt, float *dyt)
{
	TexResult ttexr = {0, 0, 0, 0, 0, texres->talpha, NULL};	// temp TexResult
	float tco[3], texv[3], cd, ud, vd, du, dv, idu, idv;
	const int fromrgb = ((tex->type == TEX_IMAGE) || ((tex->flag & TEX_COLORBAND)!=0));
	const float bf = -0.04f*Tnor*mtex->norfac;
	int rgbnor;
	// disable internal bump eval
	float* nvec = texres->nor;
	texres->nor = NULL;
	// du & dv estimates, constant value defaults
	du = dv = 0.01f;

	// compute ortho basis around normal
	if (!compat_bump->nunvdone) {
		// render normal is negated
		negate_v3_v3(compat_bump->nn, shi->vn);
		ortho_basis_v3v3_v3(compat_bump->nu, compat_bump->nv, compat_bump->nn);
		compat_bump->nunvdone= 1;
	}

	// two methods, either constant based on main image resolution,
	// (which also works without osa, though of course not always good (or even very bad) results),
	// or based on tex derivative max values (osa only). Not sure which is best...

	if (!shi->osatex && (tex->type == TEX_IMAGE) && tex->ima) {
		// in case we have no proper derivatives, fall back to
		// computing du/dv it based on image size
		ImBuf* ibuf = BKE_image_get_ibuf(tex->ima, &tex->iuser);
		if (ibuf) {
			du = 1.f/(float)ibuf->x;
			dv = 1.f/(float)ibuf->y;
		}
	}
	else if (shi->osatex) {
		// we have derivatives, can compute proper du/dv
		if (tex->type == TEX_IMAGE) {	// 2d image, use u & v max. of dx/dy 2d vecs
			const float adx[2] = {fabsf(dx[0]), fabsf(dx[1])};
			const float ady[2] = {fabsf(dy[0]), fabsf(dy[1])};
			du = MAX2(adx[0], ady[0]);
			dv = MAX2(adx[1], ady[1]);
		}
		else {	// 3d procedural, estimate from all dx/dy elems
			const float adx[3] = {fabsf(dx[0]), fabsf(dx[1]), fabsf(dx[2])};
			const float ady[3] = {fabsf(dy[0]), fabsf(dy[1]), fabsf(dy[2])};
			du = MAX3(adx[0], adx[1], adx[2]);
			dv = MAX3(ady[0], ady[1], ady[2]);
		}
	}

	// center, main return value
	texco_mapping(shi, tex, mtex, co, dx, dy, texvec, dxt, dyt);
	rgbnor = multitex_mtex(shi, mtex, texvec, dxt, dyt, texres);
	cd = fromrgb ? (texres->tr + texres->tg + texres->tb)*0.33333333f : texres->tin;

	if (mtex->texco == TEXCO_UV) {
		// for the uv case, use the same value for both du/dv,
		// since individually scaling the normal derivatives makes them useless...
		du = MIN2(du, dv);
		idu = (du < 1e-5f) ? bf : (bf/du);

		// +u val
		tco[0] = co[0] + compat_bump->dudnu*du;
		tco[1] = co[1] + compat_bump->dvdnu*du;
		tco[2] = 0.f;
		texco_mapping(shi, tex, mtex, tco, dx, dy, texv, dxt, dyt);
		multitex_mtex(shi, mtex, texv, dxt, dyt, &ttexr);
		ud = idu*(cd - (fromrgb ? (ttexr.tr + ttexr.tg + ttexr.tb)*0.33333333f : ttexr.tin));

		// +v val
		tco[0] = co[0] + compat_bump->dudnv*du;
		tco[1] = co[1] + compat_bump->dvdnv*du;
		tco[2] = 0.f;
		texco_mapping(shi, tex, mtex, tco, dx, dy, texv, dxt, dyt);
		multitex_mtex(shi, mtex, texv, dxt, dyt, &ttexr);
		vd = idu*(cd - (fromrgb ? (ttexr.tr + ttexr.tg + ttexr.tb)*0.33333333f : ttexr.tin));
	}
	else {
		float tu[3], tv[3];

		copy_v3_v3(tu, compat_bump->nu);
		copy_v3_v3(tv, compat_bump->nv);

		idu = (du < 1e-5f) ? bf : (bf/du);
		idv = (dv < 1e-5f) ? bf : (bf/dv);

		if ((mtex->texco == TEXCO_ORCO) && shi->obr && shi->obr->ob) {
			mul_mat3_m4_v3(shi->obr->ob->imat_ren, tu);
			mul_mat3_m4_v3(shi->obr->ob->imat_ren, tv);
			normalize_v3(tu);
			normalize_v3(tv);
		}
		else if (mtex->texco == TEXCO_GLOB) {
			mul_mat3_m4_v3(R.viewinv, tu);
			mul_mat3_m4_v3(R.viewinv, tv);
		}
		else if (mtex->texco == TEXCO_OBJECT && mtex->object) {
			mul_mat3_m4_v3(mtex->object->imat_ren, tu);
			mul_mat3_m4_v3(mtex->object->imat_ren, tv);
			normalize_v3(tu);
			normalize_v3(tv);
		}

		// +u val
		tco[0] = co[0] + tu[0]*du;
		tco[1] = co[1] + tu[1]*du;
		tco[2] = co[2] + tu[2]*du;
		texco_mapping(shi, tex, mtex, tco, dx, dy, texv, dxt, dyt);
		multitex_mtex(shi, mtex, texv, dxt, dyt, &ttexr);
		ud = idu*(cd - (fromrgb ? (ttexr.tr + ttexr.tg + ttexr.tb)*0.33333333f : ttexr.tin));

		// +v val
		tco[0] = co[0] + tv[0]*dv;
		tco[1] = co[1] + tv[1]*dv;
		tco[2] = co[2] + tv[2]*dv;
		texco_mapping(shi, tex, mtex, tco, dx, dy, texv, dxt, dyt);
		multitex_mtex(shi, mtex, texv, dxt, dyt, &ttexr);
		vd = idv*(cd - (fromrgb ? (ttexr.tr + ttexr.tg + ttexr.tb)*0.33333333f : ttexr.tin));
	}

	// bumped normal
	compat_bump->nu[0] += ud*compat_bump->nn[0];
	compat_bump->nu[1] += ud*compat_bump->nn[1];
	compat_bump->nu[2] += ud*compat_bump->nn[2];
	compat_bump->nv[0] += vd*compat_bump->nn[0];
	compat_bump->nv[1] += vd*compat_bump->nn[1];
	compat_bump->nv[2] += vd*compat_bump->nn[2];
	cross_v3_v3v3(nvec, compat_bump->nu, compat_bump->nv);

	nvec[0] = -nvec[0];
	nvec[1] = -nvec[1];
	nvec[2] = -nvec[2];
	texres->nor = nvec;

	rgbnor |= TEX_NOR;
	return rgbnor;
}

/* Improved bump code from later in 2.5 development cycle */

typedef struct NTapBump {
	int init_done;
	int iPrevBumpSpace;	// 0: uninitialized, 1: objectspace, 2: texturespace, 4: viewspace
	// bumpmapping
	float vNorg[3]; // backup copy of shi->vn
	float vNacc[3]; // original surface normal minus the surface gradient of every bump map which is encountered
	float vR1[3], vR2[3]; // cross products (sigma_y, original_normal), (original_normal, sigma_x)
	float sgn_det; // sign of the determinant of the matrix {sigma_x, sigma_y, original_normal}
	float fPrevMagnitude; // copy of previous magnitude, used for multiple bumps in different spaces
} NTapBump;

static void ntap_bump_init(NTapBump *ntap_bump)
{
	memset(ntap_bump, 0, sizeof(*ntap_bump));
}

static int ntap_bump_compute(NTapBump *ntap_bump, ShadeInput *shi, MTex *mtex, Tex *tex, TexResult *texres, float Tnor, float *co, float *dx, float *dy, float *texvec, float *dxt, float *dyt)
{
	TexResult ttexr = {0, 0, 0, 0, 0, texres->talpha, NULL};	// temp TexResult

	const int fromrgb = ((tex->type == TEX_IMAGE) || ((tex->flag & TEX_COLORBAND)!=0));

	// The negate on Hscale is done because the
	// normal in the renderer points inward which corresponds
	// to inverting the bump map. The normals are generated
	// this way in calc_vertexnormals(). Should this ever change
	// this negate must be removed.
	float Hscale = -Tnor*mtex->norfac;

	int dimx=512, dimy=512;
	const int imag_tspace_dimension_x = 1024;		// only used for texture space variant
	float aspect = 1.0f;

	// 2 channels for 2D texture and 3 for 3D textures.
	const int nr_channels = (mtex->texco == TEXCO_UV)? 2 : 3;
	int c, rgbnor, iBumpSpace;
	float dHdx, dHdy;
	int found_deriv_map = (tex->type==TEX_IMAGE) && (tex->imaflag & TEX_DERIVATIVEMAP);

	// disable internal bump eval in sampler, save pointer
	float *nvec = texres->nor;
	texres->nor = NULL;

	if (found_deriv_map==0) {
		if ( mtex->texflag & MTEX_BUMP_TEXTURESPACE ) {
			if (tex->ima)
				Hscale *= 13.0f; // appears to be a sensible default value
		}
		else
			Hscale *= 0.1f; // factor 0.1 proved to look like the previous bump code
	}

	if ( !ntap_bump->init_done ) {
		copy_v3_v3(ntap_bump->vNacc, shi->vn);
		copy_v3_v3(ntap_bump->vNorg, shi->vn);
		ntap_bump->fPrevMagnitude = 1.0f;
		ntap_bump->iPrevBumpSpace = 0;
		
		ntap_bump->init_done = 1;
	}

	// resolve image dimensions
	if (found_deriv_map || (mtex->texflag&MTEX_BUMP_TEXTURESPACE)!=0) {
		ImBuf* ibuf = BKE_image_get_ibuf(tex->ima, &tex->iuser);
		if (ibuf) {
			dimx = ibuf->x;
			dimy = ibuf->y;
			aspect = ((float) dimy) / dimx;
		}
	}
	
	if (found_deriv_map) {
		float dBdu, dBdv, auto_bump = 1.0f;
		float s = 1;		// negate this if flipped texture coordinate
		texco_mapping(shi, tex, mtex, co, dx, dy, texvec, dxt, dyt);
		rgbnor = multitex_mtex(shi, mtex, texvec, dxt, dyt, texres);

		if (shi->obr->ob->derivedFinal) {
			auto_bump = shi->obr->ob->derivedFinal->auto_bump_scale;
		}

		{
			float fVirtDim = sqrtf(fabsf((float) (dimx*dimy)*mtex->size[0]*mtex->size[1]));
			auto_bump /= MAX2(fVirtDim, FLT_EPSILON);
		}
		
		// this variant using a derivative map is described here
		// http://mmikkelsen3d.blogspot.com/2011/07/derivative-maps.html
		dBdu = auto_bump*Hscale*dimx*(2*texres->tr-1);
		dBdv = auto_bump*Hscale*dimy*(2*texres->tg-1);

		dHdx = dBdu*dxt[0] + s * dBdv*dxt[1];
		dHdy = dBdu*dyt[0] + s * dBdv*dyt[1];
	}
	else if (!(mtex->texflag & MTEX_5TAP_BUMP)) {
		// compute height derivatives with respect to output image pixel coordinates x and y
		float STll[3], STlr[3], STul[3];
		float Hll, Hlr, Hul;

		texco_mapping(shi, tex, mtex, co, dx, dy, texvec, dxt, dyt);

		for (c=0; c<nr_channels; c++) {
			// dx contains the derivatives (du/dx, dv/dx)
			// dy contains the derivatives (du/dy, dv/dy)
			STll[c] = texvec[c];
			STlr[c] = texvec[c]+dxt[c];
			STul[c] = texvec[c]+dyt[c];
		}

		// clear unused derivatives
		for (c=nr_channels; c<3; c++) {
			STll[c] = 0.0f;
			STlr[c] = 0.0f;
			STul[c] = 0.0f;
		}

		// use texres for the center sample, set rgbnor
		rgbnor = multitex_mtex(shi, mtex, STll, dxt, dyt, texres);
		Hll = (fromrgb)? RGBTOBW(texres->tr, texres->tg, texres->tb) : texres->tin;

		// use ttexr for the other 2 taps
		multitex_mtex(shi, mtex, STlr, dxt, dyt, &ttexr);
		Hlr = (fromrgb)? RGBTOBW(ttexr.tr, ttexr.tg, ttexr.tb) : ttexr.tin;

		multitex_mtex(shi, mtex, STul, dxt, dyt, &ttexr);
		Hul = (fromrgb)? RGBTOBW(ttexr.tr, ttexr.tg, ttexr.tb) : ttexr.tin;

		dHdx = Hscale*(Hlr - Hll);
		dHdy = Hscale*(Hul - Hll);
	}
	else {
		/* same as above, but doing 5 taps, increasing quality at cost of speed */
		float STc[3], STl[3], STr[3], STd[3], STu[3];
		float /* Hc, */ /* UNUSED */  Hl, Hr, Hd, Hu;

		texco_mapping(shi, tex, mtex, co, dx, dy, texvec, dxt, dyt);

		for (c=0; c<nr_channels; c++) {
			STc[c] = texvec[c];
			STl[c] = texvec[c] - 0.5f*dxt[c];
			STr[c] = texvec[c] + 0.5f*dxt[c];
			STd[c] = texvec[c] - 0.5f*dyt[c];
			STu[c] = texvec[c] + 0.5f*dyt[c];
		}

		// clear unused derivatives
		for (c=nr_channels; c<3; c++) {
			STc[c] = 0.0f;
			STl[c] = 0.0f;
			STr[c] = 0.0f;
			STd[c] = 0.0f;
			STu[c] = 0.0f;
		}

		// use texres for the center sample, set rgbnor
		rgbnor = multitex_mtex(shi, mtex, STc, dxt, dyt, texres);
		/* Hc = (fromrgb)? RGBTOBW(texres->tr, texres->tg, texres->tb) : texres->tin; */ /* UNUSED */

		// use ttexr for the other taps
		multitex_mtex(shi, mtex, STl, dxt, dyt, &ttexr);
		Hl = (fromrgb)? RGBTOBW(ttexr.tr, ttexr.tg, ttexr.tb) : ttexr.tin;
		multitex_mtex(shi, mtex, STr, dxt, dyt, &ttexr);
		Hr = (fromrgb)? RGBTOBW(ttexr.tr, ttexr.tg, ttexr.tb) : ttexr.tin;
		multitex_mtex(shi, mtex, STd, dxt, dyt, &ttexr);
		Hd = (fromrgb)? RGBTOBW(ttexr.tr, ttexr.tg, ttexr.tb) : ttexr.tin;
		multitex_mtex(shi, mtex, STu, dxt, dyt, &ttexr);
		Hu = (fromrgb)? RGBTOBW(ttexr.tr, ttexr.tg, ttexr.tb) : ttexr.tin;

		dHdx = Hscale*(Hr - Hl);
		dHdy = Hscale*(Hu - Hd);
	}

	// restore pointer
	texres->nor = nvec;

	/* replaced newbump with code based on listing 1 and 2 of
	 * [Mik10] Mikkelsen M. S.: Bump Mapping Unparametrized Surfaces on the GPU.
	 * -> http://jbit.net/~sparky/sfgrad_bump/mm_sfgrad_bump.pdf */

	if ( mtex->texflag & MTEX_BUMP_OBJECTSPACE )
		iBumpSpace = 1;
	else if ( mtex->texflag & MTEX_BUMP_TEXTURESPACE )
		iBumpSpace = 2;
	else
		iBumpSpace = 4; // ViewSpace
	
	if ( ntap_bump->iPrevBumpSpace != iBumpSpace ) {
		
		// initialize normal perturbation vectors
		int xyz;
		float fDet, abs_fDet, fMagnitude;
		// object2view and inverted matrix
		float obj2view[3][3], view2obj[3][3], tmp[4][4];
		// local copies of derivatives and normal
		float dPdx[3], dPdy[3], vN[3];
		copy_v3_v3(dPdx, shi->dxco);
		copy_v3_v3(dPdy, shi->dyco);
		copy_v3_v3(vN, ntap_bump->vNorg);
		
		if ( mtex->texflag & MTEX_BUMP_OBJECTSPACE ) {
			// TODO: these calculations happen for every pixel!
			//	-> move to shi->obi
			mult_m4_m4m4(tmp, R.viewmat, shi->obr->ob->obmat);
			copy_m3_m4(obj2view, tmp); // use only upper left 3x3 matrix
			invert_m3_m3(view2obj, obj2view);
		
			// generate the surface derivatives in object space
			mul_m3_v3(view2obj, dPdx);
			mul_m3_v3( view2obj, dPdy );
			// generate the unit normal in object space
			mul_transposed_m3_v3( obj2view, vN );
			normalize_v3(vN);
		}
		
		cross_v3_v3v3(ntap_bump->vR1, dPdy, vN);
		cross_v3_v3v3(ntap_bump->vR2, vN, dPdx);
		fDet = dot_v3v3(dPdx, ntap_bump->vR1);
		ntap_bump->sgn_det = (fDet < 0)? -1.0f: 1.0f;
		abs_fDet = ntap_bump->sgn_det * fDet;

		if ( mtex->texflag & MTEX_BUMP_TEXTURESPACE ) {
			if (tex->ima) {
				// crazy hack solution that gives results similar to normal mapping - part 1
				normalize_v3(ntap_bump->vR1);
				normalize_v3(ntap_bump->vR2);
				abs_fDet = 1.0f;
			}
		}
		
		fMagnitude = abs_fDet;
		if ( mtex->texflag & MTEX_BUMP_OBJECTSPACE ) {
			// pre do transform of texres->nor by the inverse transposed of obj2view
			mul_transposed_m3_v3( view2obj, vN );
			mul_transposed_m3_v3( view2obj, ntap_bump->vR1 );
			mul_transposed_m3_v3( view2obj, ntap_bump->vR2 );
			
			fMagnitude *= len_v3(vN);
		}
		
		if (ntap_bump->fPrevMagnitude > 0.0f)
			for (xyz=0; xyz<3; xyz++)
				ntap_bump->vNacc[xyz] *= fMagnitude / ntap_bump->fPrevMagnitude;
		
		ntap_bump->fPrevMagnitude = fMagnitude;
		ntap_bump->iPrevBumpSpace = iBumpSpace;
	}

	if ( mtex->texflag & MTEX_BUMP_TEXTURESPACE ) {
		if (tex->ima) {
			// crazy hack solution that gives results similar to normal mapping - part 2
			float vec[2];
			const float imag_tspace_dimension_y = aspect*imag_tspace_dimension_x;
			
			vec[0] = imag_tspace_dimension_x*dxt[0];
			vec[1] = imag_tspace_dimension_y*dxt[1];
			dHdx *= 1.0f/len_v2(vec);
			vec[0] = imag_tspace_dimension_x*dyt[0];
			vec[1] = imag_tspace_dimension_y*dyt[1];
			dHdy *= 1.0f/len_v2(vec);
		}
	}
	
	// subtract the surface gradient from vNacc
	for (c=0; c<3; c++) {
		float vSurfGrad_compi = ntap_bump->sgn_det * (dHdx * ntap_bump->vR1[c] + dHdy * ntap_bump->vR2[c]);
		ntap_bump->vNacc[c] -= vSurfGrad_compi;
		texres->nor[c] = ntap_bump->vNacc[c]; // copy
	}

	rgbnor |= TEX_NOR;
	return rgbnor;
}

void do_material_tex(ShadeInput *shi, Render *re)
{
	CompatibleBump compat_bump;
	NTapBump ntap_bump;
	MTex *mtex;
	Tex *tex;
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float *co = NULL, *dx = NULL, *dy = NULL;
	float fact, facm, factt, facmm, stencilTin=1.0;
	float texvec[3], dxt[3], dyt[3], tempvec[3], norvec[3], warpvec[3]={0.0f, 0.0f, 0.0f}, Tnor=1.0;
	int tex_nr, rgbnor= 0, warpdone=0;
	int use_compat_bump = 0, use_ntap_bump = 0;
	int found_nmapping = 0, found_deriv_map = 0;
	int iFirstTimeNMap=1;

	compatible_bump_init(&compat_bump);
	ntap_bump_init(&ntap_bump);

	if (re->r.scemode & R_NO_TEX) return;
	/* here: test flag if there's a tex (todo) */

	for (tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
		
		/* separate tex switching */
		if (shi->mat->septex & (1<<tex_nr)) continue;
		
		if (shi->mat->mtex[tex_nr]) {
			mtex= shi->mat->mtex[tex_nr];
			
			tex= mtex->tex;
			if (tex==0) continue;

			found_deriv_map = (tex->type==TEX_IMAGE) && (tex->imaflag & TEX_DERIVATIVEMAP);
			use_compat_bump= (mtex->texflag & MTEX_COMPAT_BUMP);
			use_ntap_bump= ((mtex->texflag & (MTEX_3TAP_BUMP|MTEX_5TAP_BUMP|MTEX_BICUBIC_BUMP))!=0 || found_deriv_map!=0) ? 1 : 0;

			/* XXX texture node trees don't work for this yet */
			if (tex->nodetree && tex->use_nodes) {
				use_compat_bump = 0;
				use_ntap_bump = 0;
			}
			
			/* case displacement mapping */
			if (shi->osatex==0 && use_ntap_bump) {
				use_ntap_bump = 0;
				use_compat_bump = 1;
			}
			
			/* case ocean */
			if (tex->type == TEX_OCEAN) {
				use_ntap_bump = 0;
				use_compat_bump = 0;
			}

			/* which coords */
			if (mtex->texco==TEXCO_ORCO) {
				if (mtex->texflag & MTEX_DUPLI_MAPTO) {
					co= shi->duplilo; dx= dxt; dy= dyt;
					dxt[0]= dxt[1]= dxt[2]= 0.0f;
					dyt[0]= dyt[1]= dyt[2]= 0.0f;
				}
				else {
					co= shi->lo; dx= shi->dxlo; dy= shi->dylo;
				}
			}
			else if (mtex->texco==TEXCO_STICKY) {
				co= shi->sticky; dx= shi->dxsticky; dy= shi->dysticky;
			}
			else if (mtex->texco==TEXCO_OBJECT) {
				Object *ob= mtex->object;
				if (ob) {
					co= tempvec;
					dx= dxt;
					dy= dyt;
					copy_v3_v3(tempvec, shi->co);
					if (mtex->texflag & MTEX_OB_DUPLI_ORIG)
						if (shi->obi && shi->obi->duplitexmat)
							mul_m4_v3(shi->obi->duplitexmat, tempvec);
					mul_m4_v3(ob->imat_ren, tempvec);
					if (shi->osatex) {
						copy_v3_v3(dxt, shi->dxco);
						copy_v3_v3(dyt, shi->dyco);
						mul_mat3_m4_v3(ob->imat_ren, dxt);
						mul_mat3_m4_v3(ob->imat_ren, dyt);
					}
				}
				else {
					/* if object doesn't exist, do not use orcos (not initialized) */
					co= shi->co;
					dx= shi->dxco; dy= shi->dyco;
				}
			}
			else if (mtex->texco==TEXCO_REFL) {
				calc_R_ref(shi);
				co= shi->ref; dx= shi->dxref; dy= shi->dyref;
			}
			else if (mtex->texco==TEXCO_NORM) {
				co= shi->orn; dx= shi->dxno; dy= shi->dyno;
			}
			else if (mtex->texco==TEXCO_TANGENT) {
				co= shi->tang; dx= shi->dxno; dy= shi->dyno;
			}
			else if (mtex->texco==TEXCO_GLOB) {
				co= shi->gl; dx= shi->dxgl; dy= shi->dygl;
			}
			else if (mtex->texco==TEXCO_UV) {
				if (mtex->texflag & MTEX_DUPLI_MAPTO) {
					co= shi->dupliuv; dx= dxt; dy= dyt;
					dxt[0]= dxt[1]= dxt[2]= 0.0f;
					dyt[0]= dyt[1]= dyt[2]= 0.0f;
				}
				else {
					ShadeInputUV *suv= &shi->uv[shi->actuv];
					int i = shi->actuv;

					if (mtex->uvname[0] != 0) {
						for (i = 0; i < shi->totuv; i++) {
							if (strcmp(shi->uv[i].name, mtex->uvname)==0) {
								suv= &shi->uv[i];
								break;
							}
						}
					}

					co= suv->uv;
					dx= suv->dxuv;
					dy= suv->dyuv; 

					compatible_bump_uv_derivs(&compat_bump, shi, mtex, i);
				}
			}
			else if (mtex->texco==TEXCO_WINDOW) {
				co= shi->winco; dx= shi->dxwin; dy= shi->dywin;
			}
			else if (mtex->texco==TEXCO_STRAND) {
				co= tempvec; dx= dxt; dy= dyt;
				co[0]= shi->strandco;
				co[1]= co[2]= 0.0f;
				dx[0]= shi->dxstrand;
				dx[1]= dx[2]= 0.0f;
				dy[0]= shi->dystrand;
				dy[1]= dy[2]= 0.0f;
			}
			else if (mtex->texco==TEXCO_STRESS) {
				co= tempvec; dx= dxt; dy= dyt;
				co[0]= shi->stress;
				co[1]= co[2]= 0.0f;
				dx[0]= 0.0f;
				dx[1]= dx[2]= 0.0f;
				dy[0]= 0.0f;
				dy[1]= dy[2]= 0.0f;
			}
			else continue;	// can happen when texco defines disappear and it renders old files

			/* the pointer defines if bumping happens */
			if (mtex->mapto & (MAP_NORM|MAP_WARP)) {
				texres.nor= norvec;
				norvec[0]= norvec[1]= norvec[2]= 0.0;
			}
			else texres.nor= NULL;
			
			if (warpdone) {
				add_v3_v3v3(tempvec, co, warpvec);
				co= tempvec;
			}

			/* XXX texture node trees don't work for this yet */
			if (texres.nor && !((tex->type==TEX_IMAGE) && (tex->imaflag & TEX_NORMALMAP))) {
				if (use_compat_bump) {
					rgbnor = compatible_bump_compute(&compat_bump, shi, mtex, tex,
						&texres, Tnor*stencilTin, co, dx, dy, texvec, dxt, dyt);
				}
				else if (use_ntap_bump) {
					rgbnor = ntap_bump_compute(&ntap_bump, shi, mtex, tex,
						&texres, Tnor*stencilTin, co, dx, dy, texvec, dxt, dyt);
				}
				else {
					texco_mapping(shi, tex, mtex, co, dx, dy, texvec, dxt, dyt);
					rgbnor = multitex_mtex(shi, mtex, texvec, dxt, dyt, &texres);
				}
			}
			else {
				texco_mapping(shi, tex, mtex, co, dx, dy, texvec, dxt, dyt);
				rgbnor = multitex_mtex(shi, mtex, texvec, dxt, dyt, &texres);
			}

			/* texture output */

			if ( (rgbnor & TEX_RGB) && (mtex->texflag & MTEX_RGBTOINT)) {
				texres.tin= (0.35f*texres.tr+0.45f*texres.tg+0.2f*texres.tb);
				rgbnor-= TEX_RGB;
			}
			if (mtex->texflag & MTEX_NEGATIVE) {
				if (rgbnor & TEX_RGB) {
					texres.tr= 1.0f-texres.tr;
					texres.tg= 1.0f-texres.tg;
					texres.tb= 1.0f-texres.tb;
				}
				texres.tin= 1.0f-texres.tin;
			}
			if (mtex->texflag & MTEX_STENCIL) {
				if (rgbnor & TEX_RGB) {
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
			
			if (texres.nor) {
				if ((rgbnor & TEX_NOR)==0) {
					/* make our own normal */
					if (rgbnor & TEX_RGB) {
						texres.nor[0]= texres.tr;
						texres.nor[1]= texres.tg;
						texres.nor[2]= texres.tb;
					}
					else {
						float co_nor= 0.5*cos(texres.tin-0.5f);
						float si= 0.5*sin(texres.tin-0.5f);
						float f1, f2;

						f1= shi->vn[0];
						f2= shi->vn[1];
						texres.nor[0]= f1*co_nor+f2*si;
						f1= shi->vn[1];
						f2= shi->vn[2];
						texres.nor[1]= f1*co_nor+f2*si;
						texres.nor[2]= f2*co_nor-f1*si;
					}
				}
				// warping, local space
				if (mtex->mapto & MAP_WARP) {
					float *warpnor= texres.nor, warpnor_[3];
					
					if (use_ntap_bump) {
						copy_v3_v3(warpnor_, texres.nor);
						warpnor= warpnor_;
						normalize_v3(warpnor_);
					}
					warpvec[0]= mtex->warpfac*warpnor[0];
					warpvec[1]= mtex->warpfac*warpnor[1];
					warpvec[2]= mtex->warpfac*warpnor[2];
					warpdone= 1;
				}
#if 0				
				if (mtex->texflag & MTEX_VIEWSPACE) {
					// rotate to global coords
					if (mtex->texco==TEXCO_ORCO || mtex->texco==TEXCO_UV) {
						if (shi->vlr && shi->obr && shi->obr->ob) {
							float len= normalize_v3(texres.nor);
							// can be optimized... (ton)
							mul_mat3_m4_v3(shi->obr->ob->obmat, texres.nor);
							mul_mat3_m4_v3(re->viewmat, texres.nor);
							normalize_v3(texres.nor);
							mul_v3_fl(texres.nor, len);
						}
					}
				}
#endif				
			}

			/* mapping */
			if (mtex->mapto & (MAP_COL+MAP_COLSPEC+MAP_COLMIR)) {
				float tcol[3];
				
				/* stencil maps on the texture control slider, not texture intensity value */
				
				tcol[0]=texres.tr; tcol[1]=texres.tg; tcol[2]=texres.tb;
				
				if ((rgbnor & TEX_RGB)==0) {
					tcol[0]= mtex->r;
					tcol[1]= mtex->g;
					tcol[2]= mtex->b;
				}
				else if (mtex->mapto & MAP_ALPHA) {
					texres.tin= stencilTin;
				}
				else texres.tin= texres.ta;
				
				/* inverse gamma correction */
				if (tex->type==TEX_IMAGE) {
					Image *ima = tex->ima;
					ImBuf *ibuf = BKE_image_get_ibuf(ima, &tex->iuser);
					
					/* don't linearize float buffers, assumed to be linear */
					if (ibuf && !(ibuf->rect_float) && re->r.color_mgt_flag & R_COLOR_MANAGEMENT)
						srgb_to_linearrgb_v3_v3(tcol, tcol);
				}
				
				if (mtex->mapto & MAP_COL) {
					float colfac= mtex->colfac*stencilTin;
					texture_rgb_blend(&shi->r, tcol, &shi->r, texres.tin, colfac, mtex->blendtype);
				}
				if (mtex->mapto & MAP_COLSPEC) {
					float colspecfac= mtex->colspecfac*stencilTin;
					texture_rgb_blend(&shi->specr, tcol, &shi->specr, texres.tin, colspecfac, mtex->blendtype);
				}
				if (mtex->mapto & MAP_COLMIR) {
					float mirrfac= mtex->mirrfac*stencilTin;

					// exception for envmap only
					if (tex->type==TEX_ENVMAP && mtex->blendtype==MTEX_BLEND) {
						fact= texres.tin*mirrfac;
						facm= 1.0f- fact;
						shi->refcol[0]= fact + facm*shi->refcol[0];
						shi->refcol[1]= fact*tcol[0] + facm*shi->refcol[1];
						shi->refcol[2]= fact*tcol[1] + facm*shi->refcol[2];
						shi->refcol[3]= fact*tcol[2] + facm*shi->refcol[3];
					}
					else {
						texture_rgb_blend(&shi->mirr, tcol, &shi->mirr, texres.tin, mirrfac, mtex->blendtype);
					}
				}
			}
			if ( (mtex->mapto & MAP_NORM) ) {
				if (texres.nor) {
					float norfac= mtex->norfac;
					
					/* we need to code blending modes for normals too once.. now 1 exception hardcoded */
					
					if ((tex->type==TEX_IMAGE) && (tex->imaflag & TEX_NORMALMAP)) {
						
						found_nmapping = 1;
						
						/* qdn: for normalmaps, to invert the normalmap vector,
						 * it is better to negate x & y instead of subtracting the vector as was done before */
						if (norfac < 0.0f) {
							texres.nor[0] = -texres.nor[0];
							texres.nor[1] = -texres.nor[1];
						}
						fact = Tnor*fabsf(norfac);
						if (fact>1.f) fact = 1.f;
						facm = 1.f-fact;
						if (mtex->normapspace == MTEX_NSPACE_TANGENT) {
							/* qdn: tangent space */
							float B[3], tv[3];
							const float * no = iFirstTimeNMap!=0 ? shi->nmapnorm : shi->vn;
							iFirstTimeNMap=0;
							cross_v3_v3v3(B, no, shi->nmaptang);	/* bitangent */
							mul_v3_fl(B, shi->nmaptang[3]);
							/* transform norvec from tangent space to object surface in camera space */
							tv[0] = texres.nor[0]*shi->nmaptang[0] + texres.nor[1]*B[0] + texres.nor[2]*no[0];
							tv[1] = texres.nor[0]*shi->nmaptang[1] + texres.nor[1]*B[1] + texres.nor[2]*no[1];
							tv[2] = texres.nor[0]*shi->nmaptang[2] + texres.nor[1]*B[2] + texres.nor[2]*no[2];
							shi->vn[0]= facm*no[0] + fact*tv[0];
							shi->vn[1]= facm*no[1] + fact*tv[1];
							shi->vn[2]= facm*no[2] + fact*tv[2];
						}
						else {
							float nor[3];

							copy_v3_v3(nor, texres.nor);

							if (mtex->normapspace == MTEX_NSPACE_CAMERA);
							else if (mtex->normapspace == MTEX_NSPACE_WORLD) {
								mul_mat3_m4_v3(re->viewmat, nor);
							}
							else if (mtex->normapspace == MTEX_NSPACE_OBJECT) {
								if (shi->obr && shi->obr->ob)
									mul_mat3_m4_v3(shi->obr->ob->obmat, nor);
								mul_mat3_m4_v3(re->viewmat, nor);
							}

							normalize_v3(nor);

							/* qdn: worldspace */
							shi->vn[0]= facm*shi->vn[0] + fact*nor[0];
							shi->vn[1]= facm*shi->vn[1] + fact*nor[1];
							shi->vn[2]= facm*shi->vn[2] + fact*nor[2];
						}
					}
					else {
						/* XXX texture node trees don't work for this yet */
						if (use_compat_bump || use_ntap_bump) {
							shi->vn[0] = texres.nor[0];
							shi->vn[1] = texres.nor[1];
							shi->vn[2] = texres.nor[2];
						}
						else {
							float nor[3], dot;
	
							if (shi->mat->mode & MA_TANGENT_V) {
								shi->tang[0]+= Tnor*norfac*texres.nor[0];
								shi->tang[1]+= Tnor*norfac*texres.nor[1];
								shi->tang[2]+= Tnor*norfac*texres.nor[2];
							}
	
							/* prevent bump to become negative normal */
							nor[0]= Tnor*norfac*texres.nor[0];
							nor[1]= Tnor*norfac*texres.nor[1];
							nor[2]= Tnor*norfac*texres.nor[2];
							
							dot= 0.5f + 0.5f * dot_v3v3(nor, shi->vn);
							
							shi->vn[0]+= dot*nor[0];
							shi->vn[1]+= dot*nor[1];
							shi->vn[2]+= dot*nor[2];
						}
					}
					normalize_v3(shi->vn);
					
					/* this makes sure the bump is passed on to the next texture */
					shi->orn[0]= -shi->vn[0];
					shi->orn[1]= -shi->vn[1];
					shi->orn[2]= -shi->vn[2];
				}
			}

			if ( mtex->mapto & MAP_DISPLACE ) {
				/* Now that most textures offer both Nor and Intensity, allow  */
				/* both to work, and let user select with slider.   */
				if (texres.nor) {
					float norfac= mtex->norfac;

					shi->displace[0]+= 0.2f*Tnor*norfac*texres.nor[0];
					shi->displace[1]+= 0.2f*Tnor*norfac*texres.nor[1];
					shi->displace[2]+= 0.2f*Tnor*norfac*texres.nor[2];
				}
				
				if (rgbnor & TEX_RGB) {
					texres.tin= (0.35f*texres.tr+0.45f*texres.tg+0.2f*texres.tb);
				}

				factt= (0.5f-texres.tin)*mtex->dispfac*stencilTin; facmm= 1.0f-factt;

				if (mtex->blendtype==MTEX_BLEND) {
					shi->displace[0]= factt*shi->vn[0] + facmm*shi->displace[0];
					shi->displace[1]= factt*shi->vn[1] + facmm*shi->displace[1];
					shi->displace[2]= factt*shi->vn[2] + facmm*shi->displace[2];
				}
				else if (mtex->blendtype==MTEX_MUL) {
					shi->displace[0]*= factt*shi->vn[0];
					shi->displace[1]*= factt*shi->vn[1];
					shi->displace[2]*= factt*shi->vn[2];
				}
				else { /* add or sub */
					if (mtex->blendtype==MTEX_SUB) factt= -factt;
					shi->displace[0]+= factt*shi->vn[0];
					shi->displace[1]+= factt*shi->vn[1];
					shi->displace[2]+= factt*shi->vn[2];
				}
			}

			if (mtex->mapto & MAP_VARS) {
				/* stencil maps on the texture control slider, not texture intensity value */
				
				if (rgbnor & TEX_RGB) {
					if (texres.talpha) texres.tin= texres.ta;
					else texres.tin= (0.35f*texres.tr+0.45f*texres.tg+0.2f*texres.tb);
				}

				if (mtex->mapto & MAP_REF) {
					float difffac= mtex->difffac*stencilTin;

					shi->refl= texture_value_blend(mtex->def_var, shi->refl, texres.tin, difffac, mtex->blendtype);
					if (shi->refl<0.0f) shi->refl= 0.0f;
				}
				if (mtex->mapto & MAP_SPEC) {
					float specfac= mtex->specfac*stencilTin;
					
					shi->spec= texture_value_blend(mtex->def_var, shi->spec, texres.tin, specfac, mtex->blendtype);
					if (shi->spec<0.0f) shi->spec= 0.0f;
				}
				if (mtex->mapto & MAP_EMIT) {
					float emitfac= mtex->emitfac*stencilTin;

					shi->emit= texture_value_blend(mtex->def_var, shi->emit, texres.tin, emitfac, mtex->blendtype);
					if (shi->emit<0.0f) shi->emit= 0.0f;
				}
				if (mtex->mapto & MAP_ALPHA) {
					float alphafac= mtex->alphafac*stencilTin;

					shi->alpha= texture_value_blend(mtex->def_var, shi->alpha, texres.tin, alphafac, mtex->blendtype);
					if (shi->alpha<0.0f) shi->alpha= 0.0f;
					else if (shi->alpha>1.0f) shi->alpha= 1.0f;
				}
				if (mtex->mapto & MAP_HAR) {
					float har;  // have to map to 0-1
					float hardfac= mtex->hardfac*stencilTin;
					
					har= ((float)shi->har)/128.0f;
					har= 128.0f*texture_value_blend(mtex->def_var, har, texres.tin, hardfac, mtex->blendtype);
					
					if (har<1.0f) shi->har= 1;
					else if (har>511) shi->har= 511;
					else shi->har= (int)har;
				}
				if (mtex->mapto & MAP_RAYMIRR) {
					float raymirrfac= mtex->raymirrfac*stencilTin;

					shi->ray_mirror= texture_value_blend(mtex->def_var, shi->ray_mirror, texres.tin, raymirrfac, mtex->blendtype);
					if (shi->ray_mirror<0.0f) shi->ray_mirror= 0.0f;
					else if (shi->ray_mirror>1.0f) shi->ray_mirror= 1.0f;
				}
				if (mtex->mapto & MAP_TRANSLU) {
					float translfac= mtex->translfac*stencilTin;

					shi->translucency= texture_value_blend(mtex->def_var, shi->translucency, texres.tin, translfac, mtex->blendtype);
					if (shi->translucency<0.0f) shi->translucency= 0.0f;
					else if (shi->translucency>1.0f) shi->translucency= 1.0f;
				}
				if (mtex->mapto & MAP_AMB) {
					float ambfac= mtex->ambfac*stencilTin;

					shi->amb= texture_value_blend(mtex->def_var, shi->amb, texres.tin, ambfac, mtex->blendtype);
					if (shi->amb<0.0f) shi->amb= 0.0f;
					else if (shi->amb>1.0f) shi->amb= 1.0f;
					
					shi->ambr= shi->amb*re->wrld.ambr;
					shi->ambg= shi->amb*re->wrld.ambg;
					shi->ambb= shi->amb*re->wrld.ambb;
				}
			}
		}
	}
	if ((use_compat_bump || use_ntap_bump || found_nmapping) && (shi->mat->mode & MA_TANGENT_V)!=0) {
		const float fnegdot = -dot_v3v3(shi->vn, shi->tang);
		// apply Gram-Schmidt projection
		madd_v3_v3fl(shi->tang,  shi->vn, fnegdot);
		normalize_v3(shi->tang);
	}
}


void do_volume_tex(ShadeInput *shi, const float *xyz, int mapto_flag, float *col, float *val, Render *re)
{
	MTex *mtex;
	Tex *tex;
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	int tex_nr, rgbnor= 0;
	float co[3], texvec[3];
	float fact, stencilTin=1.0;
	
	if (re->r.scemode & R_NO_TEX) return;
	/* here: test flag if there's a tex (todo) */
	
	for (tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
		/* separate tex switching */
		if (shi->mat->septex & (1<<tex_nr)) continue;
		
		if (shi->mat->mtex[tex_nr]) {
			mtex= shi->mat->mtex[tex_nr];
			tex= mtex->tex;
			if (tex==0) continue;
			
			/* only process if this texture is mapped 
			 * to one that we're interested in */
			if (!(mtex->mapto & mapto_flag)) continue;
			
			/* which coords */
			if (mtex->texco==TEXCO_OBJECT) {
				Object *ob= mtex->object;
				if (ob) {
					copy_v3_v3(co, xyz);
					if (mtex->texflag & MTEX_OB_DUPLI_ORIG) {
						if (shi->obi && shi->obi->duplitexmat)
							mul_m4_v3(shi->obi->duplitexmat, co);					
					} 
					mul_m4_v3(ob->imat_ren, co);
				}
			}
			/* not really orco, but 'local' */
			else if (mtex->texco==TEXCO_ORCO) {
				
				if (mtex->texflag & MTEX_DUPLI_MAPTO) {
					copy_v3_v3(co, shi->duplilo);
				}
				else {
					Object *ob= shi->obi->ob;
					copy_v3_v3(co, xyz);
					mul_m4_v3(ob->imat_ren, co);
				}
			}
			else if (mtex->texco==TEXCO_GLOB) {
				copy_v3_v3(co, xyz);
				mul_m4_v3(re->viewinv, co);
			}
			else continue;	// can happen when texco defines disappear and it renders old files

			texres.nor= NULL;
			
			if (tex->type==TEX_IMAGE) {
				continue;	/* not supported yet */				
				//do_2d_mapping(mtex, texvec, NULL, NULL, dxt, dyt);
			}
			else {
				/* placement */
				if (mtex->projx) texvec[0]= mtex->size[0]*(co[mtex->projx-1]+mtex->ofs[0]);
				else texvec[0]= mtex->size[0]*(mtex->ofs[0]);

				if (mtex->projy) texvec[1]= mtex->size[1]*(co[mtex->projy-1]+mtex->ofs[1]);
				else texvec[1]= mtex->size[1]*(mtex->ofs[1]);

				if (mtex->projz) texvec[2]= mtex->size[2]*(co[mtex->projz-1]+mtex->ofs[2]);
				else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
			}
			
			rgbnor= multitex(tex, texvec, NULL, NULL, 0, &texres, 0, mtex->which_output);	/* NULL = dxt/dyt, 0 = shi->osatex - not supported */
			
			/* texture output */

			if ( (rgbnor & TEX_RGB) && (mtex->texflag & MTEX_RGBTOINT)) {
				texres.tin= (0.35f*texres.tr+0.45f*texres.tg+0.2f*texres.tb);
				rgbnor-= TEX_RGB;
			}
			if (mtex->texflag & MTEX_NEGATIVE) {
				if (rgbnor & TEX_RGB) {
					texres.tr= 1.0f-texres.tr;
					texres.tg= 1.0f-texres.tg;
					texres.tb= 1.0f-texres.tb;
				}
				texres.tin= 1.0f-texres.tin;
			}
			if (mtex->texflag & MTEX_STENCIL) {
				if (rgbnor & TEX_RGB) {
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
			
			
			if ((mapto_flag & (MAP_EMISSION_COL+MAP_TRANSMISSION_COL+MAP_REFLECTION_COL)) && (mtex->mapto & (MAP_EMISSION_COL+MAP_TRANSMISSION_COL+MAP_REFLECTION_COL))) {
				float tcol[3];
				
				/* stencil maps on the texture control slider, not texture intensity value */
				
				if ((rgbnor & TEX_RGB)==0) {
					tcol[0]= mtex->r;
					tcol[1]= mtex->g;
					tcol[2]= mtex->b;
				}
				else {
					tcol[0]=texres.tr;
					tcol[1]=texres.tg;
					tcol[2]=texres.tb;
					if (texres.talpha)
						texres.tin= texres.ta;
				}
				
				/* used for emit */
				if ((mapto_flag & MAP_EMISSION_COL) && (mtex->mapto & MAP_EMISSION_COL)) {
					float colemitfac= mtex->colemitfac*stencilTin;
					texture_rgb_blend(col, tcol, col, texres.tin, colemitfac, mtex->blendtype);
				}
				
				if ((mapto_flag & MAP_REFLECTION_COL) && (mtex->mapto & MAP_REFLECTION_COL)) {
					float colreflfac= mtex->colreflfac*stencilTin;
					texture_rgb_blend(col, tcol, col, texres.tin, colreflfac, mtex->blendtype);
				}
				
				if ((mapto_flag & MAP_TRANSMISSION_COL) && (mtex->mapto & MAP_TRANSMISSION_COL)) {
					float coltransfac= mtex->coltransfac*stencilTin;
					texture_rgb_blend(col, tcol, col, texres.tin, coltransfac, mtex->blendtype);
				}
			}
			
			if ((mapto_flag & MAP_VARS) && (mtex->mapto & MAP_VARS)) {
				/* stencil maps on the texture control slider, not texture intensity value */
				
				/* convert RGB to intensity if intensity info isn't provided */
				if (!(rgbnor & TEX_INT)) {
					if (rgbnor & TEX_RGB) {
						if (texres.talpha) texres.tin= texres.ta;
						else texres.tin= (0.35f*texres.tr+0.45f*texres.tg+0.2f*texres.tb);
					}
				}
				
				if ((mapto_flag & MAP_EMISSION) && (mtex->mapto & MAP_EMISSION)) {
					float emitfac= mtex->emitfac*stencilTin;

					*val = texture_value_blend(mtex->def_var, *val, texres.tin, emitfac, mtex->blendtype);
					if (*val<0.0f) *val= 0.0f;
				}
				if ((mapto_flag & MAP_DENSITY) && (mtex->mapto & MAP_DENSITY)) {
					float densfac= mtex->densfac*stencilTin;

					*val = texture_value_blend(mtex->def_var, *val, texres.tin, densfac, mtex->blendtype);
					CLAMP(*val, 0.0f, 1.0f);
				}
				if ((mapto_flag & MAP_SCATTERING) && (mtex->mapto & MAP_SCATTERING)) {
					float scatterfac= mtex->scatterfac*stencilTin;
					
					*val = texture_value_blend(mtex->def_var, *val, texres.tin, scatterfac, mtex->blendtype);
					CLAMP(*val, 0.0f, 1.0f);
				}
				if ((mapto_flag & MAP_REFLECTION) && (mtex->mapto & MAP_REFLECTION)) {
					float reflfac= mtex->reflfac*stencilTin;
					
					*val = texture_value_blend(mtex->def_var, *val, texres.tin, reflfac, mtex->blendtype);
					CLAMP(*val, 0.0f, 1.0f);
				}
			}
		}
	}
}


/* ------------------------------------------------------------------------- */

void do_halo_tex(HaloRen *har, float xn, float yn, float col_r[4])
{
	MTex *mtex;
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float texvec[3], dxt[3], dyt[3], fact, facm, dx;
	int rgb, osatex;

	if (R.r.scemode & R_NO_TEX) return;
	
	mtex= har->mat->mtex[0];
	if (har->mat->septex & (1<<0)) return;
	if (mtex->tex==NULL) return;
	
	/* no normal mapping */
	texres.nor= NULL;
		
	texvec[0]= xn/har->rad;
	texvec[1]= yn/har->rad;
	texvec[2]= 0.0;
	
	osatex= (har->mat->texco & TEXCO_OSA);

	/* placement */
	if (mtex->projx) texvec[0]= mtex->size[0]*(texvec[mtex->projx-1]+mtex->ofs[0]);
	else texvec[0]= mtex->size[0]*(mtex->ofs[0]);
	
	if (mtex->projy) texvec[1]= mtex->size[1]*(texvec[mtex->projy-1]+mtex->ofs[1]);
	else texvec[1]= mtex->size[1]*(mtex->ofs[1]);
	
	if (mtex->projz) texvec[2]= mtex->size[2]*(texvec[mtex->projz-1]+mtex->ofs[2]);
	else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
	
	if (osatex) {
	
		dx= 1.0f/har->rad;
	
		if (mtex->projx) {
			dxt[0]= mtex->size[0]*dx;
			dyt[0]= mtex->size[0]*dx;
		}
		else dxt[0]= dyt[0]= 0.0;
		
		if (mtex->projy) {
			dxt[1]= mtex->size[1]*dx;
			dyt[1]= mtex->size[1]*dx;
		}
		else dxt[1]= dyt[1]= 0.0;
		
		if (mtex->projz) {
			dxt[2]= 0.0;
			dyt[2]= 0.0;
		}
		else dxt[2]= dyt[2]= 0.0;

	}

	if (mtex->tex->type==TEX_IMAGE) do_2d_mapping(mtex, texvec, NULL, NULL, dxt, dyt);
	
	rgb= multitex(mtex->tex, texvec, dxt, dyt, osatex, &texres, 0, mtex->which_output);

	/* texture output */
	if (rgb && (mtex->texflag & MTEX_RGBTOINT)) {
		texres.tin= (0.35f*texres.tr+0.45f*texres.tg+0.2f*texres.tb);
		rgb= 0;
	}
	if (mtex->texflag & MTEX_NEGATIVE) {
		if (rgb) {
			texres.tr= 1.0f-texres.tr;
			texres.tg= 1.0f-texres.tg;
			texres.tb= 1.0f-texres.tb;
		}
		else texres.tin= 1.0f-texres.tin;
	}

	/* mapping */
	if (mtex->mapto & MAP_COL) {
		
		if (rgb==0) {
			texres.tr= mtex->r;
			texres.tg= mtex->g;
			texres.tb= mtex->b;
		}
		else if (mtex->mapto & MAP_ALPHA) {
			texres.tin= 1.0;
		}
		else texres.tin= texres.ta;

		/* inverse gamma correction */
		if (mtex->tex->type==TEX_IMAGE) {
			Image *ima = mtex->tex->ima;
			ImBuf *ibuf = BKE_image_get_ibuf(ima, &mtex->tex->iuser);
			
			/* don't linearize float buffers, assumed to be linear */
			if (ibuf && !(ibuf->rect_float) && R.r.color_mgt_flag & R_COLOR_MANAGEMENT)
				srgb_to_linearrgb_v3_v3(&texres.tr, &texres.tr);
		}

		fact= texres.tin*mtex->colfac;
		facm= 1.0f-fact;
		
		if (mtex->blendtype==MTEX_MUL) {
			facm= 1.0f-mtex->colfac;
		}
		
		if (mtex->blendtype==MTEX_SUB) fact= -fact;

		if (mtex->blendtype==MTEX_BLEND) {
			col_r[0]= (fact*texres.tr + facm*har->r);
			col_r[1]= (fact*texres.tg + facm*har->g);
			col_r[2]= (fact*texres.tb + facm*har->b);
		}
		else if (mtex->blendtype==MTEX_MUL) {
			col_r[0]= (facm+fact*texres.tr)*har->r;
			col_r[1]= (facm+fact*texres.tg)*har->g;
			col_r[2]= (facm+fact*texres.tb)*har->b;
		}
		else {
			col_r[0]= (fact*texres.tr + har->r);
			col_r[1]= (fact*texres.tg + har->g);
			col_r[2]= (fact*texres.tb + har->b);
			
			CLAMP(col_r[0], 0.0f, 1.0f);
			CLAMP(col_r[1], 0.0f, 1.0f);
			CLAMP(col_r[2], 0.0f, 1.0f);
		}
	}
	if (mtex->mapto & MAP_ALPHA) {
		if (rgb) {
			if (texres.talpha) texres.tin= texres.ta;
			else texres.tin= (0.35f*texres.tr+0.45f*texres.tg+0.2f*texres.tb);
		}
				
		col_r[3]*= texres.tin;
	}
}

/* ------------------------------------------------------------------------- */

/* hor and zen are RGB vectors, blend is 1 float, should all be initialized */
void do_sky_tex(const float rco[3], float lo[3], const float dxyview[2], float hor[3], float zen[3], float *blend, int skyflag, short thread)
{
	MTex *mtex;
	Tex *tex;
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float *co, fact, stencilTin=1.0;
	float tempvec[3], texvec[3], dxt[3], dyt[3];
	int tex_nr, rgb= 0;
	
	if (R.r.scemode & R_NO_TEX) return;
	/* todo: add flag to test if there's a tex */
	texres.nor= NULL;
	
	for (tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
		if (R.wrld.mtex[tex_nr]) {
			mtex= R.wrld.mtex[tex_nr];
			
			tex= mtex->tex;
			if (tex==0) continue;
			/* if (mtex->mapto==0) continue; */
			
			/* which coords */
			co= lo;
			
			/* dxt dyt just from 1 value */
			if (dxyview) {
				dxt[0]= dxt[1]= dxt[2]= dxyview[0];
				dyt[0]= dyt[1]= dyt[2]= dxyview[1];
			}
			else {
				dxt[0]= dxt[1]= dxt[2]= 0.0;
				dyt[0]= dyt[1]= dyt[2]= 0.0;
			}
			
			/* Grab the mapping settings for this texture */
			switch (mtex->texco) {
			case TEXCO_ANGMAP:
				/* only works with texture being "real" */
				/* use saacos(), fixes bug [#22398], float precision caused lo[2] to be slightly less then -1.0 */
				if (lo[0] || lo[1]) { /* check for zero case [#24807] */
					fact= (1.0f/(float)M_PI)*saacos(lo[2])/(sqrtf(lo[0]*lo[0] + lo[1]*lo[1]));
					tempvec[0]= lo[0]*fact;
					tempvec[1]= lo[1]*fact;
					tempvec[2]= 0.0;
				}
				else {
					/* this value has no angle, the vector is directly along the view.
					 * avoide divide by zero and use a dummy value. */
					tempvec[0]= 1.0f;
					tempvec[1]= 0.0;
					tempvec[2]= 0.0;
				}
				co= tempvec;
				break;
				
			case TEXCO_H_SPHEREMAP:
			case TEXCO_H_TUBEMAP:
				if (skyflag & WO_ZENUP) {
					if (mtex->texco==TEXCO_H_TUBEMAP) map_to_tube( tempvec, tempvec+1, lo[0], lo[2], lo[1]);
					else map_to_sphere( tempvec, tempvec+1, lo[0], lo[2], lo[1]);
					/* tube/spheremap maps for outside view, not inside */
					tempvec[0]= 1.0f-tempvec[0];
					/* only top half */
					tempvec[1]= 2.0f*tempvec[1]-1.0f;
					tempvec[2]= 0.0;
					/* and correction for do_2d_mapping */
					tempvec[0]= 2.0f*tempvec[0]-1.0f;
					tempvec[1]= 2.0f*tempvec[1]-1.0f;
					co= tempvec;
				}
				else {
					/* potentially dangerous... check with multitex! */
					continue;
				}
				break;
			case TEXCO_EQUIRECTMAP:
				tempvec[0]= atan2f(lo[0], lo[2]) / (float)M_PI;
				tempvec[1]= 1.0f - 2.0f*saacos(lo[1]) / (float)M_PI;
				tempvec[2]= 0.0f;
				co= tempvec;
				break;
			case TEXCO_OBJECT:
				if (mtex->object) {
					copy_v3_v3(tempvec, lo);
					mul_m4_v3(mtex->object->imat_ren, tempvec);
					co= tempvec;
				}
				break;
				
			case TEXCO_GLOB:
				if (rco) {
					copy_v3_v3(tempvec, rco);
					mul_m4_v3(R.viewinv, tempvec);
					co= tempvec;
				}
				else
					co= lo;
				
//				copy_v3_v3(shi->dxgl, shi->dxco);
//				mul_m3_v3(R.imat, shi->dxco);
//				copy_v3_v3(shi->dygl, shi->dyco);
//				mul_m3_v3(R.imat, shi->dyco);
				break;
			}
			
			/* placement */			
			if (mtex->projx) texvec[0]= mtex->size[0]*(co[mtex->projx-1]+mtex->ofs[0]);
			else texvec[0]= mtex->size[0]*(mtex->ofs[0]);
			
			if (mtex->projy) texvec[1]= mtex->size[1]*(co[mtex->projy-1]+mtex->ofs[1]);
			else texvec[1]= mtex->size[1]*(mtex->ofs[1]);
			
			if (mtex->projz) texvec[2]= mtex->size[2]*(co[mtex->projz-1]+mtex->ofs[2]);
			else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
			
			/* texture */
			if (tex->type==TEX_IMAGE) do_2d_mapping(mtex, texvec, NULL, NULL, dxt, dyt);
		
			rgb= multitex(mtex->tex, texvec, dxt, dyt, R.osa, &texres, thread, mtex->which_output);
			
			/* texture output */
			if (rgb && (mtex->texflag & MTEX_RGBTOINT)) {
				texres.tin= (0.35f*texres.tr+0.45f*texres.tg+0.2f*texres.tb);
				rgb= 0;
			}
			if (mtex->texflag & MTEX_NEGATIVE) {
				if (rgb) {
					texres.tr= 1.0f-texres.tr;
					texres.tg= 1.0f-texres.tg;
					texres.tb= 1.0f-texres.tb;
				}
				else texres.tin= 1.0f-texres.tin;
			}
			if (mtex->texflag & MTEX_STENCIL) {
				if (rgb) {
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
				if (rgb) texres.ta *= stencilTin;
				else texres.tin*= stencilTin;
			}
			
			/* color mapping */
			if (mtex->mapto & (WOMAP_HORIZ+WOMAP_ZENUP+WOMAP_ZENDOWN)) {
				float tcol[3];
				
				if (rgb==0) {
					texres.tr= mtex->r;
					texres.tg= mtex->g;
					texres.tb= mtex->b;
				}
				else texres.tin= texres.ta;
				
				tcol[0]= texres.tr; tcol[1]= texres.tg; tcol[2]= texres.tb;

				/* inverse gamma correction */
				if (tex->type==TEX_IMAGE) {
					Image *ima = tex->ima;
					ImBuf *ibuf = BKE_image_get_ibuf(ima, &tex->iuser);
					
					/* don't linearize float buffers, assumed to be linear */
					if (ibuf && !(ibuf->rect_float) && R.r.color_mgt_flag & R_COLOR_MANAGEMENT)
						srgb_to_linearrgb_v3_v3(tcol, tcol);
				}

				if (mtex->mapto & WOMAP_HORIZ) {
					texture_rgb_blend(hor, tcol, hor, texres.tin, mtex->colfac, mtex->blendtype);
				}
				if (mtex->mapto & (WOMAP_ZENUP+WOMAP_ZENDOWN)) {
					float zenfac = 0.0f;

					if (R.wrld.skytype & WO_SKYREAL) {
						if ((skyflag & WO_ZENUP)) {
							if (mtex->mapto & WOMAP_ZENUP) zenfac= mtex->zenupfac;
						}
						else if (mtex->mapto & WOMAP_ZENDOWN) zenfac= mtex->zendownfac;
					}
					else {
						if (mtex->mapto & WOMAP_ZENUP) zenfac= mtex->zenupfac;
						else if (mtex->mapto & WOMAP_ZENDOWN) zenfac= mtex->zendownfac;
					}
					
					if (zenfac != 0.0f)
						texture_rgb_blend(zen, tcol, zen, texres.tin, zenfac, mtex->blendtype);
				}
			}
			if (mtex->mapto & WOMAP_BLEND) {
				if (rgb) texres.tin= (0.35f*texres.tr+0.45f*texres.tg+0.2f*texres.tb);
				
				*blend= texture_value_blend(mtex->def_var, *blend, texres.tin, mtex->blendfac, mtex->blendtype);
			}
		}
	}
}

/* ------------------------------------------------------------------------- */
/* col_r supposed to be initialized with la->r,g,b */

void do_lamp_tex(LampRen *la, const float lavec[3], ShadeInput *shi, float col_r[3], int effect)
{
	Object *ob;
	MTex *mtex;
	Tex *tex;
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float *co = NULL, *dx = NULL, *dy = NULL, fact, stencilTin=1.0;
	float texvec[3], dxt[3], dyt[3], tempvec[3];
	int i, tex_nr, rgb= 0;
	
	if (R.r.scemode & R_NO_TEX) return;
	tex_nr= 0;
	
	for (; tex_nr<MAX_MTEX; tex_nr++) {
		
		if (la->mtex[tex_nr]) {
			mtex= la->mtex[tex_nr];
			
			tex= mtex->tex;
			if (tex==NULL) continue;
			texres.nor= NULL;
			
			/* which coords */
			if (mtex->texco==TEXCO_OBJECT) {
				ob= mtex->object;
				if (ob) {
					co= tempvec;
					dx= dxt;
					dy= dyt;
					copy_v3_v3(tempvec, shi->co);
					mul_m4_v3(ob->imat_ren, tempvec);
					if (shi->osatex) {
						copy_v3_v3(dxt, shi->dxco);
						copy_v3_v3(dyt, shi->dyco);
						mul_mat3_m4_v3(ob->imat_ren, dxt);
						mul_mat3_m4_v3(ob->imat_ren, dyt);
					}
				}
				else {
					co= shi->co;
					dx= shi->dxco; dy= shi->dyco;
				}
			}
			else if (mtex->texco==TEXCO_GLOB) {
				co= shi->gl; dx= shi->dxco; dy= shi->dyco;
				copy_v3_v3(shi->gl, shi->co);
				mul_m4_v3(R.viewinv, shi->gl);
			}
			else if (mtex->texco==TEXCO_VIEW) {
				
				copy_v3_v3(tempvec, lavec);
				mul_m3_v3(la->imat, tempvec);
				
				if (la->type==LA_SPOT) {
					tempvec[0]*= la->spottexfac;
					tempvec[1]*= la->spottexfac;
				/* project from 3d to 2d */
					tempvec[0] /= -tempvec[2];
					tempvec[1] /= -tempvec[2];
				}
				co= tempvec; 
				
				dx= dxt; dy= dyt;	
				if (shi->osatex) {
					copy_v3_v3(dxt, shi->dxlv);
					copy_v3_v3(dyt, shi->dylv);
					/* need some matrix conversion here? la->imat is a [3][3]  matrix!!! **/
					mul_m3_v3(la->imat, dxt);
					mul_m3_v3(la->imat, dyt);
					
					mul_v3_fl(dxt, la->spottexfac);
					mul_v3_fl(dyt, la->spottexfac);
				}
			}
			
			
			/* placement */
			if (mtex->projx && co) texvec[0]= mtex->size[0]*(co[mtex->projx-1]+mtex->ofs[0]);
			else texvec[0]= mtex->size[0]*(mtex->ofs[0]);
			
			if (mtex->projy && co) texvec[1]= mtex->size[1]*(co[mtex->projy-1]+mtex->ofs[1]);
			else texvec[1]= mtex->size[1]*(mtex->ofs[1]);
			
			if (mtex->projz && co) texvec[2]= mtex->size[2]*(co[mtex->projz-1]+mtex->ofs[2]);
			else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
			
			if (shi->osatex) {
				if (!dx) {
					for (i=0;i<2;i++) {
						dxt[i] = dyt[i] = 0.0;
					}
				}
				else {
					if (mtex->projx) {
						dxt[0]= mtex->size[0]*dx[mtex->projx-1];
						dyt[0]= mtex->size[0]*dy[mtex->projx-1];
					}
					else {
						dxt[0]= 0.0;
						dyt[0]= 0.0;
					}
					if (mtex->projy) {
						dxt[1]= mtex->size[1]*dx[mtex->projy-1];
						dyt[1]= mtex->size[1]*dy[mtex->projy-1];
					}
					else {
						dxt[1]= 0.0;
						dyt[1]= 0.0;
					}
					if (mtex->projz) {
						dxt[2]= mtex->size[2]*dx[mtex->projz-1];
						dyt[2]= mtex->size[2]*dy[mtex->projz-1];
					}
					else {
						dxt[2]= 0.0;
						dyt[2]= 0.0;
					}
				}
			}
			
			/* texture */
			if (tex->type==TEX_IMAGE) {
				do_2d_mapping(mtex, texvec, NULL, NULL, dxt, dyt);
			}
			
			rgb= multitex(tex, texvec, dxt, dyt, shi->osatex, &texres, shi->thread, mtex->which_output);

			/* texture output */
			if (rgb && (mtex->texflag & MTEX_RGBTOINT)) {
				texres.tin= (0.35f*texres.tr+0.45f*texres.tg+0.2f*texres.tb);
				rgb= 0;
			}
			if (mtex->texflag & MTEX_NEGATIVE) {
				if (rgb) {
					texres.tr= 1.0f-texres.tr;
					texres.tg= 1.0f-texres.tg;
					texres.tb= 1.0f-texres.tb;
				}
				else texres.tin= 1.0f-texres.tin;
			}
			if (mtex->texflag & MTEX_STENCIL) {
				if (rgb) {
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
				if (rgb) texres.ta*= stencilTin;
				else texres.tin*= stencilTin;
			}
			
			/* mapping */
			if (((mtex->mapto & LAMAP_COL) && (effect & LA_TEXTURE))||((mtex->mapto & LAMAP_SHAD) && (effect & LA_SHAD_TEX))) {
				float col[3];
				
				if (rgb==0) {
					texres.tr= mtex->r;
					texres.tg= mtex->g;
					texres.tb= mtex->b;
				}
				else if (mtex->mapto & MAP_ALPHA) {
					texres.tin= stencilTin;
				}
				else texres.tin= texres.ta;

				/* inverse gamma correction */
				if (tex->type==TEX_IMAGE) {
					Image *ima = tex->ima;
					ImBuf *ibuf = BKE_image_get_ibuf(ima, &tex->iuser);
					
					/* don't linearize float buffers, assumed to be linear */
					if (ibuf && !(ibuf->rect_float) && R.r.color_mgt_flag & R_COLOR_MANAGEMENT)
						srgb_to_linearrgb_v3_v3(&texres.tr, &texres.tr);
				}

				/* lamp colors were premultiplied with this */
				col[0]= texres.tr*la->energy;
				col[1]= texres.tg*la->energy;
				col[2]= texres.tb*la->energy;
				
				texture_rgb_blend(col_r, col, col_r, texres.tin, mtex->colfac, mtex->blendtype);
			}
		}
	}
}

/* ------------------------------------------------------------------------- */

int externtex(MTex *mtex, const float vec[3], float *tin, float *tr, float *tg, float *tb, float *ta, const int thread)
{
	Tex *tex;
	TexResult texr;
	float dxt[3], dyt[3], texvec[3];
	int rgb;
	
	tex= mtex->tex;
	if (tex==NULL) return 0;
	texr.nor= NULL;
	
	/* placement */
	if (mtex->projx) texvec[0]= mtex->size[0]*(vec[mtex->projx-1]+mtex->ofs[0]);
	else texvec[0]= mtex->size[0]*(mtex->ofs[0]);
	
	if (mtex->projy) texvec[1]= mtex->size[1]*(vec[mtex->projy-1]+mtex->ofs[1]);
	else texvec[1]= mtex->size[1]*(mtex->ofs[1]);
	
	if (mtex->projz) texvec[2]= mtex->size[2]*(vec[mtex->projz-1]+mtex->ofs[2]);
	else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
	
	/* texture */
	if (tex->type==TEX_IMAGE) {
		do_2d_mapping(mtex, texvec, NULL, NULL, dxt, dyt);
	}
	
	rgb= multitex(tex, texvec, dxt, dyt, 0, &texr, thread, mtex->which_output);
	
	if (rgb) {
		texr.tin= (0.35f*texr.tr+0.45f*texr.tg+0.2f*texr.tb);
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

	if (R.r.scemode & R_NO_TEX) return;

	if (firsttime) {
		BLI_lock_thread(LOCK_IMAGE);
		if (firsttime) {
			for (a=0; a<BLENDER_MAX_THREADS; a++) {
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
	
	texvec[0]= 0.5f+0.5f*suv->uv[0];
	texvec[1]= 0.5f+0.5f*suv->uv[1];
	texvec[2] = 0.0f;  // initalize it because imagewrap looks at it.
	if (shi->osatex) {
		dx[0]= 0.5f*suv->dxuv[0];
		dx[1]= 0.5f*suv->dxuv[1];
		dy[0]= 0.5f*suv->dyuv[0];
		dy[1]= 0.5f*suv->dyuv[1];
	}
	
	texr.nor= NULL;
	
	if (shi->osatex) imagewraposa(tex, ima, NULL, texvec, dx, dy, &texr);
	else imagewrap(tex, ima, NULL, texvec, &texr); 

	shi->vcol[0]*= texr.tr;
	shi->vcol[1]*= texr.tg;
	shi->vcol[2]*= texr.tb;
	shi->vcol[3]*= texr.ta;
}

/* A modified part of shadeinput.c -> shade_input_set_uv()
 *  Used for sampling UV mapped texture color */
static void textured_face_generate_uv(float *uv, float *normal, float *hit, float *v1, float *v2, float *v3)
{

	float detsh, t00, t10, t01, t11;
	int axis1, axis2;

	/* find most stable axis to project */
	axis_dominant_v3(&axis1, &axis2, normal);

	/* compute u,v and derivatives */
	t00= v3[axis1]-v1[axis1]; t01= v3[axis2]-v1[axis2];
	t10= v3[axis1]-v2[axis1]; t11= v3[axis2]-v2[axis2];

	detsh= 1.0f/(t00*t11-t10*t01);
	t00*= detsh; t01*=detsh; 
	t10*=detsh; t11*=detsh;

	uv[0] = (hit[axis1]-v3[axis1])*t11-(hit[axis2]-v3[axis2])*t10;
	uv[1] = (hit[axis2]-v3[axis2])*t00-(hit[axis1]-v3[axis1])*t01;

	/* u and v are in range -1 to 0, we allow a little bit extra but not too much, screws up speedvectors */
	CLAMP(uv[0], -2.0f, 1.0f);
	CLAMP(uv[1], -2.0f, 1.0f);
}

/* Generate an updated copy of material to use for color sampling. */
Material *RE_init_sample_material(Material *orig_mat, Scene *scene)
{
	Tex *tex = NULL;
	Material *mat;
	int tex_nr;

	if (!orig_mat) return NULL;

	/* copy material */
	mat = localize_material(orig_mat);

	/* update material anims */
	BKE_animsys_evaluate_animdata(scene, &mat->id, mat->adt, BKE_curframe(scene), ADT_RECALC_ANIM);

	/* strip material copy from unsupported flags */
	for (tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
		if (mat->septex & (1<<tex_nr)) continue;
	
		if (mat->mtex[tex_nr]) {
			MTex *mtex = mat->mtex[tex_nr];

			if (!mtex->tex) continue;

			/* only keep compatible texflags */
			mtex->texflag = mtex->texflag & (MTEX_RGBTOINT | MTEX_STENCIL | MTEX_NEGATIVE | MTEX_ALPHAMIX);

			/* depending of material type, strip non-compatible mapping modes */
			if (mat->material_type == MA_TYPE_SURFACE) {
				if (!ELEM4(mtex->texco, TEXCO_ORCO, TEXCO_OBJECT, TEXCO_GLOB, TEXCO_UV)) {
					/* ignore this texture */
					mtex->texco = 0;
					continue;
				}
				/* strip all mapto flags except color and alpha */
				mtex->mapto = (mtex->mapto & MAP_COL) | (mtex->mapto & MAP_ALPHA);
			}
			else if (mat->material_type == MA_TYPE_VOLUME) {
				if (!ELEM3(mtex->texco, TEXCO_OBJECT, TEXCO_ORCO, TEXCO_GLOB)) {
					/* ignore */
					mtex->texco = 0;
					continue;
				}
				/* strip all mapto flags except color and alpha */
				mtex->mapto = mtex->mapto & (MAP_TRANSMISSION_COL | MAP_REFLECTION_COL | MAP_DENSITY);
			}
			
			/* if mapped to an object, calculate inverse matrices */
			if (mtex->texco==TEXCO_OBJECT) {
				Object *ob= mtex->object;
				if (ob) {
					invert_m4_m4(ob->imat, ob->obmat);
					copy_m4_m4(ob->imat_ren, ob->imat);
				}
			}

			/* copy texture */
			tex= mtex->tex = localize_texture(mtex->tex);

			/* update texture anims */
			BKE_animsys_evaluate_animdata(scene, &tex->id, tex->adt, BKE_curframe(scene), ADT_RECALC_ANIM);

			/* update texture cache if required */
			if (tex->type==TEX_VOXELDATA) {
				cache_voxeldata(tex, (int)scene->r.cfra);
			}
			if (tex->type==TEX_POINTDENSITY) {
				/* set dummy values for render and do cache */
				Render dummy_re = {0};
				dummy_re.scene = scene;
				unit_m4(dummy_re.viewinv);
				unit_m4(dummy_re.viewmat);
				unit_m4(dummy_re.winmat);
				dummy_re.winx = dummy_re.winy = 128;
				cache_pointdensity(&dummy_re, tex);
			}

			/* update image sequences and movies */
			if (tex->ima && ELEM(tex->ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
				if (tex->iuser.flag & IMA_ANIM_ALWAYS)
					BKE_image_user_calc_frame(&tex->iuser, (int)scene->r.cfra, 0);
			}
		}
	}
	return mat;
}

/* free all duplicate data allocated by RE_init_sample_material() */
void RE_free_sample_material(Material *mat)
{
	int tex_nr;

	/* free textures */
	for (tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
		if (mat->septex & (1<<tex_nr)) continue;
		if (mat->mtex[tex_nr]) {
			MTex *mtex= mat->mtex[tex_nr];
	
			if (mtex->tex) {
				free_texture(mtex->tex);
				MEM_freeN(mtex->tex);
				mtex->tex = NULL;
			}
		}
	}

	free_material(mat);
	MEM_freeN(mat);
}



/*
 *	Get material diffuse color and alpha (including linked textures) in given coordinates
 *
 *	color,alpha : input/output color values
 *	volume_co : sample coordinate in global space. used by volumetric materials
 *	surface_co : sample surface coordinate in global space. used by "surface" materials
 *	face_index : surface face index
 *	hit_quad : whether point is on second "half" of a quad
 *	orcoDm : orco state derived mesh
 */
void RE_sample_material_color(Material *mat, float color[3], float *alpha, const float volume_co[3], const float surface_co[3], int face_index, short hit_quad, DerivedMesh *orcoDm, Object *ob)
{
	MFace *mface;
	int v1, v2, v3;
	MVert *mvert;
	float uv[3], normal[3];
	ShadeInput shi = {0};
	Render re = {0};

	/* Get face data	*/
	mvert = orcoDm->getVertArray(orcoDm);
	mface = orcoDm->getTessFaceArray(orcoDm);

	if (!mvert || !mface || !mat) return;
	v1=mface[face_index].v1, v2=mface[face_index].v2, v3=mface[face_index].v3;
	if (hit_quad) {v2=mface[face_index].v3; v3=mface[face_index].v4;}
	normal_tri_v3(normal, mvert[v1].co, mvert[v2].co, mvert[v3].co);

	/* generate shadeinput with data required */
	shi.mat = mat;

	/* fill shadeinput data depending on material type */
	if (mat->material_type == MA_TYPE_SURFACE) {
		/* global coordinates */
		copy_v3_v3(shi.gl, surface_co);
		/* object space coordinates */
		copy_v3_v3(shi.co, surface_co);
		mul_m4_v3(ob->imat, shi.co);
		/* orco coordinates */
		{
			float l;
			/* Get generated UV */
			textured_face_generate_uv(uv, normal, shi.co, mvert[v1].co, mvert[v2].co, mvert[v3].co);
			l= 1.0f+uv[0]+uv[1];

			/* calculate generated coordinate */
			shi.lo[0]= l*mvert[v3].co[0]-uv[0]*mvert[v1].co[0]-uv[1]*mvert[v2].co[0];
			shi.lo[1]= l*mvert[v3].co[1]-uv[0]*mvert[v1].co[1]-uv[1]*mvert[v2].co[1];
			shi.lo[2]= l*mvert[v3].co[2]-uv[0]*mvert[v1].co[2]-uv[1]*mvert[v2].co[2];
		}
		/* uv coordinates */
		{
			int i, layers = CustomData_number_of_layers(&orcoDm->faceData, CD_MTFACE);
			int layer_index = CustomData_get_layer_index(&orcoDm->faceData, CD_MTFACE);

			/* for every uv map set coords and name */
			for (i=0; i<layers; i++) {
				if (layer_index >= 0) {
					float *uv1, *uv2, *uv3;
					float l;
					CustomData *data = &orcoDm->faceData;
					MTFace *tface = (MTFace*) data->layers[layer_index+i].data;
					float uv[3];
					/* point layer name from actual layer data */
					shi.uv[i].name = data->layers[i].name;
					/* Get generated coordinates to calculate UV from */
					textured_face_generate_uv(uv, normal, shi.co, mvert[v1].co, mvert[v2].co, mvert[v3].co);
					/* Get UV mapping coordinate */
					l= 1.0f+uv[0]+uv[1];
						
					uv1= tface[face_index].uv[0];
					uv2= (hit_quad) ? tface[face_index].uv[2] : tface[face_index].uv[1];
					uv3= (hit_quad) ? tface[face_index].uv[3] : tface[face_index].uv[2];
								
					shi.uv[i].uv[0]= -1.0f + 2.0f*(l*uv3[0]-uv[0]*uv1[0]-uv[1]*uv2[0]);
					shi.uv[i].uv[1]= -1.0f + 2.0f*(l*uv3[1]-uv[0]*uv1[1]-uv[1]*uv2[1]);
					shi.uv[i].uv[2]= 0.0f;	/* texture.c assumes there are 3 coords */
				}
			}
			/* active uv map */
			shi.actuv = CustomData_get_active_layer_index(&orcoDm->faceData, CD_MTFACE) - layer_index;
			shi.totuv = layers;
		}

		/* apply initial values from material */
		shi.r = mat->r;
		shi.g = mat->g;
		shi.b = mat->b;
		shi.alpha = mat->alpha;

		/* do texture */
		do_material_tex(&shi, &re);

		/* apply result	*/
		color[0] = shi.r;
		color[1] = shi.g;
		color[2] = shi.b;
		*alpha = shi.alpha;
	}
	else if (mat->material_type == MA_TYPE_VOLUME) {
		ObjectInstanceRen obi = {0};
		obi.ob = ob;
		shi.obi = &obi;
		unit_m4(re.viewinv);
		copy_v3_v3(color, mat->vol.reflection_col);
		*alpha = mat->vol.density;

		/* do texture */
		do_volume_tex(&shi, volume_co, (MAP_TRANSMISSION_COL | MAP_REFLECTION_COL | MAP_DENSITY),
		              color, alpha, &re);
	}
}

/* eof */
