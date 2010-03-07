/*  displist.c
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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"

#include "DNA_texture_types.h"
#include "DNA_meta_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_listBase.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_view3d_types.h"
#include "DNA_lattice_types.h"
#include "DNA_key_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_edgehash.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_displist.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_object.h"
#include "BKE_world.h"
#include "BKE_mesh.h"
#include "BKE_effect.h"
#include "BKE_mball.h"
#include "BKE_material.h"
#include "BKE_curve.h"
#include "BKE_key.h"
#include "BKE_anim.h"
#include "BKE_screen.h"
#include "BKE_texture.h"
#include "BKE_library.h"
#include "BKE_font.h"
#include "BKE_lattice.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"
#include "BKE_modifier.h"
#include "BKE_customdata.h"

#include "RE_pipeline.h"
#include "RE_shader_ext.h"

#include "BLO_sys_types.h" // for intptr_t support


static void boundbox_displist(Object *ob);

void free_disp_elem(DispList *dl)
{
	if(dl) {
		if(dl->verts) MEM_freeN(dl->verts);
		if(dl->nors) MEM_freeN(dl->nors);
		if(dl->index) MEM_freeN(dl->index);
		if(dl->col1) MEM_freeN(dl->col1);
		if(dl->col2) MEM_freeN(dl->col2);
		if(dl->bevelSplitFlag) MEM_freeN(dl->bevelSplitFlag);
		MEM_freeN(dl);
	}
}

void freedisplist(ListBase *lb)
{
	DispList *dl;

	dl= lb->first;
	while(dl) {
		BLI_remlink(lb, dl);
		free_disp_elem(dl);
		dl= lb->first;
	}
}

DispList *find_displist_create(ListBase *lb, int type)
{
	DispList *dl;
	
	dl= lb->first;
	while(dl) {
		if(dl->type==type) return dl;
		dl= dl->next;
	}

	dl= MEM_callocN(sizeof(DispList), "find_disp");
	dl->type= type;
	BLI_addtail(lb, dl);

	return dl;
}

DispList *find_displist(ListBase *lb, int type)
{
	DispList *dl;
	
	dl= lb->first;
	while(dl) {
		if(dl->type==type) return dl;
		dl= dl->next;
	}

	return 0;
}

int displist_has_faces(ListBase *lb)
{
	DispList *dl;
	for(dl= lb->first; dl; dl= dl->next) {
		if ELEM3(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)
			return 1;
	}
	return 0;
}

void copy_displist(ListBase *lbn, ListBase *lb)
{
	DispList *dln, *dl;
	
	lbn->first= lbn->last= 0;
	
	dl= lb->first;
	while(dl) {
		
		dln= MEM_dupallocN(dl);
		BLI_addtail(lbn, dln);
		dln->verts= MEM_dupallocN(dl->verts);
		dln->nors= MEM_dupallocN(dl->nors);
		dln->index= MEM_dupallocN(dl->index);
		dln->col1= MEM_dupallocN(dl->col1);
		dln->col2= MEM_dupallocN(dl->col2);
		
		dl= dl->next;
	}
}

void addnormalsDispList(Object *ob, ListBase *lb)
{
	DispList *dl = NULL;
	float *vdata, *ndata, nor[3];
	float *v1, *v2, *v3, *v4;
	float *n1, *n2, *n3, *n4;
	int a, b, p1, p2, p3, p4;


	dl= lb->first;
	
	while(dl) {
		if(dl->type==DL_INDEX3) {
			if(dl->nors==NULL) {
				dl->nors= MEM_callocN(sizeof(float)*3, "dlnors");
				if(dl->verts[2]<0.0) dl->nors[2]= -1.0;
				else dl->nors[2]= 1.0;
			}
		}
		else if(dl->type==DL_SURF) {
			if(dl->nors==NULL) {
				dl->nors= MEM_callocN(sizeof(float)*3*dl->nr*dl->parts, "dlnors");
				
				vdata= dl->verts;
				ndata= dl->nors;
				
				for(a=0; a<dl->parts; a++) {
					
					if (surfindex_displist(dl, a, &b, &p1, &p2, &p3, &p4)==0)
						break;
	
					v1= vdata+ 3*p1; 
					n1= ndata+ 3*p1;
					v2= vdata+ 3*p2; 
					n2= ndata+ 3*p2;
					v3= vdata+ 3*p3; 
					n3= ndata+ 3*p3;
					v4= vdata+ 3*p4; 
					n4= ndata+ 3*p4;
					
					for(; b<dl->nr; b++) {
	
						normal_quad_v3( nor,v1, v3, v4, v2);
	
						add_v3_v3v3(n1, n1, nor);
						add_v3_v3v3(n2, n2, nor);
						add_v3_v3v3(n3, n3, nor);
						add_v3_v3v3(n4, n4, nor);
	
						v2= v1; v1+= 3;
						v4= v3; v3+= 3;
						n2= n1; n1+= 3;
						n4= n3; n3+= 3;
					}
				}
				a= dl->parts*dl->nr;
				v1= ndata;
				while(a--) {
					normalize_v3(v1);
					v1+= 3;
				}
			}
		}
		dl= dl->next;
	}
}

void count_displist(ListBase *lb, int *totvert, int *totface)
{
	DispList *dl;
	
	dl= lb->first;
	while(dl) {
		
		switch(dl->type) {
			case DL_SURF:
				*totvert+= dl->nr*dl->parts;
				*totface+= (dl->nr-1)*(dl->parts-1);
				break;
			case DL_INDEX3:
			case DL_INDEX4:
				*totvert+= dl->nr;
				*totface+= dl->parts;
				break;
			case DL_POLY:
			case DL_SEGM:
				*totvert+= dl->nr*dl->parts;
		}
		
		dl= dl->next;
	}
}

int surfindex_displist(DispList *dl, int a, int *b, int *p1, int *p2, int *p3, int *p4)
{
	if((dl->flag & DL_CYCL_V)==0 && a==(dl->parts)-1) {
		return 0;
	}
	
	if(dl->flag & DL_CYCL_U) {
		(*p1)= dl->nr*a;
		(*p2)= (*p1)+ dl->nr-1;
		(*p3)= (*p1)+ dl->nr;
		(*p4)= (*p2)+ dl->nr;
		(*b)= 0;
	} else {
		(*p2)= dl->nr*a;
		(*p1)= (*p2)+1;
		(*p4)= (*p2)+ dl->nr;
		(*p3)= (*p1)+ dl->nr;
		(*b)= 1;
	}
	
	if( (dl->flag & DL_CYCL_V) && a==dl->parts-1) {			    \
		(*p3)-= dl->nr*dl->parts;				    \
		(*p4)-= dl->nr*dl->parts;				    \
	}
	
	return 1;
}

/* ***************************** shade displist. note colors now are in rgb(a) order ******************** */

/* create default shade input... save cpu cycles with ugly global */
/* XXXX bad code warning: local ShadeInput initialize... */
static ShadeInput shi;
static void init_fastshade_shadeinput(Render *re)
{
	memset(&shi, 0, sizeof(ShadeInput));
	shi.lay= RE_GetScene(re)->lay;
	shi.view[2]= -1.0f;
	shi.passflag= SCE_PASS_COMBINED;
	shi.combinedflag= -1;
}

