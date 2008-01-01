/* ***************************************
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



    formfactors.c	nov/dec 1992

    $Id$

 *************************************** */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "radio.h"
#include "RE_render_ext.h"       /* for `RE_zbufferall_radio and RE_zbufferall_radio */

/* locals */
void rad_setmatrices(RadView *vw);
void clearsubflagelem(RNode *rn);
void setsubflagelem(RNode *rn);

RadView hemitop, hemiside;

float calcStokefactor(RPatch *shoot, RPatch *rp, RNode *rn, float *area)
{
	float tvec[3], fac;
	float vec[4][3];	/* vectors of shoot->cent to vertices rp */
	float cross[4][3];	/* cross products of this */
	float rad[4];	/* anlgles between vecs */

	/* test for direction */
	VecSubf(tvec, shoot->cent, rp->cent);
	if( tvec[0]*shoot->norm[0]+ tvec[1]*shoot->norm[1]+ tvec[2]*shoot->norm[2]>0.0)
		return 0.0;
	
	if(rp->type==4) {

		/* corner vectors */
		VecSubf(vec[0], shoot->cent, rn->v1);
		VecSubf(vec[1], shoot->cent, rn->v2);
		VecSubf(vec[2], shoot->cent, rn->v3);
		VecSubf(vec[3], shoot->cent, rn->v4);

		Normalize(vec[0]);
		Normalize(vec[1]);
		Normalize(vec[2]);
		Normalize(vec[3]);

		/* cross product */
		Crossf(cross[0], vec[0], vec[1]);
		Crossf(cross[1], vec[1], vec[2]);
		Crossf(cross[2], vec[2], vec[3]);
		Crossf(cross[3], vec[3], vec[0]);
		Normalize(cross[0]);
		Normalize(cross[1]);
		Normalize(cross[2]);
		Normalize(cross[3]);

		/* angles */
		rad[0]= vec[0][0]*vec[1][0]+ vec[0][1]*vec[1][1]+ vec[0][2]*vec[1][2];
		rad[1]= vec[1][0]*vec[2][0]+ vec[1][1]*vec[2][1]+ vec[1][2]*vec[2][2];
		rad[2]= vec[2][0]*vec[3][0]+ vec[2][1]*vec[3][1]+ vec[2][2]*vec[3][2];
		rad[3]= vec[3][0]*vec[0][0]+ vec[3][1]*vec[0][1]+ vec[3][2]*vec[0][2];

		rad[0]= acos(rad[0]);
		rad[1]= acos(rad[1]);
		rad[2]= acos(rad[2]);
		rad[3]= acos(rad[3]);

		/* Stoke formula */
		VecMulf(cross[0], rad[0]);
		VecMulf(cross[1], rad[1]);
		VecMulf(cross[2], rad[2]);
		VecMulf(cross[3], rad[3]);

		VECCOPY(tvec, shoot->norm);
		fac=  tvec[0]*cross[0][0]+ tvec[1]*cross[0][1]+ tvec[2]*cross[0][2];
		fac+= tvec[0]*cross[1][0]+ tvec[1]*cross[1][1]+ tvec[2]*cross[1][2];
		fac+= tvec[0]*cross[2][0]+ tvec[1]*cross[2][1]+ tvec[2]*cross[2][2];
		fac+= tvec[0]*cross[3][0]+ tvec[1]*cross[3][1]+ tvec[2]*cross[3][2];
	}
	else {
		/* corner vectors */
		VecSubf(vec[0], shoot->cent, rn->v1);
		VecSubf(vec[1], shoot->cent, rn->v2);
		VecSubf(vec[2], shoot->cent, rn->v3);

		Normalize(vec[0]);
		Normalize(vec[1]);
		Normalize(vec[2]);

		/* cross product */
		Crossf(cross[0], vec[0], vec[1]);
		Crossf(cross[1], vec[1], vec[2]);
		Crossf(cross[2], vec[2], vec[0]);
		Normalize(cross[0]);
		Normalize(cross[1]);
		Normalize(cross[2]);

		/* angles */
		rad[0]= vec[0][0]*vec[1][0]+ vec[0][1]*vec[1][1]+ vec[0][2]*vec[1][2];
		rad[1]= vec[1][0]*vec[2][0]+ vec[1][1]*vec[2][1]+ vec[1][2]*vec[2][2];
		rad[2]= vec[2][0]*vec[0][0]+ vec[2][1]*vec[0][1]+ vec[2][2]*vec[0][2];

		rad[0]= acos(rad[0]);
		rad[1]= acos(rad[1]);
		rad[2]= acos(rad[2]);

		/* Stoke formula */
		VecMulf(cross[0], rad[0]);
		VecMulf(cross[1], rad[1]);
		VecMulf(cross[2], rad[2]);

		VECCOPY(tvec, shoot->norm);
		fac=  tvec[0]*cross[0][0]+ tvec[1]*cross[0][1]+ tvec[2]*cross[0][2];
		fac+= tvec[0]*cross[1][0]+ tvec[1]*cross[1][1]+ tvec[2]*cross[1][2];
		fac+= tvec[0]*cross[2][0]+ tvec[1]*cross[2][1]+ tvec[2]*cross[2][2];
	}

	*area= -fac/(2.0*PI);
	return (*area * (shoot->area/rn->area));
}


