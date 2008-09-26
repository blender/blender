/**
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

#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <stdlib.h>
#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_dynstr.h"
#include "BLI_rand.h"

#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_utildefines.h"
#include "BKE_library.h"
#include "BKE_ipo.h"
#include "BKE_depsgraph.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_main.h"
#include "BKE_key.h"

#include "BIF_editmode_undo.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BIF_space.h"
#include "BIF_editkey.h"
#include "BIF_mywindow.h"
#include "BIF_interface.h"
#include "BIF_transform.h"
#include "BIF_retopo.h"

#include "BSE_view.h"	/* For persp... */
#include "BSE_edit.h"

#include "BDR_drawobject.h"
#include "BDR_editcurve.h"
#include "BDR_editobject.h"

#include "mydevice.h"
#include "blendef.h"

#include "BDR_editcurve.h"
/* still need to eradicate a few :( */
#define callocstructN(x,y,name) (x*)MEM_callocN((y)* sizeof(x),name)


ListBase editNurb;
BPoint *lastselbp;
int actnu;		/* for selected */


/*  void freeNurblist(ListBase *lb); already declared in the kernel */

float nurbcircle[8][2]= {
	{0.0, -1.0}, {-1.0, -1.0}, {-1.0, 0.0}, {-1.0,  1.0},
	{0.0,  1.0}, { 1.0,  1.0}, { 1.0, 0.0}, { 1.0, -1.0}
};


/* this replaces the active flag used in uv/face mode */
void set_actNurb(Nurb *nu)
{
	if (nu==NULL) {
		actnu = -1;
	} else {
		actnu = BLI_findindex(&editNurb, nu);
	}
}

Nurb * get_actNurb( void )
{
	return BLI_findlink(&editNurb, actnu);
}


/* ******************* SELECTION FUNCTIONS ********************* */

/* returns 1 in case (de)selection was successful */
static short select_beztriple(BezTriple *bezt, short selstatus, short flag, short hidden)
{	
	if(bezt) {
		if((bezt->hide==0) || (hidden==1)) {
			if(selstatus==1) { /* selects */			
				bezt->f1 |= flag;
				bezt->f2 |= flag;
				bezt->f3 |= flag;
				return 1;			
			}
			else { /* deselects */	
				bezt->f1 &= ~flag; 
				bezt->f2 &= ~flag; 
				bezt->f3 &= ~flag; 
				return 1;
			}
		}
	}
	
	return 0;
}

/* returns 1 in case (de)selection was successful */
static short select_bpoint(BPoint *bp, short selstatus, short flag, short hidden) 
{	
	if(bp) {
		if((bp->hide==0) || (hidden==1)) {
			if(selstatus==1) {
				bp->f1 |= flag;
				return 1;
			}
			else {
				bp->f1 &= ~flag;
				return 1;
			}
		}
	}

	return 0;
}

static short swap_selection_beztriple(BezTriple *bezt)
{
	if(bezt->f2 & SELECT)
		return select_beztriple(bezt, DESELECT, 1, VISIBLE);
	else
		return select_beztriple(bezt, SELECT, 1, VISIBLE);
}

static short swap_selection_bpoint(BPoint *bp)
{
	if(bp->f1 & SELECT)
		return select_bpoint(bp, DESELECT, 1, VISIBLE);
	else
		return select_bpoint(bp, SELECT, 1, VISIBLE);
}

short isNurbsel(Nurb *nu)
{
	BezTriple *bezt;
	BPoint *bp;
	int a;

	if((nu->type & 7)==CU_BEZIER) {
		bezt= nu->bezt;
		a= nu->pntsu;
		while(a--) {
			if( (bezt->f1 & SELECT) || (bezt->f2 & SELECT) || (bezt->f3 & SELECT) ) return 1;
			bezt++;
		}
	}
	else {
		bp= nu->bp;
		a= nu->pntsu*nu->pntsv;
		while(a--) {
			if( (bp->f1 & SELECT) ) return 1;
			bp++;
		}
	}
	return 0;
}

int isNurbsel_count(Nurb *nu)
{
	BezTriple *bezt;
	BPoint *bp;
	int a, sel=0;

	if((nu->type & 7)==CU_BEZIER) {
		bezt= nu->bezt;
		a= nu->pntsu;
		while(a--) {
			if (BEZSELECTED_HIDDENHANDLES(bezt)) sel++;
			bezt++;
		}
	}
	else {
		bp= nu->bp;
		a= nu->pntsu*nu->pntsv;
		while(a--) {
			if( (bp->f1 & SELECT) ) sel++;
			bp++;
		}
	}
	return sel;
}

/* ******************* PRINTS ********************* */

void printknots()
{
	Nurb *nu;
	int a, num;

	for(nu= editNurb.first; nu; nu= nu->next) {
		if(isNurbsel(nu) &&  (nu->type & 7)==CU_NURBS) {
			if(nu->knotsu) {
				num= KNOTSU(nu);
				for(a=0;a<num;a++) printf("knotu %d: %f\n", a, nu->knotsu[a]);
			}
			if(nu->knotsv) {
				num= KNOTSV(nu);
				for(a=0;a<num;a++) printf("knotv %d: %f\n", a, nu->knotsv[a]);
			}
		}
	}
}

#if 0
static void printweightsNurb__doPrint(void *userData, Nurb *nurb, BPoint *bp, BezTriple *bezt, int beztindex, int x, int y)
{
	char str[30];

	if (bp && (bp->f1 & SELECT)) {
		sprintf(str,"%2.2f", bp->vec[3]);

		cpack(0x737373);
		glRasterPos2i(x-1, y-1);
		BMF_DrawString(G.font, str);

		glRasterPos2i(x+1, y+1);
		BMF_DrawString(G.font, str);

		cpack(0xFFFFFF);
		glRasterPos2i(x, y);
		BMF_DrawString(G.font, str);
	}
}
static void printweightsNurb(void)
{
	if(G.obedit==0) return;

	persp(PERSP_WIN);

	glDrawBuffer(GL_FRONT); 
	nurbs_foreachScreenVert(printweightsNurb__doPrint, NULL);
	glDrawBuffer(GL_BACK);

	persp(PERSP_VIEW);
}
#endif


/* ********************* LOAD and MAKE *************** */

void load_editNurb()
{
	/* load editNurb in object */
	Curve *cu= 0;
	Nurb *nu, *newnu;
	KeyBlock *actkey;

	if(G.obedit==0) return;

	if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
		
		G.totvert= count_curveverts(&editNurb);
		
		cu= G.obedit->data;

		/* are there keys? */
		actkey = ob_get_keyblock(G.obedit);
		if(actkey) {
			/* active key: the vertices */
			
			if(G.totvert) {
				if(actkey->data) MEM_freeN(actkey->data);
			
				actkey->data= MEM_callocN(cu->key->elemsize*G.totvert, "actkey->data");
				actkey->totelem= G.totvert;
		
				curve_to_key(cu, actkey, &editNurb);
			}
		}
		
		if(cu->key && actkey!=cu->key->refkey) {
			;
		}
		else {
			freeNurblist(&(cu->nurb));
			
			for(nu= editNurb.first; nu; nu= nu->next) {
				newnu= duplicateNurb(nu);
				BLI_addtail(&(cu->nurb), newnu);
				
				if((nu->type & 7)==CU_NURBS) {
					clamp_nurb_order_u(nu);
				}
			}
		}
		
	}
	
	set_actNurb(NULL);
}

void make_editNurb()
{
	/* make copy of baseNurb in editNurb */
	Curve *cu=0;
	Nurb *nu, *newnu;
	KeyBlock *actkey;

	if(G.obedit==NULL) return;

	lastselbp= NULL;   /* global for select row */

	if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
		freeNurblist(&editNurb);
		
		cu= G.obedit->data;
		nu= cu->nurb.first;
		
		while(nu) {
			newnu= duplicateNurb(nu);
			test2DNurb(newnu);	// after join, or any other creation of curve
			BLI_addtail(&editNurb, newnu);
			nu= nu->next;
		}
		
		actkey = ob_get_keyblock(G.obedit);
		if(actkey) {
			strcpy(G.editModeTitleExtra, "(Key) ");
			key_to_curve(actkey, cu, &editNurb);
		}
	}
	else G.obedit= NULL;
	
	countall();
	set_actNurb(NULL);
}

void remake_editNurb()
{

	if(okee("Reload original data")==0) return;
	
	make_editNurb();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
}


void separate_nurb()
{
	Nurb *nu, *nu1;
	Object *oldob;
	Base *base, *oldbase;
	Curve *cu;
	ListBase editnurbo;

	if( G.vd==0 || (G.vd->lay & G.obedit->lay)==0 ) return;

	if(okee("Separate")==0) return;

	waitcursor(1);
	
	cu= G.obedit->data;
	if(cu->key) {
		error("Can't separate a curve with vertex keys");
		return;
	}
	
	/* we are going to trick everything as follows:
	 * 1. duplicate base: this is the new one,  remember old pointer
	 * 2. set aside all NOT selected curves/nurbs
	 * 3. load_ebaseNurb(): this will be the new base
	 * 4. freelist and restore old nurbs
	 */
	
	/* only edit-base selected */
	base= FIRSTBASE;
	while(base) {
		if(base->lay & G.vd->lay) {
			if(base->object==G.obedit) base->flag |= 1;
			else base->flag &= ~1;
		}
		base= base->next;
	}

	/* set aside: everything that is not selected */
	editnurbo.first= editnurbo.last= 0;
	nu= editNurb.first;
	while(nu) {
		nu1= nu->next;
		if(isNurbsel(nu)==0) {
			BLI_remlink(&editNurb, nu);
			BLI_addtail(&editnurbo, nu);
		}
		nu= nu1;
	}

	oldob= G.obedit;
	oldbase= BASACT;

	adduplicate(1, 0); /* no transform and zero so do get a linked dupli */
	
	G.obedit= BASACT->object;	/* basact is set in adduplicate() */
	
	G.obedit->data= copy_curve(cu);
	/* because new curve is a copy: reduce user count */
	cu->id.us--;
	
	load_editNurb();
	
	BASACT->flag &= ~SELECT;
	
	if(editNurb.first) freeNurblist(&editNurb);
	
	editNurb= editnurbo;
	
	G.obedit= 0;	/* displists behave different in edit mode */
	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);	/* this is the separated one */
	DAG_object_flush_update(G.scene, oldob, OB_RECALC_DATA);	/* this is the original one */
	
	G.obedit= oldob;
	BASACT= oldbase;
	BASACT->flag |= SELECT;
	
	waitcursor(0);

	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	set_actNurb(NULL);
}

/* ******************* FLAGS ********************* */


short isNurbselUV(Nurb *nu, int *u, int *v, int flag)
{
	/* return u!=-1:     1 row in u-direction selected. U has value between 0-pntsv 
     * return v!=-1: 1 collumn in v-direction selected. V has value between 0-pntsu 
     */
	BPoint *bp;
	int a, b, sel;

	*u= *v= -1;

	bp= nu->bp;
	for(b=0; b<nu->pntsv; b++) {
		sel= 0;
		for(a=0; a<nu->pntsu; a++, bp++) {
			if(bp->f1 & flag) sel++;
		}
		if(sel==nu->pntsu) {
			if(*u== -1) *u= b;
			else return 0;
		}
		else if(sel>1) return 0;    /* because sel==1 is still ok */
	}

	for(a=0; a<nu->pntsu; a++) {
		sel= 0;
		bp= nu->bp+a;
		for(b=0; b<nu->pntsv; b++, bp+=nu->pntsu) {
			if(bp->f1 & flag) sel++;
		}
		if(sel==nu->pntsv) {
			if(*v== -1) *v= a;
			else return 0;
		}
		else if(sel>1) return 0;
	}

	if(*u==-1 && *v>-1) return 1;
	if(*v==-1 && *u>-1) return 1;
	return 0;
}

void setflagsNurb(short flag)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	for(nu= editNurb.first; nu; nu= nu->next) {
		if( (nu->type & 7)==CU_BEZIER) {
			a= nu->pntsu;
			bezt= nu->bezt;
			while(a--) {
				bezt->f1= bezt->f2= bezt->f3= flag;
				bezt++;
			}
		}
		else {
			a= nu->pntsu*nu->pntsv;
			bp= nu->bp;
			while(a--) {
				bp->f1= flag;
				bp++;
			}
		}
	}
}

void rotateflagNurb(short flag, float *cent, float rotmat[][3])
{
	/* all verts with (flag & 'flag') rotate */
	Nurb *nu;
	BPoint *bp;
	int a;

	for(nu= editNurb.first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_NURBS) {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;

			while(a--) {
				if(bp->f1 & flag) {
					bp->vec[0]-=cent[0];
					bp->vec[1]-=cent[1];
					bp->vec[2]-=cent[2];
					Mat3MulVecfl(rotmat, bp->vec);
					bp->vec[0]+=cent[0];
					bp->vec[1]+=cent[1];
					bp->vec[2]+=cent[2];
				}
				bp++;
			}
		}
	}
}


void translateflagNurb(short flag, float *vec)
{
	/* all verts with ('flag' & flag) translate */
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	for(nu= editNurb.first; nu; nu= nu->next) {
		if( (nu->type & 7)==CU_BEZIER) {
			a= nu->pntsu;
			bezt= nu->bezt;
			while(a--) {
				if(bezt->f1 & flag) VecAddf(bezt->vec[0], bezt->vec[0], vec);
				if(bezt->f2 & flag) VecAddf(bezt->vec[1], bezt->vec[1], vec);
				if(bezt->f3 & flag) VecAddf(bezt->vec[2], bezt->vec[2], vec);
				bezt++;
			}
		}
		else {
			a= nu->pntsu*nu->pntsv;
			bp= nu->bp;
			while(a--) {
				if(bp->f1 & flag) VecAddf(bp->vec, bp->vec, vec);
				bp++;
			}
		}

		test2DNurb(nu);
	}
}

void weightflagNurb(short flag, float w, int mode)	/* mode==0: replace, mode==1: multiply */
{
	Nurb *nu;
	BPoint *bp;
	int a;

	for(nu= editNurb.first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_NURBS) {
			a= nu->pntsu*nu->pntsv;
			bp= nu->bp;
			while(a--) {
				if(bp->f1 & flag) {
					if(mode==1) bp->vec[3]*= w;
					else bp->vec[3]= w;
				}
				bp++;
			}
		}
	}
}

void deleteflagNurb(short flag)
{
	Nurb *nu, *next;
	BPoint *bp, *bpn, *newbp;
	int a, b, newu, newv, sel;

	if(G.obedit && G.obedit->type==OB_SURF);
	else return;

	lastselbp= NULL;

	nu= editNurb.first;
	while(nu) {
		next= nu->next;

		/* is entire nurb selected */
		bp= nu->bp;
		a= nu->pntsu*nu->pntsv;
		while(a) {
			a--;
			if(bp->f1 & flag);
			else break;
			bp++;
		}
		if(a==0) {
			BLI_remlink(&editNurb, nu);
			freeNurb(nu); nu=NULL;
		}
		else {
			/* is nurb in U direction selected */
			newv= nu->pntsv;
			bp= nu->bp;
			for(b=0; b<nu->pntsv; b++) {
				sel= 0;
				for(a=0; a<nu->pntsu; a++, bp++) {
					if(bp->f1 & flag) sel++;
				}
				if(sel==nu->pntsu) {
					newv--;
				}
				else if(sel>=1) {
					/* don't delete */
					break;
				}
			}
			if(newv!=nu->pntsv && b==nu->pntsv)	{
				/* delete */
				bp= nu->bp;
				bpn = newbp =
					(BPoint*) MEM_mallocN(newv * nu->pntsu * sizeof(BPoint), "deleteNurb");
				for(b=0; b<nu->pntsv; b++) {
					if((bp->f1 & flag)==0) {
						memcpy(bpn, bp, nu->pntsu*sizeof(BPoint));
						bpn+= nu->pntsu;
					}
					bp+= nu->pntsu;
				}
				nu->pntsv= newv;
				MEM_freeN(nu->bp);
				nu->bp= newbp;
				clamp_nurb_order_v(nu);

				makeknots(nu, 2, nu->flagv>>1);
			}
			else {
				/* is the nurb in V direction selected */
				newu= nu->pntsu;
				for(a=0; a<nu->pntsu; a++) {
					bp= nu->bp+a;
					sel= 0;
					for(b=0; b<nu->pntsv; b++, bp+=nu->pntsu) {
						if(bp->f1 & flag) sel++;
					}
					if(sel==nu->pntsv) {
						newu--;
					}
					else if(sel>=1) {
						/* don't delete */
						break;
					}
				}
				if(newu!=nu->pntsu && a==nu->pntsu)	{
					/* delete */
					bp= nu->bp;
					bpn = newbp =
						(BPoint*) MEM_mallocN(newu * nu->pntsv * sizeof(BPoint), "deleteNurb");
					for(b=0; b<nu->pntsv; b++) {
						for(a=0; a<nu->pntsu; a++, bp++) {
							if((bp->f1 & flag)==0) {
								*bpn= *bp;
								bpn++;
							}
						}
					}
					MEM_freeN(nu->bp);
					nu->bp= newbp;
					if(newu==1 && nu->pntsv>1) {    /* make a U spline */
						nu->pntsu= nu->pntsv;
						nu->pntsv= 1;
						SWAP(short, nu->orderu, nu->orderv);
						clamp_nurb_order_u(nu);
						if(nu->knotsv) MEM_freeN(nu->knotsv);
						nu->knotsv= NULL;
					}
					else {
						nu->pntsu= newu;
						clamp_nurb_order_u(nu);
					}
					makeknots(nu, 1, nu->flagu>>1);
				}
			}
		}
		nu= next;
	}
}

