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

/** \file blender/blenkernel/intern/curve.c
 *  \ingroup bke
 */


#include <math.h>  // floor
#include <string.h>
#include <stdlib.h>  

#include "MEM_guardedalloc.h"

#include "BLI_bpath.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "DNA_curve_types.h"  
#include "DNA_material_types.h"  

/* for dereferencing pointers */
#include "DNA_key_types.h"  
#include "DNA_scene_types.h"  
#include "DNA_vfont_types.h"  
#include "DNA_object_types.h"

#include "BKE_animsys.h"
#include "BKE_anim.h"  
#include "BKE_curve.h"  
#include "BKE_displist.h"  
#include "BKE_font.h" 
#include "BKE_global.h" 
#include "BKE_key.h"  
#include "BKE_library.h"  
#include "BKE_main.h"  
#include "BKE_object.h"
#include "BKE_material.h"

/* globals */

/* local */
static int cu_isectLL(const float v1[3], const float v2[3], const float v3[3], const float v4[3],
                      short cox, short coy,
                      float *labda, float *mu, float vec[3]);

void BKE_curve_unlink(Curve *cu)
{
	int a;
	
	for (a=0; a<cu->totcol; a++) {
		if (cu->mat[a]) cu->mat[a]->id.us--;
		cu->mat[a]= NULL;
	}
	if (cu->vfont) cu->vfont->id.us--; 
	cu->vfont= NULL;

	if (cu->vfontb) cu->vfontb->id.us--; 
	cu->vfontb= NULL;

	if (cu->vfonti) cu->vfonti->id.us--; 
	cu->vfonti= NULL;

	if (cu->vfontbi) cu->vfontbi->id.us--; 
	cu->vfontbi= NULL;
	
	if (cu->key) cu->key->id.us--;
	cu->key= NULL;
}

/* frees editcurve entirely */
void BKE_curve_editfont_free(Curve *cu)
{
	if (cu->editfont) {
		EditFont *ef= cu->editfont;
		
		if (ef->oldstr) MEM_freeN(ef->oldstr);
		if (ef->oldstrinfo) MEM_freeN(ef->oldstrinfo);
		if (ef->textbuf) MEM_freeN(ef->textbuf);
		if (ef->textbufinfo) MEM_freeN(ef->textbufinfo);
		if (ef->copybuf) MEM_freeN(ef->copybuf);
		if (ef->copybufinfo) MEM_freeN(ef->copybufinfo);
		
		MEM_freeN(ef);
		cu->editfont= NULL;
	}
}

void BKE_curve_editNurb_keyIndex_free(EditNurb *editnurb)
{
	if (!editnurb->keyindex) {
		return;
	}
	BLI_ghash_free(editnurb->keyindex, NULL, (GHashValFreeFP)MEM_freeN);
	editnurb->keyindex= NULL;
}

void BKE_curve_editNurb_free (Curve *cu)
{
	if (cu->editnurb) {
		BKE_nurbList_free(&cu->editnurb->nurbs);
		BKE_curve_editNurb_keyIndex_free(cu->editnurb);
		MEM_freeN(cu->editnurb);
		cu->editnurb= NULL;
	}
}

/* don't free curve itself */
void BKE_curve_free(Curve *cu)
{
	BKE_nurbList_free(&cu->nurb);
	BLI_freelistN(&cu->bev);
	freedisplist(&cu->disp);
	BKE_curve_editfont_free(cu);

	BKE_curve_editNurb_free(cu);
	BKE_curve_unlink(cu);
	BKE_free_animdata((ID *)cu);
	
	if (cu->mat) MEM_freeN(cu->mat);
	if (cu->str) MEM_freeN(cu->str);
	if (cu->strinfo) MEM_freeN(cu->strinfo);
	if (cu->bb) MEM_freeN(cu->bb);
	if (cu->path) free_path(cu->path);
	if (cu->tb) MEM_freeN(cu->tb);
}

Curve *BKE_curve_add(const char *name, int type)
{
	Curve *cu;

	cu = alloc_libblock(&G.main->curve, ID_CU, name);
	copy_v3_fl(cu->size, 1.0f);
	cu->flag= CU_FRONT|CU_BACK|CU_DEFORM_BOUNDS_OFF|CU_PATH_RADIUS;
	cu->pathlen= 100;
	cu->resolu= cu->resolv= (type == OB_SURF) ? 4 : 12;
	cu->width= 1.0;
	cu->wordspace = 1.0;
	cu->spacing= cu->linedist= 1.0;
	cu->fsize= 1.0;
	cu->ulheight = 0.05;	
	cu->texflag= CU_AUTOSPACE;
	cu->smallcaps_scale= 0.75f;
	cu->twist_mode= CU_TWIST_MINIMUM;	// XXX: this one seems to be the best one in most cases, at least for curve deform...
	cu->type= type;
	
	cu->bb= unit_boundbox();
	
	if (type==OB_FONT) {
		cu->vfont= cu->vfontb= cu->vfonti= cu->vfontbi= get_builtin_font();
		cu->vfont->id.us+=4;
		cu->str= MEM_mallocN(12, "str");
		BLI_strncpy(cu->str, "Text", 12);
		cu->len= cu->pos= 4;
		cu->strinfo= MEM_callocN(12*sizeof(CharInfo), "strinfo new");
		cu->totbox= cu->actbox= 1;
		cu->tb= MEM_callocN(MAXTEXTBOX*sizeof(TextBox), "textbox");
		cu->tb[0].w = cu->tb[0].h = 0.0;
	}
	
	return cu;
}

Curve *BKE_curve_copy(Curve *cu)
{
	Curve *cun;
	int a;
	
	cun= copy_libblock(&cu->id);
	cun->nurb.first= cun->nurb.last= NULL;
	BKE_nurbList_duplicate( &(cun->nurb), &(cu->nurb));

	cun->mat= MEM_dupallocN(cu->mat);
	for (a=0; a<cun->totcol; a++) {
		id_us_plus((ID *)cun->mat[a]);
	}
	
	cun->str= MEM_dupallocN(cu->str);
	cun->strinfo= MEM_dupallocN(cu->strinfo);	
	cun->tb= MEM_dupallocN(cu->tb);
	cun->bb= MEM_dupallocN(cu->bb);
	
	cun->key= copy_key(cu->key);
	if (cun->key) cun->key->from= (ID *)cun;
	
	cun->disp.first= cun->disp.last= NULL;
	cun->bev.first= cun->bev.last= NULL;
	cun->path= NULL;

	cun->editnurb= NULL;
	cun->editfont= NULL;
	cun->selboxes= NULL;

#if 0	// XXX old animation system
	/* single user ipo too */
	if (cun->ipo) cun->ipo= copy_ipo(cun->ipo);
#endif // XXX old animation system

	id_us_plus((ID *)cun->vfont);
	id_us_plus((ID *)cun->vfontb);	
	id_us_plus((ID *)cun->vfonti);
	id_us_plus((ID *)cun->vfontbi);
	
	return cun;
}

static void extern_local_curve(Curve *cu)
{	
	id_lib_extern((ID *)cu->vfont);
	id_lib_extern((ID *)cu->vfontb);	
	id_lib_extern((ID *)cu->vfonti);
	id_lib_extern((ID *)cu->vfontbi);
	
	if (cu->mat) {
		extern_local_matarar(cu->mat, cu->totcol);
	}
}

void BKE_curve_make_local(Curve *cu)
{
	Main *bmain= G.main;
	Object *ob;
	int is_local= FALSE, is_lib= FALSE;
	
	/* - when there are only lib users: don't do
	 * - when there are only local users: set flag
	 * - mixed: do a copy
	 */
	
	if (cu->id.lib==NULL) return;

	if (cu->id.us==1) {
		id_clear_lib_data(bmain, &cu->id);
		extern_local_curve(cu);
		return;
	}

	for (ob= bmain->object.first; ob && ELEM(0, is_lib, is_local); ob= ob->id.next) {
		if (ob->data == cu) {
			if (ob->id.lib) is_lib= TRUE;
			else is_local= TRUE;
		}
	}

	if (is_local && is_lib == FALSE) {
		id_clear_lib_data(bmain, &cu->id);
		extern_local_curve(cu);
	}
	else if (is_local && is_lib) {
		Curve *cu_new= BKE_curve_copy(cu);
		cu_new->id.us= 0;

		BKE_id_lib_local_paths(bmain, cu->id.lib, &cu_new->id);

		for (ob= bmain->object.first; ob; ob= ob->id.next) {
			if (ob->data==cu) {
				if (ob->id.lib==NULL) {
					ob->data= cu_new;
					cu_new->id.us++;
					cu->id.us--;
				}
			}
		}
	}
}

/* Get list of nurbs from editnurbs structure */
ListBase *BKE_curve_editNurbs_get(Curve *cu)
{
	if (cu->editnurb) {
		return &cu->editnurb->nurbs;
	}

	return NULL;
}

short BKE_curve_type_get(Curve *cu)
{
	Nurb *nu;
	int type= cu->type;

	if (cu->vfont) {
		return OB_FONT;
	}

	if (!cu->type) {
		type= OB_CURVE;

		for (nu= cu->nurb.first; nu; nu= nu->next) {
			if (nu->pntsv>1) {
				type= OB_SURF;
			}
		}
	}

	return type;
}

void BKE_curve_curve_dimension_update(Curve *cu)
{
	ListBase *nurbs= BKE_curve_nurbs_get(cu);
	Nurb *nu= nurbs->first;

	if (cu->flag&CU_3D) {
		for ( ; nu; nu= nu->next) {
			nu->flag &= ~CU_2D;
		}
	}
	else {
		for ( ; nu; nu= nu->next) {
			nu->flag |= CU_2D;
			BKE_nurb_test2D(nu);

			/* since the handles are moved they need to be auto-located again */
			if (nu->type == CU_BEZIER)
				BKE_nurb_handles_calc(nu);
		}
	}
}

void BKE_curve_type_test(Object *ob)
{
	ob->type= BKE_curve_type_get(ob->data);

	if (ob->type==OB_CURVE)
		BKE_curve_curve_dimension_update((Curve *)ob->data);
}

void BKE_curve_tex_space_calc(Curve *cu)
{
	DispList *dl;
	BoundBox *bb;
	float *fp, min[3], max[3];
	int tot, doit= 0;
	
	if (cu->bb==NULL) cu->bb= MEM_callocN(sizeof(BoundBox), "boundbox");
	bb= cu->bb;
	
	INIT_MINMAX(min, max);

	dl= cu->disp.first;
	while (dl) {
		
		tot = ELEM(dl->type, DL_INDEX3, DL_INDEX4) ? dl->nr : dl->nr * dl->parts;

		if (tot) doit= 1;
		fp= dl->verts;
		while (tot--) {
			DO_MINMAX(fp, min, max);
			fp += 3;
		}
		dl= dl->next;
	}

	if (!doit) {
		min[0] = min[1] = min[2] = -1.0f;
		max[0] = max[1] = max[2] = 1.0f;
	}

	boundbox_set_from_min_max(bb, min, max);

	if (cu->texflag & CU_AUTOSPACE) {
		mid_v3_v3v3(cu->loc, min, max);
		cu->size[0]= (max[0]-min[0])/2.0f;
		cu->size[1]= (max[1]-min[1])/2.0f;
		cu->size[2]= (max[2]-min[2])/2.0f;

		zero_v3(cu->rot);

		if (cu->size[0]==0.0f) cu->size[0]= 1.0f;
		else if (cu->size[0]>0.0f && cu->size[0]<0.00001f) cu->size[0]= 0.00001f;
		else if (cu->size[0]<0.0f && cu->size[0]> -0.00001f) cu->size[0]= -0.00001f;
	
		if (cu->size[1]==0.0f) cu->size[1]= 1.0f;
		else if (cu->size[1]>0.0f && cu->size[1]<0.00001f) cu->size[1]= 0.00001f;
		else if (cu->size[1]<0.0f && cu->size[1]> -0.00001f) cu->size[1]= -0.00001f;
	
		if (cu->size[2]==0.0f) cu->size[2]= 1.0f;
		else if (cu->size[2]>0.0f && cu->size[2]<0.00001f) cu->size[2]= 0.00001f;
		else if (cu->size[2]<0.0f && cu->size[2]> -0.00001f) cu->size[2]= -0.00001f;

	}
}

int BKE_nurbList_verts_count(ListBase *nurb)
{
	Nurb *nu;
	int tot=0;
	
	nu= nurb->first;
	while (nu) {
		if (nu->bezt) tot+= 3*nu->pntsu;
		else if (nu->bp) tot+= nu->pntsu*nu->pntsv;
		
		nu= nu->next;
	}
	return tot;
}

int BKE_nurbList_verts_count_without_handles(ListBase *nurb)
{
	Nurb *nu;
	int tot=0;
	
	nu= nurb->first;
	while (nu) {
		if (nu->bezt) tot+= nu->pntsu;
		else if (nu->bp) tot+= nu->pntsu*nu->pntsv;
		
		nu= nu->next;
	}
	return tot;
}

/* **************** NURBS ROUTINES ******************** */

void BKE_nurb_free(Nurb *nu)
{

	if (nu==NULL) return;

	if (nu->bezt) MEM_freeN(nu->bezt);
	nu->bezt= NULL;
	if (nu->bp) MEM_freeN(nu->bp);
	nu->bp= NULL;
	if (nu->knotsu) MEM_freeN(nu->knotsu);
	nu->knotsu= NULL;
	if (nu->knotsv) MEM_freeN(nu->knotsv);
	nu->knotsv= NULL;
	/* if (nu->trim.first) freeNurblist(&(nu->trim)); */

	MEM_freeN(nu);

}


void BKE_nurbList_free(ListBase *lb)
{
	Nurb *nu, *next;

	if (lb==NULL) return;

	nu= lb->first;
	while (nu) {
		next= nu->next;
		BKE_nurb_free(nu);
		nu= next;
	}
	lb->first= lb->last= NULL;
}

