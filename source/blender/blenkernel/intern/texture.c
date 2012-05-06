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

/** \file blender/blenkernel/intern/texture.c
 *  \ingroup bke
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_dynlib.h"
#include "BLI_math.h"
#include "BLI_kdopbvh.h"
#include "BLI_utildefines.h"
#include "BLI_bpath.h"

#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_world_types.h"
#include "DNA_brush_types.h"
#include "DNA_node_types.h"
#include "DNA_color_types.h"
#include "DNA_particle_types.h"

#include "IMB_imbuf.h"

#include "BKE_plugin_types.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_ocean.h"

#include "BKE_library.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_key.h"
#include "BKE_icons.h"
#include "BKE_node.h"
#include "BKE_animsys.h"
#include "BKE_colortools.h"


/* ------------------------------------------------------------------------- */

/* All support for plugin textures: */
int test_dlerr(const char *name, const char *symbol)
{
	char *err;
	
	err= BLI_dynlib_get_error_as_string(NULL);
	if (err) {
		printf("var1: %s, var2: %s, var3: %s\n", name, symbol, err);
		return 1;
	}
	
	return 0;
}

/* ------------------------------------------------------------------------- */

void open_plugin_tex(PluginTex *pit)
{
	int (*version)(void);
	
	/* init all the happy variables */
	pit->doit= NULL;
	pit->pname= NULL;
	pit->stnames= NULL;
	pit->varstr= NULL;
	pit->result= NULL;
	pit->cfra= NULL;
	pit->version= 0;
	pit->instance_init= NULL;
	
	/* clear the error list */
	BLI_dynlib_get_error_as_string(NULL);

	/* no BLI_dynlib_close! multiple opened plugins... */
	/* if (pit->handle) BLI_dynlib_close(pit->handle); */
	/* pit->handle= 0; */

	/* open the needed object */
	pit->handle= BLI_dynlib_open(pit->name);
	if (test_dlerr(pit->name, pit->name)) return;

	if (pit->handle != NULL) {
		/* find the address of the version function */
		version= (int (*)(void)) BLI_dynlib_find_symbol(pit->handle, "plugin_tex_getversion");
		if (test_dlerr(pit->name, "plugin_tex_getversion")) return;
		
		if (version != NULL) {
			pit->version= version();
			if ( pit->version >= 2 && pit->version <=6) {
				int (*info_func)(PluginInfo *);
				PluginInfo *info= (PluginInfo*) MEM_mallocN(sizeof(PluginInfo), "plugin_info"); 

				info_func= (int (*)(PluginInfo *))BLI_dynlib_find_symbol(pit->handle, "plugin_getinfo");
				if (!test_dlerr(pit->name, "plugin_getinfo")) {
					info->instance_init = NULL;

					info_func(info);

					pit->doit= (int(*)(void)) info->tex_doit;
					pit->callback= (void(*)(unsigned short)) info->callback;
					pit->stypes= info->stypes;
					pit->vars= info->nvars;
					pit->pname= info->name;
					pit->stnames= info->snames;
					pit->varstr= info->varstr;
					pit->result= info->result;
					pit->cfra= info->cfra;
					pit->instance_init = info->instance_init;
					if (info->init) info->init();
				}
				MEM_freeN(info);
			}
			else {
				printf ("Plugin returned unrecognized version number\n");
				return;
			}
		}
	}
}

/* ------------------------------------------------------------------------- */

/* very badlevel define to bypass linking with BIF_interface.h */
#define INT	96
#define FLO	128

PluginTex *add_plugin_tex(char *str)
{
	PluginTex *pit;
	VarStruct *varstr;
	int a;
	
	pit= MEM_callocN(sizeof(PluginTex), "plugintex");
	
	BLI_strncpy(pit->name, str, sizeof(pit->name));
	open_plugin_tex(pit);
	
	if (pit->doit==NULL) {
		if (pit->handle==NULL) {;} //XXX error("no plugin: %s", str);
		else {;} //XXX error("in plugin: %s", str);
		MEM_freeN(pit);
		return NULL;
	}
	
	varstr= pit->varstr;
	for (a=0; a<pit->vars; a++, varstr++) {
		if ( (varstr->type & FLO)==FLO)
			pit->data[a]= varstr->def;
		else if ( (varstr->type & INT)==INT)
			*((int *)(pit->data+a))= (int) varstr->def;
	}

	if (pit->instance_init)
		pit->instance_init((void *) pit->data);

	return pit;
}

/* ------------------------------------------------------------------------- */

void free_plugin_tex(PluginTex *pit)
{
	if (pit==NULL) return;
		
	/* no BLI_dynlib_close: same plugin can be opened multiple times, 1 handle */
	MEM_freeN(pit);	
}

/* ****************** Mapping ******************* */

TexMapping *add_tex_mapping(void)
{
	TexMapping *texmap= MEM_callocN(sizeof(TexMapping), "TexMapping");
	
	default_tex_mapping(texmap);
	
	return texmap;
}

void default_tex_mapping(TexMapping *texmap)
{
	memset(texmap, 0, sizeof(TexMapping));

	texmap->size[0]= texmap->size[1]= texmap->size[2]= 1.0f;
	texmap->max[0]= texmap->max[1]= texmap->max[2]= 1.0f;
	unit_m4(texmap->mat);

	texmap->projx= PROJ_X;
	texmap->projy= PROJ_Y;
	texmap->projz= PROJ_Z;
	texmap->mapping= MTEX_FLAT;
}