void calcTopfactors()
{
	float xsq , ysq, xysq;
	float n;
	float *fp;
	int a, b, hres;

	fp = RG.topfactors;
	hres= RG.hemires/2;
	n= hres;

	for (a=0; a<hres; a++) {

		ysq= (n- ((float)a+0.5))/n;
		ysq*= ysq;

		for ( b=0 ; b<hres ; b++ ) {

			xsq= ( n-((float)b+ 0.5) )/n;
			xsq*= xsq;
			xysq=  xsq+ ysq+ 1.0 ;
			xysq*= xysq;

			*fp++ = 1.0/(xysq* PI* n*n);
		}
	}

}

void calcSidefactors()
{
	float xsq , ysq, xysq;
	float n, y;
	float *fp;
	int a, b, hres;

	fp = RG.sidefactors;
	hres= RG.hemires/2;
	n= hres;

	for (a=0; a<hres; a++) {

		y= (n- ((float)a+0.5))/n;
		ysq= y*y;

		for ( b=0 ; b<hres ; b++ ) {

			xsq= ( n-((float)b+ 0.5) )/n;
			xsq*= xsq;
			xysq=  xsq+ ysq+ 1.0 ;
			xysq*= xysq;

			*fp++ = y/(xysq* PI* n*n);
		}
	}

}


void initradiosity()
{
	/* allocates and makes LUTs for top/side factors */
	/* allocates and makes index array */
	int a, hres2;

	if(RG.topfactors) MEM_freeN(RG.topfactors);
	if(RG.sidefactors) MEM_freeN(RG.sidefactors);
	if(RG.index) MEM_freeN(RG.index);

	RG.topfactors= MEM_callocN(RG.hemires*RG.hemires, "initradiosity");
	calcTopfactors();
	RG.sidefactors= MEM_callocN(RG.hemires*RG.hemires, "initradiosity1");
	calcSidefactors();

	RG.index= MEM_callocN(4*RG.hemires, "initradiosity3");
	hres2= RG.hemires/2;
	for(a=0; a<RG.hemires; a++) {
		RG.index[a]=  a<hres2 ? a: (hres2-1-( a % hres2 ));
	}

}

void rad_make_hocos(RadView *vw)
{
	/* float hoco[4]; */
	/* int a; */
	
	/* for(a=0; a< R.totvert;a++) { */
	/* 	projectvert(vec, ver->ho); */
	/* 	ver->clip = testclip(ver->ho); */
/*  */
	/* } */
}