static Render *fastshade_get_render(Scene *scene)
{
	// XXX 2.5: this crashes combined with previewrender
	// due to global R so disabled for now
#if 0
	/* XXX ugly global still, but we can't do preview while rendering */
	if(G.rendering==0) {
		
		Render *re= RE_GetRender("_Shade View_", RE_SLOT_DEFAULT);
		if(re==NULL) {
			re= RE_NewRender("_Shade View_", RE_SLOT_DEFAULT);
		
			RE_Database_Baking(re, scene, 0, 0);	/* 0= no faces */
		}
		return re;
	}
#endif
	
	return NULL;
}

/* called on file reading */
void fastshade_free_render(void)
{
	Render *re= RE_GetRender("_Shade View_", RE_SLOT_DEFAULT);
	
	if(re) {
		RE_Database_Free(re);
		RE_FreeRender(re);
	}
}

static int fastshade_customdata_layer_num(int n, int active)
{   
	/* make the active layer the first */
	if (n == active) return 0;
	else if (n < active) return n+1;
	else return n;
}

static void fastshade_customdata(CustomData *fdata, int a, int j, Material *ma)
{
	CustomDataLayer *layer;
	MTFace *mtface;
	int index, n, needuv= ma->texco & TEXCO_UV;
	char *vertcol;

	shi.totuv= 0;
	shi.totcol= 0;

	for(index=0; index<fdata->totlayer; index++) {
		layer= &fdata->layers[index];
		
		if(needuv && layer->type == CD_MTFACE && shi.totuv < MAX_MTFACE) {
			n= fastshade_customdata_layer_num(shi.totuv, layer->active_rnd);
			mtface= &((MTFace*)layer->data)[a];

			shi.uv[shi.totuv].uv[0]= 2.0f*mtface->uv[j][0]-1.0f;
			shi.uv[shi.totuv].uv[1]= 2.0f*mtface->uv[j][1]-1.0f;
			shi.uv[shi.totuv].uv[2]= 1.0f;

			shi.uv[shi.totuv].name= layer->name;
			shi.totuv++;
		}
		else if(layer->type == CD_MCOL && shi.totcol < MAX_MCOL) {
			n= fastshade_customdata_layer_num(shi.totcol, layer->active_rnd);
			vertcol= (char*)&((MCol*)layer->data)[a*4 + j];

			shi.col[shi.totcol].col[0]= ((float)vertcol[3])/255.0f;
			shi.col[shi.totcol].col[1]= ((float)vertcol[2])/255.0f;
			shi.col[shi.totcol].col[2]= ((float)vertcol[1])/255.0f;

			shi.col[shi.totcol].name= layer->name;
			shi.totcol++;
		}
	}

	if(needuv && shi.totuv == 0)
		VECCOPY(shi.uv[0].uv, shi.lo);

	if(shi.totcol)
		VECCOPY(shi.vcol, shi.col[0].col);
}

static void fastshade(float *co, float *nor, float *orco, Material *ma, char *col1, char *col2)
{
	ShadeResult shr;
	int a;
	
	VECCOPY(shi.co, co);
	shi.vn[0]= -nor[0];
	shi.vn[1]= -nor[1];
	shi.vn[2]= -nor[2];
	VECCOPY(shi.vno, shi.vn);
	VECCOPY(shi.facenor, shi.vn);
	
	if(ma->texco) {
		VECCOPY(shi.lo, orco);
		
		if(ma->texco & TEXCO_GLOB) {
			VECCOPY(shi.gl, shi.lo);
		}
		if(ma->texco & TEXCO_WINDOW) {
			VECCOPY(shi.winco, shi.lo);
		}
		if(ma->texco & TEXCO_STICKY) {
			VECCOPY(shi.sticky, shi.lo);
		}
		if(ma->texco & TEXCO_OBJECT) {
			VECCOPY(shi.co, shi.lo);
		}
		if(ma->texco & TEXCO_NORM) {
			VECCOPY(shi.orn, shi.vn);
		}
		if(ma->texco & TEXCO_REFL) {
			float inp= 2.0*(shi.vn[2]);
			shi.ref[0]= (inp*shi.vn[0]);
			shi.ref[1]= (inp*shi.vn[1]);
			shi.ref[2]= (-1.0+inp*shi.vn[2]);
		}
	}
	
	shi.mat= ma;	/* set each time... node shaders change it */
	RE_shade_external(NULL, &shi, &shr);
	
	a= 256.0f*(shr.combined[0]);
	col1[0]= CLAMPIS(a, 0, 255);
	a= 256.0f*(shr.combined[1]);
	col1[1]= CLAMPIS(a, 0, 255);
	a= 256.0f*(shr.combined[2]);
	col1[2]= CLAMPIS(a, 0, 255);
	
	if(col2) {
		shi.vn[0]= -shi.vn[0];
		shi.vn[1]= -shi.vn[1];
		shi.vn[2]= -shi.vn[2];
		
		shi.mat= ma;	/* set each time... node shaders change it */
		RE_shade_external(NULL, &shi, &shr);
		
		a= 256.0f*(shr.combined[0]);
		col2[0]= CLAMPIS(a, 0, 255);
		a= 256.0f*(shr.combined[1]);
		col2[1]= CLAMPIS(a, 0, 255);
		a= 256.0f*(shr.combined[2]);
		col2[2]= CLAMPIS(a, 0, 255);
	}
}

static void init_fastshade_for_ob(Render *re, Object *ob, int *need_orco_r, float mat[4][4], float imat[3][3])
{
	float tmat[4][4];
	float amb[3]= {0.0f, 0.0f, 0.0f};
	int a;
	
	/* initialize globals in render */
	RE_shade_external(re, NULL, NULL);

	/* initialize global here */
	init_fastshade_shadeinput(re);
	
	RE_DataBase_GetView(re, tmat);
	mul_m4_m4m4(mat, ob->obmat, tmat);
	
	invert_m4_m4(tmat, mat);
	copy_m3_m4(imat, tmat);
	if(ob->transflag & OB_NEG_SCALE) mul_m3_fl(imat, -1.0);
	
	if (need_orco_r) *need_orco_r= 0;
	for(a=0; a<ob->totcol; a++) {
		Material *ma= give_current_material(ob, a+1);
		if(ma) {
			init_render_material(ma, 0, amb);

			if(ma->texco & TEXCO_ORCO) {
				if (need_orco_r) *need_orco_r= 1;
			}
		}
	}
}

static void end_fastshade_for_ob(Object *ob)
{
	int a;
	
	for(a=0; a<ob->totcol; a++) {
		Material *ma= give_current_material(ob, a+1);
		if(ma)
			end_render_material(ma);
	}
}