void init_tex_mapping(TexMapping *texmap)
{
	float smat[3][3], rmat[3][3], mat[3][3], proj[3][3];

	if (texmap->projx == PROJ_X && texmap->projy == PROJ_Y && texmap->projz == PROJ_Z &&
	   is_zero_v3(texmap->loc) && is_zero_v3(texmap->rot) && is_one_v3(texmap->size)) {
		unit_m4(texmap->mat);

		texmap->flag |= TEXMAP_UNIT_MATRIX;
	}
	else {
		/* axis projection */
		zero_m3(proj);

		if (texmap->projx != PROJ_N)
			proj[texmap->projx-1][0]= 1.0f;
		if (texmap->projy != PROJ_N)
			proj[texmap->projy-1][1]= 1.0f;
		if (texmap->projz != PROJ_N)
			proj[texmap->projz-1][2]= 1.0f;

		/* scale */
		size_to_mat3(smat, texmap->size);
		
		/* rotation */
		/* TexMapping rotation are now in radians. */
		eul_to_mat3(rmat, texmap->rot);
		
		/* compose it all */
		mul_m3_m3m3(mat, rmat, smat);
		mul_m3_m3m3(mat, proj, mat);
		
		/* translation */
		copy_m4_m3(texmap->mat, mat);
		copy_v3_v3(texmap->mat[3], texmap->loc);

		texmap->flag &= ~TEXMAP_UNIT_MATRIX;
	}
}

ColorMapping *add_color_mapping(void)
{
	ColorMapping *colormap= MEM_callocN(sizeof(ColorMapping), "ColorMapping");
	
	default_color_mapping(colormap);
	
	return colormap;
}

void default_color_mapping(ColorMapping *colormap)
{
	memset(colormap, 0, sizeof(ColorMapping));

	init_colorband(&colormap->coba, 1);

	colormap->bright= 1.0;
	colormap->contrast= 1.0;
	colormap->saturation= 1.0;

	colormap->blend_color[0]= 0.8f;
	colormap->blend_color[1]= 0.8f;
	colormap->blend_color[2]= 0.8f;
	colormap->blend_type= MA_RAMP_BLEND;
	colormap->blend_factor= 0.0f;
}

/* ****************** COLORBAND ******************* */

void init_colorband(ColorBand *coba, int rangetype)
{
	int a;
	
	coba->data[0].pos= 0.0;
	coba->data[1].pos= 1.0;
	
	if (rangetype==0) {
		coba->data[0].r= 0.0;
		coba->data[0].g= 0.0;
		coba->data[0].b= 0.0;
		coba->data[0].a= 0.0;
		
		coba->data[1].r= 1.0;
		coba->data[1].g= 1.0;
		coba->data[1].b= 1.0;
		coba->data[1].a= 1.0;
	}
	else {
		coba->data[0].r= 0.0;
		coba->data[0].g= 0.0;
		coba->data[0].b= 0.0;
		coba->data[0].a= 1.0;
		
		coba->data[1].r= 1.0;
		coba->data[1].g= 1.0;
		coba->data[1].b= 1.0;
		coba->data[1].a= 1.0;
	}
	
	for (a=2; a<MAXCOLORBAND; a++) {
		coba->data[a].r= 0.5;
		coba->data[a].g= 0.5;
		coba->data[a].b= 0.5;
		coba->data[a].a= 1.0;
		coba->data[a].pos= 0.5;
	}
	
	coba->tot= 2;
	
}

ColorBand *add_colorband(int rangetype)
{
	ColorBand *coba;
	
	coba= MEM_callocN(sizeof(ColorBand), "colorband");
	init_colorband(coba, rangetype);
	
	return coba;
}

/* ------------------------------------------------------------------------- */

int do_colorband(const ColorBand *coba, float in, float out[4])
{
	const CBData *cbd1, *cbd2, *cbd0, *cbd3;
	float fac, mfac, t[4];
	int a;
	
	if (coba==NULL || coba->tot==0) return 0;
	
	cbd1= coba->data;
	if (coba->tot==1) {
		out[0]= cbd1->r;
		out[1]= cbd1->g;
		out[2]= cbd1->b;
		out[3]= cbd1->a;
	}
	else {
		if (in <= cbd1->pos && coba->ipotype<2) {
			out[0]= cbd1->r;
			out[1]= cbd1->g;
			out[2]= cbd1->b;
			out[3]= cbd1->a;
		}
		else {
			CBData left, right;
			
			/* we're looking for first pos > in */
			for (a=0; a<coba->tot; a++, cbd1++) if (cbd1->pos > in) break;
				
			if (a==coba->tot) {
				cbd2= cbd1-1;
				right= *cbd2;
				right.pos= 1.0f;
				cbd1= &right;
			}
			else if (a==0) {
				left= *cbd1;
				left.pos= 0.0f;
				cbd2= &left;
			}
			else cbd2= cbd1-1;
			
			if (in >= cbd1->pos && coba->ipotype<2) {
				out[0]= cbd1->r;
				out[1]= cbd1->g;
				out[2]= cbd1->b;
				out[3]= cbd1->a;
			}
			else {
		
				if (cbd2->pos!=cbd1->pos)
					fac= (in-cbd1->pos)/(cbd2->pos-cbd1->pos);
				else {
					/* was setting to 0.0 in 2.56 & previous, but this
					 * is incorrect for the last element, see [#26732] */
					fac= (a != coba->tot) ? 0.0f : 1.0f;
				}
				
				if (coba->ipotype==4) {
					/* constant */
					out[0]= cbd2->r;
					out[1]= cbd2->g;
					out[2]= cbd2->b;
					out[3]= cbd2->a;
					return 1;
				}
				
				if (coba->ipotype>=2) {
					/* ipo from right to left: 3 2 1 0 */
					
					if (a>=coba->tot-1) cbd0= cbd1;
					else cbd0= cbd1+1;
					if (a<2) cbd3= cbd2;
					else cbd3= cbd2-1;
					
					CLAMP(fac, 0.0f, 1.0f);
					
					if (coba->ipotype==3)
						key_curve_position_weights(fac, t, KEY_CARDINAL);
					else
						key_curve_position_weights(fac, t, KEY_BSPLINE);

					out[0]= t[3]*cbd3->r +t[2]*cbd2->r +t[1]*cbd1->r +t[0]*cbd0->r;
					out[1]= t[3]*cbd3->g +t[2]*cbd2->g +t[1]*cbd1->g +t[0]*cbd0->g;
					out[2]= t[3]*cbd3->b +t[2]*cbd2->b +t[1]*cbd1->b +t[0]*cbd0->b;
					out[3]= t[3]*cbd3->a +t[2]*cbd2->a +t[1]*cbd1->a +t[0]*cbd0->a;
					CLAMP(out[0], 0.0f, 1.0f);
					CLAMP(out[1], 0.0f, 1.0f);
					CLAMP(out[2], 0.0f, 1.0f);
					CLAMP(out[3], 0.0f, 1.0f);
				}
				else {
				
					if (coba->ipotype==1) {	/* EASE */
						mfac= fac*fac;
						fac= 3.0f*mfac-2.0f*mfac*fac;
					}
					mfac= 1.0f-fac;
					
					out[0]= mfac*cbd1->r + fac*cbd2->r;
					out[1]= mfac*cbd1->g + fac*cbd2->g;
					out[2]= mfac*cbd1->b + fac*cbd2->b;
					out[3]= mfac*cbd1->a + fac*cbd2->a;
				}
			}
		}
	}
	return 1;	/* OK */
}

