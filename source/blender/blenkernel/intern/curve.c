
/*  curve.c 
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

#include <math.h>  // floor
#include <string.h>
#include <stdlib.h>  

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"  
#include "BLI_arithb.h"  

#include "DNA_object_types.h"  
#include "DNA_curve_types.h"  
#include "DNA_material_types.h"  

/* for dereferencing pointers */
#include "DNA_ID.h"  
#include "DNA_key_types.h"  
#include "DNA_scene_types.h"  
#include "DNA_vfont_types.h"  

#include "BKE_animsys.h"
#include "BKE_anim.h"  
#include "BKE_curve.h"  
#include "BKE_displist.h"  
#include "BKE_font.h" 
#include "BKE_global.h" 
#include "BKE_key.h"  
#include "BKE_library.h"  
#include "BKE_main.h"  
#include "BKE_mesh.h" 
#include "BKE_object.h"  
#include "BKE_utildefines.h"  // VECCOPY

/* globals */

/* local */
static int cu_isectLL(float *v1, float *v2, float *v3, float *v4, 
			   short cox, short coy, 
			   float *labda, float *mu, float *vec);

void unlink_curve(Curve *cu)
{
	int a;
	
	for(a=0; a<cu->totcol; a++) {
		if(cu->mat[a]) cu->mat[a]->id.us--;
		cu->mat[a]= 0;
	}
	if(cu->vfont) cu->vfont->id.us--; 
	cu->vfont= 0;
	if(cu->key) cu->key->id.us--;
	cu->key= 0;
}

/* frees editcurve entirely */
void BKE_free_editfont(Curve *cu)
{
	if(cu->editfont) {
		EditFont *ef= cu->editfont;
		
		if(ef->oldstr) MEM_freeN(ef->oldstr);
		if(ef->oldstrinfo) MEM_freeN(ef->oldstrinfo);
		if(ef->textbuf) MEM_freeN(ef->textbuf);
		if(ef->textbufinfo) MEM_freeN(ef->textbufinfo);
		if(ef->copybuf) MEM_freeN(ef->copybuf);
		if(ef->copybufinfo) MEM_freeN(ef->copybufinfo);
		
		MEM_freeN(ef);
		cu->editfont= NULL;
	}
}

/* don't free curve itself */
void free_curve(Curve *cu)
{
	freeNurblist(&cu->nurb);
	BLI_freelistN(&cu->bev);
	freedisplist(&cu->disp);
	BKE_free_editfont(cu);
	
	if(cu->editnurb) {
		freeNurblist(cu->editnurb);
		MEM_freeN(cu->editnurb);
		cu->editnurb= NULL;
	}

	unlink_curve(cu);
	BKE_free_animdata((ID *)cu);
	
	if(cu->mat) MEM_freeN(cu->mat);
	if(cu->str) MEM_freeN(cu->str);
	if(cu->strinfo) MEM_freeN(cu->strinfo);
	if(cu->bb) MEM_freeN(cu->bb);
	if(cu->path) free_path(cu->path);
	if(cu->tb) MEM_freeN(cu->tb);
}

Curve *add_curve(char *name, int type)
{
	Curve *cu;

	cu= alloc_libblock(&G.main->curve, ID_CU, name);
	
	cu->size[0]= cu->size[1]= cu->size[2]= 1.0;
	cu->flag= CU_FRONT|CU_BACK|CU_PATH_RADIUS;
	cu->pathlen= 100;
	cu->resolu= cu->resolv= 12;
	cu->width= 1.0;
	cu->wordspace = 1.0;
	cu->spacing= cu->linedist= 1.0;
	cu->fsize= 1.0;
	cu->ulheight = 0.05;	
	cu->texflag= CU_AUTOSPACE;
	cu->twist_mode= CU_TWIST_MINIMUM;	// XXX: this one seems to be the best one in most cases, at least for curve deform...
	
	cu->bb= unit_boundbox();
	
	if(type==OB_FONT) {
		cu->vfont= cu->vfontb= cu->vfonti= cu->vfontbi= get_builtin_font();
		cu->vfont->id.us+=4;
		cu->str= MEM_mallocN(12, "str");
		strcpy(cu->str, "Text");
		cu->pos= 4;
		cu->strinfo= MEM_callocN(12*sizeof(CharInfo), "strinfo new");
		cu->totbox= cu->actbox= 1;
		cu->tb= MEM_callocN(MAXTEXTBOX*sizeof(TextBox), "textbox");
		cu->tb[0].w = cu->tb[0].h = 0.0;
	}
	
	return cu;
}

Curve *copy_curve(Curve *cu)
{
	Curve *cun;
	int a;
	
	cun= copy_libblock(cu);
	cun->nurb.first= cun->nurb.last= 0;
	duplicateNurblist( &(cun->nurb), &(cu->nurb));

	cun->mat= MEM_dupallocN(cu->mat);
	for(a=0; a<cun->totcol; a++) {
		id_us_plus((ID *)cun->mat[a]);
	}
	
	cun->str= MEM_dupallocN(cu->str);
	cun->strinfo= MEM_dupallocN(cu->strinfo);	
	cun->tb= MEM_dupallocN(cu->tb);
	cun->bb= MEM_dupallocN(cu->bb);
	
	cun->key= copy_key(cu->key);
	if(cun->key) cun->key->from= (ID *)cun;
	
	cun->disp.first= cun->disp.last= 0;
	cun->bev.first= cun->bev.last= 0;
	cun->path= 0;

	cun->editnurb= NULL;

#if 0	// XXX old animation system
	/* single user ipo too */
	if(cun->ipo) cun->ipo= copy_ipo(cun->ipo);
#endif // XXX old animation system

	id_us_plus((ID *)cun->vfont);
	id_us_plus((ID *)cun->vfontb);	
	id_us_plus((ID *)cun->vfonti);
	id_us_plus((ID *)cun->vfontbi);
	
	return cun;
}

void make_local_curve(Curve *cu)
{
	Object *ob = 0;
	Curve *cun;
	int local=0, lib=0;
	
	/* - when there are only lib users: don't do
	 * - when there are only local users: set flag
	 * - mixed: do a copy
	 */
	
	if(cu->id.lib==0) return;
	
	if(cu->vfont) cu->vfont->id.lib= 0;
	
	if(cu->id.us==1) {
		cu->id.lib= 0;
		cu->id.flag= LIB_LOCAL;
		new_id(0, (ID *)cu, 0);
		return;
	}
	
	ob= G.main->object.first;
	while(ob) {
		if(ob->data==cu) {
			if(ob->id.lib) lib= 1;
			else local= 1;
		}
		ob= ob->id.next;
	}
	
	if(local && lib==0) {
		cu->id.lib= 0;
		cu->id.flag= LIB_LOCAL;
		new_id(0, (ID *)cu, 0);
	}
	else if(local && lib) {
		cun= copy_curve(cu);
		cun->id.us= 0;
		
		ob= G.main->object.first;
		while(ob) {
			if(ob->data==cu) {
				
				if(ob->id.lib==0) {
					ob->data= cun;
					cun->id.us++;
					cu->id.us--;
				}
			}
			ob= ob->id.next;
		}
	}
}

short curve_type(Curve *cu)
{
	Nurb *nu;
	if(cu->vfont) {
		return OB_FONT;
	}
	for (nu= cu->nurb.first; nu; nu= nu->next) {
		if(nu->pntsv>1) {
			return OB_SURF;
		}
	}
	
	return OB_CURVE;
}

void test_curve_type(Object *ob)
{	
	ob->type = curve_type(ob->data);
}

void tex_space_curve(Curve *cu)
{
	DispList *dl;
	BoundBox *bb;
	float *fp, min[3], max[3], loc[3], size[3];
	int tot, doit= 0;
	
	if(cu->bb==NULL) cu->bb= MEM_callocN(sizeof(BoundBox), "boundbox");
	bb= cu->bb;
	
	INIT_MINMAX(min, max);

	dl= cu->disp.first;
	while(dl) {
		
		if(dl->type==DL_INDEX3 || dl->type==DL_INDEX3) tot= dl->nr;
		else tot= dl->nr*dl->parts;
		
		if(tot) doit= 1;
		fp= dl->verts;
		while(tot--) {
			DO_MINMAX(fp, min, max);
			fp += 3;
		}
		dl= dl->next;
	}

	if(!doit) {
		min[0] = min[1] = min[2] = -1.0f;
		max[0] = max[1] = max[2] = 1.0f;
	}
	
	loc[0]= (min[0]+max[0])/2.0f;
	loc[1]= (min[1]+max[1])/2.0f;
	loc[2]= (min[2]+max[2])/2.0f;
	
	size[0]= (max[0]-min[0])/2.0f;
	size[1]= (max[1]-min[1])/2.0f;
	size[2]= (max[2]-min[2])/2.0f;

	boundbox_set_from_min_max(bb, min, max);

	if(cu->texflag & CU_AUTOSPACE) {
		VECCOPY(cu->loc, loc);
		VECCOPY(cu->size, size);
		cu->rot[0]= cu->rot[1]= cu->rot[2]= 0.0;

		if(cu->size[0]==0.0) cu->size[0]= 1.0;
		else if(cu->size[0]>0.0 && cu->size[0]<0.00001) cu->size[0]= 0.00001;
		else if(cu->size[0]<0.0 && cu->size[0]> -0.00001) cu->size[0]= -0.00001;
	
		if(cu->size[1]==0.0) cu->size[1]= 1.0;
		else if(cu->size[1]>0.0 && cu->size[1]<0.00001) cu->size[1]= 0.00001;
		else if(cu->size[1]<0.0 && cu->size[1]> -0.00001) cu->size[1]= -0.00001;
	
		if(cu->size[2]==0.0) cu->size[2]= 1.0;
		else if(cu->size[2]>0.0 && cu->size[2]<0.00001) cu->size[2]= 0.00001;
		else if(cu->size[2]<0.0 && cu->size[2]> -0.00001) cu->size[2]= -0.00001;

	}
}


int count_curveverts(ListBase *nurb)
{
	Nurb *nu;
	int tot=0;
	
	nu= nurb->first;
	while(nu) {
		if(nu->bezt) tot+= 3*nu->pntsu;
		else if(nu->bp) tot+= nu->pntsu*nu->pntsv;
		
		nu= nu->next;
	}
	return tot;
}

int count_curveverts_without_handles(ListBase *nurb)
{
	Nurb *nu;
	int tot=0;
	
	nu= nurb->first;
	while(nu) {
		if(nu->bezt) tot+= nu->pntsu;
		else if(nu->bp) tot+= nu->pntsu*nu->pntsv;
		
		nu= nu->next;
	}
	return tot;
}

/* **************** NURBS ROUTINES ******************** */