static void mesh_create_shadedColors(Render *re, Object *ob, int onlyForMesh, unsigned int **col1_r, unsigned int **col2_r)
{
	Mesh *me= ob->data;
	DerivedMesh *dm;
	MVert *mvert;
	MFace *mface;
	unsigned int *col1, *col2;
	float *orco, *vnors, *nors, imat[3][3], mat[4][4], vec[3];
	int a, i, need_orco, totface, totvert;
	CustomDataMask dataMask = CD_MASK_BAREMESH | CD_MASK_MCOL
	                          | CD_MASK_MTFACE | CD_MASK_NORMAL;


	init_fastshade_for_ob(re, ob, &need_orco, mat, imat);

	if(need_orco)
		dataMask |= CD_MASK_ORCO;

	if (onlyForMesh)
		dm = mesh_get_derived_deform(RE_GetScene(re), ob, dataMask);
	else
		dm = mesh_get_derived_final(RE_GetScene(re), ob, dataMask);
	
	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);
	nors = dm->getFaceDataArray(dm, CD_NORMAL);
	totvert = dm->getNumVerts(dm);
	totface = dm->getNumFaces(dm);
	orco= dm->getVertDataArray(dm, CD_ORCO);

	if (onlyForMesh) {
		col1 = *col1_r;
		col2 = NULL;
	} else {
		*col1_r = col1 = MEM_mallocN(sizeof(*col1)*totface*4, "col1");

		if (col2_r && (me->flag & ME_TWOSIDED))
			col2 = MEM_mallocN(sizeof(*col2)*totface*4, "col2");
		else
			col2 = NULL;
		
		if (col2_r) *col2_r = col2;
	}

		/* vertexnormals */
	vnors= MEM_mallocN(totvert*3*sizeof(float), "vnors disp");
	for (a=0; a<totvert; a++) {
		MVert *mv = &mvert[a];
		float *vn= &vnors[a*3];
		float xn= mv->no[0]; 
		float yn= mv->no[1]; 
		float zn= mv->no[2];
		
			/* transpose ! */
		vn[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
		vn[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
		vn[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
		normalize_v3(vn);
	}		

	for (i=0; i<totface; i++) {
		extern Material defmaterial;	/* material.c */
		MFace *mf= &mface[i];
		Material *ma= give_current_material(ob, mf->mat_nr+1);
		int j, vidx[4], nverts= mf->v4?4:3;
		unsigned char *col1base= (unsigned char*) &col1[i*4];
		unsigned char *col2base= (unsigned char*) (col2?&col2[i*4]:NULL);
		float nor[3], n1[3];
		
		if(ma==NULL) ma= &defmaterial;
		
		vidx[0]= mf->v1;
		vidx[1]= mf->v2;
		vidx[2]= mf->v3;
		vidx[3]= mf->v4;

		if (nors) {
			VECCOPY(nor, &nors[i*3]);
		} else {
			if (mf->v4)
				normal_quad_v3( nor,mvert[mf->v1].co, mvert[mf->v2].co, mvert[mf->v3].co, mvert[mf->v4].co);
			else
				normal_tri_v3( nor,mvert[mf->v1].co, mvert[mf->v2].co, mvert[mf->v3].co);
		}

		n1[0]= imat[0][0]*nor[0]+imat[0][1]*nor[1]+imat[0][2]*nor[2];
		n1[1]= imat[1][0]*nor[0]+imat[1][1]*nor[1]+imat[1][2]*nor[2];
		n1[2]= imat[2][0]*nor[0]+imat[2][1]*nor[1]+imat[2][2]*nor[2];
		normalize_v3(n1);

		for (j=0; j<nverts; j++) {
			MVert *mv= &mvert[vidx[j]];
			char *col1= (char*)&col1base[j*4];
			char *col2= (char*)(col2base?&col2base[j*4]:NULL);
			float *vn = (mf->flag & ME_SMOOTH)?&vnors[3*vidx[j]]:n1;
			
			VECCOPY(vec, mv->co);
			mul_m4_v3(mat, vec);
			vec[0]+= 0.001*vn[0];
			vec[1]+= 0.001*vn[1];
			vec[2]+= 0.001*vn[2];

			fastshade_customdata(&dm->faceData, i, j, ma);
			fastshade(vec, vn, orco?&orco[vidx[j]*3]:mv->co, ma, col1, col2);
		}
	} 
	MEM_freeN(vnors);

	dm->release(dm);

	end_fastshade_for_ob(ob);
}

void shadeMeshMCol(Scene *scene, Object *ob, Mesh *me)
{
	Render *re= fastshade_get_render(scene);
	int a;
	char *cp;
	unsigned int *mcol= (unsigned int*)me->mcol;
	
	if(re) {
		mesh_create_shadedColors(re, ob, 1, &mcol, NULL);
		me->mcol= (MCol*)mcol;

		/* swap bytes */
		for(cp= (char *)me->mcol, a= 4*me->totface; a>0; a--, cp+=4) {
			SWAP(char, cp[0], cp[3]);
			SWAP(char, cp[1], cp[2]);
		}
	}
}

/* has base pointer, to check for layer */
/* called from drawobject.c */
void shadeDispList(Scene *scene, Base *base)
{
	Object *ob= base->object;
	DispList *dl, *dlob;
	Material *ma = NULL;
	Curve *cu;
	Render *re;
	float imat[3][3], mat[4][4], vec[3];
	float *fp, *nor, n1[3];
	unsigned int *col1;
	int a, need_orco;
	
	re= fastshade_get_render(scene);
	if(re==NULL)
		return;
	
	dl = find_displist(&ob->disp, DL_VERTCOL);
	if (dl) {
		BLI_remlink(&ob->disp, dl);
		free_disp_elem(dl);
	}

	if(ob->type==OB_MESH) {
		dl= MEM_callocN(sizeof(DispList), "displistshade");
		dl->type= DL_VERTCOL;

		mesh_create_shadedColors(re, ob, 0, &dl->col1, &dl->col2);

		/* add dl to ob->disp after mesh_create_shadedColors, because it
		   might indirectly free ob->disp */
		BLI_addtail(&ob->disp, dl);
	}
	else {

		init_fastshade_for_ob(re, ob, &need_orco, mat, imat);
		
		if (ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		
			/* now we need the normals */
			cu= ob->data;
			dl= cu->disp.first;
			
			while(dl) {
				extern Material defmaterial;	/* material.c */
				
				dlob= MEM_callocN(sizeof(DispList), "displistshade");
				BLI_addtail(&ob->disp, dlob);
				dlob->type= DL_VERTCOL;
				dlob->parts= dl->parts;
				dlob->nr= dl->nr;
				
				if(dl->type==DL_INDEX3) {
					col1= dlob->col1= MEM_mallocN(sizeof(int)*dl->nr, "col1");
				}
				else {
					col1= dlob->col1= MEM_mallocN(sizeof(int)*dl->parts*dl->nr, "col1");
				}
				
			
				ma= give_current_material(ob, dl->col+1);
				if(ma==NULL) ma= &defmaterial;
				
				if(dl->type==DL_INDEX3) {
					if(dl->nors) {
						/* there's just one normal */
						n1[0]= imat[0][0]*dl->nors[0]+imat[0][1]*dl->nors[1]+imat[0][2]*dl->nors[2];
						n1[1]= imat[1][0]*dl->nors[0]+imat[1][1]*dl->nors[1]+imat[1][2]*dl->nors[2];
						n1[2]= imat[2][0]*dl->nors[0]+imat[2][1]*dl->nors[1]+imat[2][2]*dl->nors[2];
						normalize_v3(n1);
						
						fp= dl->verts;
						
						a= dl->nr;		
						while(a--) {
							VECCOPY(vec, fp);
							mul_m4_v3(mat, vec);
							
							fastshade(vec, n1, fp, ma, (char *)col1, NULL);
							
							fp+= 3; col1++;
						}
					}
				}
				else if(dl->type==DL_SURF) {
					if(dl->nors) {
						a= dl->nr*dl->parts;
						fp= dl->verts;
						nor= dl->nors;
						
						while(a--) {
							VECCOPY(vec, fp);
							mul_m4_v3(mat, vec);
							
							n1[0]= imat[0][0]*nor[0]+imat[0][1]*nor[1]+imat[0][2]*nor[2];
							n1[1]= imat[1][0]*nor[0]+imat[1][1]*nor[1]+imat[1][2]*nor[2];
							n1[2]= imat[2][0]*nor[0]+imat[2][1]*nor[1]+imat[2][2]*nor[2];
							normalize_v3(n1);
				
							fastshade(vec, n1, fp, ma, (char *)col1, NULL);
							
							fp+= 3; nor+= 3; col1++;
						}
					}
				}
				dl= dl->next;
			}
		}
		else if(ob->type==OB_MBALL) {
			/* there are normals already */
			dl= ob->disp.first;
			
			while(dl) {
				
				if(dl->type==DL_INDEX4) {
					if(dl->nors) {
						extern Material defmaterial;	/* material.c */
						
						if(dl->col1) MEM_freeN(dl->col1);
						col1= dl->col1= MEM_mallocN(sizeof(int)*dl->nr, "col1");
				
						ma= give_current_material(ob, dl->col+1);
						if(ma==NULL) ma= &defmaterial;
						
						fp= dl->verts;
						nor= dl->nors;
						
						a= dl->nr;		
						while(a--) {
							VECCOPY(vec, fp);
							mul_m4_v3(mat, vec);
							
							/* transpose ! */
							n1[0]= imat[0][0]*nor[0]+imat[0][1]*nor[1]+imat[0][2]*nor[2];
							n1[1]= imat[1][0]*nor[0]+imat[1][1]*nor[1]+imat[1][2]*nor[2];
							n1[2]= imat[2][0]*nor[0]+imat[2][1]*nor[1]+imat[2][2]*nor[2];
							normalize_v3(n1);
						
							fastshade(vec, n1, fp, ma, (char *)col1, NULL);
							
							fp+= 3; col1++; nor+= 3;
						}
					}
				}
				dl= dl->next;
			}
		}
		
		end_fastshade_for_ob(ob);
	}
}

/* frees render and shade part of displists */
/* note: dont do a shade again, until a redraw happens */
void reshadeall_displist(Scene *scene)
{
	Base *base;
	Object *ob;
	
	fastshade_free_render();
	
	for(base= scene->base.first; base; base= base->next) {
		ob= base->object;

		if(ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL))
			freedisplist(&ob->disp);

		if(base->lay & scene->lay) {
			/* Metaballs have standard displist at the Object */
			if(ob->type==OB_MBALL) shadeDispList(scene, base);
		}
	}
}

/* ****************** make displists ********************* */

static void curve_to_displist(Curve *cu, ListBase *nubase, ListBase *dispbase)
{
	Nurb *nu;
	DispList *dl;
	BezTriple *bezt, *prevbezt;
	BPoint *bp;
	float *data;
	int a, len, resolu;
	
	nu= nubase->first;
	while(nu) {
		if(nu->hide==0) {
			
			if(G.rendering && cu->resolu_ren!=0) 
				resolu= cu->resolu_ren;
			else
				resolu= nu->resolu;
			
			if(!check_valid_nurb_u(nu));
			else if(nu->type == CU_BEZIER) {
				
				/* count */
				len= 0;
				a= nu->pntsu-1;
				if(nu->flagu & CU_CYCLIC) a++;

				prevbezt= nu->bezt;
				bezt= prevbezt+1;
				while(a--) {
					if(a==0 && (nu->flagu & CU_CYCLIC)) bezt= nu->bezt;
					
					if(prevbezt->h2==HD_VECT && bezt->h1==HD_VECT) len++;
					else len+= resolu;
					
					if(a==0 && (nu->flagu & CU_CYCLIC)==0) len++;
					
					prevbezt= bezt;
					bezt++;
				}
				
				dl= MEM_callocN(sizeof(DispList), "makeDispListbez");
				/* len+1 because of 'forward_diff_bezier' function */
				dl->verts= MEM_callocN( (len+1)*3*sizeof(float), "dlverts");
				BLI_addtail(dispbase, dl);
				dl->parts= 1;
				dl->nr= len;
				dl->col= nu->mat_nr;
				dl->charidx= nu->charidx;

				data= dl->verts;

				if(nu->flagu & CU_CYCLIC) {
					dl->type= DL_POLY;
					a= nu->pntsu;
				}
				else {
					dl->type= DL_SEGM;
					a= nu->pntsu-1;
				}
				
				prevbezt= nu->bezt;
				bezt= prevbezt+1;
				
				while(a--) {
					if(a==0 && dl->type== DL_POLY) bezt= nu->bezt;
					
					if(prevbezt->h2==HD_VECT && bezt->h1==HD_VECT) {
						VECCOPY(data, prevbezt->vec[1]);
						data+= 3;
					}
					else {
						int j;
						for(j=0; j<3; j++) {
							forward_diff_bezier(	prevbezt->vec[1][j],
													prevbezt->vec[2][j],
													bezt->vec[0][j],
													bezt->vec[1][j],
													data+j, resolu, 3*sizeof(float));
						}
						
						data+= 3*resolu;
					}
					
					if(a==0 && dl->type==DL_SEGM) {
						VECCOPY(data, bezt->vec[1]);
					}
					
					prevbezt= bezt;
					bezt++;
				}
			}
			else if(nu->type == CU_NURBS) {
				len= (resolu*SEGMENTSU(nu));
				
				dl= MEM_callocN(sizeof(DispList), "makeDispListsurf");
				dl->verts= MEM_callocN(len*3*sizeof(float), "dlverts");
				BLI_addtail(dispbase, dl);
				dl->parts= 1;
				
				dl->nr= len;
				dl->col= nu->mat_nr;
				dl->charidx = nu->charidx;

				data= dl->verts;
				if(nu->flagu & CU_CYCLIC) dl->type= DL_POLY;
				else dl->type= DL_SEGM;
				makeNurbcurve(nu, data, NULL, NULL, resolu, 3*sizeof(float));
			}
			else if(nu->type == CU_POLY) {
				len= nu->pntsu;
				dl= MEM_callocN(sizeof(DispList), "makeDispListpoly");
				dl->verts= MEM_callocN(len*3*sizeof(float), "dlverts");
				BLI_addtail(dispbase, dl);
				dl->parts= 1;
				dl->nr= len;
				dl->col= nu->mat_nr;
				dl->charidx = nu->charidx;

				data= dl->verts;
				if(nu->flagu & CU_CYCLIC) dl->type= DL_POLY;
				else dl->type= DL_SEGM;
				
				a= len;
				bp= nu->bp;
				while(a--) {
					VECCOPY(data, bp->vec);
					bp++;
					data+= 3;
				}
			}
		}
		nu= nu->next;
	}
}


void filldisplist(ListBase *dispbase, ListBase *to)
{
	EditVert *eve, *v1, *vlast;
	EditFace *efa;
	DispList *dlnew=0, *dl;
	float *f1;
	int colnr=0, charidx=0, cont=1, tot, a, *index;
	intptr_t totvert;
	
	if(dispbase==0) return;
	if(dispbase->first==0) return;

	while(cont) {
		cont= 0;
		totvert=0;
		
		dl= dispbase->first;
		while(dl) {
	
			if(dl->type==DL_POLY) {
				if(charidx<dl->charidx) cont= 1;
				else if(charidx==dl->charidx) {
			
					colnr= dl->col;
					charidx= dl->charidx;
		
					/* make editverts and edges */
					f1= dl->verts;
					a= dl->nr;
					eve= v1= 0;
					
					while(a--) {
						vlast= eve;
						
						eve= BLI_addfillvert(f1);
						totvert++;
						
						if(vlast==0) v1= eve;
						else {
							BLI_addfilledge(vlast, eve);
						}
						f1+=3;
					}
				
					if(eve!=0 && v1!=0) {
						BLI_addfilledge(eve, v1);
					}
				}
			}
			dl= dl->next;
		}
		
		if(totvert && BLI_edgefill(0, 0)) { // XXX (obedit && obedit->actcol)?(obedit->actcol-1):0)) {

			/* count faces  */
			tot= 0;
			efa= fillfacebase.first;
			while(efa) {
				tot++;
				efa= efa->next;
			}

			if(tot) {
				dlnew= MEM_callocN(sizeof(DispList), "filldisplist");
				dlnew->type= DL_INDEX3;
				dlnew->col= colnr;
				dlnew->nr= totvert;
				dlnew->parts= tot;

				dlnew->index= MEM_mallocN(tot*3*sizeof(int), "dlindex");
				dlnew->verts= MEM_mallocN(totvert*3*sizeof(float), "dlverts");
				
				/* vert data */
				f1= dlnew->verts;
				totvert= 0;
				eve= fillvertbase.first;
				while(eve) {
					VECCOPY(f1, eve->co);
					f1+= 3;
	
					/* index number */
					eve->tmp.l = totvert;
					totvert++;
					
					eve= eve->next;
				}
				
				/* index data */
				efa= fillfacebase.first;
				index= dlnew->index;
				while(efa) {
					index[0]= (intptr_t)efa->v1->tmp.l;
					index[1]= (intptr_t)efa->v2->tmp.l;
					index[2]= (intptr_t)efa->v3->tmp.l;
					
					index+= 3;
					efa= efa->next;
				}
			}

			BLI_addhead(to, dlnew);
			
		}
		BLI_end_edgefill();

		charidx++;
	}
	
	/* do not free polys, needed for wireframe display */
	
}

static void bevels_to_filledpoly(Curve *cu, ListBase *dispbase)
{
	ListBase front, back;
	DispList *dl, *dlnew;
	float *fp, *fp1;
	int a, dpoly;
	
	front.first= front.last= back.first= back.last= 0;
	
	dl= dispbase->first;
	while(dl) {
		if(dl->type==DL_SURF) {
			if( (dl->flag & DL_CYCL_V) && (dl->flag & DL_CYCL_U)==0 ) {
				if( (cu->flag & CU_BACK) && (dl->flag & DL_BACK_CURVE) ) {
					dlnew= MEM_callocN(sizeof(DispList), "filldisp");
					BLI_addtail(&front, dlnew);
					dlnew->verts= fp1= MEM_mallocN(sizeof(float)*3*dl->parts, "filldisp1");
					dlnew->nr= dl->parts;
					dlnew->parts= 1;
					dlnew->type= DL_POLY;
					dlnew->col= dl->col;
					dlnew->charidx = dl->charidx;
					
					fp= dl->verts;
					dpoly= 3*dl->nr;
					
					a= dl->parts;
					while(a--) {
						VECCOPY(fp1, fp);
						fp1+= 3;
						fp+= dpoly;
					}
				}
				if( (cu->flag & CU_FRONT) && (dl->flag & DL_FRONT_CURVE) ) {
					dlnew= MEM_callocN(sizeof(DispList), "filldisp");
					BLI_addtail(&back, dlnew);
					dlnew->verts= fp1= MEM_mallocN(sizeof(float)*3*dl->parts, "filldisp1");
					dlnew->nr= dl->parts;
					dlnew->parts= 1;
					dlnew->type= DL_POLY;
					dlnew->col= dl->col;
					dlnew->charidx= dl->charidx;
					
					fp= dl->verts+3*(dl->nr-1);
					dpoly= 3*dl->nr;
					
					a= dl->parts;
					while(a--) {
						VECCOPY(fp1, fp);
						fp1+= 3;
						fp+= dpoly;
					}
				}
			}
		}
		dl= dl->next;
	}

	filldisplist(&front, dispbase);
	filldisplist(&back, dispbase);
	
	freedisplist(&front);
	freedisplist(&back);

	filldisplist(dispbase, dispbase);
	
}

static void curve_to_filledpoly(Curve *cu, ListBase *nurb, ListBase *dispbase)
{
	if(cu->flag & CU_3D) return;

	if(dispbase->first && ((DispList*) dispbase->first)->type==DL_SURF) {
		bevels_to_filledpoly(cu, dispbase);
	}
	else {
		filldisplist(dispbase, dispbase);
	}
}

/* taper rules:
  - only 1 curve
  - first point left, last point right
  - based on subdivided points in original curve, not on points in taper curve (still)
*/
float calc_taper(Scene *scene, Object *taperobj, int cur, int tot)
{
	Curve *cu;
	DispList *dl;
	
	if(taperobj==NULL) return 1.0;
	
	cu= taperobj->data;
	dl= cu->disp.first;
	if(dl==NULL) {
		makeDispListCurveTypes(scene, taperobj, 0);
		dl= cu->disp.first;
	}
	if(dl) {
		float fac= ((float)cur)/(float)(tot-1);
		float minx, dx, *fp;
		int a;
		
		/* horizontal size */
		minx= dl->verts[0];
		dx= dl->verts[3*(dl->nr-1)] - minx;
		if(dx>0.0) {
		
			fp= dl->verts;
			for(a=0; a<dl->nr; a++, fp+=3) {
				if( (fp[0]-minx)/dx >= fac) {
					/* interpolate with prev */
					if(a>0) {
						float fac1= (fp[-3]-minx)/dx;
						float fac2= (fp[0]-minx)/dx;
						if(fac1!=fac2)
							return fp[1]*(fac1-fac)/(fac1-fac2) + fp[-2]*(fac-fac2)/(fac1-fac2);
					}
					return fp[1];
				}
			}
			return fp[-2];	// last y coord
		}
	}
	
	return 1.0;
}

void makeDispListMBall(Scene *scene, Object *ob)
{
	if(!ob || ob->type!=OB_MBALL) return;

	freedisplist(&(ob->disp));
	
	if(ob->type==OB_MBALL) {
		if(ob==find_basis_mball(scene, ob)) {
			metaball_polygonize(scene, ob);
			tex_space_mball(ob);

			object_deform_mball(ob);
		}
	}
	
	boundbox_displist(ob);
}

static ModifierData *curve_get_tesselate_point(Scene *scene, Object *ob, int forRender, int editmode)
{
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	ModifierData *preTesselatePoint;
	int required_mode;

	if(forRender) required_mode = eModifierMode_Render;
	else required_mode = eModifierMode_Realtime;

	if(editmode) required_mode |= eModifierMode_Editmode;

	preTesselatePoint = NULL;
	for (; md; md=md->next) {
		if (!modifier_isEnabled(scene, md, required_mode)) continue;

		if (ELEM3(md->type, eModifierType_Hook, eModifierType_Softbody, eModifierType_MeshDeform)) {
			preTesselatePoint  = md;
		}
	}

	return preTesselatePoint;
}

static void curve_calc_modifiers_pre(Scene *scene, Object *ob, int forRender, float (**originalVerts_r)[3], float (**deformedVerts_r)[3], int *numVerts_r)
{
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	ModifierData *preTesselatePoint;
	Curve *cu= ob->data;
	ListBase *nurb= cu->editnurb?cu->editnurb:&cu->nurb;
	int numVerts = 0;
	int editmode = (!forRender && cu->editnurb);
	float (*originalVerts)[3] = NULL;
	float (*deformedVerts)[3] = NULL;
	float *keyVerts= NULL;
	int required_mode;

	if(forRender) required_mode = eModifierMode_Render;
	else required_mode = eModifierMode_Realtime;

	preTesselatePoint = curve_get_tesselate_point(scene, ob, forRender, editmode);
	
	if(editmode) required_mode |= eModifierMode_Editmode;

	if(cu->editnurb==NULL) {
		keyVerts= do_ob_key(scene, ob);

		if(keyVerts) {
			/* split coords from key data, the latter also includes
			   tilts, which is passed through in the modifier stack.
			   this is also the reason curves do not use a virtual
			   shape key modifier yet. */
			deformedVerts= curve_getKeyVertexCos(cu, nurb, keyVerts);
			originalVerts= MEM_dupallocN(deformedVerts);
		}
	}
	
	if (preTesselatePoint) {
		for (; md; md=md->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);

			md->scene= scene;
			
			if ((md->mode & required_mode) != required_mode) continue;
			if (mti->isDisabled && mti->isDisabled(md, forRender)) continue;
			if (mti->type!=eModifierTypeType_OnlyDeform) continue;

			if (!deformedVerts) {
				deformedVerts = curve_getVertexCos(cu, nurb, &numVerts);
				originalVerts = MEM_dupallocN(deformedVerts);
			}
			
			mti->deformVerts(md, ob, NULL, deformedVerts, numVerts, forRender, editmode);

			if (md==preTesselatePoint)
				break;
		}
	}

	if (deformedVerts)
		curve_applyVertexCos(cu, nurb, deformedVerts);
	if (keyVerts) /* these are not passed through modifier stack */
		curve_applyKeyVertexTilts(cu, nurb, keyVerts);
	
	if(keyVerts)
		MEM_freeN(keyVerts);

	*originalVerts_r = originalVerts;
	*deformedVerts_r = deformedVerts;
	*numVerts_r = numVerts;
}