void colorband_table_RGBA(ColorBand *coba, float **array, int *size)
{
	int a;
	
	*size = CM_TABLE+1;
	*array = MEM_callocN(sizeof(float)*(*size)*4, "ColorBand");

	for (a=0; a<*size; a++)
		do_colorband(coba, (float)a/(float)CM_TABLE, &(*array)[a*4]);
}

int vergcband(const void *a1, const void *a2)
{
	const CBData *x1=a1, *x2=a2;

	if ( x1->pos > x2->pos ) return 1;
	else if ( x1->pos < x2->pos) return -1;
	return 0;
}

void colorband_update_sort(ColorBand *coba)
{
	int a;
	
	if (coba->tot<2)
		return;
	
	for (a=0; a<coba->tot; a++)
		coba->data[a].cur= a;

	qsort(coba->data, coba->tot, sizeof(CBData), vergcband);

	for (a=0; a<coba->tot; a++) {
		if (coba->data[a].cur==coba->cur) {
			coba->cur= a;
			break;
		}
	}
}

CBData *colorband_element_add(struct ColorBand *coba, float position)
{
	if (coba->tot==MAXCOLORBAND) {
		return NULL;
	}
	else if (coba->tot > 0) {
		CBData *xnew;
		float col[4];

		do_colorband(coba, position, col);

		xnew = &coba->data[coba->tot];
		xnew->pos = position;

		xnew->r = col[0];
		xnew->g = col[1];
		xnew->b = col[2];
		xnew->a = col[3];
	}

	coba->tot++;
	coba->cur = coba->tot-1;

	colorband_update_sort(coba);

	return coba->data + coba->cur;
}

int colorband_element_remove(struct ColorBand *coba, int index)
{
	int a;

	if (coba->tot < 2)
		return 0;

	if (index < 0 || index >= coba->tot)
		return 0;

	for (a = index; a < coba->tot; a++) {
		coba->data[a] = coba->data[a + 1];
	}
	if (coba->cur) coba->cur--;
	coba->tot--;
	return 1;
}

/* ******************* TEX ************************ */

void free_texture(Tex *tex)
{
	free_plugin_tex(tex->plugin);
	
	if (tex->coba) MEM_freeN(tex->coba);
	if (tex->env) BKE_free_envmap(tex->env);
	if (tex->pd) BKE_free_pointdensity(tex->pd);
	if (tex->vd) BKE_free_voxeldata(tex->vd);
	if (tex->ot) BKE_free_oceantex(tex->ot);
	BKE_free_animdata((struct ID *)tex);
	
	BKE_previewimg_free(&tex->preview);
	BKE_icon_delete((struct ID*)tex);
	tex->id.icon_id = 0;
	
	if (tex->nodetree) {
		ntreeFreeTree(tex->nodetree);
		MEM_freeN(tex->nodetree);
	}
}

/* ------------------------------------------------------------------------- */