short extrudeflagNurb(int flag)
{
	Nurb *nu;
	BPoint *bp, *bpn, *newbp;
	int ok= 0, a, u, v, len;

	if(G.obedit && G.obedit->type==OB_SURF);
	else return 0;

	nu= editNurb.first;
	while(nu) {

		if(nu->pntsv==1) {
			bp= nu->bp;
			a= nu->pntsu;
			while(a) {
				if(bp->f1 & flag);
				else break;
				bp++;
				a--;
			}
			if(a==0) {
				ok= 1;
				newbp =
					(BPoint*)MEM_mallocN(2 * nu->pntsu * sizeof(BPoint), "extrudeNurb1");
				memcpy(newbp, nu->bp, nu->pntsu*sizeof(BPoint) );
				bp= newbp+ nu->pntsu;
				memcpy(bp, nu->bp, nu->pntsu*sizeof(BPoint) );
				MEM_freeN(nu->bp);
				nu->bp= newbp;
				a= nu->pntsu;
				while(a--) {
					select_bpoint(bp, SELECT, flag, HIDDEN);
					select_bpoint(newbp, DESELECT, flag, HIDDEN);
					bp++; 
					newbp++;
				}

				nu->pntsv= 2;
				nu->orderv= 2;
				makeknots(nu, 2, nu->flagv>>1);
			}
		}
		else {
			/* which row or collumn is selected */

			if( isNurbselUV(nu, &u, &v, flag) ) {

				/* deselect all */
				bp= nu->bp;
				a= nu->pntsu*nu->pntsv;
				while(a--) {
					select_bpoint(bp, DESELECT, flag, HIDDEN);
					bp++;
				}

				if(u==0 || u== nu->pntsv-1) {	    /* row in u-direction selected */
					ok= 1;
					newbp =
						(BPoint*) MEM_mallocN(nu->pntsu*(nu->pntsv + 1)
										  * sizeof(BPoint), "extrudeNurb1");
					if(u==0) {
						len= nu->pntsv*nu->pntsu;
						memcpy(newbp+nu->pntsu, nu->bp, len*sizeof(BPoint) );
						memcpy(newbp, nu->bp, nu->pntsu*sizeof(BPoint) );
						bp= newbp;
					}
					else {
						len= nu->pntsv*nu->pntsu;
						memcpy(newbp, nu->bp, len*sizeof(BPoint) );
						memcpy(newbp+len, nu->bp+len-nu->pntsu, nu->pntsu*sizeof(BPoint) );
						bp= newbp+len;
					}

					a= nu->pntsu;
					while(a--) {
						select_bpoint(bp, SELECT, flag, HIDDEN);
						bp++;
					}

					MEM_freeN(nu->bp);
					nu->bp= newbp;
					nu->pntsv++;
					if(nu->resolv<3) nu->resolv++;
					makeknots(nu, 2, nu->flagv>>1);
				}
				else if(v==0 || v== nu->pntsu-1) {	    /* collumn in v-direction selected */
					ok= 1;
					bpn = newbp =
						(BPoint*) MEM_mallocN((nu->pntsu + 1) * nu->pntsv * sizeof(BPoint), "extrudeNurb1");
					bp= nu->bp;

					for(a=0; a<nu->pntsv; a++) {
						if(v==0) {
							*bpn= *bp;
							bpn->f1 |= flag;
							bpn++;
						}
						memcpy(bpn, bp, nu->pntsu*sizeof(BPoint));
						bp+= nu->pntsu;
						bpn+= nu->pntsu;
						if(v== nu->pntsu-1) {
							*bpn= *(bp-1);
							bpn->f1 |= flag;
							bpn++;
						}
					}

					MEM_freeN(nu->bp);
					nu->bp= newbp;
					nu->pntsu++;
					if(nu->resolu<3) nu->resolu++;
					makeknots(nu, 1, nu->flagu>>1);
				}
			}
		}
		nu= nu->next;
	}

	return ok;
}

void adduplicateflagNurb(short flag)
{
	Nurb *nu, *newnu;
	BezTriple *bezt, *bezt1;
	BPoint *bp, *bp1;
	int a, b, starta, enda, newu, newv;
	char *usel;

	nu= editNurb.last;
	while(nu) {
		if( (nu->type & 7)==CU_BEZIER) {
			bezt= nu->bezt;
			for(a=0; a<nu->pntsu; a++) {
				enda= -1;
				starta= a;
				while( (bezt->f1 & flag) || (bezt->f2 & flag) || (bezt->f3 & flag) ) {
					select_beztriple(bezt, DESELECT, flag, HIDDEN);
					enda=a;
					if(a>=nu->pntsu-1) break;
					a++;
					bezt++;
				}
				if(enda>=starta) {
					newnu = (Nurb*)MEM_mallocN(sizeof(Nurb), "adduplicateN");  
					memcpy(newnu, nu, sizeof(Nurb));
					BLI_addtail(&editNurb, newnu);
					set_actNurb(newnu);
					newnu->pntsu= enda-starta+1;
					newnu->bezt=
						(BezTriple*)MEM_mallocN((enda - starta + 1) * sizeof(BezTriple), "adduplicateN");  
					memcpy(newnu->bezt, nu->bezt+starta, newnu->pntsu*sizeof(BezTriple));

					b= newnu->pntsu;
					bezt1= newnu->bezt;
					while(b--) {
						select_beztriple(bezt1, SELECT, flag, HIDDEN);
						bezt1++;
					}

					if(nu->flagu & CU_CYCLIC) {
						if(starta!=0 || enda!=nu->pntsu-1) {
							newnu->flagu &= ~CU_CYCLIC;
						}
					}
				}
				bezt++;
			}
		}
		else if(nu->pntsv==1) {	/* because UV Nurb has a different method for dupli */
			bp= nu->bp;
			for(a=0; a<nu->pntsu; a++) {
				enda= -1;
				starta= a;
				while(bp->f1 & flag) {
					select_bpoint(bp, DESELECT, flag, HIDDEN);
					enda= a;
					if(a>=nu->pntsu-1) break;
					a++;
					bp++;
				}
				if(enda>=starta) {
					newnu = (Nurb*)MEM_mallocN(sizeof(Nurb), "adduplicateN3");  
					memcpy(newnu, nu, sizeof(Nurb));
					set_actNurb(newnu);
					BLI_addtail(&editNurb, newnu);
					newnu->pntsu= enda-starta+1;
					newnu->bp = (BPoint*)MEM_mallocN((enda-starta+1) * sizeof(BPoint), "adduplicateN4");
					memcpy(newnu->bp, nu->bp+starta, newnu->pntsu*sizeof(BPoint));

					b= newnu->pntsu;
					bp1= newnu->bp;
					while(b--) {
						select_bpoint(bp1, SELECT, flag, HIDDEN);
						bp1++;
					}

					if(nu->flagu & CU_CYCLIC) {
						if(starta!=0 || enda!=nu->pntsu-1) {
							newnu->flagu &= ~CU_CYCLIC;
						}
					}

					/* knots */
					newnu->knotsu= NULL;
					makeknots(newnu, 1, newnu->flagu>>1);
				}
				bp++;
			}
		}
		else {
			/* a rectangular area in nurb has to be selected */
			if(isNurbsel(nu)) {
				usel= MEM_callocN(nu->pntsu, "adduplicateN4");
				bp= nu->bp;
				for(a=0; a<nu->pntsv; a++) {
					for(b=0; b<nu->pntsu; b++, bp++) {
						if(bp->f1 & flag) usel[b]++;
					}
				}
				newu= 0;
				newv= 0;
				for(a=0; a<nu->pntsu; a++) {
					if(usel[a]) {
						if(newv==0 || usel[a]==newv) {
							newv= usel[a];
							newu++;
						}
						else {
							newv= 0;
							break;
						}
					}
				}
				if(newu==0 || newv==0) {
					printf("Can't duplicate Nurb\n");
				}
				else {

					if(newu==1) SWAP(short, newu, newv);

					newnu = (Nurb*)MEM_mallocN(sizeof(Nurb), "adduplicateN5");
					memcpy(newnu, nu, sizeof(Nurb));
					BLI_addtail(&editNurb, newnu);
					set_actNurb(newnu);
					newnu->pntsu= newu;
					newnu->pntsv= newv;
					newnu->bp =
						(BPoint*)MEM_mallocN(newu * newv * sizeof(BPoint), "adduplicateN6");
					clamp_nurb_order_u(newnu);
					clamp_nurb_order_v(newnu);
					
					newnu->knotsu= newnu->knotsv= NULL;
					
					bp= newnu->bp;
					bp1= nu->bp;
					for(a=0; a<nu->pntsv; a++) {
						for(b=0; b<nu->pntsu; b++, bp1++) {
							if(bp1->f1 & flag) {
								memcpy(bp, bp1, sizeof(BPoint));
								select_bpoint(bp1, DESELECT, flag, HIDDEN);
								bp++;
							}
						}
					}
					if (check_valid_nurb_u(newnu)) {
						if(nu->pntsu==newnu->pntsu && nu->knotsu) {
							newnu->knotsu= MEM_dupallocN( nu->knotsu );
						} else {
							makeknots(newnu, 1, newnu->flagu>>1);
						}
					}
					if (check_valid_nurb_v(newnu)) {
						if(nu->pntsv==newnu->pntsv && nu->knotsv) {
							newnu->knotsv= MEM_dupallocN( nu->knotsv );
						} else {
							makeknots(newnu, 2, newnu->flagv>>1);
						}
					}
				}
				MEM_freeN(usel);
			}
		}

		nu= nu->prev;
	}
	
	/* actnu changed */
	allqueue(REDRAWBUTSEDIT, 0);
}


void switchdirectionNurb2(void)
{
	Nurb *nu;
	
	if(G.vd==0 || !(G.obedit->lay & G.vd->lay))
		return;
	
	for(nu= editNurb.first; nu; nu= nu->next) {
		if( isNurbsel(nu) ) switchdirectionNurb(nu);
	}
	
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Switch direction");
}

void switchdirection_knots(float *base, int tot)
{
	float *fp1, *fp2, *tempf;
	int a;
	
	if(base==NULL || tot==0) return;
	
	/* reverse knots */
	a= tot;
	fp1= base;
	fp2= fp1+(a-1);
	a/= 2;
	while(fp1!=fp2 && a>0) {
		SWAP(float, *fp1, *fp2);
		a--;
		fp1++; 
		fp2--;
	}
	/* and make in increasing order again */
	a= tot;
	fp1= base;
	fp2=tempf= MEM_mallocN(sizeof(float)*a, "switchdirect");
	while(a--) {
		fp2[0]= fabs(fp1[1]-fp1[0]);
		fp1++;
		fp2++;
	}

	a= tot-1;
	fp1= base;
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

void setweightNurb(void)
{
	static float weight= 1.0f;
	extern ListBase editNurb;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
				
	if(fbutton(&weight, 0.0f, 1.0f, 10, 10, "Set Weight")) {
		for(nu= editNurb.first; nu; nu= nu->next) {
			if(nu->bezt) {
				for(bezt=nu->bezt, a=0; a<nu->pntsu; a++, bezt++) {
					if(bezt->f2 & SELECT)
						bezt->weight= weight;
				}
			}
			else if(nu->bp) {
				for(bp=nu->bp, a=0; a<nu->pntsu*nu->pntsv; a++, bp++) {
					if(bp->f1 & SELECT)
						bp->weight= weight;
				}
			}
		}	
	}
	BIF_undo_push("Set Curve Weight");
	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
}

void setradiusNurb( void )
{
	static float radius= 1.0f;
	extern ListBase editNurb;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
	
	if(fbutton(&radius, 0.0001f, 10.0f, 10, 10, "Set Radius")) {
		for(nu= editNurb.first; nu; nu= nu->next) {
			if(nu->bezt) {
				for(bezt=nu->bezt, a=0; a<nu->pntsu; a++, bezt++) {
					if(bezt->f2 & SELECT)
						bezt->radius= radius;
				}
			}
			else if(nu->bp) {
				for(bp=nu->bp, a=0; a<nu->pntsu*nu->pntsv; a++, bp++) {
					if(bp->f1 & SELECT)
						bp->radius= radius;
				}
			}
		}	
	}
	BIF_undo_push("Set Curve Radius");
	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSALL, 0);
	allqueue(REDRAWINFO, 1); 	/* 1, because header->win==0! */
}

void smoothNurb( void )
{

	extern ListBase editNurb;
	Nurb *nu;
	BezTriple *bezt, *beztOrig;
	BPoint *bp, *bpOrig;
	int a, i, change = 0;
	
	/* floats for smoothing */
	float val, newval, offset;
	
	for(nu= editNurb.first; nu; nu= nu->next) {
		if(nu->bezt) {
			change = 0;
			beztOrig = MEM_dupallocN( nu->bezt );
			for(bezt=nu->bezt+1, a=1; a<nu->pntsu-1; a++, bezt++) {
				if(bezt->f2 & SELECT) {
					for(i=0; i<3; i++) {
						val = bezt->vec[1][i];
						newval = ((beztOrig+(a-1))->vec[1][i] * 0.5) + ((beztOrig+(a+1))->vec[1][i] * 0.5);
						offset = (val*((1.0/6.0)*5)) + (newval*(1.0/6.0)) - val;
						/* offset handles */
						bezt->vec[1][i] += offset;
						bezt->vec[0][i] += offset;
						bezt->vec[2][i] += offset;
					}
					change = 1;
				}
			}
			MEM_freeN(beztOrig);
			if (change)
				calchandlesNurb(nu);
		} else if (nu->bp) {
			bpOrig = MEM_dupallocN( nu->bp );
			/* Same as above, keep these the same! */
			for(bp=nu->bp+1, a=1; a<nu->pntsu-1; a++, bp++) {
				if(bp->f1 & SELECT) {
					for(i=0; i<3; i++) {
						val = bp->vec[i];
						newval = ((bpOrig+(a-1))->vec[i] * 0.5) + ((bpOrig+(a+1))->vec[i] * 0.5);
						offset = (val*((1.0/6.0)*5)) + (newval*(1.0/6.0)) - val;
					
						bp->vec[i] += offset;
					}
				}
			}
			MEM_freeN(bpOrig);
		}
	}
	BIF_undo_push("Smooth Curve");
	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSALL, 0);
	allqueue(REDRAWINFO, 1); 	/* 1, because header->win==0! */
}

/* TODO, make smoothing distance based */
void smoothradiusNurb( void )
{
	extern ListBase editNurb;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
	
	/* use for smoothing */
	int last_sel;
	int start_sel, end_sel; /* selection indicies, inclusive */
	float start_rad, end_rad, fac, range;
	
	for(nu= editNurb.first; nu; nu= nu->next) {
		if(nu->bezt) {
			
			for (last_sel=0; last_sel < nu->pntsu; last_sel++) {
				/* loop over selection segments of a curve, smooth each */
				
				/* Start BezTriple code, this is duplicated below for points, make sure these functions stay in sync */
				start_sel = end_sel = -1;
				for(bezt=nu->bezt+last_sel, a=last_sel; a<nu->pntsu; a++, bezt++) {
					if(bezt->f2 & SELECT) {
						start_sel = a;
						break;
					}
				}
				/* incase there are no other selected verts */
				end_sel = start_sel;
				for(bezt=nu->bezt+(start_sel+1), a=start_sel+1; a<nu->pntsu; a++, bezt++) {
					if((bezt->f2 & SELECT)==0) {
						break;
					}
					end_sel = a;
				}
				
				if (start_sel == -1) {
					last_sel = nu->pntsu; /* next... */
				} else {
					last_sel = end_sel; /* before we modify it */
					
					/* now blend between start and end sel */
					start_rad = end_rad = -1.0;
					
					if (start_sel == end_sel) {
						/* simple, only 1 point selected */
						if (start_sel>0)						start_rad = (nu->bezt+start_sel-1)->radius;
						if (end_sel!=-1 && end_sel < nu->pntsu)	end_rad = (nu->bezt+start_sel+1)->radius;
						
						if (start_rad >= 0.0 && end_rad >= 0.0)	(nu->bezt+start_sel)->radius = (start_rad + end_rad)/2;
						else if (start_rad >= 0.0)				(nu->bezt+start_sel)->radius = start_rad;
						else if (end_rad >= 0.0)				(nu->bezt+start_sel)->radius = end_rad;
					} else {
						/* if endpoints selected, then use them */
						if (start_sel==0) {
							start_rad = (nu->bezt+start_sel)->radius;
							start_sel++; /* we dont want to edit the selected endpoint */
						} else {
							start_rad = (nu->bezt+start_sel-1)->radius;
						}
						if (end_sel==nu->pntsu-1) {
							end_rad = (nu->bezt+end_sel)->radius;
							end_sel--; /* we dont want to edit the selected endpoint */
						} else {
							end_rad = (nu->bezt+end_sel+1)->radius;
						}
						
						/* Now Blend between the points */
						range = (float)(end_sel - start_sel) + 2.0f;
						for(bezt=nu->bezt+start_sel, a=start_sel; a<=end_sel; a++, bezt++) {
							fac = (float)(1+a-start_sel) / range;
							bezt->radius = start_rad*(1.0-fac) + end_rad*fac;
						}
					}
				}
			}
		} else if (nu->bp) {
			/* Same as above, keep these the same! */
			for (last_sel=0; last_sel < nu->pntsu; last_sel++) {
				/* loop over selection segments of a curve, smooth each */
				
				/* Start BezTriple code, this is duplicated below for points, make sure these functions stay in sync */
				start_sel = end_sel = -1;
				for(bp=nu->bp+last_sel, a=last_sel; a<nu->pntsu; a++, bp++) {
					if(bp->f1 & SELECT) {
						start_sel = a;
						break;
					}
				}
				/* incase there are no other selected verts */
				end_sel = start_sel;
				for(bp=nu->bp+(start_sel+1), a=start_sel+1; a<nu->pntsu; a++, bp++) {
					if((bp->f1 & SELECT)==0) {
						break;
					}
					end_sel = a;
				}
				
				if (start_sel == -1) {
					last_sel = nu->pntsu; /* next... */
				} else {
					last_sel = end_sel; /* before we modify it */
					
					/* now blend between start and end sel */
					start_rad = end_rad = -1.0;
					
					if (start_sel == end_sel) {
						/* simple, only 1 point selected */
						if (start_sel>0)						start_rad = (nu->bp+start_sel-1)->radius;
						if (end_sel!=-1 && end_sel < nu->pntsu)	end_rad = (nu->bp+start_sel+1)->radius;
						
						if (start_rad >= 0.0 && end_rad >= 0.0)	(nu->bp+start_sel)->radius = (start_rad + end_rad)/2;
						else if (start_rad >= 0.0)				(nu->bp+start_sel)->radius = start_rad;
						else if (end_rad >= 0.0)				(nu->bp+start_sel)->radius = end_rad;
					} else {
						/* if endpoints selected, then use them */
						if (start_sel==0) {
							start_rad = (nu->bp+start_sel)->radius;
							start_sel++; /* we dont want to edit the selected endpoint */
						} else {
							start_rad = (nu->bp+start_sel-1)->radius;
						}
						if (end_sel==nu->pntsu-1) {
							end_rad = (nu->bp+end_sel)->radius;
							end_sel--; /* we dont want to edit the selected endpoint */
						} else {
							end_rad = (nu->bp+end_sel+1)->radius;
						}
						
						/* Now Blend between the points */
						range = (float)(end_sel - start_sel) + 2.0f;
						for(bp=nu->bp+start_sel, a=start_sel; a<=end_sel; a++, bp++) {
							fac = (float)(1+a-start_sel) / range;
							bp->radius = start_rad*(1.0-fac) + end_rad*fac;
						}
					}
				}
			}
		}
	}
	BIF_undo_push("Smooth Curve Radius");
	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSALL, 0);
	allqueue(REDRAWINFO, 1); 	/* 1, because header->win==0! */
}



