
/*  material.c
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

#include <string.h>
#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_curve_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"

#include "BKE_bad_level_calls.h"
#include "BKE_utildefines.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BKE_mesh.h"
#include "BKE_library.h"
#include "BKE_displist.h"
#include "BPY_extern.h"
#include "BKE_material.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void free_material(Material *ma)
{
	int a;
	MTex *mtex;

	BPY_free_scriptlink(&ma->scriptlink);
	
	if(ma->ren) MEM_freeN(ma->ren);
	ma->ren= 0;
	
	for(a=0; a<8; a++) {
		mtex= ma->mtex[a];
		if(mtex && mtex->tex) mtex->tex->id.us--;
		if(mtex) MEM_freeN(mtex);
	}
}

void init_material(Material *ma)
{
	ma->lay= 1;
	ma->r= ma->g= ma->b= ma->ref= 0.8;
	ma->specr= ma->specg= ma->specb= 1.0;
	ma->mirr= ma->mirg= ma->mirb= 1.0;
	ma->amb= 0.5;
	ma->alpha= 1.0;
	ma->spec= ma->hasize= 0.5;
	ma->har= 50;
	ma->starc= ma->ringc= 4;
	ma->linec= 12;
	ma->flarec= 1;
	ma->flaresize= ma->subsize= 1.0;
	ma->friction= 0.5;
	ma->refrac= 4.0;
	ma->roughness= 0.5;
	ma->param[0]= 0.5;
	ma->param[1]= 0.1;
	ma->param[2]= 0.5;
	ma->param[3]= 0.1;
	
	
	ma->mode= MA_TRACEBLE+MA_SHADOW+MA_RADIO;	
}

Material *add_material(char *name)
{
	Material *ma;

	ma= alloc_libblock(&G.main->mat, ID_MA, name);
	
	init_material(ma);
	
	return ma;	
}

Material *copy_material(Material *ma)
{
	Material *man;
	int a;
	
	man= copy_libblock(ma);
	
	id_us_plus((ID *)man->ipo);
	
	for(a=0; a<8; a++) {
		if(ma->mtex[a]) {
			man->mtex[a]= MEM_mallocN(sizeof(MTex), "copymaterial");
			memcpy(man->mtex[a], ma->mtex[a], sizeof(MTex));
			id_us_plus((ID *)man->mtex[a]->tex);
		}
	}
	
	BPY_copy_scriptlink(&ma->scriptlink);
	
	return man;
}

void make_local_material(Material *ma)
{
	Object *ob;
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	Material *man;
	int a, local=0, lib=0;

	/* - only lib users: do nothing
	    * - only local users: set flag
	    * - mixed: make copy
	    */
	
	if(ma->id.lib==0) return;
	if(ma->id.us==1) {
		ma->id.lib= 0;
		ma->id.flag= LIB_LOCAL;
		new_id(0, (ID *)ma, 0);
		for(a=0; a<8; a++) {
			if(ma->mtex[a]) id_lib_extern((ID *)ma->mtex[a]->tex);
		}
		
		return;
	}
	
	/* test objects */
	ob= G.main->object.first;
	while(ob) {
		if(ob->mat) {
			for(a=0; a<ob->totcol; a++) {
				if(ob->mat[a]==ma) {
					if(ob->id.lib) lib= 1;
					else local= 1;
				}
			}
		}
		ob= ob->id.next;
	}
	/* test meshes */
	me= G.main->mesh.first;
	while(me) {
		if(me->mat) {
			for(a=0; a<me->totcol; a++) {
				if(me->mat[a]==ma) {
					if(me->id.lib) lib= 1;
					else local= 1;
				}
			}
		}
		me= me->id.next;
	}
	/* test curves */
	cu= G.main->curve.first;
	while(cu) {
		if(cu->mat) {
			for(a=0; a<cu->totcol; a++) {
				if(cu->mat[a]==ma) {
					if(cu->id.lib) lib= 1;
					else local= 1;
				}
			}
		}
		cu= cu->id.next;
	}
	/* test mballs */
	mb= G.main->mball.first;
	while(mb) {
		if(mb->mat) {
			for(a=0; a<mb->totcol; a++) {
				if(mb->mat[a]==ma) {
					if(mb->id.lib) lib= 1;
					else local= 1;
				}
			}
		}
		mb= mb->id.next;
	}
	
	if(local && lib==0) {
		ma->id.lib= 0;
		ma->id.flag= LIB_LOCAL;
		
		for(a=0; a<8; a++) {
			if(ma->mtex[a]) id_lib_extern((ID *)ma->mtex[a]->tex);
		}
		
		new_id(0, (ID *)ma, 0);
	}
	else if(local && lib) {
		man= copy_material(ma);
		man->id.us= 0;
		
		/* do objects */
		ob= G.main->object.first;
		while(ob) {
			if(ob->mat) {
				for(a=0; a<ob->totcol; a++) {
					if(ob->mat[a]==ma) {
						if(ob->id.lib==0) {
							ob->mat[a]= man;
							man->id.us++;
							ma->id.us--;
						}
					}
				}
			}
			ob= ob->id.next;
		}
		/* do meshes */
		me= G.main->mesh.first;
		while(me) {
			if(me->mat) {
				for(a=0; a<me->totcol; a++) {
					if(me->mat[a]==ma) {
						if(me->id.lib==0) {
							me->mat[a]= man;
							man->id.us++;
							ma->id.us--;
						}
					}
				}
			}
			me= me->id.next;
		}
		/* do curves */
		cu= G.main->curve.first;
		while(cu) {
			if(cu->mat) {
				for(a=0; a<cu->totcol; a++) {
					if(cu->mat[a]==ma) {
						if(cu->id.lib==0) {
							cu->mat[a]= man;
							man->id.us++;
							ma->id.us--;
						}
					}
				}
			}
			cu= cu->id.next;
		}
		/* do mballs */
		mb= G.main->mball.first;
		while(mb) {
			if(mb->mat) {
				for(a=0; a<mb->totcol; a++) {
					if(mb->mat[a]==ma) {
						if(mb->id.lib==0) {
							mb->mat[a]= man;
							man->id.us++;
							ma->id.us--;
						}
					}
				}
			}
			mb= mb->id.next;
		}
	}
}