void default_tex(Tex *tex)
{
	PluginTex *pit;
	VarStruct *varstr;
	int a;

	tex->type= TEX_CLOUDS;
	tex->stype= 0;
	tex->flag= TEX_CHECKER_ODD;
	tex->imaflag= TEX_INTERPOL|TEX_MIPMAP|TEX_USEALPHA;
	tex->extend= TEX_REPEAT;
	tex->cropxmin= tex->cropymin= 0.0;
	tex->cropxmax= tex->cropymax= 1.0;
	tex->texfilter = TXF_EWA;
	tex->afmax = 8;
	tex->xrepeat= tex->yrepeat= 1;
	tex->fie_ima= 2;
	tex->sfra= 1;
	tex->frames= 0;
	tex->offset= 0;
	tex->noisesize= 0.25;
	tex->noisedepth= 2;
	tex->turbul= 5.0;
	tex->nabla= 0.025;	// also in do_versions
	tex->bright= 1.0;
	tex->contrast= 1.0;
	tex->saturation= 1.0;
	tex->filtersize= 1.0;
	tex->rfac= 1.0;
	tex->gfac= 1.0;
	tex->bfac= 1.0;
	/* newnoise: init. */
	tex->noisebasis = 0;
	tex->noisebasis2 = 0;
	/* musgrave */
	tex->mg_H = 1.0;
	tex->mg_lacunarity = 2.0;
	tex->mg_octaves = 2.0;
	tex->mg_offset = 1.0;
	tex->mg_gain = 1.0;
	tex->ns_outscale = 1.0;
	/* distnoise */
	tex->dist_amount = 1.0;
	/* voronoi */
	tex->vn_w1 = 1.0;
	tex->vn_w2 = tex->vn_w3 = tex->vn_w4 = 0.0;
	tex->vn_mexp = 2.5;
	tex->vn_distm = 0;
	tex->vn_coltype = 0;

	if (tex->env) {
		tex->env->stype=ENV_ANIM;
		tex->env->clipsta=0.1;
		tex->env->clipend=100;
		tex->env->cuberes=600;
		tex->env->depth=0;
	}

	if (tex->pd) {
		tex->pd->radius = 0.3f;
		tex->pd->falloff_type = TEX_PD_FALLOFF_STD;
	}
	
	if (tex->vd) {
		tex->vd->resol[0] = tex->vd->resol[1] = tex->vd->resol[2] = 0;
		tex->vd->interp_type=TEX_VD_LINEAR;
		tex->vd->file_format=TEX_VD_SMOKE;
	}
	
	if (tex->ot) {
		tex->ot->output = TEX_OCN_DISPLACEMENT;
		tex->ot->object = NULL;
	}
	pit = tex->plugin;
	if (pit) {
		varstr= pit->varstr;
		if (varstr) {
			for (a=0; a<pit->vars; a++, varstr++) {
				pit->data[a] = varstr->def;
			}
		}
	}
	
	tex->iuser.fie_ima= 2;
	tex->iuser.ok= 1;
	tex->iuser.frames= 100;
	tex->iuser.sfra= 1;
	
	tex->preview = NULL;
}

void tex_set_type(Tex *tex, int type)
{
	switch (type) {
			
		case TEX_VOXELDATA:
			if (tex->vd == NULL)
				tex->vd = BKE_add_voxeldata();
			break;
		case TEX_POINTDENSITY:
			if (tex->pd == NULL)
				tex->pd = BKE_add_pointdensity();
			break;
		case TEX_ENVMAP:
			if (tex->env == NULL)
				tex->env = BKE_add_envmap();
			break;
		case TEX_OCEAN:
			if (tex->ot == NULL)
				tex->ot = BKE_add_oceantex();
			break;
	}
	
	tex->type = type;
}

/* ------------------------------------------------------------------------- */

Tex *add_texture(const char *name)
{
	Main *bmain= G.main;
	Tex *tex;

	tex= alloc_libblock(&bmain->tex, ID_TE, name);
	
	default_tex(tex);
	
	return tex;
}

/* ------------------------------------------------------------------------- */

void default_mtex(MTex *mtex)
{
	mtex->texco= TEXCO_ORCO;
	mtex->mapto= MAP_COL;
	mtex->object= NULL;
	mtex->projx= PROJ_X;
	mtex->projy= PROJ_Y;
	mtex->projz= PROJ_Z;
	mtex->mapping= MTEX_FLAT;
	mtex->ofs[0]= 0.0;
	mtex->ofs[1]= 0.0;
	mtex->ofs[2]= 0.0;
	mtex->size[0]= 1.0;
	mtex->size[1]= 1.0;
	mtex->size[2]= 1.0;
	mtex->tex= NULL;
	mtex->texflag= MTEX_3TAP_BUMP | MTEX_BUMP_OBJECTSPACE;
	mtex->colormodel= 0;
	mtex->r= 1.0;
	mtex->g= 0.0;
	mtex->b= 1.0;
	mtex->k= 1.0;
	mtex->def_var= 1.0;
	mtex->blendtype= MTEX_BLEND;
	mtex->colfac= 1.0;
	mtex->norfac= 1.0;
	mtex->varfac= 1.0;
	mtex->dispfac=0.2;
	mtex->colspecfac= 1.0f;
	mtex->mirrfac= 1.0f;
	mtex->alphafac= 1.0f;
	mtex->difffac= 1.0f;
	mtex->specfac= 1.0f;
	mtex->emitfac= 1.0f;
	mtex->hardfac= 1.0f;
	mtex->raymirrfac= 1.0f;
	mtex->translfac= 1.0f;
	mtex->ambfac= 1.0f;
	mtex->colemitfac= 1.0f;
	mtex->colreflfac= 1.0f;
	mtex->coltransfac= 1.0f;
	mtex->densfac= 1.0f;
	mtex->scatterfac= 1.0f;
	mtex->reflfac= 1.0f;
	mtex->shadowfac= 1.0f;
	mtex->zenupfac= 1.0f;
	mtex->zendownfac= 1.0f;
	mtex->blendfac= 1.0f;
	mtex->timefac= 1.0f;
	mtex->lengthfac= 1.0f;
	mtex->clumpfac= 1.0f;
	mtex->kinkfac= 1.0f;
	mtex->roughfac= 1.0f;
	mtex->padensfac= 1.0f;
	mtex->lifefac= 1.0f;
	mtex->sizefac= 1.0f;
	mtex->ivelfac= 1.0f;
	mtex->dampfac= 1.0f;
	mtex->gravityfac= 1.0f;
	mtex->fieldfac= 1.0f;
	mtex->normapspace= MTEX_NSPACE_TANGENT;
}


/* ------------------------------------------------------------------------- */