Nurb *BKE_nurb_duplicate(Nurb *nu)
{
	Nurb *newnu;
	int len;

	newnu= (Nurb*)MEM_mallocN(sizeof(Nurb),"duplicateNurb");
	if (newnu==NULL) return NULL;
	memcpy(newnu, nu, sizeof(Nurb));

	if (nu->bezt) {
		newnu->bezt=
			(BezTriple*)MEM_mallocN((nu->pntsu)* sizeof(BezTriple),"duplicateNurb2");
		memcpy(newnu->bezt, nu->bezt, nu->pntsu*sizeof(BezTriple));
	}
	else {
		len= nu->pntsu*nu->pntsv;
		newnu->bp=
			(BPoint*)MEM_mallocN((len)* sizeof(BPoint),"duplicateNurb3");
		memcpy(newnu->bp, nu->bp, len*sizeof(BPoint));
		
		newnu->knotsu= newnu->knotsv= NULL;
		
		if (nu->knotsu) {
			len= KNOTSU(nu);
			if (len) {
				newnu->knotsu= MEM_mallocN(len*sizeof(float), "duplicateNurb4");
				memcpy(newnu->knotsu, nu->knotsu, sizeof(float)*len);
			}
		}
		if (nu->pntsv>1 && nu->knotsv) {
			len= KNOTSV(nu);
			if (len) {
				newnu->knotsv= MEM_mallocN(len*sizeof(float), "duplicateNurb5");
				memcpy(newnu->knotsv, nu->knotsv, sizeof(float)*len);
			}
		}
	}
	return newnu;
}

void BKE_nurbList_duplicate(ListBase *lb1, ListBase *lb2)
{
	Nurb *nu, *nun;
	
	BKE_nurbList_free(lb1);
	
	nu= lb2->first;
	while (nu) {
		nun= BKE_nurb_duplicate(nu);
		BLI_addtail(lb1, nun);
		
		nu= nu->next;
	}
}

void BKE_nurb_test2D(Nurb *nu)
{
	BezTriple *bezt;
	BPoint *bp;
	int a;
	
	if ((nu->flag & CU_2D)==0)
		return;

	if (nu->type == CU_BEZIER) {
		a= nu->pntsu;
		bezt= nu->bezt;
		while (a--) {
			bezt->vec[0][2]= 0.0; 
			bezt->vec[1][2]= 0.0; 
			bezt->vec[2][2]= 0.0;
			bezt++;
		}
	}
	else {
		a= nu->pntsu*nu->pntsv;
		bp= nu->bp;
		while (a--) {
			bp->vec[2]= 0.0;
			bp++;
		}
	}
}

void BKE_nurb_minmax(Nurb *nu, float *min, float *max)
{
	BezTriple *bezt;
	BPoint *bp;
	int a;

	if (nu->type == CU_BEZIER) {
		a= nu->pntsu;
		bezt= nu->bezt;
		while (a--) {
			DO_MINMAX(bezt->vec[0], min, max);
			DO_MINMAX(bezt->vec[1], min, max);
			DO_MINMAX(bezt->vec[2], min, max);
			bezt++;
		}
	}
	else {
		a= nu->pntsu*nu->pntsv;
		bp= nu->bp;
		while (a--) {
			DO_MINMAX(bp->vec, min, max);
			bp++;
		}
	}
}

/* be sure to call makeknots after this */
void BKE_nurb_points_add(Nurb *nu, int number)
{
	BPoint *tmp= nu->bp;
	int i;
	nu->bp= (BPoint *)MEM_mallocN((nu->pntsu + number) * sizeof(BPoint), "rna_Curve_spline_points_add");

	if (tmp) {
		memmove(nu->bp, tmp, nu->pntsu * sizeof(BPoint));
		MEM_freeN(tmp);
	}

	memset(nu->bp + nu->pntsu, 0, number * sizeof(BPoint));

	for (i=0, tmp= nu->bp + nu->pntsu; i < number; i++, tmp++) {
		tmp->radius= 1.0f;
	}

	nu->pntsu += number;
}

void BKE_nurb_bezierPoints_add(Nurb *nu, int number)
{
	BezTriple *tmp= nu->bezt;
	int i;
	nu->bezt= (BezTriple *)MEM_mallocN((nu->pntsu + number) * sizeof(BezTriple), "rna_Curve_spline_points_add");

	if (tmp) {
		memmove(nu->bezt, tmp, nu->pntsu * sizeof(BezTriple));
		MEM_freeN(tmp);
	}

	memset(nu->bezt + nu->pntsu, 0, number * sizeof(BezTriple));

	for (i=0, tmp= nu->bezt + nu->pntsu; i < number; i++, tmp++) {
		tmp->radius= 1.0f;
	}

	nu->pntsu += number;
}

/* ~~~~~~~~~~~~~~~~~~~~Non Uniform Rational B Spline calculations ~~~~~~~~~~~ */


static void calcknots(float *knots, const short pnts, const short order, const short flag)
{
	/* knots: number of pnts NOT corrected for cyclic */
	const int pnts_order= pnts + order;
	float k;
	int a;

	switch (flag & (CU_NURB_ENDPOINT|CU_NURB_BEZIER)) {
	case CU_NURB_ENDPOINT:
		k= 0.0;
		for (a=1; a <= pnts_order; a++) {
			knots[a-1]= k;
			if (a >= order && a <= pnts) k+= 1.0f;
		}
		break;
	case CU_NURB_BEZIER:
		/* Warning, the order MUST be 2 or 4,
		 * if this is not enforced, the displist will be corrupt */
		if (order==4) {
			k= 0.34;
			for (a=0; a < pnts_order; a++) {
				knots[a]= floorf(k);
				k+= (1.0f/3.0f);
			}
		}
		else if (order==3) {
			k= 0.6f;
			for (a=0; a < pnts_order; a++) {
				if (a >= order && a <= pnts) k+= 0.5f;
				knots[a]= floorf(k);
			}
		}
		else {
			printf("bez nurb curve order is not 3 or 4, should never happen\n");
		}
		break;
	default:
		for (a=0; a < pnts_order; a++) {
			knots[a]= (float)a;
		}
		break;
	}
}

static void makecyclicknots(float *knots, short pnts, short order)
/* pnts, order: number of pnts NOT corrected for cyclic */
{
	int a, b, order2, c;

	if (knots==NULL) return;

	order2=order-1;

	/* do first long rows (order -1), remove identical knots at endpoints */
	if (order>2) {
		b= pnts+order2;
		for (a=1; a<order2; a++) {
			if (knots[b]!= knots[b-a]) break;
		}
		if (a==order2) knots[pnts+order-2]+= 1.0f;
	}

	b= order;
		c=pnts + order + order2;
	for (a=pnts+order2; a<c; a++) {
		knots[a]= knots[a-1]+ (knots[b]-knots[b-1]);
		b--;
	}
}



static void makeknots(Nurb *nu, short uv)
{
	if (nu->type == CU_NURBS) {
		if (uv == 1) {
			if (nu->knotsu) MEM_freeN(nu->knotsu);
			if (BKE_nurb_check_valid_u(nu)) {
				nu->knotsu= MEM_callocN(4+sizeof(float)*KNOTSU(nu), "makeknots");
				if (nu->flagu & CU_NURB_CYCLIC) {
					calcknots(nu->knotsu, nu->pntsu, nu->orderu, 0);  /* cyclic should be uniform */
					makecyclicknots(nu->knotsu, nu->pntsu, nu->orderu);
				}
				else {
					calcknots(nu->knotsu, nu->pntsu, nu->orderu, nu->flagu);
				}
			}
			else nu->knotsu= NULL;
		
		}
		else if (uv == 2) {
			if (nu->knotsv) MEM_freeN(nu->knotsv);
			if (BKE_nurb_check_valid_v(nu)) {
				nu->knotsv= MEM_callocN(4+sizeof(float)*KNOTSV(nu), "makeknots");
				if (nu->flagv & CU_NURB_CYCLIC) {
					calcknots(nu->knotsv, nu->pntsv, nu->orderv, 0);  /* cyclic should be uniform */
					makecyclicknots(nu->knotsv, nu->pntsv, nu->orderv);
				}
				else {
					calcknots(nu->knotsv, nu->pntsv, nu->orderv, nu->flagv);
				}
			}
			else nu->knotsv= NULL;
		}
	}
}

void BKE_nurb_knot_calc_u(Nurb *nu)
{
	makeknots(nu, 1);
}

void BKE_nurb_knot_calc_v(Nurb *nu)
{
	makeknots(nu, 2);
}

static void basisNurb(float t, short order, short pnts, float *knots, float *basis, int *start, int *end)
{
	float d, e;
	int i, i1 = 0, i2 = 0 ,j, orderpluspnts, opp2, o2;

	orderpluspnts= order+pnts;
		opp2 = orderpluspnts-1;

	/* this is for float inaccuracy */
	if (t < knots[0]) t= knots[0];
	else if (t > knots[opp2]) t= knots[opp2];

	/* this part is order '1' */
		o2 = order + 1;
	for (i=0;i<opp2;i++) {
		if (knots[i]!=knots[i+1] && t>= knots[i] && t<=knots[i+1]) {
			basis[i]= 1.0;
			i1= i-o2;
			if (i1<0) i1= 0;
			i2= i;
			i++;
			while (i<opp2) {
				basis[i]= 0.0;
				i++;
			}
			break;
		}
		else basis[i]= 0.0;
	}
	basis[i]= 0.0;
	
	/* this is order 2,3,... */
	for (j=2; j<=order; j++) {

		if (i2+j>= orderpluspnts) i2= opp2-j;

		for (i= i1; i<=i2; i++) {
			if (basis[i]!=0.0f)
				d= ((t-knots[i])*basis[i]) / (knots[i+j-1]-knots[i]);
			else
				d= 0.0f;

			if (basis[i+1] != 0.0f)
				e= ((knots[i+j]-t)*basis[i+1]) / (knots[i+j]-knots[i+1]);
			else
				e= 0.0;

			basis[i]= d+e;
		}
	}

	*start= 1000;
	*end= 0;

	for (i=i1; i<=i2; i++) {
		if (basis[i] > 0.0f) {
			*end= i;
			if (*start==1000) *start= i;
		}
	}
}


void BKE_nurb_makeFaces(Nurb *nu, float *coord_array, int rowstride, int resolu, int resolv)
/* coord_array  has to be 3*4*resolu*resolv in size, and zero-ed */
{
	BPoint *bp;
	float *basisu, *basis, *basisv, *sum, *fp, *in;
	float u, v, ustart, uend, ustep, vstart, vend, vstep, sumdiv;
	int i, j, iofs, jofs, cycl, len, curu, curv;
	int istart, iend, jsta, jen, *jstart, *jend, ratcomp;
	
	int totu = nu->pntsu*resolu, totv = nu->pntsv*resolv;
	
	if (nu->knotsu==NULL || nu->knotsv==NULL) return;
	if (nu->orderu>nu->pntsu) return;
	if (nu->orderv>nu->pntsv) return;
	if (coord_array==NULL) return;
	
	/* allocate and initialize */
	len = totu * totv;
	if (len==0) return;
	

	
	sum= (float *)MEM_callocN(sizeof(float)*len, "makeNurbfaces1");
	
	len= totu*totv;
	if (len==0) {
		MEM_freeN(sum);
		return;
	}

	bp= nu->bp;
	i= nu->pntsu*nu->pntsv;
	ratcomp=0;
	while (i--) {
		if (bp->vec[3] != 1.0f) {
			ratcomp= 1;
			break;
		}
		bp++;
	}
	
	fp= nu->knotsu;
	ustart= fp[nu->orderu-1];
	if (nu->flagu & CU_NURB_CYCLIC) uend= fp[nu->pntsu+nu->orderu-1];
	else uend= fp[nu->pntsu];
	ustep= (uend-ustart)/((nu->flagu & CU_NURB_CYCLIC) ? totu : totu - 1);
	
	basisu= (float *)MEM_mallocN(sizeof(float)*KNOTSU(nu), "makeNurbfaces3");

	fp= nu->knotsv;
	vstart= fp[nu->orderv-1];
	
	if (nu->flagv & CU_NURB_CYCLIC) vend= fp[nu->pntsv+nu->orderv-1];
	else vend= fp[nu->pntsv];
	vstep= (vend-vstart)/((nu->flagv & CU_NURB_CYCLIC) ? totv : totv - 1);
	
	len= KNOTSV(nu);
	basisv= (float *)MEM_mallocN(sizeof(float)*len*totv, "makeNurbfaces3");
	jstart= (int *)MEM_mallocN(sizeof(float)*totv, "makeNurbfaces4");
	jend= (int *)MEM_mallocN(sizeof(float)*totv, "makeNurbfaces5");

	/* precalculation of basisv and jstart,jend */
	if (nu->flagv & CU_NURB_CYCLIC) cycl= nu->orderv-1; 
	else cycl= 0;
	v= vstart;
	basis= basisv;
	curv= totv;
	while (curv--) {
		basisNurb(v, nu->orderv, (short)(nu->pntsv+cycl), nu->knotsv, basis, jstart+curv, jend+curv);
		basis+= KNOTSV(nu);
		v+= vstep;
	}

	if (nu->flagu & CU_NURB_CYCLIC) cycl= nu->orderu-1; 
	else cycl= 0;
	in= coord_array;
	u= ustart;
	curu= totu;
	while (curu--) {

		basisNurb(u, nu->orderu, (short)(nu->pntsu+cycl), nu->knotsu, basisu, &istart, &iend);

		basis= basisv;
		curv= totv;
		while (curv--) {

			jsta= jstart[curv];
			jen= jend[curv];

			/* calculate sum */
			sumdiv= 0.0;
			fp= sum;

			for (j= jsta; j<=jen; j++) {

				if (j>=nu->pntsv) jofs= (j - nu->pntsv);
				else jofs= j;
				bp= nu->bp+ nu->pntsu*jofs+istart-1;

				for (i= istart; i<=iend; i++, fp++) {

					if (i>= nu->pntsu) {
						iofs= i- nu->pntsu;
						bp= nu->bp+ nu->pntsu*jofs+iofs;
					}
					else bp++;

					if (ratcomp) {
						*fp= basisu[i]*basis[j]*bp->vec[3];
						sumdiv+= *fp;
					}
					else *fp= basisu[i]*basis[j];
				}
			}
		
			if (ratcomp) {
				fp= sum;
				for (j= jsta; j<=jen; j++) {
					for (i= istart; i<=iend; i++, fp++) {
						*fp/= sumdiv;
					}
				}
			}

			/* one! (1.0) real point now */
			fp= sum;
			for (j= jsta; j<=jen; j++) {

				if (j>=nu->pntsv) jofs= (j - nu->pntsv);
				else jofs= j;
				bp= nu->bp+ nu->pntsu*jofs+istart-1;

				for (i= istart; i<=iend; i++, fp++) {

					if (i>= nu->pntsu) {
						iofs= i- nu->pntsu;
						bp= nu->bp+ nu->pntsu*jofs+iofs;
					}
					else bp++;

					if (*fp != 0.0f) {
						in[0]+= (*fp) * bp->vec[0];
						in[1]+= (*fp) * bp->vec[1];
						in[2]+= (*fp) * bp->vec[2];
					}
				}
			}

			in+=3;
			basis+= KNOTSV(nu);
		}
		u+= ustep;
		if (rowstride!=0) in = (float*) (((unsigned char*) in) + (rowstride - 3*totv*sizeof(*in)));
	}

	/* free */
	MEM_freeN(sum);
	MEM_freeN(basisu);
	MEM_freeN(basisv);
	MEM_freeN(jstart);
	MEM_freeN(jend);
}