/* **************** EDIT ************************ */

/* next == 1 -> select next 		*/
/* next == -1 -> select previous 	*/
/* cont == 1 -> select continuously 	*/
/* selstatus, inverts behaviour		*/
static void select_adjacent_cp(short next, short cont, short selstatus)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
	short lastsel= 0, sel=0;
	
	if(next==0) return;
	
	for(nu= editNurb.first; nu; nu= nu->next) {
		lastsel=0;
		if((nu->type & 7)==CU_BEZIER) {			
			a= nu->pntsu;
			bezt= nu->bezt;
			if(next < 0) bezt= (nu->bezt + (a-1));
			while(a--) {
				if(a-abs(next) < 0) break;
				sel= 0;
				if((lastsel==0) && (bezt->hide==0) && ((bezt->f2 & SELECT) || (selstatus==0))) {
					bezt+=next;
					if(!(bezt->f2 & SELECT) || (selstatus==0)) {
						sel= select_beztriple(bezt, selstatus, 1, VISIBLE);	
						if((sel==1) && (cont==0)) lastsel= 1;
					}							
				}
				else {
					bezt+=next;
					lastsel= 0;
				}
				/* move around in zigzag way so that we go through each */				
				bezt-=(next-next/abs(next));				
			}
		}
		else {
			a= nu->pntsu*nu->pntsv;
			bp= nu->bp;
			if(next < 0) bp= (nu->bp + (a-1));
			while(a--) {
				if(a-abs(next) < 0) break;
				sel=0;
				if((lastsel==0) && (bp->hide==0) && ((bp->f1 & SELECT) || (selstatus==0))) {
					bp+=next;
					if(!(bp->f1 & SELECT) || (selstatus==0)) {
						sel= select_bpoint(bp, selstatus, 1, VISIBLE);
						if((sel==1) && (cont==0)) lastsel= 1;
					}			
				}
				else {
					bp+=next;
					lastsel= 0;
				}
				/* move around in zigzag way so that we go through each */
				bp-=(next-next/abs(next));				
			}
		}
	}
}

static short nurb_has_selected_cps()
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	for(nu= editNurb.first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			a= nu->pntsu;
			bezt= nu->bezt;
			while(a--) {
				if(bezt->hide==0) {
					if((bezt->f1 & SELECT)
					|| (bezt->f2 & SELECT)
					|| (bezt->f3 & SELECT)) return 1;
				}
				bezt++;
			}
		}
		else {
			a= nu->pntsu*nu->pntsv;
			bp= nu->bp;
			while(a--) {
				if((bp->hide==0) && (bp->f1 & SELECT)) return 1;
				bp++;
			}
		}
	}
	
	return 0;
}

void deselectall_nurb()
{
	if(!G.vd || !(G.obedit->lay & G.vd->lay))
		return;

	if(nurb_has_selected_cps()) { /* deselect all */
		selectend_nurb(FIRST, 0, DESELECT); /* set first control points as unselected */
		select_adjacent_cp(1, 1, DESELECT); /* cascade selection */	
	}
	else { /* select all */
		selectend_nurb(FIRST, 0, SELECT); /* set first control points as selected */
		select_adjacent_cp(1, 1, SELECT); /* cascade selection */
 	}
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Deselect all");
}

void hideNurb(int swap)
{
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int a, sel;

	if(G.obedit==0) return;

	BIF_undo_push("Hide");

	for(nu= editNurb.first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			sel= 0;
			while(a--) {
				if(BEZSELECTED_HIDDENHANDLES(bezt)) {
					select_beztriple(bezt, DESELECT, 1, HIDDEN);
					bezt->hide= 1;
				}
				if(bezt->hide) sel++;
				bezt++;
			}
			if(sel==nu->pntsu) nu->hide= 1;
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			sel= 0;
			while(a--) {
				if(swap==0 && (bp->f1 & SELECT)) {
					select_bpoint(bp, DESELECT, 1, HIDDEN);
					bp->hide= 1;
				}
				else if(swap && (bp->f1 & SELECT)==0) {
					select_bpoint(bp, DESELECT, 1, HIDDEN);
					bp->hide= 1;
				}
				if(bp->hide) sel++;
				bp++;
			}
			if(sel==nu->pntsu*nu->pntsv) nu->hide= 1;
		}
	}

	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
}

void revealNurb()
{
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int a;

	if(G.obedit==0) return;

	for(nu= editNurb.first; nu; nu= nu->next) {
		nu->hide= 0;
		if((nu->type & 7)==CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(bezt->hide) {
					select_beztriple(bezt, SELECT, 1, HIDDEN);
					bezt->hide= 0;
				}
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				if(bp->hide) {
					select_bpoint(bp, SELECT, 1, HIDDEN);
					bp->hide= 0;
				}
				bp++;
			}
		}
	}

	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	countall();
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Reveal");

}

void selectswapNurb()
{
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int a;

	if(G.obedit==0) return;

	for(nu= editNurb.first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(bezt->hide==0) {
					bezt->f2 ^= SELECT; /* always do the center point */
					if ((G.f & G_HIDDENHANDLES)==0) {
						bezt->f1 ^= SELECT;
						bezt->f3 ^= SELECT;
					}
				}
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				swap_selection_bpoint(bp);
				bp++;
			}
		}
	}

	countall();
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Select swap");

}

/** Divide the line segments associated with the currently selected
 * curve nodes (Bezier or NURB). If there are no valid segment
 * selections within the current selection, nothing happens.
 *
 * @deffunc subdividenurb subdivideNurb(void)
 * @return Nothing
 * @param  None
*/
void subdivideNurb()
{
	Nurb *nu;
	BezTriple *prevbezt, *bezt, *beztnew, *beztn;
	BPoint *bp, *prevbp, *bpnew, *bpn;
	float vec[15];
	int a, b, sel, amount, *usel, *vsel;

   // printf("*** subdivideNurb: entering subdivide\n");

	for(nu= editNurb.first; nu; nu= nu->next) {
		amount= 0;
		if((nu->type & 7)==CU_BEZIER) {
        /* 
           Insert a point into a 2D Bezier curve. 
           Endpoints are preserved. Otherwise, all selected and inserted points are 
           newly created. Old points are discarded.
        */
			/* count */
			if(nu->flagu & CU_CYCLIC) {
				a= nu->pntsu;
				bezt= nu->bezt;
				prevbezt= bezt+(a-1);
			}
			else {
				a= nu->pntsu-1;
				prevbezt= nu->bezt;
				bezt= prevbezt+1;
			}
			while(a--) {
				if( BEZSELECTED_HIDDENHANDLES(prevbezt) && BEZSELECTED_HIDDENHANDLES(bezt) ) amount++;
				prevbezt= bezt;
				bezt++;
			}

			if(amount) {
				/* insert */
				beztnew =
					(BezTriple*)MEM_mallocN((amount + nu->pntsu) * sizeof(BezTriple), "subdivNurb");
				beztn= beztnew;
				if(nu->flagu & CU_CYCLIC) {
					a= nu->pntsu;
					bezt= nu->bezt;
					prevbezt= bezt+(a-1);
				}
				else {
					a= nu->pntsu-1;
					prevbezt= nu->bezt;
					bezt= prevbezt+1;
				}
				while(a--) {
					memcpy(beztn, prevbezt, sizeof(BezTriple));
					beztn++;

					if( BEZSELECTED_HIDDENHANDLES(prevbezt) && BEZSELECTED_HIDDENHANDLES(bezt) ) {
						memcpy(beztn, bezt, sizeof(BezTriple));
						
						/* midpoint subdividing */
						VecMidf(vec, prevbezt->vec[1], prevbezt->vec[2]);
						VecMidf(vec+3, prevbezt->vec[2], bezt->vec[0]);
						VecMidf(vec+6, bezt->vec[0], bezt->vec[1]);
						
						VecMidf(vec+9, vec, vec+3);
						VecMidf(vec+12, vec+3, vec+6);
						
						/* change handle of prev beztn */
						VECCOPY((beztn-1)->vec[2], vec);
						/* new point */
						VECCOPY(beztn->vec[0], vec+9);
						VecMidf(beztn->vec[1], vec+9, vec+12);
						VECCOPY(beztn->vec[2], vec+12);
						/* handle of next bezt */
						if(a==0 && (nu->flagu & CU_CYCLIC)) {VECCOPY(beztnew->vec[0], vec+6);}
						else {VECCOPY(bezt->vec[0], vec+6);}
						
						beztn->radius = (prevbezt->radius + bezt->radius)/2.0f;
						beztn->weight = (prevbezt->weight + bezt->weight)/2.0f;
						
						beztn++;
					}

					prevbezt= bezt;
					bezt++;
				}
				/* last point */
				if((nu->flagu & CU_CYCLIC)==0) memcpy(beztn, prevbezt, sizeof(BezTriple));

				MEM_freeN(nu->bezt);
				nu->bezt= beztnew;
				nu->pntsu+= amount;

				calchandlesNurb(nu);
			}
		} /* End of 'if((nu->type & 7)==CU_BEZIER)' */
		else if (nu->pntsv==1) {
        /* 
           All flat lines (ie. co-planar), except flat Nurbs. Flat NURB curves 
           are handled together with the regular NURB plane division, as it 
           should be. I split it off just now, let's see if it is
           stable... nzc 30-5-'00
         */
			/* count */
			if(nu->flagu & CU_CYCLIC) {
				a= nu->pntsu;
				bp= nu->bp;
				prevbp= bp+(a-1);
			}
			else {
				a= nu->pntsu-1;
				prevbp= nu->bp;
				bp= prevbp+1;
			}
			while(a--) {
				if( (bp->f1 & SELECT) && (prevbp->f1 & SELECT) ) amount++;
				prevbp= bp;
				bp++;
			}

			if(amount) {
				/* insert */
				bpnew =
					(BPoint*)MEM_mallocN((amount + nu->pntsu) * sizeof(BPoint), "subdivNurb2");
				bpn= bpnew;

				if(nu->flagu & CU_CYCLIC) {
					a= nu->pntsu;
					bp= nu->bp;
					prevbp= bp+(a-1);
				}
				else {
					a= nu->pntsu-1;
					prevbp= nu->bp;
					bp= prevbp+1;
				}
				while(a--) {
					memcpy(bpn, prevbp, sizeof(BPoint));
					bpn++;

					if( (bp->f1 & SELECT) && (prevbp->f1 & SELECT) ) {
                 // printf("*** subdivideNurb: insert 'linear' point\n");
						memcpy(bpn, bp, sizeof(BPoint));
						bpn->vec[0]= (prevbp->vec[0]+bp->vec[0])/2.0;
						bpn->vec[1]= (prevbp->vec[1]+bp->vec[1])/2.0;
						bpn->vec[2]= (prevbp->vec[2]+bp->vec[2])/2.0;
						bpn->vec[3]= (prevbp->vec[3]+bp->vec[3])/2.0;
						bpn++;

					}
					prevbp= bp;
					bp++;
				}
				if((nu->flagu & CU_CYCLIC)==0) memcpy(bpn, prevbp, sizeof(BPoint));	/* last point */

				MEM_freeN(nu->bp);
				nu->bp= bpnew;
				nu->pntsu+= amount;

				if(nu->type & 4) {
					makeknots(nu, 1, nu->flagu>>1);
				}
			}
		} /* End of 'else if(nu->pntsv==1)' */
		else if((nu->type & 7)==CU_NURBS) {
        /* This is a very strange test ... */
        /** 
           Subdivide NURB surfaces - nzc 30-5-'00 -
           
             Subdivision of a NURB curve can be effected by adding a 
           control point (insertion of a knot), or by raising the
           degree of the functions used to build the NURB. The
           expression 

               degree = #knots - #controlpoints + 1 (J Walter piece)
               degree = #knots - #controlpoints     (Blender
                                                      implementation)
                 ( this is confusing.... what is true? Another concern
                 is that the JW piece allows the curve to become
                 explicitly 1st order derivative discontinuous, while
                 this is not what we want here... )

           is an invariant for a single NURB curve. Raising the degree
           of the NURB is done elsewhere; the degree is assumed
           constant during this opration. Degree is a property shared
           by all controlpoints in a curve (even though it is stored
           per control point - this can be misleading).
             Adding a knot is done by searching for the place in the
           knot vector where a certain knot value must be inserted, or
           by picking an appropriate knot value between two existing
           ones. The number of controlpoints that is influenced by the
           insertion depends on the order of the curve. A certain
           minimum number of knots is needed to form high-order
           curves, as can be seen from the equation above. In Blender,
           currently NURBs may be up to 6th order, so we modify at
           most 6 points. One point is added. For an n-degree curve,
           n points are discarded, and n+1 points inserted
           (so effectively, n points are modified).  (that holds for
           the JW piece, but it seems not for our NURBs)
              In practice, the knot spacing is copied, but the tail
           (the points following the insertion point) need to be
           offset to keep the knot series ascending. The knot series
           is always a series of monotonically ascending integers in
           Blender. When not enough control points are available to
           fit the order, duplicates of the endpoints are added as
           needed. 
        */
			/* selection-arrays */
			usel= MEM_callocN(sizeof(int)*nu->pntsu, "subivideNurb3");
			vsel= MEM_callocN(sizeof(int)*nu->pntsv, "subivideNurb3");
			sel= 0;

         /* Count the number of selected points. */
			bp= nu->bp;
			for(a=0; a<nu->pntsv; a++) {
				for(b=0; b<nu->pntsu; b++) {
					if(bp->f1 & SELECT) {
						usel[b]++;
						vsel[a]++;
						sel++;
					}
					bp++;
				}
			}
			if( sel == (nu->pntsu*nu->pntsv) ) {	/* subdivide entire nurb */
           /* Global subdivision is a special case of partial
              subdivision. Strange it is considered separately... */
				bpn=bpnew= MEM_mallocN( (2*nu->pntsu-1)*(2*nu->pntsv-1)*sizeof(BPoint), "subdivideNurb4");
				bp= nu->bp;
				/* first subdivide rows */
				for(a=0; a<nu->pntsv; a++) {
					for(b=0; b<nu->pntsu; b++) {
						*bpn= *bp;
						bpn++; 
						bp++;
						if(b<nu->pntsu-1) {
							*bpn= *bp;
							prevbp= bp-1;
							bpn->vec[0]= (prevbp->vec[0]+bp->vec[0])/2.0;
							bpn->vec[1]= (prevbp->vec[1]+bp->vec[1])/2.0;
							bpn->vec[2]= (prevbp->vec[2]+bp->vec[2])/2.0;
							bpn->vec[3]= (prevbp->vec[3]+bp->vec[3])/2.0;
							bpn++;
						}
					}
					bpn+= (2*nu->pntsu-1);
				}
				/* now insert new */
				bpn= bpnew+(2*nu->pntsu-1);
				bp= bpnew+(4*nu->pntsu-2);
				prevbp= bpnew;
				for(a=1; a<nu->pntsv; a++) {

					for(b=0; b<2*nu->pntsu-1; b++) {
						*bpn= *bp;
						bpn->vec[0]= (prevbp->vec[0]+bp->vec[0])/2.0;
						bpn->vec[1]= (prevbp->vec[1]+bp->vec[1])/2.0;
						bpn->vec[2]= (prevbp->vec[2]+bp->vec[2])/2.0;
						bpn->vec[3]= (prevbp->vec[3]+bp->vec[3])/2.0;
						bpn++; 
						bp++; 
						prevbp++;
					}
					bp+= (2*nu->pntsu-1);
					bpn+= (2*nu->pntsu-1);
					prevbp+= (2*nu->pntsu-1);
				}
				MEM_freeN(nu->bp);
				nu->bp= bpnew;
				nu->pntsu= 2*nu->pntsu-1;
				nu->pntsv= 2*nu->pntsv-1;
				makeknots(nu, 1, nu->flagu>>1);
				makeknots(nu, 2, nu->flagv>>1);
			} /* End of 'if(sel== nu->pntsu*nu->pntsv)' (subdivide entire NURB) */
			else {
				/* subdivide in v direction? */
				sel= 0;
				for(a=0; a<nu->pntsv-1; a++) {
					if(vsel[a]==nu->pntsu && vsel[a+1]==nu->pntsu) sel++;
				}

				if(sel) {   /* V ! */
					bpn=bpnew= MEM_mallocN( (sel+nu->pntsv)*nu->pntsu*sizeof(BPoint), "subdivideNurb4");
					bp= nu->bp;
					for(a=0; a<nu->pntsv; a++) {
						for(b=0; b<nu->pntsu; b++) {
							*bpn= *bp;
							bpn++; 
							bp++;
						}
						if( (a<nu->pntsv-1) && vsel[a]==nu->pntsu && vsel[a+1]==nu->pntsu ) {
							prevbp= bp- nu->pntsu;
							for(b=0; b<nu->pntsu; b++) {
                       /* 
                          This simple bisection must be replaces by a
                          subtle resampling of a number of points. Our 
                          task is made slightly easier because each
                          point in our curve is a separate data
                          node. (is it?)
                       */
								*bpn= *prevbp;
								bpn->vec[0]= (prevbp->vec[0]+bp->vec[0])/2.0;
								bpn->vec[1]= (prevbp->vec[1]+bp->vec[1])/2.0;
								bpn->vec[2]= (prevbp->vec[2]+bp->vec[2])/2.0;
								bpn->vec[3]= (prevbp->vec[3]+bp->vec[3])/2.0;
								bpn++;
								prevbp++;
								bp++;
							}
							bp-= nu->pntsu;
						}
					}
					MEM_freeN(nu->bp);
					nu->bp= bpnew;
					nu->pntsv+= sel;
					makeknots(nu, 2, nu->flagv>>1);
				}
				else {
					/* or in u direction? */
					sel= 0;
					for(a=0; a<nu->pntsu-1; a++) {
						if(usel[a]==nu->pntsv && usel[a+1]==nu->pntsv) sel++;
					}

					if(sel) {	/* U ! */
                 /* Inserting U points is sort of 'default' Flat curves only get */
                 /* U points inserted in them.                                   */
						bpn=bpnew= MEM_mallocN( (sel+nu->pntsu)*nu->pntsv*sizeof(BPoint), "subdivideNurb4");
						bp= nu->bp;
						for(a=0; a<nu->pntsv; a++) {
							for(b=0; b<nu->pntsu; b++) {
								*bpn= *bp;
								bpn++; 
								bp++;
								if( (b<nu->pntsu-1) && usel[b]==nu->pntsv && usel[b+1]==nu->pntsv ) {
                          /* 
                             One thing that bugs me here is that the
                             orders of things are not the same as in
                             the JW piece. Also, this implies that we
                             handle at most 3rd order curves? I miss
                             some symmetry here...
                          */
									prevbp= bp- 1;
									*bpn= *prevbp;
									bpn->vec[0]= (prevbp->vec[0]+bp->vec[0])/2.0;
									bpn->vec[1]= (prevbp->vec[1]+bp->vec[1])/2.0;
									bpn->vec[2]= (prevbp->vec[2]+bp->vec[2])/2.0;
									bpn->vec[3]= (prevbp->vec[3]+bp->vec[3])/2.0;
									bpn++;
								}
							}
						}
						MEM_freeN(nu->bp);
						nu->bp= bpnew;
						nu->pntsu+= sel;
						makeknots(nu, 1, nu->flagu>>1); /* shift knots
                                                     forward */
					}
				}
			}
			MEM_freeN(usel); 
			MEM_freeN(vsel);

		} /* End of 'if((nu->type & 7)==CU_NURBS)'  */
	}


	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Subdivide");

}