MTex *add_mtex(void)
{
	MTex *mtex;
	
	mtex= MEM_callocN(sizeof(MTex), "add_mtex");
	
	default_mtex(mtex);
	
	return mtex;
}

/* slot -1 for first free ID */
MTex *add_mtex_id(ID *id, int slot)
{
	MTex **mtex_ar;
	short act;

	give_active_mtex(id, &mtex_ar, &act);

	if (mtex_ar==NULL) {
		return NULL;
	}
	
	if (slot==-1) {
		/* find first free */
		int i;		
		for (i= 0; i < MAX_MTEX; i++) {
			if (!mtex_ar[i]) {
				slot= i;
				break;
			}
		}
		if (slot == -1) {
			return NULL;
		}
	}
	else {
		/* make sure slot is valid */
		if (slot < 0 || slot >= MAX_MTEX) {
			return NULL;
		}
	}

	if (mtex_ar[slot]) {
		id_us_min((ID *)mtex_ar[slot]->tex);
		MEM_freeN(mtex_ar[slot]);
		mtex_ar[slot]= NULL;
	}

	mtex_ar[slot]= add_mtex();

	return mtex_ar[slot];
}

/* ------------------------------------------------------------------------- */

Tex *copy_texture(Tex *tex)
{
	Tex *texn;
	
	texn= copy_libblock(&tex->id);
	if (texn->type==TEX_IMAGE) id_us_plus((ID *)texn->ima);
	else texn->ima= NULL;
	
	if (texn->plugin) {
		texn->plugin= MEM_dupallocN(texn->plugin);
		open_plugin_tex(texn->plugin);
	}
	
	if (texn->coba) texn->coba= MEM_dupallocN(texn->coba);
	if (texn->env) texn->env= BKE_copy_envmap(texn->env);
	if (texn->pd) texn->pd= BKE_copy_pointdensity(texn->pd);
	if (texn->vd) texn->vd= MEM_dupallocN(texn->vd);
	if (texn->ot) texn->ot= BKE_copy_oceantex(texn->ot);
	if (tex->preview) texn->preview = BKE_previewimg_copy(tex->preview);

	if (tex->nodetree) {
		if (tex->nodetree->execdata) {
			ntreeTexEndExecTree(tex->nodetree->execdata, 1);
		}
		texn->nodetree= ntreeCopyTree(tex->nodetree); 
	}
	
	return texn;
}

/* texture copy without adding to main dbase */
Tex *localize_texture(Tex *tex)
{
	Tex *texn;
	
	texn= copy_libblock(&tex->id);
	BLI_remlink(&G.main->tex, texn);
	
	/* image texture: free_texture also doesn't decrease */
	
	if (texn->plugin) {
		texn->plugin= MEM_dupallocN(texn->plugin);
		open_plugin_tex(texn->plugin);
	}
	
	if (texn->coba) texn->coba= MEM_dupallocN(texn->coba);
	if (texn->env) {
		texn->env= BKE_copy_envmap(texn->env);
		id_us_min(&texn->env->ima->id);
	}
	if (texn->pd) texn->pd= BKE_copy_pointdensity(texn->pd);
	if (texn->vd) {
		texn->vd= MEM_dupallocN(texn->vd);
		if (texn->vd->dataset)
			texn->vd->dataset= MEM_dupallocN(texn->vd->dataset);
	}
	if (texn->ot) {
		texn->ot= BKE_copy_oceantex(tex->ot);
	}
	
	texn->preview = NULL;
	
	if (tex->nodetree) {
		texn->nodetree= ntreeLocalize(tex->nodetree);
	}
	
	return texn;
}


/* ------------------------------------------------------------------------- */

static void extern_local_texture(Tex *tex)
{
	id_lib_extern((ID *)tex->ima);
}