static void curve_calc_modifiers_post(Scene *scene, Object *ob, ListBase *dispbase, int forRender, float (*originalVerts)[3], float (*deformedVerts)[3])
{
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	ModifierData *preTesselatePoint;
	Curve *cu= ob->data;
	ListBase *nurb= cu->editnurb?cu->editnurb:&cu->nurb;
	DispList *dl;
	int required_mode;
	int editmode = (!forRender && cu->editnurb);
	DerivedMesh *dm= NULL, *ndm;

	if(forRender) required_mode = eModifierMode_Render;
	else required_mode = eModifierMode_Realtime;

	preTesselatePoint = curve_get_tesselate_point(scene, ob, forRender, editmode);
	
	if(editmode) required_mode |= eModifierMode_Editmode;

	if (preTesselatePoint) {
		md = preTesselatePoint->next;
	}

	if (ob->derivedFinal) {
		ob->derivedFinal->release (ob->derivedFinal);
	}

	for (; md; md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		/* modifier depends on derived mesh and has got valid applyModifier call */
		int dmApplyMod = mti->type==eModifierTypeType_Constructive ||
			mti->type==eModifierTypeType_Nonconstructive;

		md->scene= scene;

		if ((md->mode & required_mode) != required_mode) continue;
		if (mti->isDisabled && mti->isDisabled(md, forRender)) continue;

		/* need to put all verts in 1 block for curve deform */
		/* we also need all verts in 1 block for derived mesh creation */
		if(md->type==eModifierType_Curve || dmApplyMod) {
			float *allverts = NULL, *fp;
			int totvert= 0;

			if (md->type==eModifierType_Curve || (dmApplyMod && !dm)) {
				for (dl=dispbase->first; dl; dl=dl->next)
					totvert+= (dl->type==DL_INDEX3)?dl->nr:dl->parts*dl->nr;

				fp= allverts= MEM_mallocN(totvert*sizeof(float)*3, "temp vert");
				for (dl=dispbase->first; dl; dl=dl->next) {
					int offs= 3 * ((dl->type==DL_INDEX3)?dl->nr:dl->parts*dl->nr);
					memcpy(fp, dl->verts, sizeof(float) * offs);
					fp+= offs;
				}
			}

			if (dmApplyMod) {
				if (!dm) {
					dm= CDDM_from_curve(ob);
					/*
					 * TODO: Maybe we should apply deformedVerts?
					 *       But for now it causes invalid working of SoftBody modifier
					 */
					CDDM_apply_vert_coords(dm, (float(*)[3]) allverts);
					CDDM_calc_normals(dm);
				}

				ndm = mti->applyModifier(md, ob, dm, forRender, editmode);

				if (dm && dm != ndm) /* Modifier  */
					dm->release (dm);
				dm = ndm;
			} else {
				mti->deformVerts(md, ob, NULL, (float(*)[3]) allverts, totvert, forRender, editmode);
			}

			if (allverts) {
				fp= allverts;
				for (dl=dispbase->first; dl; dl=dl->next) {
					int offs= 3 * ((dl->type==DL_INDEX3)?dl->nr:dl->parts*dl->nr);
					memcpy(dl->verts, fp, sizeof(float) * offs);
					fp+= offs;
				}
				MEM_freeN(allverts);
			}
		}
		else {
			if (dm) {
				float (*deformedVerts)[3] = NULL;
				int numVerts;

				numVerts = dm->getNumVerts(dm);
				deformedVerts =
					MEM_mallocN(sizeof(*deformedVerts) * numVerts, "dfmv");
				dm->getVertCos(dm, deformedVerts);

				mti->deformVerts(md, ob, dm, deformedVerts, numVerts, forRender, editmode);

				CDDM_apply_vert_coords(dm, deformedVerts);

				MEM_freeN(deformedVerts);
			} else {
				for (dl=dispbase->first; dl; dl=dl->next) {
					mti->deformVerts(md, ob, dm, (float(*)[3]) dl->verts, (dl->type==DL_INDEX3)?dl->nr:dl->parts*dl->nr, forRender, editmode);
				}
			}
		}
	}

	ob->derivedFinal = dm;

	if (deformedVerts) {
		curve_applyVertexCos(ob->data, nurb, originalVerts);
		MEM_freeN(originalVerts);
		MEM_freeN(deformedVerts);
	}
}