static void findnearestNurbvert__doClosest(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, int x, int y)
{
	struct { BPoint *bp; BezTriple *bezt; Nurb *nurb; short dist, hpoint, select, mval[2]; } *data = userData;

	short flag;
	short temp;

	if (bp) {
		flag = bp->f1;
	} else {
		if (beztindex==0) {
			flag = bezt->f1;
		} else if (beztindex==1) {
			flag = bezt->f2;
		} else {
			flag = bezt->f3;
		}
	}

	temp = abs(data->mval[0]-x) + abs(data->mval[1]-y);
	if ((flag&1)==data->select) temp += 5;
	if (bezt && beztindex==1) temp += 3; /* middle points get a small disadvantage */

	if (temp<data->dist) {
		data->dist = temp;

		data->bp = bp;
		data->bezt = bezt;
		data->nurb = nu;
		data->hpoint = bezt?beztindex:0;
	}
}

static short findnearestNurbvert(short sel, Nurb **nurb, BezTriple **bezt, BPoint **bp)
{
		/* sel==1: selected gets a disadvantage */
		/* in nurb and bezt or bp the nearest is written */
		/* return 0 1 2: handlepunt */
	struct { BPoint *bp; BezTriple *bezt; Nurb *nurb; short dist, hpoint, select, mval[2]; } data = {0};

	data.dist = 100;
	data.hpoint = 0;
	data.select = sel;
	getmouseco_areawin(data.mval);

	nurbs_foreachScreenVert(findnearestNurbvert__doClosest, &data);

	*nurb = data.nurb;
	*bezt = data.bezt;
	*bp = data.bp;

	return data.hpoint;
}


static void findselectedNurbvert(Nurb **nu, BezTriple **bezt, BPoint **bp)
{
	/* in nu and (bezt or bp) selected are written if there's 1 sel.  */
	/* if more points selected in 1 spline: return only nu, bezt and bp are 0 */
	Nurb *nu1;
	BezTriple *bezt1;
	BPoint *bp1;
	int a;

	*nu= 0;
	*bezt= 0;
	*bp= 0;
	for(nu1= editNurb.first; nu1; nu1= nu1->next) {
		if((nu1->type & 7)==CU_BEZIER) {
			bezt1= nu1->bezt;
			a= nu1->pntsu;
			while(a--) {
				if( (bezt1->f1 & SELECT) || (bezt1->f2 & SELECT) || (bezt1->f3 & SELECT) ) {
					if(*nu!=0 && *nu!= nu1) {
						*nu= 0;
						*bp= 0;
						*bezt= 0;
						return;
					}
					else if(*bezt || *bp) {
						*bp= 0;
						*bezt= 0;
					}
					else {
						*bezt= bezt1;
						*nu= nu1;
					}
				}
				bezt1++;
			}
		}
		else {
			bp1= nu1->bp;
			a= nu1->pntsu*nu1->pntsv;
			while(a--) {
				if( bp1->f1 & 1 ) {
					if(*nu!=0 && *nu!= nu1) {
						*bp= 0;
						*bezt= 0;
						*nu= 0;
						return;
					}
					else if(*bezt || *bp) {
						*bp= 0;
						*bezt= 0;
					}
					else {
						*bp= bp1;
						*nu= nu1;
					}
				}
				bp1++;
			}
		}
	}
}

int convertspline(short type, Nurb *nu)
{
	BezTriple *bezt;
	BPoint *bp;
	int a, c, nr;

	if((nu->type & 7)==0) {		/* Poly */
		if(type==CU_BEZIER) {			    /* to Bezier with vecthandles  */
			nr= nu->pntsu;
			bezt =
				(BezTriple*)MEM_callocN(nr * sizeof(BezTriple), "setsplinetype2");
			nu->bezt= bezt;
			a= nr;
			bp= nu->bp;
			while(a--) {
				VECCOPY(bezt->vec[1], bp->vec);
				bezt->f1=bezt->f2=bezt->f3= bp->f1;
				bezt->h1= bezt->h2= HD_VECT;
				bezt->weight= bp->weight;
				bezt->radius= bp->radius;
				bp++;
				bezt++;
			}
			MEM_freeN(nu->bp);
			nu->bp= 0;
			nu->pntsu= nr;
			nu->type &= ~7;
			nu->type |= 1;
			calchandlesNurb(nu);
		}
		else if(type==CU_NURBS) {
			nu->type &= ~7;
			nu->type+= 4;
			nu->orderu= 4;
			nu->flagu &= CU_CYCLIC; /* disable all flags except for cyclic */
			nu->flagu += 4;
			makeknots(nu, 1, nu->flagu>>1);
			a= nu->pntsu*nu->pntsv;
			bp= nu->bp;
			while(a--) {
				bp->vec[3]= 1.0;
				bp++;
			}
		}
	}
	else if((nu->type & 7)==CU_BEZIER) {	/* Bezier */
		if(type==0 || type==4) {	    /* to Poly or Nurb */
			nr= 3*nu->pntsu;
			nu->bp = MEM_callocN(nr * sizeof(BPoint), "setsplinetype");
			a= nu->pntsu;
			bezt= nu->bezt;
			bp= nu->bp;
			while(a--) {
				if(type==0 && bezt->h1==HD_VECT && bezt->h2==HD_VECT) {
					/* vector handle becomes 1 poly vertice */
					VECCOPY(bp->vec, bezt->vec[1]);
					bp->vec[3]= 1.0;
					bp->f1= bezt->f2;
					nr-= 2;
					bp->radius= bezt->radius;
					bp->weight= bezt->weight;
					bp++;
				}
				else {
					for(c=0;c<3;c++) {
						VECCOPY(bp->vec, bezt->vec[c]);
						bp->vec[3]= 1.0;
						if(c==0) bp->f1= bezt->f1;
						else if(c==1) bp->f1= bezt->f2;
						else bp->f1= bezt->f3;
						bp->radius= bezt->radius;
						bp->weight= bezt->weight;
						bp++;
					}
				}
				bezt++;
			}
			MEM_freeN(nu->bezt); 
			nu->bezt= 0;
			nu->pntsu= nr;
			nu->pntsv= 1;
			nu->orderu= 4;
			nu->orderv= 1;
			nu->type &= ~7;
			nu->type+= type;
			if(nu->flagu & CU_CYCLIC) c= nu->orderu-1; 
			else c= 0;
			if(type== 4) {
				nu->flagu &= CU_CYCLIC; /* disable all flags except for cyclic */
				nu->flagu += 4;
				makeknots(nu, 1, nu->flagu>>1);
			}
		}
	}
	else if( (nu->type & 7)==CU_NURBS) {
		if(type==0) {			/* to Poly */
			nu->type &= ~7;
			if(nu->knotsu) MEM_freeN(nu->knotsu); /* python created nurbs have a knotsu of zero */
			nu->knotsu= NULL;
			if(nu->knotsv) MEM_freeN(nu->knotsv);
			nu->knotsv= NULL;
		}
		else if(type==CU_BEZIER) {		/* to Bezier */
			nr= nu->pntsu/3;

			if(nr<2) 
				return 1;	/* conversion impossible */
			else {
				bezt = MEM_callocN(nr * sizeof(BezTriple), "setsplinetype2");
				nu->bezt= bezt;
				a= nr;
				bp= nu->bp;
				while(a--) {
					VECCOPY(bezt->vec[0], bp->vec);
					bezt->f1= bp->f1;
					bp++;
					VECCOPY(bezt->vec[1], bp->vec);
					bezt->f2= bp->f1;
					bp++;
					VECCOPY(bezt->vec[2], bp->vec);
					bezt->f3= bp->f1;
					bezt->radius= bp->radius;
					bezt->weight= bp->weight;
					bp++;
					bezt++;
				}
				MEM_freeN(nu->bp);
				nu->bp= 0;
				MEM_freeN(nu->knotsu);
				nu->knotsu= NULL;
				nu->pntsu= nr;
				nu->type &= ~7;
				nu->type+= 1;
			}
		}
	}
	return 0;
}

void setsplinetype(short type)
{
	Nurb *nu;

	if(type==CU_CARDINAL || type==CU_BSPLINE) {
		error("Not implemented yet");
		return;
	}
	
	for(nu= editNurb.first; nu; nu= nu->next) {
		if(isNurbsel(nu)) {
			if (convertspline(type, nu))
				error("no conversion possible");
		}
	}
	BIF_undo_push("Set spline type");
	
}

/* ******************** SKINNING LOFTING!!! ******************** */

void rotate_direction_nurb(Nurb *nu)
{
	BPoint *bp1, *bp2, *temp;
	int u, v;
	
	SWAP(short, nu->pntsu, nu->pntsv);
	SWAP(short, nu->orderu, nu->orderv);
	SWAP(short, nu->resolu, nu->resolv);
	SWAP(short, nu->flagu, nu->flagv);
	
	SWAP(float *, nu->knotsu, nu->knotsv);
	switchdirection_knots(nu->knotsv, KNOTSV(nu) );
	
	temp= MEM_dupallocN(nu->bp);
	bp1= nu->bp;
	for(v=0; v<nu->pntsv; v++) {
		for(u=0; u<nu->pntsu; u++, bp1++) {
			bp2= temp + (nu->pntsu-u-1)*(nu->pntsv) + v;
			*bp1= *bp2;
		}
	}

	MEM_freeN(temp);
}

int is_u_selected(Nurb *nu, int u)
{
	BPoint *bp;
	int v;
	
	/* what about resolu == 2? */
	bp= nu->bp+u;
	for(v=0; v<nu->pntsv-1; v++, bp+=nu->pntsu) {
		if(v) if(bp->f1 & SELECT) return 1;
	}
	
	return 0;
}

/* ******************************** */

typedef struct NurbSort {
	struct NurbSort *next, *prev;
	Nurb *nu;
	float vec[3];
} NurbSort;

static ListBase nsortbase= {0, 0};
/*  static NurbSort *nusmain; */ /* this var seems to go unused... at least in this file */

void make_selection_list_nurb()
{
	ListBase nbase= {0, 0};
	NurbSort *nus, *nustest, *headdo, *taildo;
	Nurb *nu;
	BPoint *bp;
	float dist, headdist, taildist;
	int a;
	
	for(nu= editNurb.first; nu; nu= nu->next) {
		if( isNurbsel(nu) ) {
			
			nus = (NurbSort*)MEM_callocN(sizeof(NurbSort), "sort");
			BLI_addhead(&nbase, nus);
			nus->nu= nu;
			
			bp= nu->bp;
			a= nu->pntsu;
			while(a--) {
				VecAddf(nus->vec, nus->vec, bp->vec);
				bp++;
			}
			VecMulf(nus->vec, 1.0/(float)nu->pntsu);
			
			
		}
	}

	/* just add the first one */
	nus= nbase.first;
	BLI_remlink(&nbase, nus);
	BLI_addtail( &nsortbase, nus);
	
	/* now add, either at head or tail, the closest one */
	while(nbase.first) {
	
		headdist= taildist= 1.0e30;
		headdo= taildo= 0;

		nustest= nbase.first;
		while(nustest) {
			dist= VecLenf(nustest->vec, ((NurbSort *)nsortbase.first)->vec);

			if(dist<headdist) {
				headdist= dist;
				headdo= nustest;
			}
			dist= VecLenf(nustest->vec, ((NurbSort *)nsortbase.last)->vec);

			if(dist<taildist) {
				taildist= dist;
				taildo= nustest;
			}
			nustest= nustest->next;
		}
		
		if(headdist<taildist) {
			BLI_remlink(&nbase, headdo);
			BLI_addhead(&nsortbase, headdo);
		}
		else {
			BLI_remlink(&nbase, taildo);
			BLI_addtail(&nsortbase, taildo);
		}
	}
}

