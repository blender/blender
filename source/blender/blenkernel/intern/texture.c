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

#include "MEM_guardedalloc.h"

#include "PIL_dynlib.h"

#include "MTC_matrixops.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "DNA_texture_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_image_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BKE_osa_types.h"
#include "BKE_plugin_types.h"

#include "BKE_bad_level_calls.h"
#include "BKE_utildefines.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BKE_library.h"
#include "BKE_image.h"
#include "BKE_texture.h"
#include "BKE_key.h"
#include "BKE_ipo.h"


/* These vars form the texture channel */
float Tin, Tr, Tg, Tb, Ta, Txtra;
extern int Talpha;


/* ------------------------------------------------------------------------- */

/* All support for plugin textures: */
int test_dlerr(const char *name,  const char *symbol)
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
			if (pit->version==2 || pit->version==3) {
				int (*info_func)(PluginInfo *);
				PluginInfo *info= (PluginInfo*) MEM_mallocN(sizeof(PluginInfo), "plugin_info"); 

				info_func= (int (*)(PluginInfo *))PIL_dynlib_find_symbol(pit->handle, "plugin_getinfo");
				if (!test_dlerr(pit->name, "plugin_getinfo")) {

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

	return pit;
}

/* ------------------------------------------------------------------------- */

void free_plugin_tex(PluginTex *pit)
{
	if(pit==0) return;
		
	/* no PIL_dynlib_close: same plugin can be opened multiple times, 1 handle */
	MEM_freeN(pit);	
}

/* ****************** COLORBAND ******************* */

ColorBand *add_colorband()
{
	ColorBand *coba;
	int a;
	
	coba= MEM_callocN( sizeof(ColorBand), "colorband");
	
	coba->data[0].r= 0.0;
	coba->data[0].g= 0.0;
	coba->data[0].b= 0.0;
	coba->data[0].a= 0.0;
	coba->data[0].pos= 0.0;

	coba->data[1].r= 0.0;
	coba->data[1].g= 1.0;
	coba->data[1].b= 1.0;
	coba->data[1].a= 1.0;
	coba->data[1].pos= 1.0;
	
	for(a=2; a<MAXCOLORBAND; a++) {
		coba->data[a].r= 0.5;
		coba->data[a].g= 0.5;
		coba->data[a].b= 0.5;
		coba->data[a].a= 1.0;
		coba->data[a].pos= 0.5;
	}
	
	coba->tot= 2;
	
	return coba;
}

/* ------------------------------------------------------------------------- */

int do_colorband(ColorBand *coba)
{
	CBData *cbd1, *cbd2, *cbd0, *cbd3;
	float fac, mfac, t[4];
	int a;
	
	if(coba->tot==0) return 0;
	Talpha= 1;
	
	cbd1= coba->data;
	
	if(Tin <= cbd1->pos) {	/* ultimate left */
		Tr= cbd1->r;
		Tg= cbd1->g;
		Tb= cbd1->b;
		Ta= cbd1->a;
	}
	else {
		/* we're looking for first pos > Tin */
	
		for(a=0; a<coba->tot; a++, cbd1++) if(cbd1->pos >= Tin) break;
			
		if(a==coba->tot) {	/* ultimate right */
			cbd1--;
			Tr= cbd1->r;
			Tg= cbd1->g;
			Tb= cbd1->b;
			Ta= cbd1->a;
		}
		else {
			cbd2= cbd1-1;
			fac= (Tin-cbd1->pos)/(cbd2->pos-cbd1->pos);
			
			if(coba->ipotype==2) {
				/* ipo from right to left: 3 2 1 0 */
				
				if(a>=coba->tot-1) cbd0= cbd1;
				else cbd0= cbd1+1;
				if(a<2) cbd3= cbd2;
				else cbd3= cbd2-1;
				
				set_four_ipo(fac, t, KEY_BSPLINE);

				Tr= t[3]*cbd3->r +t[2]*cbd2->r +t[1]*cbd1->r +t[0]*cbd0->r;
				Tg= t[3]*cbd3->g +t[2]*cbd2->g +t[1]*cbd1->g +t[0]*cbd0->g;
				Tb= t[3]*cbd3->b +t[2]*cbd2->b +t[1]*cbd1->b +t[0]*cbd0->b;
				Ta= t[3]*cbd3->a +t[2]*cbd2->a +t[1]*cbd1->a +t[0]*cbd0->a;
				CLAMP(Tr, 0.0, 1.0);
				CLAMP(Tg, 0.0, 1.0);
				CLAMP(Tb, 0.0, 1.0);
				CLAMP(Ta, 0.0, 1.0);
			}
			else {
			
				if(coba->ipotype==1) {	/* EASE */
					mfac= fac*fac;
					fac= 3.0f*mfac-2.0f*mfac*fac;
				}
				mfac= 1.0f-fac;
				
				Tr= mfac*cbd1->r + fac*cbd2->r;
				Tg= mfac*cbd1->g + fac*cbd2->g;
				Tb= mfac*cbd1->b + fac*cbd2->b;
				Ta= mfac*cbd1->a + fac*cbd2->a;
			}
		}
	}
	return 1;	/* OK */
}

/* ******************* TEX ************************ */

void free_texture(Tex *tex)
{
	free_plugin_tex(tex->plugin);
	if(tex->coba) MEM_freeN(tex->coba);
	if(tex->env) RE_free_envmap(tex->env);
}

/* ------------------------------------------------------------------------- */

void default_tex(Tex *tex)
{
	tex->stype= 0;
	tex->imaflag= TEX_INTERPOL+TEX_MIPMAP;
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
	tex->bright= 1.0;
	tex->contrast= tex->filtersize= 1.0;
	tex->rfac= 1.0;
	tex->gfac= 1.0;
	tex->bfac= 1.0;

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
	
	if(texn->plugin) {
		texn->plugin= MEM_dupallocN(texn->plugin);
		open_plugin_tex(texn->plugin);
	}
	
	if(texn->coba) texn->coba= MEM_dupallocN(texn->coba);
	if(texn->env) texn->env= RE_copy_envmap(texn->env);
	
	return texn;
}

/* ------------------------------------------------------------------------- */

void make_local_texture(Tex *tex)
{
	Tex *texn;
	Material *ma;
	World *wrld;
	Lamp *la;
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
		for(a=0; a<8; a++) {
			if(ma->mtex[a] && ma->mtex[a]->tex==tex) {
				if(ma->id.lib) lib= 1;
				else local= 1;
			}
		}
		ma= ma->id.next;
	}
	la= G.main->lamp.first;
	while(la) {
		for(a=0; a<8; a++) {
			if(la->mtex[a] && la->mtex[a]->tex==tex) {
				if(la->id.lib) lib= 1;
				else local= 1;
			}
		}
		la= la->id.next;
	}
	wrld= G.main->world.first;
	while(wrld) {
		for(a=0; a<8; a++) {
			if(wrld->mtex[a] && wrld->mtex[a]->tex==tex) {
				if(wrld->id.lib) lib= 1;
				else local= 1;
			}
		}
		wrld= wrld->id.next;
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
			for(a=0; a<8; a++) {
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
			for(a=0; a<8; a++) {
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
			for(a=0; a<8; a++) {
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

	}
}

/* ------------------------------------------------------------------------- */

void autotexname(Tex *tex)
{
/*  	extern char texstr[15][8];	 *//* buttons.c, already in bad lev calls*/
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

void init_render_texture(Tex *tex)
{
	Image *ima;
	int imanr;
	unsigned short numlen;
	char name[256], head[FILE_MAXFILE], tail[FILE_MAXFILE];

	/* is also used as signal */
	tex->nor= 0;

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


/* ************************** */

int clouds(Tex *tex, float *texvec)
{
	float (*turbfunc)(float, float, float, float, int);

	if(tex->noisetype==TEX_NOISESOFT) turbfunc= BLI_turbulence;
	else turbfunc= BLI_turbulence1;
	
	Tin= turbfunc(tex->noisesize, texvec[0], texvec[1], texvec[2], tex->noisedepth);

	if(tex->stype==1) {

		Tr= Tin;
		Tg= turbfunc(tex->noisesize, texvec[1], texvec[0], texvec[2], tex->noisedepth);

		Tb= turbfunc(tex->noisesize,texvec[1],texvec[2],texvec[0], tex->noisedepth);
		
		BRICONRGB;
		Ta= 1.0;
		
		return 1;
	}
	
	BRICON;

	if(tex->flag & TEX_COLORBAND)  return do_colorband(tex->coba);
		
	return 0;
}

/* ------------------------------------------------------------------------- */

int blend(Tex *tex, float *texvec)
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

int wood(Tex *tex, float *texvec)
{
	float (*noisefunc)(float, float, float, float);

	if(tex->noisetype==TEX_NOISESOFT) noisefunc= BLI_hnoise;
	else noisefunc= BLI_hnoisep;

	
	if(tex->stype==0) {
		Tin= 0.5+0.5*sin( (texvec[0]+texvec[1]+texvec[2])*10.0 );
	}
	else if(tex->stype==1) {
		Tin= 0.5+0.5*sin( sqrt(texvec[0]*texvec[0]+texvec[1]*texvec[1]+texvec[2]*texvec[2])*20.0 );
	}
	else if(tex->stype==2) {
		Tin= noisefunc(tex->noisesize, texvec[0], texvec[1], texvec[2]);
		Tin= 0.5+ 0.5*sin(tex->turbul*Tin+(texvec[0]+texvec[1]+texvec[2])*10.0);
	}
	else if(tex->stype==3) {
		Tin= noisefunc(tex->noisesize, texvec[0], texvec[1], texvec[2]);
		Tin= 0.5+ 0.5*sin(tex->turbul*Tin+(sqrt(texvec[0]*texvec[0]+texvec[1]*texvec[1]+texvec[2]*texvec[2]))*20.0);
	}
	
	
	BRICON;
	if(tex->flag & TEX_COLORBAND)  return do_colorband(tex->coba);
	
	return 0;
}

/* ------------------------------------------------------------------------- */

int marble(Tex *tex, float *texvec)
{
	float n;
	float (*turbfunc)(float, float, float, float, int);

	if(tex->noisetype==TEX_NOISESOFT) turbfunc= BLI_turbulence;
	else turbfunc= BLI_turbulence1;
	
	n= 5.0*(texvec[0]+texvec[1]+texvec[2]);

	Tin = 0.5+0.5*sin(n+tex->turbul*turbfunc(tex->noisesize, texvec[0],texvec[1],texvec[2], tex->noisedepth));

	switch (tex->stype) {
	case 1:
		Tin= sqrt(Tin);
		break;
	case 2:
		Tin= sqrt(Tin);
		Tin= sqrt(Tin);
		break;
	}
	
	BRICON;
	if(tex->flag & TEX_COLORBAND)  return do_colorband(tex->coba);
	
	return 0;
}

/* ------------------------------------------------------------------------- */

int magic(Tex *tex, float *texvec)
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

int stucci(Tex *tex, float *texvec)
{
	float b2, vec[3];
	float ofs;
	float (*noisefunc)(float, float, float, float);

	if(tex->nor == NULL) return 0;

	if(tex->noisetype==TEX_NOISESOFT) noisefunc= BLI_hnoise;
	else noisefunc= BLI_hnoisep;

	ofs= tex->turbul/200.0;
	
	b2= noisefunc(tex->noisesize, texvec[0], texvec[1], texvec[2]);
	if(tex->stype) ofs*=(b2*b2);
	vec[0]= b2-noisefunc(tex->noisesize, texvec[0]+ofs, texvec[1], texvec[2]);
	vec[1]= b2-noisefunc(tex->noisesize, texvec[0], texvec[1]+ofs, texvec[2]);
	vec[2]= b2-noisefunc(tex->noisesize, texvec[0], texvec[1], texvec[2]+ofs);

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

int texnoise(Tex *tex)
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

int plugintex(Tex *tex, float *texvec, float *dxt, float *dyt)
{
	PluginTex *pit;
	int rgbnor=0;
	
	Tin= 0.0;

	pit= tex->plugin;
	if(pit && pit->doit) {
		VECCOPY(pit->result+5, R.vn); 

		if(R.osatex) rgbnor= ((TexDoit)pit->doit)(tex->stype, pit->data, texvec, dxt, dyt);
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

static int cubemap_glob(MTex *mtex, float x, float y, float z, float *adr1, float *adr2)
{
	float x1, y1, z1, nor[3];
	int ret;
	
	VECCOPY(nor, R.vn);
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

static int cubemap(MTex *mtex, float x, float y, float z, float *adr1, float *adr2)
{
	int proj[4], ret= 0;
	
	if(R.vlr && R.vlr->mface) {
		int index;
		/* the mtex->proj{xyz} have type char. maybe this should be wider? */
		/* casting to int ensures that the index type is right.            */
		index = (int) mtex->projx;
		proj[index]= ME_PROJXY;

		index = (int) mtex->projy;
		proj[index]= ME_PROJXZ;

		index = (int) mtex->projz;
		proj[index]= ME_PROJYZ;
		
		if(R.vlr->mface->puno & proj[1]) {
			*adr1 = (x + 1.0) / 2.0;
			*adr2 = (y + 1.0) / 2.0;	
		}
		else if(R.vlr->mface->puno & proj[2]) {
			*adr1 = (x + 1.0) / 2.0;
			*adr2 = (z + 1.0) / 2.0;
			ret= 1;
		}
		else {
			*adr1 = (y + 1.0) / 2.0;
			*adr2 = (z + 1.0) / 2.0;
			ret= 2;
		}		
	} else
		return cubemap_glob(mtex, x, y, z, adr1, adr2);

	return ret;
}

/* ------------------------------------------------------------------------- */

static int cubemap_ob(MTex *mtex, float x, float y, float z, float *adr1, float *adr2)
{
	float x1, y1, z1, nor[3];
	int ret;
	
	VECCOPY(nor, R.vn);
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

void do_2d_mapping(MTex *mtex, float *t, float *dxt, float *dyt)
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
			if(mtex->texco==TEXCO_OBJECT) cubemap_ob(mtex, t[0], t[1], t[2], &fx, &fy);
			else if(mtex->texco==TEXCO_GLOB) cubemap_glob(mtex, t[0], t[1], t[2], &fx, &fy);
			else cubemap(mtex, t[0], t[1], t[2], &fx, &fy);
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

			if(mtex->texco==TEXCO_OBJECT) proj = cubemap_ob(mtex, t[0], t[1], t[2], &fx, &fy);
			else if (mtex->texco==TEXCO_GLOB) proj = cubemap_glob(mtex, t[0], t[1], t[2], &fx, &fy);
			else proj = cubemap(mtex, t[0], t[1], t[2], &fx, &fy);

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

int multitex(Tex *tex, float *texvec, float *dxt, float *dyt)
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
		if(R.osatex) return imagewraposa(tex, texvec, dxt, dyt); 
		else return imagewrap(tex, texvec); 
		break;
	case TEX_PLUGIN:
		return plugintex(tex, texvec, dxt, dyt);
		break;
	case TEX_ENVMAP:
		return RE_envmaptex(tex, texvec, dxt, dyt);
		break;
	}
	return 0;
}

/* ------------------------------------------------------------------------- */

void do_material_tex()
{
	Object *ob;
	Material *mat_col, *mat_colspec, *mat_colmir, *mat_ref;
	Material *mat_spec, *mat_har, *mat_emit, *mat_alpha;
	MTex *mtex;
	Tex *tex;
	float *co = NULL, *dx = NULL, *dy = NULL, fact, 
		facm, factt, facmm, facmul = 0.0, stencilTin=1.0;
	float texvec[3], dxt[3], dyt[3], tempvec[3], norvec[3], Tnor=1.0;
	int tex_nr, rgbnor= 0;

	/* here: test flag if there's a tex (todo) */
	
	mat_col=mat_colspec=mat_colmir=mat_ref=mat_spec=mat_har=mat_emit=mat_alpha= R.mat;
	
	for(tex_nr=0; tex_nr<8; tex_nr++) {
		
		/* separate tex switching */
		if(R.mat->septex & (1<<tex_nr)) continue;
		
		if(R.mat->mtex[tex_nr]) {
			mtex= R.mat->mtex[tex_nr];
			
			tex= mtex->tex;
			if(tex==0) continue;
			
			/* which coords */
			if(mtex->texco==TEXCO_ORCO) {
				co= R.lo; dx= O.dxlo; dy= O.dylo;
			}
			else if(mtex->texco==TEXCO_STICKY) {
				co= R.sticky; dx= O.dxsticky; dy= O.dysticky;
			}
			else if(mtex->texco==TEXCO_OBJECT) {
				ob= mtex->object;
				if(ob) {
					co= tempvec;
					dx= dxt;
					dy= dyt;
					VECCOPY(tempvec, R.co);
					MTC_Mat4MulVecfl(ob->imat, tempvec);
					if(R.osatex) {
						VECCOPY(dxt, O.dxco);
						VECCOPY(dyt, O.dyco);
						MTC_Mat4Mul3Vecfl(ob->imat, dxt);
						MTC_Mat4Mul3Vecfl(ob->imat, dyt);
					}
				}
				else {
					/* if object doesn't exist, do not use orcos (not initialized) */
					co= R.co;
					dx= O.dxco; dy= O.dyco;
				}
			}
			else if(mtex->texco==TEXCO_REFL) {
				co= R.ref; dx= O.dxref; dy= O.dyref;
			}
			else if(mtex->texco==TEXCO_NORM) {
				co= R.orn; dx= O.dxno; dy= O.dyno;
			}
			else if(mtex->texco==TEXCO_GLOB) {
				co= R.gl; dx= O.dxco; dy= O.dyco;
			}
			else if(mtex->texco==TEXCO_UV) {
				co= R.uv; dx= O.dxuv; dy= O.dyuv;
			}
			else if(mtex->texco==TEXCO_WINDOW) {
				co= R.winco; dx= O.dxwin; dy= O.dywin;
			}
			
			/* de pointer defines if bumping happens */
			if(mtex->mapto & MAP_NORM) {
				tex->nor= norvec;
				norvec[0]= norvec[1]= norvec[2]= 0.0;
			}
			else tex->nor= 0;
			
			if(tex->type==TEX_IMAGE) {
				
				/* new: first swap coords, then map, then trans/scale */
				
				/* placement */
				if(mtex->projx) texvec[0]= co[mtex->projx-1];
				else texvec[0]= 0.0;
				if(mtex->projy) texvec[1]= co[mtex->projy-1];
				else texvec[1]= 0.0;
				if(mtex->projz) texvec[2]= co[mtex->projz-1];
				else texvec[2]= 0.0;

				if(R.osatex) {

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

				do_2d_mapping(mtex, texvec, dxt, dyt);
				
				/* translate and scale */
				texvec[0]= mtex->size[0]*(texvec[0]-0.5) +mtex->ofs[0]+0.5;
				texvec[1]= mtex->size[1]*(texvec[1]-0.5) +mtex->ofs[1]+0.5;
				if(R.osatex) {
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
				
				if(R.osatex) {
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

			rgbnor= multitex(tex, texvec, dxt, dyt);
			
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
				if(rgbnor & TEX_RGB) Ta*= stencilTin;
				else if(rgbnor & TEX_NOR) Tnor*= stencilTin;
				else Tin*= stencilTin;
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
					
					f1= R.vn[0];
					f2= R.vn[1];
					tex->nor[0]= f1*co+f2*si;
					tex->nor[1]= f2*co-f1*si;
					f1= R.vn[1];
					f2= R.vn[2];
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
						R.matren->r= (fact*Tr + facm*mat_col->r);
						R.matren->g= (fact*Tg + facm*mat_col->g);
						R.matren->b= (fact*Tb + facm*mat_col->b);
					}
					else if(mtex->blendtype==MTEX_MUL) {
						R.matren->r= (facm+fact*Tr)*mat_col->r;
						R.matren->g= (facm+fact*Tg)*mat_col->g;
						R.matren->b= (facm+fact*Tb)*mat_col->b;
					}
					else {
						R.matren->r= (fact*Tr + mat_col->r);
						R.matren->g= (fact*Tg + mat_col->g);
						R.matren->b= (fact*Tb + mat_col->b);
					}
					mat_col= R.matren;
				}
				if(mtex->mapto & MAP_COLSPEC) {
					if(mtex->blendtype==MTEX_BLEND) {
						R.matren->specr= (fact*Tr + facm*mat_colspec->specr);
						R.matren->specg= (fact*Tg + facm*mat_colspec->specg);
						R.matren->specb= (fact*Tb + facm*mat_colspec->specb);
					}
					else if(mtex->blendtype==MTEX_MUL) {
						R.matren->specr= (facm+fact*Tr)*mat_colspec->specr;
						R.matren->specg= (facm+fact*Tg)*mat_colspec->specg;
						R.matren->specb= (facm+fact*Tb)*mat_colspec->specb;
					}
					else {
						R.matren->specr= (fact*Tr + mat_colspec->specr);
						R.matren->specg= (fact*Tg + mat_colspec->specg);
						R.matren->specb= (fact*Tb + mat_colspec->specb);
					}
					mat_colspec= R.matren;
				}
				if(mtex->mapto & MAP_COLMIR) {
					if(mtex->blendtype==MTEX_BLEND) {
						R.refcol[0]= fact + facm*R.refcol[0];
						
						R.refcol[1]= fact*Tr + facm*R.refcol[1];
						R.refcol[2]= fact*Tg + facm*R.refcol[2];
						R.refcol[3]= fact*Tb + facm*R.refcol[3];
					}
					else if(mtex->blendtype==MTEX_MUL) {
						R.matren->mirr= (facm+fact*Tr)*mat_colmir->mirr;
						R.matren->mirg= (facm+fact*Tg)*mat_colmir->mirg;
						R.matren->mirb= (facm+fact*Tb)*mat_colmir->mirb;
					}
					else {
						R.matren->mirr= (fact*Tr + mat_colmir->mirr);
						R.matren->mirg= (fact*Tg + mat_colmir->mirg);
						R.matren->mirb= (fact*Tb + mat_colmir->mirb);
					}
					mat_colmir= R.matren;
				}
			}
			if( (mtex->mapto & MAP_NORM) ) {
				if(tex->nor) {
					
					if(mtex->maptoneg & MAP_NORM) tex->norfac= -mtex->norfac;
					else tex->norfac= mtex->norfac;

					R.vn[0]+= Tnor*tex->norfac*tex->nor[0];
					R.vn[1]+= Tnor*tex->norfac*tex->nor[1];
					R.vn[2]+= Tnor*tex->norfac*tex->nor[2];
					
					Normalise(R.vn);
					
					/* this makes sure the bump is passed on to the next texture */
					R.orn[0]= R.vn[0];
					R.orn[1]= -R.vn[1];
					R.orn[2]= R.vn[2];
					
					/* reflection vector */
					RE_calc_R_ref();
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
						R.matren->ref= factt*mtex->def_var+ facmm*mat_ref->ref;
					else if(mtex->blendtype==MTEX_MUL)
						R.matren->ref= (facmul+factt)*mat_ref->ref;
					else {
						R.matren->ref= factt+mat_ref->ref;
						if(R.matren->ref<0.0) R.matren->ref= 0.0;
					}
					mat_ref= R.matren;
				}
				if(mtex->mapto & MAP_SPEC) {
					if(mtex->maptoneg & MAP_SPEC) {factt= facm; facmm= fact;}
					else {factt= fact; facmm= facm;}
					
					if(mtex->blendtype==MTEX_BLEND)
						R.matren->spec= factt*mtex->def_var+ facmm*mat_spec->spec;
					else if(mtex->blendtype==MTEX_MUL)
						R.matren->spec= (facmul+factt)*mat_spec->spec;
					else {
						R.matren->spec= factt+mat_spec->spec;
						if(R.matren->spec<0.0) R.matren->spec= 0.0;
					}
					mat_spec= R.matren;
				}
				if(mtex->mapto & MAP_EMIT) {
					if(mtex->maptoneg & MAP_EMIT) {factt= facm; facmm= fact;}
					else {factt= fact; facmm= facm;}
					
					if(mtex->blendtype==MTEX_BLEND)
						R.matren->emit= factt*mtex->def_var+ facmm*mat_emit->emit;
					else if(mtex->blendtype==MTEX_MUL)
						R.matren->emit= (facmul+factt)*mat_emit->emit;
					else {
						R.matren->emit= factt+mat_emit->emit;
						if(R.matren->emit<0.0) R.matren->emit= 0.0;
					}
					mat_emit= R.matren;
				}
				if(mtex->mapto & MAP_ALPHA) {
					if(mtex->maptoneg & MAP_ALPHA) {factt= facm; facmm= fact;}
					else {factt= fact; facmm= facm;}
					
					if(mtex->blendtype==MTEX_BLEND)
						R.matren->alpha= factt*mtex->def_var+ facmm*mat_alpha->alpha;
					else if(mtex->blendtype==MTEX_MUL)
						R.matren->alpha= (facmul+factt)*mat_alpha->alpha;
					else {
						R.matren->alpha= factt+mat_alpha->alpha;
						if(R.matren->alpha<0.0) R.matren->alpha= 0.0;
						else if(R.matren->alpha>1.0) R.matren->alpha= 1.0;
					}
					mat_alpha= R.matren;
				}
				if(mtex->mapto & MAP_HAR) {
					if(mtex->maptoneg & MAP_HAR) {factt= facm; facmm= fact;}
					else {factt= fact; facmm= facm;}
					
					if(mtex->blendtype==MTEX_BLEND) {
						R.matren->har= 128.0*factt*mtex->def_var+ facmm*mat_har->har;
					} else if(mtex->blendtype==MTEX_MUL) {
						R.matren->har= (facmul+factt)*mat_har->har;
					} else {
						R.matren->har= 128.0*factt+mat_har->har;
						if(R.matren->har<1) R.matren->har= 1;
					}
					mat_har= R.matren;
				}
			}
		}
		
		if(R.mat->septex) break;
	}
}

/* ------------------------------------------------------------------------- */

void do_halo_tex(HaloRen *har, float xn, float yn, float *colf)
{
	MTex *mtex;
	float texvec[3], dxt[3], dyt[3], fact, facm, dx;
	int rgb;
	
	mtex= har->mat->mtex[0];
	if(mtex->tex==0) return;
	/* no normal mapping */
	mtex->tex->nor= 0;
		
	texvec[0]= xn/har->rad;
	texvec[1]= yn/har->rad;
	texvec[2]= 0.0;
	
	R.osatex= (har->mat->texco & TEXCO_OSA);

	/* placement */
	if(mtex->projx) texvec[0]= mtex->size[0]*(texvec[mtex->projx-1]+mtex->ofs[0]);
	else texvec[0]= mtex->size[0]*(mtex->ofs[0]);
	
	if(mtex->projy) texvec[1]= mtex->size[1]*(texvec[mtex->projy-1]+mtex->ofs[1]);
	else texvec[1]= mtex->size[1]*(mtex->ofs[1]);
	
	if(mtex->projz) texvec[2]= mtex->size[2]*(texvec[mtex->projz-1]+mtex->ofs[2]);
	else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
	
	if(R.osatex) {
	
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


	if(mtex->tex->type==TEX_IMAGE) do_2d_mapping(mtex, texvec, dxt, dyt);
	
	rgb= multitex(mtex->tex, texvec, dxt, dyt);

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

	R.osatex= 0;
}

/* ------------------------------------------------------------------------- */

void do_sky_tex()
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
			co= R.lo;
			
			/* Grab the mapping settings for this texture */
			if(mtex->texco==TEXCO_OBJECT) {
				Object *ob= mtex->object;
				if(ob) {
					VECCOPY(tempvec, R.lo);
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
			if(mtex->tex->type==TEX_IMAGE) do_2d_mapping(mtex, texvec, dxt, dyt);
			
			rgb= multitex(mtex->tex, texvec, dxt, dyt);
			
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
void do_lamp_tex(LampRen *la, float *lavec)
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
			
			/* which coords */
			if(mtex->texco==TEXCO_OBJECT) {
				ob= mtex->object;
				if(ob) {
					co= tempvec;
					dx= dxt;
					dy= dyt;
					VECCOPY(tempvec, R.co);
					MTC_Mat4MulVecfl(ob->imat, tempvec);
					if(R.osatex) {
						VECCOPY(dxt, O.dxco);
						VECCOPY(dyt, O.dyco);
						MTC_Mat4Mul3Vecfl(ob->imat, dxt);
						MTC_Mat4Mul3Vecfl(ob->imat, dyt);
					}
				}
				else {
					co= R.co;
					dx= O.dxco; dy= O.dyco;
				}
			}
			else if(mtex->texco==TEXCO_GLOB) {
				co= R.gl; dx= O.dxco; dy= O.dyco;
				VECCOPY(R.gl, R.co);
				MTC_Mat4MulVecfl(R.viewinv, R.gl);
			}
			else if(mtex->texco==TEXCO_VIEW) {
				
				VECCOPY(tempvec, lavec);
				MTC_Mat3MulVecfl(la->imat, tempvec);
				
				tempvec[0]*= la->spottexfac;
				tempvec[1]*= la->spottexfac;
				co= tempvec; 
				
				dx= dxt; dy= dyt;	
				if(R.osatex) {
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
			
			if(R.osatex) {
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
				do_2d_mapping(mtex, texvec, dxt, dyt);
				
				if(mtex->mapto & MAP_NORM) {
					/* the pointer defines if bump happens */
					tex->nor= R.vn;
					if(mtex->maptoneg & MAP_NORM) tex->norfac= -mtex->norfac;
					else tex->norfac= mtex->norfac;
				}
				else tex->nor= 0;
			}
			
			rgb= multitex(tex, texvec, dxt, dyt);
			
			
			
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
				la_col= la; /* Is it just me or is this a useless statement? */
			}
			
		}
	}
}

/* ------------------------------------------------------------------------- */

void externtex(MTex *mtex, float *vec)
{
	Tex *tex;
	float dxt[3], dyt[3], texvec[3];
	int rgb;
	
	tex= mtex->tex;
	if(tex==0) return;
	
	R.osatex= 0;
	R.vlr= 0;
	
	/* placement */
	if(mtex->projx) texvec[0]= mtex->size[0]*(vec[mtex->projx-1]+mtex->ofs[0]);
	else texvec[0]= mtex->size[0]*(mtex->ofs[0]);
	
	if(mtex->projy) texvec[1]= mtex->size[1]*(vec[mtex->projy-1]+mtex->ofs[1]);
	else texvec[1]= mtex->size[1]*(mtex->ofs[1]);
	
	if(mtex->projz) texvec[2]= mtex->size[2]*(vec[mtex->projz-1]+mtex->ofs[2]);
	else texvec[2]= mtex->size[2]*(mtex->ofs[2]);
	
	/* texture */
	if(tex->type==TEX_IMAGE) {
		do_2d_mapping(mtex, texvec, dxt, dyt);
		
		if(mtex->mapto & MAP_NORM) {
			/* the pointer defines if there's bump */
			tex->nor= R.vn;
			if(mtex->maptoneg & MAP_NORM) tex->norfac= -mtex->norfac;
			else tex->norfac= mtex->norfac;
		}
		else tex->nor= 0;
	}
	
	rgb= multitex(tex, texvec, dxt, dyt);
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

void render_realtime_texture()
{
	static Tex tex;
	static int firsttime= 1;
	float texvec[2], dx[2], dy[2];
	
	if(firsttime) {
		default_tex(&tex);
		tex.type= TEX_IMAGE;
		firsttime= 0;
	}
	
	tex.ima = R.vlr->tface->tpage;
	if(tex.ima) {
		
		texvec[0]= 0.5+0.5*R.uv[0];
		texvec[1]= 0.5+0.5*R.uv[1];
		if(R.osatex) {
			dx[0]= 0.5*O.dxuv[0];
			dx[1]= 0.5*O.dxuv[1];
			dy[0]= 0.5*O.dyuv[0];
			dy[1]= 0.5*O.dyuv[1];
		}
		
		if(R.osatex) imagewraposa(&tex, texvec, dx, dy); 
		else imagewrap(&tex, texvec); 
		
		R.vcol[0]*= Tr;
		R.vcol[1]*= Tg;
		R.vcol[2]*= Tb;
	}
	
	
}

/* eof */