static void displist_surf_indices(DispList *dl)
{
	int a, b, p1, p2, p3, p4;
	int *index;
	
	dl->totindex= 0;
	
	index=dl->index= MEM_mallocN( 4*sizeof(int)*(dl->parts+1)*(dl->nr+1), "index array nurbs");
	
	for(a=0; a<dl->parts; a++) {
		
		if (surfindex_displist(dl, a, &b, &p1, &p2, &p3, &p4)==0)
			break;
		
		for(; b<dl->nr; b++, index+=4) {	
			index[0]= p1;
			index[1]= p2;
			index[2]= p4;
			index[3]= p3;
			
			dl->totindex++;
			
			p2= p1; p1++;
			p4= p3; p3++;

		}
	}
	
}

static DerivedMesh *create_orco_dm(Scene *scene, Object *ob)
{
	DerivedMesh *dm;
	float (*orco)[3];

	dm= CDDM_from_curve(ob);
	orco= (float(*)[3])make_orco_curve(scene, ob);

	CDDM_apply_vert_coords(dm, orco);
	CDDM_calc_normals(dm);
	MEM_freeN(orco);

	return dm;
}

static void add_orco_dm(Scene *scene, Object *ob, DerivedMesh *dm, DerivedMesh *orcodm)
{
	float (*orco)[3], (*layerorco)[3];
	int totvert, a;
	Curve *cu= ob->data;

	totvert= dm->getNumVerts(dm);

	if(orcodm) {
		orco= MEM_callocN(sizeof(float)*3*totvert, "dm orco");

		if(orcodm->getNumVerts(orcodm) == totvert)
			orcodm->getVertCos(orcodm, orco);
		else
			dm->getVertCos(dm, orco);
	}
	else {
		orco= (float(*)[3])make_orco_curve(scene, ob);
	}

	for(a=0; a<totvert; a++) {
		float *co = orco[a];
		co[0] = (co[0]-cu->loc[0])/cu->size[0];
		co[1] = (co[1]-cu->loc[1])/cu->size[1];
		co[2] = (co[2]-cu->loc[2])/cu->size[2];
	}

	if((layerorco = DM_get_vert_data_layer(dm, CD_ORCO))) {
		memcpy(layerorco, orco, sizeof(float)*totvert);
		MEM_freeN(orco);
	}
	else
		DM_add_vert_layer(dm, CD_ORCO, CD_ASSIGN, orco);
}