void BKE_nurb_makeCurve(Nurb *nu, float *coord_array, float *tilt_array, float *radius_array, float *weight_array, int resolu, int stride)
/* coord_array has to be 3*4*pntsu*resolu in size and zero-ed
 * tilt_array and radius_array will be written to if valid */
{
	BPoint *bp;
	float u, ustart, uend, ustep, sumdiv;
	float *basisu, *sum, *fp;
	float *coord_fp= coord_array, *tilt_fp= tilt_array, *radius_fp= radius_array, *weight_fp= weight_array;
	int i, len, istart, iend, cycl;

	if (nu->knotsu==NULL) return;
	if (nu->orderu>nu->pntsu) return;
	if (coord_array==NULL) return;

	/* allocate and initialize */
	len= nu->pntsu;
	if (len==0) return;
	sum= (float *)MEM_callocN(sizeof(float)*len, "makeNurbcurve1");
	
	resolu= (resolu*SEGMENTSU(nu));
	
	if (resolu==0) {
		MEM_freeN(sum);
		return;
	}

	fp= nu->knotsu;
	ustart= fp[nu->orderu-1];
	if (nu->flagu & CU_NURB_CYCLIC) uend= fp[nu->pntsu+nu->orderu-1];
	else uend= fp[nu->pntsu];
	ustep= (uend-ustart)/(resolu - ((nu->flagu & CU_NURB_CYCLIC) ? 0 : 1));
	
	basisu= (float *)MEM_mallocN(sizeof(float)*KNOTSU(nu), "makeNurbcurve3");

	if (nu->flagu & CU_NURB_CYCLIC) cycl= nu->orderu-1; 
	else cycl= 0;

	u= ustart;
	while (resolu--) {

		basisNurb(u, nu->orderu, (short)(nu->pntsu+cycl), nu->knotsu, basisu, &istart, &iend);
		/* calc sum */
		sumdiv= 0.0;
		fp= sum;
		bp= nu->bp+ istart-1;
		for (i= istart; i<=iend; i++, fp++) {

			if (i>=nu->pntsu) bp= nu->bp+(i - nu->pntsu);
			else bp++;

			*fp= basisu[i]*bp->vec[3];
			sumdiv+= *fp;
		}
		if (sumdiv != 0.0f) if (sumdiv < 0.999f || sumdiv > 1.001f) {
			/* is normalizing needed? */
			fp= sum;
			for (i= istart; i<=iend; i++, fp++) {
				*fp/= sumdiv;
			}
		}

		/* one! (1.0) real point */
		fp= sum;
		bp= nu->bp+ istart-1;
		for (i= istart; i<=iend; i++, fp++) {

			if (i>=nu->pntsu) bp= nu->bp+(i - nu->pntsu);
			else bp++;

			if (*fp != 0.0f) {
				
				coord_fp[0]+= (*fp) * bp->vec[0];
				coord_fp[1]+= (*fp) * bp->vec[1];
				coord_fp[2]+= (*fp) * bp->vec[2];
				
				if (tilt_fp)
					(*tilt_fp) += (*fp) * bp->alfa;
				
				if (radius_fp)
					(*radius_fp) += (*fp) * bp->radius;

				if (weight_fp)
					(*weight_fp) += (*fp) * bp->weight;
				
			}
		}

		coord_fp = (float *)(((char *)coord_fp) + stride);
		
		if (tilt_fp)	tilt_fp = (float *)(((char *)tilt_fp) + stride);
		if (radius_fp)	radius_fp = (float *)(((char *)radius_fp) + stride);
		if (weight_fp)	weight_fp = (float *)(((char *)weight_fp) + stride);
		
		u+= ustep;
	}

	/* free */
	MEM_freeN(sum);
	MEM_freeN(basisu);
}

/* forward differencing method for bezier curve */
void BKE_curve_forward_diff_bezier(float q0, float q1, float q2, float q3, float *p, int it, int stride)
{
	float rt0,rt1,rt2,rt3,f;
	int a;

	f= (float)it;
	rt0= q0;
	rt1= 3.0f*(q1-q0)/f;
	f*= f;
	rt2= 3.0f*(q0-2.0f*q1+q2)/f;
	f*= it;
	rt3= (q3-q0+3.0f*(q1-q2))/f;

	q0= rt0;
	q1= rt1+rt2+rt3;
	q2= 2*rt2+6*rt3;
	q3= 6*rt3;

	for (a=0; a<=it; a++) {
		*p= q0;
		p = (float *)(((char *)p)+stride);
		q0+= q1;
		q1+= q2;
		q2+= q3;
	}
}