void merge_2_nurb(Nurb *nu1, Nurb *nu2)
{
	BPoint *bp, *bp1, *bp2, *temp;
	float  len1, len2;
	int    origu, u, v;
	
	/* first nurbs will be changed to make u = resolu-1 selected */
	/* 2nd nurbs will be changed to make u = 0 selected */

	/* first nurbs: u = resolu-1 selected */
	
	if( is_u_selected(nu1, nu1->pntsu-1) );
	else {
		rotate_direction_nurb(nu1);
		if( is_u_selected(nu1, nu1->pntsu-1) );
		else {
			rotate_direction_nurb(nu1);
			if( is_u_selected(nu1, nu1->pntsu-1) );
			else {
				rotate_direction_nurb(nu1);
				if( is_u_selected(nu1, nu1->pntsu-1) );
				else {
					/* rotate again, now its OK! */
					if(nu1->pntsv!=1) rotate_direction_nurb(nu1);
					return;
				}
			}
		}
	}
	
	/* 2nd nurbs: u = 0 selected */
	if( is_u_selected(nu2, 0) );
	else {
		rotate_direction_nurb(nu2);
		if( is_u_selected(nu2, 0) );
		else {
			rotate_direction_nurb(nu2);
			if( is_u_selected(nu2, 0) );
			else {
				rotate_direction_nurb(nu2);
				if( is_u_selected(nu2, 0) );
				else {
					/* rotate again, now its OK! */
					if(nu1->pntsu==1) rotate_direction_nurb(nu1);
					if(nu2->pntsv!=1) rotate_direction_nurb(nu2);
					return;
				}
			}
		}
	}
	
	if( nu1->pntsv != nu2->pntsv ) {
		error("Resolution doesn't match");
		return;
	}
	
	/* ok, now nu1 has the rightmost collumn and nu2 the leftmost collumn selected */
	/* maybe we need a 'v' flip of nu2? */
	
	bp1= nu1->bp+nu1->pntsu-1;
	bp2= nu2->bp;
	len1= 0.0;
	
	for(v=0; v<nu1->pntsv; v++, bp1+=nu1->pntsu, bp2+=nu2->pntsu) {
		len1+= VecLenf(bp1->vec, bp2->vec);
	}

	bp1= nu1->bp + nu1->pntsu-1;
	bp2= nu2->bp + nu2->pntsu*(nu2->pntsv-1);
	len2= 0.0;
	
	for(v=0; v<nu1->pntsv; v++, bp1+=nu1->pntsu, bp2-=nu2->pntsu) {
		len2+= VecLenf(bp1->vec, bp2->vec);
	}

	/* merge */
	origu= nu1->pntsu;
	nu1->pntsu+= nu2->pntsu;
	nu1->resolu+= nu2->pntsu;
	if(nu1->resolv < nu2->resolv) nu1->resolv= nu2->resolv;
	if(nu1->orderu<3) nu1->orderu++;
	if(nu1->orderv<3) nu1->orderv++;
	temp= nu1->bp;
	nu1->bp= MEM_mallocN(nu1->pntsu*nu1->pntsv*sizeof(BPoint), "mergeBP");
	
	bp= nu1->bp;
	bp1= temp;
	
	for(v=0; v<nu1->pntsv; v++) {
		
		/* switch direction? */
		if(len1<len2) bp2= nu2->bp + v*nu2->pntsu;
		else bp2= nu2->bp + (nu1->pntsv-v-1)*nu2->pntsu;

		for(u=0; u<nu1->pntsu; u++, bp++) {
			if(u<origu) {
				*bp= *bp1; bp1++;
				select_bpoint(bp, SELECT, 1, HIDDEN);
			}
			else {
				*bp= *bp2; bp2++;
			}
		}
	}

	if((nu1->type & 7)==4) {
		/* merge knots */
		makeknots(nu1, 1, nu1->flagu>>1);
	
		/* make knots, for merged curved for example */
		makeknots(nu1, 2, nu1->flagv>>1);
	}
	
	MEM_freeN(temp);
	BLI_remlink(&editNurb, nu2);
	freeNurb(nu2);
}

void merge_nurb()
{
	NurbSort *nus1, *nus2;
	int ok= 1;
	
	make_selection_list_nurb();
	
	if(nsortbase.first == nsortbase.last) {
		BLI_freelistN(&nsortbase);
		error("Too few selections to merge");
		return;
	}
	
	nus1= nsortbase.first;
	nus2= nus1->next;

	/* resolution match, to avoid uv rotations */
	if(nus1->nu->pntsv==1) {
		if(nus1->nu->pntsu==nus2->nu->pntsu || nus1->nu->pntsu==nus2->nu->pntsv);
		else ok= 0;
	}
	else if(nus2->nu->pntsv==1) {
		if(nus2->nu->pntsu==nus1->nu->pntsu || nus2->nu->pntsu==nus1->nu->pntsv);
		else ok= 0;
	}
	else if( nus1->nu->pntsu==nus2->nu->pntsu || nus1->nu->pntsv==nus2->nu->pntsv);
	else if( nus1->nu->pntsu==nus2->nu->pntsv || nus1->nu->pntsv==nus2->nu->pntsu);
	else {
		ok= 0;
	}
	
	if(ok==0) {
		error("Resolution doesn't match");
		BLI_freelistN(&nsortbase);
		return;
	}
	
	while(nus2) {
		merge_2_nurb(nus1->nu, nus2->nu);
		nus2= nus2->next;
	}
	
	BLI_freelistN(&nsortbase);
	
	countall();
	set_actNurb(NULL);

	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Merge");
	
}


void addsegment_nurb()
{
	/* joins 2 curves */
	Nurb *nu, *nu1=0, *nu2=0;
	BezTriple *bezt;
	BPoint *bp;
	float *fp, offset;
	int a;

	/* first decide if this is a surface merge! */
	if(G.obedit->type==OB_SURF) nu= editNurb.first;
	else nu= NULL;
	
	while(nu) {
		if( isNurbsel(nu) ) {
		
			if(nu->pntsu>1 && nu->pntsv>1) break;
			if(isNurbsel_count(nu)>1) break;
			if(isNurbsel_count(nu)==1) {
				/* only 1 selected, not first or last, a little complex, but intuitive */
				if(nu->pntsv==1) {
					if( (nu->bp->f1 & SELECT) || ((nu->bp+nu->pntsu-1)->f1 & SELECT));
					else break;
				}
			}
		}
		nu= nu->next;
	}
	if(nu) {
		merge_nurb();
		return;
	}
	
	/* find both nurbs and points, nu1 will be put behind nu2 */
	for(nu= editNurb.first; nu; nu= nu->next) {
		if((nu->flagu & CU_CYCLIC)==0) {    /* not cyclic */
			if( (nu->type & 7)==CU_BEZIER ) {
				bezt= nu->bezt;
				if(nu1==0) {
					if( BEZSELECTED_HIDDENHANDLES(bezt) ) nu1= nu;
					else {
						bezt= bezt+(nu->pntsu-1);
						if( BEZSELECTED_HIDDENHANDLES(bezt) ) {
							nu1= nu;
							switchdirectionNurb(nu);
						}
					}
				}
				else if(nu2==0) {
					if( BEZSELECTED_HIDDENHANDLES(bezt) ) {
						nu2= nu;
						switchdirectionNurb(nu);
					}
					else {
						bezt= bezt+(nu->pntsu-1);
						if( BEZSELECTED_HIDDENHANDLES(bezt) ) {
							nu2= nu;
						}
					}
				}
				else break;
			}
			else if(nu->pntsv==1) {
				bp= nu->bp;
				if(nu1==0) {
					if( bp->f1 & SELECT) nu1= nu;
					else {
						bp= bp+(nu->pntsu-1);
						if( bp->f1 & SELECT ) {
							nu1= nu;
							switchdirectionNurb(nu);
						}
					}
				}
				else if(nu2==0) {
					if( bp->f1 & SELECT ) {
						nu2= nu;
						switchdirectionNurb(nu);
					}
					else {
						bp= bp+(nu->pntsu-1);
						if( bp->f1 & SELECT ) {
							nu2= nu;
						}
					}
				}
				else break;
			}
		}
	}

	if((nu1 && nu2) && (nu1!=nu2)) {
		if( nu1->type==nu2->type) {
			if((nu1->type & 7)==CU_BEZIER) {
				bezt =
					(BezTriple*)MEM_mallocN((nu1->pntsu+nu2->pntsu) * sizeof(BezTriple), "addsegmentN");
				memcpy(bezt, nu2->bezt, nu2->pntsu*sizeof(BezTriple));
				memcpy(bezt+nu2->pntsu, nu1->bezt, nu1->pntsu*sizeof(BezTriple));
				MEM_freeN(nu1->bezt);
				nu1->bezt= bezt;
				nu1->pntsu+= nu2->pntsu;
				BLI_remlink(&editNurb, nu2);
				freeNurb(nu2); nu2= NULL;
				calchandlesNurb(nu1);
			}
			else {
				bp =
					(BPoint*)MEM_mallocN((nu1->pntsu+nu2->pntsu) * sizeof(BPoint), "addsegmentN2");
				memcpy(bp, nu2->bp, nu2->pntsu*sizeof(BPoint) );
				memcpy(bp+nu2->pntsu, nu1->bp, nu1->pntsu*sizeof(BPoint));
				MEM_freeN(nu1->bp);
				nu1->bp= bp;

				a= nu1->pntsu+nu1->orderu;

				nu1->pntsu+= nu2->pntsu;
				BLI_remlink(&editNurb, nu2);

				/* now join the knots */
				if((nu1->type & 7)==4) {
					if(nu1->knotsu==NULL) {
						makeknots(nu1, 1, nu1->flagu>>1);
					}
					else {
						fp= MEM_mallocN(sizeof(float)*KNOTSU(nu1), "addsegment3");
						memcpy(fp, nu1->knotsu, sizeof(float)*a);
						MEM_freeN(nu1->knotsu);
						nu1->knotsu= fp;
						
						
						offset= nu1->knotsu[a-1] +1.0;
						fp= nu1->knotsu+a;
						for(a=0; a<nu2->pntsu; a++, fp++) {
							if(nu2->knotsu) 
								*fp= offset+nu2->knotsu[a+1];
							else 
								*fp = offset;
						}
					}
				}
				freeNurb(nu2); nu2= NULL;
			}
		}
		
		set_actNurb(NULL);	/* for selected */

		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	

		countall();
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		BIF_undo_push("Add segment");
	
	}
	else error("Can't make segment");
}

void mouse_nurb()
{
	Nurb *nu;
	BezTriple *bezt=0;
	BPoint *bp=0;
	short hand;

	hand= findnearestNurbvert(1, &nu, &bezt, &bp);

	if(bezt || bp) {
		if((G.qual & LR_SHIFTKEY)==0) {
		
			setflagsNurb(0);

			if(bezt) {

				if(hand==1) select_beztriple(bezt, SELECT, 1, HIDDEN);
				else if(hand==0) bezt->f1|= SELECT;
				else bezt->f3|= SELECT;
			}
			else {
				lastselbp= bp;
				select_bpoint(bp, SELECT, 1, HIDDEN);
			}

			allqueue(REDRAWVIEW3D, 0);
		}
		else {
			if(bezt) {
				if(hand==1) {
					if(bezt->f2 & SELECT) select_beztriple(bezt, DESELECT, 1, HIDDEN);
					else select_beztriple(bezt, SELECT, 1, HIDDEN);
				} else if(hand==0) {
					bezt->f1 ^= SELECT;
				} else {
					bezt->f3 ^= SELECT;
				}
			}
			else {
				if(bp->f1 & SELECT) select_bpoint(bp, DESELECT, 1, HIDDEN);
				else {
					select_bpoint(bp, SELECT, 1, HIDDEN);
					lastselbp= bp;
				}
			}

			allqueue(REDRAWVIEW3D, 0);

		}

		countall();
	}

	rightmouse_transform();
	
	if(nu!=get_actNurb()) {
		set_actNurb(nu);
		allqueue(REDRAWBUTSEDIT, 0);
	}
	
}

/* from what I can gather, the mode==0 magic number spins and bridges the nurbs based on the 
 * orientation of the global 3d view (yuck yuck!) mode==1 does the same, but doesn't bridge up
 * up the new geometry, mode==2 now does the same as 0, but aligned to world axes, not the view.
*/
static void spin_nurb(float *dvec, short mode)
{
	Nurb *nu;
	float *curs, si,phi,n[3],q[4],cmat[3][3],tmat[3][3],imat[3][3];
	float cent[3],bmat[3][3], rotmat[3][3], scalemat1[3][3], scalemat2[3][3];
	float persmat[3][3], persinv[3][3];
	short a,ok;
	
	if(G.vd==0 || G.obedit==0 || G.obedit->type!=OB_SURF) return;
	if( (G.vd->lay & G.obedit->lay)==0 ) return;

	if (mode != 2) Mat3CpyMat4(persmat, G.vd->viewmat);
	else Mat3One(persmat);
	Mat3Inv(persinv, persmat);

	/* imat and center and size */
	Mat3CpyMat4(bmat, G.obedit->obmat);
	Mat3Inv(imat, bmat);

	curs= give_cursor();
	VECCOPY(cent, curs);
	VecSubf(cent, cent, G.obedit->obmat[3]);
	Mat3MulVecfl(imat,cent);

	if(dvec || mode==2) {
		n[0]=n[1]= 0.0;
		n[2]= 1.0;
	} else {
		n[0]= G.vd->viewinv[2][0];
		n[1]= G.vd->viewinv[2][1];
		n[2]= G.vd->viewinv[2][2];
		Normalize(n);
	}

	phi= M_PI/8.0;
	q[0]= cos(phi);
	si= sin(phi);
	q[1]= n[0]*si;
	q[2]= n[1]*si;
	q[3]= n[2]*si;
	QuatToMat3(q, cmat);
	Mat3MulMat3(tmat, cmat, bmat);
	Mat3MulMat3(rotmat, imat, tmat);

	Mat3One(scalemat1);
	scalemat1[0][0]= sqrt(2.0);
	scalemat1[1][1]= sqrt(2.0);

	Mat3MulMat3(tmat,persmat,bmat);
	Mat3MulMat3(cmat,scalemat1,tmat);
	Mat3MulMat3(tmat,persinv,cmat);
	Mat3MulMat3(scalemat1,imat,tmat);

	Mat3One(scalemat2);
	scalemat2[0][0]/= sqrt(2.0);
	scalemat2[1][1]/= sqrt(2.0);

	Mat3MulMat3(tmat,persmat,bmat);
	Mat3MulMat3(cmat,scalemat2,tmat);
	Mat3MulMat3(tmat,persinv,cmat);
	Mat3MulMat3(scalemat2,imat,tmat);

	ok= 1;

	for(a=0;a<7;a++) {
		if(mode==0 || mode==2) ok= extrudeflagNurb(1);
		else adduplicateflagNurb(1);
		if(ok==0) {
			error("Can't spin");
			break;
		}
		rotateflagNurb(1,cent,rotmat);

		if(mode==0 || mode==2) {
			if( (a & 1)==0 ) {
				rotateflagNurb(1,cent,scalemat1);
				weightflagNurb(1, 0.25*sqrt(2.0), 1);
			}
			else {
				rotateflagNurb(1,cent,scalemat2);
				weightflagNurb(1, 4.0/sqrt(2.0), 1);
			}
		}
		if(dvec) {
			Mat3MulVecfl(bmat,dvec);
			translateflagNurb(1,dvec);
		}
	}

	if(ok) {
		for(nu= editNurb.first; nu; nu= nu->next) {
			if(isNurbsel(nu)) {
				nu->orderv= 4;
				nu->flagv |= CU_CYCLIC;
				makeknots(nu, 2, nu->flagv>>1);
			}
		}
	}
	
}

/* external one, for undo */
void spinNurb(float *dvec, short mode)
{
	spin_nurb(dvec, mode);
	BIF_undo_push("Spin");
}

void addvert_Nurb(int mode)
{
	Nurb *nu;
	BezTriple *bezt, *newbezt = NULL;
	BPoint *bp, *newbp = NULL;
	float *curs, mat[3][3],imat[3][3], temp[3];

	if(G.obedit==0 || G.vd == 0) return;
	if( (G.vd->lay & G.obedit->lay)==0 ) return;

	Mat3CpyMat4(mat, G.obedit->obmat);
	Mat3Inv(imat,mat);

	findselectedNurbvert(&nu, &bezt, &bp);
	if(bezt==0 && bp==0) return;

	if((nu->type & 7)==CU_BEZIER) {
		/* which bezpoint? */
		if(bezt== nu->bezt) {   /* first */
			BEZ_DESEL(bezt);
			newbezt =
				(BezTriple*)MEM_callocN((nu->pntsu+1) * sizeof(BezTriple), "addvert_Nurb");
			memcpy(newbezt+1, bezt, nu->pntsu*sizeof(BezTriple));
			*newbezt= *bezt;
			BEZ_SEL(newbezt);
			if(newbezt->h1 >= 0) newbezt->h2= newbezt->h1;
			else newbezt->h2= newbezt->h1= HD_ALIGN; /* does this ever happen? */
			VECCOPY(temp, bezt->vec[1]);
			MEM_freeN(nu->bezt);
			nu->bezt= newbezt;
			bezt= newbezt+1;
		}
		else if(bezt== (nu->bezt+nu->pntsu-1)) {  /* last */
			BEZ_DESEL(bezt);
			newbezt =
				(BezTriple*)MEM_callocN((nu->pntsu+1) * sizeof(BezTriple), "addvert_Nurb");
			memcpy(newbezt, nu->bezt, nu->pntsu*sizeof(BezTriple));
			*(newbezt+nu->pntsu)= *bezt;
			VECCOPY(temp, bezt->vec[1]);
			MEM_freeN(nu->bezt);
			nu->bezt= newbezt;
			newbezt+= nu->pntsu;
			BEZ_SEL(newbezt);
			if(newbezt->h1 >= 0) newbezt->h2= newbezt->h1;
			else newbezt->h2= newbezt->h1= HD_ALIGN; /* does this ever happen? */
			bezt= nu->bezt+nu->pntsu-1;
		}
		else bezt= 0;

		if(bezt) {
			nu->pntsu++;
			
			if(mode=='e') {
				VECCOPY(newbezt->vec[0], bezt->vec[0]);
				VECCOPY(newbezt->vec[1], bezt->vec[1]);
				VECCOPY(newbezt->vec[2], bezt->vec[2]);
			}
			else {
				curs= give_cursor();
			
				VECCOPY(newbezt->vec[1], curs);
				VecSubf(newbezt->vec[1],newbezt->vec[1],G.obedit->obmat[3]);
				Mat3MulVecfl(imat,newbezt->vec[1]);
				VecSubf(temp, newbezt->vec[1],temp);
				VecAddf(newbezt->vec[0], bezt->vec[0],temp);
				VecAddf(newbezt->vec[2], bezt->vec[2],temp);
				calchandlesNurb(nu);
			}
		}
	}
	else if(nu->pntsv==1) {
		/* which b-point? */
		if(bp== nu->bp) {   /* first */
			bp->f1= 0;
			newbp =
				(BPoint*)MEM_callocN((nu->pntsu+1) * sizeof(BPoint), "addvert_Nurb3");
			memcpy(newbp+1, bp, nu->pntsu*sizeof(BPoint));
			*newbp= *bp;
			newbp->f1= 1;
			MEM_freeN(nu->bp);
			nu->bp= newbp;
			bp= newbp + 1;
		}
		else if(bp== (nu->bp+nu->pntsu-1)) {  /* last */
			bp->f1= 0;
			newbp =
				(BPoint*)MEM_callocN((nu->pntsu+1) * sizeof(BPoint), "addvert_Nurb4");
			memcpy(newbp, nu->bp, nu->pntsu*sizeof(BPoint));
			*(newbp+nu->pntsu)= *bp;
			MEM_freeN(nu->bp);
			nu->bp= newbp;
			newbp+= nu->pntsu;
			newbp->f1= 1;
			bp= newbp - 1;
		}
		else bp= 0;

		if(bp) {
			nu->pntsu++;
			
			if(nu->resolu<3) nu->resolu++;
			makeknots(nu, 1, nu->flagu>>1);
			
			if(mode=='e') {
				VECCOPY(newbp->vec, bp->vec);
			}
			else {
				curs= give_cursor();
			
				VECCOPY(newbp->vec, curs);
				VecSubf(newbp->vec, newbp->vec, G.obedit->obmat[3]);
				Mat3MulVecfl(imat,newbp->vec);
				newbp->vec[3]= 1.0;
			}
		}
	}

	retopo_do_all();

	test2DNurb(nu);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);

	if(mode=='e') {
		BIF_TransformSetUndo("Extrude");
		initTransform(TFM_TRANSLATION, CTX_NO_PET);
		Transform();
	}
	else while(get_mbut()&R_MOUSE) BIF_wait_for_statechange();

	if(mode!='e') {
		/* dependencies with other objects, should become event */
		BIF_undo_push("Add vertex");
	
	}
}