void rad_setmatrices(RadView *vw)	    /* for hemi's */
{
	float up1[3], len, twist;

	i_lookat(vw->cam[0], vw->cam[1], vw->cam[2], vw->tar[0], vw->tar[1], vw->tar[2], 0, vw->viewmat);
	up1[0] = vw->viewmat[0][0]*vw->up[0] + vw->viewmat[1][0]*vw->up[1] + vw->viewmat[2][0]*vw->up[2];
	up1[1] = vw->viewmat[0][1]*vw->up[0] + vw->viewmat[1][1]*vw->up[1] + vw->viewmat[2][1]*vw->up[2];
	up1[2] = vw->viewmat[0][2]*vw->up[0] + vw->viewmat[1][2]*vw->up[1] + vw->viewmat[2][2]*vw->up[2];

	len= up1[0]*up1[0]+up1[1]*up1[1];
	if(len>0.0) {
		twist= -atan2(up1[0], up1[1]);
	}
	else twist= 0.0;
	
	i_lookat(vw->cam[0], vw->cam[1], vw->cam[2], vw->tar[0], vw->tar[1], vw->tar[2], (180.0*twist/M_PI), vw->viewmat);

	/* window matrix was set in inithemiwindows */
	
}


void hemizbuf(RadView *vw)
{
	float *factors;
	unsigned int *rz;
	int a, b, inda, hres;

	rad_setmatrices(vw);
	RE_zbufferall_radio(vw, RG.elem, RG.totelem, RG.re);	/* Render for when we got renderfaces */

	/* count factors */
	if(vw->recty==vw->rectx) factors= RG.topfactors;
	else factors= RG.sidefactors;
	hres= RG.hemires/2;

	rz= vw->rect;
	for(a=0; a<vw->recty; a++) {
		inda= hres*RG.index[a];
		for(b=0; b<vw->rectx; b++, rz++) {
			if(*rz<RG.totelem) {
				RG.formfactors[*rz]+= factors[inda+RG.index[b]];
			}
		}
	}
}

int makeformfactors(RPatch *shoot)
{
	RNode **re;
	float len, vec[3], up[3], side[3], tar[5][3], *fp;
	int a=0, overfl;

	if(RG.totelem==0) return 0;
	
	memset(RG.formfactors, 0, 4*RG.totelem);

	/* set up hemiview */
	/* first: random upvector */
	do {
		a++;
		vec[0]= (float)BLI_drand();
		vec[1]= (float)BLI_drand();
		vec[2]= (float)BLI_drand();
		Crossf(up, shoot->norm, vec);
		len= Normalize(up);
		/* this safety for input normals that are zero or illegal sized */
		if(a>3) return 0;
	} while(len==0.0 || len>1.0);

	VECCOPY(hemitop.up, up);
	VECCOPY(hemiside.up, shoot->norm);

	Crossf(side, shoot->norm, up);

	/* five targets */
	VecAddf(tar[0], shoot->cent, shoot->norm);
	VecAddf(tar[1], shoot->cent, up);
	VecSubf(tar[2], shoot->cent, up);
	VecAddf(tar[3], shoot->cent, side);
	VecSubf(tar[4], shoot->cent, side);

	/* camera */
	VECCOPY(hemiside.cam, shoot->cent);
	VECCOPY(hemitop.cam, shoot->cent);

	/* do it! */
	VECCOPY(hemitop.tar, tar[0]);
	hemizbuf(&hemitop);

	for(a=1; a<5; a++) {
		VECCOPY(hemiside.tar, tar[a]);
		hemizbuf(&hemiside);
	}

	/* convert factors to real radiosity */
	re= RG.elem;
	fp= RG.formfactors;

	overfl= 0;
	for(a= RG.totelem; a>0; a--, re++, fp++) {
	
		if(*fp!=0.0) {
			
			*fp *= shoot->area/(*re)->area;

			if(*fp>1.0) {
				overfl= 1;
				*fp= 1.0001;
			}
		}
	}
	
	if(overfl) {
		if(shoot->first->down1) {
			splitpatch(shoot);
			return 0;
		}
	}
	
	return 1;
}