static void forward_diff_bezier_cotangent(float *p0, float *p1, float *p2, float *p3, float *p, int it, int stride)
{
	/* note that these are not purpendicular to the curve
	 * they need to be rotated for this,
	 *
	 * This could also be optimized like forward_diff_bezier */
	int a;
	for (a=0; a<=it; a++) {
		float t = (float)a / (float)it;

		int i;
		for (i=0; i<3; i++) {
			p[i]= (-6*t + 6)*p0[i] + (18*t - 12)*p1[i] + (-18*t + 6)*p2[i] + (6*t)*p3[i];
		}
		normalize_v3(p);
		p = (float *)(((char *)p)+stride);
	}
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

float *BKE_curve_surf_make_orco(Object *ob)
{
	/* Note: this function is used in convertblender only atm, so
	 * suppose nonzero curve's render resolution should always be used */
	Curve *cu= ob->data;
	Nurb *nu;
	int a, b, tot=0;
	int sizeu, sizev;
	int resolu, resolv;
	float *fp, *coord_array;
	
	/* first calculate the size of the datablock */
	nu= cu->nurb.first;
	while (nu) {
		/* as we want to avoid the seam in a cyclic nurbs
		 * texture wrapping, reserve extra orco data space to save these extra needed
		 * vertex based UV coordinates for the meridian vertices.
		 * Vertices on the 0/2pi boundary are not duplicated inside the displist but later in
		 * the renderface/vert construction.
		 *
		 * See also convertblender.c: init_render_surf()
		 */

		resolu= cu->resolu_ren ? cu->resolu_ren : nu->resolu;
		resolv= cu->resolv_ren ? cu->resolv_ren : nu->resolv;
		
		sizeu = nu->pntsu*resolu;
		sizev = nu->pntsv*resolv;
		if (nu->flagu & CU_NURB_CYCLIC) sizeu++;
		if (nu->flagv & CU_NURB_CYCLIC) sizev++;
		if (nu->pntsv>1) tot+= sizeu * sizev;
		
		nu= nu->next;
	}
	/* makeNurbfaces wants zeros */
	fp= coord_array= MEM_callocN(3*sizeof(float)*tot, "make_orco");
	
	nu= cu->nurb.first;
	while (nu) {
		resolu= cu->resolu_ren ? cu->resolu_ren : nu->resolu;
		resolv= cu->resolv_ren ? cu->resolv_ren : nu->resolv;

		if (nu->pntsv>1) {
			sizeu = nu->pntsu*resolu;
			sizev = nu->pntsv*resolv;
			if (nu->flagu & CU_NURB_CYCLIC) sizeu++;
			if (nu->flagv & CU_NURB_CYCLIC) sizev++;
			
			if (cu->flag & CU_UV_ORCO) {
				for (b=0; b< sizeu; b++) {
					for (a=0; a< sizev; a++) {
						
						if (sizev <2) fp[0]= 0.0f;
						else fp[0]= -1.0f + 2.0f*((float)a)/(sizev - 1);
						
						if (sizeu <2) fp[1]= 0.0f;
						else fp[1]= -1.0f + 2.0f*((float)b)/(sizeu - 1);
						
						fp[2]= 0.0;
						
						fp+= 3;
					}
				}
			}
			else {
				float *_tdata= MEM_callocN((nu->pntsu*resolu) * (nu->pntsv*resolv) *3*sizeof(float), "temp data");
				float *tdata= _tdata;
				
				BKE_nurb_makeFaces(nu, tdata, 0, resolu, resolv);
				
				for (b=0; b<sizeu; b++) {
					int use_b= b;
					if (b==sizeu-1 && (nu->flagu & CU_NURB_CYCLIC))
						use_b= 0;
					
					for (a=0; a<sizev; a++) {
						int use_a= a;
						if (a==sizev-1 && (nu->flagv & CU_NURB_CYCLIC))
							use_a= 0;
						
						tdata = _tdata + 3 * (use_b * (nu->pntsv*resolv) + use_a);
						
						fp[0]= (tdata[0]-cu->loc[0])/cu->size[0];
						fp[1]= (tdata[1]-cu->loc[1])/cu->size[1];
						fp[2]= (tdata[2]-cu->loc[2])/cu->size[2];
						fp+= 3;
					}
				}
				
				MEM_freeN(_tdata);
			}
		}
		nu= nu->next;
	}
	
	return coord_array;
}


	/* NOTE: This routine is tied to the order of vertex
	 * built by displist and as passed to the renderer.
	 */
float *BKE_curve_make_orco(Scene *scene, Object *ob)
{
	Curve *cu = ob->data;
	DispList *dl;
	int u, v, numVerts;
	float *fp, *coord_array;
	ListBase disp = {NULL, NULL};

	makeDispListCurveTypes_forOrco(scene, ob, &disp);

	numVerts = 0;
	for (dl=disp.first; dl; dl=dl->next) {
		if (dl->type==DL_INDEX3) {
			numVerts += dl->nr;
		}
		else if (dl->type==DL_SURF) {
			/* convertblender.c uses the Surface code for creating renderfaces when cyclic U only (closed circle beveling) */
			if (dl->flag & DL_CYCL_U) {
				if (dl->flag & DL_CYCL_V)
					numVerts += (dl->parts+1)*(dl->nr+1);
				else
					numVerts += dl->parts*(dl->nr+1);
			}
			else
				numVerts += dl->parts*dl->nr;
		}
	}

	fp= coord_array= MEM_mallocN(3*sizeof(float)*numVerts, "cu_orco");
	for (dl=disp.first; dl; dl=dl->next) {
		if (dl->type==DL_INDEX3) {
			for (u=0; u<dl->nr; u++, fp+=3) {
				if (cu->flag & CU_UV_ORCO) {
					fp[0]= 2.0f*u/(dl->nr-1) - 1.0f;
					fp[1]= 0.0;
					fp[2]= 0.0;
				}
				else {
					copy_v3_v3(fp, &dl->verts[u*3]);

					fp[0]= (fp[0]-cu->loc[0])/cu->size[0];
					fp[1]= (fp[1]-cu->loc[1])/cu->size[1];
					fp[2]= (fp[2]-cu->loc[2])/cu->size[2];
				}
			}
		}
		else if (dl->type==DL_SURF) {
			int sizeu= dl->nr, sizev= dl->parts;
			
			/* exception as handled in convertblender.c too */
			if (dl->flag & DL_CYCL_U) {
				sizeu++;
				if (dl->flag & DL_CYCL_V)
					sizev++;
			}
			
			for (u=0; u<sizev; u++) {
				for (v=0; v<sizeu; v++,fp+=3) {
					if (cu->flag & CU_UV_ORCO) {
						fp[0]= 2.0f*u/(sizev - 1) - 1.0f;
						fp[1]= 2.0f*v/(sizeu - 1) - 1.0f;
						fp[2]= 0.0;
					}
					else {
						float *vert;
						int realv= v % dl->nr;
						int realu= u % dl->parts;
						
						vert= dl->verts + 3*(dl->nr*realu + realv);
						copy_v3_v3(fp, vert);

						fp[0]= (fp[0]-cu->loc[0])/cu->size[0];
						fp[1]= (fp[1]-cu->loc[1])/cu->size[1];
						fp[2]= (fp[2]-cu->loc[2])/cu->size[2];
					}
				}
			}
		}
	}

	freedisplist(&disp);

	return coord_array;
}


/* ***************** BEVEL ****************** */

void BKE_curve_bevel_make(Scene *scene, Object *ob, ListBase *disp, int forRender)
{
	DispList *dl, *dlnew;
	Curve *bevcu, *cu;
	float *fp, facx, facy, angle, dangle;
	int nr, a;

	cu= ob->data;
	disp->first = disp->last = NULL;

	/* if a font object is being edited, then do nothing */
// XXX	if ( ob == obedit && ob->type == OB_FONT ) return;

	if (cu->bevobj) {
		if (cu->bevobj->type!=OB_CURVE) return;

		bevcu= cu->bevobj->data;
		if (bevcu->ext1==0.0f && bevcu->ext2==0.0f) {
			ListBase bevdisp= {NULL, NULL};
			facx= cu->bevobj->size[0];
			facy= cu->bevobj->size[1];

			if (forRender) {
				makeDispListCurveTypes_forRender(scene, cu->bevobj, &bevdisp, NULL, 0);
				dl= bevdisp.first;
			}
			else {
				dl= cu->bevobj->disp.first;
				if (dl==NULL) {
					makeDispListCurveTypes(scene, cu->bevobj, 0);
					dl= cu->bevobj->disp.first;
				}
			}

			while (dl) {
				if (ELEM(dl->type, DL_POLY, DL_SEGM)) {
					dlnew= MEM_mallocN(sizeof(DispList), "makebevelcurve1");
					*dlnew= *dl;
					dlnew->verts= MEM_mallocN(3*sizeof(float)*dl->parts*dl->nr, "makebevelcurve1");
					memcpy(dlnew->verts, dl->verts, 3*sizeof(float)*dl->parts*dl->nr);

					if (dlnew->type==DL_SEGM) dlnew->flag |= (DL_FRONT_CURVE|DL_BACK_CURVE);

					BLI_addtail(disp, dlnew);
					fp= dlnew->verts;
					nr= dlnew->parts*dlnew->nr;
					while (nr--) {
						fp[2]= fp[1]*facy;
						fp[1]= -fp[0]*facx;
						fp[0]= 0.0;
						fp+= 3;
					}
				}
				dl= dl->next;
			}

			freedisplist(&bevdisp);
		}
	}
	else if (cu->ext1==0.0f && cu->ext2==0.0f) {
		;
	}
	else if (cu->ext2==0.0f) {
		dl= MEM_callocN(sizeof(DispList), "makebevelcurve2");
		dl->verts= MEM_mallocN(2*3*sizeof(float), "makebevelcurve2");
		BLI_addtail(disp, dl);
		dl->type= DL_SEGM;
		dl->parts= 1;
		dl->flag= DL_FRONT_CURVE|DL_BACK_CURVE;
		dl->nr= 2;
		
		fp= dl->verts;
		fp[0]= fp[1]= 0.0;
		fp[2]= -cu->ext1;
		fp[3]= fp[4]= 0.0;
		fp[5]= cu->ext1;
	}
	else if ( (cu->flag & (CU_FRONT|CU_BACK))==0 && cu->ext1==0.0f)	{ // we make a full round bevel in that case
		
		nr= 4+ 2*cu->bevresol;
		   
		dl= MEM_callocN(sizeof(DispList), "makebevelcurve p1");
		dl->verts= MEM_mallocN(nr*3*sizeof(float), "makebevelcurve p1");
		BLI_addtail(disp, dl);
		dl->type= DL_POLY;
		dl->parts= 1;
		dl->flag= DL_BACK_CURVE;
		dl->nr= nr;

		/* a circle */
		fp= dl->verts;
		dangle= (2.0f*(float)M_PI/(nr));
		angle= -(nr-1)*dangle;
		
		for (a=0; a<nr; a++) {
			fp[0]= 0.0;
			fp[1]= (cosf(angle)*(cu->ext2));
			fp[2]= (sinf(angle)*(cu->ext2)) - cu->ext1;
			angle+= dangle;
			fp+= 3;
		}
	}
	else {
		short dnr;
		
		/* bevel now in three parts, for proper vertex normals */
		/* part 1, back */

		if ((cu->flag & CU_BACK) || !(cu->flag & CU_FRONT)) {
			dnr= nr= 2+ cu->bevresol;
			if ( (cu->flag & (CU_FRONT|CU_BACK))==0)
				nr= 3+ 2*cu->bevresol;

			dl= MEM_callocN(sizeof(DispList), "makebevelcurve p1");
			dl->verts= MEM_mallocN(nr*3*sizeof(float), "makebevelcurve p1");
			BLI_addtail(disp, dl);
			dl->type= DL_SEGM;
			dl->parts= 1;
			dl->flag= DL_BACK_CURVE;
			dl->nr= nr;

			/* half a circle */
			fp= dl->verts;
			dangle= (0.5*M_PI/(dnr-1));
			angle= -(nr-1)*dangle;

			for (a=0; a<nr; a++) {
				fp[0]= 0.0;
				fp[1]= (float)(cosf(angle)*(cu->ext2));
				fp[2]= (float)(sinf(angle)*(cu->ext2)) - cu->ext1;
				angle+= dangle;
				fp+= 3;
			}
		}
		
		/* part 2, sidefaces */
		if (cu->ext1!=0.0f) {
			nr= 2;
			
			dl= MEM_callocN(sizeof(DispList), "makebevelcurve p2");
			dl->verts= MEM_callocN(nr*3*sizeof(float), "makebevelcurve p2");
			BLI_addtail(disp, dl);
			dl->type= DL_SEGM;
			dl->parts= 1;
			dl->nr= nr;
			
			fp= dl->verts;
			fp[1]= cu->ext2;
			fp[2]= -cu->ext1;
			fp[4]= cu->ext2;
			fp[5]= cu->ext1;
			
			if ( (cu->flag & (CU_FRONT|CU_BACK))==0) {
				dl= MEM_dupallocN(dl);
				dl->verts= MEM_dupallocN(dl->verts);
				BLI_addtail(disp, dl);
				
				fp= dl->verts;
				fp[1]= -fp[1];
				fp[2]= -fp[2];
				fp[4]= -fp[4];
				fp[5]= -fp[5];
			}
		}
		
		/* part 3, front */
		if ((cu->flag & CU_FRONT) || !(cu->flag & CU_BACK)) {
			dnr= nr= 2+ cu->bevresol;
			if ( (cu->flag & (CU_FRONT|CU_BACK))==0)
				nr= 3+ 2*cu->bevresol;

			dl= MEM_callocN(sizeof(DispList), "makebevelcurve p3");
			dl->verts= MEM_mallocN(nr*3*sizeof(float), "makebevelcurve p3");
			BLI_addtail(disp, dl);
			dl->type= DL_SEGM;
			dl->flag= DL_FRONT_CURVE;
			dl->parts= 1;
			dl->nr= nr;

			/* half a circle */
			fp= dl->verts;
			angle= 0.0;
			dangle= (0.5*M_PI/(dnr-1));

			for (a=0; a<nr; a++) {
				fp[0]= 0.0;
				fp[1]= (float)(cosf(angle)*(cu->ext2));
				fp[2]= (float)(sinf(angle)*(cu->ext2)) + cu->ext1;
				angle+= dangle;
				fp+= 3;
			}
		}
	}
}

static int cu_isectLL(const float v1[3], const float v2[3], const float v3[3], const float v4[3],
                      short cox, short coy,
                      float *labda, float *mu, float vec[3])
{
	/* return:
	 * -1: colliniar
	 *  0: no intersection of segments
	 *  1: exact intersection of segments
	 *  2: cross-intersection of segments
	 */
	float deler;

	deler= (v1[cox]-v2[cox])*(v3[coy]-v4[coy])-(v3[cox]-v4[cox])*(v1[coy]-v2[coy]);
	if (deler==0.0f) return -1;

	*labda= (v1[coy]-v3[coy])*(v3[cox]-v4[cox])-(v1[cox]-v3[cox])*(v3[coy]-v4[coy]);
	*labda= -(*labda/deler);

	deler= v3[coy]-v4[coy];
	if (deler==0) {
		deler=v3[cox]-v4[cox];
		*mu= -(*labda*(v2[cox]-v1[cox])+v1[cox]-v3[cox])/deler;
	}
	else {
		*mu= -(*labda*(v2[coy]-v1[coy])+v1[coy]-v3[coy])/deler;
	}
	vec[cox]= *labda*(v2[cox]-v1[cox])+v1[cox];
	vec[coy]= *labda*(v2[coy]-v1[coy])+v1[coy];

	if (*labda>=0.0f && *labda<=1.0f && *mu>=0.0f && *mu<=1.0f) {
		if (*labda==0.0f || *labda==1.0f || *mu==0.0f || *mu==1.0f) return 1;
		return 2;
	}
	return 0;
}


static short bevelinside(BevList *bl1,BevList *bl2)
{
	/* is bl2 INSIDE bl1 ? with left-right method and "labda's" */
	/* returns '1' if correct hole  */
	BevPoint *bevp, *prevbevp;
	float min,max,vec[3],hvec1[3],hvec2[3],lab,mu;
	int nr, links=0,rechts=0,mode;

	/* take first vertex of possible hole */

	bevp= (BevPoint *)(bl2+1);
	hvec1[0]= bevp->vec[0]; 
	hvec1[1]= bevp->vec[1]; 
	hvec1[2]= 0.0;
	copy_v3_v3(hvec2,hvec1);
	hvec2[0]+=1000;

	/* test it with all edges of potential surounding poly */
	/* count number of transitions left-right  */

	bevp= (BevPoint *)(bl1+1);
	nr= bl1->nr;
	prevbevp= bevp+(nr-1);

	while (nr--) {
		min= prevbevp->vec[1];
		max= bevp->vec[1];
		if (max<min) {
			min= max;
			max= prevbevp->vec[1];
		}
		if (min!=max) {
			if (min<=hvec1[1] && max>=hvec1[1]) {
				/* there's a transition, calc intersection point */
				mode= cu_isectLL(prevbevp->vec, bevp->vec, hvec1, hvec2, 0, 1, &lab, &mu, vec);
				/* if lab==0.0 or lab==1.0 then the edge intersects exactly a transition
				 * only allow for one situation: we choose lab= 1.0
				 */
				if (mode >= 0 && lab != 0.0f) {
					if (vec[0]<hvec1[0]) links++;
					else rechts++;
				}
			}
		}
		prevbevp= bevp;
		bevp++;
	}
	
	if ( (links & 1) && (rechts & 1) ) return 1;
	return 0;
}


struct bevelsort {
	float left;
	BevList *bl;
	int dir;
};

static int vergxcobev(const void *a1, const void *a2)
{
	const struct bevelsort *x1=a1,*x2=a2;

	if ( x1->left > x2->left ) return 1;
	else if ( x1->left < x2->left) return -1;
	return 0;
}

/* this function cannot be replaced with atan2, but why? */

static void calc_bevel_sin_cos(float x1, float y1, float x2, float y2, float *sina, float *cosa)
{
	float t01, t02, x3, y3;

	t01= (float)sqrt(x1*x1+y1*y1);
	t02= (float)sqrt(x2*x2+y2*y2);
	if (t01==0.0f) t01= 1.0f;
	if (t02==0.0f) t02= 1.0f;

	x1/=t01; 
	y1/=t01;
	x2/=t02; 
	y2/=t02;

	t02= x1*x2+y1*y2;
	if (fabs(t02)>=1.0) t02= .5*M_PI;
	else t02= (saacos(t02))/2.0f;

	t02= (float)sin(t02);
	if (t02==0.0f) t02= 1.0f;

	x3= x1-x2;
	y3= y1-y2;
	if (x3==0 && y3==0) {
		x3= y1;
		y3= -x1;
	}
	else {
		t01= (float)sqrt(x3*x3+y3*y3);
		x3/=t01; 
		y3/=t01;
	}

	*sina= -y3/t02;
	*cosa= x3/t02;

}

static void alfa_bezpart(BezTriple *prevbezt, BezTriple *bezt, Nurb *nu, float *tilt_array, float *radius_array, float *weight_array, int resolu, int stride)
{
	BezTriple *pprev, *next, *last;
	float fac, dfac, t[4];
	int a;
	
	if (tilt_array==NULL && radius_array==NULL)
		return;
	
	last= nu->bezt+(nu->pntsu-1);
	
	/* returns a point */
	if (prevbezt==nu->bezt) {
		if (nu->flagu & CU_NURB_CYCLIC) pprev= last;
		else pprev= prevbezt;
	}
	else pprev= prevbezt-1;
	
	/* next point */
	if (bezt==last) {
		if (nu->flagu & CU_NURB_CYCLIC) next= nu->bezt;
		else next= bezt;
	}
	else next= bezt+1;
	
	fac= 0.0;
	dfac= 1.0f/(float)resolu;
	
	for (a=0; a<resolu; a++, fac+= dfac) {
		if (tilt_array) {
			if (nu->tilt_interp==KEY_CU_EASE) { /* May as well support for tilt also 2.47 ease interp */
				*tilt_array = prevbezt->alfa + (bezt->alfa - prevbezt->alfa)*(3.0f*fac*fac - 2.0f*fac*fac*fac);
			}
			else {
				key_curve_position_weights(fac, t, nu->tilt_interp);
				*tilt_array= t[0]*pprev->alfa + t[1]*prevbezt->alfa + t[2]*bezt->alfa + t[3]*next->alfa;
			}
			
			tilt_array = (float *)(((char *)tilt_array) + stride); 
		}
		
		if (radius_array) {
			if (nu->radius_interp==KEY_CU_EASE) {
				/* Support 2.47 ease interp
				 * Note! - this only takes the 2 points into account,
				 * giving much more localized results to changes in radius, sometimes you want that */
				*radius_array = prevbezt->radius + (bezt->radius - prevbezt->radius)*(3.0f*fac*fac - 2.0f*fac*fac*fac);
			}
			else {
				
				/* reuse interpolation from tilt if we can */
				if (tilt_array==NULL || nu->tilt_interp != nu->radius_interp) {
					key_curve_position_weights(fac, t, nu->radius_interp);
				}
				*radius_array= t[0]*pprev->radius + t[1]*prevbezt->radius + t[2]*bezt->radius + t[3]*next->radius;
			}
			
			radius_array = (float *)(((char *)radius_array) + stride); 
		}

		if (weight_array) {
			/* basic interpolation for now, could copy tilt interp too  */
			*weight_array = prevbezt->weight + (bezt->weight - prevbezt->weight)*(3.0f*fac*fac - 2.0f*fac*fac*fac);

			weight_array = (float *)(((char *)weight_array) + stride);
		}
	}
}

/* make_bevel_list_3D_* funcs, at a minimum these must
 * fill in the bezp->quat and bezp->dir values */

/* correct non-cyclic cases by copying direction and rotation
 * values onto the first & last end-points */
static void bevel_list_cyclic_fix_3D(BevList *bl)
{
	BevPoint *bevp, *bevp1;

	bevp= (BevPoint *)(bl+1);
	bevp1= bevp+1;
	copy_qt_qt(bevp->quat, bevp1->quat);
	copy_v3_v3(bevp->dir, bevp1->dir);
	copy_v3_v3(bevp->tan, bevp1->tan);
	bevp= (BevPoint *)(bl+1);
	bevp+= (bl->nr-1);
	bevp1= bevp-1;
	copy_qt_qt(bevp->quat, bevp1->quat);
	copy_v3_v3(bevp->dir, bevp1->dir);
	copy_v3_v3(bevp->tan, bevp1->tan);
}
/* utility for make_bevel_list_3D_* funcs */
static void bevel_list_calc_bisect(BevList *bl)
{
	BevPoint *bevp2, *bevp1, *bevp0;
	int nr;

	bevp2= (BevPoint *)(bl+1);
	bevp1= bevp2+(bl->nr-1);
	bevp0= bevp1-1;

	nr= bl->nr;
	while (nr--) {
		/* totally simple */
		bisect_v3_v3v3v3(bevp1->dir, bevp0->vec, bevp1->vec, bevp2->vec);

		bevp0= bevp1;
		bevp1= bevp2;
		bevp2++;
	}
}
static void bevel_list_flip_tangents(BevList *bl)
{
	BevPoint *bevp2, *bevp1, *bevp0;
	int nr;

	bevp2= (BevPoint *)(bl+1);
	bevp1= bevp2+(bl->nr-1);
	bevp0= bevp1-1;

	nr= bl->nr;
	while (nr--) {
		if (RAD2DEGF(angle_v2v2(bevp0->tan, bevp1->tan)) > 90.0f)
			negate_v3(bevp1->tan);

		bevp0= bevp1;
		bevp1= bevp2;
		bevp2++;
	}
}
/* apply user tilt */
static void bevel_list_apply_tilt(BevList *bl)
{
	BevPoint *bevp2, *bevp1;
	int nr;
	float q[4];

	bevp2= (BevPoint *)(bl+1);
	bevp1= bevp2+(bl->nr-1);

	nr= bl->nr;
	while (nr--) {
		axis_angle_to_quat(q, bevp1->dir, bevp1->alfa);
		mul_qt_qtqt(bevp1->quat, q, bevp1->quat);
		normalize_qt(bevp1->quat);

		bevp1= bevp2;
		bevp2++;
	}
}
/* smooth quats, this function should be optimized, it can get slow with many iterations. */
static void bevel_list_smooth(BevList *bl, int smooth_iter)
{
	BevPoint *bevp2, *bevp1, *bevp0;
	int nr;

	float q[4];
	float bevp0_quat[4];
	int a;

	for (a=0; a < smooth_iter; a++) {

		bevp2= (BevPoint *)(bl+1);
		bevp1= bevp2+(bl->nr-1);
		bevp0= bevp1-1;

		nr= bl->nr;

		if (bl->poly== -1) { /* check its not cyclic */
			/* skip the first point */
			/* bevp0= bevp1; */
			bevp1= bevp2;
			bevp2++;
			nr--;

			bevp0= bevp1;
			bevp1= bevp2;
			bevp2++;
			nr--;

		}

		copy_qt_qt(bevp0_quat, bevp0->quat);

		while (nr--) {
			/* interpolate quats */
			float zaxis[3] = {0,0,1}, cross[3], q2[4];
			interp_qt_qtqt(q, bevp0_quat, bevp2->quat, 0.5);
			normalize_qt(q);

			mul_qt_v3(q, zaxis);
			cross_v3_v3v3(cross, zaxis, bevp1->dir);
			axis_angle_to_quat(q2, cross, angle_normalized_v3v3(zaxis, bevp1->dir));
			normalize_qt(q2);

			copy_qt_qt(bevp0_quat, bevp1->quat);
			mul_qt_qtqt(q, q2, q);
			interp_qt_qtqt(bevp1->quat, bevp1->quat, q, 0.5);
			normalize_qt(bevp1->quat);


			/* bevp0= bevp1; */ /* UNUSED */
			bevp1= bevp2;
			bevp2++;
		}
	}
}

static void make_bevel_list_3D_zup(BevList *bl)
{
	BevPoint *bevp2, *bevp1, *bevp0; /* standard for all make_bevel_list_3D_* funcs */
	int nr;

	bevp2= (BevPoint *)(bl+1);
	bevp1= bevp2+(bl->nr-1);
	bevp0= bevp1-1;

	nr= bl->nr;
	while (nr--) {
		/* totally simple */
		bisect_v3_v3v3v3(bevp1->dir, bevp0->vec, bevp1->vec, bevp2->vec);
		vec_to_quat( bevp1->quat,bevp1->dir, 5, 1);

		bevp0= bevp1;
		bevp1= bevp2;
		bevp2++;
	}
}

static void make_bevel_list_3D_minimum_twist(BevList *bl)
{
	BevPoint *bevp2, *bevp1, *bevp0; /* standard for all make_bevel_list_3D_* funcs */
	int nr;
	float q[4];

	bevel_list_calc_bisect(bl);

	bevp2= (BevPoint *)(bl+1);
	bevp1= bevp2+(bl->nr-1);
	bevp0= bevp1-1;

	nr= bl->nr;
	while (nr--) {

		if (nr+4 > bl->nr) { /* first time and second time, otherwise first point adjusts last */
			vec_to_quat( bevp1->quat,bevp1->dir, 5, 1);
		}
		else {
			float angle= angle_normalized_v3v3(bevp0->dir, bevp1->dir);

			if (angle > 0.0f) { /* otherwise we can keep as is */
				float cross_tmp[3];
				cross_v3_v3v3(cross_tmp, bevp0->dir, bevp1->dir);
				axis_angle_to_quat(q, cross_tmp, angle);
				mul_qt_qtqt(bevp1->quat, q, bevp0->quat);
			}
			else {
				copy_qt_qt(bevp1->quat, bevp0->quat);
			}
		}

		bevp0= bevp1;
		bevp1= bevp2;
		bevp2++;
	}

	if (bl->poly != -1) { /* check for cyclic */

		/* Need to correct for the start/end points not matching
		 * do this by calculating the tilt angle difference, then apply
		 * the rotation gradually over the entire curve
		 *
		 * note that the split is between last and second last, rather than first/last as youd expect.
		 *
		 * real order is like this
		 * 0,1,2,3,4 --> 1,2,3,4,0
		 *
		 * this is why we compare last with second last
		 * */
		float vec_1[3]= {0,1,0}, vec_2[3]= {0,1,0}, angle, ang_fac, cross_tmp[3];

		BevPoint *bevp_first;
		BevPoint *bevp_last;


		bevp_first= (BevPoint *)(bl+1);
		bevp_first+= bl->nr-1;
		bevp_last = bevp_first;
		bevp_last--;

		/* quats and vec's are normalized, should not need to re-normalize */
		mul_qt_v3(bevp_first->quat, vec_1);
		mul_qt_v3(bevp_last->quat, vec_2);
		normalize_v3(vec_1);
		normalize_v3(vec_2);

		/* align the vector, can avoid this and it looks 98% OK but
		 * better to align the angle quat roll's before comparing */
		{
			cross_v3_v3v3(cross_tmp, bevp_last->dir, bevp_first->dir);
			angle = angle_normalized_v3v3(bevp_first->dir, bevp_last->dir);
			axis_angle_to_quat(q, cross_tmp, angle);
			mul_qt_v3(q, vec_2);
		}

		angle= angle_normalized_v3v3(vec_1, vec_2);

		/* flip rotation if needs be */
		cross_v3_v3v3(cross_tmp, vec_1, vec_2);
		normalize_v3(cross_tmp);
		if (angle_normalized_v3v3(bevp_first->dir, cross_tmp) < DEG2RADF(90.0f))
			angle = -angle;

		bevp2= (BevPoint *)(bl+1);
		bevp1= bevp2+(bl->nr-1);
		bevp0= bevp1-1;

		nr= bl->nr;
		while (nr--) {
			ang_fac= angle * (1.0f-((float)nr/bl->nr)); /* also works */

			axis_angle_to_quat(q, bevp1->dir, ang_fac);
			mul_qt_qtqt(bevp1->quat, q, bevp1->quat);

			bevp0= bevp1;
			bevp1= bevp2;
			bevp2++;
		}
	}
}

static void make_bevel_list_3D_tangent(BevList *bl)
{
	BevPoint *bevp2, *bevp1, *bevp0; /* standard for all make_bevel_list_3D_* funcs */
	int nr;

	float bevp0_tan[3], cross_tmp[3];

	bevel_list_calc_bisect(bl);
	if (bl->poly== -1) /* check its not cyclic */
		bevel_list_cyclic_fix_3D(bl); // XXX - run this now so tangents will be right before doing the flipping
	bevel_list_flip_tangents(bl);

	/* correct the tangents */
	bevp2= (BevPoint *)(bl+1);
	bevp1= bevp2+(bl->nr-1);
	bevp0= bevp1-1;

	nr= bl->nr;
	while (nr--) {

		cross_v3_v3v3(cross_tmp, bevp1->tan, bevp1->dir);
		cross_v3_v3v3(bevp1->tan, cross_tmp, bevp1->dir);
		normalize_v3(bevp1->tan);

		bevp0= bevp1;
		bevp1= bevp2;
		bevp2++;
	}


	/* now for the real twist calc */
	bevp2= (BevPoint *)(bl+1);
	bevp1= bevp2+(bl->nr-1);
	bevp0= bevp1-1;

	copy_v3_v3(bevp0_tan, bevp0->tan);

	nr= bl->nr;
	while (nr--) {

		/* make perpendicular, modify tan in place, is ok */
		float cross_tmp[3];
		float zero[3] = {0,0,0};

		cross_v3_v3v3(cross_tmp, bevp1->tan, bevp1->dir);
		normalize_v3(cross_tmp);
		tri_to_quat( bevp1->quat,zero, cross_tmp, bevp1->tan); /* XXX - could be faster */

		/* bevp0= bevp1; */ /* UNUSED */
		bevp1= bevp2;
		bevp2++;
	}
}

static void make_bevel_list_3D(BevList *bl, int smooth_iter, int twist_mode)
{
	switch (twist_mode) {
	case CU_TWIST_TANGENT:
		make_bevel_list_3D_tangent(bl);
		break;
	case CU_TWIST_MINIMUM:
		make_bevel_list_3D_minimum_twist(bl);
		break;
	default: /* CU_TWIST_Z_UP default, pre 2.49c */
		make_bevel_list_3D_zup(bl);
	}

	if (bl->poly== -1) /* check its not cyclic */
		bevel_list_cyclic_fix_3D(bl);

	if (smooth_iter)
		bevel_list_smooth(bl, smooth_iter);

	bevel_list_apply_tilt(bl);
}



/* only for 2 points */
static void make_bevel_list_segment_3D(BevList *bl)
{
	float q[4];

	BevPoint *bevp2= (BevPoint *)(bl+1);
	BevPoint *bevp1= bevp2+1;

	/* simple quat/dir */
	sub_v3_v3v3(bevp1->dir, bevp1->vec, bevp2->vec);
	normalize_v3(bevp1->dir);

	vec_to_quat( bevp1->quat,bevp1->dir, 5, 1);

	axis_angle_to_quat(q, bevp1->dir, bevp1->alfa);
	mul_qt_qtqt(bevp1->quat, q, bevp1->quat);
	normalize_qt(bevp1->quat);
	copy_v3_v3(bevp2->dir, bevp1->dir);
	copy_qt_qt(bevp2->quat, bevp1->quat);
}



void BKE_curve_bevelList_make(Object *ob)
{
	/*
	 * - convert all curves to polys, with indication of resol and flags for double-vertices
	 * - possibly; do a smart vertice removal (in case Nurb)
	 * - separate in individual blicks with BoundBox
	 * - AutoHole detection
	 */
	Curve *cu;
	Nurb *nu;
	BezTriple *bezt, *prevbezt;
	BPoint *bp;
	BevList *bl, *blnew, *blnext;
	BevPoint *bevp, *bevp2, *bevp1 = NULL, *bevp0;
	float min, inp, x1, x2, y1, y2;
	struct bevelsort *sortdata, *sd, *sd1;
	int a, b, nr, poly, resolu = 0, len = 0;
	int do_tilt, do_radius, do_weight;
	
	/* this function needs an object, because of tflag and upflag */
	cu= ob->data;

	/* do we need to calculate the radius for each point? */
	/* do_radius = (cu->bevobj || cu->taperobj || (cu->flag & CU_FRONT) || (cu->flag & CU_BACK)) ? 0 : 1; */
	
	/* STEP 1: MAKE POLYS  */

	BLI_freelistN(&(cu->bev));
	if (cu->editnurb && ob->type!=OB_FONT) {
		ListBase *nurbs= BKE_curve_editNurbs_get(cu);
		nu = nurbs->first;
	}
	else {
		nu = cu->nurb.first;
	}
	
	while (nu) {
		
		/* check if we will calculate tilt data */
		do_tilt = CU_DO_TILT(cu, nu);
		do_radius = CU_DO_RADIUS(cu, nu); /* normal display uses the radius, better just to calculate them */
		do_weight = 1;
		
		/* check we are a single point? also check we are not a surface and that the orderu is sane,
		 * enforced in the UI but can go wrong possibly */
		if (!BKE_nurb_check_valid_u(nu)) {
			bl= MEM_callocN(sizeof(BevList)+1*sizeof(BevPoint), "makeBevelList1");
			BLI_addtail(&(cu->bev), bl);
			bl->nr= 0;
		}
		else {
			if (G.rendering && cu->resolu_ren!=0) 
				resolu= cu->resolu_ren;
			else
				resolu= nu->resolu;
			
			if (nu->type == CU_POLY) {
				len= nu->pntsu;
				bl= MEM_callocN(sizeof(BevList)+len*sizeof(BevPoint), "makeBevelList2");
				BLI_addtail(&(cu->bev), bl);
	
				if (nu->flagu & CU_NURB_CYCLIC) bl->poly= 0;
				else bl->poly= -1;
				bl->nr= len;
				bl->dupe_nr= 0;
				bevp= (BevPoint *)(bl+1);
				bp= nu->bp;
	
				while (len--) {
					copy_v3_v3(bevp->vec, bp->vec);
					bevp->alfa= bp->alfa;
					bevp->radius= bp->radius;
					bevp->weight= bp->weight;
					bevp->split_tag= TRUE;
					bevp++;
					bp++;
				}
			}
			else if (nu->type == CU_BEZIER) {
	
				len= resolu*(nu->pntsu+ (nu->flagu & CU_NURB_CYCLIC) -1)+1;	/* in case last point is not cyclic */
				bl= MEM_callocN(sizeof(BevList)+len*sizeof(BevPoint), "makeBevelBPoints");
				BLI_addtail(&(cu->bev), bl);
	
				if (nu->flagu & CU_NURB_CYCLIC) bl->poly= 0;
				else bl->poly= -1;
				bevp= (BevPoint *)(bl+1);
	
				a= nu->pntsu-1;
				bezt= nu->bezt;
				if (nu->flagu & CU_NURB_CYCLIC) {
					a++;
					prevbezt= nu->bezt+(nu->pntsu-1);
				}
				else {
					prevbezt= bezt;
					bezt++;
				}
				
				while (a--) {
					if (prevbezt->h2==HD_VECT && bezt->h1==HD_VECT) {

						copy_v3_v3(bevp->vec, prevbezt->vec[1]);
						bevp->alfa= prevbezt->alfa;
						bevp->radius= prevbezt->radius;
						bevp->weight= prevbezt->weight;
						bevp->split_tag= TRUE;
						bevp->dupe_tag= FALSE;
						bevp++;
						bl->nr++;
						bl->dupe_nr= 1;
					}
					else {
						/* always do all three, to prevent data hanging around */
						int j;
						
						/* BevPoint must stay aligned to 4 so sizeof(BevPoint)/sizeof(float) works */
						for (j=0; j<3; j++) {
							BKE_curve_forward_diff_bezier(	prevbezt->vec[1][j],	prevbezt->vec[2][j],
													bezt->vec[0][j],		bezt->vec[1][j],
													&(bevp->vec[j]), resolu, sizeof(BevPoint));
						}
						
						/* if both arrays are NULL do nothiong */
						alfa_bezpart(	prevbezt, bezt, nu,
										 do_tilt	? &bevp->alfa : NULL,
										 do_radius	? &bevp->radius : NULL,
										 do_weight	? &bevp->weight : NULL,
										 resolu, sizeof(BevPoint));

						
						if (cu->twist_mode==CU_TWIST_TANGENT) {
							forward_diff_bezier_cotangent(
													prevbezt->vec[1],	prevbezt->vec[2],
													bezt->vec[0],		bezt->vec[1],
													bevp->tan, resolu, sizeof(BevPoint));
						}

						/* indicate with handlecodes double points */
						if (prevbezt->h1==prevbezt->h2) {
							if (prevbezt->h1==0 || prevbezt->h1==HD_VECT) bevp->split_tag= TRUE;
						}
						else {
							if (prevbezt->h1==0 || prevbezt->h1==HD_VECT) bevp->split_tag= TRUE;
							else if (prevbezt->h2==0 || prevbezt->h2==HD_VECT) bevp->split_tag= TRUE;
						}
						bl->nr+= resolu;
						bevp+= resolu;
					}
					prevbezt= bezt;
					bezt++;
				}
				
				if ((nu->flagu & CU_NURB_CYCLIC)==0) {	    /* not cyclic: endpoint */
					copy_v3_v3(bevp->vec, prevbezt->vec[1]);
					bevp->alfa= prevbezt->alfa;
					bevp->radius= prevbezt->radius;
					bevp->weight= prevbezt->weight;
					bl->nr++;
				}
			}
			else if (nu->type == CU_NURBS) {
				if (nu->pntsv==1) {
					len= (resolu*SEGMENTSU(nu));
					
					bl= MEM_callocN(sizeof(BevList)+len*sizeof(BevPoint), "makeBevelList3");
					BLI_addtail(&(cu->bev), bl);
					bl->nr= len;
					bl->dupe_nr= 0;
					if (nu->flagu & CU_NURB_CYCLIC) bl->poly= 0;
					else bl->poly= -1;
					bevp= (BevPoint *)(bl+1);
					
					BKE_nurb_makeCurve(	nu, &bevp->vec[0],
									do_tilt		? &bevp->alfa : NULL,
									do_radius	? &bevp->radius : NULL,
									do_weight	? &bevp->weight : NULL,
									resolu, sizeof(BevPoint));
				}
			}
		}
		nu= nu->next;
	}

	/* STEP 2: DOUBLE POINTS AND AUTOMATIC RESOLUTION, REDUCE DATABLOCKS */
	bl= cu->bev.first;
	while (bl) {
		if (bl->nr) { /* null bevel items come from single points */
			nr= bl->nr;
			bevp1= (BevPoint *)(bl+1);
			bevp0= bevp1+(nr-1);
			nr--;
			while (nr--) {
				if ( fabs(bevp0->vec[0]-bevp1->vec[0])<0.00001 ) {
					if ( fabs(bevp0->vec[1]-bevp1->vec[1])<0.00001 ) {
						if ( fabs(bevp0->vec[2]-bevp1->vec[2])<0.00001 ) {
							bevp0->dupe_tag= TRUE;
							bl->dupe_nr++;
						}
					}
				}
				bevp0= bevp1;
				bevp1++;
			}
		}
		bl= bl->next;
	}
	bl= cu->bev.first;
	while (bl) {
		blnext= bl->next;
		if (bl->nr && bl->dupe_nr) {
			nr= bl->nr- bl->dupe_nr+1;	/* +1 because vectorbezier sets flag too */
			blnew= MEM_mallocN(sizeof(BevList)+nr*sizeof(BevPoint), "makeBevelList4");
			memcpy(blnew, bl, sizeof(BevList));
			blnew->nr= 0;
			BLI_remlink(&(cu->bev), bl);
			BLI_insertlinkbefore(&(cu->bev),blnext,blnew);	/* to make sure bevlijst is tuned with nurblist */
			bevp0= (BevPoint *)(bl+1);
			bevp1= (BevPoint *)(blnew+1);
			nr= bl->nr;
			while (nr--) {
				if (bevp0->dupe_tag==0) {
					memcpy(bevp1, bevp0, sizeof(BevPoint));
					bevp1++;
					blnew->nr++;
				}
				bevp0++;
			}
			MEM_freeN(bl);
			blnew->dupe_nr= 0;
		}
		bl= blnext;
	}

	/* STEP 3: POLYS COUNT AND AUTOHOLE */
	bl= cu->bev.first;
	poly= 0;
	while (bl) {
		if (bl->nr && bl->poly>=0) {
			poly++;
			bl->poly= poly;
			bl->hole= 0;
		}
		bl= bl->next;
	}
	

	/* find extreme left points, also test (turning) direction */
	if (poly>0) {
		sd= sortdata= MEM_mallocN(sizeof(struct bevelsort)*poly, "makeBevelList5");
		bl= cu->bev.first;
		while (bl) {
			if (bl->poly>0) {

				min= 300000.0;
				bevp= (BevPoint *)(bl+1);
				nr= bl->nr;
				while (nr--) {
					if (min>bevp->vec[0]) {
						min= bevp->vec[0];
						bevp1= bevp;
					}
					bevp++;
				}
				sd->bl= bl;
				sd->left= min;

				bevp= (BevPoint *)(bl+1);
				if (bevp1== bevp) bevp0= bevp+ (bl->nr-1);
				else bevp0= bevp1-1;
				bevp= bevp+ (bl->nr-1);
				if (bevp1== bevp) bevp2= (BevPoint *)(bl+1);
				else bevp2= bevp1+1;

				inp= (bevp1->vec[0]- bevp0->vec[0]) * (bevp0->vec[1]- bevp2->vec[1]) + (bevp0->vec[1]- bevp1->vec[1]) * (bevp0->vec[0]- bevp2->vec[0]);

				if (inp > 0.0f) sd->dir= 1;
				else sd->dir= 0;

				sd++;
			}

			bl= bl->next;
		}
		qsort(sortdata,poly,sizeof(struct bevelsort), vergxcobev);

		sd= sortdata+1;
		for (a=1; a<poly; a++, sd++) {
			bl= sd->bl;	    /* is bl a hole? */
			sd1= sortdata+ (a-1);
			for (b=a-1; b>=0; b--, sd1--) {	/* all polys to the left */
				if (bevelinside(sd1->bl, bl)) {
					bl->hole= 1- sd1->bl->hole;
					break;
				}
			}
		}

		/* turning direction */
		if ((cu->flag & CU_3D)==0) {
			sd= sortdata;
			for (a=0; a<poly; a++, sd++) {
				if (sd->bl->hole==sd->dir) {
					bl= sd->bl;
					bevp1= (BevPoint *)(bl+1);
					bevp2= bevp1+ (bl->nr-1);
					nr= bl->nr/2;
					while (nr--) {
						SWAP(BevPoint, *bevp1, *bevp2);
						bevp1++;
						bevp2--;
					}
				}
			}
		}
		MEM_freeN(sortdata);
	}

	/* STEP 4: 2D-COSINES or 3D ORIENTATION */
	if ((cu->flag & CU_3D)==0) {
		/* note: bevp->dir and bevp->quat are not needed for beveling but are
		 * used when making a path from a 2D curve, therefor they need to be set - Campbell */
		bl= cu->bev.first;
		while (bl) {

			if (bl->nr < 2) {
				/* do nothing */
			}
			else if (bl->nr==2) {	/* 2 pnt, treat separate */
				bevp2= (BevPoint *)(bl+1);
				bevp1= bevp2+1;

				x1= bevp1->vec[0]- bevp2->vec[0];
				y1= bevp1->vec[1]- bevp2->vec[1];

				calc_bevel_sin_cos(x1, y1, -x1, -y1, &(bevp1->sina), &(bevp1->cosa));
				bevp2->sina= bevp1->sina;
				bevp2->cosa= bevp1->cosa;

				/* fill in dir & quat */
				make_bevel_list_segment_3D(bl);
			}
			else {
				bevp2= (BevPoint *)(bl+1);
				bevp1= bevp2+(bl->nr-1);
				bevp0= bevp1-1;

				nr= bl->nr;
				while (nr--) {
					x1= bevp1->vec[0]- bevp0->vec[0];
					x2= bevp1->vec[0]- bevp2->vec[0];
					y1= bevp1->vec[1]- bevp0->vec[1];
					y2= bevp1->vec[1]- bevp2->vec[1];

					calc_bevel_sin_cos(x1, y1, x2, y2, &(bevp1->sina), &(bevp1->cosa));

					/* from: make_bevel_list_3D_zup, could call but avoid a second loop.
					 * no need for tricky tilt calculation as with 3D curves */
					bisect_v3_v3v3v3(bevp1->dir, bevp0->vec, bevp1->vec, bevp2->vec);
					vec_to_quat( bevp1->quat,bevp1->dir, 5, 1);
					/* done with inline make_bevel_list_3D_zup */

					bevp0= bevp1;
					bevp1= bevp2;
					bevp2++;
				}

				/* correct non-cyclic cases */
				if (bl->poly== -1) {
					bevp= (BevPoint *)(bl+1);
					bevp1= bevp+1;
					bevp->sina= bevp1->sina;
					bevp->cosa= bevp1->cosa;
					bevp= (BevPoint *)(bl+1);
					bevp+= (bl->nr-1);
					bevp1= bevp-1;
					bevp->sina= bevp1->sina;
					bevp->cosa= bevp1->cosa;

					/* correct for the dir/quat, see above why its needed */
					bevel_list_cyclic_fix_3D(bl);
				}
			}
			bl= bl->next;
		}
	}
	else { /* 3D Curves */
		bl= cu->bev.first;
		while (bl) {

			if (bl->nr < 2) {
				/* do nothing */
			}
			else if (bl->nr==2) {	/* 2 pnt, treat separate */
				make_bevel_list_segment_3D(bl);
			}
			else {
				make_bevel_list_3D(bl, (int)(resolu*cu->twist_smooth), cu->twist_mode);
			}
			bl= bl->next;
		}
	}
}

/* ****************** HANDLES ************** */

/*
 *   handlecodes:
 *		0: nothing,  1:auto,  2:vector,  3:aligned
 */

/* mode: is not zero when FCurve, is 2 when forced horizontal for autohandles */
static void calchandleNurb_intern(BezTriple *bezt, BezTriple *prev, BezTriple *next, int mode, int skip_align)
{
	float *p1,*p2,*p3, pt[3];
	float dvec_a[3], dvec_b[3];
	float len, len_a, len_b;
	const float eps= 1e-5;

	if (bezt->h1==0 && bezt->h2==0) {
		return;
	}

	p2= bezt->vec[1];

	if (prev==NULL) {
		p3= next->vec[1];
		pt[0]= 2.0f*p2[0] - p3[0];
		pt[1]= 2.0f*p2[1] - p3[1];
		pt[2]= 2.0f*p2[2] - p3[2];
		p1= pt;
	}
	else {
		p1= prev->vec[1];
	}

	if (next==NULL) {
		pt[0]= 2.0f*p2[0] - p1[0];
		pt[1]= 2.0f*p2[1] - p1[1];
		pt[2]= 2.0f*p2[2] - p1[2];
		p3= pt;
	}
	else {
		p3= next->vec[1];
	}

	sub_v3_v3v3(dvec_a, p2, p1);
	sub_v3_v3v3(dvec_b, p3, p2);

	if (mode != 0) {
		len_a= dvec_a[0];
		len_b= dvec_b[0];
	}
	else {
		len_a= len_v3(dvec_a);
		len_b= len_v3(dvec_b);
	}

	if (len_a==0.0f) len_a=1.0f;
	if (len_b==0.0f) len_b=1.0f;


	if (ELEM(bezt->h1,HD_AUTO,HD_AUTO_ANIM) || ELEM(bezt->h2,HD_AUTO,HD_AUTO_ANIM)) {    /* auto */
		float tvec[3];
		tvec[0]= dvec_b[0]/len_b + dvec_a[0]/len_a;
		tvec[1]= dvec_b[1]/len_b + dvec_a[1]/len_a;
		tvec[2]= dvec_b[2]/len_b + dvec_a[2]/len_a;
		len= len_v3(tvec) * 2.5614f;

		if (len!=0.0f) {
			int leftviolate=0, rightviolate=0;	/* for mode==2 */
			
			if (len_a>5.0f*len_b) len_a= 5.0f*len_b;
			if (len_b>5.0f*len_a) len_b= 5.0f*len_a;
			
			if (ELEM(bezt->h1,HD_AUTO,HD_AUTO_ANIM)) {
				len_a/=len;
				madd_v3_v3v3fl(p2-3, p2, tvec, -len_a);
				
				if ((bezt->h1==HD_AUTO_ANIM) && next && prev) { /* keep horizontal if extrema */
					float ydiff1= prev->vec[1][1] - bezt->vec[1][1];
					float ydiff2= next->vec[1][1] - bezt->vec[1][1];
					if ( (ydiff1 <= 0.0f && ydiff2 <= 0.0f) || (ydiff1 >= 0.0f && ydiff2 >= 0.0f) ) {
						bezt->vec[0][1]= bezt->vec[1][1];
					}
					else { /* handles should not be beyond y coord of two others */
						if (ydiff1 <= 0.0f) {
							if (prev->vec[1][1] > bezt->vec[0][1]) {
								bezt->vec[0][1]= prev->vec[1][1]; 
								leftviolate= 1;
							}
						}
						else {
							if (prev->vec[1][1] < bezt->vec[0][1]) {
								bezt->vec[0][1]= prev->vec[1][1]; 
								leftviolate= 1;
							}
						}
					}
				}
			}
			if (ELEM(bezt->h2,HD_AUTO,HD_AUTO_ANIM)) {
				len_b/=len;
				madd_v3_v3v3fl(p2+3, p2, tvec,  len_b);
				
				if ((bezt->h2==HD_AUTO_ANIM) && next && prev) { /* keep horizontal if extrema */
					float ydiff1= prev->vec[1][1] - bezt->vec[1][1];
					float ydiff2= next->vec[1][1] - bezt->vec[1][1];
					if ( (ydiff1 <= 0.0f && ydiff2 <= 0.0f) || (ydiff1 >= 0.0f && ydiff2 >= 0.0f) ) {
						bezt->vec[2][1]= bezt->vec[1][1];
					}
					else { /* andles should not be beyond y coord of two others */
						if (ydiff1 <= 0.0f) {
							if (next->vec[1][1] < bezt->vec[2][1]) {
								bezt->vec[2][1]= next->vec[1][1]; 
								rightviolate= 1;
							}
						}
						else {
							if (next->vec[1][1] > bezt->vec[2][1]) {
								bezt->vec[2][1]= next->vec[1][1]; 
								rightviolate= 1;
							}
						}
					}
				}
			}
			if (leftviolate || rightviolate) { /* align left handle */
				float h1[3], h2[3];
				float dot;
				
				sub_v3_v3v3(h1, p2-3, p2);
				sub_v3_v3v3(h2, p2, p2+3);

				len_a= normalize_v3(h1);
				len_b= normalize_v3(h2);

				dot= dot_v3v3(h1, h2);

				if (leftviolate) {
					mul_v3_fl(h1, dot * len_b);
					sub_v3_v3v3(p2+3, p2, h1);
				}
				else {
					mul_v3_fl(h2, dot * len_a);
					add_v3_v3v3(p2-3, p2, h2);
				}
			}
			
		}
	}

	if (bezt->h1==HD_VECT) {	/* vector */
		madd_v3_v3v3fl(p2-3, p2, dvec_a, -1.0f/3.0f);
	}
	if (bezt->h2==HD_VECT) {
		madd_v3_v3v3fl(p2+3, p2, dvec_b,  1.0f/3.0f);
	}

	if (skip_align) {
		/* handles need to be updated during animation and applying stuff like hooks,
		 * but in such situatios it's quite difficult to distinguish in which order
		 * align handles should be aligned so skip them for now */
		return;
	}

	len_b= len_v3v3(p2, p2+3);
	len_a= len_v3v3(p2, p2-3);
	if (len_a==0.0f) len_a= 1.0f;
	if (len_b==0.0f) len_b= 1.0f;

	if (bezt->f1 & SELECT) { /* order of calculation */
		if (bezt->h2==HD_ALIGN) { /* aligned */
			if (len_a>eps) {
				len= len_b/len_a;
				p2[3]= p2[0]+len*(p2[0] - p2[-3]);
				p2[4]= p2[1]+len*(p2[1] - p2[-2]);
				p2[5]= p2[2]+len*(p2[2] - p2[-1]);
			}
		}
		if (bezt->h1==HD_ALIGN) {
			if (len_b>eps) {
				len= len_a/len_b;
				p2[-3]= p2[0]+len*(p2[0] - p2[3]);
				p2[-2]= p2[1]+len*(p2[1] - p2[4]);
				p2[-1]= p2[2]+len*(p2[2] - p2[5]);
			}
		}
	}
	else {
		if (bezt->h1==HD_ALIGN) {
			if (len_b>eps) {
				len= len_a/len_b;
				p2[-3]= p2[0]+len*(p2[0] - p2[3]);
				p2[-2]= p2[1]+len*(p2[1] - p2[4]);
				p2[-1]= p2[2]+len*(p2[2] - p2[5]);
			}
		}
		if (bezt->h2==HD_ALIGN) {	/* aligned */
			if (len_a>eps) {
				len= len_b/len_a;
				p2[3]= p2[0]+len*(p2[0] - p2[-3]);
				p2[4]= p2[1]+len*(p2[1] - p2[-2]);
				p2[5]= p2[2]+len*(p2[2] - p2[-1]);
			}
		}
	}
}

static void calchandlesNurb_intern(Nurb *nu, int skip_align)
{
	BezTriple *bezt, *prev, *next;
	short a;

	if (nu->type != CU_BEZIER) return;
	if (nu->pntsu<2) return;
	
	a= nu->pntsu;
	bezt= nu->bezt;
	if (nu->flagu & CU_NURB_CYCLIC) prev= bezt+(a-1);
	else prev= NULL;
	next= bezt+1;

	while (a--) {
		calchandleNurb_intern(bezt, prev, next, 0, skip_align);
		prev= bezt;
		if (a==1) {
			if (nu->flagu & CU_NURB_CYCLIC) next= nu->bezt;
			else next= NULL;
		}
		else next++;

		bezt++;
	}
}

void BKE_nurb_handle_calc(BezTriple *bezt, BezTriple *prev, BezTriple *next, int mode)
{
	calchandleNurb_intern(bezt, prev, next, mode, FALSE);
}

void BKE_nurb_handles_calc(Nurb *nu) /* first, if needed, set handle flags */
{
	calchandlesNurb_intern(nu, FALSE);
}


void BKE_nurb_handles_test(Nurb *nu)
{
	/* use when something has changed with handles.
	 * it treats all BezTriples with the following rules:
	 * PHASE 1: do types have to be altered?
	 *    Auto handles: become aligned when selection status is NOT(000 || 111)
	 *    Vector handles: become 'nothing' when (one half selected AND other not)
	 * PHASE 2: recalculate handles
	 */
	BezTriple *bezt;
	short flag, a;

	if (nu->type != CU_BEZIER) return;

	bezt= nu->bezt;
	a= nu->pntsu;
	while (a--) {
		flag= 0;
		if (bezt->f1 & SELECT) flag++;
		if (bezt->f2 & SELECT) flag += 2;
		if (bezt->f3 & SELECT) flag += 4;
		
		if ( !(flag==0 || flag==7) ) {
			if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM)) {   /* auto */
				bezt->h1= HD_ALIGN;
			}
			if (ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM)) {   /* auto */
				bezt->h2= HD_ALIGN;
			}
			
			if (bezt->h1==HD_VECT) {   /* vector */
				if (flag < 4) bezt->h1= 0;
			}
			if (bezt->h2==HD_VECT) {   /* vector */
				if ( flag > 3) bezt->h2= 0;
			}
		}
		bezt++;
	}
	
	BKE_nurb_handles_calc(nu);
}