Material ***give_matarar(Object *ob)
{
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	
	if(ob->type==OB_MESH) {
		me= ob->data;
		return &(me->mat);
	}
	else if ELEM3(ob->type, OB_CURVE, OB_FONT, OB_SURF) {
		cu= ob->data;
		return &(cu->mat);
	}
	else if(ob->type==OB_MBALL) {
		mb= ob->data;
		return &(mb->mat);
	}
	return 0;
}

short *give_totcolp(Object *ob)
{
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	
	if(ob->type==OB_MESH) {
		me= ob->data;
		return &(me->totcol);
	}
	else if ELEM3(ob->type, OB_CURVE, OB_FONT, OB_SURF) {
		cu= ob->data;
		return &(cu->totcol);
	}
	else if(ob->type==OB_MBALL) {
		mb= ob->data;
		return &(mb->totcol);
	}
	return 0;
}

Material *give_current_material(Object *ob, int act)
{
	Material ***matarar, *ma;
	
	if(ob==0) return 0;
	if(ob->totcol==0) return 0;
	
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
	
	return ma;
}

ID *material_from(Object *ob, int act)
{

	if(ob==0) return 0;

	if(ob->totcol==0) return ob->data;
	if(act==0) act= 1;

	if( BTST(ob->colbits, act-1) ) return (ID *)ob;
	else return ob->data;
}

/* GS reads the memory pointed at in a specific ordering. There are,
 * however two definitions for it. I have jotted them down here, both,
 * but I think the first one is actually used. The thing is that
 * big-endian systems might read this the wrong way round. OTOH, we
 * constructed the IDs that are read out with this macro explicitly as
 * well. I expect we'll sort it out soon... */