void applyformfactors(RPatch *shoot)
{
	RPatch *rp;
	RNode **el, *rn;
	float *fp, *ref, unr, ung, unb, r, g, b, w;
	int a;

	unr= shoot->unshot[0];
	ung= shoot->unshot[1];
	unb= shoot->unshot[2];

	fp= RG.formfactors;
	el= RG.elem;
	for(a=0; a<RG.totelem; a++, el++, fp++) {
		rn= *el;
		if(*fp!= 0.0) {
			rp= rn->par;
			ref= rp->ref;
			
			r= (*fp)*unr*ref[0];
			g= (*fp)*ung*ref[1];
			b= (*fp)*unb*ref[2];

			w= rn->area/rp->area;
			rn->totrad[0]+= r;
			rn->totrad[1]+= g;
			rn->totrad[2]+= b;
			
			rp->unshot[0]+= w*r;
			rp->unshot[1]+= w*g;
			rp->unshot[2]+= w*b;
		}
	}
	
	shoot->unshot[0]= shoot->unshot[1]= shoot->unshot[2]= 0.0;
}

RPatch *findshootpatch()
{
	RPatch *rp, *shoot;
	float energy, maxenergy;

	shoot= 0;
	maxenergy= 0.0;
	rp= RG.patchbase.first;
	while(rp) {
		energy= rp->unshot[0]*rp->area;
		energy+= rp->unshot[1]*rp->area;
		energy+= rp->unshot[2]*rp->area;

		if(energy>maxenergy) {
			shoot= rp;
			maxenergy= energy;
		}
		rp= rp->next;
	}

	if(shoot) {
		maxenergy/= RG.totenergy;
		if(maxenergy<RG.convergence) return 0;
	}

	return shoot;
}

void setnodeflags(RNode *rn, int flag, int set)
{
	
	if(rn->down1) {
		setnodeflags(rn->down1, flag, set);
		setnodeflags(rn->down2, flag, set);
	}
	else {
		if(set) rn->f |= flag;
		else rn->f &= ~flag;
	}
}

void backface_test(RPatch *shoot)
{
	RPatch *rp;
	float tvec[3];
	
	rp= RG.patchbase.first;
	while(rp) {
		if(rp!=shoot) {
		
			VecSubf(tvec, shoot->cent, rp->cent);
			if( tvec[0]*rp->norm[0]+ tvec[1]*rp->norm[1]+ tvec[2]*rp->norm[2]<0.0) {		
				setnodeflags(rp->first, RAD_BACKFACE, 1);
			}
		}
		rp= rp->next;
	}
}

void clear_backface_test()
{
	RNode **re;
	int a;
	
	re= RG.elem;
	for(a= RG.totelem-1; a>=0; a--, re++) {
		(*re)->f &= ~RAD_BACKFACE;
	}
	
}

void rad_init_energy()
{
	/* call before shooting */
	/* keep patches and elements, clear all data */
	RNode **el, *rn;
	RPatch *rp;
	int a;

	el= RG.elem;
	for(a=RG.totelem; a>0; a--, el++) {
		rn= *el;
		VECCOPY(rn->totrad, rn->par->emit);
	}

	RG.totenergy= 0.0;
	rp= RG.patchbase.first;
	while(rp) {
		VECCOPY(rp->unshot, rp->emit);

		RG.totenergy+= rp->unshot[0]*rp->area;
		RG.totenergy+= rp->unshot[1]*rp->area;
		RG.totenergy+= rp->unshot[2]*rp->area;

		rp->f= 0;
		
		rp= rp->next;
	}
}