void BKE_nurb_handles_autocalc(Nurb *nu, int flag)
{
	/* checks handle coordinates and calculates type */
	
	BezTriple *bezt2, *bezt1, *bezt0;
	int i, align, leftsmall, rightsmall;

	if (nu==NULL || nu->bezt==NULL) return;
	
	bezt2 = nu->bezt;
	bezt1 = bezt2 + (nu->pntsu-1);
	bezt0 = bezt1 - 1;
	i = nu->pntsu;

	while (i--) {
		
		align= leftsmall= rightsmall= 0;
		
		/* left handle: */
		if (flag==0 || (bezt1->f1 & flag) ) {
			bezt1->h1= 0;
			/* distance too short: vectorhandle */
			if ( len_v3v3( bezt1->vec[1], bezt0->vec[1] ) < 0.0001f) {
				bezt1->h1= HD_VECT;
				leftsmall= 1;
			}
			else {
				/* aligned handle? */
				if (dist_to_line_v2(bezt1->vec[1], bezt1->vec[0], bezt1->vec[2]) < 0.0001f) {
					align= 1;
					bezt1->h1= HD_ALIGN;
				}
				/* or vector handle? */
				if (dist_to_line_v2(bezt1->vec[0], bezt1->vec[1], bezt0->vec[1]) < 0.0001f)
					bezt1->h1= HD_VECT;
				
			}
		}
		/* right handle: */
		if (flag==0 || (bezt1->f3 & flag) ) {
			bezt1->h2= 0;
			/* distance too short: vectorhandle */
			if ( len_v3v3( bezt1->vec[1], bezt2->vec[1] ) < 0.0001f) {
				bezt1->h2= HD_VECT;
				rightsmall= 1;
			}
			else {
				/* aligned handle? */
				if (align) bezt1->h2= HD_ALIGN;

				/* or vector handle? */
				if (dist_to_line_v2(bezt1->vec[2], bezt1->vec[1], bezt2->vec[1]) < 0.0001f)
					bezt1->h2= HD_VECT;
				
			}
		}
		if (leftsmall && bezt1->h2==HD_ALIGN) bezt1->h2= 0;
		if (rightsmall && bezt1->h1==HD_ALIGN) bezt1->h1= 0;
		
		/* undesired combination: */
		if (bezt1->h1==HD_ALIGN && bezt1->h2==HD_VECT) bezt1->h1= 0;
		if (bezt1->h2==HD_ALIGN && bezt1->h1==HD_VECT) bezt1->h2= 0;
		
		bezt0= bezt1;
		bezt1= bezt2;
		bezt2++;
	}

	BKE_nurb_handles_calc(nu);
}