/* from blendef: */
#define GS(a)	(*((short *)(a)))

/* from misc_util: flip the bytes from x  */
/*  #define GS(x) (((unsigned char *)(x))[0] << 8 | ((unsigned char *)(x))[1]) */

void test_object_materials(ID *id)
{
	/* make the ob mat-array same size as 'ob->data' mat-array */
	Object *ob;
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	Material **newmatar;
	int totcol=0;

	if(id==0) return;

	if( GS(id->name)==ID_ME ) {
		me= (Mesh *)id;
		totcol= me->totcol;
	}
	else if( GS(id->name)==ID_CU ) {
		cu= (Curve *)id;
		totcol= cu->totcol;
	}
	else if( GS(id->name)==ID_MB ) {
		mb= (MetaBall *)id;
		totcol= mb->totcol;
	}
	else return;

	ob= G.main->object.first;
	while(ob) {
		
		if(ob->data==id) {
		
			if(totcol==0) {
				if(ob->totcol) {
					MEM_freeN(ob->mat);
					ob->mat= 0;
				}
			}
			else if(ob->totcol<totcol) {
				newmatar= MEM_callocN(sizeof(void *)*totcol, "newmatar");
				if(ob->totcol) {
					memcpy(newmatar, ob->mat, sizeof(void *)*ob->totcol);
					MEM_freeN(ob->mat);
				}
				ob->mat= newmatar;
			}
			ob->totcol= totcol;
			if(ob->totcol && ob->actcol==0) ob->actcol= 1;
			if(ob->actcol>ob->totcol) ob->actcol= ob->totcol;
		}
		ob= ob->id.next;
	}
}


void assign_material(Object *ob, Material *ma, int act)
{
	Material *mao, **matar, ***matarar;
	short *totcolp;

	if(act>MAXMAT) return;
	if(act<1) act= 1;
	
	/* test arraylens */
	
	totcolp= give_totcolp(ob);
	matarar= give_matarar(ob);
	
	if(totcolp==0 || matarar==0) return;
	
	if( act > *totcolp) {
		matar= MEM_callocN(sizeof(void *)*act, "matarray1");
		if( *totcolp) {
			memcpy(matar, *matarar, sizeof(void *)*( *totcolp ));
			MEM_freeN(*matarar);
		}
		*matarar= matar;
		*totcolp= act;
	}
	
	if(act > ob->totcol) {
		matar= MEM_callocN(sizeof(void *)*act, "matarray2");
		if( ob->totcol) {
			memcpy(matar, ob->mat, sizeof(void *)*( ob->totcol ));
			MEM_freeN(ob->mat);
		}
		ob->mat= matar;
		ob->totcol= act;
	}
	
	/* do it */

	if( BTST(ob->colbits, act-1) ) {	/* in object */
		mao= ob->mat[act-1];
		if(mao) mao->id.us--;
		ob->mat[act-1]= ma;
	}
	else {	/* in data */
		mao= (*matarar)[act-1];
		if(mao) mao->id.us--;
		(*matarar)[act-1]= ma;
	}
	id_us_plus((ID *)ma);
	test_object_materials(ob->data);
}

void new_material_to_objectdata(Object *ob)
{
	Material *ma;
	
	if(ob==0) return;
	if(ob->totcol>=MAXMAT) return;
	
	ma= give_current_material(ob, ob->actcol);
	if(ma==0) {
		ma= add_material("Material");
		ma->id.us= 0;
	}
	
	if(ob->actcol) {
		if( BTST(ob->colbits, ob->actcol-1) ) {
			ob->colbits= BSET(ob->colbits, ob->totcol);
		}
	}
	
	assign_material(ob, ma, ob->totcol+1);
	ob->actcol= ob->totcol;
}