void make_local_texture(Tex *tex)
{
	Main *bmain= G.main;
	Material *ma;
	World *wrld;
	Lamp *la;
	Brush *br;
	ParticleSettings *pa;
	int a, is_local= FALSE, is_lib= FALSE;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */
	
	if (tex->id.lib==NULL) return;

	if (tex->id.us==1) {
		id_clear_lib_data(bmain, &tex->id);
		extern_local_texture(tex);
		return;
	}
	
	ma= bmain->mat.first;
	while (ma) {
		for (a=0; a<MAX_MTEX; a++) {
			if (ma->mtex[a] && ma->mtex[a]->tex==tex) {
				if (ma->id.lib) is_lib= TRUE;
				else is_local= TRUE;
			}
		}
		ma= ma->id.next;
	}
	la= bmain->lamp.first;
	while (la) {
		for (a=0; a<MAX_MTEX; a++) {
			if (la->mtex[a] && la->mtex[a]->tex==tex) {
				if (la->id.lib) is_lib= TRUE;
				else is_local= TRUE;
			}
		}
		la= la->id.next;
	}
	wrld= bmain->world.first;
	while (wrld) {
		for (a=0; a<MAX_MTEX; a++) {
			if (wrld->mtex[a] && wrld->mtex[a]->tex==tex) {
				if (wrld->id.lib) is_lib= TRUE;
				else is_local= TRUE;
			}
		}
		wrld= wrld->id.next;
	}
	br= bmain->brush.first;
	while (br) {
		if (br->mtex.tex==tex) {
			if (br->id.lib) is_lib= TRUE;
			else is_local= TRUE;
		}
		br= br->id.next;
	}
	pa= bmain->particle.first;
	while (pa) {
		for (a=0; a<MAX_MTEX; a++) {
			if (pa->mtex[a] && pa->mtex[a]->tex==tex) {
				if (pa->id.lib) is_lib= TRUE;
				else is_local= TRUE;
			}
		}
		pa= pa->id.next;
	}
	
	if (is_local && is_lib == FALSE) {
		id_clear_lib_data(bmain, &tex->id);
		extern_local_texture(tex);
	}
	else if (is_local && is_lib) {
		Tex *tex_new= copy_texture(tex);

		tex_new->id.us= 0;

		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, tex->id.lib, &tex_new->id);
		
		ma= bmain->mat.first;
		while (ma) {
			for (a=0; a<MAX_MTEX; a++) {
				if (ma->mtex[a] && ma->mtex[a]->tex==tex) {
					if (ma->id.lib==NULL) {
						ma->mtex[a]->tex= tex_new;
						tex_new->id.us++;
						tex->id.us--;
					}
				}
			}
			ma= ma->id.next;
		}
		la= bmain->lamp.first;
		while (la) {
			for (a=0; a<MAX_MTEX; a++) {
				if (la->mtex[a] && la->mtex[a]->tex==tex) {
					if (la->id.lib==NULL) {
						la->mtex[a]->tex= tex_new;
						tex_new->id.us++;
						tex->id.us--;
					}
				}
			}
			la= la->id.next;
		}
		wrld= bmain->world.first;
		while (wrld) {
			for (a=0; a<MAX_MTEX; a++) {
				if (wrld->mtex[a] && wrld->mtex[a]->tex==tex) {
					if (wrld->id.lib==NULL) {
						wrld->mtex[a]->tex= tex_new;
						tex_new->id.us++;
						tex->id.us--;
					}
				}
			}
			wrld= wrld->id.next;
		}
		br= bmain->brush.first;
		while (br) {
			if (br->mtex.tex==tex) {
				if (br->id.lib==NULL) {
					br->mtex.tex= tex_new;
					tex_new->id.us++;
					tex->id.us--;
				}
			}
			br= br->id.next;
		}
		pa= bmain->particle.first;
		while (pa) {
			for (a=0; a<MAX_MTEX; a++) {
				if (pa->mtex[a] && pa->mtex[a]->tex==tex) {
					if (pa->id.lib==NULL) {
						pa->mtex[a]->tex= tex_new;
						tex_new->id.us++;
						tex->id.us--;
					}
				}
			}
			pa= pa->id.next;
		}
	}
}

/* ------------------------------------------------------------------------- */
#if 0 /* UNUSED */
void autotexname(Tex *tex)
{
	Main *bmain= G.main;
	char texstr[20][15]= {"None", "Clouds", "Wood", "Marble", "Magic", "Blend",
		"Stucci", "Noise", "Image", "Plugin", "EnvMap", "Musgrave",
		"Voronoi", "DistNoise", "Point Density", "Voxel Data", "Ocean", "", "", ""};
	Image *ima;
	char di[FILE_MAXDIR], fi[FILE_MAXFILE];
	
	if (tex) {
		if (tex->use_nodes) {
			new_id(&bmain->tex, (ID *)tex, "Noddy");
		}
		else
		if (tex->type==TEX_IMAGE) {
			ima= tex->ima;
			if (ima) {
				BLI_strncpy(di, ima->name, sizeof(di));
				BLI_splitdirstring(di, fi);
				strcpy(di, "I.");
				strcat(di, fi);
				new_id(&bmain->tex, (ID *)tex, di);
			}
			else new_id(&bmain->tex, (ID *)tex, texstr[tex->type]);
		}
		else if (tex->type==TEX_PLUGIN && tex->plugin) new_id(&bmain->tex, (ID *)tex, tex->plugin->pname);
		else new_id(&bmain->tex, (ID *)tex, texstr[tex->type]);
	}
}
#endif

/* ------------------------------------------------------------------------- */

Tex *give_current_object_texture(Object *ob)
{
	Material *ma, *node_ma;
	Tex *tex= NULL;
	
	if (ob==NULL) return NULL;
	if (ob->totcol==0 && !(ob->type==OB_LAMP)) return NULL;
	
	if (ob->type==OB_LAMP) {
		tex= give_current_lamp_texture(ob->data);
	}
	else {
		ma= give_current_material(ob, ob->actcol);

		if ((node_ma=give_node_material(ma)))
			ma= node_ma;

		tex= give_current_material_texture(ma);
	}
	
	return tex;
}

Tex *give_current_lamp_texture(Lamp *la)
{
	MTex *mtex= NULL;
	Tex *tex= NULL;

	if (la) {
		mtex= la->mtex[(int)(la->texact)];
		if (mtex) tex= mtex->tex;
	}

	return tex;
}

void set_current_lamp_texture(Lamp *la, Tex *newtex)
{
	int act= la->texact;

	if (la->mtex[act] && la->mtex[act]->tex)
		id_us_min(&la->mtex[act]->tex->id);

	if (newtex) {
		if (!la->mtex[act]) {
			la->mtex[act]= add_mtex();
			la->mtex[act]->texco= TEXCO_GLOB;
		}
		
		la->mtex[act]->tex= newtex;
		id_us_plus(&newtex->id);
	}
	else if (la->mtex[act]) {
		MEM_freeN(la->mtex[act]);
		la->mtex[act]= NULL;
	}
}

bNode *give_current_material_texture_node(Material *ma)
{
	if (ma && ma->use_nodes && ma->nodetree)
		return nodeGetActiveID(ma->nodetree, ID_TE);
	
	return NULL;
}

Tex *give_current_material_texture(Material *ma)
{
	MTex *mtex= NULL;
	Tex *tex= NULL;
	bNode *node;
	
	if (ma && ma->use_nodes && ma->nodetree) {
		/* first check texture, then material, this works together
		 * with a hack that clears the active ID flag for textures on
		 * making a material node active */
		node= nodeGetActiveID(ma->nodetree, ID_TE);

		if (node) {
			tex= (Tex *)node->id;
			ma= NULL;
		}
	}

	if (ma) {
		mtex= ma->mtex[(int)(ma->texact)];
		if (mtex) tex= mtex->tex;
	}
	
	return tex;
}