void BKE_nurbList_handles_autocalc(ListBase *editnurb, int flag)
{
	Nurb *nu;
	
	nu= editnurb->first;
	while (nu) {
		BKE_nurb_handles_autocalc(nu, flag);
		nu= nu->next;
	}
}

void BKE_nurbList_handles_set(ListBase *editnurb, short code)
{
	/* code==1: set autohandle */
	/* code==2: set vectorhandle */
	/* code==3 (HD_ALIGN) it toggle, vectorhandles become HD_FREE */
	/* code==4: sets icu flag to become IPO_AUTO_HORIZ, horizontal extremes on auto-handles */
	/* code==5: Set align, like 3 but no toggle */
	/* code==6: Clear align, like 3 but no toggle */
	Nurb *nu;
	BezTriple *bezt;
	short a, ok=0;

	if (code==1 || code==2) {
		nu= editnurb->first;
		while (nu) {
			if (nu->type == CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while (a--) {
					if ((bezt->f1 & SELECT) || (bezt->f3 & SELECT)) {
						if (bezt->f1 & SELECT) bezt->h1= code;
						if (bezt->f3 & SELECT) bezt->h2= code;
						if (bezt->h1!=bezt->h2) {
							if (ELEM(bezt->h1, HD_ALIGN, HD_AUTO)) bezt->h1 = HD_FREE;
							if (ELEM(bezt->h2, HD_ALIGN, HD_AUTO)) bezt->h2 = HD_FREE;
						}
					}
					bezt++;
				}
				BKE_nurb_handles_calc(nu);
			}
			nu= nu->next;
		}
	}
	else {
		/* there is 1 handle not FREE: FREE it all, else make ALIGNED  */
		
		nu= editnurb->first;
		if (code == 5) {
			ok = HD_ALIGN;
		}
		else if (code == 6) {
			ok = HD_FREE;
		}
		else {
			/* Toggle */
			while (nu) {
				if (nu->type == CU_BEZIER) {
					bezt= nu->bezt;
					a= nu->pntsu;
					while (a--) {
						if ((bezt->f1 & SELECT) && bezt->h1) ok= 1;
						if ((bezt->f3 & SELECT) && bezt->h2) ok= 1;
						if (ok) break;
						bezt++;
					}
				}
				nu= nu->next;
			}
			if (ok) ok= HD_FREE;
			else ok= HD_ALIGN;
		}
		nu= editnurb->first;
		while (nu) {
			if (nu->type == CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while (a--) {
					if (bezt->f1 & SELECT) bezt->h1= ok;
					if (bezt->f3 & SELECT) bezt->h2= ok;
	
					bezt++;
				}
				BKE_nurb_handles_calc(nu);
			}
			nu= nu->next;
		}
	}
}