static void curve_calc_orcodm(Scene *scene, Object *ob, int forRender)
{
	/* this function represents logic of mesh's orcodm calculation */
	/* for displist-based objects */

	ModifierData *md = modifiers_getVirtualModifierList(ob);
	ModifierData *preTesselatePoint;
	Curve *cu= ob->data;
	int required_mode;
	int editmode = (!forRender && cu->editnurb);
	DerivedMesh *dm= ob->derivedFinal, *ndm, *orcodm= NULL;

	if(forRender) required_mode = eModifierMode_Render;
	else required_mode = eModifierMode_Realtime;

	preTesselatePoint = curve_get_tesselate_point(scene, ob, forRender, editmode);

	if(editmode) required_mode |= eModifierMode_Editmode;

	if (preTesselatePoint) {
		md = preTesselatePoint->next;
	}

	for (; md; md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		md->scene= scene;

		if ((md->mode & required_mode) != required_mode) continue;
		if (mti->isDisabled && mti->isDisabled(md, forRender)) continue;
		if (mti->type!=eModifierTypeType_Constructive) continue;

		if(!orcodm)
			orcodm= create_orco_dm(scene, ob);

		ndm = mti->applyModifier(md, ob, orcodm, forRender, 0);

		if(ndm) {
			/* if the modifier returned a new dm, release the old one */
			if(orcodm && orcodm != ndm) {
				orcodm->release(orcodm);
			}
			orcodm = ndm;
		}
	}

	/* add an orco layer if needed */
	add_orco_dm(scene, ob, dm, orcodm);

	if(orcodm)
		orcodm->release(orcodm);
}