int give_active_mtex(ID *id, MTex ***mtex_ar, short *act)
{
	switch (GS(id->name)) {
	case ID_MA:
		*mtex_ar=		((Material *)id)->mtex;
		if (act) *act=	(((Material *)id)->texact);
		break;
	case ID_WO:
		*mtex_ar=		((World *)id)->mtex;
		if (act) *act=	(((World *)id)->texact);
		break;
	case ID_LA:
		*mtex_ar=		((Lamp *)id)->mtex;
		if (act) *act=	(((Lamp *)id)->texact);
		break;
	case ID_PA:
		*mtex_ar=		((ParticleSettings *)id)->mtex;
		if (act) *act=	(((ParticleSettings *)id)->texact);
		break;
	default:
		*mtex_ar = NULL;
		if (act) *act=	0;
		return FALSE;
	}

	return TRUE;
}

void set_active_mtex(ID *id, short act)
{
	if (act<0)				act= 0;
	else if (act>=MAX_MTEX)	act= MAX_MTEX-1;

	switch (GS(id->name)) {
	case ID_MA:
		((Material *)id)->texact= act;
		break;
	case ID_WO:
		((World *)id)->texact= act;
		break;
	case ID_LA:
		((Lamp *)id)->texact= act;
		break;
	case ID_PA:
		((ParticleSettings *)id)->texact= act;
		break;
	}
}

void set_current_material_texture(Material *ma, Tex *newtex)
{
	Tex *tex= NULL;
	bNode *node;
	
	if (ma && ma->use_nodes && ma->nodetree) {
		node= nodeGetActiveID(ma->nodetree, ID_TE);

		if (node) {
			tex= (Tex *)node->id;
			id_us_min(&tex->id);
			node->id= &newtex->id;
			id_us_plus(&newtex->id);
			ma= NULL;
		}
	}
	if (ma) {
		int act= (int)ma->texact;

		tex= (ma->mtex[act])? ma->mtex[act]->tex: NULL;
		id_us_min(&tex->id);

		if (newtex) {
			if (!ma->mtex[act])
				ma->mtex[act]= add_mtex();
			
			ma->mtex[act]->tex= newtex;
			id_us_plus(&newtex->id);
		}
		else if (ma->mtex[act]) {
			MEM_freeN(ma->mtex[act]);
			ma->mtex[act]= NULL;
		}
	}
}

int has_current_material_texture(Material *ma)
{
	bNode *node;

	if (ma && ma->use_nodes && ma->nodetree) {
		node= nodeGetActiveID(ma->nodetree, ID_TE);

		if (node)
			return 1;
	}

	return (ma != NULL);
}

Tex *give_current_world_texture(World *world)
{
	MTex *mtex= NULL;
	Tex *tex= NULL;
	
	if (!world) return NULL;
	
	mtex= world->mtex[(int)(world->texact)];
	if (mtex) tex= mtex->tex;
	
	return tex;
}

void set_current_world_texture(World *wo, Tex *newtex)
{
	int act= wo->texact;

	if (wo->mtex[act] && wo->mtex[act]->tex)
		id_us_min(&wo->mtex[act]->tex->id);

	if (newtex) {
		if (!wo->mtex[act]) {
			wo->mtex[act]= add_mtex();
			wo->mtex[act]->texco= TEXCO_VIEW;
		}
		
		wo->mtex[act]->tex= newtex;
		id_us_plus(&newtex->id);
	}
	else if (wo->mtex[act]) {
		MEM_freeN(wo->mtex[act]);
		wo->mtex[act]= NULL;
	}
}

Tex *give_current_brush_texture(Brush *br)
{
	return br->mtex.tex;
}

void set_current_brush_texture(Brush *br, Tex *newtex)
{
	if (br->mtex.tex)
		id_us_min(&br->mtex.tex->id);

	if (newtex) {
		br->mtex.tex= newtex;
		id_us_plus(&newtex->id);
	}
}

Tex *give_current_particle_texture(ParticleSettings *part)
{
	MTex *mtex= NULL;
	Tex *tex= NULL;
	
	if (!part) return NULL;
	
	mtex= part->mtex[(int)(part->texact)];
	if (mtex) tex= mtex->tex;
	
	return tex;
}

void set_current_particle_texture(ParticleSettings *part, Tex *newtex)
{
	int act= part->texact;

	if (part->mtex[act] && part->mtex[act]->tex)
		id_us_min(&part->mtex[act]->tex->id);

	if (newtex) {
		if (!part->mtex[act]) {
			part->mtex[act]= add_mtex();
			part->mtex[act]->texco= TEXCO_ORCO;
			part->mtex[act]->blendtype= MTEX_MUL;
		}
		
		part->mtex[act]->tex= newtex;
		id_us_plus(&newtex->id);
	}
	else if (part->mtex[act]) {
		MEM_freeN(part->mtex[act]);
		part->mtex[act]= NULL;
	}
}

/* ------------------------------------------------------------------------- */

EnvMap *BKE_add_envmap(void)
{
	EnvMap *env;
	
	env= MEM_callocN(sizeof(EnvMap), "envmap");
	env->type= ENV_CUBE;
	env->stype= ENV_ANIM;
	env->clipsta= 0.1;
	env->clipend= 100.0;
	env->cuberes= 600;
	env->viewscale = 0.5;
	
	return env;
} 

/* ------------------------------------------------------------------------- */