static void swapdata(void *adr1, void *adr2, int len)
{

	if (len<=0) return;

	if (len<65) {
		char adr[64];

		memcpy(adr, adr1, len);
		memcpy(adr1, adr2, len);
		memcpy(adr2, adr, len);
	}
	else {
		char *adr;

		adr= (char *)MEM_mallocN(len, "curve swap");
		memcpy(adr, adr1, len);
		memcpy(adr1, adr2, len);
		memcpy(adr2, adr, len);
		MEM_freeN(adr);
	}
}

void BKE_nurb_direction_switch(Nurb *nu)
{
	BezTriple *bezt1, *bezt2;
	BPoint *bp1, *bp2;
	float *fp1, *fp2, *tempf;
	int a, b;

	if (nu->pntsu==1 && nu->pntsv==1) return;

	if (nu->type == CU_BEZIER) {
		a= nu->pntsu;
		bezt1= nu->bezt;
		bezt2= bezt1+(a-1);
		if (a & 1) a+= 1;	/* if odd, also swap middle content */
		a/= 2;
		while (a>0) {
			if (bezt1!=bezt2) SWAP(BezTriple, *bezt1, *bezt2);

			swapdata(bezt1->vec[0], bezt1->vec[2], 12);
			if (bezt1!=bezt2) swapdata(bezt2->vec[0], bezt2->vec[2], 12);

			SWAP(char, bezt1->h1, bezt1->h2);
			SWAP(short, bezt1->f1, bezt1->f3);
			
			if (bezt1!=bezt2) {
				SWAP(char, bezt2->h1, bezt2->h2);
				SWAP(short, bezt2->f1, bezt2->f3);
				bezt1->alfa= -bezt1->alfa;
				bezt2->alfa= -bezt2->alfa;
			}
			a--;
			bezt1++; 
			bezt2--;
		}
	}
	else if (nu->pntsv==1) {
		a= nu->pntsu;
		bp1= nu->bp;
		bp2= bp1+(a-1);
		a/= 2;
		while (bp1!=bp2 && a>0) {
			SWAP(BPoint, *bp1, *bp2);
			a--;
			bp1->alfa= -bp1->alfa;
			bp2->alfa= -bp2->alfa;
			bp1++; 
			bp2--;
		}
		if (nu->type == CU_NURBS) {
			/* no knots for too short paths */
			if (nu->knotsu) {
				/* inverse knots */
				a= KNOTSU(nu);
				fp1= nu->knotsu;
				fp2= fp1+(a-1);
				a/= 2;
				while (fp1!=fp2 && a>0) {
					SWAP(float, *fp1, *fp2);
					a--;
					fp1++; 
					fp2--;
				}
				/* and make in increasing order again */
				a= KNOTSU(nu);
				fp1= nu->knotsu;
				fp2=tempf= MEM_mallocN(sizeof(float)*a, "switchdirect");
				while (a--) {
					fp2[0]= fabs(fp1[1]-fp1[0]);
					fp1++;
					fp2++;
				}
		
				a= KNOTSU(nu)-1;
				fp1= nu->knotsu;
				fp2= tempf;
				fp1[0]= 0.0;
				fp1++;
				while (a--) {
					fp1[0]= fp1[-1]+fp2[0];
					fp1++;
					fp2++;
				}
				MEM_freeN(tempf);
			}
		}
	}
	else {
		
		for (b=0; b<nu->pntsv; b++) {
		
			bp1= nu->bp+b*nu->pntsu;
			a= nu->pntsu;
			bp2= bp1+(a-1);
			a/= 2;
			
			while (bp1!=bp2 && a>0) {
				SWAP(BPoint, *bp1, *bp2);
				a--;
				bp1++; 
				bp2--;
			}
		}
	}
}


