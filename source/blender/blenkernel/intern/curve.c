
/*  curve.c      MIXED MODEL
 * 
 *  maart 95
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

#define STRUBI hack

#include <math.h>  // floor
#include <string.h>
#include <stdlib.h>  

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"  
#include "BLI_arithb.h"  

#include "DNA_object_types.h"  
#include "DNA_curve_types.h"  
#include "DNA_material_types.h"  
#include "DNA_mesh_types.h"

/* for dereferencing pointers */
#include "DNA_ID.h"  
#include "DNA_vfont_types.h"  
#include "DNA_key_types.h"  
#include "DNA_ipo_types.h"  

#include "BKE_global.h" 
#include "BKE_main.h"  
#include "BKE_utildefines.h"  // VECCOPY
#include "BKE_object.h"  
#include "BKE_mesh.h" 
#include "BKE_curve.h"  
#include "BKE_displist.h"  
#include "BKE_ipo.h"  
#include "BKE_anim.h"  
#include "BKE_library.h"  
#include "BKE_key.h"  


/* globals */

extern ListBase editNurb;  /* editcurve.c */

/* local */
int cu_isectLL(float *v1, float *v2, float *v3, float *v4, 
			   short cox, short coy, 
			   float *labda, float *mu, float *vec);


#ifdef STRUBI
/* hotfix; copies x*y array into extended (x+dx)*(y+dy) array
old[] and new[] can be the same ! */
int copyintoExtendedArray(float *old, int oldx, int oldy, float *new, int newx, int newy)
{
	int x, y, ttt, ooo;
	float *oldp, *newp;
        
	if (newx < oldx || newy < oldy) return 0;
	
		
	for (y = newy - 1; y >= oldy; y--) {	
                ttt = y * newx;
		for (x = newx - 1; x >= 0; x--) {
			newp = new + 3 * (ttt + x);
			newp[0] = 0.0; newp[1] = 0.0; newp[2] = 0.0;
		}
	}	

	for (; y >= 0; y--) {
		ttt = y * newx;
                ooo = y * oldx;
		for (x = newx - 1; x >= oldx; x--) {	
			newp = new + 3 * (ttt + x);
			newp[0] = 0.0; newp[1] = 0.0; newp[2] = 0.0;
		}
		for (; x  >= 0; x--) {
			oldp = old + 3 * (ooo + x);
			newp = new + 3 * (ttt + x);
			VECCOPY(newp, oldp);
		}
	}
	return 1;
}
#endif

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
	if(cu->ipo) cu->ipo->id.us--;
	cu->ipo= 0;
}


/* niet curve zelf vrijgeven */
void free_curve(Curve *cu)
{

	freeNurblist(&cu->nurb);
	BLI_freelistN(&cu->bev);
	freedisplist(&cu->disp);
	
	unlink_curve(cu);
	
	if(cu->mat) MEM_freeN(cu->mat);
	if(cu->str) MEM_freeN(cu->str);
	if(cu->bb) MEM_freeN(cu->bb);
	if(cu->path) free_path(cu->path);
}

Curve *add_curve(int type)
{
	Curve *cu;
	char *str;
	
	if(type==OB_CURVE) str= "Curve";
	else if(type==OB_SURF) str= "Surf";
	else str= "Text";

	cu= alloc_libblock(&G.main->curve, ID_CU, str);
	
	cu->size[0]= cu->size[1]= cu->size[2]= 1.0;
	cu->flag= CU_FRONT+CU_BACK;
	cu->pathlen= 100;
	cu->resolu= cu->resolv= 6;
	cu->width= 1.0;
	cu->spacing= cu->linedist= 1.0;
	cu->fsize= 1.0;
	cu->texflag= AUTOSPACE;
	
	cu->bb= unit_boundbox();
	
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
	cun->bb= MEM_dupallocN(cu->bb);
	
	cun->key= copy_key(cu->key);
	if(cun->key) cun->key->from= (ID *)cun;
	
	cun->disp.first= cun->disp.last= 0;
	cun->bev.first= cun->bev.last= 0;
	cun->path= 0;

	/* ook single user ipo */
	if(cun->ipo) cun->ipo= copy_ipo(cun->ipo);

	id_us_plus((ID *)cun->vfont);
	
	return cun;
}