void extrude_nurb()
{
	Nurb *nu;
	int ok= 0;

	if(G.obedit && G.obedit->type==OB_SURF) {

		/* first test: curve? */
		for(nu= editNurb.first; nu; nu= nu->next) {
			if(nu->pntsv==1 && isNurbsel_count(nu)==1 ) break;
		}
		if(nu) {
			addvert_Nurb('e');
		} else {
			ok= extrudeflagNurb(1); /* '1'= flag */
		
			if(ok) {
				DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
				countall();
				BIF_TransformSetUndo("Extrude");
				initTransform(TFM_TRANSLATION, CTX_NO_PET);
				Transform();
			}
		}
	}
}



void makecyclicNurb()
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	float *fp;
	int a, b, cyclmode=0;

	for(nu= editNurb.first; nu; nu= nu->next) {
		if( nu->pntsu>1 || nu->pntsv>1) {
			if( (nu->type & 7)==0 ) {
				a= nu->pntsu;
				bp= nu->bp;
				while(a--) {
					if( bp->f1 & SELECT ) {
						if(nu->flagu & CU_CYCLIC) nu->flagu &= ~CU_CYCLIC;
						else nu->flagu |= CU_CYCLIC;
						break;
					}
					bp++;
				}
			}
			else if( (nu->type & 7)==CU_BEZIER ) {
				a= nu->pntsu;
				bezt= nu->bezt;
				while(a--) {
					if( BEZSELECTED_HIDDENHANDLES(bezt) ) {
						if(nu->flagu & CU_CYCLIC) nu->flagu &= ~CU_CYCLIC;
						else nu->flagu |= CU_CYCLIC;
						break;
					}
					bezt++;
				}
				calchandlesNurb(nu);
			}
			else if(nu->pntsv==1 && (nu->type & 7)==CU_NURBS) {
				if (nu->knotsu) { /* if check_valid_nurb_u fails the knotsu can be NULL */
					a= nu->pntsu;
					bp= nu->bp;
					while(a--) {
						if( bp->f1 & SELECT ) {
							if(nu->flagu & CU_CYCLIC) nu->flagu &= ~CU_CYCLIC;
							else {
								nu->flagu |= CU_CYCLIC;
								nu->flagu &= ~2;	/* endpoint flag, fixme */
								fp= MEM_mallocN(sizeof(float)*KNOTSU(nu), "makecyclicN");
								b= (nu->orderu+nu->pntsu);
								memcpy(fp, nu->knotsu, sizeof(float)*b);
								MEM_freeN(nu->knotsu);
								nu->knotsu= fp;
							
								makeknots(nu, 1, 0);	/* 1==u  0==uniform */
							
							}
							break;
						}
						bp++;
					}
				}
			}
			else if(nu->type==CU_NURBS) {
				if(cyclmode==0) {
					cyclmode= pupmenu("Toggle %t|cyclic U%x1|cyclic V%x2");
					if(cyclmode < 1) return;
				}
				a= nu->pntsu*nu->pntsv;
				bp= nu->bp;
				while(a--) {
	
					if( bp->f1 & SELECT) {
						if(cyclmode==1 && nu->pntsu>1) {
							if(nu->flagu & CU_CYCLIC) nu->flagu &= ~CU_CYCLIC;
							else {
								nu->flagu |= CU_CYCLIC;
								if (check_valid_nurb_u(nu)) {
									fp= MEM_mallocN(sizeof(float)*KNOTSU(nu), "makecyclicN");
									b= (nu->orderu+nu->pntsu);
									if (nu->knotsu) { /* null if check_valid_nurb_u failed before but is valid now */
										memcpy(fp, nu->knotsu, sizeof(float)*b);
										MEM_freeN(nu->knotsu);
									}
									nu->knotsu= fp;
								
									makeknots(nu, 1, 0);	/* 1==u  0==uniform */
								}
							}
						}
						if(cyclmode==2 && nu->pntsv>1) {
							if(nu->flagv & 1) nu->flagv--;
							else {
								nu->flagv++;
								if (check_valid_nurb_v(nu)) {
									fp= MEM_mallocN(sizeof(float)*KNOTSV(nu), "makecyclicN");
									b= (nu->orderv+nu->pntsv);
									if (nu->knotsv) { /* null if check_valid_nurb_v failed before but is valid now */
										memcpy(fp, nu->knotsv, sizeof(float)*b);
										MEM_freeN(nu->knotsv);
									}
									nu->knotsv= fp;
								
									makeknots(nu, 2, 0);	/* 2==v  0==uniform */
								}
							}
						}
						break;
					}
					bp++;
				}
	
			}
		}
	}
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	BIF_undo_push("Cyclic");
}

void selectconnected_nurb()
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	findnearestNurbvert(1, &nu, &bezt, &bp);
	if(bezt) {
		a= nu->pntsu;
		bezt= nu->bezt;
		while(a--) {
			if(G.qual & LR_SHIFTKEY) select_beztriple(bezt, DESELECT, 1, VISIBLE);
			else select_beztriple(bezt, SELECT, 1, VISIBLE);
			bezt++;
		}
	}
	else if(bp) {
		a= nu->pntsu*nu->pntsv;
		bp= nu->bp;
		while(a--) {
			if(G.qual & LR_SHIFTKEY) select_bpoint(bp, DESELECT, 1, VISIBLE);
			else select_bpoint(bp, SELECT, 1, VISIBLE);
			bp++;
		}
	}

	countall();
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Select connected");
	
}

void selectrow_nurb()
{
	static BPoint *last=0;
	static int direction=0;
	Nurb *nu;
	BPoint *bp;
	int u = 0, v = 0, a, b, ok=0;

	if(editNurb.first==0) return;
	if(G.obedit==NULL || G.obedit->type!=OB_SURF) return;
	if(lastselbp==NULL) return;

	/* find the correct nurb and toggle with u of v */
	for(nu= editNurb.first; nu; nu= nu->next) {
		bp= nu->bp;
		for(v=0; v<nu->pntsv; v++) {
			for(u=0; u<nu->pntsu; u++, bp++) {
				if(bp==lastselbp) {
					if(bp->f1 & SELECT) {
						ok= 1;
						break;
					}
				}
			}
			if(ok) break;
		}
		if(ok) {
			if(last==lastselbp) {
				direction= 1-direction;
				setflagsNurb(0);
			}
			last= lastselbp;

			bp= nu->bp;
			for(a=0; a<nu->pntsv; a++) {
				for(b=0; b<nu->pntsu; b++, bp++) {
					if(direction) {
						if(a==v) select_bpoint(bp, SELECT, 1, VISIBLE);
					}
					else {
						if(b==u) select_bpoint(bp, SELECT, 1, VISIBLE);
					}
				}
			}
			countall();
			allqueue(REDRAWVIEW3D, 0);
			return;
		}
	}
	BIF_undo_push("Select Row");
	
}

/* (de)selects first or last of visible part of each Nurb depending on selFirst     */
/* selFirst: defines the end of which to select					    */
/* doswap: defines if selection state of each first/last control point is swapped   */
/* selstatus: selection status in case doswap is false				    */
void selectend_nurb(short selfirst, short doswap, short selstatus)
{
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int a;
	short sel;

	if(G.obedit==0) return;

	for(nu= editNurb.first; nu; nu= nu->next) {
		sel= 0;
		if((nu->type & 7)==CU_BEZIER) {
			a= nu->pntsu;
			
			/* which point? */
			if(selfirst==0) { /* select last */ 
				bezt= (nu->bezt + (a-1));
			}
			else { /* select first */
				bezt= nu->bezt;
			}
			
			while(a--) {
				if(doswap) sel= swap_selection_beztriple(bezt);
				else sel= select_beztriple(bezt, selstatus, 1, VISIBLE);
				
				if(sel==1) break;
			}
		}
		else {
			a= nu->pntsu*nu->pntsv;
			
			/* which point? */
			if(selfirst==0) { /* select last */
				bp= (nu->bp + (a-1));
			}
			else{ /* select first */
				bp= nu->bp;
			}
				
			while(a--) {
				if (bp->hide == 0) {
					if(doswap) sel= swap_selection_bpoint(bp);
					else sel= select_bpoint(bp, selstatus, 1, VISIBLE);
					
					if(sel==1) break;
				}
			}
		}
	}
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Select/Deselect End");
}

void select_next_nurb()
{
	if(G.obedit==0) return;
	
	select_adjacent_cp(1, 0, SELECT);
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Select Next");
}

void select_prev_nurb()
{
	if(G.obedit==0) return;
	
	select_adjacent_cp(-1, 0, SELECT);
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Select Previous");
}

void select_more_nurb()
{
	Nurb *nu;
	BPoint *bp, *tempbp;
	int a;
	short sel= 0;
	short *selbpoints;
	
	if(G.obedit==0) return;
	
	/* note that NURBS surface is a special case because we mimic */
	/* the behaviour of "select more" of mesh tools.	      */
	/* The algorithm is designed to work in planar cases so it    */
	/* may not be optimal always (example: end of NURBS sphere)   */
	if(G.obedit->type==OB_SURF) {
		for(nu= editNurb.first; nu; nu= nu->next) {
			a= nu->pntsu*nu->pntsv;
			bp= nu->bp;
			selbpoints= MEM_callocN(sizeof(short)*a-nu->pntsu, "selectlist");
			while(a > 0) {
				if((selbpoints[a]!=1) && (bp->hide==0) && (bp->f1 & SELECT)) {
					/* upper control point */
					if(a%nu->pntsu != 0) {
						tempbp= bp-1;
						if(!(tempbp->f1 & SELECT)) select_bpoint(tempbp, SELECT, 1, VISIBLE); 
					}

					/* left control point. select only if it is not selected already */
					if(a-nu->pntsu > 0) {
						sel= 0;
						tempbp= bp+nu->pntsu;
						if(!(tempbp->f1 & SELECT)) sel= select_bpoint(tempbp, SELECT, 1, VISIBLE); 
						/* make sure selected bpoint is discarded */
						if(sel == 1) selbpoints[a-nu->pntsu]= 1;
					}
					
					/* right control point */
					if(a+nu->pntsu < nu->pntsu*nu->pntsv) {
						tempbp= bp-nu->pntsu;
						if(!(tempbp->f1 & SELECT)) select_bpoint(tempbp, SELECT, 1, VISIBLE); 
					}
				
					/* lower control point. skip next bp in case selection was made */
					if(a%nu->pntsu != 1) {
						sel= 0;
						tempbp= bp+1;
						if(!(tempbp->f1 & 1)) sel= select_bpoint(tempbp, SELECT, 1, VISIBLE); 
						if(sel) {
							bp++;	
							a--;
						}
					}				
				}

				bp++;
				a--;
			}
			
			MEM_freeN(selbpoints);
		}
	}
	else {
		select_adjacent_cp(1, 0, SELECT);
		select_adjacent_cp(-1, 0, SELECT);
	}
		
	countall();
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Select More");
}

/* basic method: deselect if control point doesn't have all neighbours selected */
void select_less_nurb()
{
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int a;
	short sel= 0, lastsel= 0;
	short *selbpoints;

	if(G.obedit==0) return;
	
	if(G.obedit->type==OB_SURF) {		
		for(nu= editNurb.first; nu; nu= nu->next) {
			a= nu->pntsu*nu->pntsv;
			bp= nu->bp;
			selbpoints= MEM_callocN(sizeof(short)*a, "selectlist");
			while(a--) {
				if((bp->hide==0) && (bp->f1 & SELECT)) {
					sel= 0;
									
					/* check if neighbours have been selected */	
					/* edges of surface are an exception */	
					if((a+1)%nu->pntsu==0) sel++;	
					else {
						bp--;
						if((selbpoints[a+1]==1) || ((bp->hide==0) && (bp->f1 & SELECT))) sel++;
						bp++;
					}
					
					if((a+1)%nu->pntsu==1) sel++;
					else {
						bp++;
						if((bp->hide==0) && (bp->f1 & SELECT)) sel++;
						bp--;
					}
					
					if(a+1 > nu->pntsu*nu->pntsv-nu->pntsu) sel++;
					else {
						bp-=nu->pntsu;
						if((selbpoints[a+nu->pntsu]==1) || ((bp->hide==0) && (bp->f1 & SELECT))) sel++;
						bp+=nu->pntsu;
					}
									
					if(a < nu->pntsu) sel++;
					else {
						bp+=nu->pntsu;
						if((bp->hide==0) && (bp->f1 & SELECT)) sel++;
						bp-=nu->pntsu;
					}
													
					if(sel!=4) {
						select_bpoint(bp, DESELECT, 1, VISIBLE); 
						selbpoints[a]= 1;												
					}									
				}
				else lastsel= 0;
					
				bp++;
			}
			
			MEM_freeN(selbpoints);
		}
	}
	else {
		for(nu= editNurb.first; nu; nu= nu->next) {
			lastsel=0;
			/* check what type of curve/nurb it is */
			if((nu->type & 7)==CU_BEZIER) {			
				a= nu->pntsu;
				bezt= nu->bezt;
				while(a--) {
					if((bezt->hide==0) && (bezt->f2 & SELECT)) {
						if(lastsel==1) sel= 1;
						else sel= 0;
												
						/* check if neighbours have been selected */						
						/* first and last are exceptions */					
						if(a==nu->pntsu-1) sel++; 
						else { 
							bezt--;
							if((bezt->hide==0) && (bezt->f2 & SELECT)) sel++;
							bezt++;
						}
						
						if(a==0) sel++;
						else {
							bezt++;
							if((bezt->hide==0) && (bezt->f2 & SELECT)) sel++;
							bezt--;
						}

						if(sel!=2) {
							select_beztriple(bezt, DESELECT, 1, VISIBLE);	
							lastsel= 1;
						}
						else lastsel= 0;
					}
					else lastsel= 0;
						
					bezt++;	
				}
			}
			else {
				a= nu->pntsu*nu->pntsv;
				bp= nu->bp;
				while(a--) {
					if((lastsel==0) && (bp->hide==0) && (bp->f1 & SELECT)) {
						if(lastsel!=0) sel= 1;
						else sel= 0;
						
						/* first and last are exceptions */					
						if(a==nu->pntsu*nu->pntsv-1) sel++; 
						else { 
							bp--;
							if((bp->hide==0) && (bp->f1 & SELECT)) sel++;
							bp++;
						}
						
						if(a==0) sel++;
						else {
							bp++;
							if((bp->hide==0) && (bp->f1 & SELECT)) sel++;
							bp--;
						}
											
						if(sel!=2) {
							select_bpoint(bp, DESELECT, 1, VISIBLE); 	
							lastsel= 1;						
						}				
						else lastsel= 0;					
					}
					else lastsel= 0;
						
					bp++;
				}
			}
		}
	}
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Select Less");
}