void freeNurb(Nurb *nu)
{

	if(nu==0) return;

	if(nu->bezt) MEM_freeN(nu->bezt);
	nu->bezt= 0;
	if(nu->bp) MEM_freeN(nu->bp);
	nu->bp= 0;
	if(nu->knotsu) MEM_freeN(nu->knotsu);
	nu->knotsu= NULL;
	if(nu->knotsv) MEM_freeN(nu->knotsv);
	nu->knotsv= NULL;
	/* if(nu->trim.first) freeNurblist(&(nu->trim)); */

	MEM_freeN(nu);

}


void freeNurblist(ListBase *lb)
{
	Nurb *nu, *next;

	if(lb==0) return;

	nu= lb->first;
	while(nu) {
		next= nu->next;
		freeNurb(nu);
		nu= next;
	}
	lb->first= lb->last= 0;
}

Nurb *duplicateNurb(Nurb *nu)
{
	Nurb *newnu;
	int len;

	newnu= (Nurb*)MEM_mallocN(sizeof(Nurb),"duplicateNurb");
	if(newnu==0) return 0;
	memcpy(newnu, nu, sizeof(Nurb));

	if(nu->bezt) {
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
		
		if(nu->knotsu) {
			len= KNOTSU(nu);
			if(len) {
				newnu->knotsu= MEM_mallocN(len*sizeof(float), "duplicateNurb4");
				memcpy(newnu->knotsu, nu->knotsu, sizeof(float)*len);
			}
		}
		if(nu->pntsv>1 && nu->knotsv) {
			len= KNOTSV(nu);
			if(len) {
				newnu->knotsv= MEM_mallocN(len*sizeof(float), "duplicateNurb5");
				memcpy(newnu->knotsv, nu->knotsv, sizeof(float)*len);
			}
		}
	}
	return newnu;
}

void duplicateNurblist(ListBase *lb1, ListBase *lb2)
{
	Nurb *nu, *nun;
	
	freeNurblist(lb1);
	
	nu= lb2->first;
	while(nu) {
		nun= duplicateNurb(nu);
		BLI_addtail(lb1, nun);
		
		nu= nu->next;
	}
}

void test2DNurb(Nurb *nu)
{
	BezTriple *bezt;
	BPoint *bp;
	int a;
	
	if((nu->flag & CU_2D)==0)
		return;

	if(nu->type == CU_BEZIER) {
		a= nu->pntsu;
		bezt= nu->bezt;
		while(a--) {
			bezt->vec[0][2]= 0.0; 
			bezt->vec[1][2]= 0.0; 
			bezt->vec[2][2]= 0.0;
			bezt++;
		}
	}
	else {
		a= nu->pntsu*nu->pntsv;
		bp= nu->bp;
		while(a--) {
			bp->vec[2]= 0.0;
			bp++;
		}
	}
}

void minmaxNurb(Nurb *nu, float *min, float *max)
{
	BezTriple *bezt;
	BPoint *bp;
	int a;

	if(nu->type == CU_BEZIER) {
		a= nu->pntsu;
		bezt= nu->bezt;
		while(a--) {
			DO_MINMAX(bezt->vec[0], min, max);
			DO_MINMAX(bezt->vec[1], min, max);
			DO_MINMAX(bezt->vec[2], min, max);
			bezt++;
		}
	}
	else {
		a= nu->pntsu*nu->pntsv;
		bp= nu->bp;
		while(a--) {
			DO_MINMAX(bp->vec, min, max);
			bp++;
		}
	}

}

/* ~~~~~~~~~~~~~~~~~~~~Non Uniform Rational B Spline calculations ~~~~~~~~~~~ */


static void calcknots(float *knots, short aantal, short order, short type)
/* knots: number of pnts NOT corrected for cyclic */
/* type;	 0: uniform, 1: endpoints, 2: bezier */
{
	float k;
	int a, t;

        t = aantal+order;
	if(type==0) {

		for(a=0;a<t;a++) {
			knots[a]= (float)a;
		}
	}
	else if(type==1) {
		k= 0.0;
		for(a=1;a<=t;a++) {
			knots[a-1]= k;
			if(a>=order && a<=aantal) k+= 1.0;
		}
	}
	else if(type==2) {
		/* Warning, the order MUST be 2 or 4, if this is not enforced, the displist will be corrupt */
		if(order==4) {
			k= 0.34;
			for(a=0;a<t;a++) {
				knots[a]= (float)floor(k);
				k+= (1.0/3.0);
			}
		}
		else if(order==3) {
			k= 0.6;
			for(a=0;a<t;a++) {
				if(a>=order && a<=aantal) k+= (0.5);
				knots[a]= (float)floor(k);
			}
		}
		else {
			printf("bez nurb curve order is not 3 or 4, should never happen\n");
		}
	}
}

static void makecyclicknots(float *knots, short pnts, short order)
/* pnts, order: number of pnts NOT corrected for cyclic */
{
	int a, b, order2, c;

	if(knots==0) return;

	order2=order-1;

	/* do first long rows (order -1), remove identical knots at endpoints */
	if(order>2) {
		b= pnts+order2;
		for(a=1; a<order2; a++) {
			if(knots[b]!= knots[b-a]) break;
		}
		if(a==order2) knots[pnts+order-2]+= 1.0;
	}

	b= order;
        c=pnts + order + order2;
	for(a=pnts+order2; a<c; a++) {
		knots[a]= knots[a-1]+ (knots[b]-knots[b-1]);
		b--;
	}
}



void makeknots(Nurb *nu, short uv)
{
	if(nu->type == CU_NURBS) {
		if(uv == 1) {
			if(nu->knotsu) MEM_freeN(nu->knotsu);
			if(check_valid_nurb_u(nu)) {
				nu->knotsu= MEM_callocN(4+sizeof(float)*KNOTSU(nu), "makeknots");
				if(nu->flagu & CU_CYCLIC) {
					calcknots(nu->knotsu, nu->pntsu, nu->orderu, 0);  /* cyclic should be uniform */
					makecyclicknots(nu->knotsu, nu->pntsu, nu->orderu);
				} else {
					calcknots(nu->knotsu, nu->pntsu, nu->orderu, nu->flagu>>1);
				}
			}
			else nu->knotsu= NULL;
		
		} else if(uv == 2) {
			if(nu->knotsv) MEM_freeN(nu->knotsv);
			if(check_valid_nurb_v(nu)) {
				nu->knotsv= MEM_callocN(4+sizeof(float)*KNOTSV(nu), "makeknots");
				if(nu->flagv & CU_CYCLIC) {
					calcknots(nu->knotsv, nu->pntsv, nu->orderv, 0);  /* cyclic should be uniform */
					makecyclicknots(nu->knotsv, nu->pntsv, nu->orderv);
				} else {
					calcknots(nu->knotsv, nu->pntsv, nu->orderv, nu->flagv>>1);
				}
			}
			else nu->knotsv= NULL;
		}
	}
}

static void basisNurb(float t, short order, short pnts, float *knots, float *basis, int *start, int *end)
{
	float d, e;
	int i, i1 = 0, i2 = 0 ,j, orderpluspnts, opp2, o2;

	orderpluspnts= order+pnts;
        opp2 = orderpluspnts-1;

	/* this is for float inaccuracy */
	if(t < knots[0]) t= knots[0];
	else if(t > knots[opp2]) t= knots[opp2];

	/* this part is order '1' */
        o2 = order + 1;
	for(i=0;i<opp2;i++) {
		if(knots[i]!=knots[i+1] && t>= knots[i] && t<=knots[i+1]) {
			basis[i]= 1.0;
			i1= i-o2;
			if(i1<0) i1= 0;
			i2= i;
			i++;
			while(i<opp2) {
				basis[i]= 0.0;
				i++;
			}
			break;
		}
		else basis[i]= 0.0;
	}
	basis[i]= 0.0;
	
	/* this is order 2,3,... */
	for(j=2; j<=order; j++) {

		if(i2+j>= orderpluspnts) i2= opp2-j;

		for(i= i1; i<=i2; i++) {
			if(basis[i]!=0.0)
				d= ((t-knots[i])*basis[i]) / (knots[i+j-1]-knots[i]);
			else
				d= 0.0;

			if(basis[i+1]!=0.0)
				e= ((knots[i+j]-t)*basis[i+1]) / (knots[i+j]-knots[i+1]);
			else
				e= 0.0;

			basis[i]= d+e;
		}
	}

	*start= 1000;
	*end= 0;

	for(i=i1; i<=i2; i++) {
		if(basis[i]>0.0) {
			*end= i;
			if(*start==1000) *start= i;
		}
	}
}