void init_render_material(Material *ma)
{
	MTex *mtex;
	int a, needuv=0;
	
	if(ma->ren) return;

	if(ma->flarec==0) ma->flarec= 1;

	ma->ren= MEM_mallocN(sizeof(Material), "initrendermaterial");
	memcpy(ma->ren, ma, sizeof(Material));
	
	/* add all texcoflags from mtex */
	ma= ma->ren;
	ma->texco= 0;
	ma->mapto= 0;
	for(a=0; a<8; a++) {
		mtex= ma->mtex[a];
		if(mtex && mtex->tex) {
		
			/* force std. ref mapping for envmap */
			if(mtex->tex->type==TEX_ENVMAP) {
/* 				mtex->texco= TEXCO_REFL; */
/* 				mtex->projx= PROJ_X; */
/* 				mtex->projy= PROJ_Y; */
/* 				mtex->projz= PROJ_Z; */
/* 				mtex->mapping= MTEX_FLAT; */
			}
			/* do not test for mtex->object and set mtex->texco at TEXCO_ORCO: mtex is linked! */
			
			ma->texco |= mtex->texco;
			ma->mapto |= mtex->mapto;
			if(R.osa) {
				if ELEM3(mtex->tex->type, TEX_IMAGE, TEX_PLUGIN, TEX_ENVMAP) ma->texco |= TEXCO_OSA;
			}
			
			if(ma->texco & (511)) needuv= 1;
			
			if(mtex->object) mtex->object->flag |= OB_DO_IMAT;
			
		}
	}
	if(ma->mode & MA_ZTRA) {
		/* if(ma->alpha==0.0 || ma->alpha==1.0) */
		R.flag |= R_ZTRA;
	}
	if(ma->mode & MA_VERTEXCOLP) ma->mode |= MA_VERTEXCOL; 
	
	if(ma->mode & MA_RADIO) needuv= 1;
	
	if(ma->mode & (MA_VERTEXCOL|MA_FACETEXTURE)) {
		needuv= 1;
		if(R.osa) ma->texco |= TEXCO_OSA;		/* for texfaces */
	}
	if(needuv) ma->texco |= NEED_UV;

	ma->ambr= ma->amb*R.wrld.ambr;
	ma->ambg= ma->amb*R.wrld.ambg;
	ma->ambb= ma->amb*R.wrld.ambb;
	
}

void init_render_materials()
{
	Material *ma;
	
	ma= G.main->mat.first;
	while(ma) {
		if(ma->id.us) init_render_material(ma);
		ma= ma->id.next;
	}
	
}

void end_render_material(Material *ma)
{
	
	if(ma->ren) MEM_freeN(ma->ren);
	ma->ren= 0;

	if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
		if( !(ma->mode & MA_HALO) ) {
			ma->r= ma->g= ma->b= 1.0;
		}
	}
}

void end_render_materials()
{
	Material *ma;
	
	ma= G.main->mat.first;
	while(ma) {
		if(ma->id.us) end_render_material(ma);
		ma= ma->id.next;
	}
	
}


/* ****************** */

char colname_array[125][20]= {
"Black","DarkRed","HalveRed","Red","Red",
"DarkGreen","DarkOlive","Brown","Chocolate","OrangeRed",
"HalveGreen","GreenOlive","DryOlive","Goldenrod","DarkOrange",
"LightGreen","Chartreuse","YellowGreen","Yellow","Gold",
"Green","LawnGreen","GreenYellow","LightOlive","Yellow",
"DarkBlue","DarkPurple","HotPink","VioletPink","RedPink",
"SlateGray","DarkGrey","PalePurple","IndianRed","Tomato",
"SeaGreen","PaleGreen","GreenKhaki","LightBrown","LightSalmon",
"SpringGreen","PaleGreen","MediumOlive","YellowBrown","LightGold",
"LightGreen","LightGreen","LightGreen","GreenYellow","PaleYellow",
"HalveBlue","DarkSky","HalveMagenta","VioletRed","DeepPink",
"SteelBlue","SkyBlue","Orchid","LightHotPink","HotPink",
"SeaGreen","SlateGray","MediumGrey","Burlywood","LightPink",
"SpringGreen","Aquamarine","PaleGreen","Khaki","PaleOrange",
"SpringGreen","SeaGreen","PaleGreen","PaleWhite","YellowWhite",
"LightBlue","Purple","MediumOrchid","Magenta","Magenta",
"RoyalBlue","SlateBlue","MediumOrchid","Orchid","Magenta",
"DeepSkyBlue","LightSteelBlue","LightSkyBlue","Violet","LightPink",
"Cyaan","DarkTurquoise","SkyBlue","Grey","Snow",
"Mint","Mint","Aquamarine","MintCream","Ivory",
"Blue","Blue","DarkMagenta","DarkOrchid","Magenta",
"SkyBlue","RoyalBlue","LightSlateBlue","MediumOrchid","Magenta",
"DodgerBlue","SteelBlue","MediumPurple","PalePurple","Plum",
"DeepSkyBlue","PaleBlue","LightSkyBlue","PalePurple","Thistle",
"Cyan","ColdBlue","PaleTurquoise","GhostWhite","White"
};