/* this function could be moved elsewhere as it can be reused in other parts of the source needing randomized list */
/* returns list containing -1 in indices that have been left out of the list. otherwise index contains reference   */
/* to next index. basically *list contains a linked list							   */
static void generate_pickable_list(int *list, int size, int pickamount)
{
	int i, j, removable;
	
	BLI_srand( BLI_rand() ); /* random seed */

	/* generate list in form 0->1, 1->2, 2->3, ... i-2->i-1, i->0 */
	for(i=0; i<size; i++) {
		if(i == size-1) list[i]= 0;
		else list[i]= i+1;
	}

	for(i=0; i<size-pickamount; i++) { 
		removable= floor(BLI_frand()*(size-1)+0.5); /* with rounding. frand returns [0,1] */
		
		/* seek proper item as the one randomly selected might not be appropriate */
		for(j=0; j<size; j++, removable++) {
			if(list[removable] != -1) break;
			if(removable == size-1) removable= -1;
		}
				
		/* pick unwanted item out of the list */
		list[list[removable]]= -1; /* mark former last as invalid */

		if(list[removable] == size-1) list[removable]= 0;
		else list[removable]= list[removable]+1;
	}
}

void select_random_nurb()
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	static short randfac= 50;
	int amounttoselect, amountofcps, a, i, k= 0;
	int *itemstobeselected;
	
	if(!G.obedit) return;

	if(!button(&randfac,0, 100,"Percentage:")) return;

	if(randfac == 0) return;
	
	amountofcps= count_curveverts_without_handles(&editNurb);
	itemstobeselected= MEM_callocN(sizeof(int) * amountofcps, "selectitems");
	amounttoselect= floor(randfac * amountofcps / 100 + 0.5);
	generate_pickable_list(itemstobeselected, amountofcps, amounttoselect);
	
	/* select elements */
	for(i=1, nu= editNurb.first; nu; nu= nu->next) {	
		if((nu->type & 7)==CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(itemstobeselected[k] != -1) select_beztriple(bezt, SELECT, 1, VISIBLE);
				k++;
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				if(itemstobeselected[k] != -1) select_bpoint(bp, SELECT, 1, VISIBLE); 
				k++;
				bp++;
			}
		}		
	}
		
	MEM_freeN(itemstobeselected);

	countall();
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Select Random");	
}

void select_every_nth_nurb()
{
	static short nfac= 2;
		
	if(!G.obedit) return;

	if(!button(&nfac, 2, 25,"N:")) return;
						
	select_adjacent_cp(nfac, 1, SELECT);
	select_adjacent_cp(-nfac, 1, SELECT);
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Select Every Nth");	
}

void adduplicate_nurb()
{

	if(G.vd==0 || (G.vd->lay & G.obedit->lay)==0 ) return;

	adduplicateflagNurb(1);

	countall();
	BIF_TransformSetUndo("Add Duplicate");
	initTransform(TFM_TRANSLATION, CTX_NO_PET);
	Transform();
}

void delNurb()
{
	Nurb *nu, *next, *nu1;
	BezTriple *bezt, *bezt1, *bezt2;
	BPoint *bp, *bp1, *bp2;
	int a;
	short event, cut = 0;

	if(G.obedit==0 ) return;
	if(G.vd==0 || (G.vd->lay & G.obedit->lay)==0 ) return;

	if(G.obedit->type==OB_SURF) event= pupmenu("Erase %t|Selected%x0|All%x2");
	else event= pupmenu("Erase %t|Selected%x0|Segment%x1|All%x2");

	if(event== -1) return;

	if(G.obedit->type==OB_SURF) {
		if(event==0) deleteflagNurb(1);
		else freeNurblist(&editNurb);

		countall();
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		BIF_undo_push("Delete");
	
		return;
	}

	if(event==0) {
		/* first loop, can we remove entire pieces? */
		nu= editNurb.first;
		while(nu) {
			next= nu->next;
			if( (nu->type & 7)==CU_BEZIER ) {
				bezt= nu->bezt;
				a= nu->pntsu;
				if(a) {
					while(a) {
						if( BEZSELECTED_HIDDENHANDLES(bezt) );
						else break;
						a--;
						bezt++;
					}
					if(a==0) {
						BLI_remlink(&editNurb, nu);
						freeNurb(nu); nu= NULL;
					}
				}
			}
			else {
				bp= nu->bp;
				a= nu->pntsu*nu->pntsv;
				if(a) {
					while(a) {
						if(bp->f1 & SELECT);
						else break;
						a--;
						bp++;
					}
					if(a==0) {
						BLI_remlink(&editNurb, nu);
						freeNurb(nu); nu= NULL;
					}
				}
			}
			
			/* Never allow the order to exceed the number of points
			- note, this is ok but changes unselected nurbs, disable for now */
			/*
			if ((nu!= NULL) && ((nu->type & 7)==CU_NURBS)) {
				clamp_nurb_order_u(nu);
			}
			*/
			nu= next;
		}
		/* 2nd loop, delete small pieces: just for curves */
		nu= editNurb.first;
		while(nu) {
			next= nu->next;
			event= 0;
			if( (nu->type & 7)==CU_BEZIER ) {
				bezt= nu->bezt;
				for(a=0;a<nu->pntsu;a++) {
					if( BEZSELECTED_HIDDENHANDLES(bezt) ) {
						memmove(bezt, bezt+1, (nu->pntsu-a-1)*sizeof(BezTriple));
						nu->pntsu--;
						a--;
						event= 1;
					}
					else bezt++;
				}
				if(event) {
					bezt1 =
						(BezTriple*)MEM_mallocN((nu->pntsu) * sizeof(BezTriple), "delNurb");
					memcpy(bezt1, nu->bezt, (nu->pntsu)*sizeof(BezTriple) );
					MEM_freeN(nu->bezt);
					nu->bezt= bezt1;
					calchandlesNurb(nu);
				}
			}
			else if(nu->pntsv==1) {
				bp= nu->bp;
				
				for(a=0;a<nu->pntsu;a++) {
					if( bp->f1 & SELECT ) {
						memmove(bp, bp+1, (nu->pntsu-a-1)*sizeof(BPoint));
						nu->pntsu--;
						a--;
						event= 1;
					}
					else {
						bp++;
					}
				}
				if(event) {
					bp1 = (BPoint*)MEM_mallocN(nu->pntsu * sizeof(BPoint), "delNurb2");
					memcpy(bp1, nu->bp, (nu->pntsu)*sizeof(BPoint) );
					MEM_freeN(nu->bp);
					nu->bp= bp1;
					
					/* Never allow the order to exceed the number of points\
					- note, this is ok but changes unselected nurbs, disable for now */
					/*
					if ((nu->type & 7)==CU_NURBS) {
						clamp_nurb_order_u(nu);
					}*/
				}
				makeknots(nu, 1, nu->flagu>>1);
			}
			nu= next;
		}
	}
	else if(event==1) {	/* erase segment */
		/* find the 2 selected points */
		bezt1= bezt2= 0;
		bp1= bp2= 0;
		nu= editNurb.first;
		nu1= 0;
		while(nu) {
			next= nu->next;
			if( (nu->type & 7)==CU_BEZIER ) {
				bezt= nu->bezt;
				for(a=0; a<nu->pntsu-1; a++) {
					if( BEZSELECTED_HIDDENHANDLES(bezt) ) {
						bezt1= bezt;
						bezt2= bezt+1;
						if( (bezt2->f1 & SELECT) || (bezt2->f2 & SELECT) || (bezt2->f3 & SELECT) ) ;
						else {	/* maybe do not make cyclic */
							if(a==0 && (nu->flagu & CU_CYCLIC) ) {
								bezt2= bezt+(nu->pntsu-1);
								if( (bezt2->f1 & SELECT) || (bezt2->f2 & SELECT) || (bezt2->f3 & SELECT) ) {
									nu->flagu &= ~CU_CYCLIC;
									DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
									allqueue(REDRAWVIEW3D, 0);
									allqueue(REDRAWBUTSEDIT, 0);
									BIF_undo_push("Delete");
								}
							}
							return;
						}
						cut= a;
						nu1= nu;
						break;
					}
					bezt++;
				}
			}
			else if(nu->pntsv==1) {
				bp= nu->bp;
				for(a=0; a<nu->pntsu-1; a++) {
					if( bp->f1 & SELECT ) {
						bp1= bp;
						bp2= bp+1;
						if( bp2->f1 & 1 ) ;
						else {	/* maybe do not make cyclic */
							if(a==0 && (nu->flagu & CU_CYCLIC) ) {
								bp2= bp+(nu->pntsu-1);
								if( bp2->f1 & SELECT ) {
									nu->flagu &= ~CU_CYCLIC;
									DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
									allqueue(REDRAWVIEW3D, 0);
									allqueue(REDRAWBUTSEDIT, 0);
									BIF_undo_push("Delete");
								}
							}
							return;
						}
						cut= a;
						nu1= nu;
						break;
					}
					bp++;
				}
			}
			if(nu1) break;

			nu= nu->next;
		}
		if(nu1) {
			if(bezt1) {
				if(nu1->pntsu==2) {	/* remove completely */
					BLI_remlink(&editNurb, nu);
					freeNurb(nu); nu = NULL;
				}
				else if(nu1->flagu & CU_CYCLIC) {	/* cyclic */
					bezt =
						(BezTriple*)MEM_mallocN((cut+1) * sizeof(BezTriple), "delNurb1");
					memcpy(bezt, nu1->bezt,(cut+1)*sizeof(BezTriple));
					a= nu1->pntsu-cut-1;
					memcpy(nu1->bezt, bezt2, a*sizeof(BezTriple));
					memcpy(nu1->bezt+a, bezt, (cut+1)*sizeof(BezTriple));
					nu1->flagu &= ~CU_CYCLIC;
					MEM_freeN(bezt);
					calchandlesNurb(nu);
				}
				else {			/* add new curve */

/* seems to be an error here... but where? (a can become zero) */

					nu =
						(Nurb*)MEM_mallocN(sizeof(Nurb), "delNurb2");
					memcpy(nu, nu1, sizeof(Nurb));
					BLI_addtail(&editNurb, nu);
					nu->bezt =
						(BezTriple*)MEM_mallocN((cut+1) * sizeof(BezTriple), "delNurb3");
					memcpy(nu->bezt, nu1->bezt,(cut+1)*sizeof(BezTriple));
					a= nu1->pntsu-cut-1;
					
					bezt =
						(BezTriple*)MEM_mallocN(a * sizeof(BezTriple), "delNurb4");
					memcpy(bezt, nu1->bezt+cut+1,a*sizeof(BezTriple));
					MEM_freeN(nu1->bezt);
					nu1->bezt= bezt;
					nu1->pntsu= a;
					nu->pntsu= cut+1;
					
					
					calchandlesNurb(nu);
					calchandlesNurb(nu1);
				}
			}
			else if(bp1) {
				if(nu1->pntsu==2) {	/* remove completely */
					BLI_remlink(&editNurb, nu);
					freeNurb(nu); nu= NULL;
				}
				else if(nu1->flagu & CU_CYCLIC) {	/* cyclic */
					bp =
						(BPoint*)MEM_mallocN((cut+1) * sizeof(BPoint), "delNurb5");
					memcpy(bp, nu1->bp,(cut+1)*sizeof(BPoint));
					a= nu1->pntsu-cut-1;
					memcpy(nu1->bp, bp2, a*sizeof(BPoint));
					memcpy(nu1->bp+a, bp, (cut+1)*sizeof(BPoint));
					nu1->flagu &= ~CU_CYCLIC;
					MEM_freeN(bp);
				}
				else {			/* add new curve */
					nu = (Nurb*)MEM_mallocN(sizeof(Nurb), "delNurb6");
					memcpy(nu, nu1, sizeof(Nurb));
					BLI_addtail(&editNurb, nu);
					nu->bp =
						(BPoint*)MEM_mallocN((cut+1) * sizeof(BPoint), "delNurb7");
					memcpy(nu->bp, nu1->bp,(cut+1)*sizeof(BPoint));
					a= nu1->pntsu-cut-1;
					bp =
						(BPoint*)MEM_mallocN(a * sizeof(BPoint), "delNurb8");
					memcpy(bp, nu1->bp+cut+1,a*sizeof(BPoint));
					MEM_freeN(nu1->bp);
					nu1->bp= bp;
					nu1->pntsu= a;
					nu->pntsu= cut+1;
				}
			}
		}
	}
	else if(event==2) {
		freeNurblist(&editNurb);
	}

	countall();
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Delete");
	
}

void nurb_set_smooth(short event)
{
	Nurb *nu;
	
	if(G.obedit==0) return;
	
	if(G.obedit->type != OB_CURVE) return;
	
	for(nu= editNurb.first; nu; nu= nu->next) {
		if(isNurbsel(nu)) {
			if(event==1) nu->flag |= CU_SMOOTH;
			else if(event==0) nu->flag &= ~CU_SMOOTH;
		}
	}
	
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
	
	if(event==1) BIF_undo_push("Set Smooth");
	else if(event==0) BIF_undo_push("Set Solid");
}

int join_curve(int type)
{
	Base *base, *nextb;
	Object *ob;
	Curve *cu;
	Nurb *nu, *newnu;
	BezTriple *bezt;
	BPoint *bp;
	ListBase tempbase;
	float imat[4][4], cmat[4][4];
	int a;
	
	ob= OBACT;
	if (object_data_is_libdata(ob)) {
		error_libdata();
		return 0;
	} 
	
	if(!G.vd || ob->type!=type) return 0;
	if(ob->lay & G.vd->lay); else return 0;
	tempbase.first= tempbase.last= 0;
	
	/* trasnform all selected curves inverse in obact */
	Mat4Invert(imat, ob->obmat);
	
	base= FIRSTBASE;
	while(base) {
		nextb= base->next;
		if TESTBASE(base) {
			if(base->object->type==type) {
				if(base->object != ob) {
				
					cu= base->object->data;
				
					if(cu->nurb.first) {
						/* watch it: switch order here really goes wrong */
						Mat4MulMat4(cmat, base->object->obmat, imat);
						
						nu= cu->nurb.first;
						while(nu) {
							newnu= duplicateNurb(nu);
							BLI_addtail(&tempbase, newnu);
							
							if( (bezt= newnu->bezt) ) {
								a= newnu->pntsu;
								while(a--) {
									Mat4MulVecfl(cmat, bezt->vec[0]);
									Mat4MulVecfl(cmat, bezt->vec[1]);
									Mat4MulVecfl(cmat, bezt->vec[2]);
									bezt++;
								}
							}
							if( (bp= newnu->bp) ) {
								a= newnu->pntsu*nu->pntsv;
								while(a--) {
									Mat4MulVecfl(cmat, bp->vec);
									bp++;
								}
							}
							nu= nu->next;
						}
					}
				
					free_and_unlink_base(base);
				}
			}
		}
		base= nextb;
	}
	
	cu= ob->data;
	addlisttolist(&cu->nurb, &tempbase);
	
	DAG_scene_sort(G.scene);	// because we removed object(s), call before editmode!
	
	enter_editmode(EM_WAITCURSOR);
	exit_editmode(EM_FREEDATA|EM_WAITCURSOR);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Join");
	return 1;
}