void progressiverad()
{
	RPatch *shoot;
	float unshot[3];
	int it= 0;

	rad_printstatus();
	rad_init_energy();

	shoot=findshootpatch();

	while( shoot ) {

		setnodeflags(shoot->first, RAD_SHOOT, 1);
		
		backface_test(shoot);
		
		drawpatch_ext(shoot, 0x88FF00);

		if(shoot->first->f & RAD_TWOSIDED) {
			VECCOPY(unshot, shoot->unshot);
			VecMulf(shoot->norm, -1.0);
			if(makeformfactors(shoot))
				applyformfactors(shoot);
			VecMulf(shoot->norm, -1.0);
			VECCOPY(shoot->unshot, unshot);
		}
	
		if( makeformfactors(shoot) ) {
			applyformfactors(shoot);
	
			it++;
			//XXX set_timecursor(it);
			if( (it & 3)==1 ) {
				make_node_display();
				rad_forcedraw();
			}
			setnodeflags(shoot->first, RAD_SHOOT, 0);
		}
		
		clear_backface_test();
		
		//XXX if(blender_test_break()) break;
		if(RG.maxiter && RG.maxiter<=it) break;

		shoot=findshootpatch();

	}
	
}


/* *************  subdivideshoot *********** */

void minmaxradelem(RNode *rn, float *min, float *max)
{
	int c;
	
	if(rn->down1) {
		minmaxradelem(rn->down1, min, max);
		minmaxradelem(rn->down2, min, max);
	}
	else {
		for(c=0; c<3; c++) {
			min[c]= MIN2(min[c], rn->totrad[c]);
			max[c]= MAX2(max[c], rn->totrad[c]);
		}
	}
}

void minmaxradelemfilt(RNode *rn, float *min, float *max, float *errmin, float *errmax)
{
	float col[3], area;
	int c;
	
	if(rn->down1) {
		minmaxradelemfilt(rn->down1, min, max, errmin, errmax);
		minmaxradelemfilt(rn->down2, min, max, errmin, errmax);
	}
	else {
		VECCOPY(col, rn->totrad);

		for(c=0; c<3; c++) {
			min[c]= MIN2(min[c], col[c]);
			max[c]= MAX2(max[c], col[c]);
		}

		VecMulf(col, 2.0);
		area= 2.0;
		if(rn->ed1) {
			VecAddf(col, rn->ed1->totrad, col);
			area+= 1.0;
		}
		if(rn->ed2) {
			VecAddf(col, rn->ed2->totrad, col);
			area+= 1.0;
		}
		if(rn->ed3) {
			VecAddf(col, rn->ed3->totrad, col);
			area+= 1.0;
		}
		if(rn->ed4) {
			VecAddf(col, rn->ed4->totrad, col);
			area+= 1.0;
		}
		VecMulf(col, 1.0/area);

		for(c=0; c<3; c++) {
			errmin[c]= MIN2(errmin[c], col[c]);
			errmax[c]= MAX2(errmax[c], col[c]);
		}
	}
}

void setsubflagelem(RNode *rn)
{
	
	if(rn->down1) {
		setsubflagelem(rn->down1);
		setsubflagelem(rn->down2);
	}
	else {
		rn->f |= RAD_SUBDIV;
	}
}

void clearsubflagelem(RNode *rn)
{
	
	if(rn->down1) {
		setsubflagelem(rn->down1);
		setsubflagelem(rn->down2);
	}
	else {
		rn->f &= ~RAD_SUBDIV;
	}
}

