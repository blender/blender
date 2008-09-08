/* texture.c
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "PIL_dynlib.h"

#include "MTC_matrixops.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "DNA_texture_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_image_types.h"
#include "DNA_world_types.h"
#include "DNA_brush_types.h"
#include "DNA_node_types.h"
#include "DNA_color_types.h"
#include "DNA_scene_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BKE_plugin_types.h"

#include "BKE_bad_level_calls.h"
#include "BKE_utildefines.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BKE_library.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_key.h"
#include "BKE_icons.h"
#include "BKE_ipo.h"
#include "BKE_brush.h"
#include "BKE_node.h"


/* ------------------------------------------------------------------------- */

/* All support for plugin textures: */
int test_dlerr(const char *name, const char *symbol)
{
	char *err;
	
	err= PIL_dynlib_get_error_as_string(NULL);
	if(err) {
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
	pit->doit= 0;
	pit->pname= 0;
	pit->stnames= 0;
	pit->varstr= 0;
	pit->result= 0;
	pit->cfra= 0;
	pit->version= 0;
	pit->instance_init= 0;
	
	/* clear the error list */
	PIL_dynlib_get_error_as_string(NULL);

	/* no PIL_dynlib_close! multiple opened plugins... */
	/* if(pit->handle) PIL_dynlib_close(pit->handle); */
	/* pit->handle= 0; */

	/* open the needed object */
	pit->handle= PIL_dynlib_open(pit->name);
	if(test_dlerr(pit->name, pit->name)) return;

	if (pit->handle != 0) {
		/* find the address of the version function */
		version= (int (*)(void)) PIL_dynlib_find_symbol(pit->handle, "plugin_tex_getversion");
		if (test_dlerr(pit->name, "plugin_tex_getversion")) return;
		
		if (version != 0) {
			pit->version= version();
			if( pit->version >= 2 && pit->version <=6) {
				int (*info_func)(PluginInfo *);
				PluginInfo *info= (PluginInfo*) MEM_mallocN(sizeof(PluginInfo), "plugin_info"); 

				info_func= (int (*)(PluginInfo *))PIL_dynlib_find_symbol(pit->handle, "plugin_getinfo");
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
			} else {
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
	
	strcpy(pit->name, str);
	open_plugin_tex(pit);
	
	if(pit->doit==0) {
		if(pit->handle==0) error("no plugin: %s", str);
		else error("in plugin: %s", str);
		MEM_freeN(pit);
		return NULL;
	}
	
	varstr= pit->varstr;
	for(a=0; a<pit->vars; a++, varstr++) {
		if( (varstr->type & FLO)==FLO)
			pit->data[a]= varstr->def;
		else if( (varstr->type & INT)==INT)
			*((int *)(pit->data+a))= (int) varstr->def;
	}

	if (pit->instance_init)
		pit->instance_init((void *) pit->data);

	return pit;
}

/* ------------------------------------------------------------------------- */

void free_plugin_tex(PluginTex *pit)
{
	if(pit==0) return;
		
	/* no PIL_dynlib_close: same plugin can be opened multiple times, 1 handle */
	MEM_freeN(pit);	
}

/* ****************** Mapping ******************* */

TexMapping *add_mapping(void)
{
	TexMapping *texmap= MEM_callocN(sizeof(TexMapping), "Tex map");
	
	texmap->size[0]= texmap->size[1]= texmap->size[2]= 1.0f;
	texmap->max[0]= texmap->max[1]= texmap->max[2]= 1.0f;
	Mat4One(texmap->mat);
	
	return texmap;
}

void init_mapping(TexMapping *texmap)
{
	float eul[3], smat[3][3], rmat[3][3], mat[3][3];
	
	SizeToMat3(texmap->size, smat);
	
	eul[0]= (M_PI/180.0f)*texmap->rot[0];
	eul[1]= (M_PI/180.0f)*texmap->rot[1];
	eul[2]= (M_PI/180.0f)*texmap->rot[2];
	EulToMat3(eul, rmat);
	
	Mat3MulMat3(mat, rmat, smat);
	
	Mat4CpyMat3(texmap->mat, mat);
	VECCOPY(texmap->mat[3], texmap->loc);

}

/* ****************** COLORBAND ******************* */

void init_colorband(ColorBand *coba, int rangetype)
{
	int a;
	
	coba->data[0].pos= 0.0;
	coba->data[1].pos= 1.0;
	
	if(rangetype==0) {
		coba->data[0].r= 0.0;
		coba->data[0].g= 0.0;
		coba->data[0].b= 0.0;
		coba->data[0].a= 0.0;
		
		coba->data[1].r= 0.0;
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
	
	for(a=2; a<MAXCOLORBAND; a++) {
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
	
	coba= MEM_callocN( sizeof(ColorBand), "colorband");
	init_colorband(coba, rangetype);
	
	return coba;
}

/* ------------------------------------------------------------------------- */

int do_colorband(ColorBand *coba, float in, float out[4])
{
	CBData *cbd1, *cbd2, *cbd0, *cbd3;
	float fac, mfac, t[4];
	int a;
	
	if(coba==NULL || coba->tot==0) return 0;
	
	cbd1= coba->data;
	if(coba->tot==1) {
		out[0]= cbd1->r;
		out[1]= cbd1->g;
		out[2]= cbd1->b;
		out[3]= cbd1->a;
	}
	else {
		if(in <= cbd1->pos && coba->ipotype<2) {
			out[0]= cbd1->r;
			out[1]= cbd1->g;
			out[2]= cbd1->b;
			out[3]= cbd1->a;
		}
		else {
			CBData left, right;
			
			/* we're looking for first pos > in */
			for(a=0; a<coba->tot; a++, cbd1++) if(cbd1->pos > in) break;
				
			if(a==coba->tot) {
				cbd2= cbd1-1;
				right= *cbd2;
				right.pos= 1.0f;
				cbd1= &right;
			}
			else if(a==0) {
				left= *cbd1;
				left.pos= 0.0f;
				cbd2= &left;
			}
			else cbd2= cbd1-1;
			
			if(in >= cbd1->pos && coba->ipotype<2) {
				out[0]= cbd1->r;
				out[1]= cbd1->g;
				out[2]= cbd1->b;
				out[3]= cbd1->a;
			}
			else {
		
				if(cbd2->pos!=cbd1->pos)
					fac= (in-cbd1->pos)/(cbd2->pos-cbd1->pos);
				else
					fac= 0.0f;
				
				if(coba->ipotype>=2) {
					/* ipo from right to left: 3 2 1 0 */
					
					if(a>=coba->tot-1) cbd0= cbd1;
					else cbd0= cbd1+1;
					if(a<2) cbd3= cbd2;
					else cbd3= cbd2-1;
					
					CLAMP(fac, 0.0f, 1.0f);
					
					if(coba->ipotype==3)
						set_four_ipo(fac, t, KEY_CARDINAL);
					else
						set_four_ipo(fac, t, KEY_BSPLINE);

					out[0]= t[3]*cbd3->r +t[2]*cbd2->r +t[1]*cbd1->r +t[0]*cbd0->r;
					out[1]= t[3]*cbd3->g +t[2]*cbd2->g +t[1]*cbd1->g +t[0]*cbd0->g;
					out[2]= t[3]*cbd3->b +t[2]*cbd2->b +t[1]*cbd1->b +t[0]*cbd0->b;
					out[3]= t[3]*cbd3->a +t[2]*cbd2->a +t[1]*cbd1->a +t[0]*cbd0->a;
					CLAMP(out[0], 0.0, 1.0);
					CLAMP(out[1], 0.0, 1.0);
					CLAMP(out[2], 0.0, 1.0);
					CLAMP(out[3], 0.0, 1.0);
				}
				else {
				
					if(coba->ipotype==1) {	/* EASE */
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

	for(a=0; a<*size; a++)
		do_colorband(coba, (float)a/(float)CM_TABLE, &(*array)[a*4]);
}

/* ******************* TEX ************************ */

void free_texture(Tex *tex)
{
	free_plugin_tex(tex->plugin);
	if(tex->coba) MEM_freeN(tex->coba);
	if(tex->env) BKE_free_envmap(tex->env);
	BKE_previewimg_free(&tex->preview);
	BKE_icon_delete((struct ID*)tex);
	tex->id.icon_id = 0;
}

/* ------------------------------------------------------------------------- */

void default_tex(Tex *tex)
{
	PluginTex *pit;
	VarStruct *varstr;
	int a;

	tex->stype= 0;
	tex->flag= TEX_CHECKER_ODD;
	tex->imaflag= TEX_INTERPOL+TEX_MIPMAP+TEX_USEALPHA;
	tex->extend= TEX_REPEAT;
	tex->cropxmin= tex->cropymin= 0.0;
	tex->cropxmax= tex->cropymax= 1.0;
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
		tex->env->stype=ENV_STATIC;
		tex->env->clipsta=0.1;
		tex->env->clipend=100;
		tex->env->cuberes=100;
		tex->env->depth=0;
	}

	pit = tex->plugin;
	if (pit) {
		varstr= pit->varstr;
		if(varstr) {
			for(a=0; a<pit->vars; a++, varstr++) {
				pit->data[a] = varstr->def;
			}
		}
	}
	
	tex->iuser.fie_ima= 2;
	tex->iuser.ok= 1;
	tex->iuser.frames= 100;
	
	tex->preview = NULL;
}

/* ------------------------------------------------------------------------- */

Tex *add_texture(char *name)
{
	Tex *tex;

	tex= alloc_libblock(&G.main->tex, ID_TE, name);
	
	default_tex(tex);
	
	return tex;
}

/* ------------------------------------------------------------------------- */

void default_mtex(MTex *mtex)
{
	mtex->texco= TEXCO_ORCO;
	mtex->mapto= MAP_COL;
	mtex->object= 0;
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
	mtex->tex= 0;
	mtex->texflag= 0;
	mtex->colormodel= 0;
	mtex->r= 1.0;
	mtex->g= 0.0;
	mtex->b= 1.0;
	mtex->k= 1.0;
	mtex->def_var= 1.0;
	mtex->blendtype= MTEX_BLEND;
	mtex->colfac= 1.0;
	mtex->norfac= 0.5;
	mtex->varfac= 1.0;
	mtex->dispfac=0.2;
	mtex->normapspace= MTEX_NSPACE_TANGENT;
}


/* ------------------------------------------------------------------------- */

MTex *add_mtex()
{
	MTex *mtex;
	
	mtex= MEM_callocN(sizeof(MTex), "add_mtex");
	
	default_mtex(mtex);
	
	return mtex;
}

/* ------------------------------------------------------------------------- */

Tex *copy_texture(Tex *tex)
{
	Tex *texn;
	
	texn= copy_libblock(tex);
	if(texn->type==TEX_IMAGE) id_us_plus((ID *)texn->ima);
	else texn->ima= 0;
	
	id_us_plus((ID *)texn->ipo);
	
	if(texn->plugin) {
		texn->plugin= MEM_dupallocN(texn->plugin);
		open_plugin_tex(texn->plugin);
	}
	
	if(texn->coba) texn->coba= MEM_dupallocN(texn->coba);
	if(texn->env) texn->env= BKE_copy_envmap(texn->env);
	
	if(tex->preview) texn->preview = BKE_previewimg_copy(tex->preview);

	return texn;
}

/* ------------------------------------------------------------------------- */

void make_local_texture(Tex *tex)
{
	Tex *texn;
	Material *ma;
	World *wrld;
	Lamp *la;
	Brush *br;
	int a, local=0, lib=0;

	/* - only lib users: do nothing
	    * - only local users: set flag
	    * - mixed: make copy
	    */
	
	if(tex->id.lib==0) return;

	/* special case: ima always local immediately */
	if(tex->ima) {
		tex->ima->id.lib= 0;
		tex->ima->id.flag= LIB_LOCAL;
		new_id(0, (ID *)tex->ima, 0);
	}

	if(tex->id.us==1) {
		tex->id.lib= 0;
		tex->id.flag= LIB_LOCAL;
		new_id(0, (ID *)tex, 0);

		return;
	}
	
	ma= G.main->mat.first;
	while(ma) {
		for(a=0; a<MAX_MTEX; a++) {
			if(ma->mtex[a] && ma->mtex[a]->tex==tex) {
				if(ma->id.lib) lib= 1;
				else local= 1;
			}
		}
		ma= ma->id.next;
	}
	la= G.main->lamp.first;
	while(la) {
		for(a=0; a<MAX_MTEX; a++) {
			if(la->mtex[a] && la->mtex[a]->tex==tex) {
				if(la->id.lib) lib= 1;
				else local= 1;
			}
		}
		la= la->id.next;
	}
	wrld= G.main->world.first;
	while(wrld) {
		for(a=0; a<MAX_MTEX; a++) {
			if(wrld->mtex[a] && wrld->mtex[a]->tex==tex) {
				if(wrld->id.lib) lib= 1;
				else local= 1;
			}
		}
		wrld= wrld->id.next;
	}
	br= G.main->brush.first;
	while(br) {
		for(a=0; a<MAX_MTEX; a++) {
			if(br->mtex[a] && br->mtex[a]->tex==tex) {
				if(br->id.lib) lib= 1;
				else local= 1;
			}
		}
		br= br->id.next;
	}
	
	if(local && lib==0) {
		tex->id.lib= 0;
		tex->id.flag= LIB_LOCAL;
		new_id(0, (ID *)tex, 0);
	}
	else if(local && lib) {
		texn= copy_texture(tex);
		texn->id.us= 0;
		
		ma= G.main->mat.first;
		while(ma) {
			for(a=0; a<MAX_MTEX; a++) {
				if(ma->mtex[a] && ma->mtex[a]->tex==tex) {
					if(ma->id.lib==0) {
						ma->mtex[a]->tex= texn;
						texn->id.us++;
						tex->id.us--;
					}
				}
			}
			ma= ma->id.next;
		}
		la= G.main->lamp.first;
		while(la) {
			for(a=0; a<MAX_MTEX; a++) {
				if(la->mtex[a] && la->mtex[a]->tex==tex) {
					if(la->id.lib==0) {
						la->mtex[a]->tex= texn;
						texn->id.us++;
						tex->id.us--;
					}
				}
			}
			la= la->id.next;
		}
		wrld= G.main->world.first;
		while(wrld) {
			for(a=0; a<MAX_MTEX; a++) {
				if(wrld->mtex[a] && wrld->mtex[a]->tex==tex) {
					if(wrld->id.lib==0) {
						wrld->mtex[a]->tex= texn;
						texn->id.us++;
						tex->id.us--;
					}
				}
			}
			wrld= wrld->id.next;
		}
		br= G.main->brush.first;
		while(br) {
			for(a=0; a<MAX_MTEX; a++) {
				if(br->mtex[a] && br->mtex[a]->tex==tex) {
					if(br->id.lib==0) {
						br->mtex[a]->tex= texn;
						texn->id.us++;
						tex->id.us--;
					}
				}
			}
			br= br->id.next;
		}
	}
}

/* ------------------------------------------------------------------------- */

void autotexname(Tex *tex)
{
/*	extern char texstr[20][12];	 *//* buttons.c, already in bad lev calls*/
	Image *ima;
	char di[FILE_MAXDIR], fi[FILE_MAXFILE];
	
	if(tex) {
		if(tex->type==TEX_IMAGE) {
			ima= tex->ima;
			if(ima) {
				strcpy(di, ima->name);
				BLI_splitdirstring(di, fi);
				strcpy(di, "I.");
				strcat(di, fi);
				new_id(&G.main->tex, (ID *)tex, di);
			}
			else new_id(&G.main->tex, (ID *)tex, texstr[tex->type]);
		}
		else if(tex->type==TEX_PLUGIN && tex->plugin) new_id(&G.main->tex, (ID *)tex, tex->plugin->pname);
		else new_id(&G.main->tex, (ID *)tex, texstr[tex->type]);
	}
}

/* ------------------------------------------------------------------------- */

Tex *give_current_texture(Object *ob, int act)
{
	Material ***matarar, *ma;
	Lamp *la = 0;
	MTex *mtex = 0;
	Tex *tex = 0;
	bNode *node;
	
	if(ob==0) return 0;
	if(ob->totcol==0 && !(ob->type==OB_LAMP)) return 0;
	
	if(ob->type==OB_LAMP) {
		la=(Lamp *)ob->data;
		if(la) {
			mtex= la->mtex[(int)(la->texact)];
			if(mtex) tex= mtex->tex;
		}
	} else {
		if(act>ob->totcol) act= ob->totcol;
		else if(act==0) act= 1;
		
		if( BTST(ob->colbits, act-1) ) {	/* in object */
			ma= ob->mat[act-1];
		}
		else {								/* in data */
			matarar= give_matarar(ob);
			
			if(matarar && *matarar) ma= (*matarar)[act-1];
			else ma= 0;
		}

		if(ma && ma->use_nodes && ma->nodetree) {
			node= nodeGetActiveID(ma->nodetree, ID_TE);

			if(node) {
				tex= (Tex *)node->id;
				ma= NULL;
			}
			else {
				node= nodeGetActiveID(ma->nodetree, ID_MA);
				if(node)
					ma= (Material*)node->id;
			}
		}
		if(ma) {
			mtex= ma->mtex[(int)(ma->texact)];
			if(mtex) tex= mtex->tex;
		}
	}
	
	return tex;
}

Tex *give_current_world_texture(void)
{
	MTex *mtex = 0;
	Tex *tex = 0;
	
	if(!(G.scene->world)) return 0;
	
	mtex= G.scene->world->mtex[(int)(G.scene->world->texact)];
	if(mtex) tex= mtex->tex;
	
	return tex;
}

/* ------------------------------------------------------------------------- */

EnvMap *BKE_add_envmap(void)
{
	EnvMap *env;
	
	env= MEM_callocN(sizeof(EnvMap), "envmap");
	env->type= ENV_CUBE;
	env->stype= ENV_STATIC;
	env->clipsta= 0.1;
	env->clipend= 100.0;
	env->cuberes= 100;
	
	return env;
} 

/* ------------------------------------------------------------------------- */

EnvMap *BKE_copy_envmap(EnvMap *env)
{
	EnvMap *envn;
	int a;
	
	envn= MEM_dupallocN(env);
	envn->ok= 0;
	for(a=0; a<6; a++) envn->cube[a]= NULL;
	if(envn->ima) id_us_plus((ID *)envn->ima);
	
	return envn;
}

/* ------------------------------------------------------------------------- */

void BKE_free_envmapdata(EnvMap *env)
{
	unsigned int part;
	
	for(part=0; part<6; part++) {
		if(env->cube[part])
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
int BKE_texture_dependsOnTime(const struct Tex *texture)
{
	if(texture->plugin) {
		// assume all plugins depend on time
		return 1;
	} else if(	texture->ima && 
			ELEM(texture->ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
		return 1;
	} else if(texture->ipo) {
		// assume any ipo means the texture is animated
		return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------------- */