void makeDispListSurf(Scene *scene, Object *ob, ListBase *dispbase, int forRender, int forOrco)
{
	ListBase *nubase;
	Nurb *nu;
	Curve *cu = ob->data;
	DispList *dl;
	float *data;
	int len;
	int numVerts;
	float (*originalVerts)[3];
	float (*deformedVerts)[3];
		
	if(!forRender && cu->editnurb)
		nubase= cu->editnurb;
	else
		nubase= &cu->nurb;

	if(!forOrco)
		curve_calc_modifiers_pre(scene, ob, forRender, &originalVerts, &deformedVerts, &numVerts);

	for (nu=nubase->first; nu; nu=nu->next) {
		if(forRender || nu->hide==0) {
			if(nu->pntsv==1) {
				len= SEGMENTSU(nu)*nu->resolu;
				
				dl= MEM_callocN(sizeof(DispList), "makeDispListsurf");
				dl->verts= MEM_callocN(len*3*sizeof(float), "dlverts");
				
				BLI_addtail(dispbase, dl);
				dl->parts= 1;
				dl->nr= len;
				dl->col= nu->mat_nr;
				dl->charidx= nu->charidx;
				dl->rt= nu->flag;
				
				data= dl->verts;
				if(nu->flagu & CU_CYCLIC) dl->type= DL_POLY;
				else dl->type= DL_SEGM;
				
				makeNurbcurve(nu, data, NULL, NULL, nu->resolu, 3*sizeof(float));
			}
			else {
				len= (nu->pntsu*nu->resolu) * (nu->pntsv*nu->resolv);
				
				dl= MEM_callocN(sizeof(DispList), "makeDispListsurf");
				dl->verts= MEM_callocN(len*3*sizeof(float), "dlverts");
				BLI_addtail(dispbase, dl);

				dl->col= nu->mat_nr;
				dl->charidx= nu->charidx;
				dl->rt= nu->flag;
				
				data= dl->verts;
				dl->type= DL_SURF;
				
				dl->parts= (nu->pntsu*nu->resolu);	/* in reverse, because makeNurbfaces works that way */
				dl->nr= (nu->pntsv*nu->resolv);
				if(nu->flagv & CU_CYCLIC) dl->flag|= DL_CYCL_U;	/* reverse too! */
				if(nu->flagu & CU_CYCLIC) dl->flag|= DL_CYCL_V;

				makeNurbfaces(nu, data, 0);
				
				/* gl array drawing: using indices */
				displist_surf_indices(dl);
			}
		}
	}

	if (!forRender) {
		tex_space_curve(cu);
	}

	if(!forOrco)
		curve_calc_modifiers_post(scene, ob, dispbase, forRender, originalVerts, deformedVerts);
}