void subdivideshootElements(int it)
{
	RPatch *rp, *shoot;
	RNode **el, *rn;
	float *fp, err, stoke, area, min[3], max[3], errmin[3], errmax[3];
	int a, b, c, d, e, f, contin;
	int maxlamp;
	
	if(RG.maxsublamp==0) maxlamp= RG.totlamp;
	else maxlamp= RG.maxsublamp;
	
	while(it) {
		rad_printstatus();
		rad_init_energy();
		it--;
		
		for(a=0; a<maxlamp; a++) {
			shoot= findshootpatch();
			if(shoot==0) break;
			
			//XXX set_timecursor(a);
			drawpatch_ext(shoot, 0x88FF00);
			
			setnodeflags(shoot->first, RAD_SHOOT, 1);
			if( makeformfactors(shoot) ) {
			
				fp= RG.formfactors;
				el= RG.elem;
				for(b=RG.totelem; b>0; b--, el++) {
					rn= *el;
					
					if( (rn->f & RAD_SUBDIV)==0 && *fp!=0.0) {
						if(rn->par->emit[0]+rn->par->emit[1]+rn->par->emit[2]==0.0) {
						
							stoke= calcStokefactor(shoot, rn->par, rn, &area);
							if(stoke!= 0.0) {
							
								err= *fp/stoke;
								
								/* area error */
								area*=(0.5*RG.hemires*RG.hemires);
								
								if(area>35.0) {
									if(err<0.95 || err>1.05) {
										if(err>0.05) {
											rn->f |= RAD_SUBDIV;
											rn->par->f |= RAD_SUBDIV;
										}
									}
								}
							}
							
						}
					}
					
					fp++;
	
				}
	
				applyformfactors(shoot);
		
				if( (a & 3)==1 ) {
					make_node_display();
					rad_forcedraw();
				}
				
				setnodeflags(shoot->first, RAD_SHOOT, 0);
			}
			else a--;
			
			//XXX if(blender_test_break()) break;
		}
		
		/* test for extreme small color change within a patch with subdivflag */
		
		rp= RG.patchbase.first;
		
		while(rp) {
			if(rp->f & RAD_SUBDIV) {		/* rp has elems that need subdiv */
				/* at least 4 levels deep */
				rn= rp->first->down1;
				if(rn) {
					rn= rn->down1;
					if(rn) {
						rn= rn->down1;
						if(rn) rn= rn->down1;
					}
				}
				if(rn) {
					min[0]= min[1]= min[2]= 1.0e10;
					max[0]= max[1]= max[2]= -1.0e10;
					/* errmin and max are the filtered colors */
					errmin[0]= errmin[1]= errmin[2]= 1.0e10;
					errmax[0]= errmax[1]= errmax[2]= -1.0e10;
					minmaxradelemfilt(rp->first, min, max, errmin, errmax);
					
					/* if small difference between colors: no subdiv */
					/* also test for the filtered ones: but with higher critical level */
					
					contin= 0;
					a= abs( calculatecolor(min[0])-calculatecolor(max[0]));
					b= abs( calculatecolor(errmin[0])-calculatecolor(errmax[0]));
					if(a<15 || b<7) {
						c= abs( calculatecolor(min[1])-calculatecolor(max[1]));
						d= abs( calculatecolor(errmin[1])-calculatecolor(errmax[1]));
						if(c<15 || d<7) {
							e= abs( calculatecolor(min[2])-calculatecolor(max[2]));
							f= abs( calculatecolor(errmin[2])-calculatecolor(errmax[2]));
							if(e<15 || f<7) {
								contin= 1;
								clearsubflagelem(rp->first);
								/* printf("%d %d %d %d %d %d\n", a, b, c, d, e, f); */
							}
						}
					}
					if(contin) {
						drawpatch_ext(rp, 0xFFFF);
					}
				}
			}
			rp->f &= ~RAD_SUBDIV;
			rp= rp->next;
		}
		
		contin= 0;
		
		el= RG.elem;
		for(b=RG.totelem; b>0; b--, el++) {
			rn= *el;
			if(rn->f & RAD_SUBDIV) {
				rn->f-= RAD_SUBDIV;
				subdivideNode(rn, 0);
				if(rn->down1) {
					subdivideNode(rn->down1, 0);
					subdivideNode(rn->down2, 0);
					contin= 1;
				}
			}
		}
		makeGlobalElemArray();

		//XXX if(contin==0 || blender_test_break()) break;
	}
	
	make_node_display();
}