void automatname(Material *ma)
{
	int nr, r, g, b;
	float ref;
	
	if(ma==0) return;
	if(ma->mode & MA_SHLESS) ref= 1.0;
	else ref= ma->ref;
	
	r= (int)(4.99*(ref*ma->r));
	g= (int)(4.99*(ref*ma->g));
	b= (int)(4.99*(ref*ma->b));
	nr= r + 5*g + 25*b;
	if(nr>124) nr= 124;
	new_id(&G.main->mat, (ID *)ma, colname_array[nr]);
	
}


void delete_material_index()
{
	Material *mao, ***matarar;
	Object *ob, *obt;
	Mesh *me;
	Curve *cu;
	Nurb *nu;
	MFace *mface;
	short *totcolp;
	int a, actcol;
	
	if(G.obedit) {
		error("Unable to perform function in EditMode");
		return;
	}
	ob= ((G.scene->basact)? (G.scene->basact->object) : 0) ;
	if(ob==0 || ob->totcol==0) return;
	
	/* take a mesh/curve/mball as starting point, remove 1 index,
	 * AND with all objects that share the ob->data
	 * 
	 * after that check indices in mesh/curve/mball!!!
	 */
	
	totcolp= give_totcolp(ob);
	matarar= give_matarar(ob);

	/* we delete the actcol */
	if(ob->totcol) {
		mao= (*matarar)[ob->actcol-1];
		if(mao) mao->id.us--;
	}
	
	for(a=ob->actcol; a<ob->totcol; a++) {
		(*matarar)[a-1]= (*matarar)[a];
	}
	(*totcolp)--;
	
	if(*totcolp==0) {
		MEM_freeN(*matarar);
		*matarar= 0;
	}
	
	actcol= ob->actcol;
	obt= G.main->object.first;
	while(obt) {
	
		if(obt->data==ob->data) {
			
			/* WATCH IT: do not use actcol from ob or from obt (can become zero) */
			mao= obt->mat[actcol-1];
			if(mao) mao->id.us--;
		
			for(a=actcol; a<obt->totcol; a++) obt->mat[a-1]= obt->mat[a];
			obt->totcol--;
			if(obt->actcol > obt->totcol) obt->actcol= obt->totcol;
			
			if(obt->totcol==0) {
				MEM_freeN(obt->mat);
				obt->mat= 0;
			}
		}
		obt= obt->id.next;
	}
	allqueue(REDRAWBUTSMAT, 0);
	

	/* check indices from mesh */

	if(ob->type==OB_MESH) {
		me= get_mesh(ob);
		mface= me->mface;
		a= me->totface;
		while(a--) {
			if(mface->mat_nr && mface->mat_nr>=actcol-1) mface->mat_nr--;
			mface++;
		}
		makeDispList(ob);
	}
	else if ELEM(ob->type, OB_CURVE, OB_SURF) {
		cu= ob->data;
		nu= cu->nurb.first;
		
		while(nu) {
			if(nu->mat_nr && nu->mat_nr>=actcol-1) nu->mat_nr--;
			nu= nu->next;
		}
		makeDispList(ob);
	}
}