void makeNurbfaces(Nurb *nu, float *coord_array, int rowstride) 
/* coord_array  has to be 3*4*resolu*resolv in size, and zero-ed */
{
	BPoint *bp;
	float *basisu, *basis, *basisv, *sum, *fp, *in;
	float u, v, ustart, uend, ustep, vstart, vend, vstep, sumdiv;
	int i, j, iofs, jofs, cycl, len, resolu, resolv;
	int istart, iend, jsta, jen, *jstart, *jend, ratcomp;
	
	int totu = nu->pntsu*nu->resolu, totv = nu->pntsv*nu->resolv;
	
	if(nu->knotsu==NULL || nu->knotsv==NULL) return;
	if(nu->orderu>nu->pntsu) return;
	if(nu->orderv>nu->pntsv) return;
	if(coord_array==NULL) return;
	
	/* allocate and initialize */
	len = totu * totv;
	if(len==0) return;
	

	
	sum= (float *)MEM_callocN(sizeof(float)*len, "makeNurbfaces1");
	
	len= totu*totv;
	if(len==0) {
		MEM_freeN(sum);
		return;
	}

	bp= nu->bp;
	i= nu->pntsu*nu->pntsv;
	ratcomp=0;
	while(i--) {
		if(bp->vec[3]!=1.0) {
			ratcomp= 1;
			break;
		}
		bp++;
	}
	
	fp= nu->knotsu;
	ustart= fp[nu->orderu-1];
	if(nu->flagu & CU_CYCLIC) uend= fp[nu->pntsu+nu->orderu-1];
	else uend= fp[nu->pntsu];
	ustep= (uend-ustart)/((nu->flagu & CU_CYCLIC) ? totu : totu - 1);
	
	basisu= (float *)MEM_mallocN(sizeof(float)*KNOTSU(nu), "makeNurbfaces3");

	fp= nu->knotsv;
	vstart= fp[nu->orderv-1];
	
	if(nu->flagv & CU_CYCLIC) vend= fp[nu->pntsv+nu->orderv-1];
	else vend= fp[nu->pntsv];
	vstep= (vend-vstart)/((nu->flagv & CU_CYCLIC) ? totv : totv - 1);
	
	len= KNOTSV(nu);
	basisv= (float *)MEM_mallocN(sizeof(float)*len*totv, "makeNurbfaces3");
	jstart= (int *)MEM_mallocN(sizeof(float)*totv, "makeNurbfaces4");
	jend= (int *)MEM_mallocN(sizeof(float)*totv, "makeNurbfaces5");

	/* precalculation of basisv and jstart,jend */
	if(nu->flagv & CU_CYCLIC) cycl= nu->orderv-1; 
	else cycl= 0;
	v= vstart;
	basis= basisv;
	resolv= totv;
	while(resolv--) {
		basisNurb(v, nu->orderv, (short)(nu->pntsv+cycl), nu->knotsv, basis, jstart+resolv, jend+resolv);
		basis+= KNOTSV(nu);
		v+= vstep;
	}

	if(nu->flagu & CU_CYCLIC) cycl= nu->orderu-1; 
	else cycl= 0;
	in= coord_array;
	u= ustart;
	resolu= totu;
	while(resolu--) {

		basisNurb(u, nu->orderu, (short)(nu->pntsu+cycl), nu->knotsu, basisu, &istart, &iend);

		basis= basisv;
		resolv= totv;
		while(resolv--) {

			jsta= jstart[resolv];
			jen= jend[resolv];

			/* calculate sum */
			sumdiv= 0.0;
			fp= sum;

			for(j= jsta; j<=jen; j++) {

				if(j>=nu->pntsv) jofs= (j - nu->pntsv);
				else jofs= j;
				bp= nu->bp+ nu->pntsu*jofs+istart-1;

				for(i= istart; i<=iend; i++, fp++) {

					if(i>= nu->pntsu) {
						iofs= i- nu->pntsu;
						bp= nu->bp+ nu->pntsu*jofs+iofs;
					}
					else bp++;

					if(ratcomp) {
						*fp= basisu[i]*basis[j]*bp->vec[3];
						sumdiv+= *fp;
					}
					else *fp= basisu[i]*basis[j];
				}
			}
		
			if(ratcomp) {
				fp= sum;
				for(j= jsta; j<=jen; j++) {
					for(i= istart; i<=iend; i++, fp++) {
						*fp/= sumdiv;
					}
				}
			}

			/* one! (1.0) real point now */
			fp= sum;
			for(j= jsta; j<=jen; j++) {

				if(j>=nu->pntsv) jofs= (j - nu->pntsv);
				else jofs= j;
				bp= nu->bp+ nu->pntsu*jofs+istart-1;

				for(i= istart; i<=iend; i++, fp++) {

					if(i>= nu->pntsu) {
						iofs= i- nu->pntsu;
						bp= nu->bp+ nu->pntsu*jofs+iofs;
					}
					else bp++;

					if(*fp!=0.0) {
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

void makeNurbcurve(Nurb *nu, float *coord_array, float *tilt_array, float *radius_array, int resolu, int stride)
/* coord_array has to be 3*4*pntsu*resolu in size and zero-ed
 * tilt_array and radius_array will be written to if valid */
{
	BPoint *bp;
	float u, ustart, uend, ustep, sumdiv;
	float *basisu, *sum, *fp;
	float *coord_fp= coord_array, *tilt_fp= tilt_array, *radius_fp= radius_array;
	int i, len, istart, iend, cycl;

	if(nu->knotsu==NULL) return;
	if(nu->orderu>nu->pntsu) return;
	if(coord_array==0) return;

	/* allocate and initialize */
	len= nu->pntsu;
	if(len==0) return;
	sum= (float *)MEM_callocN(sizeof(float)*len, "makeNurbcurve1");
	
	resolu= (resolu*SEGMENTSU(nu));
	
	if(resolu==0) {
		MEM_freeN(sum);
		return;
	}

	fp= nu->knotsu;
	ustart= fp[nu->orderu-1];
	if(nu->flagu & CU_CYCLIC) uend= fp[nu->pntsu+nu->orderu-1];
	else uend= fp[nu->pntsu];
	ustep= (uend-ustart)/(resolu - ((nu->flagu & CU_CYCLIC) ? 0 : 1));
	
	basisu= (float *)MEM_mallocN(sizeof(float)*KNOTSU(nu), "makeNurbcurve3");

	if(nu->flagu & CU_CYCLIC) cycl= nu->orderu-1; 
	else cycl= 0;

	u= ustart;
	while(resolu--) {

		basisNurb(u, nu->orderu, (short)(nu->pntsu+cycl), nu->knotsu, basisu, &istart, &iend);
		/* calc sum */
		sumdiv= 0.0;
		fp= sum;
		bp= nu->bp+ istart-1;
		for(i= istart; i<=iend; i++, fp++) {

			if(i>=nu->pntsu) bp= nu->bp+(i - nu->pntsu);
			else bp++;

			*fp= basisu[i]*bp->vec[3];
			sumdiv+= *fp;
		}
		if(sumdiv!=0.0) if(sumdiv<0.999 || sumdiv>1.001) {
			/* is normalizing needed? */
			fp= sum;
			for(i= istart; i<=iend; i++, fp++) {
				*fp/= sumdiv;
			}
		}

		/* one! (1.0) real point */
		fp= sum;
		bp= nu->bp+ istart-1;
		for(i= istart; i<=iend; i++, fp++) {

			if(i>=nu->pntsu) bp= nu->bp+(i - nu->pntsu);
			else bp++;

			if(*fp!=0.0) {
				
				coord_fp[0]+= (*fp) * bp->vec[0];
				coord_fp[1]+= (*fp) * bp->vec[1];
				coord_fp[2]+= (*fp) * bp->vec[2];
				
				if (tilt_fp)
					(*tilt_fp) += (*fp) * bp->alfa;
				
				if (radius_fp)
					(*radius_fp) += (*fp) * bp->radius;
				
			}
		}

		coord_fp = (float *)(((char *)coord_fp) + stride);
		
		if (tilt_fp)	tilt_fp = (float *)(((char *)tilt_fp) + stride);
		if (radius_fp)	radius_fp = (float *)(((char *)radius_fp) + stride);
		
		u+= ustep;
	}

	/* free */
	MEM_freeN(sum);
	MEM_freeN(basisu);
}

/* forward differencing method for bezier curve */
void forward_diff_bezier(float q0, float q1, float q2, float q3, float *p, int it, int stride)
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
  
  	for(a=0; a<=it; a++) {
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
  	for(a=0; a<=it; a++) {
		float t = (float)a / (float)it;

		int i;
		for(i=0; i<3; i++) {
			p[i]= (-6*t + 6)*p0[i] + (18*t - 12)*p1[i] + (-18*t + 6)*p2[i] + (6*t)*p3[i];
		}
		Normalize(p);
		p = (float *)(((char *)p)+stride);
 	}
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

float *make_orco_surf(Object *ob)
{
	Curve *cu= ob->data;
	Nurb *nu;
	int a, b, tot=0;
	int sizeu, sizev;
	float *fp, *coord_array;
	
	/* first calculate the size of the datablock */
	nu= cu->nurb.first;
	while(nu) {
		/* as we want to avoid the seam in a cyclic nurbs
		texture wrapping, reserve extra orco data space to save these extra needed
		vertex based UV coordinates for the meridian vertices.
		Vertices on the 0/2pi boundary are not duplicated inside the displist but later in
		the renderface/vert construction.
		
		See also convertblender.c: init_render_surf()
		*/
		
		sizeu = nu->pntsu*nu->resolu; 
		sizev = nu->pntsv*nu->resolv;
		if (nu->flagu & CU_CYCLIC) sizeu++;
		if (nu->flagv & CU_CYCLIC) sizev++;
 		if(nu->pntsv>1) tot+= sizeu * sizev;
		
		nu= nu->next;
	}
	/* makeNurbfaces wants zeros */
	fp= coord_array= MEM_callocN(3*sizeof(float)*tot, "make_orco");
	
	nu= cu->nurb.first;
	while(nu) {
		if(nu->pntsv>1) {
			sizeu = nu->pntsu*nu->resolu; 
			sizev = nu->pntsv*nu->resolv;
			if (nu->flagu & CU_CYCLIC) sizeu++;
			if (nu->flagv & CU_CYCLIC) sizev++;
			
			if(cu->flag & CU_UV_ORCO) {
				for(b=0; b< sizeu; b++) {
					for(a=0; a< sizev; a++) {
						
						if(sizev <2) fp[0]= 0.0f;
						else fp[0]= -1.0f + 2.0f*((float)a)/(sizev - 1);
						
						if(sizeu <2) fp[1]= 0.0f;
						else fp[1]= -1.0f + 2.0f*((float)b)/(sizeu - 1);
						
						fp[2]= 0.0;
						
						fp+= 3;
					}
				}
			}
			else {
				float *_tdata= MEM_callocN((nu->pntsu*nu->resolu) * (nu->pntsv*nu->resolv) *3*sizeof(float), "temp data");
				float *tdata= _tdata;
				
				makeNurbfaces(nu, tdata, 0);
				
				for(b=0; b<sizeu; b++) {
					int use_b= b;
					if (b==sizeu-1 && (nu->flagu & CU_CYCLIC))
						use_b= 0;
					
					for(a=0; a<sizev; a++) {
						int use_a= a;
						if (a==sizev-1 && (nu->flagv & CU_CYCLIC))
							use_a= 0;
						
						tdata = _tdata + 3 * (use_b * (nu->pntsv*nu->resolv) + use_a);
						
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
float *make_orco_curve(Scene *scene, Object *ob)
{
	Curve *cu = ob->data;
	DispList *dl;
	int u, v, numVerts;
	float *fp, *coord_array;
	int remakeDisp = 0;

	if (!(cu->flag&CU_UV_ORCO) && cu->key && cu->key->block.first) {
		makeDispListCurveTypes(scene, ob, 1);
		remakeDisp = 1;
	}

	/* Assumes displist has been built */

	numVerts = 0;
	for (dl=cu->disp.first; dl; dl=dl->next) {
		if (dl->type==DL_INDEX3) {
			numVerts += dl->nr;
		} else if (dl->type==DL_SURF) {
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
	for (dl=cu->disp.first; dl; dl=dl->next) {
		if (dl->type==DL_INDEX3) {
			for (u=0; u<dl->nr; u++, fp+=3) {
				if (cu->flag & CU_UV_ORCO) {
					fp[0]= 2.0f*u/(dl->nr-1) - 1.0f;
					fp[1]= 0.0;
					fp[2]= 0.0;
				} else {
					VECCOPY(fp, &dl->verts[u*3]);

					fp[0]= (fp[0]-cu->loc[0])/cu->size[0];
					fp[1]= (fp[1]-cu->loc[1])/cu->size[1];
					fp[2]= (fp[2]-cu->loc[2])/cu->size[2];
				}
			}
		} else if (dl->type==DL_SURF) {
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
						fp[0]= 2.0f*u/(dl->parts-1) - 1.0f;
						fp[1]= 2.0f*v/(dl->nr-1) - 1.0f;
						fp[2]= 0.0;
					} else {
						float *vert;
						int realv= v % dl->nr;
						int realu= u % dl->parts;
						
						vert= dl->verts + 3*(dl->nr*realu + realv);
						VECCOPY(fp, vert);

						fp[0]= (fp[0]-cu->loc[0])/cu->size[0];
						fp[1]= (fp[1]-cu->loc[1])/cu->size[1];
						fp[2]= (fp[2]-cu->loc[2])/cu->size[2];
					}
				}
			}
		}
	}

	if (remakeDisp) {
		makeDispListCurveTypes(scene, ob, 0);
	}

	return coord_array;
}


/* ***************** BEVEL ****************** */

void makebevelcurve(Scene *scene, Object *ob, ListBase *disp)
{
	DispList *dl, *dlnew;
	Curve *bevcu, *cu;
	float *fp, facx, facy, angle, dangle;
	int nr, a;

	cu= ob->data;
	disp->first = disp->last = NULL;

	/* if a font object is being edited, then do nothing */
// XXX	if( ob == obedit && ob->type == OB_FONT ) return;

	if(cu->bevobj && cu->bevobj!=ob) {
		if(cu->bevobj->type==OB_CURVE) {
			bevcu= cu->bevobj->data;
			if(bevcu->ext1==0.0 && bevcu->ext2==0.0) {
				facx= cu->bevobj->size[0];
				facy= cu->bevobj->size[1];

				dl= bevcu->disp.first;
				if(dl==0) {
					makeDispListCurveTypes(scene, cu->bevobj, 0);
					dl= bevcu->disp.first;
				}
				while(dl) {
					if ELEM(dl->type, DL_POLY, DL_SEGM) {
						dlnew= MEM_mallocN(sizeof(DispList), "makebevelcurve1");					
						*dlnew= *dl;
						dlnew->verts= MEM_mallocN(3*sizeof(float)*dl->parts*dl->nr, "makebevelcurve1");
						memcpy(dlnew->verts, dl->verts, 3*sizeof(float)*dl->parts*dl->nr);
						
						if(dlnew->type==DL_SEGM) dlnew->flag |= (DL_FRONT_CURVE|DL_BACK_CURVE);
						
						BLI_addtail(disp, dlnew);
						fp= dlnew->verts;
						nr= dlnew->parts*dlnew->nr;
						while(nr--) {
							fp[2]= fp[1]*facy;
							fp[1]= -fp[0]*facx;
							fp[0]= 0.0;
							fp+= 3;
						}
					}
					dl= dl->next;
				}
			}
		}
	}
	else if(cu->ext1==0.0 && cu->ext2==0.0) {
		;
	}
	else if(cu->ext2==0.0) {
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
	else if( (cu->flag & (CU_FRONT|CU_BACK))==0 && cu->ext1==0.0f)	{ // we make a full round bevel in that case
		
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
		dangle= (2.0f*M_PI/(nr));
		angle= -(nr-1)*dangle;
		
		for(a=0; a<nr; a++) {
			fp[0]= 0.0;
			fp[1]= (float)(cos(angle)*(cu->ext2));
			fp[2]= (float)(sin(angle)*(cu->ext2)) - cu->ext1;
			angle+= dangle;
			fp+= 3;
		}
	}
	else {
		short dnr;
		
		/* bevel now in three parts, for proper vertex normals */
		/* part 1 */
		dnr= nr= 2+ cu->bevresol;
		if( (cu->flag & (CU_FRONT|CU_BACK))==0)
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
		
		for(a=0; a<nr; a++) {
			fp[0]= 0.0;
			fp[1]= (float)(cos(angle)*(cu->ext2));
			fp[2]= (float)(sin(angle)*(cu->ext2)) - cu->ext1;
			angle+= dangle;
			fp+= 3;
		}
		
		/* part 2, sidefaces */
		if(cu->ext1!=0.0) {
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
			
			if( (cu->flag & (CU_FRONT|CU_BACK))==0) {
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
		
		/* part 3 */
		dnr= nr= 2+ cu->bevresol;
		if( (cu->flag & (CU_FRONT|CU_BACK))==0)
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
		
		for(a=0; a<nr; a++) {
			fp[0]= 0.0;
			fp[1]= (float)(cos(angle)*(cu->ext2));
			fp[2]= (float)(sin(angle)*(cu->ext2)) + cu->ext1;
			angle+= dangle;
			fp+= 3;
		}
	}
}

static int cu_isectLL(float *v1, float *v2, float *v3, float *v4, short cox, short coy, float *labda, float *mu, float *vec)
{
	/* return:
		-1: colliniar
		 0: no intersection of segments
		 1: exact intersection of segments
		 2: cross-intersection of segments
	*/
	float deler;

	deler= (v1[cox]-v2[cox])*(v3[coy]-v4[coy])-(v3[cox]-v4[cox])*(v1[coy]-v2[coy]);
	if(deler==0.0) return -1;

	*labda= (v1[coy]-v3[coy])*(v3[cox]-v4[cox])-(v1[cox]-v3[cox])*(v3[coy]-v4[coy]);
	*labda= -(*labda/deler);

	deler= v3[coy]-v4[coy];
	if(deler==0) {
		deler=v3[cox]-v4[cox];
		*mu= -(*labda*(v2[cox]-v1[cox])+v1[cox]-v3[cox])/deler;
	} else {
		*mu= -(*labda*(v2[coy]-v1[coy])+v1[coy]-v3[coy])/deler;
	}
	vec[cox]= *labda*(v2[cox]-v1[cox])+v1[cox];
	vec[coy]= *labda*(v2[coy]-v1[coy])+v1[coy];

	if(*labda>=0.0 && *labda<=1.0 && *mu>=0.0 && *mu<=1.0) {
		if(*labda==0.0 || *labda==1.0 || *mu==0.0 || *mu==1.0) return 1;
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
	VECCOPY(hvec2,hvec1);
	hvec2[0]+=1000;

	/* test it with all edges of potential surounding poly */
	/* count number of transitions left-right  */

	bevp= (BevPoint *)(bl1+1);
	nr= bl1->nr;
	prevbevp= bevp+(nr-1);

	while(nr--) {
		min= prevbevp->vec[0];
		max= bevp->vec[1];
		if(max<min) {
			min= max;
			max= prevbevp->vec[1];
		}
		if(min!=max) {
			if(min<=hvec1[1] && max>=hvec1[1]) {
				/* there's a transition, calc intersection point */
				mode= cu_isectLL(prevbevp->vec, bevp->vec, hvec1, hvec2, 0, 1, &lab, &mu, vec);
				/* if lab==0.0 or lab==1.0 then the edge intersects exactly a transition
			           only allow for one situation: we choose lab= 1.0
				 */
				if(mode>=0 && lab!=0.0) {
					if(vec[0]<hvec1[0]) links++;
					else rechts++;
				}
			}
		}
		prevbevp= bevp;
		bevp++;
	}
	
	if( (links & 1) && (rechts & 1) ) return 1;
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

	if( x1->left > x2->left ) return 1;
	else if( x1->left < x2->left) return -1;
	return 0;
}

/* this function cannot be replaced with atan2, but why? */

static void calc_bevel_sin_cos(float x1, float y1, float x2, float y2, float *sina, float *cosa)
{
	float t01, t02, x3, y3;

	t01= (float)sqrt(x1*x1+y1*y1);
	t02= (float)sqrt(x2*x2+y2*y2);
	if(t01==0.0) t01= 1.0;
	if(t02==0.0) t02= 1.0;

	x1/=t01; 
	y1/=t01;
	x2/=t02; 
	y2/=t02;

	t02= x1*x2+y1*y2;
	if(fabs(t02)>=1.0) t02= .5*M_PI;
	else t02= (saacos(t02))/2.0f;

	t02= (float)sin(t02);
	if(t02==0.0) t02= 1.0;

	x3= x1-x2;
	y3= y1-y2;
	if(x3==0 && y3==0) {
		x3= y1;
		y3= -x1;
	} else {
		t01= (float)sqrt(x3*x3+y3*y3);
		x3/=t01; 
		y3/=t01;
	}

	*sina= -y3/t02;
	*cosa= x3/t02;

}

static void alfa_bezpart(BezTriple *prevbezt, BezTriple *bezt, Nurb *nu, float *tilt_array, float *radius_array, int resolu, int stride)
{
	BezTriple *pprev, *next, *last;
	float fac, dfac, t[4];
	int a;
	
	if(tilt_array==NULL && radius_array==NULL)
		return;
	
	last= nu->bezt+(nu->pntsu-1);
	
	/* returns a point */
	if(prevbezt==nu->bezt) {
		if(nu->flagu & CU_CYCLIC) pprev= last;
		else pprev= prevbezt;
	}
	else pprev= prevbezt-1;
	
	/* next point */
	if(bezt==last) {
		if(nu->flagu & CU_CYCLIC) next= nu->bezt;
		else next= bezt;
	}
	else next= bezt+1;
	
	fac= 0.0;
	dfac= 1.0f/(float)resolu;
	
	for(a=0; a<resolu; a++, fac+= dfac) {
		if (tilt_array) {
			if (nu->tilt_interp==3) { /* May as well support for tilt also 2.47 ease interp */
				*tilt_array = prevbezt->alfa + (bezt->alfa - prevbezt->alfa)*(3.0f*fac*fac - 2.0f*fac*fac*fac);
			} else {
				key_curve_position_weights(fac, t, nu->tilt_interp);
				*tilt_array= t[0]*pprev->alfa + t[1]*prevbezt->alfa + t[2]*bezt->alfa + t[3]*next->alfa;
			}
			
			tilt_array = (float *)(((char *)tilt_array) + stride); 
		}
		
		if (radius_array) {
			if (nu->radius_interp==3) {
				/* Support 2.47 ease interp
				 * Note! - this only takes the 2 points into account,
				 * giving much more localized results to changes in radius, sometimes you want that */
				*radius_array = prevbezt->radius + (bezt->radius - prevbezt->radius)*(3.0f*fac*fac - 2.0f*fac*fac*fac);
			} else {
				
				/* reuse interpolation from tilt if we can */
				if (tilt_array==NULL || nu->tilt_interp != nu->radius_interp) {
					key_curve_position_weights(fac, t, nu->radius_interp);
				}
				*radius_array= t[0]*pprev->radius + t[1]*prevbezt->radius + t[2]*bezt->radius + t[3]*next->radius;
			}
			
			radius_array = (float *)(((char *)radius_array) + stride); 
		}
	}
}

/* make_bevel_list_3D_* funcs, at a minimum these must
 * fill in the bezp->quat and bezp->dir values */

/* correct non-cyclic cases by copying direction and rotation
 * values onto the first & last end-points */
static void bevel_list_cyclic_fix(BevList *bl)
{
	BevPoint *bevp, *bevp1;

	bevp= (BevPoint *)(bl+1);
	bevp1= bevp+1;
	QUATCOPY(bevp->quat, bevp1->quat);
	VECCOPY(bevp->dir, bevp1->dir);
	VECCOPY(bevp->tan, bevp1->tan);
	bevp= (BevPoint *)(bl+1);
	bevp+= (bl->nr-1);
	bevp1= bevp-1;
	QUATCOPY(bevp->quat, bevp1->quat);
	VECCOPY(bevp->dir, bevp1->dir);
	VECCOPY(bevp->tan, bevp1->tan);
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
	while(nr--) {
		/* totally simple */
		VecBisect3(bevp1->dir, bevp0->vec, bevp1->vec, bevp2->vec);

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
	while(nr--) {
		if(RAD2DEG(VecAngle2(bevp0->tan, bevp1->tan)) > 90)
			VecNegf(bevp1->tan);

		bevp0= bevp1;
		bevp1= bevp2;
		bevp2++;
	}
}
/* apply user tilt */
static void bevel_list_apply_tilt(BevList *bl)
{
	BevPoint *bevp2, *bevp1, *bevp0;
	int nr;
	float q[4];

	bevp2= (BevPoint *)(bl+1);
	bevp1= bevp2+(bl->nr-1);
	bevp0= bevp1-1;

	nr= bl->nr;
	while(nr--) {
		AxisAngleToQuat(q, bevp1->dir, bevp1->alfa);
		QuatMul(bevp1->quat, q, bevp1->quat);
		NormalQuat(bevp1->quat);

		bevp0= bevp1;
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

	for(a=0; a < smooth_iter; a++) {

		bevp2= (BevPoint *)(bl+1);
		bevp1= bevp2+(bl->nr-1);
		bevp0= bevp1-1;

		nr= bl->nr;

		if(bl->poly== -1) { /* check its not cyclic */
			/* skip the first point */
			bevp0= bevp1;
			bevp1= bevp2;
			bevp2++;
			nr--;

			bevp0= bevp1;
			bevp1= bevp2;
			bevp2++;
			nr--;

		}

		QUATCOPY(bevp0_quat, bevp0->quat);

		while(nr--) {
			/* interpolate quats */
			float zaxis[3] = {0,0,1}, cross[3], q2[4];
			QuatInterpol(q, bevp0_quat, bevp2->quat, 0.5);
			NormalQuat(q);

			QuatMulVecf(q, zaxis);
			Crossf(cross, zaxis, bevp1->dir);
			AxisAngleToQuat(q2, cross, NormalizedVecAngle2(zaxis, bevp1->dir));
			NormalQuat(q2);

			QUATCOPY(bevp0_quat, bevp1->quat);
			QuatMul(q, q2, q);
			QuatInterpol(bevp1->quat, bevp1->quat, q, 0.5);
			NormalQuat(bevp1->quat);


			bevp0= bevp1;
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
	while(nr--) {
		/* totally simple */
		VecBisect3(bevp1->dir, bevp0->vec, bevp1->vec, bevp2->vec);
		vectoquat(bevp1->dir, 5, 1, bevp1->quat);

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

	float cross_tmp[3];

	bevel_list_calc_bisect(bl);

	bevp2= (BevPoint *)(bl+1);
	bevp1= bevp2+(bl->nr-1);
	bevp0= bevp1-1;

	nr= bl->nr;
	while(nr--) {

		if(nr+4 > bl->nr) { /* first time and second time, otherwise first point adjusts last */
			vectoquat(bevp1->dir, 5, 1, bevp1->quat);
		}
		else {
			float angle= NormalizedVecAngle2(bevp0->dir, bevp1->dir);

			if(angle > 0.0f) { /* otherwise we can keep as is */
				Crossf(cross_tmp, bevp0->dir, bevp1->dir);
				AxisAngleToQuat(q, cross_tmp, angle);
				QuatMul(bevp1->quat, q, bevp0->quat);
			}
			else {
				QUATCOPY(bevp1->quat, bevp0->quat);
			}
		}

		bevp0= bevp1;
		bevp1= bevp2;
		bevp2++;
	}

	if(bl->poly != -1) { /* check for cyclic */

		/* Need to correct for the start/end points not matching
		 * do this by calculating the tilt angle difference, then apply
		 * the rotation gradually over the entire curve
		 *
		 * note that the split is between last and second last, rather then first/last as youd expect.
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
		QuatMulVecf(bevp_first->quat, vec_1);
		QuatMulVecf(bevp_last->quat, vec_2);
		Normalize(vec_1);
		Normalize(vec_2);

		/* align the vector, can avoid this and it looks 98% OK but
		 * better to align the angle quat roll's before comparing */
		{
			Crossf(cross_tmp, bevp_last->dir, bevp_first->dir);
			angle = NormalizedVecAngle2(bevp_first->dir, bevp_last->dir);
			AxisAngleToQuat(q, cross_tmp, angle);
			QuatMulVecf(q, vec_2);
		}

		angle= NormalizedVecAngle2(vec_1, vec_2);

		/* flip rotation if needs be */
		Crossf(cross_tmp, vec_1, vec_2);
		Normalize(cross_tmp);
		if(NormalizedVecAngle2(bevp_first->dir, cross_tmp) < 90/(180.0/M_PI))
			angle = -angle;

		bevp2= (BevPoint *)(bl+1);
		bevp1= bevp2+(bl->nr-1);
		bevp0= bevp1-1;

		nr= bl->nr;
		while(nr--) {
			ang_fac= angle * (1.0f-((float)nr/bl->nr)); /* also works */

			AxisAngleToQuat(q, bevp1->dir, ang_fac);
			QuatMul(bevp1->quat, q, bevp1->quat);

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
	if(bl->poly== -1) /* check its not cyclic */
		bevel_list_cyclic_fix(bl); // XXX - run this now so tangents will be right before doing the flipping
	bevel_list_flip_tangents(bl);

	/* correct the tangents */
	bevp2= (BevPoint *)(bl+1);
	bevp1= bevp2+(bl->nr-1);
	bevp0= bevp1-1;

	nr= bl->nr;
	while(nr--) {

		Crossf(cross_tmp, bevp1->tan, bevp1->dir);
		Crossf(bevp1->tan, cross_tmp, bevp1->dir);
		Normalize(bevp1->tan);

		bevp0= bevp1;
		bevp1= bevp2;
		bevp2++;
	}


	/* now for the real twist calc */
	bevp2= (BevPoint *)(bl+1);
	bevp1= bevp2+(bl->nr-1);
	bevp0= bevp1-1;

	VECCOPY(bevp0_tan, bevp0->tan);

	nr= bl->nr;
	while(nr--) {

		/* make perpendicular, modify tan in place, is ok */
		float cross_tmp[3];
		float zero[3] = {0,0,0};

		Crossf(cross_tmp, bevp1->tan, bevp1->dir);
		Normalize(cross_tmp);
		triatoquat(zero, cross_tmp, bevp1->tan, bevp1->quat); /* XXX - could be faster */

		bevp0= bevp1;
		bevp1= bevp2;
		bevp2++;
	}
}

static void make_bevel_list_3D(BevList *bl, int smooth_iter, int twist_mode)
{
	switch(twist_mode) {
	case CU_TWIST_TANGENT:
		make_bevel_list_3D_tangent(bl);
		break;
	case CU_TWIST_MINIMUM:
		make_bevel_list_3D_minimum_twist(bl);
		break;
	default: /* CU_TWIST_Z_UP default, pre 2.49c */
		make_bevel_list_3D_zup(bl);
	}

	if(bl->poly== -1) /* check its not cyclic */
		bevel_list_cyclic_fix(bl);

	if(smooth_iter)
		bevel_list_smooth(bl, smooth_iter);

	bevel_list_apply_tilt(bl);
}



void makeBevelList(Object *ob)
{
	/*
	 - convert all curves to polys, with indication of resol and flags for double-vertices
	 - possibly; do a smart vertice removal (in case Nurb)
	 - separate in individual blicks with BoundBox
	 - AutoHole detection
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
	int do_tilt, do_radius;
	
	/* this function needs an object, because of tflag and upflag */
	cu= ob->data;

	/* do we need to calculate the radius for each point? */
	/* do_radius = (cu->bevobj || cu->taperobj || (cu->flag & CU_FRONT) || (cu->flag & CU_BACK)) ? 0 : 1; */
	
	/* STEP 1: MAKE POLYS  */

	BLI_freelistN(&(cu->bev));
	if(cu->editnurb && ob->type!=OB_FONT) nu= cu->editnurb->first;
	else nu= cu->nurb.first;
	
	while(nu) {
		
		/* check if we will calculate tilt data */
		do_tilt = CU_DO_TILT(cu, nu);
		do_radius = CU_DO_RADIUS(cu, nu); /* normal display uses the radius, better just to calculate them */
		
		/* check we are a single point? also check we are not a surface and that the orderu is sane,
		 * enforced in the UI but can go wrong possibly */
		if(!check_valid_nurb_u(nu)) {
			bl= MEM_callocN(sizeof(BevList)+1*sizeof(BevPoint), "makeBevelList1");
			BLI_addtail(&(cu->bev), bl);
			bl->nr= 0;
		} else {
			if(G.rendering && cu->resolu_ren!=0) 
				resolu= cu->resolu_ren;
			else
				resolu= nu->resolu;
			
			if(nu->type == CU_POLY) {
				len= nu->pntsu;
				bl= MEM_callocN(sizeof(BevList)+len*sizeof(BevPoint), "makeBevelList2");
				BLI_addtail(&(cu->bev), bl);
	
				if(nu->flagu & CU_CYCLIC) bl->poly= 0;
				else bl->poly= -1;
				bl->nr= len;
				bl->dupe_nr= 0;
				bevp= (BevPoint *)(bl+1);
				bp= nu->bp;
	
				while(len--) {
					VECCOPY(bevp->vec, bp->vec);
					bevp->alfa= bp->alfa;
					bevp->radius= bp->radius;
					bevp->split_tag= TRUE;
					bevp++;
					bp++;
				}
			}
			else if(nu->type == CU_BEZIER) {
	
				len= resolu*(nu->pntsu+ (nu->flagu & CU_CYCLIC) -1)+1;	/* in case last point is not cyclic */
				bl= MEM_callocN(sizeof(BevList)+len*sizeof(BevPoint), "makeBevelBPoints");
				BLI_addtail(&(cu->bev), bl);
	
				if(nu->flagu & CU_CYCLIC) bl->poly= 0;
				else bl->poly= -1;
				bevp= (BevPoint *)(bl+1);
	
				a= nu->pntsu-1;
				bezt= nu->bezt;
				if(nu->flagu & CU_CYCLIC) {
					a++;
					prevbezt= nu->bezt+(nu->pntsu-1);
				}
				else {
					prevbezt= bezt;
					bezt++;
				}
				
				while(a--) {
					if(prevbezt->h2==HD_VECT && bezt->h1==HD_VECT) {

						VECCOPY(bevp->vec, prevbezt->vec[1]);
						bevp->alfa= prevbezt->alfa;
						bevp->radius= prevbezt->radius;
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
						for(j=0; j<3; j++) {
							forward_diff_bezier(	prevbezt->vec[1][j],	prevbezt->vec[2][j],
													bezt->vec[0][j],		bezt->vec[1][j],
													&(bevp->vec[j]), resolu, sizeof(BevPoint));
						}
						
						/* if both arrays are NULL do nothiong */
						alfa_bezpart(	prevbezt, bezt, nu,
										 do_tilt	? &bevp->alfa : NULL,
										 do_radius	? &bevp->radius : NULL,
										 resolu, sizeof(BevPoint));

						
						if(cu->twist_mode==CU_TWIST_TANGENT) {
							forward_diff_bezier_cotangent(
													prevbezt->vec[1],	prevbezt->vec[2],
													bezt->vec[0],		bezt->vec[1],
													bevp->tan, resolu, sizeof(BevPoint));
						}

						/* indicate with handlecodes double points */
						if(prevbezt->h1==prevbezt->h2) {
							if(prevbezt->h1==0 || prevbezt->h1==HD_VECT) bevp->split_tag= TRUE;
						}
						else {
							if(prevbezt->h1==0 || prevbezt->h1==HD_VECT) bevp->split_tag= TRUE;
							else if(prevbezt->h2==0 || prevbezt->h2==HD_VECT) bevp->split_tag= TRUE;
						}
						bl->nr+= resolu;
						bevp+= resolu;
					}
					prevbezt= bezt;
					bezt++;
				}
				
				if((nu->flagu & CU_CYCLIC)==0) {	    /* not cyclic: endpoint */
					VECCOPY(bevp->vec, prevbezt->vec[1]);
					bevp->alfa= prevbezt->alfa;
					bevp->radius= prevbezt->radius;
					bl->nr++;
				}
			}
			else if(nu->type == CU_NURBS) {
				if(nu->pntsv==1) {
					len= (resolu*SEGMENTSU(nu));
					
					bl= MEM_callocN(sizeof(BevList)+len*sizeof(BevPoint), "makeBevelList3");
					BLI_addtail(&(cu->bev), bl);
					bl->nr= len;
					bl->dupe_nr= 0;
					if(nu->flagu & CU_CYCLIC) bl->poly= 0;
					else bl->poly= -1;
					bevp= (BevPoint *)(bl+1);
					
					makeNurbcurve(	nu, &bevp->vec[0],
									do_tilt		? &bevp->alfa : NULL,
									do_radius	? &bevp->radius : NULL,
									resolu, sizeof(BevPoint));
				}
			}
		}
		nu= nu->next;
	}

	/* STEP 2: DOUBLE POINTS AND AUTOMATIC RESOLUTION, REDUCE DATABLOCKS */
	bl= cu->bev.first;
	while(bl) {
		if (bl->nr) { /* null bevel items come from single points */
			nr= bl->nr;
			bevp1= (BevPoint *)(bl+1);
			bevp0= bevp1+(nr-1);
			nr--;
			while(nr--) {
				if( fabs(bevp0->vec[0]-bevp1->vec[0])<0.00001 ) {
					if( fabs(bevp0->vec[1]-bevp1->vec[1])<0.00001 ) {
						if( fabs(bevp0->vec[2]-bevp1->vec[2])<0.00001 ) {
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
	while(bl) {
		blnext= bl->next;
		if(bl->nr && bl->dupe_nr) {
			nr= bl->nr- bl->dupe_nr+1;	/* +1 because vectorbezier sets flag too */
			blnew= MEM_mallocN(sizeof(BevList)+nr*sizeof(BevPoint), "makeBevelList4");
			memcpy(blnew, bl, sizeof(BevList));
			blnew->nr= 0;
			BLI_remlink(&(cu->bev), bl);
			BLI_insertlinkbefore(&(cu->bev),blnext,blnew);	/* to make sure bevlijst is tuned with nurblist */
			bevp0= (BevPoint *)(bl+1);
			bevp1= (BevPoint *)(blnew+1);
			nr= bl->nr;
			while(nr--) {
				if(bevp0->dupe_tag==0) {
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
	while(bl) {
		if(bl->nr && bl->poly>=0) {
			poly++;
			bl->poly= poly;
			bl->hole= 0;
		}
		bl= bl->next;
	}
	

	/* find extreme left points, also test (turning) direction */
	if(poly>0) {
		sd= sortdata= MEM_mallocN(sizeof(struct bevelsort)*poly, "makeBevelList5");
		bl= cu->bev.first;
		while(bl) {
			if(bl->poly>0) {

				min= 300000.0;
				bevp= (BevPoint *)(bl+1);
				nr= bl->nr;
				while(nr--) {
					if(min>bevp->vec[0]) {
						min= bevp->vec[0];
						bevp1= bevp;
					}
					bevp++;
				}
				sd->bl= bl;
				sd->left= min;

				bevp= (BevPoint *)(bl+1);
				if(bevp1== bevp) bevp0= bevp+ (bl->nr-1);
				else bevp0= bevp1-1;
				bevp= bevp+ (bl->nr-1);
				if(bevp1== bevp) bevp2= (BevPoint *)(bl+1);
				else bevp2= bevp1+1;

				inp= (bevp1->vec[0]- bevp0->vec[0]) * (bevp0->vec[1]- bevp2->vec[1]) + (bevp0->vec[1]- bevp1->vec[1]) * (bevp0->vec[0]- bevp2->vec[0]);

				if(inp>0.0) sd->dir= 1;
				else sd->dir= 0;

				sd++;
			}

			bl= bl->next;
		}
		qsort(sortdata,poly,sizeof(struct bevelsort), vergxcobev);

		sd= sortdata+1;
		for(a=1; a<poly; a++, sd++) {
			bl= sd->bl;	    /* is bl a hole? */
			sd1= sortdata+ (a-1);
			for(b=a-1; b>=0; b--, sd1--) {	/* all polys to the left */
				if(bevelinside(sd1->bl, bl)) {
					bl->hole= 1- sd1->bl->hole;
					break;
				}
			}
		}

		/* turning direction */
		if((cu->flag & CU_3D)==0) {
			sd= sortdata;
			for(a=0; a<poly; a++, sd++) {
				if(sd->bl->hole==sd->dir) {
					bl= sd->bl;
					bevp1= (BevPoint *)(bl+1);
					bevp2= bevp1+ (bl->nr-1);
					nr= bl->nr/2;
					while(nr--) {
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
	if((cu->flag & CU_3D)==0) { /* 3D */
		bl= cu->bev.first;
		while(bl) {

			if(bl->nr < 2) {
				/* do nothing */
			}
			else if(bl->nr==2) {	/* 2 pnt, treat separate */
				bevp2= (BevPoint *)(bl+1);
				bevp1= bevp2+1;

				x1= bevp1->vec[0]- bevp2->vec[0];
				y1= bevp1->vec[1]- bevp2->vec[1];

				calc_bevel_sin_cos(x1, y1, -x1, -y1, &(bevp1->sina), &(bevp1->cosa));
				bevp2->sina= bevp1->sina;
				bevp2->cosa= bevp1->cosa;
			}
			else {
				bevp2= (BevPoint *)(bl+1);
				bevp1= bevp2+(bl->nr-1);
				bevp0= bevp1-1;

				nr= bl->nr;
				while(nr--) {
					x1= bevp1->vec[0]- bevp0->vec[0];
					x2= bevp1->vec[0]- bevp2->vec[0];
					y1= bevp1->vec[1]- bevp0->vec[1];
					y2= bevp1->vec[1]- bevp2->vec[1];

					calc_bevel_sin_cos(x1, y1, x2, y2, &(bevp1->sina), &(bevp1->cosa));

					bevp0= bevp1;
					bevp1= bevp2;
					bevp2++;
				}

				/* correct non-cyclic cases */
				if(bl->poly== -1) {
					bevp= (BevPoint *)(bl+1);
					bevp1= bevp+1;
					bevp->sina= bevp1->sina;
					bevp->cosa= bevp1->cosa;
					bevp= (BevPoint *)(bl+1);
					bevp+= (bl->nr-1);
					bevp1= bevp-1;
					bevp->sina= bevp1->sina;
					bevp->cosa= bevp1->cosa;
				}
			}
			bl= bl->next;
		}
	}
	else { /* 3D Curves */
		bl= cu->bev.first;
		while(bl) {

			if(bl->nr < 2) {
				/* do nothing */
			}
			else if(bl->nr==2) {	/* 2 pnt, treat separate */
				float q[4];

				bevp2= (BevPoint *)(bl+1);
				bevp1= bevp2+1;

				/* simple quat/dir */
				VecSubf(bevp1->dir, bevp1->vec, bevp2->vec);
				Normalize(bevp1->dir);
				
				vectoquat(bevp1->dir, 5, 1, bevp1->quat);
				
				AxisAngleToQuat(q, bevp1->dir, bevp1->alfa);
				QuatMul(bevp1->quat, q, bevp1->quat);
				NormalQuat(bevp1->quat);
				VECCOPY(bevp2->dir, bevp1->dir);
				QUATCOPY(bevp2->quat, bevp1->quat);
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
 *		1: nothing,  1:auto,  2:vector,  3:aligned
 */

/* mode: is not zero when FCurve, is 2 when forced horizontal for autohandles */
void calchandleNurb(BezTriple *bezt, BezTriple *prev, BezTriple *next, int mode)
{
	float *p1,*p2,*p3, pt[3];
	float dx1,dy1,dz1,dx,dy,dz,vx,vy,vz,len,len1,len2;

	if(bezt->h1==0 && bezt->h2==0) return;

	p2= bezt->vec[1];

	if(prev==0) {
		p3= next->vec[1];
		pt[0]= 2*p2[0]- p3[0];
		pt[1]= 2*p2[1]- p3[1];
		pt[2]= 2*p2[2]- p3[2];
		p1= pt;
	}
	else p1= prev->vec[1];

	if(next==0) {
		pt[0]= 2*p2[0]- p1[0];
		pt[1]= 2*p2[1]- p1[1];
		pt[2]= 2*p2[2]- p1[2];
		p3= pt;
	}
	else p3= next->vec[1];

	dx= p2[0]- p1[0];
	dy= p2[1]- p1[1];
	dz= p2[2]- p1[2];
	
	if(mode) len1= dx;
	else len1= (float)sqrt(dx*dx+dy*dy+dz*dz);
	
	dx1= p3[0]- p2[0];
	dy1= p3[1]- p2[1];
	dz1= p3[2]- p2[2];
	
	if(mode) len2= dx1;
	else len2= (float)sqrt(dx1*dx1+dy1*dy1+dz1*dz1);

	if(len1==0.0f) len1=1.0f;
	if(len2==0.0f) len2=1.0f;


	if(bezt->h1==HD_AUTO || bezt->h2==HD_AUTO) {    /* auto */
		vx= dx1/len2 + dx/len1;
		vy= dy1/len2 + dy/len1;
		vz= dz1/len2 + dz/len1;
		len= 2.5614f*(float)sqrt(vx*vx + vy*vy + vz*vz);
		if(len!=0.0f) {
			int leftviolate=0, rightviolate=0;	/* for mode==2 */
			
			if(len1>5.0f*len2) len1= 5.0f*len2;	
			if(len2>5.0f*len1) len2= 5.0f*len1;
			
			if(bezt->h1==HD_AUTO) {
				len1/=len;
				*(p2-3)= *p2-vx*len1;
				*(p2-2)= *(p2+1)-vy*len1;
				*(p2-1)= *(p2+2)-vz*len1;
				
				if(mode==2 && next && prev) {	// keep horizontal if extrema
					float ydiff1= prev->vec[1][1] - bezt->vec[1][1];
					float ydiff2= next->vec[1][1] - bezt->vec[1][1];
					if( (ydiff1<=0.0 && ydiff2<=0.0) || (ydiff1>=0.0 && ydiff2>=0.0) ) {
						bezt->vec[0][1]= bezt->vec[1][1];
					}
					else {						// handles should not be beyond y coord of two others
						if(ydiff1<=0.0) { 
							if(prev->vec[1][1] > bezt->vec[0][1]) {
								bezt->vec[0][1]= prev->vec[1][1]; 
								leftviolate= 1;
							}
						}
						else {
							if(prev->vec[1][1] < bezt->vec[0][1]) {
								bezt->vec[0][1]= prev->vec[1][1]; 
								leftviolate= 1;
							}
						}
					}
				}
			}
			if(bezt->h2==HD_AUTO) {
				len2/=len;
				*(p2+3)= *p2+vx*len2;
				*(p2+4)= *(p2+1)+vy*len2;
				*(p2+5)= *(p2+2)+vz*len2;
				
				if(mode==2 && next && prev) {	// keep horizontal if extrema
					float ydiff1= prev->vec[1][1] - bezt->vec[1][1];
					float ydiff2= next->vec[1][1] - bezt->vec[1][1];
					if( (ydiff1<=0.0 && ydiff2<=0.0) || (ydiff1>=0.0 && ydiff2>=0.0) ) {
						bezt->vec[2][1]= bezt->vec[1][1];
					}
					else {						// handles should not be beyond y coord of two others
						if(ydiff1<=0.0) { 
							if(next->vec[1][1] < bezt->vec[2][1]) {
								bezt->vec[2][1]= next->vec[1][1]; 
								rightviolate= 1;
							}
						}
						else {
							if(next->vec[1][1] > bezt->vec[2][1]) {
								bezt->vec[2][1]= next->vec[1][1]; 
								rightviolate= 1;
							}
						}
					}
				}
			}
			if(leftviolate || rightviolate) {	/* align left handle */
				float h1[3], h2[3];
				
				VecSubf(h1, p2-3, p2);
				VecSubf(h2, p2, p2+3);
				len1= Normalize(h1);
				len2= Normalize(h2);
				
				vz= INPR(h1, h2);
				
				if(leftviolate) {
					*(p2+3)= *(p2)   - vz*len2*h1[0];
					*(p2+4)= *(p2+1) - vz*len2*h1[1];
					*(p2+5)= *(p2+2) - vz*len2*h1[2];
				}
				else {
					*(p2-3)= *(p2)   + vz*len1*h2[0];
					*(p2-2)= *(p2+1) + vz*len1*h2[1];
					*(p2-1)= *(p2+2) + vz*len1*h2[2];
				}
			}
			
		}
	}

	if(bezt->h1==HD_VECT) {	/* vector */
		dx/=3.0; 
		dy/=3.0; 
		dz/=3.0;
		*(p2-3)= *p2-dx;
		*(p2-2)= *(p2+1)-dy;
		*(p2-1)= *(p2+2)-dz;
	}
	if(bezt->h2==HD_VECT) {
		dx1/=3.0; 
		dy1/=3.0; 
		dz1/=3.0;
		*(p2+3)= *p2+dx1;
		*(p2+4)= *(p2+1)+dy1;
		*(p2+5)= *(p2+2)+dz1;
	}

	len2= VecLenf(p2, p2+3);
	len1= VecLenf(p2, p2-3);
	if(len1==0.0) len1=1.0;
	if(len2==0.0) len2=1.0;

	if(bezt->f1 & SELECT) { /* order of calculation */
		if(bezt->h2==HD_ALIGN) {	/* aligned */
			len= len2/len1;
			p2[3]= p2[0]+len*(p2[0]-p2[-3]);
			p2[4]= p2[1]+len*(p2[1]-p2[-2]);
			p2[5]= p2[2]+len*(p2[2]-p2[-1]);
		}
		if(bezt->h1==HD_ALIGN) {
			len= len1/len2;
			p2[-3]= p2[0]+len*(p2[0]-p2[3]);
			p2[-2]= p2[1]+len*(p2[1]-p2[4]);
			p2[-1]= p2[2]+len*(p2[2]-p2[5]);
		}
	}
	else {
		if(bezt->h1==HD_ALIGN) {
			len= len1/len2;
			p2[-3]= p2[0]+len*(p2[0]-p2[3]);
			p2[-2]= p2[1]+len*(p2[1]-p2[4]);
			p2[-1]= p2[2]+len*(p2[2]-p2[5]);
		}
		if(bezt->h2==HD_ALIGN) {	/* aligned */
			len= len2/len1;
			p2[3]= p2[0]+len*(p2[0]-p2[-3]);
			p2[4]= p2[1]+len*(p2[1]-p2[-2]);
			p2[5]= p2[2]+len*(p2[2]-p2[-1]);
		}
	}
}

void calchandlesNurb(Nurb *nu) /* first, if needed, set handle flags */
{
	BezTriple *bezt, *prev, *next;
	short a;

	if(nu->type != CU_BEZIER) return;
	if(nu->pntsu<2) return;
	
	a= nu->pntsu;
	bezt= nu->bezt;
	if(nu->flagu & CU_CYCLIC) prev= bezt+(a-1);
	else prev= 0;
	next= bezt+1;

	while(a--) {
		calchandleNurb(bezt, prev, next, 0);
		prev= bezt;
		if(a==1) {
			if(nu->flagu & CU_CYCLIC) next= nu->bezt;
			else next= 0;
		}
		else next++;

		bezt++;
	}
}


void testhandlesNurb(Nurb *nu)
{
    /* use when something has changed with handles.
    it treats all BezTriples with the following rules:
    PHASE 1: do types have to be altered?
       Auto handles: become aligned when selection status is NOT(000 || 111)
       Vector handles: become 'nothing' when (one half selected AND other not)
    PHASE 2: recalculate handles
    */
	BezTriple *bezt;
	short flag, a;

	if(nu->type != CU_BEZIER) return;

	bezt= nu->bezt;
	a= nu->pntsu;
	while(a--) {
		flag= 0;
		if(bezt->f1 & SELECT) flag++;
		if(bezt->f2 & SELECT) flag += 2;
		if(bezt->f3 & SELECT) flag += 4;

		if( !(flag==0 || flag==7) ) {
			if(bezt->h1==HD_AUTO) {   /* auto */
				bezt->h1= HD_ALIGN;
			}
			if(bezt->h2==HD_AUTO) {   /* auto */
				bezt->h2= HD_ALIGN;
			}

			if(bezt->h1==HD_VECT) {   /* vector */
				if(flag < 4) bezt->h1= 0;
			}
			if(bezt->h2==HD_VECT) {   /* vector */
				if( flag > 3) bezt->h2= 0;
			}
		}
		bezt++;
	}

	calchandlesNurb(nu);
}

void autocalchandlesNurb(Nurb *nu, int flag)
{
	/* checks handle coordinates and calculates type */
	
	BezTriple *bezt2, *bezt1, *bezt0;
	int i, align, leftsmall, rightsmall;

	if(nu==0 || nu->bezt==0) return;
	
	bezt2 = nu->bezt;
	bezt1 = bezt2 + (nu->pntsu-1);
	bezt0 = bezt1 - 1;
	i = nu->pntsu;

	while(i--) {
		
		align= leftsmall= rightsmall= 0;
		
		/* left handle: */
		if(flag==0 || (bezt1->f1 & flag) ) {
			bezt1->h1= 0;
			/* distance too short: vectorhandle */
			if( VecLenf( bezt1->vec[1], bezt0->vec[1] ) < 0.0001) {
				bezt1->h1= HD_VECT;
				leftsmall= 1;
			}
			else {
				/* aligned handle? */
				if(DistVL2Dfl(bezt1->vec[1], bezt1->vec[0], bezt1->vec[2]) < 0.0001) {
					align= 1;
					bezt1->h1= HD_ALIGN;
				}
				/* or vector handle? */
				if(DistVL2Dfl(bezt1->vec[0], bezt1->vec[1], bezt0->vec[1]) < 0.0001)
					bezt1->h1= HD_VECT;
				
			}
		}
		/* right handle: */
		if(flag==0 || (bezt1->f3 & flag) ) {
			bezt1->h2= 0;
			/* distance too short: vectorhandle */
			if( VecLenf( bezt1->vec[1], bezt2->vec[1] ) < 0.0001) {
				bezt1->h2= HD_VECT;
				rightsmall= 1;
			}
			else {
				/* aligned handle? */
				if(align) bezt1->h2= HD_ALIGN;

				/* or vector handle? */
				if(DistVL2Dfl(bezt1->vec[2], bezt1->vec[1], bezt2->vec[1]) < 0.0001)
					bezt1->h2= HD_VECT;
				
			}
		}
		if(leftsmall && bezt1->h2==HD_ALIGN) bezt1->h2= 0;
		if(rightsmall && bezt1->h1==HD_ALIGN) bezt1->h1= 0;
		
		/* undesired combination: */
		if(bezt1->h1==HD_ALIGN && bezt1->h2==HD_VECT) bezt1->h1= 0;
		if(bezt1->h2==HD_ALIGN && bezt1->h1==HD_VECT) bezt1->h2= 0;
		
		bezt0= bezt1;
		bezt1= bezt2;
		bezt2++;
	}

	calchandlesNurb(nu);
}

void autocalchandlesNurb_all(ListBase *editnurb, int flag)
{
	Nurb *nu;
	
	nu= editnurb->first;
	while(nu) {
		autocalchandlesNurb(nu, flag);
		nu= nu->next;
	}
}

void sethandlesNurb(ListBase *editnurb, short code)
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

	if(code==1 || code==2) {
		nu= editnurb->first;
		while(nu) {
			if(nu->type == CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if((bezt->f1 & SELECT) || (bezt->f3 & SELECT)) {
						if(bezt->f1 & SELECT) bezt->h1= code;
						if(bezt->f3 & SELECT) bezt->h2= code;
						if(bezt->h1!=bezt->h2) {
							if ELEM(bezt->h1, HD_ALIGN, HD_AUTO) bezt->h1= HD_FREE;
							if ELEM(bezt->h2, HD_ALIGN, HD_AUTO) bezt->h2= HD_FREE;
						}
					}
					bezt++;
				}
				calchandlesNurb(nu);
			}
			nu= nu->next;
		}
	}
	else {
		/* there is 1 handle not FREE: FREE it all, else make ALIGNED  */
		
		nu= editnurb->first;
		if (code == 5) {
			ok = HD_ALIGN;
		} else if (code == 6) {
			ok = HD_FREE;
		} else {
			/* Toggle */
			while(nu) {
				if(nu->type == CU_BEZIER) {
					bezt= nu->bezt;
					a= nu->pntsu;
					while(a--) {
						if((bezt->f1 & SELECT) && bezt->h1) ok= 1;
						if((bezt->f3 & SELECT) && bezt->h2) ok= 1;
						if(ok) break;
						bezt++;
					}
				}
				nu= nu->next;
			}
			if(ok) ok= HD_FREE;
			else ok= HD_ALIGN;
		}
		nu= editnurb->first;
		while(nu) {
			if(nu->type == CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(bezt->f1 & SELECT) bezt->h1= ok;
					if(bezt->f3 & SELECT) bezt->h2= ok;
	
					bezt++;
				}
				calchandlesNurb(nu);
			}
			nu= nu->next;
		}
	}
}

static void swapdata(void *adr1, void *adr2, int len)
{

	if(len<=0) return;

	if(len<65) {
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

void switchdirectionNurb(Nurb *nu)
{
	BezTriple *bezt1, *bezt2;
	BPoint *bp1, *bp2;
	float *fp1, *fp2, *tempf;
	int a, b;

	if(nu->pntsu==1 && nu->pntsv==1) return;

	if(nu->type == CU_BEZIER) {
		a= nu->pntsu;
		bezt1= nu->bezt;
		bezt2= bezt1+(a-1);
		if(a & 1) a+= 1;	/* if odd, also swap middle content */
		a/= 2;
		while(a>0) {
			if(bezt1!=bezt2) SWAP(BezTriple, *bezt1, *bezt2);

			swapdata(bezt1->vec[0], bezt1->vec[2], 12);
			if(bezt1!=bezt2) swapdata(bezt2->vec[0], bezt2->vec[2], 12);

			SWAP(char, bezt1->h1, bezt1->h2);
			SWAP(short, bezt1->f1, bezt1->f3);
			
			if(bezt1!=bezt2) {
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
	else if(nu->pntsv==1) {
		a= nu->pntsu;
		bp1= nu->bp;
		bp2= bp1+(a-1);
		a/= 2;
		while(bp1!=bp2 && a>0) {
			SWAP(BPoint, *bp1, *bp2);
			a--;
			bp1->alfa= -bp1->alfa;
			bp2->alfa= -bp2->alfa;
			bp1++; 
			bp2--;
		}
		if(nu->type == CU_NURBS) {
			/* inverse knots */
			a= KNOTSU(nu);
			fp1= nu->knotsu;
			fp2= fp1+(a-1);
			a/= 2;
			while(fp1!=fp2 && a>0) {
				SWAP(float, *fp1, *fp2);
				a--;
				fp1++; 
				fp2--;
			}
			/* and make in increasing order again */
			a= KNOTSU(nu);
			fp1= nu->knotsu;
			fp2=tempf= MEM_mallocN(sizeof(float)*a, "switchdirect");
			while(a--) {
				fp2[0]= fabs(fp1[1]-fp1[0]);
				fp1++;
				fp2++;
			}
	
			a= KNOTSU(nu)-1;
			fp1= nu->knotsu;
			fp2= tempf;
			fp1[0]= 0.0;
			fp1++;
			while(a--) {
				fp1[0]= fp1[-1]+fp2[0];
				fp1++;
				fp2++;
			}
			MEM_freeN(tempf);
		}
	}
	else {
		
		for(b=0; b<nu->pntsv; b++) {
		
			bp1= nu->bp+b*nu->pntsu;
			a= nu->pntsu;
			bp2= bp1+(a-1);
			a/= 2;
			
			while(bp1!=bp2 && a>0) {
				SWAP(BPoint, *bp1, *bp2);
				a--;
				bp1++; 
				bp2--;
			}
		}
	}
}


float (*curve_getVertexCos(Curve *cu, ListBase *lb, int *numVerts_r))[3]
{
	int i, numVerts = *numVerts_r = count_curveverts(lb);
	float *co, (*cos)[3] = MEM_mallocN(sizeof(*cos)*numVerts, "cu_vcos");
	Nurb *nu;

	co = cos[0];
	for (nu=lb->first; nu; nu=nu->next) {
		if (nu->type == CU_BEZIER) {
			BezTriple *bezt = nu->bezt;

			for (i=0; i<nu->pntsu; i++,bezt++) {
				VECCOPY(co, bezt->vec[0]); co+=3;
				VECCOPY(co, bezt->vec[1]); co+=3;
				VECCOPY(co, bezt->vec[2]); co+=3;
			}
		} else {
			BPoint *bp = nu->bp;

			for (i=0; i<nu->pntsu*nu->pntsv; i++,bp++) {
				VECCOPY(co, bp->vec); co+=3;
			}
		}
	}

	return cos;
}

void curve_applyVertexCos(Curve *cu, ListBase *lb, float (*vertexCos)[3])
{
	float *co = vertexCos[0];
	Nurb *nu;
	int i;

	for (nu=lb->first; nu; nu=nu->next) {
		if (nu->type == CU_BEZIER) {
			BezTriple *bezt = nu->bezt;

			for (i=0; i<nu->pntsu; i++,bezt++) {
				VECCOPY(bezt->vec[0], co); co+=3;
				VECCOPY(bezt->vec[1], co); co+=3;
				VECCOPY(bezt->vec[2], co); co+=3;
			}
		} else {
			BPoint *bp = nu->bp;

			for (i=0; i<nu->pntsu*nu->pntsv; i++,bp++) {
				VECCOPY(bp->vec, co); co+=3;
			}
		}
	}
}

float (*curve_getKeyVertexCos(Curve *cu, ListBase *lb, float *key))[3]
{
	int i, numVerts = count_curveverts(lb);
	float *co, (*cos)[3] = MEM_mallocN(sizeof(*cos)*numVerts, "cu_vcos");
	Nurb *nu;

	co = cos[0];
	for (nu=lb->first; nu; nu=nu->next) {
		if (nu->type == CU_BEZIER) {
			BezTriple *bezt = nu->bezt;

			for (i=0; i<nu->pntsu; i++,bezt++) {
				VECCOPY(co, key); co+=3; key+=3;
				VECCOPY(co, key); co+=3; key+=3;
				VECCOPY(co, key); co+=3; key+=3;
				key++; /* skip tilt */
			}
		}
		else {
			BPoint *bp = nu->bp;

			for(i=0; i<nu->pntsu*nu->pntsv; i++,bp++) {
				VECCOPY(co, key); co+=3; key+=3;
				key++; /* skip tilt */
			}
		}
	}

	return cos;
}

void curve_applyKeyVertexTilts(Curve *cu, ListBase *lb, float *key)
{
	Nurb *nu;
	int i;

	for(nu=lb->first; nu; nu=nu->next) {
		if(nu->type == CU_BEZIER) {
			BezTriple *bezt = nu->bezt;

			for(i=0; i<nu->pntsu; i++,bezt++) {
				key+=3*3;
				bezt->alfa= *key;
				key++;
			}
		}
		else {
			BPoint *bp = nu->bp;

			for(i=0; i<nu->pntsu*nu->pntsv; i++,bp++) {
				key+=3;
				bp->alfa= *key;
				key++;
			}
		}
	}
}

int check_valid_nurb_u( struct Nurb *nu )
{
	if (nu==NULL)						return 0;
	if (nu->pntsu <= 1)					return 0;
	if (nu->type != CU_NURBS)			return 1; /* not a nurb, lets assume its valid */
	
	if (nu->pntsu < nu->orderu)			return 0;
	if (((nu->flag & CU_CYCLIC)==0) && ((nu->flagu>>1) & 2)) { /* Bezier U Endpoints */
		if (nu->orderu==4) {
			if (nu->pntsu < 5)			return 0; /* bezier with 4 orderu needs 5 points */
		} else if (nu->orderu != 3)		return 0; /* order must be 3 or 4 */
	}
	return 1;
}
int check_valid_nurb_v( struct Nurb *nu)
{
	if (nu==NULL)						return 0;
	if (nu->pntsv <= 1)					return 0;
	if (nu->type != CU_NURBS)			return 1; /* not a nurb, lets assume its valid */
	
	if (nu->pntsv < nu->orderv)			return 0;
	if (((nu->flag & CU_CYCLIC)==0) && ((nu->flagv>>1) & 2)) { /* Bezier V Endpoints */
		if (nu->orderv==4) {
			if (nu->pntsv < 5)			return 0; /* bezier with 4 orderu needs 5 points */
		} else if (nu->orderv != 3)		return 0; /* order must be 3 or 4 */
	}
	return 1;
}

int clamp_nurb_order_u( struct Nurb *nu )
{
	int change = 0;
	if(nu->pntsu<nu->orderu) {
		nu->orderu= nu->pntsu;
		change= 1;
	}
	if(((nu->flag & CU_CYCLIC)==0) && (nu->flagu>>1)&2) {
		CLAMP(nu->orderu, 3,4);
		change= 1;
	}
	return change;
}

int clamp_nurb_order_v( struct Nurb *nu)
{
	int change = 0;
	if(nu->pntsv<nu->orderv) {
		nu->orderv= nu->pntsv;
		change= 1;
	}
	if(((nu->flag & CU_CYCLIC)==0) && (nu->flagv>>1)&2) {
		CLAMP(nu->orderv, 3,4);
		change= 1;
	}
	return change;
}