void make_local_curve(Curve *cu)
{
	Object *ob = 0;
	Curve *cun;
	int local=0, lib=0;
	
	/* - zijn er alleen lib users: niet doen
	 * - zijn er alleen locale users: flag zetten
	 * - mixed: copy
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


void test_curve_type(Object *ob)
{
	Nurb *nu;
	Curve *cu;
	
	cu= ob->data;
	if(cu->vfont) {
		ob->type= OB_FONT;
		return;
	}
	else {
		nu= cu->nurb.first;
		while(nu) {
			if(nu->pntsv>1) {
				ob->type= OB_SURF;
				return;
			}
			nu= nu->next;
		}
	}
	ob->type= OB_CURVE;
}

void tex_space_curve(Curve *cu)
{
	DispList *dl;
	BoundBox *bb;
	float *data, min[3], max[3], loc[3], size[3];
	int tot, doit= 0;
	
	if(cu->bb==0) cu->bb= MEM_callocN(sizeof(BoundBox), "boundbox");
	bb= cu->bb;
	
	INIT_MINMAX(min, max);

	dl= cu->disp.first;
	while(dl) {
		
		if(dl->type==DL_INDEX3 || dl->type==DL_INDEX3) tot= dl->nr;
		else tot= dl->nr*dl->parts;
		
		if(tot) doit= 1;
		data= dl->verts;
		while(tot--) {
			DO_MINMAX(data, min, max);
			data+= 3;
		}
		dl= dl->next;
	}

	if(doit) {
		loc[0]= (min[0]+max[0])/2.0f;
		loc[1]= (min[1]+max[1])/2.0f;
		loc[2]= (min[2]+max[2])/2.0f;
		
		size[0]= (max[0]-min[0])/2.0f;
		size[1]= (max[1]-min[1])/2.0f;
		size[2]= (max[2]-min[2])/2.0f;
	}
	else {
		loc[0]= loc[1]= loc[2]= 0.0f;
		size[0]= size[1]= size[2]= 1.0f;
	}
	
	bb->vec[0][0]=bb->vec[1][0]=bb->vec[2][0]=bb->vec[3][0]= loc[0]-size[0];
	bb->vec[4][0]=bb->vec[5][0]=bb->vec[6][0]=bb->vec[7][0]= loc[0]+size[0];
	
	bb->vec[0][1]=bb->vec[1][1]=bb->vec[4][1]=bb->vec[5][1]= loc[1]-size[1];
	bb->vec[2][1]=bb->vec[3][1]=bb->vec[6][1]=bb->vec[7][1]= loc[1]+size[1];

	bb->vec[0][2]=bb->vec[3][2]=bb->vec[4][2]=bb->vec[7][2]= loc[2]-size[2];
	bb->vec[1][2]=bb->vec[2][2]=bb->vec[5][2]=bb->vec[6][2]= loc[2]+size[2];

	if(cu->texflag & AUTOSPACE) {
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



/* **************** NURBS ROUTINES ******************** */

void freeNurb(Nurb *nu)
{

	if(nu==0) return;

	if(nu->bezt) MEM_freeN(nu->bezt);
	nu->bezt= 0;
	if(nu->bp) MEM_freeN(nu->bp);
	nu->bp= 0;
	if(nu->knotsu) MEM_freeN(nu->knotsu);
	nu->knotsu= 0;
	if(nu->knotsv) MEM_freeN(nu->knotsv);
	nu->knotsv= 0;
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
		
		newnu->knotsu=newnu->knotsv= 0;
		
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

	if( nu->type== CU_BEZIER+CU_2D ) {
		a= nu->pntsu;
		bezt= nu->bezt;
		while(a--) {
			bezt->vec[0][2]= 0.0; 
			bezt->vec[1][2]= 0.0; 
			bezt->vec[2][2]= 0.0;
			bezt++;
		}
	}
	else if(nu->type & CU_2D) {
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

	if( (nu->type & 7)==CU_BEZIER ) {
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

/* ~~~~~~~~~~~~~~~~~~~~Non Uniform Rational B Spline berekeningen ~~~~~~~~~~~ */


/* voor de goede orde: eigenlijk horen hier doubles gebruikt te worden */

void extend_spline(float * pnts, int in, int out)
{
	float *_pnts;
	double * add;
	int i, j, k, in2;

	_pnts = pnts;
	add = (double*)MEM_mallocN((in)* sizeof(double), "extend_spline");

        in2 = in -1;

	for (k = 3; k > 0; k--){
		pnts = _pnts;

		/* punten kopieren naar add */
		for (i = 0; i < in; i++){
			add[i] = *pnts;
			pnts += 3;
		}

		/* inverse forward differencen */
		for (i = 0; i < in2; i++){
			for (j = in2; j > i; j--){
				add[j] -= add[j - 1];
			}
		}

		pnts = _pnts;
		for (i = out; i > 0; i--){
			*pnts = (float)(add[0]);
			pnts += 3;
			for (j = 0; j < in2; j++){
				add[j] += add[j+1];
			}
		}

		_pnts++;
	}

	MEM_freeN(add);
}


void calcknots(float *knots, short aantal, short order, short type)
/* knots: aantal pnts NIET gecorrigeerd voor cyclic */
/* aantal, order, type;	 0: uniform, 1: endpoints, 2: bezier */
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
	}
}

void makecyclicknots(float *knots, short pnts, short order)
/* pnts, order: aantal pnts NIET gecorrigeerd voor cyclic */
{
	int a, b, order2, c;

	if(knots==0) return;
        order2=order-1;

	/* eerst lange rijen (order -1) dezelfde knots aan uiteinde verwijderen */
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


void makeknots(Nurb *nu, short uv, short type)	/* 0: uniform, 1: endpoints, 2: bezier */
{
	if( (nu->type & 7)==CU_NURBS ) {
		if(uv & 1) {
			if(nu->knotsu) MEM_freeN(nu->knotsu);
			if(nu->pntsu>1) {
				nu->knotsu= MEM_callocN(4+sizeof(float)*KNOTSU(nu), "makeknots");
				calcknots(nu->knotsu, nu->pntsu, nu->orderu, type);
				if(nu->flagu & 1) makecyclicknots(nu->knotsu, nu->pntsu, nu->orderu);
			}
			else nu->knotsu= 0;
		}
		if(uv & 2) {
			if(nu->knotsv) MEM_freeN(nu->knotsv);
			if(nu->pntsv>1) {
				nu->knotsv= MEM_callocN(4+sizeof(float)*KNOTSV(nu), "makeknots");
				calcknots(nu->knotsv, nu->pntsv, nu->orderv, type);
				if(nu->flagv & 1) makecyclicknots(nu->knotsv, nu->pntsv, nu->orderv);
			}
			else nu->knotsv= 0;
		}
	}
}

void basisNurb(float t, short order, short pnts, float *knots, float *basis, int *start, int *end)
{
	float d, e;
	int i, i1 = 0, i2 = 0 ,j, orderpluspnts, opp2, o2;

	orderpluspnts= order+pnts;
        opp2 = orderpluspnts-1;

	/* this is for float inaccuracy */
	if(t < knots[0]) t= knots[0];
	else if(t > knots[opp2]) t= knots[opp2];

	/* dit stuk is order '1' */
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

	/* printf("u %f\n", t); for(k=0;k<orderpluspnts;k++) printf(" %2.2f",basis[k]); printf("\n");    */
	
	/* dit is order 2,3,... */
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


void makeNurbfaces(Nurb *nu, float *data) 
/* data  moet 3*4*resolu*resolv lang zijn en op nul staan */
{
	BPoint *bp;
	float *basisu, *basis, *basisv, *sum, *fp, *in;
	float u, v, ustart, uend, ustep, vstart, vend, vstep, sumdiv;
	int i, j, iofs, jofs, cycl, len, resolu, resolv;
	int istart, iend, jsta, jen, *jstart, *jend, ratcomp;

	if(nu->knotsu==0 || nu->knotsv==0) return;
	if(nu->orderu>nu->pntsu) return;
	if(nu->orderv>nu->pntsv) return;
	if(data==0) return;

	/* alloceren en vars goedzetten */
	len= nu->pntsu*nu->pntsv;
	if(len==0) return;
	sum= (float *)MEM_callocN(sizeof(float)*len, "makeNurbfaces1");

	resolu= nu->resolu;
	resolv= nu->resolv;
	len= resolu*resolv;
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
	if(nu->flagu & 1) uend= fp[nu->pntsu+nu->orderu-1];
	else uend= fp[nu->pntsu];
	ustep= (uend-ustart)/(resolu-1+(nu->flagu & 1));
	basisu= (float *)MEM_mallocN(sizeof(float)*KNOTSU(nu), "makeNurbfaces3");

	fp= nu->knotsv;
	vstart= fp[nu->orderv-1];
	
	if(nu->flagv & 1) vend= fp[nu->pntsv+nu->orderv-1];
	else vend= fp[nu->pntsv];
	vstep= (vend-vstart)/(resolv-1+(nu->flagv & 1));
	len= KNOTSV(nu);
	basisv= (float *)MEM_mallocN(sizeof(float)*len*resolv, "makeNurbfaces3");
	jstart= (int *)MEM_mallocN(sizeof(float)*resolv, "makeNurbfaces4");
	jend= (int *)MEM_mallocN(sizeof(float)*resolv, "makeNurbfaces5");

	/* voorberekenen basisv en jstart,jend */
	if(nu->flagv & 1) cycl= nu->orderv-1; 
	else cycl= 0;
	v= vstart;
	basis= basisv;
	while(resolv--) {
		basisNurb(v, nu->orderv, (short)(nu->pntsv+cycl), nu->knotsv, basis, jstart+resolv, jend+resolv);
		basis+= KNOTSV(nu);
		v+= vstep;
	}

	if(nu->flagu & 1) cycl= nu->orderu-1; 
	else cycl= 0;
	in= data;
	u= ustart;
	while(resolu--) {

		basisNurb(u, nu->orderu, (short)(nu->pntsu+cycl), nu->knotsu, basisu, &istart, &iend);

		basis= basisv;
		resolv= nu->resolv;
		while(resolv--) {

			jsta= jstart[resolv];
			jen= jend[resolv];

			/* bereken sum */
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

			/* een! (1.0) echt punt nu */
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
	}

	/* vrijgeven */
	MEM_freeN(sum);
	MEM_freeN(basisu);
	MEM_freeN(basisv);
	MEM_freeN(jstart);
	MEM_freeN(jend);
}


void makeNurbcurve_forw(Nurb *nu, float *data)
/* *data: moet 3*4*pntsu*resolu lang zijn en op nul staan */
{
	BPoint *bp;
	float *basisu, *sum, *fp,  *in;
	float u, ustart, uend, ustep, sumdiv;
	int i, j, k, len, resolu, istart, iend;
	int wanted, org;

	if(nu->knotsu==0) return;
	if(data==0) return;

	/* alloceren en vars goedzetten */
	len= nu->pntsu;
	if(len==0) return;
	sum= (float *)MEM_callocN(sizeof(float)*len, "makeNurbcurve1");

	resolu= nu->resolu*nu->pntsu;
	if(resolu==0) {
		MEM_freeN(sum);
		return;
	}

	fp= nu->knotsu;
	ustart= fp[nu->orderu-1];
	uend= fp[nu->pntsu];
	ustep= (uend-ustart)/(resolu-1);
	basisu= (float *)MEM_mallocN(sizeof(float)*(nu->orderu+nu->pntsu), "makeNurbcurve3");

	in= data;
	u= ustart;
	for (k = nu->orderu - 1; k < nu->pntsu; k++){

		wanted = (int)((nu->knotsu[k+1] - nu->knotsu[k]) / ustep);
		org = 4;	/* gelijk aan order */
		if (org > wanted) org = wanted;

		for (j = org; j > 0; j--){

			basisNurb(u, nu->orderu, nu->pntsu, nu->knotsu, basisu, &istart, &iend);
			/* bereken sum */
			sumdiv= 0.0;
			fp= sum;
			for(i= istart; i<=iend; i++, fp++) {
				/* hier nog rationele component doen */
				*fp= basisu[i];
				sumdiv+= *fp;
			}
			if(sumdiv!=0.0) if(sumdiv<0.999 || sumdiv>1.001) {
				/* is dit normaliseren ook nodig? */
				fp= sum;
				for(i= istart; i<=iend; i++, fp++) {
					*fp/= sumdiv;
				}
			}

			/* een! (1.0) echt punt nu */
			fp= sum;
			bp= nu->bp+ istart;
			for(i= istart; i<=iend; i++, bp++, fp++) {

				if(*fp!=0.0) {
					in[0]+= (*fp) * bp->vec[0];
					in[1]+= (*fp) * bp->vec[1];
					in[2]+= (*fp) * bp->vec[2];
				}
			}

			in+=3;

			u+= ustep;
		}

		if (wanted > org){
			extend_spline(in - 3 * org, org, wanted);
			in += 3 * (wanted - org);
			u += ustep * (wanted - org);
		}
	}

	/* vrijgeven */
	MEM_freeN(sum);
	MEM_freeN(basisu);
}


void makeNurbcurve(Nurb *nu, float *data, int dim)
/* data moet dim*4*pntsu*resolu lang zijn en op nul staan */
{
	BPoint *bp;
	float u, ustart, uend, ustep, sumdiv;
	float *basisu, *sum, *fp,  *in;
	int i, len, resolu, istart, iend, cycl;

	if(nu->knotsu==0) return;
	if(nu->orderu>nu->pntsu) return;
	if(data==0) return;

	/* alloceren en vars goedzetten */
	len= nu->pntsu;
	if(len==0) return;
	sum= (float *)MEM_callocN(sizeof(float)*len, "makeNurbcurve1");

	resolu= nu->resolu*nu->pntsu;
	if(resolu==0) {
		MEM_freeN(sum);
		return;
	}

	fp= nu->knotsu;
	ustart= fp[nu->orderu-1];
	if(nu->flagu & 1) uend= fp[nu->pntsu+nu->orderu-1];
	else uend= fp[nu->pntsu];
	ustep= (uend-ustart)/(resolu-1+(nu->flagu & 1));
	basisu= (float *)MEM_mallocN(sizeof(float)*KNOTSU(nu), "makeNurbcurve3");

	if(nu->flagu & 1) cycl= nu->orderu-1; 
	else cycl= 0;

	in= data;
	u= ustart;
	while(resolu--) {

		basisNurb(u, nu->orderu, (short)(nu->pntsu+cycl), nu->knotsu, basisu, &istart, &iend);
		/* bereken sum */
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
			/* is dit normaliseren ook nodig? */
			fp= sum;
			for(i= istart; i<=iend; i++, fp++) {
				*fp/= sumdiv;
			}
		}

		/* een! (1.0) echt punt nu */
		fp= sum;
		bp= nu->bp+ istart-1;
		for(i= istart; i<=iend; i++, fp++) {

			if(i>=nu->pntsu) bp= nu->bp+(i - nu->pntsu);
			else bp++;

			if(*fp!=0.0) {
				
				in[0]+= (*fp) * bp->vec[0];
				in[1]+= (*fp) * bp->vec[1];
				if(dim>=3) {
					in[2]+= (*fp) * bp->vec[2];
					if(dim==4) in[3]+= (*fp) * bp->alfa;
				}
			}
		}

		in+= dim;

		u+= ustep;
	}

	/* vrijgeven */
	MEM_freeN(sum);
	MEM_freeN(basisu);
}

void maakbez(float q0, float q1, float q2, float q3, float *p, int it)
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
		p+= 3;
		q0+= q1;
 		q1+= q2;
 		q2+= q3;
 	}
}	

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void make_orco_surf(Curve *cu)
{
	Nurb *nu;
	int a, b, tot=0;
	int sizeu, sizev;// ###
	float *data;


	/* eerst voorspellen hoelang datablok moet worden */
	nu= cu->nurb.first;
	while(nu) {
#ifdef STRUBI
/* this is a bad hack: as we want to avoid the seam in a cyclic nurbs
texture wrapping, reserve extra orco data space to save these extra needed
vertex based UV coordinates for the meridian vertices.
Vertices on the 0/2pi boundary are not duplicated inside the displist but later in
the renderface/vert construction.

See also blenderWorldManipulation.c: init_render_surf()

*/

		sizeu = nu->resolu; sizev = nu->resolv;
		if (nu->flagu & CU_CYCLIC) sizeu++;
		if (nu->flagv & CU_CYCLIC) sizev++;
		if(nu->pntsv>1) tot+= sizeu * sizev;
#else
		if(nu->pntsv>1) tot+= nu->resolu*nu->resolv;
#endif
		nu= nu->next;
	}
				/* makeNurbfaces wil nullen */
	data= cu->orco= MEM_callocN(3*sizeof(float)*tot, "make_orco");

	nu= cu->nurb.first;
	while(nu) {
		if(nu->pntsv>1) {
			sizeu = nu->resolu;
			sizev = nu->resolv;
#ifdef STRUBI
			if (nu->flagu & CU_CYCLIC) sizeu++;
			if (nu->flagv & CU_CYCLIC) sizev++;
#endif
			
			if(cu->flag & CU_UV_ORCO) {
				for(b=0; b< sizeu; b++) {
					for(a=0; a< sizev; a++) {
					
						if(sizev <2) data[0]= 0.0f;
						else data[0]= -1.0f + 2.0f*((float)a)/(sizev - 1);
						
						if(sizeu <2) data[1]= 0.0f;
						else data[1]= -1.0f + 2.0f*((float)b)/(sizeu - 1);
						
						data[2]= 0.0;
		
						data+= 3;
					}
				}
			}
			else {
				makeNurbfaces(nu, data);
#ifdef STRUBI 
				for(b=0; b< nu->resolu; b++) {
					for(a=0; a< nu->resolv; a++) {
						data = cu->orco + 3 * (b * nu->resolv + a);
						data[0]= (data[0]-cu->loc[0])/cu->size[0];
						data[1]= (data[1]-cu->loc[1])/cu->size[1];
						data[2]= (data[2]-cu->loc[2])/cu->size[2];
					}
				}
				copyintoExtendedArray(cu->orco, nu->resolv, nu->resolu, cu->orco, sizev, sizeu);
				/* copy U/V-cyclic orco's */
				if (nu->flagv & CU_CYCLIC) {
					b = sizeu - 1;	
					for(a=0; a< sizev; a++) {
						data = cu->orco + 3 * (b * sizev + a);
						VECCOPY(data, cu->orco + 3*a);
					}
				}	
				if (nu->flagu & CU_CYCLIC) {
					a = sizev - 1;	
					for(b=0; b< sizeu; b++) {
						data = cu->orco + 3 * (b * sizev + a);
						VECCOPY(data, cu->orco + 3 * b*sizev);
					}
				}	

#else
				tot= sizeu * sizev;
				while(tot--) {
					data[0]= (data[0]-cu->loc[0])/cu->size[0];
					data[1]= (data[1]-cu->loc[1])/cu->size[1];
					data[2]= (data[2]-cu->loc[2])/cu->size[2];
	
					data+= 3;
				}
#endif
			}
		}
		nu= nu->next;
	}
	/* loadkeypostype(22, base, base); */

}



/* ***************** BEVEL ****************** */

void makebevelcurve(Object *ob, ListBase *disp)
{
	DispList *dl, *dlnew;
	Curve *bevcu, *cu;
	float *fp, facx, facy, hoek, dhoek;
	int nr, a;

	cu= ob->data;

	if(cu->bevobj && cu->bevobj!=ob) {
		if(cu->bevobj->type==OB_CURVE) {
			bevcu= cu->bevobj->data;
			if(bevcu->ext1==0.0 && bevcu->ext2==0.0) {
				facx= cu->bevobj->size[0];
				facy= cu->bevobj->size[1];

				dl= bevcu->disp.first;
				if(dl==0) {
					makeDispList(cu->bevobj);
					dl= bevcu->disp.first;
				}
				while(dl) {
					if ELEM(dl->type, DL_POLY, DL_SEGM) {
						dlnew= MEM_mallocN(sizeof(DispList), "makebevelcurve1");					
						*dlnew= *dl;
						dlnew->verts= MEM_mallocN(3*sizeof(float)*dl->parts*dl->nr, "makebevelcurve1");
						memcpy(dlnew->verts, dl->verts, 3*sizeof(float)*dl->parts*dl->nr);
						
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
	else if(cu->ext2==0.0) {
		dl= MEM_callocN(sizeof(DispList), "makebevelcurve2");
		dl->verts= MEM_mallocN(2*3*sizeof(float), "makebevelcurve2");
		BLI_addtail(disp, dl);
		dl->type= DL_SEGM;
		dl->parts= 1;
		dl->nr= 2;
		fp= dl->verts;
		fp[0]= fp[1]= 0.0;
		fp[2]= -cu->ext1;
		fp[3]= fp[4]= 0.0;
		fp[5]= cu->ext1;
	}
	else {
		nr= 4+2*cu->bevresol;

		dl= MEM_callocN(sizeof(DispList), "makebevelcurve3");
		dl->verts= MEM_mallocN(nr*3*sizeof(float), "makebevelcurve3");
		BLI_addtail(disp, dl);
		dl->type= DL_SEGM;
		dl->parts= 1;
		dl->nr= nr;

		/* eerst cirkel maken */
		fp= dl->verts;
		hoek= -0.5*M_PI;
		dhoek= (float)(M_PI/(nr-2));
		for(a=0; a<nr; a++) {
			fp[0]= 0.0;
			fp[1]= (float)(cos(hoek)*(cu->ext2));
			fp[2]= (float)(sin(hoek)*(cu->ext2));
			hoek+= dhoek;
			fp+= 3;
			if(cu->ext1!=0.0 && a==((nr/2)-1) ) {
				VECCOPY(fp, fp-3);
				fp+=3;
				a++;
			}
		}
		if(cu->ext1==0.0) dl->nr--;
		else {
			fp= dl->verts;
			for(a=0; a<nr; a++) {
				if(a<=(nr/2-1)) fp[2]-= (cu->ext1);
				else fp[2]+= (cu->ext1);
				fp+= 3;
			}
		}
	}

}

int cu_isectLL(float *v1, float *v2, float *v3, float *v4, short cox, short coy, float *labda, float *mu, float *vec)
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


short bevelinside(BevList *bl1,BevList *bl2)
{
	/* is bl2 INSIDE bl1 ? met links-rechts methode en "labda's" */
	/* geeft als correct gat 1 terug  */
	BevPoint *bevp, *prevbevp;
	float min,max,vec[3],hvec1[3],hvec2[3],lab,mu;
	int nr, links=0,rechts=0,mode;

	/* neem eerste vertex van het mogelijke gat */

	bevp= (BevPoint *)(bl2+1);
	hvec1[0]= bevp->x; 
	hvec1[1]= bevp->y; 
	hvec1[2]= 0.0;
	VECCOPY(hvec2,hvec1);
	hvec2[0]+=1000;

	/* test deze met alle edges van mogelijk omringende poly */
	/* tel aantal overgangen links en rechts */

	bevp= (BevPoint *)(bl1+1);
	nr= bl1->nr;
	prevbevp= bevp+(nr-1);

	while(nr--) {
		min= prevbevp->y;
		max= bevp->y;
		if(max<min) {
			min= max;
			max= prevbevp->y;
		}
		if(min!=max) {
			if(min<=hvec1[1] && max>=hvec1[1]) {
				/* er is een overgang, snijpunt berekenen */
				mode= cu_isectLL(&(prevbevp->x),&(bevp->x),hvec1,hvec2,0,1,&lab,&mu,vec);
				/* als lab==0.0 of lab==1.0 dan snijdt de edge exact de overgang
				 * alleen toestaan voor lab= 1.0 (of andersom,  maakt niet uit)
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

int vergxcobev(const void *a1, const void *a2)
{
	const struct bevelsort *x1=a1,*x2=a2;

	if( x1->left > x2->left ) return 1;
	else if( x1->left < x2->left) return -1;
	return 0;
}

/* deze kan niet zomaar door atan2 vervangen worden, maar waarom? */

void calc_bevel_sin_cos(float x1, float y1, float x2, float y2, float *sina, float *cosa)
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
		/* printf("x3 en y3  nul \n"); */
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

void alfa_bezpart(BezTriple *prevbezt, BezTriple *bezt, Nurb *nu, float *data_a)
{
	BezTriple *pprev, *next, *last;
	float fac, dfac, t[4];
	int a;
	
	last= nu->bezt+(nu->pntsu-1);
	
	/* een punt terug */
	if(prevbezt==nu->bezt) {
		if(nu->flagu & 1) pprev= last;
		else pprev= prevbezt;
	}
	else pprev= prevbezt-1;
	
	/* een punt verder */
	if(bezt==last) {
		if(nu->flagu & 1) next= nu->bezt;
		else next= bezt;
	}
	else next= bezt+1;
	
	fac= 0.0;
	dfac= 1.0f/(float)nu->resolu;
	
	for(a=0; a<nu->resolu; a++, fac+= dfac) {
		
		set_four_ipo(fac, t, KEY_BSPLINE);
		
		data_a[a]= t[0]*pprev->alfa + t[1]*prevbezt->alfa + t[2]*bezt->alfa + t[3]*next->alfa;
	}
}

void makeBevelList(Object *ob)
{
	/* - alle curves omzetten in poly's, met aangegeven resol en vlaggen voor dubbele punten
       - eventueel intelligent punten verwijderen (geval Nurb) 
       - scheiden in verschillende blokken met Boundbox
       - Autogat detectie */
	Curve *cu;
	Nurb *nu;
	BezTriple *bezt, *prevbezt;
	BPoint *bp;
	BevList *bl, *blnew, *blnext;
	BevPoint *bevp, *bevp2, *bevp1 = NULL, *bevp0;
	float  *data, *data_a, *v1, *v2, min, inp, x1, x2, y1, y2, vec[3];
	struct bevelsort *sortdata, *sd, *sd1;
	int a, b, len, nr, poly;

	/* deze fie moet object hebben in verband met tflag en upflag */
	cu= ob->data;

	/* STAP 1: POLY'S MAKEN */

	BLI_freelistN(&(cu->bev));
	if(ob==G.obedit) nu= editNurb.first;
	else nu= cu->nurb.first;
	
	while(nu) {
		if(nu->pntsu>1) {
		
			if((nu->type & 7)==CU_POLY) {
	
				len= nu->pntsu;
				bl= MEM_callocN(sizeof(BevList)+len*sizeof(BevPoint), "makeBevelList");
				BLI_addtail(&(cu->bev), bl);
	
				if(nu->flagu & 1) bl->poly= 0;
				else bl->poly= -1;
				bl->nr= len;
				bl->flag= 0;
				bevp= (BevPoint *)(bl+1);
				bp= nu->bp;
	
				while(len--) {
					bevp->x= bp->vec[0];
					bevp->y= bp->vec[1];
					bevp->z= bp->vec[2];
					bevp->alfa= bp->alfa;
					bevp->f1= 1;
					bevp++;
					bp++;
				}
			}
			else if((nu->type & 7)==CU_BEZIER) {
	
				len= nu->resolu*(nu->pntsu+ (nu->flagu & 1) -1)+1;	/* voor laatste punt niet cyclic */
				bl= MEM_callocN(sizeof(BevList)+len*sizeof(BevPoint), "makeBevelList");
				BLI_addtail(&(cu->bev), bl);
	
				if(nu->flagu & 1) bl->poly= 0;
				else bl->poly= -1;
				bevp= (BevPoint *)(bl+1);
	
				a= nu->pntsu-1;
				bezt= nu->bezt;
				if(nu->flagu & 1) {
					a++;
					prevbezt= nu->bezt+(nu->pntsu-1);
				}
				else {
					prevbezt= bezt;
					bezt++;
				}
				
				data= MEM_mallocN(3*sizeof(float)*(nu->resolu+1), "makeBevelList2");
				data_a= MEM_callocN(sizeof(float)*(nu->resolu+1), "data_a");
				
				while(a--) {
					if(prevbezt->h2==HD_VECT && bezt->h1==HD_VECT) {
	
						bevp->x= prevbezt->vec[1][0];
						bevp->y= prevbezt->vec[1][1];
						bevp->z= prevbezt->vec[1][2];
						bevp->alfa= prevbezt->alfa;
						bevp->f1= 1;
						bevp->f2= 0;
						bevp++;
						bl->nr++;
						bl->flag= 1;
					}
					else {
						v1= prevbezt->vec[1];
						v2= bezt->vec[0];
						
						/* altijd alle drie doen: anders blijft data hangen */
						maakbez(v1[0], v1[3], v2[0], v2[3], data, nu->resolu);
						maakbez(v1[1], v1[4], v2[1], v2[4], data+1, nu->resolu);
						maakbez(v1[2], v1[5], v2[2], v2[5], data+2, nu->resolu);
						
						if((nu->type & CU_2D)==0) {
							if(cu->flag & CU_3D) {
								alfa_bezpart(prevbezt, bezt, nu, data_a);
							}
						}
						
						
						/* met handlecodes dubbele punten aangeven */
						if(prevbezt->h1==prevbezt->h2) {
							if(prevbezt->h1==0 || prevbezt->h1==HD_VECT) bevp->f1= 1;
						}
						else {
							if(prevbezt->h1==0 || prevbezt->h1==HD_VECT) bevp->f1= 1;
							else if(prevbezt->h2==0 || prevbezt->h2==HD_VECT) bevp->f1= 1;
						}
						
						v1= data;
						v2= data_a;
						nr= nu->resolu;
						
						while(nr--) {
							bevp->x= v1[0]; 
							bevp->y= v1[1];
							bevp->z= v1[2];
							bevp->alfa= v2[0];
							bevp++;
							v1+=3;
							v2++;
						}
						bl->nr+= nu->resolu;
	
					}
					prevbezt= bezt;
					bezt++;
				}
				
				MEM_freeN(data);
				MEM_freeN(data_a);
				
				if((nu->flagu & 1)==0) {	    /* niet cyclic: endpoint */
					bevp->x= prevbezt->vec[1][0];
					bevp->y= prevbezt->vec[1][1];
					bevp->z= prevbezt->vec[1][2];
					bl->nr++;
				}
	
			}
			else if((nu->type & 7)==CU_NURBS) {
				if(nu->pntsv==1) {
					len= nu->resolu*nu->pntsu;
					bl= MEM_mallocN(sizeof(BevList)+len*sizeof(BevPoint), "makeBevelList3");
					BLI_addtail(&(cu->bev), bl);
					bl->nr= len;
					bl->flag= 0;
					if(nu->flagu & 1) bl->poly= 0;
					else bl->poly= -1;
					bevp= (BevPoint *)(bl+1);
	
					data= MEM_callocN(4*sizeof(float)*len, "makeBevelList4");    /* moet op nul staan */
					makeNurbcurve(nu, data, 4);
					
					v1= data;
					while(len--) {
						bevp->x= v1[0]; 
						bevp->y= v1[1];
						bevp->z= v1[2];
						bevp->alfa= v1[3];
						
						bevp->f1= bevp->f2= 0;
						bevp++;
						v1+=4;
					}
					MEM_freeN(data);
				}
			}
		}
		nu= nu->next;
	}

	/* STAP 2: DUBBELE PUNTEN EN AUTOMATISCHE RESOLUTIE, DATABLOKKEN VERKLEINEN */
	bl= cu->bev.first;
	while(bl) {
		nr= bl->nr;
		bevp1= (BevPoint *)(bl+1);
		bevp0= bevp1+(nr-1);
		nr--;
		while(nr--) {
			if( fabs(bevp0->x-bevp1->x)<0.00001 ) {
				if( fabs(bevp0->y-bevp1->y)<0.00001 ) {
					if( fabs(bevp0->z-bevp1->z)<0.00001 ) {
						bevp0->f2= 1;
						bl->flag++;
					}
				}
			}
			bevp0= bevp1;
			bevp1++;
		}
		bl= bl->next;
	}
	bl= cu->bev.first;
	while(bl) {
		blnext= bl->next;
		if(bl->flag) {
			nr= bl->nr- bl->flag+1;	/* +1 want vectorbezier zet ook flag */
			blnew= MEM_mallocN(sizeof(BevList)+nr*sizeof(BevPoint), "makeBevelList");
			memcpy(blnew, bl, sizeof(BevList));
			blnew->nr= 0;
			BLI_remlink(&(cu->bev), bl);
			BLI_insertlinkbefore(&(cu->bev),blnext,blnew);	/* zodat bevlijst met nurblijst gelijk loopt */
			bevp0= (BevPoint *)(bl+1);
			bevp1= (BevPoint *)(blnew+1);
			nr= bl->nr;
			while(nr--) {
				if(bevp0->f2==0) {
					memcpy(bevp1, bevp0, sizeof(BevPoint));
					bevp1++;
					blnew->nr++;
				}
				bevp0++;
			}
			MEM_freeN(bl);
			blnew->flag= 0;
		}
		bl= blnext;
	}

	/* STAP 3: POLY'S TELLEN EN AUTOGAT */
	bl= cu->bev.first;
	poly= 0;
	while(bl) {
		if(bl->poly>=0) {
			poly++;
			bl->poly= poly;
			bl->gat= 0;
		}
		bl= bl->next;
	}
	

	/* meest linkse punten vinden, tevens richting testen */
	if(poly>0) {
		sd= sortdata= MEM_mallocN(sizeof(struct bevelsort)*poly, "makeBevelList5");
		bl= cu->bev.first;
		while(bl) {
			if(bl->poly>0) {

				min= 300000.0;
				bevp= (BevPoint *)(bl+1);
				nr= bl->nr;
				while(nr--) {
					if(min>bevp->x) {
						min= bevp->x;
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

				inp= (bevp1->x- bevp0->x)*(bevp0->y- bevp2->y)
				    +(bevp0->y- bevp1->y)*(bevp0->x- bevp2->x);

				if(inp>0.0) sd->dir= 1;
				else sd->dir= 0;

				sd++;
			}

			bl= bl->next;
		}
		qsort(sortdata,poly,sizeof(struct bevelsort), vergxcobev);

		sd= sortdata+1;
		for(a=1; a<poly; a++, sd++) {
			bl= sd->bl;	    /* is bl een gat? */
			sd1= sortdata+ (a-1);
			for(b=a-1; b>=0; b--, sd1--) {	/* alle polys links ervan */
				if(bevelinside(sd1->bl, bl)) {
					bl->gat= 1- sd1->bl->gat;
					break;
				}
			}
		}

		/* draairichting */
		if((cu->flag & CU_3D)==0) {
			sd= sortdata;
			for(a=0; a<poly; a++, sd++) {
				if(sd->bl->gat==sd->dir) {
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

	/* STAP 4: COSINUSSEN */
	bl= cu->bev.first;
	while(bl) {
	
		if(bl->nr==2) {	/* 2 pnt, apart afhandelen: KAN DAT NIET AFGESCHAFT? */
			bevp2= (BevPoint *)(bl+1);
			bevp1= bevp2+1;

			x1= bevp1->x- bevp2->x;
			y1= bevp1->y- bevp2->y;

			calc_bevel_sin_cos(x1, y1, -x1, -y1, &(bevp1->sina), &(bevp1->cosa));
			bevp2->sina= bevp1->sina;
			bevp2->cosa= bevp1->cosa;

			if(cu->flag & CU_3D) {	/* 3D */
				float *quat, q[4];
			
				vec[0]= bevp1->x - bevp2->x;
				vec[1]= bevp1->y - bevp2->y;
				vec[2]= bevp1->z - bevp2->z;
				
				quat= vectoquat(vec, 5, 1);
				
				Normalise(vec);
				q[0]= (float)cos(0.5*bevp1->alfa);
				x1= (float)sin(0.5*bevp1->alfa);
				q[1]= x1*vec[0];
				q[2]= x1*vec[1];
				q[3]= x1*vec[2];
				QuatMul(quat, q, quat);
				
				QuatToMat3(quat, bevp1->mat);
				Mat3CpyMat3(bevp2->mat, bevp1->mat);
			}

		}
		else if(bl->nr>2) {
			bevp2= (BevPoint *)(bl+1);
			bevp1= bevp2+(bl->nr-1);
			bevp0= bevp1-1;

		
			nr= bl->nr;
	
			while(nr--) {
	
				if(cu->flag & CU_3D) {	/* 3D */
					float *quat, q[4];
				
					vec[0]= bevp2->x - bevp0->x;
					vec[1]= bevp2->y - bevp0->y;
					vec[2]= bevp2->z - bevp0->z;
					
					Normalise(vec);

					quat= vectoquat(vec, 5, 1);
					
					q[0]= (float)cos(0.5*bevp1->alfa);
					x1= (float)sin(0.5*bevp1->alfa);
					q[1]= x1*vec[0];
					q[2]= x1*vec[1];
					q[3]= x1*vec[2];
					QuatMul(quat, q, quat);
					
					QuatToMat3(quat, bevp1->mat);
				}
				
				x1= bevp1->x- bevp0->x;
				x2= bevp1->x- bevp2->x;
				y1= bevp1->y- bevp0->y;
				y2= bevp1->y- bevp2->y;
			
				calc_bevel_sin_cos(x1, y1, x2, y2, &(bevp1->sina), &(bevp1->cosa));
				
				
				bevp0= bevp1;
				bevp1= bevp2;
				bevp2++;
			}
			/* niet cyclic gevallen corrigeren */
			if(bl->poly== -1) {
				if(bl->nr>2) {
					bevp= (BevPoint *)(bl+1);
					bevp1= bevp+1;
					bevp->sina= bevp1->sina;
					bevp->cosa= bevp1->cosa;
					Mat3CpyMat3(bevp->mat, bevp1->mat);
					bevp= (BevPoint *)(bl+1);
					bevp+= (bl->nr-1);
					bevp1= bevp-1;
					bevp->sina= bevp1->sina;
					bevp->cosa= bevp1->cosa;
					Mat3CpyMat3(bevp->mat, bevp1->mat);
				}
			}
		}
		bl= bl->next;
	}
}

/* ****************** HANDLES ************** */

/*
 *   handlecodes:
 *		1: niets,  1:auto,  2:vector,  3:aligned
 */


void calchandleNurb(BezTriple *bezt,BezTriple *prev, BezTriple *next, int mode)
{
	float *p1,*p2,*p3,pt[3];
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

	if(mode && bezt->h1==HD_AUTO && prev) {
		dx= p2[0] - (p1[0]+p1[3])/2.0f;
		dy= p2[1] - (p1[1]+p1[4])/2.0f;
		dz= p2[2] - (p1[2]+p1[5])/2.0f;
	}
	else {
		dx= p2[0]- p1[0];
		dy= p2[1]- p1[1];
		dz= p2[2]- p1[2];
	}
	len1= (float)sqrt(dx*dx+dy*dy+dz*dz);
	
	if(mode && bezt->h2==HD_AUTO && next) {
		dx1= (p3[0]+p3[-3])/2.0f - p2[0];
		dy1= (p3[1]+p3[-2])/2.0f - p2[1];
		dz1= (p3[2]+p3[-1])/2.0f - p2[2];
	}
	else {
		dx1= p3[0]- p2[0];
		dy1= p3[1]- p2[1];
		dz1= p3[2]- p2[2];
	}
	len2= (float)sqrt(dx1*dx1+dy1*dy1+dz1*dz1);

	if(len1==0.0f) len1=1.0f;
	if(len2==0.0f) len2=1.0f;


	if(bezt->h1==HD_AUTO || bezt->h2==HD_AUTO) {    /* auto */
		vx= dx1/len2 + dx/len1;
		vy= dy1/len2 + dy/len1;
		vz= dz1/len2 + dz/len1;
		len= 2.71f*(float)sqrt(vx*vx + vy*vy + vz*vz);
		if(len!=0.0f) {
		
			if(len1>5.0f*len2) len1= 5.0f*len2;	
			if(len2>5.0f*len1) len2= 5.0f*len1;
			
			if(bezt->h1==HD_AUTO) {
				len1/=len;
				*(p2-3)= *p2-vx*len1;
				*(p2-2)= *(p2+1)-vy*len1;
				*(p2-1)= *(p2+2)-vz*len1;
			}
			if(bezt->h2==HD_AUTO) {
				len2/=len;
				*(p2+3)= *p2+vx*len2;
				*(p2+4)= *(p2+1)+vy*len2;
				*(p2+5)= *(p2+2)+vz*len2;
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
	if(bezt->f1 & 1) { /* volgorde van berekenen */
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

void calchandlesNurb(Nurb *nu) /* wel eerst (zonodig) de handlevlaggen zetten */
{
	BezTriple *bezt, *prev, *next;
	short a;

	if((nu->type & 7)!=1) return;
	if(nu->pntsu<2) return;
	
	a= nu->pntsu;
	bezt= nu->bezt;
	if(nu->flagu & 1) prev= bezt+(a-1);
	else prev= 0;
	next= bezt+1;

	while(a--) {
		calchandleNurb(bezt, prev, next, 0);
		prev= bezt;
		if(a==1) {
			if(nu->flagu & 1) next= nu->bezt;
			else next= 0;
		}
		else next++;

		bezt++;
	}
}


void testhandlesNurb(Nurb *nu)
{
	/* Te gebruiken als er iets an de handles is veranderd.
	 * Loopt alle BezTriples af met de volgende regels:
     * FASE 1: types veranderen?
     *  Autocalchandles: worden ligned als NOT(000 || 111)
     *  Vectorhandles worden 'niets' als (selected en andere niet) 
     * FASE 2: handles herbereken
     */
	BezTriple *bezt;
	short flag, a;

	if((nu->type & 7)!=CU_BEZIER) return;

	bezt= nu->bezt;
	a= nu->pntsu;
	while(a--) {
		flag= 0;
		if(bezt->f1 & 1) flag++;
		if(bezt->f2 & 1) flag += 2;
		if(bezt->f3 & 1) flag += 4;

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
	/* Kijkt naar de coordinaten van de handles en berekent de soort */
	
	BezTriple *bezt2, *bezt1, *bezt0;
	int i, align, leftsmall, rightsmall;

	if(nu==0 || nu->bezt==0) return;
	
	bezt2 = nu->bezt;
	bezt1 = bezt2 + (nu->pntsu-1);
	bezt0 = bezt1 - 1;
	i = nu->pntsu;

	while(i--) {
		
		align= leftsmall= rightsmall= 0;
		
		/* linker handle: */
		if(flag==0 || (bezt1->f1 & flag) ) {
			bezt1->h1= 0;
			/* afstand te klein: vectorhandle */
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
				/* of toch vector handle? */
				if(DistVL2Dfl(bezt1->vec[0], bezt1->vec[1], bezt0->vec[1]) < 0.0001)
					bezt1->h1= HD_VECT;
				
			}
		}
		/* rechter handle: */
		if(flag==0 || (bezt1->f3 & flag) ) {
			bezt1->h2= 0;
			/* afstand te klein: vectorhandle */
			if( VecLenf( bezt1->vec[1], bezt2->vec[1] ) < 0.0001) {
				bezt1->h2= HD_VECT;
				rightsmall= 1;
			}
			else {
				/* aligned handle? */
				if(align) bezt1->h2= HD_ALIGN;

				/* of toch vector handle? */
				if(DistVL2Dfl(bezt1->vec[2], bezt1->vec[1], bezt2->vec[1]) < 0.0001)
					bezt1->h2= HD_VECT;
				
			}
		}
		if(leftsmall && bezt1->h2==HD_ALIGN) bezt1->h2= 0;
		if(rightsmall && bezt1->h1==HD_ALIGN) bezt1->h1= 0;
		
		/* onzalige combinatie: */
		if(bezt1->h1==HD_ALIGN && bezt1->h2==HD_VECT) bezt1->h1= 0;
		if(bezt1->h2==HD_ALIGN && bezt1->h1==HD_VECT) bezt1->h2= 0;
		
		bezt0= bezt1;
		bezt1= bezt2;
		bezt2++;
	}

	calchandlesNurb(nu);
}

void autocalchandlesNurb_all(int flag)
{
	Nurb *nu;
	
	nu= editNurb.first;
	while(nu) {
		autocalchandlesNurb(nu, flag);
		nu= nu->next;
	}
}

void sethandlesNurb(short code)
{
	/* code==1: set autohandle */
	/* code==2: set vectorhandle */
	/* als code==3 (HD_ALIGN) toggelt het, vectorhandles worden HD_FREE */
	Nurb *nu;
	BezTriple *bezt;
	short a, ok=0;

	if(code==1 || code==2) {
		nu= editNurb.first;
		while(nu) {
			if( (nu->type & 7)==1) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(bezt->f1 || bezt->f3) {
						if(bezt->f1) bezt->h1= code;
						if(bezt->f3) bezt->h2= code;
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
		/* is er 1 handle NIET vrij: alles vrijmaken, else ALIGNED maken */
		
		nu= editNurb.first;
		while(nu) {
			if( (nu->type & 7)==1) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(bezt->f1 && bezt->h1) ok= 1;
					if(bezt->f3 && bezt->h2) ok= 1;
					if(ok) break;
					bezt++;
				}
			}
			nu= nu->next;
		}
		if(ok) ok= HD_FREE;
		else ok= HD_ALIGN;
		
		nu= editNurb.first;
		while(nu) {
			if( (nu->type & 7)==1) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(bezt->f1) bezt->h1= ok;
					if(bezt->f3 ) bezt->h2= ok;
	
					bezt++;
				}
				calchandlesNurb(nu);
			}
			nu= nu->next;
		}
	}
}

void swapdata(void *adr1, void *adr2, int len)
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

		adr= (char *)malloc(len);
		memcpy(adr, adr1, len);
		memcpy(adr1, adr2, len);
		memcpy(adr2, adr, len);
		free(adr);
	}
}

void switchdirectionNurb(Nurb *nu)
{
	BezTriple *bezt1, *bezt2;
	BPoint *bp1, *bp2;
	float *fp1, *fp2, *tempf;
	int a, b;

	if(nu->pntsu==1 && nu->pntsv==1) return;

	if((nu->type & 7)==CU_BEZIER) {
		a= nu->pntsu;
		bezt1= nu->bezt;
		bezt2= bezt1+(a-1);
		if(a & 1) a+= 1;	/* bij oneven ook van middelste inhoud swappen */
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
		if((nu->type & 7)==CU_NURBS) {
			/* de knots omkeren */
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
			/* en weer in stijgende lijn maken */
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