void makeDispListCurveTypes(Scene *scene, Object *ob, int forOrco)
{
	Curve *cu = ob->data;
	ListBase *dispbase;
	
	/* we do allow duplis... this is only displist on curve level */
	if(!ELEM3(ob->type, OB_SURF, OB_CURVE, OB_FONT)) return;

	freedisplist(&(ob->disp));
	dispbase= &(cu->disp);
	freedisplist(dispbase);
	
	if(ob->type==OB_SURF) {
		makeDispListSurf(scene, ob, dispbase, 0, forOrco);
	}
	else if (ELEM(ob->type, OB_CURVE, OB_FONT)) {
		ListBase dlbev;
		ListBase *nubase;
		float (*originalVerts)[3];
		float (*deformedVerts)[3];
		int numVerts;

		if(cu->editnurb)
			nubase= cu->editnurb;
		else
			nubase= &cu->nurb;
		
		BLI_freelistN(&(cu->bev));
		
		if(cu->path) free_path(cu->path);
		cu->path= NULL;
		
		if(ob->type==OB_FONT) BKE_text_to_curve(scene, ob, 0);
		
		if(!forOrco) curve_calc_modifiers_pre(scene, ob, 0, &originalVerts, &deformedVerts, &numVerts);

		makeBevelList(ob);

		/* If curve has no bevel will return nothing */
		makebevelcurve(scene, ob, &dlbev);

		/* no bevel or extrude, and no width correction? */
		if (!dlbev.first && cu->width==1.0f) {
			curve_to_displist(cu, nubase, dispbase);
		} else {
			float widfac= cu->width-1.0;
			BevList *bl= cu->bev.first;
			Nurb *nu= nubase->first;

			for (; bl && nu; bl=bl->next,nu=nu->next) {
				DispList *dl;
				float *fp1, *data;
				BevPoint *bevp;
				int a,b;
				
				if (bl->nr) { /* blank bevel lists can happen */
					
					/* exception handling; curve without bevel or extrude, with width correction */
					if(dlbev.first==NULL) {
						dl= MEM_callocN(sizeof(DispList), "makeDispListbev");
						dl->verts= MEM_callocN(3*sizeof(float)*bl->nr, "dlverts");
						BLI_addtail(dispbase, dl);
						
						if(bl->poly!= -1) dl->type= DL_POLY;
						else dl->type= DL_SEGM;
						
						if(dl->type==DL_SEGM) dl->flag = (DL_FRONT_CURVE|DL_BACK_CURVE);
						
						dl->parts= 1;
						dl->nr= bl->nr;
						dl->col= nu->mat_nr;
						dl->charidx= nu->charidx;
						dl->rt= nu->flag;
						
						a= dl->nr;
						bevp= (BevPoint *)(bl+1);
						data= dl->verts;
						while(a--) {
							data[0]= bevp->vec[0]+widfac*bevp->sina;
							data[1]= bevp->vec[1]+widfac*bevp->cosa;
							data[2]= bevp->vec[2];
							bevp++;
							data+=3;
						}
					}
					else {
						DispList *dlb;
						
						for (dlb=dlbev.first; dlb; dlb=dlb->next) {
	
								/* for each part of the bevel use a separate displblock */
							dl= MEM_callocN(sizeof(DispList), "makeDispListbev1");
							dl->verts= data= MEM_callocN(3*sizeof(float)*dlb->nr*bl->nr, "dlverts");
							BLI_addtail(dispbase, dl);
	
							dl->type= DL_SURF;
							
							dl->flag= dlb->flag & (DL_FRONT_CURVE|DL_BACK_CURVE);
							if(dlb->type==DL_POLY) dl->flag |= DL_CYCL_U;
							if(bl->poly>=0) dl->flag |= DL_CYCL_V;
							
							dl->parts= bl->nr;
							dl->nr= dlb->nr;
							dl->col= nu->mat_nr;
							dl->charidx= nu->charidx;
							dl->rt= nu->flag;
							dl->bevelSplitFlag= MEM_callocN(sizeof(*dl->col2)*((bl->nr+0x1F)>>5), "col2");
							bevp= (BevPoint *)(bl+1);
	
								/* for each point of poly make a bevel piece */
							bevp= (BevPoint *)(bl+1);
							for(a=0; a<bl->nr; a++,bevp++) {
								float fac=1.0;
								if (cu->taperobj==NULL) {
									if ( (cu->bevobj!=NULL) || !((cu->flag & CU_FRONT) || (cu->flag & CU_BACK)) )
										fac = bevp->radius;
								} else {
									fac = calc_taper(scene, cu->taperobj, a, bl->nr);
								}
								
								if (bevp->split_tag) {
									dl->bevelSplitFlag[a>>5] |= 1<<(a&0x1F);
								}
	
									/* rotate bevel piece and write in data */
								fp1= dlb->verts;
								for (b=0; b<dlb->nr; b++,fp1+=3,data+=3) {
									if(cu->flag & CU_3D) {
										float vec[3];
	
										vec[0]= fp1[1]+widfac;
										vec[1]= fp1[2];
										vec[2]= 0.0;
										
										mul_qt_v3(bevp->quat, vec);
										
										data[0]= bevp->vec[0] + fac*vec[0];
										data[1]= bevp->vec[1] + fac*vec[1];
										data[2]= bevp->vec[2] + fac*vec[2];
									}
									else {
										data[0]= bevp->vec[0] + fac*(widfac+fp1[1])*bevp->sina;
										data[1]= bevp->vec[1] + fac*(widfac+fp1[1])*bevp->cosa;
										data[2]= bevp->vec[2] + fac*fp1[2];
									}
								}
							}
							
							/* gl array drawing: using indices */
							displist_surf_indices(dl);
						}
					}
				}

			}
			freedisplist(&dlbev);
		}

		curve_to_filledpoly(cu, nubase, dispbase);

		if(cu->flag & CU_PATH) calc_curvepath(ob);

		if(!forOrco) curve_calc_modifiers_post(scene, ob, &cu->disp, 0, originalVerts, deformedVerts);
		tex_space_curve(cu);
	}

	if (ob->derivedFinal) {
		DM_set_object_boundbox (ob, ob->derivedFinal);
	} else {
		boundbox_displist (ob);
	}
}

/* add Orco layer to the displist object which has got derived mesh and return orco */
/* XXX: is it good place to keep this function here? */
float *makeOrcoDispList(Scene *scene, Object *ob, int forRender) {
	float *orco;
	DerivedMesh *dm= ob->derivedFinal;

	if (!dm->getVertDataArray(dm, CD_ORCO)) {
		curve_calc_orcodm(scene, ob, forRender);
	}

	orco= dm->getVertDataArray(dm, CD_ORCO);

	if(orco) {
		orco= MEM_dupallocN(orco);
	}

	return orco;
}

void imagestodisplist(void)
{
	/* removed */
}

/* this is confusing, there's also min_max_object, appplying the obmat... */
static void boundbox_displist(Object *ob)
{
	BoundBox *bb=0;
	float min[3], max[3];
	DispList *dl;
	float *vert;
	int a, tot=0;
	
	INIT_MINMAX(min, max);

	if(ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		Curve *cu= ob->data;
		int doit= 0;

		if(cu->bb==0) cu->bb= MEM_callocN(sizeof(BoundBox), "boundbox");
		bb= cu->bb;
		
		dl= cu->disp.first;

		while (dl) {
			if(dl->type==DL_INDEX3) tot= dl->nr;
			else tot= dl->nr*dl->parts;
			
			vert= dl->verts;
			for(a=0; a<tot; a++, vert+=3) {
				doit= 1;
				DO_MINMAX(vert, min, max);
			}

			dl= dl->next;
		}
		
		if(!doit) {
			min[0] = min[1] = min[2] = -1.0f;
			max[0] = max[1] = max[2] = 1.0f;
		}
		
	}
	
	if(bb) {
		boundbox_set_from_min_max(bb, min, max);
	}
}

