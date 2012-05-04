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

/** \file blender/blenkernel/intern/displist.c
 *  \ingroup bke
 */


#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_scanfill.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_displist.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_object.h"
#include "BKE_mball.h"
#include "BKE_material.h"
#include "BKE_curve.h"
#include "BKE_key.h"
#include "BKE_anim.h"
#include "BKE_font.h"
#include "BKE_lattice.h"
#include "BKE_modifier.h"

#include "BLO_sys_types.h" // for intptr_t support

static void boundbox_displist(Object *ob);

void free_disp_elem(DispList *dl)
{
	if (dl) {
		if (dl->verts) MEM_freeN(dl->verts);
		if (dl->nors) MEM_freeN(dl->nors);
		if (dl->index) MEM_freeN(dl->index);
		if (dl->col1) MEM_freeN(dl->col1);
		if (dl->col2) MEM_freeN(dl->col2);
		if (dl->bevelSplitFlag) MEM_freeN(dl->bevelSplitFlag);
		MEM_freeN(dl);
	}
}

void freedisplist(ListBase *lb)
{
	DispList *dl;

	dl= lb->first;
	while (dl) {
		BLI_remlink(lb, dl);
		free_disp_elem(dl);
		dl= lb->first;
	}
}

DispList *find_displist_create(ListBase *lb, int type)
{
	DispList *dl;
	
	dl= lb->first;
	while (dl) {
		if (dl->type==type) return dl;
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
	while (dl) {
		if (dl->type==type) return dl;
		dl= dl->next;
	}

	return NULL;
}

int displist_has_faces(ListBase *lb)
{
	DispList *dl;
	for (dl= lb->first; dl; dl= dl->next) {
		if (ELEM3(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
			return 1;
		}
	}
	return 0;
}

void copy_displist(ListBase *lbn, ListBase *lb)
{
	DispList *dln, *dl;
	
	freedisplist(lbn);
	
	dl= lb->first;
	while (dl) {
		
		dln= MEM_dupallocN(dl);
		BLI_addtail(lbn, dln);
		dln->verts= MEM_dupallocN(dl->verts);
		dln->nors= MEM_dupallocN(dl->nors);
		dln->index= MEM_dupallocN(dl->index);
		dln->col1= MEM_dupallocN(dl->col1);
		dln->col2= MEM_dupallocN(dl->col2);

		if (dl->bevelSplitFlag)
			dln->bevelSplitFlag= MEM_dupallocN(dl->bevelSplitFlag);

		dl= dl->next;
	}
}

void addnormalsDispList(ListBase *lb)
{
	DispList *dl = NULL;
	float *vdata, *ndata, nor[3];
	float *v1, *v2, *v3, *v4;
	float *n1, *n2, *n3, *n4;
	int a, b, p1, p2, p3, p4;


	dl= lb->first;
	
	while (dl) {
		if (dl->type==DL_INDEX3) {
			if (dl->nors==NULL) {
				dl->nors= MEM_callocN(sizeof(float)*3, "dlnors");
				if (dl->verts[2] < 0.0f) dl->nors[2]= -1.0f;
				else dl->nors[2]= 1.0f;
			}
		}
		else if (dl->type==DL_SURF) {
			if (dl->nors==NULL) {
				dl->nors= MEM_callocN(sizeof(float)*3*dl->nr*dl->parts, "dlnors");
				
				vdata= dl->verts;
				ndata= dl->nors;
				
				for (a=0; a<dl->parts; a++) {
					
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
					
					for (; b<dl->nr; b++) {
	
						normal_quad_v3(nor, v1, v3, v4, v2);
	
						add_v3_v3(n1, nor);
						add_v3_v3(n2, nor);
						add_v3_v3(n3, nor);
						add_v3_v3(n4, nor);
	
						v2= v1; v1+= 3;
						v4= v3; v3+= 3;
						n2= n1; n1+= 3;
						n4= n3; n3+= 3;
					}
				}
				a= dl->parts*dl->nr;
				v1= ndata;
				while (a--) {
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
	while (dl) {
		
		switch (dl->type) {
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
	if ((dl->flag & DL_CYCL_V)==0 && a==(dl->parts)-1) {
		return 0;
	}
	
	if (dl->flag & DL_CYCL_U) {
		(*p1)= dl->nr*a;
		(*p2)= (*p1)+ dl->nr-1;
		(*p3)= (*p1)+ dl->nr;
		(*p4)= (*p2)+ dl->nr;
		(*b)= 0;
	}
	else {
		(*p2)= dl->nr*a;
		(*p1)= (*p2)+1;
		(*p4)= (*p2)+ dl->nr;
		(*p3)= (*p1)+ dl->nr;
		(*b)= 1;
	}
	
	if ( (dl->flag & DL_CYCL_V) && a==dl->parts-1) {
		(*p3)-= dl->nr*dl->parts;
		(*p4)-= dl->nr*dl->parts;
	}
	
	return 1;
}

/* ****************** make displists ********************* */

static void curve_to_displist(Curve *cu, ListBase *nubase, ListBase *dispbase, int forRender)
{
	Nurb *nu;
	DispList *dl;
	BezTriple *bezt, *prevbezt;
	BPoint *bp;
	float *data;
	int a, len, resolu;
	
	nu= nubase->first;
	while (nu) {
		if (nu->hide==0) {
			
			if (forRender && cu->resolu_ren!=0)
				resolu= cu->resolu_ren;
			else
				resolu= nu->resolu;
			
			if (!BKE_nurb_check_valid_u(nu));
			else if (nu->type == CU_BEZIER) {
				
				/* count */
				len= 0;
				a= nu->pntsu-1;
				if (nu->flagu & CU_NURB_CYCLIC) a++;

				prevbezt= nu->bezt;
				bezt= prevbezt+1;
				while (a--) {
					if (a==0 && (nu->flagu & CU_NURB_CYCLIC)) bezt= nu->bezt;
					
					if (prevbezt->h2==HD_VECT && bezt->h1==HD_VECT) len++;
					else len+= resolu;
					
					if (a==0 && (nu->flagu & CU_NURB_CYCLIC)==0) len++;
					
					prevbezt= bezt;
					bezt++;
				}
				
				dl= MEM_callocN(sizeof(DispList), "makeDispListbez");
				/* len+1 because of 'forward_diff_bezier' function */
				dl->verts= MEM_callocN((len+1)*3*sizeof(float), "dlverts");
				BLI_addtail(dispbase, dl);
				dl->parts= 1;
				dl->nr= len;
				dl->col= nu->mat_nr;
				dl->charidx= nu->charidx;

				data= dl->verts;

				if (nu->flagu & CU_NURB_CYCLIC) {
					dl->type= DL_POLY;
					a= nu->pntsu;
				}
				else {
					dl->type= DL_SEGM;
					a= nu->pntsu-1;
				}
				
				prevbezt= nu->bezt;
				bezt= prevbezt+1;
				
				while (a--) {
					if (a==0 && dl->type== DL_POLY) bezt= nu->bezt;
					
					if (prevbezt->h2==HD_VECT && bezt->h1==HD_VECT) {
						copy_v3_v3(data, prevbezt->vec[1]);
						data+= 3;
					}
					else {
						int j;
						for (j=0; j<3; j++) {
							BKE_curve_forward_diff_bezier(prevbezt->vec[1][j],
							                              prevbezt->vec[2][j],
							                              bezt->vec[0][j],
							                              bezt->vec[1][j],
							                              data + j, resolu, 3 * sizeof(float));
						}
						
						data+= 3*resolu;
					}
					
					if (a==0 && dl->type==DL_SEGM) {
						copy_v3_v3(data, bezt->vec[1]);
					}
					
					prevbezt= bezt;
					bezt++;
				}
			}
			else if (nu->type == CU_NURBS) {
				len= (resolu*SEGMENTSU(nu));
				
				dl= MEM_callocN(sizeof(DispList), "makeDispListsurf");
				dl->verts= MEM_callocN(len*3*sizeof(float), "dlverts");
				BLI_addtail(dispbase, dl);
				dl->parts= 1;
				
				dl->nr= len;
				dl->col= nu->mat_nr;
				dl->charidx = nu->charidx;

				data= dl->verts;
				if (nu->flagu & CU_NURB_CYCLIC) dl->type= DL_POLY;
				else dl->type= DL_SEGM;
				BKE_nurb_makeCurve(nu, data, NULL, NULL, NULL, resolu, 3*sizeof(float));
			}
			else if (nu->type == CU_POLY) {
				len= nu->pntsu;
				dl= MEM_callocN(sizeof(DispList), "makeDispListpoly");
				dl->verts= MEM_callocN(len*3*sizeof(float), "dlverts");
				BLI_addtail(dispbase, dl);
				dl->parts= 1;
				dl->nr= len;
				dl->col= nu->mat_nr;
				dl->charidx = nu->charidx;

				data= dl->verts;
				if (nu->flagu & CU_NURB_CYCLIC) dl->type= DL_POLY;
				else dl->type= DL_SEGM;
				
				a= len;
				bp= nu->bp;
				while (a--) {
					copy_v3_v3(data, bp->vec);
					bp++;
					data+= 3;
				}
			}
		}
		nu= nu->next;
	}
}


void filldisplist(ListBase *dispbase, ListBase *to, int flipnormal)
{
	ScanFillContext sf_ctx;
	ScanFillVert *eve, *v1, *vlast;
	ScanFillFace *efa;
	DispList *dlnew=NULL, *dl;
	float *f1;
	int colnr=0, charidx=0, cont=1, tot, a, *index, nextcol= 0;
	intptr_t totvert;
	
	if (dispbase==NULL) return;
	if (dispbase->first==NULL) return;

	while (cont) {
		cont= 0;
		totvert= 0;
		nextcol= 0;
		
		BLI_begin_edgefill(&sf_ctx);
		
		dl= dispbase->first;
		while (dl) {
	
			if (dl->type==DL_POLY) {
				if (charidx<dl->charidx) cont= 1;
				else if (charidx==dl->charidx) { /* character with needed index */
					if (colnr==dl->col) {
						/* make editverts and edges */
						f1= dl->verts;
						a= dl->nr;
						eve= v1= NULL;
						
						while (a--) {
							vlast= eve;

							eve = BLI_addfillvert(&sf_ctx, f1);
							totvert++;

							if (vlast==NULL) v1= eve;
							else {
								BLI_addfilledge(&sf_ctx, vlast, eve);
							}
							f1+=3;
						}

						if (eve!=NULL && v1!=NULL) {
							BLI_addfilledge(&sf_ctx, eve, v1);
						}
					}
					else if (colnr<dl->col) {
						/* got poly with next material at current char */
						cont= 1;
						nextcol= 1;
					}
				}
			}
			dl= dl->next;
		}
		
		if (totvert && (tot= BLI_edgefill(&sf_ctx, FALSE))) { // XXX (obedit && obedit->actcol)?(obedit->actcol-1):0)) {
			if (tot) {
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
				eve= sf_ctx.fillvertbase.first;
				while (eve) {
					copy_v3_v3(f1, eve->co);
					f1+= 3;
	
					/* index number */
					eve->tmp.l = totvert;
					totvert++;
					
					eve= eve->next;
				}
				
				/* index data */
				efa = sf_ctx.fillfacebase.first;
				index= dlnew->index;
				while (efa) {
					index[0]= (intptr_t)efa->v1->tmp.l;
					index[1]= (intptr_t)efa->v2->tmp.l;
					index[2]= (intptr_t)efa->v3->tmp.l;

					if (flipnormal)
						SWAP(int, index[0], index[2]);
					
					index+= 3;
					efa= efa->next;
				}
			}

			BLI_addhead(to, dlnew);
			
		}
		BLI_end_edgefill(&sf_ctx);

		if (nextcol) {
			/* stay at current char but fill polys with next material */
			colnr++;
		}
		else {
			/* switch to next char and start filling from first material */
			charidx++;
			colnr= 0;
		}
	}
	
	/* do not free polys, needed for wireframe display */
	
}

static void bevels_to_filledpoly(Curve *cu, ListBase *dispbase)
{
	ListBase front, back;
	DispList *dl, *dlnew;
	float *fp, *fp1;
	int a, dpoly;
	
	front.first= front.last= back.first= back.last= NULL;
	
	dl= dispbase->first;
	while (dl) {
		if (dl->type==DL_SURF) {
			if ( (dl->flag & DL_CYCL_V) && (dl->flag & DL_CYCL_U)==0 ) {
				if ( (cu->flag & CU_BACK) && (dl->flag & DL_BACK_CURVE) ) {
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
					while (a--) {
						copy_v3_v3(fp1, fp);
						fp1+= 3;
						fp+= dpoly;
					}
				}
				if ( (cu->flag & CU_FRONT) && (dl->flag & DL_FRONT_CURVE) ) {
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
					while (a--) {
						copy_v3_v3(fp1, fp);
						fp1+= 3;
						fp+= dpoly;
					}
				}
			}
		}
		dl= dl->next;
	}

	filldisplist(&front, dispbase, 1);
	filldisplist(&back, dispbase, 0);
	
	freedisplist(&front);
	freedisplist(&back);

	filldisplist(dispbase, dispbase, 0);
	
}

static void curve_to_filledpoly(Curve *cu, ListBase *UNUSED(nurb), ListBase *dispbase)
{
	if (cu->flag & CU_3D) return;

	if (dispbase->first && ((DispList*) dispbase->first)->type==DL_SURF) {
		bevels_to_filledpoly(cu, dispbase);
	}
	else {
		filldisplist(dispbase, dispbase, 0);
	}
}

/* taper rules:
 * - only 1 curve
 * - first point left, last point right
 * - based on subdivided points in original curve, not on points in taper curve (still)
 */
float calc_taper(Scene *scene, Object *taperobj, int cur, int tot)
{
	DispList *dl;
	
	if (taperobj==NULL || taperobj->type!=OB_CURVE) return 1.0;
	
	dl= taperobj->disp.first;
	if (dl==NULL) {
		makeDispListCurveTypes(scene, taperobj, 0);
		dl= taperobj->disp.first;
	}
	if (dl) {
		float fac= ((float)cur)/(float)(tot-1);
		float minx, dx, *fp;
		int a;
		
		/* horizontal size */
		minx= dl->verts[0];
		dx= dl->verts[3*(dl->nr-1)] - minx;
		if (dx > 0.0f) {
		
			fp= dl->verts;
			for (a=0; a<dl->nr; a++, fp+=3) {
				if ( (fp[0]-minx)/dx >= fac) {
					/* interpolate with prev */
					if (a>0) {
						float fac1= (fp[-3]-minx)/dx;
						float fac2= (fp[0]-minx)/dx;
						if (fac1!=fac2)
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
	if (!ob || ob->type!=OB_MBALL) return;

	// XXX: mball stuff uses plenty of global variables
	//      while this is unchanged updating during render is unsafe
	if (G.rendering) return;

	freedisplist(&(ob->disp));

	if (ob->type==OB_MBALL) {
		if (ob==BKE_metaball_basis_find(scene, ob)) {
			BKE_metaball_polygonize(scene, ob, &ob->disp);
			BKE_metaball_tex_space_calc(ob);

			object_deform_mball(ob, &ob->disp);
		}
	}
	
	boundbox_displist(ob);
}

void makeDispListMBall_forRender(Scene *scene, Object *ob, ListBase *dispbase)
{
	BKE_metaball_polygonize(scene, ob, dispbase);
	BKE_metaball_tex_space_calc(ob);
	
	object_deform_mball(ob, dispbase);
}

static ModifierData *curve_get_tessellate_point(Scene *scene, Object *ob, int forRender, int editmode)
{
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	ModifierData *pretessellatePoint;
	int required_mode;

	if (forRender) required_mode = eModifierMode_Render;
	else required_mode = eModifierMode_Realtime;

	if (editmode) required_mode |= eModifierMode_Editmode;

	pretessellatePoint = NULL;
	for (; md; md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!modifier_isEnabled(scene, md, required_mode)) continue;
		if (mti->type == eModifierTypeType_Constructive) return pretessellatePoint;

		if (ELEM3(md->type, eModifierType_Hook, eModifierType_Softbody, eModifierType_MeshDeform)) {
			pretessellatePoint = md;

			/* this modifiers are moving point of tessellation automatically
			 * (some of them even can't be applied on tessellated curve), set flag
			 * for incformation button in modifier's header */
			md->mode |= eModifierMode_ApplyOnSpline;
		}
		else if (md->mode&eModifierMode_ApplyOnSpline) {
			pretessellatePoint = md;
		}
	}

	return pretessellatePoint;
}

static void curve_calc_modifiers_pre(Scene *scene, Object *ob, int forRender, float (**originalVerts_r)[3], float (**deformedVerts_r)[3], int *numVerts_r)
{
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	ModifierData *pretessellatePoint;
	Curve *cu= ob->data;
	ListBase *nurb= BKE_curve_nurbs_get(cu);
	int numVerts = 0;
	int editmode = (!forRender && cu->editnurb);
	float (*originalVerts)[3] = NULL;
	float (*deformedVerts)[3] = NULL;
	float *keyVerts= NULL;
	int required_mode;

	if (forRender) required_mode = eModifierMode_Render;
	else required_mode = eModifierMode_Realtime;

	pretessellatePoint = curve_get_tessellate_point(scene, ob, forRender, editmode);
	
	if (editmode) required_mode |= eModifierMode_Editmode;

	if (cu->editnurb==NULL) {
		keyVerts= do_ob_key(scene, ob);

		if (keyVerts) {
			/* split coords from key data, the latter also includes
			 * tilts, which is passed through in the modifier stack.
			 * this is also the reason curves do not use a virtual
			 * shape key modifier yet. */
			deformedVerts= BKE_curve_keyVertexCos_get(cu, nurb, keyVerts);
			originalVerts= MEM_dupallocN(deformedVerts);
			numVerts = BKE_nurbList_verts_count_without_handles(nurb);
		}
	}
	
	if (pretessellatePoint) {
		for (; md; md=md->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);

			md->scene= scene;
			
			if ((md->mode & required_mode) != required_mode) continue;
			if (mti->isDisabled && mti->isDisabled(md, forRender)) continue;
			if (mti->type!=eModifierTypeType_OnlyDeform) continue;

			if (!deformedVerts) {
				deformedVerts = BKE_curve_vertexCos_get(cu, nurb, &numVerts);
				originalVerts = MEM_dupallocN(deformedVerts);
			}

			mti->deformVerts(md, ob, NULL, deformedVerts, numVerts, forRender, editmode);

			if (md==pretessellatePoint)
				break;
		}
	}

	if (deformedVerts)
		BK_curve_vertexCos_apply(cu, nurb, deformedVerts);
	if (keyVerts) /* these are not passed through modifier stack */
		BKE_curve_keyVertexTilts_apply(cu, nurb, keyVerts);

	if (keyVerts)
		MEM_freeN(keyVerts);

	*originalVerts_r = originalVerts;
	*deformedVerts_r = deformedVerts;
	*numVerts_r = numVerts;
}

static float (*displist_get_allverts (ListBase *dispbase, int *totvert))[3]
{
	DispList *dl;
	float (*allverts)[3], *fp;

	*totvert= 0;

	for (dl=dispbase->first; dl; dl=dl->next)
		*totvert+= (dl->type==DL_INDEX3)?dl->nr:dl->parts*dl->nr;

	allverts= MEM_mallocN((*totvert)*sizeof(float)*3, "displist_get_allverts allverts");
	fp= (float*)allverts;
	for (dl=dispbase->first; dl; dl=dl->next) {
		int offs= 3 * ((dl->type==DL_INDEX3)?dl->nr:dl->parts*dl->nr);
		memcpy(fp, dl->verts, sizeof(float) * offs);
		fp+= offs;
	}

	return allverts;
}

static void displist_apply_allverts(ListBase *dispbase, float (*allverts)[3])
{
	DispList *dl;
	float *fp;

	fp= (float*)allverts;
	for (dl=dispbase->first; dl; dl=dl->next) {
		int offs= 3 * ((dl->type==DL_INDEX3)?dl->nr:dl->parts*dl->nr);
		memcpy(dl->verts, fp, sizeof(float) * offs);
		fp+= offs;
	}
}

static void curve_calc_modifiers_post(Scene *scene, Object *ob, ListBase *dispbase,
	DerivedMesh **derivedFinal, int forRender, float (*originalVerts)[3], float (*deformedVerts)[3])
{
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	ModifierData *pretessellatePoint;
	Curve *cu= ob->data;
	ListBase *nurb= BKE_curve_nurbs_get(cu);
	int required_mode = 0, totvert = 0;
	int editmode = (!forRender && cu->editnurb);
	DerivedMesh *dm= NULL, *ndm;
	float (*vertCos)[3] = NULL;
	int useCache = !forRender;

	if (forRender) required_mode = eModifierMode_Render;
	else required_mode = eModifierMode_Realtime;

	pretessellatePoint = curve_get_tessellate_point(scene, ob, forRender, editmode);
	
	if (editmode) required_mode |= eModifierMode_Editmode;

	if (pretessellatePoint) {
		md = pretessellatePoint->next;
	}

	if (derivedFinal && *derivedFinal) {
		(*derivedFinal)->release (*derivedFinal);
	}

	for (; md; md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		md->scene= scene;

		if ((md->mode & required_mode) != required_mode) continue;
		if (mti->isDisabled && mti->isDisabled(md, forRender)) continue;

		if (mti->type == eModifierTypeType_OnlyDeform ||
				(mti->type == eModifierTypeType_DeformOrConstruct && !dm)) {
			if (dm) {
				if (!vertCos) {
					totvert = dm->getNumVerts(dm);
					vertCos = MEM_mallocN(sizeof(*vertCos) * totvert, "dfmv");
					dm->getVertCos(dm, vertCos);
				}

				mti->deformVerts(md, ob, dm, vertCos, totvert, forRender, editmode);
			}
			else {
				if (!vertCos) {
					vertCos= displist_get_allverts(dispbase, &totvert);
				}

				mti->deformVerts(md, ob, NULL, vertCos, totvert, forRender, editmode);
			}
		}
		else {
			if (!derivedFinal) {
				/* makeDisplistCurveTypes could be used for beveling, where derived mesh */
				/* is totally unnecessary, so we could stop modifiers applying */
				/* when we found constructive modifier but derived mesh is unwanted result */
				break;
			}

			if (dm) {
				if (vertCos) {
					DerivedMesh *tdm = CDDM_copy(dm);
					dm->release(dm);
					dm = tdm;

					CDDM_apply_vert_coords(dm, vertCos);
					CDDM_calc_normals_mapping(dm);
				}
			}
			else {
				if (vertCos) {
					displist_apply_allverts(dispbase, vertCos);
				}

				if (ELEM(ob->type, OB_CURVE, OB_FONT) && (cu->flag & CU_DEFORM_FILL)) {
					curve_to_filledpoly(cu, nurb, dispbase);
				}

				dm= CDDM_from_curve_customDB(ob, dispbase);

				CDDM_calc_normals_mapping(dm);
			}

			if (vertCos) {
				/* Vertex coordinates were applied to necessary data, could free it */
				MEM_freeN(vertCos);
				vertCos= NULL;
			}

			ndm = mti->applyModifier(md, ob, dm, forRender, useCache);

			if (ndm) {
				/* Modifier returned a new derived mesh */

				if (dm && dm != ndm) /* Modifier  */
					dm->release (dm);
				dm = ndm;
			}
		}
	}

	if (vertCos) {
		if (dm) {
			DerivedMesh *tdm = CDDM_copy(dm);
			dm->release(dm);
			dm = tdm;

			CDDM_apply_vert_coords(dm, vertCos);
			CDDM_calc_normals_mapping(dm);
			MEM_freeN(vertCos);
		}
		else {
			displist_apply_allverts(dispbase, vertCos);
			MEM_freeN(vertCos);
			vertCos= NULL;
		}
	}

	if (derivedFinal) {
		if (dm) DM_ensure_tessface(dm); /* needed for drawing */
		(*derivedFinal) = dm;
	}

	if (deformedVerts) {
		BK_curve_vertexCos_apply(ob->data, nurb, originalVerts);
		MEM_freeN(originalVerts);
		MEM_freeN(deformedVerts);
	}
}

static void displist_surf_indices(DispList *dl)
{
	int a, b, p1, p2, p3, p4;
	int *index;
	
	dl->totindex= 0;
	
	index=dl->index= MEM_mallocN(4*sizeof(int)*(dl->parts+1)*(dl->nr+1), "index array nurbs");
	
	for (a=0; a<dl->parts; a++) {
		
		if (surfindex_displist(dl, a, &b, &p1, &p2, &p3, &p4)==0)
			break;
		
		for (; b<dl->nr; b++, index+=4) {	
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
	ListBase disp= {NULL, NULL};

	/* OrcoDM should be created from underformed disp lists */
	makeDispListCurveTypes_forOrco(scene, ob, &disp);
	dm= CDDM_from_curve_customDB(ob, &disp);

	freedisplist(&disp);

	return dm;
}

static void add_orco_dm(Scene *scene, Object *ob, DerivedMesh *dm, DerivedMesh *orcodm)
{
	float (*orco)[3], (*layerorco)[3];
	int totvert, a;
	Curve *cu= ob->data;

	totvert= dm->getNumVerts(dm);

	if (orcodm) {
		orco= MEM_callocN(sizeof(float)*3*totvert, "dm orco");

		if (orcodm->getNumVerts(orcodm) == totvert)
			orcodm->getVertCos(orcodm, orco);
		else
			dm->getVertCos(dm, orco);
	}
	else {
		orco= (float(*)[3])BKE_curve_make_orco(scene, ob);
	}

	for (a=0; a<totvert; a++) {
		float *co = orco[a];
		co[0] = (co[0]-cu->loc[0])/cu->size[0];
		co[1] = (co[1]-cu->loc[1])/cu->size[1];
		co[2] = (co[2]-cu->loc[2])/cu->size[2];
	}

	if ((layerorco = DM_get_vert_data_layer(dm, CD_ORCO))) {
		memcpy(layerorco, orco, sizeof(float)*totvert);
		MEM_freeN(orco);
	}
	else
		DM_add_vert_layer(dm, CD_ORCO, CD_ASSIGN, orco);
}

static void curve_calc_orcodm(Scene *scene, Object *ob, DerivedMesh *derivedFinal, int forRender)
{
	/* this function represents logic of mesh's orcodm calculation */
	/* for displist-based objects */

	ModifierData *md = modifiers_getVirtualModifierList(ob);
	ModifierData *pretessellatePoint;
	Curve *cu= ob->data;
	int required_mode;
	int editmode = (!forRender && cu->editnurb);
	DerivedMesh *ndm, *orcodm= NULL;

	if (forRender) required_mode = eModifierMode_Render;
	else required_mode = eModifierMode_Realtime;

	pretessellatePoint = curve_get_tessellate_point(scene, ob, forRender, editmode);

	if (editmode) required_mode |= eModifierMode_Editmode;

	if (pretessellatePoint) {
		md = pretessellatePoint->next;
	}

	for (; md; md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		md->scene= scene;

		if ((md->mode & required_mode) != required_mode) continue;
		if (mti->isDisabled && mti->isDisabled(md, forRender)) continue;
		if (mti->type!=eModifierTypeType_Constructive) continue;

		if (!orcodm)
			orcodm= create_orco_dm(scene, ob);

		ndm = mti->applyModifier(md, ob, orcodm, forRender, 0);

		if (ndm) {
			/* if the modifier returned a new dm, release the old one */
			if (orcodm && orcodm != ndm) {
				orcodm->release(orcodm);
			}
			orcodm = ndm;
		}
	}

	/* add an orco layer if needed */
	add_orco_dm(scene, ob, derivedFinal, orcodm);

	if (orcodm)
		orcodm->release(orcodm);
}

void makeDispListSurf(Scene *scene, Object *ob, ListBase *dispbase,
	DerivedMesh **derivedFinal, int forRender, int forOrco)
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

	if (!forRender && cu->editnurb)
		nubase= BKE_curve_editNurbs_get(cu);
	else
		nubase= &cu->nurb;

	if (!forOrco)
		curve_calc_modifiers_pre(scene, ob, forRender, &originalVerts, &deformedVerts, &numVerts);

	for (nu=nubase->first; nu; nu=nu->next) {
		if (forRender || nu->hide==0) {
			int resolu= nu->resolu, resolv= nu->resolv;

			if (forRender) {
				if (cu->resolu_ren) resolu= cu->resolu_ren;
				if (cu->resolv_ren) resolv= cu->resolv_ren;
			}

			if (nu->pntsv==1) {
				len= SEGMENTSU(nu)*resolu;

				dl= MEM_callocN(sizeof(DispList), "makeDispListsurf");
				dl->verts= MEM_callocN(len*3*sizeof(float), "dlverts");

				BLI_addtail(dispbase, dl);
				dl->parts= 1;
				dl->nr= len;
				dl->col= nu->mat_nr;
				dl->charidx= nu->charidx;

				/* dl->rt will be used as flag for render face and */
				/* CU_2D conflicts with R_NOPUNOFLIP */
				dl->rt= nu->flag & ~CU_2D;

				data= dl->verts;
				if (nu->flagu & CU_NURB_CYCLIC) dl->type= DL_POLY;
				else dl->type= DL_SEGM;

				BKE_nurb_makeCurve(nu, data, NULL, NULL, NULL, resolu, 3*sizeof(float));
			}
			else {
				len= (nu->pntsu*resolu) * (nu->pntsv*resolv);
				
				dl= MEM_callocN(sizeof(DispList), "makeDispListsurf");
				dl->verts= MEM_callocN(len*3*sizeof(float), "dlverts");
				BLI_addtail(dispbase, dl);

				dl->col= nu->mat_nr;
				dl->charidx= nu->charidx;

				/* dl->rt will be used as flag for render face and */
				/* CU_2D conflicts with R_NOPUNOFLIP */
				dl->rt= nu->flag & ~CU_2D;

				data= dl->verts;
				dl->type= DL_SURF;

				dl->parts= (nu->pntsu*resolu);	/* in reverse, because makeNurbfaces works that way */
				dl->nr= (nu->pntsv*resolv);
				if (nu->flagv & CU_NURB_CYCLIC) dl->flag|= DL_CYCL_U;	/* reverse too! */
				if (nu->flagu & CU_NURB_CYCLIC) dl->flag|= DL_CYCL_V;

				BKE_nurb_makeFaces(nu, data, 0, resolu, resolv);
				
				/* gl array drawing: using indices */
				displist_surf_indices(dl);
			}
		}
	}

	/* make copy of 'undeformed" displist for texture space calculation
	 * actually, it's not totally undeformed -- pre-tessellation modifiers are
	 * already applied, thats how it worked for years, so keep for compatibility (sergey) */
	copy_displist(&cu->disp, dispbase);

	if (!forRender) {
		BKE_curve_tex_space_calc(cu);
	}

	if (!forOrco)
		curve_calc_modifiers_post(scene, ob, dispbase, derivedFinal,
			forRender, originalVerts, deformedVerts);
}

static void rotateBevelPiece(Curve *cu, BevPoint *bevp, DispList *dlb, float widfac, float fac, float **data_r)
{
	float *fp, *data = *data_r;
	int b;

	fp = dlb->verts;
	for (b = 0; b<dlb->nr; b++, fp += 3, data += 3) {
		if (cu->flag & CU_3D) {
			float vec[3];

			vec[0] = fp[1]+widfac;
			vec[1] = fp[2];
			vec[2 ]= 0.0;

			mul_qt_v3(bevp->quat, vec);

			data[0] = bevp->vec[0] + fac*vec[0];
			data[1] = bevp->vec[1] + fac*vec[1];
			data[2] = bevp->vec[2] + fac*vec[2];
		}
		else {
			data[0] = bevp->vec[0] + fac*(widfac+fp[1])*bevp->sina;
			data[1] = bevp->vec[1] + fac*(widfac+fp[1])*bevp->cosa;
			data[2] = bevp->vec[2] + fac*fp[2];
		}
	}

	*data_r = data;
}

static void fillBevelCap(Nurb *nu, DispList *dlb, float *prev_fp, ListBase *dispbase)
{
	DispList *dl;
	float *data;
	int b;

	dl= MEM_callocN(sizeof(DispList), "makeDispListbev2");
	dl->verts= data= MEM_callocN(3*sizeof(float)*dlb->nr, "dlverts");

	dl->type= DL_POLY;

	dl->parts= 1;
	dl->nr= dlb->nr;
	dl->col= nu->mat_nr;
	dl->charidx= nu->charidx;

	/* dl->rt will be used as flag for render face and */
	/* CU_2D conflicts with R_NOPUNOFLIP */
	dl->rt= nu->flag & ~CU_2D;

	for (b = 0; b < dlb->nr; b++, prev_fp += 3, data += 3)
		copy_v3_v3(data, prev_fp);

	BLI_addtail(dispbase, dl);
}

static void do_makeDispListCurveTypes(Scene *scene, Object *ob, ListBase *dispbase,
	DerivedMesh **derivedFinal, int forRender, int forOrco)
{
	Curve *cu = ob->data;

	/* we do allow duplis... this is only displist on curve level */
	if (!ELEM3(ob->type, OB_SURF, OB_CURVE, OB_FONT)) return;

	if (ob->type==OB_SURF) {
		makeDispListSurf(scene, ob, dispbase, derivedFinal, forRender, forOrco);
	}
	else if (ELEM(ob->type, OB_CURVE, OB_FONT)) {
		ListBase dlbev;
		ListBase *nubase;
		float (*originalVerts)[3];
		float (*deformedVerts)[3];
		int numVerts;

		nubase= BKE_curve_nurbs_get(cu);

		BLI_freelistN(&(cu->bev));

		if (cu->path) free_path(cu->path);
		cu->path= NULL;

		if (ob->type==OB_FONT) BKE_text_to_curve(G.main, scene, ob, 0);

		if (!forOrco) curve_calc_modifiers_pre(scene, ob, forRender, &originalVerts, &deformedVerts, &numVerts);

		BKE_curve_bevelList_make(ob);

		/* If curve has no bevel will return nothing */
		BKE_curve_bevel_make(scene, ob, &dlbev, forRender);

		/* no bevel or extrude, and no width correction? */
		if (!dlbev.first && cu->width==1.0f) {
			curve_to_displist(cu, nubase, dispbase, forRender);
		}
		else {
			float widfac= cu->width - 1.0f;
			BevList *bl= cu->bev.first;
			Nurb *nu= nubase->first;

			for (; bl && nu; bl=bl->next, nu=nu->next) {
				DispList *dl;
				float *data;
				BevPoint *bevp;
				int a;

				if (bl->nr) { /* blank bevel lists can happen */

					/* exception handling; curve without bevel or extrude, with width correction */
					if (dlbev.first==NULL) {
						dl= MEM_callocN(sizeof(DispList), "makeDispListbev");
						dl->verts= MEM_callocN(3*sizeof(float)*bl->nr, "dlverts");
						BLI_addtail(dispbase, dl);

						if (bl->poly!= -1) dl->type= DL_POLY;
						else dl->type= DL_SEGM;

						if (dl->type==DL_SEGM) dl->flag = (DL_FRONT_CURVE|DL_BACK_CURVE);

						dl->parts= 1;
						dl->nr= bl->nr;
						dl->col= nu->mat_nr;
						dl->charidx= nu->charidx;

						/* dl->rt will be used as flag for render face and */
						/* CU_2D conflicts with R_NOPUNOFLIP */
						dl->rt= nu->flag & ~CU_2D;

						a= dl->nr;
						bevp= (BevPoint *)(bl+1);
						data= dl->verts;
						while (a--) {
							data[0]= bevp->vec[0]+widfac*bevp->sina;
							data[1]= bevp->vec[1]+widfac*bevp->cosa;
							data[2]= bevp->vec[2];
							bevp++;
							data+=3;
						}
					}
					else {
						DispList *dlb;
						ListBase bottom_capbase = {NULL, NULL};
						ListBase top_capbase = {NULL, NULL};

						for (dlb=dlbev.first; dlb; dlb=dlb->next) {
							int i, start, steps;
							float bevfac1 = MIN2(cu->bevfac1, cu->bevfac2), bevfac2 = MAX2(cu->bevfac1, cu->bevfac2);
							float firstblend = 0.0f, lastblend = 0.0f;

							if (cu->bevfac1 - cu->bevfac2 == 0.0f)
								continue;

							start = (int)(bevfac1*(bl->nr-1));
							steps = 2+(int)((bevfac2)*(bl->nr-1)) - start;
							firstblend = 1.0f - ((float)bevfac1*(bl->nr-1) - (int)((float)bevfac1*(bl->nr-1)));
							lastblend  = (float)bevfac2*(bl->nr-1) - (int)((float)bevfac2*(bl->nr-1));

							if (steps > bl->nr) {
								steps = bl->nr;
								lastblend = 1.0f;
							}

							/* for each part of the bevel use a separate displblock */
							dl= MEM_callocN(sizeof(DispList), "makeDispListbev1");
							dl->verts= data= MEM_callocN(3*sizeof(float)*dlb->nr*steps, "dlverts");
							BLI_addtail(dispbase, dl);
	
							dl->type= DL_SURF;
							
							dl->flag= dlb->flag & (DL_FRONT_CURVE|DL_BACK_CURVE);
							if (dlb->type==DL_POLY) dl->flag |= DL_CYCL_U;
							if (bl->poly>=0) dl->flag |= DL_CYCL_V;

							dl->parts= steps;
							dl->nr= dlb->nr;
							dl->col= nu->mat_nr;
							dl->charidx= nu->charidx;

							/* dl->rt will be used as flag for render face and */
							/* CU_2D conflicts with R_NOPUNOFLIP */
							dl->rt= nu->flag & ~CU_2D;

							dl->bevelSplitFlag= MEM_callocN(sizeof(*dl->col2)*((steps+0x1F)>>5), "bevelSplitFlag");
	
							/* for each point of poly make a bevel piece */
							bevp= (BevPoint *)(bl+1) + start;
							for (i=start, a=0; a<steps; i++,bevp++,a++) {
								float fac=1.0;
								float *cur_data = data;

								if (cu->taperobj==NULL) {
									if ( (cu->bevobj!=NULL) || !((cu->flag & CU_FRONT) || (cu->flag & CU_BACK)) )
										fac = bevp->radius;
								}
								else {
									fac = calc_taper(scene, cu->taperobj, i, bl->nr);
								}

								if (bevp->split_tag) {
									dl->bevelSplitFlag[a>>5] |= 1<<(a&0x1F);
								}
	
								/* rotate bevel piece and write in data */
								rotateBevelPiece(cu, bevp, dlb, widfac, fac, &data);

								if (a == 1 || a == steps - 1) {
									float *cur_fp = cur_data, *prev_fp = cur_data - 3*dlb->nr;
									int b;

									for (b = 0; b < dlb->nr; b++, prev_fp += 3, cur_fp += 3) {
										float cur[3], prev[3];

										copy_v3_v3(cur, cur_fp);
										copy_v3_v3(prev, prev_fp);

										if (a == 1)
											interp_v3_v3v3(prev, cur_fp, prev_fp, firstblend);
										if (a == steps - 1)
											interp_v3_v3v3(cur, prev_fp, cur_fp, lastblend);

										copy_v3_v3(cur_fp, cur);
										copy_v3_v3(prev_fp, prev);
									}
								}

								if (cu->bevobj && (cu->flag & CU_FILL_CAPS)) {
									if (a == 1) 
										fillBevelCap(nu, dlb, cur_data - 3*dlb->nr, &bottom_capbase);
									if (a == steps - 1)
										fillBevelCap(nu, dlb, cur_data, &top_capbase);
								}
							}

							/* gl array drawing: using indices */
							displist_surf_indices(dl);
						}

						if (bottom_capbase.first) {
							filldisplist(&bottom_capbase, dispbase, 1);
							filldisplist(&top_capbase, dispbase, 0);
							freedisplist(&bottom_capbase);
							freedisplist(&top_capbase);
						}
					}
				}

			}
			freedisplist(&dlbev);
		}

		if (!(cu->flag & CU_DEFORM_FILL)) {
			curve_to_filledpoly(cu, nubase, dispbase);
		}

		if ((cu->flag & CU_PATH) && !forOrco) calc_curvepath(ob);

		/* make copy of 'undeformed" displist for texture space calculation
		 * actually, it's not totally undeformed -- pre-tessellation modifiers are
		 * already applied, thats how it worked for years, so keep for compatibility (sergey) */
		copy_displist(&cu->disp, dispbase);

		if (!forRender) {
			BKE_curve_tex_space_calc(cu);
		}

		if (!forOrco) curve_calc_modifiers_post(scene, ob, dispbase, derivedFinal, forRender, originalVerts, deformedVerts);

		if (cu->flag & CU_DEFORM_FILL && !ob->derivedFinal) {
			curve_to_filledpoly(cu, nubase, dispbase);
		}
	}
}

void makeDispListCurveTypes(Scene *scene, Object *ob, int forOrco)
{
	Curve *cu= ob->data;
	ListBase *dispbase;

	/* The same check for duplis as in do_makeDispListCurveTypes.
	 * Happens when curve used for constraint/bevel was converted to mesh.
	 * check there is still needed for render displist and orco displists. */
	if (!ELEM3(ob->type, OB_SURF, OB_CURVE, OB_FONT)) return;

	freedisplist(&(ob->disp));
	dispbase= &(ob->disp);
	freedisplist(dispbase);

	/* free displist used for textspace */
	freedisplist(&cu->disp);

	do_makeDispListCurveTypes(scene, ob, dispbase, &ob->derivedFinal, 0, forOrco);

	if (ob->derivedFinal) {
		DM_set_object_boundbox (ob, ob->derivedFinal);
	}
	else {
		boundbox_displist (ob);

		/* if there is no derivedMesh, object's boundbox is unneeded */
		if (ob->bb) {
			MEM_freeN(ob->bb);
			ob->bb= NULL;
		}
	}
}

void makeDispListCurveTypes_forRender(Scene *scene, Object *ob, ListBase *dispbase,
	DerivedMesh **derivedFinal, int forOrco)
{
	do_makeDispListCurveTypes(scene, ob, dispbase, derivedFinal, 1, forOrco);
}

void makeDispListCurveTypes_forOrco(struct Scene *scene, struct Object *ob, struct ListBase *dispbase)
{
	do_makeDispListCurveTypes(scene, ob, dispbase, NULL, 1, 1);
}

/* add Orco layer to the displist object which has got derived mesh and return orco */
float *makeOrcoDispList(Scene *scene, Object *ob, DerivedMesh *derivedFinal, int forRender)
{
	float *orco;

	if (derivedFinal == NULL)
		derivedFinal= ob->derivedFinal;

	if (!derivedFinal->getVertDataArray(derivedFinal, CD_ORCO)) {
		curve_calc_orcodm(scene, ob, derivedFinal, forRender);
	}

	orco= derivedFinal->getVertDataArray(derivedFinal, CD_ORCO);

	if (orco) {
		orco= MEM_dupallocN(orco);
	}

	return orco;
}

/* this is confusing, there's also min_max_object, appplying the obmat... */
static void boundbox_displist(Object *ob)
{
	BoundBox *bb=NULL;
	float min[3], max[3];
	DispList *dl;
	float *vert;
	int a, tot=0;
	
	INIT_MINMAX(min, max);

	if (ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		Curve *cu= ob->data;
		int doit= 0;

		if (cu->bb==NULL) cu->bb= MEM_callocN(sizeof(BoundBox), "boundbox");
		bb= cu->bb;
		
		dl= ob->disp.first;

		while (dl) {
			if (dl->type==DL_INDEX3) tot= dl->nr;
			else tot= dl->nr*dl->parts;
			
			vert= dl->verts;
			for (a=0; a<tot; a++, vert+=3) {
				doit= 1;
				DO_MINMAX(vert, min, max);
			}

			dl= dl->next;
		}
		
		if (!doit) {
			/* there's no geometry in displist, use zero-sized boundbox */
			zero_v3(min);
			zero_v3(max);
		}
		
	}
	
	if (bb) {
		boundbox_set_from_min_max(bb, min, max);
	}
}