float (*BKE_curve_vertexCos_get(Curve *UNUSED(cu), ListBase *lb, int *numVerts_r))[3]
{
	int i, numVerts = *numVerts_r = BKE_nurbList_verts_count(lb);
	float *co, (*cos)[3] = MEM_mallocN(sizeof(*cos)*numVerts, "cu_vcos");
	Nurb *nu;

	co = cos[0];
	for (nu=lb->first; nu; nu=nu->next) {
		if (nu->type == CU_BEZIER) {
			BezTriple *bezt = nu->bezt;

			for (i=0; i<nu->pntsu; i++,bezt++) {
				copy_v3_v3(co, bezt->vec[0]); co+=3;
				copy_v3_v3(co, bezt->vec[1]); co+=3;
				copy_v3_v3(co, bezt->vec[2]); co+=3;
			}
		}
		else {
			BPoint *bp = nu->bp;

			for (i=0; i<nu->pntsu*nu->pntsv; i++,bp++) {
				copy_v3_v3(co, bp->vec); co+=3;
			}
		}
	}

	return cos;
}

void BK_curve_vertexCos_apply(Curve *UNUSED(cu), ListBase *lb, float (*vertexCos)[3])
{
	float *co = vertexCos[0];
	Nurb *nu;
	int i;

	for (nu=lb->first; nu; nu=nu->next) {
		if (nu->type == CU_BEZIER) {
			BezTriple *bezt = nu->bezt;

			for (i=0; i<nu->pntsu; i++,bezt++) {
				copy_v3_v3(bezt->vec[0], co); co+=3;
				copy_v3_v3(bezt->vec[1], co); co+=3;
				copy_v3_v3(bezt->vec[2], co); co+=3;
			}
		}
		else {
			BPoint *bp = nu->bp;

			for (i=0; i<nu->pntsu*nu->pntsv; i++,bp++) {
				copy_v3_v3(bp->vec, co); co+=3;
			}
		}

		calchandlesNurb_intern(nu, TRUE);
	}
}

float (*BKE_curve_keyVertexCos_get(Curve *UNUSED(cu), ListBase *lb, float *key))[3]
{
	int i, numVerts = BKE_nurbList_verts_count(lb);
	float *co, (*cos)[3] = MEM_mallocN(sizeof(*cos)*numVerts, "cu_vcos");
	Nurb *nu;

	co = cos[0];
	for (nu=lb->first; nu; nu=nu->next) {
		if (nu->type == CU_BEZIER) {
			BezTriple *bezt = nu->bezt;

			for (i=0; i<nu->pntsu; i++,bezt++) {
				copy_v3_v3(co, key); co+=3; key+=3;
				copy_v3_v3(co, key); co+=3; key+=3;
				copy_v3_v3(co, key); co+=3; key+=3;
				key+=3; /* skip tilt */
			}
		}
		else {
			BPoint *bp = nu->bp;

			for (i=0; i<nu->pntsu*nu->pntsv; i++,bp++) {
				copy_v3_v3(co, key); co+=3; key+=3;
				key++; /* skip tilt */
			}
		}
	}

	return cos;
}

void BKE_curve_keyVertexTilts_apply(Curve *UNUSED(cu), ListBase *lb, float *key)
{
	Nurb *nu;
	int i;

	for (nu=lb->first; nu; nu=nu->next) {
		if (nu->type == CU_BEZIER) {
			BezTriple *bezt = nu->bezt;

			for (i=0; i<nu->pntsu; i++,bezt++) {
				key+=3*3;
				bezt->alfa= *key;
				key+=3;
			}
		}
		else {
			BPoint *bp = nu->bp;

			for (i=0; i<nu->pntsu*nu->pntsv; i++,bp++) {
				key+=3;
				bp->alfa= *key;
				key++;
			}
		}
	}
}

int BKE_nurb_check_valid_u( struct Nurb *nu )
{
	if (nu==NULL)						return 0;
	if (nu->pntsu <= 1)					return 0;
	if (nu->type != CU_NURBS)			return 1; /* not a nurb, lets assume its valid */
	
	if (nu->pntsu < nu->orderu)			return 0;
	if (((nu->flag & CU_NURB_CYCLIC)==0) && (nu->flagu & CU_NURB_BEZIER)) { /* Bezier U Endpoints */
		if (nu->orderu==4) {
			if (nu->pntsu < 5)			return 0; /* bezier with 4 orderu needs 5 points */
		}
		else if (nu->orderu != 3)		return 0; /* order must be 3 or 4 */
	}
	return 1;
}
int BKE_nurb_check_valid_v( struct Nurb *nu)
{
	if (nu==NULL)						return 0;
	if (nu->pntsv <= 1)					return 0;
	if (nu->type != CU_NURBS)			return 1; /* not a nurb, lets assume its valid */
	
	if (nu->pntsv < nu->orderv)			return 0;
	if (((nu->flag & CU_NURB_CYCLIC)==0) && (nu->flagv & CU_NURB_BEZIER)) { /* Bezier V Endpoints */
		if (nu->orderv==4) {
			if (nu->pntsv < 5)			return 0; /* bezier with 4 orderu needs 5 points */
		}
		else if (nu->orderv != 3)		return 0; /* order must be 3 or 4 */
	}
	return 1;
}

int BKE_nurb_order_clamp_u( struct Nurb *nu )
{
	int change = 0;
	if (nu->pntsu<nu->orderu) {
		nu->orderu= nu->pntsu;
		change= 1;
	}
	if (((nu->flagu & CU_NURB_CYCLIC)==0) && (nu->flagu & CU_NURB_BEZIER)) {
		CLAMP(nu->orderu, 3,4);
		change= 1;
	}
	return change;
}

int BKE_nurb_order_clamp_v( struct Nurb *nu)
{
	int change = 0;
	if (nu->pntsv<nu->orderv) {
		nu->orderv= nu->pntsv;
		change= 1;
	}
	if (((nu->flagv & CU_NURB_CYCLIC)==0) && (nu->flagv & CU_NURB_BEZIER)) {
		CLAMP(nu->orderv, 3,4);
		change= 1;
	}
	return change;
}

/* Get edit nurbs or normal nurbs list */
ListBase *BKE_curve_nurbs_get(Curve *cu)
{
	if (cu->editnurb) {
		return BKE_curve_editNurbs_get(cu);
	}

	return &cu->nurb;
}


/* basic vertex data functions */
int BKE_curve_minmax(Curve *cu, float min[3], float max[3])
{
	ListBase *nurb_lb= BKE_curve_nurbs_get(cu);
	Nurb *nu;

	for (nu= nurb_lb->first; nu; nu= nu->next)
		BKE_nurb_minmax(nu, min, max);

	return (nurb_lb->first != NULL);
}

int BKE_curve_center_median(Curve *cu, float cent[3])
{
	ListBase *nurb_lb= BKE_curve_nurbs_get(cu);
	Nurb *nu;
	int total= 0;

	zero_v3(cent);

	for (nu= nurb_lb->first; nu; nu= nu->next) {
		int i;

		if (nu->type == CU_BEZIER) {
			BezTriple *bezt;
			i= nu->pntsu;
			total += i * 3;
			for (bezt= nu->bezt; i--; bezt++) {
				add_v3_v3(cent, bezt->vec[0]);
				add_v3_v3(cent, bezt->vec[1]);
				add_v3_v3(cent, bezt->vec[2]);
			}
		}
		else {
			BPoint *bp;
			i= nu->pntsu*nu->pntsv;
			total += i;
			for (bp= nu->bp; i--; bp++) {
				add_v3_v3(cent, bp->vec);
			}
		}
	}

	mul_v3_fl(cent, 1.0f/(float)total);

	return (total != 0);
}

int BKE_curve_center_bounds(Curve *cu, float cent[3])
{
	float min[3], max[3];
	INIT_MINMAX(min, max);
	if (BKE_curve_minmax(cu, min, max)) {
		mid_v3_v3v3(cent, min, max);
		return 1;
	}

	return 0;
}

void BKE_curve_translate(Curve *cu, float offset[3], int do_keys)
{
	ListBase *nurb_lb= BKE_curve_nurbs_get(cu);
	Nurb *nu;
	int i;

	for (nu= nurb_lb->first; nu; nu= nu->next) {
		BezTriple *bezt;
		BPoint *bp;

		if (nu->type == CU_BEZIER) {
			i= nu->pntsu;
			for (bezt= nu->bezt; i--; bezt++) {
				add_v3_v3(bezt->vec[0], offset);
				add_v3_v3(bezt->vec[1], offset);
				add_v3_v3(bezt->vec[2], offset);
			}
		}
		else {
			i= nu->pntsu*nu->pntsv;
			for (bp= nu->bp; i--; bp++) {
				add_v3_v3(bp->vec, offset);
			}
		}
	}

	if (do_keys && cu->key) {
		KeyBlock *kb;
		for (kb=cu->key->block.first; kb; kb=kb->next) {
			float *fp= kb->data;
			for (i= kb->totelem; i--; fp+=3) {
				add_v3_v3(fp, offset);
			}
		}
	}
}

void BKE_curve_delete_material_index(Curve *cu, int index)
{
	const int curvetype= BKE_curve_type_get(cu);

	if (curvetype == OB_FONT) {
		struct CharInfo *info= cu->strinfo;
		int i;
		for (i= cu->len-1; i >= 0; i--, info++) {
			if (info->mat_nr && info->mat_nr>=index) {
				info->mat_nr--;
			}
		}
	}
	else {
		Nurb *nu;

		for (nu= cu->nurb.first; nu; nu= nu->next) {
			if (nu->mat_nr && nu->mat_nr>=index) {
				nu->mat_nr--;
				if (curvetype == OB_CURVE) nu->charidx--;
			}
		}
	}
}