void subdivideshootPatches(int it)
{
	RPatch *rp, *shoot, *next;
	float *fp, err, stoke, area;
	int a, contin;
	int maxlamp;
	
	if(RG.maxsublamp==0) maxlamp= RG.totlamp;
	else maxlamp= RG.maxsublamp;
	
	while(it) {
		rad_printstatus();
		rad_init_energy();
		it--;
		
		for(a=0; a<maxlamp; a++) {
			shoot= findshootpatch();
			if(shoot==0) break;
			
			//XXX set_timecursor(a);
			drawpatch_ext(shoot, 0x88FF00);
			
			setnodeflags(shoot->first, RAD_SHOOT, 1);
			
			if( makeformfactors(shoot) ) {
			
				fp= RG.formfactors;
				rp= RG.patchbase.first;
				while(rp) {
					if(*fp!=0.0 && rp!=shoot) {
					
						stoke= calcStokefactor(shoot, rp, rp->first, &area);
						if(stoke!= 0.0) {
							if(area>.1) {	/* does patch receive more than (about)10% of energy? */
								rp->f= RAD_SUBDIV;
							}
							else {
					
								err= *fp/stoke;
							
								/* area error */
								area*=(0.5*RG.hemires*RG.hemires);
								
								if(area>45.0) {
									if(err<0.95 || err>1.05) {
										if(err>0.05) {
														
											rp->f= RAD_SUBDIV;
											
										}
									}
								}
							}
						}
					}
					fp++;
	
					rp= rp->next;
				}
	
				applyformfactors(shoot);
		
				if( (a & 3)==1 ) {
					make_node_display();
					rad_forcedraw();
				}
				
				setnodeflags(shoot->first, RAD_SHOOT, 0);
				
				//XXX if(blender_test_break()) break;
			}
			else a--;
			
		}
		
		contin= 0;

		rp= RG.patchbase.first;
		while(rp) {
			next= rp->next;
			if(rp->f & RAD_SUBDIV) {
				if(rp->emit[0]+rp->emit[1]+rp->emit[2]==0.0) {
					contin= 1;
					subdivideNode(rp->first, 0);
					if(rp->first->down1) {
						subdivideNode(rp->first->down1, 0);
						subdivideNode(rp->first->down2, 0);
					}
				}
			}
			rp= next;
		}
		
		converttopatches();
		makeGlobalElemArray();

		//XXX if(contin==0 || blender_test_break()) break;
	}
	make_node_display();
}

void inithemiwindows()
{
	RadView *vw;

	/* the hemiwindows */
	vw= &(hemitop);
	memset(vw, 0, sizeof(RadView));
	vw->rectx= RG.hemires;
	vw->recty= RG.hemires;
	vw->rectz= MEM_mallocN(sizeof(int)*vw->rectx*vw->recty, "initwindows");
	vw->rect= MEM_mallocN(sizeof(int)*vw->rectx*vw->recty, "initwindows");
	vw->mynear= RG.maxsize/2000.0;
	vw->myfar= 2.0*RG.maxsize;
	vw->wx1= -vw->mynear;
	vw->wx2= vw->mynear;
	vw->wy1= -vw->mynear;
	vw->wy2= vw->mynear;
	
	i_window(vw->wx1, vw->wx2, vw->wy1, vw->wy2, vw->mynear, vw->myfar, vw->winmat);

	hemiside= hemitop;

	vw= &(hemiside);
	vw->recty/= 2;
	vw->wy1= vw->wy2;
	vw->wy2= 0.0;

	i_window(vw->wx1, vw->wx2, vw->wy1, vw->wy2, vw->mynear, vw->myfar, vw->winmat);

}

void closehemiwindows()
{

	if(hemiside.rect) MEM_freeN(hemiside.rect);
	if(hemiside.rectz) MEM_freeN(hemiside.rectz);
	hemiside.rectz= 0;
	hemiside.rect= 0;
	hemitop.rectz= 0;
	hemitop.rect= 0;
}