Nurb *addNurbprim(int type, int stype, int newname)
/* type: &8= 2D;  0=poly,1 bez, 4 nurb
 * stype:   0: 2/4 points curve
 *	    1: 8 points circle
 *	    2: 4x4 patch Nurb
 *	    3: tube 4:sphere 5:donut
 *		6: 5 points,  5th order straight line (for anim path) 
 */
{
	static int xzproj= 0;
	Nurb *nu = NULL;
	BezTriple *bezt;
	BPoint *bp;
	float *curs, cent[3],vec[3],imat[3][3],mat[3][3];
	float fac,cmat[3][3], grid;
	int a, b;
	
	if (G.vd)	grid = G.vd->grid;
	else		grid = 1.0;
	
	/* imat and center and size */
	if(G.obedit) {
		
		Mat3CpyMat4(mat, G.obedit->obmat);
		curs= give_cursor();
		VECCOPY(cent, curs);
		cent[0]-= G.obedit->obmat[3][0];
		cent[1]-= G.obedit->obmat[3][1];
		cent[2]-= G.obedit->obmat[3][2];
		
		if (G.vd) {
			if ( !(newname) || U.flag & USER_ADD_VIEWALIGNED) Mat3CpyMat4(imat, G.vd->viewmat);
			else Mat3One(imat);
			Mat3MulVecfl(imat, cent);
			Mat3MulMat3(cmat, imat, mat);
			Mat3Inv(imat, cmat);
		}
		setflagsNurb(0);
	}
	else {
		Mat3One(imat);
		cent[0]= cent[1]= cent[2]= 0.0;
	}

	if ELEM5(stype, 0, 1, 2, 4, 6) {
		nu = (Nurb*)MEM_callocN(sizeof(Nurb), "addNurbprim");
		nu->type= type;
		nu->resolu= 12;
		nu->resolv= 12;
	}

	switch(stype) {
	case 0:	/* curve */
		if(newname) {
			rename_id((ID *)G.obedit, "Curve");
			rename_id((ID *)G.obedit->data, "Curve");
		}
		if((type & 7)==CU_BEZIER) {
			nu->pntsu= 2;
			nu->bezt =
				(BezTriple*)MEM_callocN(2 * sizeof(BezTriple), "addNurbprim1");
			bezt= nu->bezt;
			bezt->h1= bezt->h2= HD_ALIGN;
			bezt->f1= bezt->f2= bezt->f3= SELECT;
			bezt->radius = 1.0;

			for(a=0;a<3;a++) {
				VECCOPY(bezt->vec[a], cent);
			}
			bezt->vec[1][0]+= -grid;
			bezt->vec[0][0]+= -1.5*grid;
			bezt->vec[0][1]+= -0.5*grid;
			bezt->vec[2][0]+= -0.5*grid;
			bezt->vec[2][1]+=  0.5*grid;
			for(a=0;a<3;a++) Mat3MulVecfl(imat, bezt->vec[a]);

			bezt++;
			bezt->h1= bezt->h2= HD_ALIGN;
			bezt->f1= bezt->f2= bezt->f3= SELECT;
			bezt->radius = bezt->weight = 1.0;

			for(a=0;a<3;a++) {
				VECCOPY(bezt->vec[a], cent);
			}
			bezt->vec[1][0]+= grid;
			for(a=0;a<3;a++) Mat3MulVecfl(imat, bezt->vec[a]);

			calchandlesNurb(nu);
		}
		else {
			
			nu->pntsu= 4;
			nu->pntsv= 1;
			nu->orderu= 4;
			nu->bp= callocstructN(BPoint, 4, "addNurbprim3");

			bp= nu->bp;
			for(a=0;a<4;a++, bp++) {
				VECCOPY(bp->vec, cent);
				bp->vec[3]= 1.0;
				bp->f1= SELECT;
				bp->radius = bp->weight = 1.0;
			}

			bp= nu->bp;
			bp->vec[0]+= -1.5*grid; 
			bp++;
			bp->vec[0]+= -grid;
			bp->vec[1]+=  grid; 
			bp++;
			bp->vec[0]+= grid;
			bp->vec[1]+= grid; 
			bp++;
			bp->vec[0]+= 1.5*grid;

			bp= nu->bp;
			for(a=0;a<4;a++, bp++) Mat3MulVecfl(imat,bp->vec);

			if((type & 7)==4) {
				nu->knotsu= 0;	/* makeknots allocates */
				makeknots(nu, 1, nu->flagu>>1);
			}

		}
		break;
	case 6:	/* 5 point path */
		nu->pntsu= 5;
		nu->pntsv= 1;
		nu->orderu= 5;
		nu->flagu= 2;	/* endpoint */
		nu->resolu= 32;
		nu->bp= callocstructN(BPoint, 5, "addNurbprim3");

		bp= nu->bp;
		for(a=0;a<5;a++, bp++) {
			VECCOPY(bp->vec, cent);
			bp->vec[3]= 1.0;
			bp->f1= SELECT;
			bp->radius = bp->weight = 1.0;
		}

		bp= nu->bp;
		bp->vec[0]+= -2.0*grid; 
		bp++;
		bp->vec[0]+= -grid;
		bp++; bp++;
		bp->vec[0]+= grid;
		bp++;
		bp->vec[0]+= 2.0*grid;

		bp= nu->bp;
		for(a=0;a<5;a++, bp++) Mat3MulVecfl(imat,bp->vec);

		if((type & 7)==4) {
			nu->knotsu= 0;	/* makeknots allocates */
			makeknots(nu, 1, nu->flagu>>1);
		}

		break;
	case 1:	/* circle */
		if(newname) {
			rename_id((ID *)G.obedit, "CurveCircle");
			rename_id((ID *)G.obedit->data, "CurveCircle");
		}
		if((type & 7)==CU_BEZIER) {
			nu->pntsu= 4;
			nu->bezt= callocstructN(BezTriple, 4, "addNurbprim1");
			nu->flagu= CU_CYCLIC;
			bezt= nu->bezt;

			for(a=0;a<3;a++) {
				VECCOPY(bezt->vec[a], cent);
			}
			bezt->h1= bezt->h2= HD_AUTO;
			bezt->f1= bezt->f2= bezt->f3= SELECT;
			bezt->vec[1][0]+= -grid;
			for(a=0;a<3;a++) Mat3MulVecfl(imat,bezt->vec[a]);
			bezt->radius = bezt->weight = 1.0;
			
			bezt++;
			for(a=0;a<3;a++) {
				VECCOPY(bezt->vec[a], cent);
			}
			bezt->h1= bezt->h2= HD_AUTO;
			bezt->f1= bezt->f2= bezt->f3= SELECT;
			bezt->vec[1][1]+= grid;
			for(a=0;a<3;a++) Mat3MulVecfl(imat,bezt->vec[a]);
			bezt->radius = bezt->weight = 1.0;

			bezt++;
			for(a=0;a<3;a++) {
				VECCOPY(bezt->vec[a], cent);
			}
			bezt->h1= bezt->h2= HD_AUTO;
			bezt->f1= bezt->f2= bezt->f3= SELECT;
			bezt->vec[1][0]+= grid;
			for(a=0;a<3;a++) Mat3MulVecfl(imat,bezt->vec[a]);
			bezt->radius = bezt->weight = 1.0;

			bezt++;
			for(a=0;a<3;a++) {
				VECCOPY(bezt->vec[a], cent);
			}
			bezt->h1= bezt->h2= HD_AUTO;
			bezt->f1= bezt->f2= bezt->f3= SELECT;
			bezt->vec[1][1]+= -grid;
			for(a=0;a<3;a++) Mat3MulVecfl(imat,bezt->vec[a]);
			bezt->radius = bezt->weight = 1.0;

			calchandlesNurb(nu);
		}
		else if( (type & 7)==CU_NURBS ) {  /* nurb */
			nu->pntsu= 8;
			nu->pntsv= 1;
			nu->orderu= 4;
			nu->bp= callocstructN(BPoint, 8, "addNurbprim6");
			nu->flagu= CU_CYCLIC;
			bp= nu->bp;

			for(a=0; a<8; a++) {
				bp->f1= SELECT;
				VECCOPY(bp->vec, cent);

				if(xzproj==0) {
					bp->vec[0]+= nurbcircle[a][0]*grid;
					bp->vec[1]+= nurbcircle[a][1]*grid;
				}
				else {
					bp->vec[0]+= 0.25*nurbcircle[a][0]*grid-.75*grid;
					bp->vec[2]+= 0.25*nurbcircle[a][1]*grid;
				}
				if(a & 1) bp->vec[3]= 0.25*sqrt(2.0);
				else bp->vec[3]= 1.0;
				Mat3MulVecfl(imat,bp->vec);
				bp->radius = bp->weight = 1.0;
				
				bp++;
			}

			makeknots(nu, 1, nu->flagu>>1);
		}
		break;
	case 2:	/* 4x4 patch */
		if( (type & 7)==CU_NURBS ) {  /* nurb */
			if(newname) {
				rename_id((ID *)G.obedit, "Surf");
				rename_id((ID *)G.obedit->data, "Surf");
			}

			nu->pntsu= 4;
			nu->pntsv= 4;
			nu->orderu= 4;
			nu->orderv= 4;
			nu->flag= CU_SMOOTH;
			nu->bp= callocstructN(BPoint, 4*4, "addNurbprim6");
			nu->flagu= 0;
			nu->flagv= 0;
			bp= nu->bp;

			for(a=0; a<4; a++) {
				for(b=0; b<4; b++) {
					VECCOPY(bp->vec, cent);
					bp->f1= SELECT;
					fac= (float)a -1.5;
					bp->vec[0]+= fac*grid;
					fac= (float)b -1.5;
					bp->vec[1]+= fac*grid;
					if(a==1 || a==2) if(b==1 || b==2) {
						bp->vec[2]+= grid;
					}
					Mat3MulVecfl(imat,bp->vec);
					bp->vec[3]= 1.0;
					bp++;
				}
			}

			makeknots(nu, 1, nu->flagu>>1);
			makeknots(nu, 2, nu->flagv>>1);
		}
		break;
	case 3:	/* tube */
		if( (type & 7)==CU_NURBS ) {
			if(newname) {
				rename_id((ID *)G.obedit, "SurfTube");
				rename_id((ID *)G.obedit->data, "SurfTube");
			}

			nu= addNurbprim(4, 1, newname);  /* circle */
			nu->resolu= 32;
			nu->flag= CU_SMOOTH;
			BLI_addtail(&editNurb, nu); /* temporal for extrude and translate */
			vec[0]=vec[1]= 0.0;
			vec[2]= -grid;
			Mat3MulVecfl(imat, vec);
			translateflagNurb(1, vec);
			extrudeflagNurb(1);
			vec[0]= -2*vec[0]; 
			vec[1]= -2*vec[1]; 
			vec[2]= -2*vec[2];
			translateflagNurb(1, vec);

			BLI_remlink(&editNurb, nu);

			a= nu->pntsu*nu->pntsv;
			bp= nu->bp;
			while(a-- >0) {
				bp->f1 |= SELECT;
				bp++;
			}
		}
		break;
	case 4:	/* sphere */
		if( (type & 7)==CU_NURBS ) {
			if(newname) {
				rename_id((ID *)G.obedit, "SurfSphere");
				rename_id((ID *)G.obedit->data, "SurfSphere");
			}

			nu->pntsu= 5;
			nu->pntsv= 1;
			nu->orderu= 3;
			nu->resolu= 24;
			nu->resolv= 32;
			nu->flag= CU_SMOOTH;
			nu->bp= callocstructN(BPoint, 5, "addNurbprim6");
			nu->flagu= 0;
			bp= nu->bp;

			for(a=0; a<5; a++) {
				bp->f1= SELECT;
				VECCOPY(bp->vec, cent);
				bp->vec[0]+= nurbcircle[a][0]*grid;
				bp->vec[2]+= nurbcircle[a][1]*grid;
				if(a & 1) bp->vec[3]= 0.5*sqrt(2.0);
				else bp->vec[3]= 1.0;
				Mat3MulVecfl(imat,bp->vec);
				bp++;
			}
			nu->flagu= 4;
			makeknots(nu, 1, nu->flagu>>1);

			BLI_addtail(&editNurb, nu); /* temporal for spin */
			if(newname && (U.flag & USER_ADD_VIEWALIGNED) == 0)
				spin_nurb(0, 2);
			else
				spin_nurb(0, 0);

			makeknots(nu, 2, nu->flagv>>1);

			a= nu->pntsu*nu->pntsv;
			bp= nu->bp;
			while(a-- >0) {
				bp->f1 |= SELECT;
				bp++;
			}
			BLI_remlink(&editNurb, nu);
		}
		break;
	case 5:	/* donut */
		if( (type & 7)==CU_NURBS ) {
			if(newname) {
				rename_id((ID *)G.obedit, "SurfDonut");
				rename_id((ID *)G.obedit->data, "SurfDonut");
			}

			xzproj= 1;
			nu= addNurbprim(4, 1, newname);  /* circle */
			xzproj= 0;
			nu->resolu= 24;
			nu->resolv= 32;
			nu->flag= CU_SMOOTH;
			BLI_addtail(&editNurb, nu); /* temporal for extrude and translate */
			if(newname && (U.flag & USER_ADD_VIEWALIGNED) == 0)
				spin_nurb(0, 2);
			else
				spin_nurb(0, 0);

			BLI_remlink(&editNurb, nu);

			a= nu->pntsu*nu->pntsv;
			bp= nu->bp;
			while(a-- >0) {
				bp->f1 |= SELECT;
				bp++;
			}

		}
		break;
	}
	
	/* always do: */
	nu->flag= CU_SMOOTH;
	
	test2DNurb(nu);
	
	return nu;
}

void default_curve_ipo(Curve *cu)
{
	IpoCurve *icu;
	BezTriple *bezt;
	
	if(cu->ipo) return;
	
	cu->ipo= add_ipo("CurveIpo", ID_CU);
	
	icu= MEM_callocN(sizeof(IpoCurve), "ipocurve");
			
	icu->blocktype= ID_CU;
	icu->adrcode= CU_SPEED;
	icu->flag= IPO_VISIBLE|IPO_SELECT|IPO_AUTO_HORIZ;
	set_icu_vars(icu);
	
	BLI_addtail( &(cu->ipo->curve), icu);
	
	icu->bezt= bezt= MEM_callocN(2*sizeof(BezTriple), "defaultipo");
	icu->totvert= 2;
	
	bezt->hide= IPO_BEZ;
	bezt->f1=bezt->f2= bezt->f3= SELECT;
	bezt->h1= bezt->h2= HD_AUTO;
	bezt++;
	bezt->vec[1][0]= 100.0;
	bezt->vec[1][1]= 1.0;
	bezt->hide= IPO_BEZ;
	bezt->f1=bezt->f2= bezt->f3= SELECT;
	bezt->h1= bezt->h2= HD_AUTO;
	
	calchandles_ipocurve(icu);
}

void add_primitiveCurve(int stype)
{
	Nurb *nu;
	Curve *cu;
	int type, newname= 0;

	if(G.vd==0) return;
	if(G.scene->id.lib) return;

	/* this function also comes from an info window */
	if ELEM(curarea->spacetype, SPACE_VIEW3D, SPACE_INFO); else return;

	if(stype>=10 && stype<20) type= CU_2D+1;
	else if(stype>=20 && stype<30) type= CU_2D+2;
	else if(stype>=30 && stype<40) type= CU_2D+3;
	else if(stype>=40 && stype<50) {
		if(stype==46) type= 4;
		else type= CU_2D+4;
	}
	else type= CU_2D;

	check_editmode(OB_CURVE);
	
	/* if no obedit: new object and enter editmode */
	if(G.obedit==NULL) {
		add_object_draw(OB_CURVE);
		base_init_from_view3d(BASACT, G.vd);
		G.obedit= BASACT->object;
		
		where_is_object(G.obedit);
		
		make_editNurb();
		setcursor_space(SPACE_VIEW3D, CURSOR_EDIT);
		newname= 1;
		
		cu= G.obedit->data;
		if(stype==46) {
			cu->flag |= (CU_3D+CU_PATH);
			
			default_curve_ipo(cu);
		}
	}
	else {
		cu= G.obedit->data;
	}

	if(cu->flag & CU_3D) type &= ~CU_2D;

	stype= (stype % 10);
	
	nu= addNurbprim(type, stype, newname);   /* 2D */
	
	BLI_addtail(&editNurb, nu);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

	countall();
	
	/* if a new object was created, it stores it in Curve, for reload original data and undo */
	if ( !(newname) || U.flag & USER_ADD_EDITMODE) {
		if(newname) load_editNurb();
	} else {
		exit_editmode(2);
	}
	
	allqueue(REDRAWALL, 0);
	
	BIF_undo_push("Add Curve");
}

void add_primitiveNurb(int type)
{
	Nurb *nu;
	int newname= 0;
	
	if(G.scene->id.lib) return;
	if ELEM(curarea->spacetype, SPACE_VIEW3D, SPACE_INFO); else return;

	check_editmode(OB_SURF);

	/* if no obedit: new object and enter editmode */
	if(G.obedit==0) {
		add_object_draw(OB_SURF);
		base_init_from_view3d(BASACT, G.vd);
		G.obedit= BASACT->object;
		
		where_is_object(G.obedit);
		
		make_editNurb();
		setcursor_space(SPACE_VIEW3D, CURSOR_EDIT);
		newname= 1;
	}

	nu= addNurbprim(4, type, newname);
	BLI_addtail(&editNurb,nu);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

	countall();
	
	/* if a new object was created, it stores it in Curve, for reload original data and undo */
	if ( !(newname) || U.flag & USER_ADD_EDITMODE) {
		if(newname) load_editNurb();
	} else {
		exit_editmode(2);
	}
	allqueue(REDRAWALL, 0);
	
	BIF_undo_push("Add Surface");
}



void clear_tilt()
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	if(okee("Clear tilt")==0) return;

	for(nu= editNurb.first; nu; nu= nu->next) {
		if( nu->bezt ) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(BEZSELECTED_HIDDENHANDLES(bezt)) bezt->alfa= 0.0;
				bezt++;
			}
		}
		else if(nu->bp) {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				if(bp->f1 & SELECT) bp->alfa= 0.0;
				bp++;
			}
		}
	}

	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Clear tilt");
	
}

int bezt_compare (const void *e1, const void *e2)
{
	BezTriple *b1 = *((BezTriple**)e1);
	BezTriple *b2 = *((BezTriple**)e2);

	/* Check numerical values */
	float val = b1->vec[1][0] - b2->vec[1][0];

	if (val<0)
		return -1;

	if (val>0)
		return 1;

	/* Check selected flags : Ensures that selected keys will be listed first */

	if ((b1->f2 & SELECT) && !(b2->f2 & SELECT))
		return -1;
	if (!(b1->f2 & SELECT) && (b2->f2 & SELECT))
		return 1;

	return 0;
}


/* **************** undo for curves ************** */

static void undoCurve_to_editCurve(void *lbv)
{
	ListBase *lb= lbv;
	Nurb *nu, *newnu;
	
	freeNurblist(&editNurb);

	/* copy  */
	for(nu= lb->first; nu; nu= nu->next) {
		newnu= duplicateNurb(nu);
		BLI_addtail(&editNurb, newnu);
	}
}

static void *editCurve_to_undoCurve(void)
{
	ListBase *lb;
	Nurb *nu, *newnu;

	lb= MEM_callocN(sizeof(ListBase), "listbase undo");
	
	/* copy  */
	for(nu= editNurb.first; nu; nu= nu->next) {
		newnu= duplicateNurb(nu);
		BLI_addtail(lb, newnu);
	}
	return lb;
}

static void free_undoCurve(void *lbv)
{
	ListBase *lb= lbv;
	
	freeNurblist(lb);
	MEM_freeN(lb);
}

/* and this is all the undo system needs to know */
void undo_push_curve(char *name)
{
	undo_editmode_push(name, free_undoCurve, undoCurve_to_editCurve, editCurve_to_undoCurve, NULL);
}



/***/