EnvMap *BKE_copy_envmap(EnvMap *env)
{
	EnvMap *envn;
	int a;
	
	envn= MEM_dupallocN(env);
	envn->ok= 0;
	for (a=0; a<6; a++) envn->cube[a]= NULL;
	if (envn->ima) id_us_plus((ID *)envn->ima);
	
	return envn;
}

/* ------------------------------------------------------------------------- */

void BKE_free_envmapdata(EnvMap *env)
{
	unsigned int part;
	
	for (part=0; part<6; part++) {
		if (env->cube[part])
			IMB_freeImBuf(env->cube[part]);
		env->cube[part]= NULL;
	}
	env->ok= 0;
}

/* ------------------------------------------------------------------------- */

void BKE_free_envmap(EnvMap *env)
{
	
	BKE_free_envmapdata(env);
	MEM_freeN(env);
	
}

/* ------------------------------------------------------------------------- */

PointDensity *BKE_add_pointdensity(void)
{
	PointDensity *pd;
	
	pd= MEM_callocN(sizeof(PointDensity), "pointdensity");
	pd->flag = 0;
	pd->radius = 0.3f;
	pd->falloff_type = TEX_PD_FALLOFF_STD;
	pd->falloff_softness = 2.0;
	pd->source = TEX_PD_PSYS;
	pd->point_tree = NULL;
	pd->point_data = NULL;
	pd->noise_size = 0.5f;
	pd->noise_depth = 1;
	pd->noise_fac = 1.0f;
	pd->noise_influence = TEX_PD_NOISE_STATIC;
	pd->coba = add_colorband(1);
	pd->speed_scale = 1.0f;
	pd->totpoints = 0;
	pd->object = NULL;
	pd->psys = 0;
	pd->psys_cache_space= TEX_PD_WORLDSPACE;
	pd->falloff_curve = curvemapping_add(1, 0, 0, 1, 1);

	pd->falloff_curve->preset = CURVE_PRESET_LINE;
	pd->falloff_curve->cm->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
	curvemap_reset(pd->falloff_curve->cm, &pd->falloff_curve->clipr, pd->falloff_curve->preset, CURVEMAP_SLOPE_POSITIVE);
	curvemapping_changed(pd->falloff_curve, 0);

	return pd;
} 

PointDensity *BKE_copy_pointdensity(PointDensity *pd)
{
	PointDensity *pdn;

	pdn= MEM_dupallocN(pd);
	pdn->point_tree = NULL;
	pdn->point_data = NULL;
	if (pdn->coba) pdn->coba= MEM_dupallocN(pdn->coba);
	pdn->falloff_curve = curvemapping_copy(pdn->falloff_curve); /* can be NULL */
	return pdn;
}

void BKE_free_pointdensitydata(PointDensity *pd)
{
	if (pd->point_tree) {
		BLI_bvhtree_free(pd->point_tree);
		pd->point_tree = NULL;
	}
	if (pd->point_data) {
		MEM_freeN(pd->point_data);
		pd->point_data = NULL;
	}
	if (pd->coba) {
		MEM_freeN(pd->coba);
		pd->coba = NULL;
	}

	curvemapping_free(pd->falloff_curve); /* can be NULL */
}

void BKE_free_pointdensity(PointDensity *pd)
{
	BKE_free_pointdensitydata(pd);
	MEM_freeN(pd);
}

/* ------------------------------------------------------------------------- */

void BKE_free_voxeldatadata(struct VoxelData *vd)
{
	if (vd->dataset) {
		MEM_freeN(vd->dataset);
		vd->dataset = NULL;
	}

}
 
void BKE_free_voxeldata(struct VoxelData *vd)
{
	BKE_free_voxeldatadata(vd);
	MEM_freeN(vd);
}
 
struct VoxelData *BKE_add_voxeldata(void)
{
	VoxelData *vd;

	vd= MEM_callocN(sizeof(struct VoxelData), "voxeldata");
	vd->dataset = NULL;
	vd->resol[0] = vd->resol[1] = vd->resol[2] = 1;
	vd->interp_type= TEX_VD_LINEAR;
	vd->file_format= TEX_VD_SMOKE;
	vd->int_multiplier = 1.0;
	vd->extend = TEX_CLIP;
	vd->object = NULL;
	vd->cachedframe = -1;
	vd->ok = 0;
	
	return vd;
 }
 
struct VoxelData *BKE_copy_voxeldata(struct VoxelData *vd)
{
	VoxelData *vdn;

	vdn= MEM_dupallocN(vd);	
	vdn->dataset = NULL;

	return vdn;
}

/* ------------------------------------------------------------------------- */

struct OceanTex *BKE_add_oceantex(void)
{
	OceanTex *ot;
	
	ot= MEM_callocN(sizeof(struct OceanTex), "ocean texture");
	ot->output = TEX_OCN_DISPLACEMENT;
	ot->object = NULL;
	
	return ot;
}

struct OceanTex *BKE_copy_oceantex(struct OceanTex *ot)
{
	OceanTex *otn= MEM_dupallocN(ot);
	
	return otn;
}

void BKE_free_oceantex(struct OceanTex *ot)
{
	MEM_freeN(ot);
}


/* ------------------------------------------------------------------------- */
int BKE_texture_dependsOnTime(const struct Tex *texture)
{
	if (texture->plugin) {
		// assume all plugins depend on time
		return 1;
	} 
	else if (	texture->ima && 
			ELEM(texture->ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
		return 1;
	}
	else if (texture->adt) {
		// assume anything in adt means the texture is animated
		return 1;
	}
	else if (texture->type == TEX_NOISE) {
		// noise always varies with time
		return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------------- */
