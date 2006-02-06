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
 * Contributors: Hos, Robert Wenzlaff.
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* system includes */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* External modules: */
#include "MTC_matrixops.h"
#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_jitter.h"

#include "BKE_utildefines.h"

#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"

#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_texture.h"

/* local include */
#include "renderpipeline.h"
#include "render_types.h"
#include "renderdatabase.h"
#include "pixelblending.h"
#include "pixelshading.h"
#include "gammaCorrectionTables.h"
#include "shadbuf.h"
#include "zbuf.h"

#include "texture.h"

/* own include */
#include "rendercore.h"


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* x and y are current pixels in rect to be rendered */
/* do not normalize! */
void calc_view_vector(float *view, float x, float y)
{

	if(R.r.mode & R_ORTHO) {
		view[0]= view[1]= 0.0;
	}
	else {
		/* move x and y to real viewplane coords */
		x= (x/(float)R.winx);
		view[0]= R.viewplane.xmin + x*(R.viewplane.xmax - R.viewplane.xmin);
		
		y= (y/(float)R.winy);
		view[1]= R.viewplane.ymin + y*(R.viewplane.ymax - R.viewplane.ymin);
		
//		if(R.flag & R_SEC_FIELD) {
//			if(R.r.mode & R_ODDFIELD) view[1]= (y+R.ystart)*R.ycor;
//			else view[1]= (y+R.ystart+1.0)*R.ycor;
//		}
//		else view[1]= (y+R.ystart+R.bluroffsy+0.5)*R.ycor;
	}
	
	view[2]= -R.clipsta;
	
	if(R.r.mode & R_PANORAMA) {
		float u= view[0]; float v= view[2];
		view[0]= R.panoco*u + R.panosi*v;
		view[2]= -R.panosi*u + R.panoco*v;
	}

}

#if 0
static void fogcolor(float *colf, float *rco, float *view)
{
	float alpha, stepsize, startdist, dist, hor[4], zen[3], vec[3], dview[3];
	float div=0.0f, distfac;
	
	hor[0]= R.wrld.horr; hor[1]= R.wrld.horg; hor[2]= R.wrld.horb;
	zen[0]= R.wrld.zenr; zen[1]= R.wrld.zeng; zen[2]= R.wrld.zenb;
	
	VECCOPY(vec, rco);
	
	/* we loop from cur coord to mist start in steps */
	stepsize= 1.0f;
	
	div= ABS(view[2]);
	dview[0]= view[0]/(stepsize*div);
	dview[1]= view[1]/(stepsize*div);
	dview[2]= -stepsize;

	startdist= -rco[2] + BLI_frand();
	for(dist= startdist; dist>R.wrld.miststa; dist-= stepsize) {
		
		hor[0]= R.wrld.horr; hor[1]= R.wrld.horg; hor[2]= R.wrld.horb;
		alpha= 1.0f;
		do_sky_tex(vec, vec, NULL, hor, zen, &alpha);
		
		distfac= (dist-R.wrld.miststa)/R.wrld.mistdist;
		
		hor[3]= hor[0]*distfac*distfac;
		
		/* premul! */
		alpha= hor[3];
		hor[0]= hor[0]*alpha;
		hor[1]= hor[1]*alpha;
		hor[2]= hor[2]*alpha;
		addAlphaOverFloat(colf, hor);
		
		VECSUB(vec, vec, dview);
	}	
}
#endif

float mistfactor(float zcor, float *co)	/* dist en height, return alpha */
{
	float fac, hi;
	
	fac= zcor - R.wrld.miststa;	/* zcor is calculated per pixel */

	/* fac= -co[2]-R.wrld.miststa; */

	if(fac>0.0) {
		if(fac< R.wrld.mistdist) {
			
			fac= (fac/(R.wrld.mistdist));
			
			if(R.wrld.mistype==0) fac*= fac;
			else if(R.wrld.mistype==1);
			else fac= sqrt(fac);
		}
		else fac= 1.0;
	}
	else fac= 0.0;
	
	/* height switched off mist */
	if(R.wrld.misthi!=0.0 && fac!=0.0) {
		/* at height misthi the mist is completely gone */

		hi= R.viewinv[0][2]*co[0]+R.viewinv[1][2]*co[1]+R.viewinv[2][2]*co[2]+R.viewinv[3][2];
		
		if(hi>R.wrld.misthi) fac= 0.0;
		else if(hi>0.0) {
			hi= (R.wrld.misthi-hi)/R.wrld.misthi;
			fac*= hi*hi;
		}
	}

	return (1.0-fac)* (1.0-R.wrld.misi);	
}

static void spothalo(struct LampRen *lar, ShadeInput *shi, float *intens)
{
	double a, b, c, disc, nray[3], npos[3];
	float t0, t1 = 0.0, t2= 0.0, t3, haint;
	float p1[3], p2[3], ladist, maxz = 0.0, maxy = 0.0;
	int snijp, doclip=1, use_yco=0;
	int ok1=0, ok2=0;
	
	*intens= 0.0;
	haint= lar->haint;
	
	if(R.r.mode & R_ORTHO) {
		/* camera pos (view vector) cannot be used... */
		/* camera position (cox,coy,0) rotate around lamp */
		p1[0]= shi->co[0]-lar->co[0];
		p1[1]= shi->co[1]-lar->co[1];
		p1[2]= -lar->co[2];
		MTC_Mat3MulVecfl(lar->imat, p1);
		VECCOPY(npos, p1);	// npos is double!
	}
	else {
		VECCOPY(npos, lar->sh_invcampos);	/* in initlamp calculated */
	}
	
	/* rotate view */
	VECCOPY(nray, shi->view);
	MTC_Mat3MulVecd(lar->imat, nray);
	
	if(R.wrld.mode & WO_MIST) {
		/* patchy... */
		haint *= mistfactor(-lar->co[2], lar->co);
		if(haint==0.0) {
			return;
		}
	}


	/* rotate maxz */
	if(shi->co[2]==0.0) doclip= 0;	/* for when halo at sky */
	else {
		p1[0]= shi->co[0]-lar->co[0];
		p1[1]= shi->co[1]-lar->co[1];
		p1[2]= shi->co[2]-lar->co[2];
	
		maxz= lar->imat[0][2]*p1[0]+lar->imat[1][2]*p1[1]+lar->imat[2][2]*p1[2];
		maxz*= lar->sh_zfac;
		maxy= lar->imat[0][1]*p1[0]+lar->imat[1][1]*p1[1]+lar->imat[2][1]*p1[2];

		if( fabs(nray[2]) <0.000001 ) use_yco= 1;
	}
	
	/* scale z to make sure volume is normalized */	
	nray[2]*= lar->sh_zfac;
	/* nray does not need normalization */
	
	ladist= lar->sh_zfac*lar->dist;
	
	/* solve */
	a = nray[0] * nray[0] + nray[1] * nray[1] - nray[2]*nray[2];
	b = nray[0] * npos[0] + nray[1] * npos[1] - nray[2]*npos[2];
	c = npos[0] * npos[0] + npos[1] * npos[1] - npos[2]*npos[2];

	snijp= 0;
	if (fabs(a) < 0.00000001) {
		/*
		 * Only one intersection point...
		 */
		return;
	}
	else {
		disc = b*b - a*c;
		
		if(disc==0.0) {
			t1=t2= (-b)/ a;
			snijp= 2;
		}
		else if (disc > 0.0) {
			disc = sqrt(disc);
			t1 = (-b + disc) / a;
			t2 = (-b - disc) / a;
			snijp= 2;
		}
	}
	if(snijp==2) {
		/* sort */
		if(t1>t2) {
			a= t1; t1= t2; t2= a;
		}

		/* z of intersection points with diabolo */
		p1[2]= npos[2] + t1*nray[2];
		p2[2]= npos[2] + t2*nray[2];

		/* evaluate both points */
		if(p1[2]<=0.0) ok1= 1;
		if(p2[2]<=0.0 && t1!=t2) ok2= 1;
		
		/* at least 1 point with negative z */
		if(ok1==0 && ok2==0) return;
		
		/* intersction point with -ladist, the bottom of the cone */
		if(use_yco==0) {
			t3= (-ladist-npos[2])/nray[2];
				
			/* de we have to replace one of the intersection points? */
			if(ok1) {
				if(p1[2]<-ladist) t1= t3;
			}
			else {
				ok1= 1;
				t1= t3;
			}
			if(ok2) {
				if(p2[2]<-ladist) t2= t3;
			}
			else {
				ok2= 1;
				t2= t3;
			}
		}
		else if(ok1==0 || ok2==0) return;
		
		/* at least 1 visible interesction point */
		if(t1<0.0 && t2<0.0) return;
		
		if(t1<0.0) t1= 0.0;
		if(t2<0.0) t2= 0.0;
		
		if(t1==t2) return;
		
		/* sort again to be sure */
		if(t1>t2) {
			a= t1; t1= t2; t2= a;
		}
		
		/* calculate t0: is the maximum visible z (when halo is intersected by face) */ 
		if(doclip) {
			if(use_yco==0) t0= (maxz-npos[2])/nray[2];
			else t0= (maxy-npos[1])/nray[1];

			if(t0<t1) return;
			if(t0<t2) t2= t0;
		}

		/* calc points */
		p1[0]= npos[0] + t1*nray[0];
		p1[1]= npos[1] + t1*nray[1];
		p1[2]= npos[2] + t1*nray[2];
		p2[0]= npos[0] + t2*nray[0];
		p2[1]= npos[1] + t2*nray[1];
		p2[2]= npos[2] + t2*nray[2];
		
			
		/* now we have 2 points, make three lengths with it */
		
		a= sqrt(p1[0]*p1[0]+p1[1]*p1[1]+p1[2]*p1[2]);
		b= sqrt(p2[0]*p2[0]+p2[1]*p2[1]+p2[2]*p2[2]);
		c= VecLenf(p1, p2);
		
		a/= ladist;
		a= sqrt(a);
		b/= ladist; 
		b= sqrt(b);
		c/= ladist;
		
		*intens= c*( (1.0-a)+(1.0-b) );

		/* WATCH IT: do not clip a,b en c at 1.0, this gives nasty little overflows
			at the edges (especially with narrow halos) */
		if(*intens<=0.0) return;

		/* soft area */
		/* not needed because t0 has been used for p1/p2 as well */
		/* if(doclip && t0<t2) { */
		/* 	*intens *= (t0-t1)/(t2-t1); */
		/* } */
		
		*intens *= haint;
		
		if(lar->shb && lar->shb->shadhalostep) {
			*intens *= shadow_halo(lar, p1, p2);
		}
		
	}
}

static void renderspothalo(ShadeInput *shi, float *col, float alpha)
{
	GroupObject *go;
	LampRen *lar;
	float i;
	
	if(alpha==0.0f) return;

	for(go=R.lights.first; go; go= go->next) {
		lar= go->lampren;
		
		if(lar->type==LA_SPOT && (lar->mode & LA_HALO) && lar->haint>0) {
	
			spothalo(lar, shi, &i);
			if(i>0.0) {
				col[3]+= i*alpha;			// all premul
				col[0]+= i*lar->r*alpha;
				col[1]+= i*lar->g*alpha;
				col[2]+= i*lar->b*alpha;	
			}
		}
	}
	/* clip alpha, is needed for unified 'alpha threshold' (vanillaRenderPipe.c) */
	if(col[3]>1.0) col[3]= 1.0;
}



/* also used in zbuf.c */
int count_mask(unsigned short mask)
{
	if(R.samples)
		return (R.samples->cmask[mask & 255]+R.samples->cmask[mask>>8]);
	return 0;
}

static int calchalo_z(HaloRen *har, int zz)
{
	
	if(har->type & HA_ONLYSKY) {
		if(zz!=0x7FFFFFFF) zz= - 0x7FFFFF;
	}
	else {
		zz= (zz>>8);
	}
	return zz;
}

static void halo_pixelstruct(HaloRen *har, float *rb, float dist, float xn, float yn, PixStr *ps)
{
	float col[4], accol[4];
	int amount, amountm, zz, flarec;
	
	amount= 0;
	accol[0]=accol[1]=accol[2]=accol[3]= 0.0;
	flarec= har->flarec;
	
	while(ps) {
		amountm= count_mask(ps->mask);
		amount+= amountm;
		
		zz= calchalo_z(har, ps->z);
		if(zz> har->zs) {
			float fac;
			
			shadeHaloFloat(har, col, zz, dist, xn, yn, flarec);
			fac= ((float)amountm)/(float)R.osa;
			accol[0]+= fac*col[0];
			accol[1]+= fac*col[1];
			accol[2]+= fac*col[2];
			accol[3]+= fac*col[3];
			flarec= 0;
		}
		
		ps= ps->next;
	}
	/* now do the sky sub-pixels */
	amount= R.osa-amount;
	if(amount) {
		float fac;

		shadeHaloFloat(har, col, 0x7FFFFF, dist, xn, yn, flarec);
		fac= ((float)amount)/(float)R.osa;
		accol[0]+= fac*col[0];
		accol[1]+= fac*col[1];
		accol[2]+= fac*col[2];
		accol[3]+= fac*col[3];
	}
	col[0]= accol[0];
	col[1]= accol[1];
	col[2]= accol[2];
	col[3]= accol[3];
	
	addalphaAddfacFloat(rb, col, har->add);
	
}

static void halo_tile(RenderPart *pa, float *pass, unsigned int lay)
{
	HaloRen *har = NULL;
	rcti disprect= pa->disprect;
	float dist, xsq, ysq, xn, yn, *rb;
	float col[4];
	long *rd= NULL;
	int a, *rz, zz, y;
	short minx, maxx, miny, maxy, x;

	for(a=0; a<R.tothalo; a++) {
		if((a & 255)==0) {
			if(R.test_break() ) break; 
			har= R.bloha[a>>8];
		}
		else har++;

		/* layer test, clip halo with y */
		if((har->lay & lay)==0);
		else if(disprect.ymin > har->maxy);
		else if(disprect.ymax < har->miny);
		else {
			
			minx= floor(har->xs-har->rad);
			maxx= ceil(har->xs+har->rad);
			
			if(disprect.xmin > maxx);
			else if(disprect.xmax < minx);
			else {
				
				minx= MAX2(minx, disprect.xmin);
				maxx= MIN2(maxx, disprect.xmax);
			
				miny= MAX2(har->miny, disprect.ymin);
				maxy= MIN2(har->maxy, disprect.ymax);
			
				for(y=miny; y<maxy; y++) {
					int rectofs= (y-disprect.ymin)*pa->rectx + (minx - disprect.xmin);
					rb= pass + 4*rectofs;
					rz= pa->rectz + rectofs;
					
					if(pa->rectdaps)
						rd= pa->rectdaps + rectofs;
					
					yn= (y-har->ys)*R.ycor;
					ysq= yn*yn;
					
					for(x=minx; x<maxx; x++, rb+=4, rz++) {
						xn= x- har->xs;
						xsq= xn*xn;
						dist= xsq+ysq;
						if(dist<har->radsq) {
							if(rd && *rd) {
								halo_pixelstruct(har, rb, dist, xn, yn, (PixStr *)*rd);
							}
							else {
								zz= calchalo_z(har, *rz);
								if(zz> har->zs) {
									shadeHaloFloat(har, col, zz, dist, xn, yn, har->flarec);
									addalphaAddfacFloat(rb, col, har->add);
								}
							}
						}
						if(rd) rd++;
					}
				}
			}
		}
	}
}

/* ---------------- shaders ----------------------- */

static double Normalise_d(double *n)
{
	double d;
	
	d= n[0]*n[0]+n[1]*n[1]+n[2]*n[2];

	if(d>0.00000000000000001) {
		d= sqrt(d);

		n[0]/=d; 
		n[1]/=d; 
		n[2]/=d;
	} else {
		n[0]=n[1]=n[2]= 0.0;
		d= 0.0;
	}
	return d;
}

/* mix of 'real' fresnel and allowing control. grad defines blending gradient */
float fresnel_fac(float *view, float *vn, float grad, float fac)
{
	float t1, t2;
	
	if(fac==0.0) return 1.0;
	
	t1= (view[0]*vn[0] + view[1]*vn[1] + view[2]*vn[2]);
	if(t1>0.0)  t2= 1.0+t1;
	else t2= 1.0-t1;
	
	t2= grad + (1.0-grad)*pow(t2, fac);
	
	if(t2<0.0) return 0.0;
	else if(t2>1.0) return 1.0;
	return t2;
}

static double saacos_d(double fac)
{
	if(fac<= -1.0f) return M_PI;
	else if(fac>=1.0f) return 0.0;
	else return acos(fac);
}

/* Stoke's form factor. Need doubles here for extreme small area sizes */
static float area_lamp_energy(float *co, float *vn, LampRen *lar)
{
	double fac;
	double vec[4][3];	/* vectors of rendered co to vertices lamp */
	double cross[4][3];	/* cross products of this */
	double rad[4];		/* angles between vecs */

	VECSUB(vec[0], co, lar->area[0]);
	VECSUB(vec[1], co, lar->area[1]);
	VECSUB(vec[2], co, lar->area[2]);
	VECSUB(vec[3], co, lar->area[3]);
	
	Normalise_d(vec[0]);
	Normalise_d(vec[1]);
	Normalise_d(vec[2]);
	Normalise_d(vec[3]);

	/* cross product */
	CROSS(cross[0], vec[0], vec[1]);
	CROSS(cross[1], vec[1], vec[2]);
	CROSS(cross[2], vec[2], vec[3]);
	CROSS(cross[3], vec[3], vec[0]);

	Normalise_d(cross[0]);
	Normalise_d(cross[1]);
	Normalise_d(cross[2]);
	Normalise_d(cross[3]);

	/* angles */
	rad[0]= vec[0][0]*vec[1][0]+ vec[0][1]*vec[1][1]+ vec[0][2]*vec[1][2];
	rad[1]= vec[1][0]*vec[2][0]+ vec[1][1]*vec[2][1]+ vec[1][2]*vec[2][2];
	rad[2]= vec[2][0]*vec[3][0]+ vec[2][1]*vec[3][1]+ vec[2][2]*vec[3][2];
	rad[3]= vec[3][0]*vec[0][0]+ vec[3][1]*vec[0][1]+ vec[3][2]*vec[0][2];

	rad[0]= saacos_d(rad[0]);
	rad[1]= saacos_d(rad[1]);
	rad[2]= saacos_d(rad[2]);
	rad[3]= saacos_d(rad[3]);

	/* Stoke formula */
	fac=  rad[0]*(vn[0]*cross[0][0]+ vn[1]*cross[0][1]+ vn[2]*cross[0][2]);
	fac+= rad[1]*(vn[0]*cross[1][0]+ vn[1]*cross[1][1]+ vn[2]*cross[1][2]);
	fac+= rad[2]*(vn[0]*cross[2][0]+ vn[1]*cross[2][1]+ vn[2]*cross[2][2]);
	fac+= rad[3]*(vn[0]*cross[3][0]+ vn[1]*cross[3][1]+ vn[2]*cross[3][2]);

	if(fac<=0.0) return 0.0;
	return pow(fac*lar->areasize, lar->k);	// corrected for buttons size and lar->dist^2
}

static float spec(float inp, int hard)	
{
	float b1;
	
	if(inp>=1.0) return 1.0;
	else if (inp<=0.0) return 0.0;
	
	b1= inp*inp;
	/* avoid FPE */
	if(b1<0.01) b1= 0.01;	
	
	if((hard & 1)==0)  inp= 1.0;
	if(hard & 2)  inp*= b1;
	b1*= b1;
	if(hard & 4)  inp*= b1;
	b1*= b1;
	if(hard & 8)  inp*= b1;
	b1*= b1;
	if(hard & 16) inp*= b1;
	b1*= b1;

	/* avoid FPE */
	if(b1<0.001) b1= 0.0;	

	if(hard & 32) inp*= b1;
	b1*= b1;
	if(hard & 64) inp*=b1;
	b1*= b1;
	if(hard & 128) inp*=b1;

	if(b1<0.001) b1= 0.0;	

	if(hard & 256) {
		b1*= b1;
		inp*=b1;
	}

	return inp;
}

static float Phong_Spec( float *n, float *l, float *v, int hard, int tangent )
{
	float h[3];
	float rslt;
	
	h[0] = l[0] + v[0];
	h[1] = l[1] + v[1];
	h[2] = l[2] + v[2];
	Normalise(h);
	
	rslt = h[0]*n[0] + h[1]*n[1] + h[2]*n[2];
	if(tangent) rslt= sasqrt(1.0 - rslt*rslt);
		
	if( rslt > 0.0 ) rslt= spec(rslt, hard);
	else rslt = 0.0;
	
	return rslt;
}


/* reduced cook torrance spec (for off-specular peak) */
static float CookTorr_Spec(float *n, float *l, float *v, int hard, int tangent)
{
	float i, nh, nv, h[3];

	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	Normalise(h);

	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2];
	if(tangent) nh= sasqrt(1.0 - nh*nh);
	else if(nh<0.0) return 0.0;
	
	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2];
	if(tangent) nv= sasqrt(1.0 - nv*nv);
	else if(nv<0.0) nv= 0.0;

	i= spec(nh, hard);

	i= i/(0.1+nv);
	return i;
}

/* Blinn spec */
static float Blinn_Spec(float *n, float *l, float *v, float refrac, float spec_power, int tangent)
{
	float i, nh, nv, nl, vh, h[3];
	float a, b, c, g=0.0, p, f, ang;

	if(refrac < 1.0) return 0.0;
	if(spec_power == 0.0) return 0.0;
	
	/* conversion from 'hardness' (1-255) to 'spec_power' (50 maps at 0.1) */
	if(spec_power<100.0)
		spec_power= sqrt(1.0/spec_power);
	else spec_power= 10.0/spec_power;
	
	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	Normalise(h);

	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if(tangent) nh= sasqrt(1.0f - nh*nh);
	else if(nh<0.0) return 0.0;

	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if(tangent) nv= sasqrt(1.0f - nv*nv);
	if(nv<=0.0) nv= 0.01;				/* hrms... */

	nl= n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if(tangent) nl= sasqrt(1.0f - nl*nl);
	if(nl<=0.0) {
		return 0.0;
	}

	vh= v[0]*h[0]+v[1]*h[1]+v[2]*h[2]; /* Dot product between view vector and half-way vector */
	if(vh<=0.0) vh= 0.01;

	a = 1.0;
	b = (2.0*nh*nv)/vh;
	c = (2.0*nh*nl)/vh;

	if( a < b && a < c ) g = a;
	else if( b < a && b < c ) g = b;
	else if( c < a && c < b ) g = c;

	p = sqrt( (double)((refrac * refrac)+(vh*vh)-1.0) );
	f = (((p-vh)*(p-vh))/((p+vh)*(p+vh)))*(1+((((vh*(p+vh))-1.0)*((vh*(p+vh))-1.0))/(((vh*(p-vh))+1.0)*((vh*(p-vh))+1.0))));
	ang = saacos(nh);

	i= f * g * exp((double)(-(ang*ang) / (2.0*spec_power*spec_power)));
	if(i<0.0) i= 0.0;
	
	return i;
}

/* cartoon render spec */
static float Toon_Spec( float *n, float *l, float *v, float size, float smooth, int tangent)
{
	float h[3];
	float ang;
	float rslt;
	
	h[0] = l[0] + v[0];
	h[1] = l[1] + v[1];
	h[2] = l[2] + v[2];
	Normalise(h);
	
	rslt = h[0]*n[0] + h[1]*n[1] + h[2]*n[2];
	if(tangent) rslt = sasqrt(1.0f - rslt*rslt);
	
	ang = saacos( rslt ); 
	
	if( ang < size ) rslt = 1.0;
	else if( ang >= (size + smooth) || smooth == 0.0 ) rslt = 0.0;
	else rslt = 1.0 - ((ang - size) / smooth);
	
	return rslt;
}

/* Ward isotropic gaussian spec */
static float WardIso_Spec( float *n, float *l, float *v, float rms, int tangent)
{
	float i, nh, nv, nl, h[3], angle, alpha;


	/* half-way vector */
	h[0] = l[0] + v[0];
	h[1] = l[1] + v[1];
	h[2] = l[2] + v[2];
	Normalise(h);

	nh = n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if(tangent) nh = sasqrt(1.0f - nh*nh);
	if(nh<=0.0) nh = 0.001f;
	
	nv = n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if(tangent) nv = sasqrt(1.0f - nv*nv);
	if(nv<=0.0) nv = 0.001f;

	nl = n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if(tangent) nl = sasqrt(1.0f - nl*nl);
	if(nl<=0.0) nl = 0.001;

	angle = tan(saacos(nh));
	alpha = MAX2(rms,0.001);

	i= nl * (1.0/(4*M_PI*alpha*alpha)) * (exp( -(angle*angle)/(alpha*alpha))/(sqrt(nv*nl)));

	return i;
}

/* cartoon render diffuse */
static float Toon_Diff( float *n, float *l, float *v, float size, float smooth )
{
	float rslt, ang;

	rslt = n[0]*l[0] + n[1]*l[1] + n[2]*l[2];

	ang = saacos( (double)(rslt) );

	if( ang < size ) rslt = 1.0;
	else if( ang >= (size + smooth) || smooth == 0.0 ) rslt = 0.0;
	else rslt = 1.0 - ((ang - size) / smooth);

	return rslt;
}

/* Oren Nayar diffuse */

/* 'nl' is either dot product, or return value of area light */
/* in latter case, only last multiplication uses 'nl' */
static float OrenNayar_Diff(float nl, float *n, float *l, float *v, float rough )
{
	float i, nh, nv, vh, realnl, h[3];
	float a, b, t, A, B;
	float Lit_A, View_A, Lit_B[3], View_B[3];
	
	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	Normalise(h);
	
	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if(nh<0.0) nh = 0.0;
	
	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if(nv<=0.0) nv= 0.0;
	
	realnl= n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if(realnl<=0.0) return 0.0;
	if(nl<0.0) return 0.0;		/* value from area light */
	
	vh= v[0]*h[0]+v[1]*h[1]+v[2]*h[2]; /* Dot product between view vector and halfway vector */
	if(vh<=0.0) vh= 0.0;
	
	Lit_A = saacos(realnl);
	View_A = saacos( nv );
	
	Lit_B[0] = l[0] - (realnl * n[0]);
	Lit_B[1] = l[1] - (realnl * n[1]);
	Lit_B[2] = l[2] - (realnl * n[2]);
	Normalise( Lit_B );
	
	View_B[0] = v[0] - (nv * n[0]);
	View_B[1] = v[1] - (nv * n[1]);
	View_B[2] = v[2] - (nv * n[2]);
	Normalise( View_B );
	
	t = Lit_B[0]*View_B[0] + Lit_B[1]*View_B[1] + Lit_B[2]*View_B[2];
	if( t < 0 ) t = 0;
	
	if( Lit_A > View_A ) {
		a = Lit_A;
		b = View_A;
	}
	else {
		a = View_A;
		b = Lit_A;
	}
	
	A = 1 - (0.5 * ((rough * rough) / ((rough * rough) + 0.33)));
	B = 0.45 * ((rough * rough) / ((rough * rough) + 0.09));
	
	b*= 0.95;	/* prevent tangens from shooting to inf, 'nl' can be not a dot product here. */
				/* overflow only happens with extreme size area light, and higher roughness */
	i = nl * ( A + ( B * t * sin(a) * tan(b) ) );
	
	return i;
}

/* Minnaert diffuse */
static float Minnaert_Diff(float nl, float *n, float *v, float darkness)
{

	float i, nv;

	/* nl = dot product between surface normal and light vector */
	if (nl <= 0.0)
		return 0;

	/* nv = dot product between surface normal and view vector */
	nv = n[0]*v[0]+n[1]*v[1]+n[2]*v[2];
	if (nv < 0.0)
		nv = 0;

	if (darkness <= 1)
		i = nl * pow(MAX2(nv*nl, 0.1), (darkness - 1) ); /*The Real model*/
	else
		i = nl * pow( (1.001 - nv), (darkness  - 1) ); /*Nvidia model*/

	return i;
}

static float Fresnel_Diff(float *vn, float *lv, float *view, float fac_i, float fac)
{
	return fresnel_fac(lv, vn, fac_i, fac);
}

/* --------------------------------------------- */
/* also called from texture.c */
void calc_R_ref(ShadeInput *shi)
{
	float i;

	/* shi->vn dot shi->view */
	i= -2*(shi->vn[0]*shi->view[0]+shi->vn[1]*shi->view[1]+shi->vn[2]*shi->view[2]);

	shi->ref[0]= (shi->view[0]+i*shi->vn[0]);
	shi->ref[1]= (shi->view[1]+i*shi->vn[1]);
	shi->ref[2]= (shi->view[2]+i*shi->vn[2]);
	if(shi->osatex) {
		if(shi->vlr->flag & R_SMOOTH) {
			i= -2*( (shi->vn[0]+shi->dxno[0])*(shi->view[0]+shi->dxview) +
				(shi->vn[1]+shi->dxno[1])*shi->view[1]+ (shi->vn[2]+shi->dxno[2])*shi->view[2] );

			shi->dxref[0]= shi->ref[0]- ( shi->view[0]+shi->dxview+i*(shi->vn[0]+shi->dxno[0]));
			shi->dxref[1]= shi->ref[1]- (shi->view[1]+ i*(shi->vn[1]+shi->dxno[1]));
			shi->dxref[2]= shi->ref[2]- (shi->view[2]+ i*(shi->vn[2]+shi->dxno[2]));

			i= -2*( (shi->vn[0]+shi->dyno[0])*shi->view[0]+
				(shi->vn[1]+shi->dyno[1])*(shi->view[1]+shi->dyview)+ (shi->vn[2]+shi->dyno[2])*shi->view[2] );

			shi->dyref[0]= shi->ref[0]- (shi->view[0]+ i*(shi->vn[0]+shi->dyno[0]));
			shi->dyref[1]= shi->ref[1]- (shi->view[1]+shi->dyview+i*(shi->vn[1]+shi->dyno[1]));
			shi->dyref[2]= shi->ref[2]- (shi->view[2]+ i*(shi->vn[2]+shi->dyno[2]));

		}
		else {

			i= -2*( shi->vn[0]*(shi->view[0]+shi->dxview) +
				shi->vn[1]*shi->view[1]+ shi->vn[2]*shi->view[2] );

			shi->dxref[0]= shi->ref[0]- (shi->view[0]+shi->dxview+i*shi->vn[0]);
			shi->dxref[1]= shi->ref[1]- (shi->view[1]+ i*shi->vn[1]);
			shi->dxref[2]= shi->ref[2]- (shi->view[2]+ i*shi->vn[2]);

			i= -2*( shi->vn[0]*shi->view[0]+
				shi->vn[1]*(shi->view[1]+shi->dyview)+ shi->vn[2]*shi->view[2] );

			shi->dyref[0]= shi->ref[0]- (shi->view[0]+ i*shi->vn[0]);
			shi->dyref[1]= shi->ref[1]- (shi->view[1]+shi->dyview+i*shi->vn[1]);
			shi->dyref[2]= shi->ref[2]- (shi->view[2]+ i*shi->vn[2]);
		}
	}

}

/* called from ray.c */
void shade_color(ShadeInput *shi, ShadeResult *shr)
{
	Material *ma= shi->mat;

	if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
		shi->r= shi->vcol[0];
		shi->g= shi->vcol[1];
		shi->b= shi->vcol[2];
	}
	
	if(ma->texco) {
		if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
			shi->r= shi->vcol[0];
			shi->g= shi->vcol[1];
			shi->b= shi->vcol[2];
		}
		do_material_tex(shi);
	}

	if(ma->mode & (MA_ZTRA|MA_RAYTRANSP)) {
		if(ma->fresnel_tra!=0.0) 
			shi->alpha*= fresnel_fac(shi->view, shi->vn, ma->fresnel_tra_i, ma->fresnel_tra);
	}

	shr->diff[0]= shi->r;
	shr->diff[1]= shi->g;
	shr->diff[2]= shi->b;
	shr->alpha= shi->alpha;
}

/* ramp for at end of shade */
static void ramp_diffuse_result(float *diff, ShadeInput *shi)
{
	Material *ma= shi->mat;
	float col[4], fac=0;

	if(ma->ramp_col) {
		if(ma->rampin_col==MA_RAMP_IN_RESULT) {
			
			fac= 0.3*diff[0] + 0.58*diff[1] + 0.12*diff[2];
			do_colorband(ma->ramp_col, fac, col);
			
			/* blending method */
			fac= col[3]*ma->rampfac_col;
			
			ramp_blend(ma->rampblend_col, diff, diff+1, diff+2, fac, col);
		}
	}
}

/* r,g,b denote energy, ramp is used with different values to make new material color */
static void add_to_diffuse(float *diff, ShadeInput *shi, float is, float r, float g, float b)
{
	Material *ma= shi->mat;
	float col[4], colt[3], fac=0;
	
	if(ma->ramp_col && (ma->mode & MA_RAMP_COL)) {
		
		/* MA_RAMP_IN_RESULT is exceptional */
		if(ma->rampin_col==MA_RAMP_IN_RESULT) {
			// normal add
			diff[0] += r * shi->r;
			diff[1] += g * shi->g;
			diff[2] += b * shi->b;
		}
		else {
			/* input */
			switch(ma->rampin_col) {
			case MA_RAMP_IN_ENERGY:
				fac= 0.3*r + 0.58*g + 0.12*b;
				break;
			case MA_RAMP_IN_SHADER:
				fac= is;
				break;
			case MA_RAMP_IN_NOR:
				fac= shi->view[0]*shi->vn[0] + shi->view[1]*shi->vn[1] + shi->view[2]*shi->vn[2];
				break;
			}
	
			do_colorband(ma->ramp_col, fac, col);
			
			/* blending method */
			fac= col[3]*ma->rampfac_col;
			colt[0]= shi->r;
			colt[1]= shi->g;
			colt[2]= shi->b;

			ramp_blend(ma->rampblend_col, colt, colt+1, colt+2, fac, col);

			/* output to */
			diff[0] += r * colt[0];
			diff[1] += g * colt[1];
			diff[2] += b * colt[2];
		}
	}
	else {
		diff[0] += r * shi->r;
		diff[1] += g * shi->g;
		diff[2] += b * shi->b;
	}
}

static void ramp_spec_result(float *specr, float *specg, float *specb, ShadeInput *shi)
{
	Material *ma= shi->mat;
	float col[4];
	float fac;
	
	if(ma->ramp_spec && (ma->rampin_spec==MA_RAMP_IN_RESULT)) {
		fac= 0.3*(*specr) + 0.58*(*specg) + 0.12*(*specb);
		do_colorband(ma->ramp_spec, fac, col);
		
		/* blending method */
		fac= col[3]*ma->rampfac_spec;
		
		ramp_blend(ma->rampblend_spec, specr, specg, specb, fac, col);
		
	}
}

/* is = dot product shade, t = spec energy */
static void do_specular_ramp(ShadeInput *shi, float is, float t, float *spec)
{
	Material *ma= shi->mat;
	float col[4];
	float fac=0.0;
	
	spec[0]= shi->specr;
	spec[1]= shi->specg;
	spec[2]= shi->specb;

	/* MA_RAMP_IN_RESULT is exception */
	if(ma->ramp_spec && (ma->rampin_spec!=MA_RAMP_IN_RESULT)) {
		
		/* input */
		switch(ma->rampin_spec) {
		case MA_RAMP_IN_ENERGY:
			fac= t;
			break;
		case MA_RAMP_IN_SHADER:
			fac= is;
			break;
		case MA_RAMP_IN_NOR:
			fac= shi->view[0]*shi->vn[0] + shi->view[1]*shi->vn[1] + shi->view[2]*shi->vn[2];
			break;
		}
		
		do_colorband(ma->ramp_spec, fac, col);
		
		/* blending method */
		fac= col[3]*ma->rampfac_spec;
		
		ramp_blend(ma->rampblend_spec, spec, spec+1, spec+2, fac, col);
	}
}



static void ambient_occlusion(ShadeInput *shi, ShadeResult *shr)
{
	float f, shadfac[4];
	
	if((R.wrld.mode & WO_AMB_OCC) && (R.r.mode & R_RAYTRACE) && shi->amb!=0.0) {
		ray_ao(shi, shadfac);

		if(R.wrld.aocolor==WO_AOPLAIN) {
			if (R.wrld.aomix==WO_AOADDSUB) shadfac[3] = 2.0*shadfac[3]-1.0;
			else if (R.wrld.aomix==WO_AOSUB) shadfac[3] = shadfac[3]-1.0;

			f= R.wrld.aoenergy*shadfac[3]*shi->amb;
			shr->ao[0]+= f;
			shr->ao[1]+= f;
			shr->ao[2]+= f;
		}
		else {
			if (R.wrld.aomix==WO_AOADDSUB) {
				shadfac[0] = 2.0*shadfac[0]-1.0;
				shadfac[1] = 2.0*shadfac[1]-1.0;
				shadfac[2] = 2.0*shadfac[2]-1.0;
			}
			else if (R.wrld.aomix==WO_AOSUB) {
				shadfac[0] = shadfac[0]-1.0;
				shadfac[1] = shadfac[1]-1.0;
				shadfac[2] = shadfac[2]-1.0;
			}
			f= R.wrld.aoenergy*shi->amb;
			shr->ao[0]+= f*shadfac[0];
			shr->ao[1]+= f*shadfac[1];
			shr->ao[2]+= f*shadfac[2];
		}
	}
}

/* function returns diff, spec and optional shadow */
/* if passrender it returns shadow color, otherwise it applies it to diffuse and spec */
static void shade_one_light(LampRen *lar, ShadeInput *shi, ShadeResult *shr, int passrender)
{
	Material *ma= shi->mat;
	VlakRen *vlr= shi->vlr;
	float lv[3], lampdist, ld, lacol[3], shadfac[4];
	float i, is, inp, i_noshad, *vn, *view, vnor[3], phongcorr;
	
	vn= shi->vn;
	view= shi->view;
	
	/* lampdist calculation */
	if(lar->type==LA_SUN || lar->type==LA_HEMI) {
		VECCOPY(lv, lar->vec);
		lampdist= 1.0;
	}
	else {
		lv[0]= shi->co[0]-lar->co[0];
		lv[1]= shi->co[1]-lar->co[1];
		lv[2]= shi->co[2]-lar->co[2];
		ld= sqrt(lv[0]*lv[0]+lv[1]*lv[1]+lv[2]*lv[2]);
		lv[0]/= ld;
		lv[1]/= ld;
		lv[2]/= ld;
		
		/* ld is re-used further on (texco's) */
		if(lar->type==LA_AREA) {
			lampdist= 1.0;
		}
		else {
			if(lar->mode & LA_QUAD) {
				float t= 1.0;
				if(lar->ld1>0.0)
					t= lar->dist/(lar->dist+lar->ld1*ld);
				if(lar->ld2>0.0)
					t*= lar->distkw/(lar->distkw+lar->ld2*ld*ld);
				
				lampdist= t;
			}
			else {
				lampdist= (lar->dist/(lar->dist+ld));
			}
			
			if(lar->mode & LA_SPHERE) {
				float t= lar->dist - ld;
				if(t<0.0) return;
				
				t/= lar->dist;
				lampdist*= (t);
			}
		}
	}
	
	lacol[0]= lar->r;
	lacol[1]= lar->g;
	lacol[2]= lar->b;
	
	if(lar->type==LA_SPOT) {
		float t, inpr;
		
		if(lar->mode & LA_SQUARE) {
			if(lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2]>0.0) {
				float lvrot[3], x;
				
				/* rotate view to lampspace */
				VECCOPY(lvrot, lv);
				MTC_Mat3MulVecfl(lar->imat, lvrot);
				
				x= MAX2(fabs(lvrot[0]/lvrot[2]) , fabs(lvrot[1]/lvrot[2]));
				/* 1.0/(sqrt(1+x*x)) is equivalent to cos(atan(x)) */
				
				inpr= 1.0f/(sqrt(1.0f+x*x));
			}
			else inpr= 0.0;
		}
		else {
			inpr= lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2];
		}
		
		t= lar->spotsi;
		if(inpr<t) return;
		else {
			t= inpr-t;
			i= 1.0;
			if(t<lar->spotbl && lar->spotbl!=0.0) {
				/* soft area */
				i= t/lar->spotbl;
				t= i*i;
				inpr*= (3.0*t-2.0*t*i);
			}
			lampdist*=inpr;
		}
		
		if(lar->mode & LA_OSATEX) {
			shi->osatex= 1;	/* signal for multitex() */
			
			shi->dxlv[0]= lv[0] - (shi->co[0]-lar->co[0]+shi->dxco[0])/ld;
			shi->dxlv[1]= lv[1] - (shi->co[1]-lar->co[1]+shi->dxco[1])/ld;
			shi->dxlv[2]= lv[2] - (shi->co[2]-lar->co[2]+shi->dxco[2])/ld;
			
			shi->dylv[0]= lv[0] - (shi->co[0]-lar->co[0]+shi->dyco[0])/ld;
			shi->dylv[1]= lv[1] - (shi->co[1]-lar->co[1]+shi->dyco[1])/ld;
			shi->dylv[2]= lv[2] - (shi->co[2]-lar->co[2]+shi->dyco[2])/ld;
		}
		
	}
	
	if(lar->mode & LA_TEXTURE)  do_lamp_tex(lar, lv, shi, lacol);
	
	/* dot product and reflectivity */
	/* inp = dotproduct, is = shader result, i = lamp energy (with shadow) */
	
	/* tangent case; calculate fake face normal, aligned with lampvector */
	if(vlr->flag & R_TANGENT) {
		float cross[3];
		Crossf(cross, lv, vn);
		Crossf(vnor, cross, vn);
		vnor[0]= -vnor[0];vnor[1]= -vnor[1];vnor[2]= -vnor[2];
		vn= vnor;
	}
	else if(ma->mode & MA_TANGENT_V) {
		float cross[3];
		Crossf(cross, lv, shi->tang);
		Crossf(vnor, cross, shi->tang);
		vnor[0]= -vnor[0];vnor[1]= -vnor[1];vnor[2]= -vnor[2];
		vn= vnor;
	}
	
	inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];
	
	/* phong threshold to prevent backfacing faces having artefacts on ray shadow (terminator problem) */
	if((ma->mode & MA_RAYBIAS) && (lar->mode & LA_SHAD_RAY) && (vlr->flag & R_SMOOTH)) {
		float thresh= vlr->ob->smoothresh;
		if(inp>thresh)
			phongcorr= (inp-thresh)/(inp*(1.0-thresh));
		else
			phongcorr= 0.0;
	}
	else if(ma->sbias!=0.0f) {
		if(inp>ma->sbias)
			phongcorr= (inp-ma->sbias)/(inp*(1.0-ma->sbias));
		else
			phongcorr= 0.0;
	}
	else phongcorr= 1.0;
	
	/* diffuse shaders */
	if(lar->mode & LA_NO_DIFF) {
		is= 0.0;	// skip shaders
	}
	else if(lar->type==LA_HEMI) {
		is= 0.5*inp + 0.5;
	}
	else {
		
		if(lar->type==LA_AREA) {
			/* single sided */
			if(lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2]>0.0) {
				inp= area_lamp_energy(shi->co, vn, lar);
			}
			else inp= 0.0;
		}
		
		/* diffuse shaders (oren nayer gets inp from area light) */
		if(ma->diff_shader==MA_DIFF_ORENNAYAR) is= OrenNayar_Diff(inp, vn, lv, view, ma->roughness);
		else if(ma->diff_shader==MA_DIFF_TOON) is= Toon_Diff(vn, lv, view, ma->param[0], ma->param[1]);
		else if(ma->diff_shader==MA_DIFF_MINNAERT) is= Minnaert_Diff(inp, vn, view, ma->darkness);
		else if(ma->diff_shader==MA_DIFF_FRESNEL) is= Fresnel_Diff(vn, lv, view, ma->param[0], ma->param[1]);
		else is= inp;	// Lambert
	}
	
	i= is*phongcorr;
	
	if(i>0.0) {
		i*= lampdist*shi->refl;
	}
	i_noshad= i;
	
	vn= shi->vn;	// bring back original vector, we use special specular shaders for tangent
	if(ma->mode & MA_TANGENT_V)
		vn= shi->tang;
	
	/* init transp shadow */
	shadfac[0]= shadfac[1]= shadfac[2]= shadfac[3]= 1.0;
	
	/* shadow and spec, (lampdist==0 outside spot) */
	if(lampdist> 0.0) {
		
		if(i>0.0 && (R.r.mode & R_SHADOW)) {
			if(ma->mode & MA_SHADOW) {
				if(lar->type==LA_HEMI);	// no shadow
				else {
					if(lar->shb) {
						shadfac[3] = testshadowbuf(lar->shb, shi->co, shi->dxco, shi->dyco, inp);
					}
					else if(lar->mode & LA_SHAD_RAY) {
						ray_shadow(shi, lar, shadfac);
					}
					
					/* warning, here it skips the loop */
					if(lar->mode & LA_ONLYSHADOW) {
						
						shadfac[3]= i*lar->energy*(1.0-shadfac[3]);
						shr->diff[0] -= shadfac[3]*shi->r;
						shr->diff[1] -= shadfac[3]*shi->g;
						shr->diff[2] -= shadfac[3]*shi->b;
						return;
					}
					
					if(passrender==0)
						if(shadfac[3]==0.0) return;
					
					i*= shadfac[3];
				}
			}
		}
		
		/* in case 'no diffuse' we still do most calculus, spec can be in shadow */
		if(i>0.0 && !(lar->mode & LA_NO_DIFF)) {
			if(ma->mode & MA_SHADOW_TRA)
				add_to_diffuse(shr->diff, shi, is, i*shadfac[0]*lacol[0], i*shadfac[1]*lacol[1], i*shadfac[2]*lacol[2]);
			else
				add_to_diffuse(shr->diff, shi, is, i*lacol[0], i*lacol[1], i*lacol[2]);
		}
		if(passrender && i_noshad>0.0 && !(lar->mode & LA_NO_DIFF)) {
			/* while passrender we store shadowless diffuse in shr->shad, so we can subtract */
			if(ma->mode & MA_SHADOW_TRA)
				add_to_diffuse(shr->shad, shi, is, i_noshad*shadfac[0]*lacol[0], i_noshad*shadfac[1]*lacol[1], i_noshad*shadfac[2]*lacol[2]);
			else
				add_to_diffuse(shr->shad, shi, is, i_noshad*lacol[0], i_noshad*lacol[1], i_noshad*lacol[2]);
		}
		
		/* specularity */
		if(shadfac[3]>0.0 && shi->spec!=0.0 && !(lar->mode & LA_NO_SPEC)) {
			
			if(lar->type==LA_HEMI) {
				float t;
				/* hemi uses no spec shaders (yet) */
				
				lv[0]+= view[0];
				lv[1]+= view[1];
				lv[2]+= view[2];
				
				Normalise(lv);
				
				t= vn[0]*lv[0]+vn[1]*lv[1]+vn[2]*lv[2];
				
				if(lar->type==LA_HEMI) {
					t= 0.5*t+0.5;
				}
				
				t= shadfac[3]*shi->spec*spec(t, shi->har);
				
				shr->spec[0]+= t*(lacol[0] * shi->specr);
				shr->spec[1]+= t*(lacol[1] * shi->specg);
				shr->spec[2]+= t*(lacol[2] * shi->specb);
			}
			else {
				/* specular shaders */
				float specfac, t;
				
				if(ma->spec_shader==MA_SPEC_PHONG) 
					specfac= Phong_Spec(vn, lv, view, shi->har, (vlr->flag & R_TANGENT) || (ma->mode & MA_TANGENT_V));
				else if(ma->spec_shader==MA_SPEC_COOKTORR) 
					specfac= CookTorr_Spec(vn, lv, view, shi->har, (vlr->flag & R_TANGENT) || (ma->mode & MA_TANGENT_V));
				else if(ma->spec_shader==MA_SPEC_BLINN) 
					specfac= Blinn_Spec(vn, lv, view, ma->refrac, (float)shi->har, (vlr->flag & R_TANGENT) || (ma->mode & MA_TANGENT_V));
				else if(ma->spec_shader==MA_SPEC_WARDISO)
					specfac= WardIso_Spec( vn, lv, view, ma->rms, (vlr->flag & R_TANGENT) || (ma->mode & MA_TANGENT_V));
				else 
					specfac= Toon_Spec(vn, lv, view, ma->param[2], ma->param[3], (vlr->flag & R_TANGENT) || (ma->mode & MA_TANGENT_V));
				
				/* area lamp correction */
				if(lar->type==LA_AREA) specfac*= inp;
				
				t= shadfac[3]*shi->spec*lampdist*specfac;
				
				if(ma->mode & MA_RAMP_SPEC) {
					float spec[3];
					do_specular_ramp(shi, specfac, t, spec);
					shr->spec[0]+= t*(lacol[0] * spec[0]);
					shr->spec[1]+= t*(lacol[1] * spec[1]);
					shr->spec[2]+= t*(lacol[2] * spec[2]);
				}
				else {
					shr->spec[0]+= t*(lacol[0] * shi->specr);
					shr->spec[1]+= t*(lacol[1] * shi->specg);
					shr->spec[2]+= t*(lacol[2] * shi->specb);
				}
			}
		}
	}
	
}

#if 0
static void shade_lamp_loop_pass(ShadeInput *shi, ShadeResult *shr, int passflag)
{
	Material *ma= shi->mat;
	VlakRen *vlr= shi->vlr;
	
	memset(shr, 0, sizeof(ShadeResult));
	
	/* envmap hack, always reset */
	shi->refcol[0]= shi->refcol[1]= shi->refcol[2]= shi->refcol[3]= 0.0f;
	
	/* material color itself */
	if(passflag & (SCE_PASS_COMBINED|SCE_PASS_RGBA)) {
		if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
			shi->r= shi->vcol[0];
			shi->g= shi->vcol[1];
			shi->b= shi->vcol[2];
		}
		if(ma->texco)
			do_material_tex(shi);
		
		shr->col[0]= shi->r;
		shr->col[1]= shi->g;
		shr->col[2]= shi->b;
	}
	
	if(ma->mode & MA_SHLESS) {
		shr->diff[0]= shi->r;
		shr->diff[1]= shi->g;
		shr->diff[2]= shi->b;
		shr->alpha= shi->alpha;
		return;
	}

	if( (ma->mode & (MA_VERTEXCOL|MA_VERTEXCOLP))== MA_VERTEXCOL ) {	// vertexcolor light
		shr->diff[0]= shi->r*(shi->emit+shi->vcol[0]);
		shr->diff[1]= shi->g*(shi->emit+shi->vcol[1]);
		shr->diff[2]= shi->b*(shi->emit+shi->vcol[2]);
	}
	else {
		shr->diff[0]= shi->r*shi->emit;
		shr->diff[1]= shi->g*shi->emit;
		shr->diff[2]= shi->b*shi->emit;
	}
	
	/* AO pass */
	if(passflag & (SCE_PASS_COMBINED|SCE_PASS_AO)) {
		ambient_occlusion(shi, shr);
	}
		
	/* lighting pass */
	if(passflag & (SCE_PASS_COMBINED|SCE_PASS_DIFFUSE|SCE_PASS_SPEC|SCE_PASS_SHADOW)) {
		GroupObject *go;
		ListBase *lights;
		LampRen *lar;
		float diff[3];
		
		/* lights */
		if(ma->group)
			lights= &ma->group->gobject;
		else
			lights= &R.lights;
		
		for(go=lights->first; go; go= go->next) {
			lar= go->lampren;
			if(lar==NULL) continue;
			
			/* yafray: ignore shading by photonlights, not used in Blender */
			if (lar->type==LA_YF_PHOTON) continue;
			
			/* test for lamp layer */
			if(lar->mode & LA_LAYER) if((lar->lay & vlr->lay)==0) continue;
			if((lar->lay & shi->lay)==0) continue;

			/* accumulates in shr->diff and shr->spec and shr->shad */
			shade_one_light(lar, shi, shr, passflag);
		}
		
		/* calculate shadow */
		VECCOPY(diff, shr->shad);
		VECSUB(shr->shad, shr->shad, shr->diff);
		VECCOPY(shr->diff, diff);
	}
	
	/* alpha in end, spec can influence it */
	if(passflag & (SCE_PASS_COMBINED|SCE_PASS_RGBA)) {
		if(ma->mode & (MA_ZTRA|MA_RAYTRANSP)) {
			if(ma->fresnel_tra!=0.0) 
				shi->alpha*= fresnel_fac(shi->view, shi->vn, ma->fresnel_tra_i, ma->fresnel_tra);
			
			if(shi->spectra!=0.0) {
				float t = MAX3(shr->spec[0], shr->spec[1], shr->spec[2]);
				t *= shi->spectra;
				if(t>1.0) t= 1.0;
				shi->alpha= (1.0-t)*shi->alpha+t;
			}
		}
		shr->col[3]= shi->alpha;
	}
	shr->alpha= shi->alpha;
	
	shr->diff[0]+= shi->ambr + shi->r*shi->amb*shi->rad[0];
	shr->diff[1]+= shi->ambg + shi->g*shi->amb*shi->rad[1];
	shr->diff[2]+= shi->ambb + shi->b*shi->amb*shi->rad[2];
	
	if(ma->mode & MA_RAMP_COL) ramp_diffuse_result(shr->diff, shi);
	if(ma->mode & MA_RAMP_SPEC) ramp_spec_result(shr->spec, shr->spec+1, shr->spec+2, shi);
	
	/* refcol is for envmap only */
	if(shi->refcol[0]!=0.0) {
		shr->diff[0]= shi->mirr*shi->refcol[1] + (1.0 - shi->mirr*shi->refcol[0])*shr->diff[0];
		shr->diff[1]= shi->mirg*shi->refcol[2] + (1.0 - shi->mirg*shi->refcol[0])*shr->diff[1];
		shr->diff[2]= shi->mirb*shi->refcol[3] + (1.0 - shi->mirb*shi->refcol[0])*shr->diff[2];
	}
	
	if(passflag & SCE_PASS_COMBINED) {
		shr->combined[0]= shr->diff[0]+(shr->ao[0])*shr->col[0] + shr->spec[0];
		shr->combined[1]= shr->diff[1]+(shr->ao[1])*shr->col[1] + shr->spec[1];
		shr->combined[2]= shr->diff[2]+(shr->ao[2])*shr->col[2] + shr->spec[2];
		shr->combined[3]= shr->alpha;
	}
	
	if(R.r.mode & R_RAYTRACE) {
		
		if((ma->mode & MA_RAYMIRROR)==0) shi->ray_mirror= 0.0;
		
		if(shi->ray_mirror!=0.0 || ((shi->mat->mode & MA_RAYTRANSP) && shr->alpha!=1.0)) {
			float diff[3];
			
			VECCOPY(diff, shr->diff);
			
			ray_trace(shi, shr);
			
			VECSUB(shr->ray, shr->diff, diff);
			VECCOPY(shr->diff, diff);
			VECADD(shr->combined, shr->combined, shr->ray);
		}
	}
	else {
		/* doesnt look 'correct', but is better for preview, plus envmaps dont raytrace this */
		if(shi->mat->mode & MA_RAYTRANSP) shr->alpha= 1.0;
	}	
}
#endif

void shade_lamp_loop(ShadeInput *shi, ShadeResult *shr)
{
	LampRen *lar;
	GroupObject *go;
	Material *ma= shi->mat;
	VlakRen *vlr= shi->vlr;
	ListBase *lights;
	
	memset(shr, 0, sizeof(ShadeResult));
	
	if((ma->mode & MA_RAYMIRROR)==0) shi->ray_mirror= 0.0;
	
	/* lights */
	if(ma->group)
		lights= &ma->group->gobject;
	else
		lights= &R.lights;
	
	/* separate loop */
	if(ma->mode & MA_ONLYSHADOW) {
		float i, inp, inpr, lv[3];
		float *vn, *view, shadfac[4];
		float t, ir;
		
		vn= shi->vn;
		view= shi->view;
		
		if(R.r.mode & R_SHADOW) {
			
			shadfac[3]= ir= 0.0;
			for(go=lights->first; go; go= go->next) {
				lar= go->lampren;
				if(lar==NULL) continue;
				
				/* yafray: ignore shading by photonlights, not used in Blender */
				if (lar->type==LA_YF_PHOTON) continue;
				
				if(lar->mode & LA_LAYER) if((lar->lay & vlr->lay)==0) continue;
				if((lar->lay & shi->lay)==0) continue;
				
				lv[0]= shi->co[0]-lar->co[0];
				lv[1]= shi->co[1]-lar->co[1];
				lv[2]= shi->co[2]-lar->co[2];

				if(lar->type==LA_SPOT) {
					/* only test within spotbundel */
					if(lar->shb || (lar->mode & LA_SHAD_RAY)) {

						Normalise(lv);
						inpr= lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2];
						if(inpr>lar->spotsi) {
							
							inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];
							
							if(lar->shb) i = testshadowbuf(lar->shb, shi->co, shi->dxco, shi->dyco, inp);
							else {
								float shad[4];
								ray_shadow(shi, lar, shad);
								i= shad[3];
							}
							
							t= inpr - lar->spotsi;
							if(t<lar->spotbl && lar->spotbl!=0.0) {
								t/= lar->spotbl;
								t*= t;
								i= t*i+(1.0-t);
							}
							
							shadfac[3]+= i;
							ir+= 1.0;
						}
						else {
							shadfac[3]+= 1.0;
							ir+= 1.0;
						}
					}
				}
				else if(lar->mode & LA_SHAD_RAY) {
					float shad[4];
					
					/* single sided? */
					if( shi->facenor[0]*lv[0] + shi->facenor[1]*lv[1] + shi->facenor[2]*lv[2] > -0.01) {
						ray_shadow(shi, lar, shad);
						shadfac[3]+= shad[3];
						ir+= 1.0;
					}
				}

			}
			if(ir>0.0) {
				shadfac[3]/= ir;
				shr->alpha= (shi->alpha)*(1.0-shadfac[3]);
			}
		}
		
		if((R.wrld.mode & WO_AMB_OCC) && (R.r.mode & R_RAYTRACE) && shi->amb!=0.0) {
			float f;

			ray_ao(shi, shadfac);	// shadfac==0: full light
			shadfac[3]= 1.0-shadfac[3];
			
			f= R.wrld.aoenergy*shadfac[3]*shi->amb;
			
			if(R.wrld.aomix==WO_AOADD) {
				shr->alpha += f;
				shr->alpha *= f;
			}
			else if(R.wrld.aomix==WO_AOSUB) {
				shr->alpha += f;
			}
			else {
				shr->alpha *= f;
				shr->alpha += f;
			}
		}
		
		return;
	}
		
	if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
		shi->r= shi->vcol[0];
		shi->g= shi->vcol[1];
		shi->b= shi->vcol[2];
	}
	
	/* envmap hack, always reset */
	shi->refcol[0]= shi->refcol[1]= shi->refcol[2]= shi->refcol[3]= 0.0;

	if(ma->texco) {
		if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
			shi->r= shi->vcol[0];
			shi->g= shi->vcol[1];
			shi->b= shi->vcol[2];
		}
		do_material_tex(shi);
	}
	
	if(ma->mode & MA_SHLESS) {
		shr->diff[0]= shi->r;
		shr->diff[1]= shi->g;
		shr->diff[2]= shi->b;
		shr->alpha= shi->alpha;
		return;
	}

	if( (ma->mode & (MA_VERTEXCOL|MA_VERTEXCOLP))== MA_VERTEXCOL ) {	// vertexcolor light
		shr->diff[0]= shi->r*(shi->emit+shi->vcol[0]);
		shr->diff[1]= shi->g*(shi->emit+shi->vcol[1]);
		shr->diff[2]= shi->b*(shi->emit+shi->vcol[2]);
	}
	else {
		shr->diff[0]= shi->r*shi->emit;
		shr->diff[1]= shi->g*shi->emit;
		shr->diff[2]= shi->b*shi->emit;
	}
	
	if(R.wrld.mode & WO_AMB_OCC) {
		ambient_occlusion(shi, shr);
		VECADD(shr->diff, shr->diff, shr->ao);
	}
	
	for(go=lights->first; go; go= go->next) {
		lar= go->lampren;
		if(lar==NULL) continue;
		
		/* yafray: ignore shading by photonlights, not used in Blender */
		if (lar->type==LA_YF_PHOTON) continue;

		/* test for lamp layer */
		if(lar->mode & LA_LAYER) if((lar->lay & vlr->lay)==0) continue;
		if((lar->lay & shi->lay)==0) continue;
		
		/* accumulates in shr->diff and shr->spec, 0= no passrender */
		shade_one_light(lar, shi, shr, 0);
	}

	if(ma->mode & (MA_ZTRA|MA_RAYTRANSP)) {
		if(ma->fresnel_tra!=0.0) 
			shi->alpha*= fresnel_fac(shi->view, shi->vn, ma->fresnel_tra_i, ma->fresnel_tra);

		if(shi->spectra!=0.0) {
			float t = MAX3(shr->spec[0], shr->spec[1], shr->spec[2]);
			t *= shi->spectra;
			if(t>1.0) t= 1.0;
			shi->alpha= (1.0-t)*shi->alpha+t;
		}
	}

	shr->alpha= shi->alpha;

	shr->diff[0]+= shi->r*shi->amb*shi->rad[0];
	shr->diff[0]+= shi->ambr;
	
	shr->diff[1]+= shi->g*shi->amb*shi->rad[1];
	shr->diff[1]+= shi->ambg;
	
	shr->diff[2]+= shi->b*shi->amb*shi->rad[2];
	shr->diff[2]+= shi->ambb;
	
	if(ma->mode & MA_RAMP_COL) ramp_diffuse_result(shr->diff, shi);
	if(ma->mode & MA_RAMP_SPEC) ramp_spec_result(shr->spec, shr->spec+1, shr->spec+2, shi);
	
	/* refcol is for envmap only */
	if(shi->refcol[0]!=0.0) {
		shr->diff[0]= shi->mirr*shi->refcol[1] + (1.0 - shi->mirr*shi->refcol[0])*shr->diff[0];
		shr->diff[1]= shi->mirg*shi->refcol[2] + (1.0 - shi->mirg*shi->refcol[0])*shr->diff[1];
		shr->diff[2]= shi->mirb*shi->refcol[3] + (1.0 - shi->mirb*shi->refcol[0])*shr->diff[2];
	}

}

/* this function sets all coords for render (shared with raytracer) */
/* warning; exception for ortho render is here, can be done better! */
void shade_input_set_coords(ShadeInput *shi, float u, float v, int i1, int i2, int i3)
{
	VertRen *v1, *v2, *v3;
	VlakRen *vlr= shi->vlr;
	float l, dl;
	short texco= shi->mat->texco;
	int mode= shi->mat->mode_l;		/* or-ed result for all layers */
	char p1, p2, p3;
	
	/* for rendering of quads, the following values are used to denote vertices:
	   0 1 2	scanline tria & first half quad, and ray tria
	   0 2 3    scanline 2nd half quad
	   0 1 3    raytracer first half quad
	   2 1 3    raytracer 2nd half quad
	*/

	if(i1==0) {
		v1= vlr->v1;
		p1= ME_FLIPV1;
	} else {
		v1= vlr->v3;
		p1= ME_FLIPV3;
	}

	if(i2==1) {
		v2= vlr->v2;
		p2= ME_FLIPV2;
	} else {
		v2= vlr->v3;
		p2= ME_FLIPV3;
	}
	
	if(i3==2) {
		v3= vlr->v3;
		p3= ME_FLIPV3;
	} else {
		v3= vlr->v4;
		p3= ME_FLIPV4;
	}

	/* calculate U and V, for scanline (normal u and v are -1 to 0) */
	if(u==1.0) {
		if( (vlr->flag & R_SMOOTH) || (texco & NEED_UV) ) {
			/* exception case for wire render of edge */
			if(vlr->v2==vlr->v3) {
				float lend, lenc;
				
				lend= VecLenf(v2->co, v1->co);
				lenc= VecLenf(shi->co, v1->co);
				
				if(lend==0.0f) {
					u=v= 0.0f;
				}
				else {
					u= - (1.0f - lenc/lend);
					v= 0.0f;
				}
				
				if(shi->osatex) {
					shi->dxuv[0]=  0.0f;
					shi->dxuv[1]=  0.0f;
					shi->dyuv[0]=  0.0f;
					shi->dyuv[1]=  0.0f;
				}
			}
			else {
				float detsh, t00, t10, t01, t11;
				
				if(vlr->snproj==0) {
					t00= v3->co[0]-v1->co[0]; t01= v3->co[1]-v1->co[1];
					t10= v3->co[0]-v2->co[0]; t11= v3->co[1]-v2->co[1];
				}
				else if(vlr->snproj==1) {
					t00= v3->co[0]-v1->co[0]; t01= v3->co[2]-v1->co[2];
					t10= v3->co[0]-v2->co[0]; t11= v3->co[2]-v2->co[2];
				}
				else {
					t00= v3->co[1]-v1->co[1]; t01= v3->co[2]-v1->co[2];
					t10= v3->co[1]-v2->co[1]; t11= v3->co[2]-v2->co[2];
				}
				
				detsh= 1.0/(t00*t11-t10*t01);
				t00*= detsh; t01*=detsh; 
				t10*=detsh; t11*=detsh;
			
				if(vlr->snproj==0) {
					u= (shi->co[0]-v3->co[0])*t11-(shi->co[1]-v3->co[1])*t10;
					v= (shi->co[1]-v3->co[1])*t00-(shi->co[0]-v3->co[0])*t01;
					if(shi->osatex) {
						shi->dxuv[0]=  shi->dxco[0]*t11- shi->dxco[1]*t10;
						shi->dxuv[1]=  shi->dxco[1]*t00- shi->dxco[0]*t01;
						shi->dyuv[0]=  shi->dyco[0]*t11- shi->dyco[1]*t10;
						shi->dyuv[1]=  shi->dyco[1]*t00- shi->dyco[0]*t01;
					}
				}
				else if(vlr->snproj==1) {
					u= (shi->co[0]-v3->co[0])*t11-(shi->co[2]-v3->co[2])*t10;
					v= (shi->co[2]-v3->co[2])*t00-(shi->co[0]-v3->co[0])*t01;
					if(shi->osatex) {
						shi->dxuv[0]=  shi->dxco[0]*t11- shi->dxco[2]*t10;
						shi->dxuv[1]=  shi->dxco[2]*t00- shi->dxco[0]*t01;
						shi->dyuv[0]=  shi->dyco[0]*t11- shi->dyco[2]*t10;
						shi->dyuv[1]=  shi->dyco[2]*t00- shi->dyco[0]*t01;
					}
				}
				else {
					u= (shi->co[1]-v3->co[1])*t11-(shi->co[2]-v3->co[2])*t10;
					v= (shi->co[2]-v3->co[2])*t00-(shi->co[1]-v3->co[1])*t01;
					if(shi->osatex) {
						shi->dxuv[0]=  shi->dxco[1]*t11- shi->dxco[2]*t10;
						shi->dxuv[1]=  shi->dxco[2]*t00- shi->dxco[1]*t01;
						shi->dyuv[0]=  shi->dyco[1]*t11- shi->dyco[2]*t10;
						shi->dyuv[1]=  shi->dyco[2]*t00- shi->dyco[1]*t01;
					}
				}
			}
		}	
	}
	l= 1.0+u+v;
	
	/* calculate punos (vertexnormals) */
	if(vlr->flag & R_SMOOTH) { 
		float n1[3], n2[3], n3[3];
		
		if(shi->puno & p1) {
			n1[0]= -v1->n[0]; n1[1]= -v1->n[1]; n1[2]= -v1->n[2];
		} else {
			n1[0]= v1->n[0]; n1[1]= v1->n[1]; n1[2]= v1->n[2];
		}
		if(shi->puno & p2) {
			n2[0]= -v2->n[0]; n2[1]= -v2->n[1]; n2[2]= -v2->n[2];
		} else {
			n2[0]= v2->n[0]; n2[1]= v2->n[1]; n2[2]= v2->n[2];
		}
		
		if(shi->puno & p3) {
			n3[0]= -v3->n[0]; n3[1]= -v3->n[1]; n3[2]= -v3->n[2];
		} else {
			n3[0]= v3->n[0]; n3[1]= v3->n[1]; n3[2]= v3->n[2];
		}

		shi->vn[0]= l*n3[0]-u*n1[0]-v*n2[0];
		shi->vn[1]= l*n3[1]-u*n1[1]-v*n2[1];
		shi->vn[2]= l*n3[2]-u*n1[2]-v*n2[2];

		Normalise(shi->vn);

		if(shi->osatex && (texco & (TEXCO_NORM|TEXCO_REFL)) ) {
			dl= shi->dxuv[0]+shi->dxuv[1];
			shi->dxno[0]= dl*n3[0]-shi->dxuv[0]*n1[0]-shi->dxuv[1]*n2[0];
			shi->dxno[1]= dl*n3[1]-shi->dxuv[0]*n1[1]-shi->dxuv[1]*n2[1];
			shi->dxno[2]= dl*n3[2]-shi->dxuv[0]*n1[2]-shi->dxuv[1]*n2[2];
			dl= shi->dyuv[0]+shi->dyuv[1];
			shi->dyno[0]= dl*n3[0]-shi->dyuv[0]*n1[0]-shi->dyuv[1]*n2[0];
			shi->dyno[1]= dl*n3[1]-shi->dyuv[0]*n1[1]-shi->dyuv[1]*n2[1];
			shi->dyno[2]= dl*n3[2]-shi->dyuv[0]*n1[2]-shi->dyuv[1]*n2[2];

		}
		
		if(mode & MA_TANGENT_V) {
			float *s1, *s2, *s3;
			
			s1= RE_vertren_get_tangent(&R, v1, 0);
			s2= RE_vertren_get_tangent(&R, v2, 0);
			s3= RE_vertren_get_tangent(&R, v3, 0);
			if(s1 && s2 && s3) {
				shi->tang[0]= (l*s3[0] - u*s1[0] - v*s2[0]);
				shi->tang[1]= (l*s3[1] - u*s1[1] - v*s2[1]);
				shi->tang[2]= (l*s3[2] - u*s1[2] - v*s2[2]);
			}
			else shi->tang[0]= shi->tang[1]= shi->tang[2]= 0.0f;
		}		
	}
	else {
		VECCOPY(shi->vn, shi->facenor);
		if(mode & MA_TANGENT_V) 
			shi->tang[0]= shi->tang[1]= shi->tang[2]= 0.0f;
	}
	
	if(R.r.mode & R_SPEED) {
		float *s1, *s2, *s3;
		
		s1= RE_vertren_get_winspeed(&R, v1, 0);
		s2= RE_vertren_get_winspeed(&R, v2, 0);
		s3= RE_vertren_get_winspeed(&R, v3, 0);
		if(s1 && s2 && s3) {
			shi->winspeed[0]= (l*s3[0] - u*s1[0] - v*s2[0]);
			shi->winspeed[1]= (l*s3[1] - u*s1[1] - v*s2[1]);
			shi->winspeed[2]= (l*s3[2] - u*s1[2] - v*s2[2]);
			shi->winspeed[3]= (l*s3[3] - u*s1[3] - v*s2[3]);
		}
	}
	
	/* texture coordinates. shi->dxuv shi->dyuv have been set */
	if(texco & NEED_UV) {
		if(texco & TEXCO_ORCO) {
			if(v1->orco) {
				float *o1, *o2, *o3;
				
				o1= v1->orco;
				o2= v2->orco;
				o3= v3->orco;
				
				shi->lo[0]= l*o3[0]-u*o1[0]-v*o2[0];
				shi->lo[1]= l*o3[1]-u*o1[1]-v*o2[1];
				shi->lo[2]= l*o3[2]-u*o1[2]-v*o2[2];
	
				if(shi->osatex) {
					dl= shi->dxuv[0]+shi->dxuv[1];
					shi->dxlo[0]= dl*o3[0]-shi->dxuv[0]*o1[0]-shi->dxuv[1]*o2[0];
					shi->dxlo[1]= dl*o3[1]-shi->dxuv[0]*o1[1]-shi->dxuv[1]*o2[1];
					shi->dxlo[2]= dl*o3[2]-shi->dxuv[0]*o1[2]-shi->dxuv[1]*o2[2];
					dl= shi->dyuv[0]+shi->dyuv[1];
					shi->dylo[0]= dl*o3[0]-shi->dyuv[0]*o1[0]-shi->dyuv[1]*o2[0];
					shi->dylo[1]= dl*o3[1]-shi->dyuv[0]*o1[1]-shi->dyuv[1]*o2[1];
					shi->dylo[2]= dl*o3[2]-shi->dyuv[0]*o1[2]-shi->dyuv[1]*o2[2];
				}
			}
		}
		
		if(texco & TEXCO_GLOB) {
			VECCOPY(shi->gl, shi->co);
			MTC_Mat4MulVecfl(R.viewinv, shi->gl);
			if(shi->osatex) {
				VECCOPY(shi->dxgl, shi->dxco);
				MTC_Mat3MulVecfl(R.imat, shi->dxco);
				VECCOPY(shi->dygl, shi->dyco);
				MTC_Mat3MulVecfl(R.imat, shi->dyco);
			}
		}
		if(texco & TEXCO_STRAND) {
			shi->strand= (l*v3->accum - u*v1->accum - v*v2->accum);
			if(shi->osatex) {
				dl= shi->dxuv[0]+shi->dxuv[1];
				shi->dxstrand= dl*v3->accum-shi->dxuv[0]*v1->accum-shi->dxuv[1]*v2->accum;
				dl= shi->dyuv[0]+shi->dyuv[1];
				shi->dystrand= dl*v3->accum-shi->dyuv[0]*v1->accum-shi->dyuv[1]*v2->accum;
			}
		}
		if((texco & TEXCO_UV) || (mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE)))  {
			int j1=i1, j2=i2, j3=i3;
			
			/* to prevent storing new tfaces or vcols, we check a split runtime */
			/* 		4---3		4---3 */
			/*		|\ 1|	or  |1 /| */
			/*		|0\ |		|/ 0| */
			/*		1---2		1---2 	0 = orig face, 1 = new face */
			
			/* Update vert nums to point to correct verts of original face */
			if(vlr->flag & R_DIVIDE_24) {  
				if(vlr->flag & R_FACE_SPLIT) {
					j1++; j2++; j3++;
				}
				else {
					j3++;
				}
			}
			else if(vlr->flag & R_FACE_SPLIT) {
				j2++; j3++; 
			}
			
			if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP)) {
				
				if(vlr->vcol) {
					char *cp1, *cp2, *cp3;
					
					cp1= (char *)(vlr->vcol+j1);
					cp2= (char *)(vlr->vcol+j2);
					cp3= (char *)(vlr->vcol+j3);

					shi->vcol[0]= (l*((float)cp3[3]) - u*((float)cp1[3]) - v*((float)cp2[3]))/255.0;
					shi->vcol[1]= (l*((float)cp3[2]) - u*((float)cp1[2]) - v*((float)cp2[2]))/255.0;
					shi->vcol[2]= (l*((float)cp3[1]) - u*((float)cp1[1]) - v*((float)cp2[1]))/255.0;
				}
				else {
					shi->vcol[0]= 0.0;
					shi->vcol[1]= 0.0;
					shi->vcol[2]= 0.0;
				}
			}
			if(vlr->tface) {
				float *uv1, *uv2, *uv3;
				
				uv1= vlr->tface->uv[j1];
				uv2= vlr->tface->uv[j2];
				uv3= vlr->tface->uv[j3];
				
				shi->uv[0]= -1.0 + 2.0*(l*uv3[0]-u*uv1[0]-v*uv2[0]);
				shi->uv[1]= -1.0 + 2.0*(l*uv3[1]-u*uv1[1]-v*uv2[1]);
				shi->uv[2]= 0.0;	// texture.c assumes there are 3 coords
				
				if(shi->osatex) {
					float duv[2];
					
					dl= shi->dxuv[0]+shi->dxuv[1];
					duv[0]= shi->dxuv[0]; 
					duv[1]= shi->dxuv[1];
					
					shi->dxuv[0]= 2.0*(dl*uv3[0]-duv[0]*uv1[0]-duv[1]*uv2[0]);
					shi->dxuv[1]= 2.0*(dl*uv3[1]-duv[0]*uv1[1]-duv[1]*uv2[1]);
	
					dl= shi->dyuv[0]+shi->dyuv[1];
					duv[0]= shi->dyuv[0]; 
					duv[1]= shi->dyuv[1];
	
					shi->dyuv[0]= 2.0*(dl*uv3[0]-duv[0]*uv1[0]-duv[1]*uv2[0]);
					shi->dyuv[1]= 2.0*(dl*uv3[1]-duv[0]*uv1[1]-duv[1]*uv2[1]);
				}
				if(mode & MA_FACETEXTURE) {
					if((mode & (MA_VERTEXCOL|MA_VERTEXCOLP))==0) {
						shi->vcol[0]= 1.0;
						shi->vcol[1]= 1.0;
						shi->vcol[2]= 1.0;
					}
					if(vlr->tface) render_realtime_texture(shi);
				}
			}
			else {
				shi->uv[0]= 2.0*(u+.5);
				shi->uv[1]= 2.0*(v+.5);
				shi->uv[2]= 0.0;	// texture.c assumes there are 3 coords
				if(mode & MA_FACETEXTURE) {
					/* no tface? set at 1.0 */
					shi->vcol[0]= 1.0;
					shi->vcol[1]= 1.0;
					shi->vcol[2]= 1.0;
				}
			}
		}
		if(texco & TEXCO_NORM) {
			shi->orn[0]= -shi->vn[0];
			shi->orn[1]= -shi->vn[1];
			shi->orn[2]= -shi->vn[2];
		}
		if(mode & MA_RADIO) {
			float *r1, *r2, *r3;
			
			r1= RE_vertren_get_rad(&R, v1, 0);
			r2= RE_vertren_get_rad(&R, v2, 0);
			r3= RE_vertren_get_rad(&R, v3, 0);
			
			if(r1 && r2 && r3) {
				shi->rad[0]= (l*r3[0] - u*r1[0] - v*r2[0]);
				shi->rad[1]= (l*r3[1] - u*r1[1] - v*r2[1]);
				shi->rad[2]= (l*r3[2] - u*r1[2] - v*r2[2]);
			}
			else {
				shi->rad[0]= shi->rad[1]= shi->rad[2]= 0.0;
			}
		}
		else {
			shi->rad[0]= shi->rad[1]= shi->rad[2]= 0.0;
		}
		if(texco & TEXCO_REFL) {
			/* mirror reflection colour textures (and envmap) */
			calc_R_ref(shi);	/* wrong location for normal maps! XXXXXXXXXXXXXX */
		}
		if(texco & TEXCO_STRESS) {
			float *s1, *s2, *s3;
			
			s1= RE_vertren_get_stress(&R, v1, 0);
			s2= RE_vertren_get_stress(&R, v2, 0);
			s3= RE_vertren_get_stress(&R, v3, 0);
			if(s1 && s2 && s3) {
				shi->stress= l*s3[0] - u*s1[0] - v*s2[0];
				if(shi->stress<1.0f) shi->stress-= 1.0f;
				else shi->stress= (shi->stress-1.0f)/shi->stress;
			}
			else shi->stress= 0.0f;
		}
		if(texco & TEXCO_TANGENT) {
			if((mode & MA_TANGENT_V)==0) {
				/* just prevent surprises */
				shi->tang[0]= shi->tang[1]= shi->tang[2]= 0.0f;
			}
		}
	}
	else {
		shi->rad[0]= shi->rad[1]= shi->rad[2]= 0.0;
	}
}

#if 0
/* return labda for view vector being closest to line v3-v4 */
/* was used for wire render */
static float isec_view_line(float *view, float *v3, float *v4)
{
	float vec[3];
	float dot0, dot1, dot2, veclen, viewlen;
	float fac, div;
	
	vec[0]= v4[0] - v3[0];
	vec[1]= v4[1] - v3[1];
	vec[2]= v4[2] - v3[2];
	
	dot0 = v3[0]*vec[0] + v3[1]*vec[1] + v3[2]*vec[2];
	dot1 = vec[0]*view[0] + vec[1]*view[1] + vec[2]*view[2];
	dot2 = v3[0]*view[0] + v3[1]*view[1] + v3[2]*view[2];
	
	veclen = vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2];
	viewlen = view[0]*view[0] + view[1]*view[1] + view[2]*view[2];
	
	div = viewlen*veclen - dot1*dot1;
	if (div==0.0) return 0.0;
	
	fac = dot2*veclen - dot0*dot1;
	return fac/div;
}
#endif


/* also used as callback for nodes */
void shade_material_loop(ShadeInput *shi, ShadeResult *shr)
{
	
	shade_lamp_loop(shi, shr);	/* clears shr */
	
	if(shi->translucency!=0.0) {
		ShadeResult shr_t;
		
		VECCOPY(shi->vn, shi->vno);
		VecMulf(shi->vn, -1.0);
		VecMulf(shi->facenor, -1.0);
		shade_lamp_loop(shi, &shr_t);
		
		shr->diff[0]+= shi->translucency*shr_t.diff[0];
		shr->diff[1]+= shi->translucency*shr_t.diff[1];
		shr->diff[2]+= shi->translucency*shr_t.diff[2];
		VecMulf(shi->vn, -1.0);
		VecMulf(shi->facenor, -1.0);
	}
	
	if(R.r.mode & R_RAYTRACE) {
		if(shi->ray_mirror!=0.0 || ((shi->mat->mode & MA_RAYTRANSP) && shr->alpha!=1.0)) {
			ray_trace(shi, shr);
		}
	}
	else {
		/* doesnt look 'correct', but is better for preview, plus envmaps dont raytrace this */
		if(shi->mat->mode & MA_RAYTRANSP) shr->alpha= 1.0;
	}
	
}

/* x,y: window coordinate from 0 to rectx,y */
/* return pointer to rendered face */
/* note, facenr declared volatile due to over-eager -O2 optimizations
 * on cygwin (particularly -frerun-cse-after-loop)
 */
void *shadepixel(ShadePixelInfo *shpi, float x, float y, int z, volatile int facenr, int mask, float *rco)
{
	ShadeResult *shr= &shpi->shr;
	ShadeInput shi;
	VlakRen *vlr=NULL;
	
	if(facenr< 0) {	/* error */
		return NULL;
	}
	/* currently in use for dithering (soft shadow) node preview */
	shi.xs= (int)(x+0.5f);
	shi.ys= (int)(y+0.5f);

	shi.thread= shpi->thread;
	shi.do_preview= R.r.scemode & R_NODE_PREVIEW;
	shi.lay= shpi->lay;

	/* mask is used to indicate amount of samples (ray shad/mir and AO) */
	shi.mask= mask;
	shi.depth= 0;	// means first hit, not raytracing
	
	if(facenr==0) {	/* sky */
		memset(shr, 0, sizeof(ShadeResult));
		rco[0]= rco[1]= rco[2]= 0.0f;
	}
	else if( (facenr & 0x7FFFFF) <= R.totvlak) {
		VertRen *v1;
		float alpha, fac, zcor;
		
		vlr= RE_findOrAddVlak(&R, (facenr-1) & 0x7FFFFF);
		
		shi.vlr= vlr;
		shi.mat= vlr->mat;
		
		shi.osatex= (shi.mat->texco & TEXCO_OSA);
		
		/* copy the face normal (needed because it gets flipped for tracing */
		VECCOPY(shi.facenor, vlr->n);
		shi.puno= vlr->puno;
		
		v1= vlr->v1;
		
		/* COXYZ AND VIEW VECTOR  */
		calc_view_vector(shi.view, x, y);	/* returns not normalized, so is in viewplane coords */

		/* wire cannot use normal for calculating shi.co */
		if(shi.mat->mode & MA_WIRE) {
			float zco;
			/* inverse of zbuf calc: zbuf = MAXZ*hoco_z/hoco_w */
			
			zco= ((float)z)/2147483647.0f;
			shi.co[2]= R.winmat[3][2]/( R.winmat[2][3]*zco - R.winmat[2][2] );
			
			fac= zcor= shi.co[2]/shi.view[2];
			
			shi.co[0]= fac*shi.view[0];
			shi.co[1]= fac*shi.view[1];
		}
		else {
			float dface;
			
			dface= v1->co[0]*shi.facenor[0]+v1->co[1]*shi.facenor[1]+v1->co[2]*shi.facenor[2];
			
			/* ortho viewplane cannot intersect using view vector originating in (0,0,0) */
			if(R.r.mode & R_ORTHO) {
				/* x and y 3d coordinate can be derived from pixel coord and winmat */
				float fx= 2.0/(R.winx*R.winmat[0][0]);
				float fy= 2.0/(R.winy*R.winmat[1][1]);
				
				shi.co[0]= (0.5 + x - 0.5*R.winx)*fx - R.winmat[3][0]/R.winmat[0][0];
				shi.co[1]= (0.5 + y - 0.5*R.winy)*fy - R.winmat[3][1]/R.winmat[1][1];
				
				/* using a*x + b*y + c*z = d equation, (a b c) is normal */
				if(shi.facenor[2]!=0.0f)
					shi.co[2]= (dface - shi.facenor[0]*shi.co[0] - shi.facenor[1]*shi.co[1])/shi.facenor[2];
				else
					shi.co[2]= 0.0f;
				
				zcor= 1.0; // only to prevent not-initialize
				
				if(shi.osatex || (R.r.mode & R_SHADOW) ) {
					shi.dxco[0]= fx;
					shi.dxco[1]= 0.0;
					if(shi.facenor[2]!=0.0f)
						shi.dxco[2]= (shi.facenor[0]*fx)/shi.facenor[2];
					else 
						shi.dxco[2]= 0.0f;
					
					shi.dyco[0]= 0.0;
					shi.dyco[1]= fy;
					if(shi.facenor[2]!=0.0f)
						shi.dyco[2]= (shi.facenor[1]*fy)/shi.facenor[2];
					else 
						shi.dyco[2]= 0.0f;
				}
			}
			else {
				float div;
				
				div= shi.facenor[0]*shi.view[0] + shi.facenor[1]*shi.view[1] + shi.facenor[2]*shi.view[2];
				if (div!=0.0) fac= zcor= dface/div;
				else fac= zcor= 0.0;
				
				shi.co[0]= fac*shi.view[0];
				shi.co[1]= fac*shi.view[1];
				shi.co[2]= fac*shi.view[2];
			
				/* pixel dx/dy for render coord */
				if(shi.osatex || (R.r.mode & R_SHADOW) ) {
					float u= dface/(div - R.viewdx*shi.facenor[0]);
					float v= dface/(div - R.viewdy*shi.facenor[1]);

					shi.dxco[0]= shi.co[0]- (shi.view[0]-R.viewdx)*u;
					shi.dxco[1]= shi.co[1]- (shi.view[1])*u;
					shi.dxco[2]= shi.co[2]- (shi.view[2])*u;

					shi.dyco[0]= shi.co[0]- (shi.view[0])*v;
					shi.dyco[1]= shi.co[1]- (shi.view[1]-R.viewdy)*v;
					shi.dyco[2]= shi.co[2]- (shi.view[2])*v;

				}
			}
		}
		/* rco might be used for sky texture */
		VECCOPY(rco, shi.co);
		
		/* cannot normalise earlier, code above needs it at viewplane level */
		fac= Normalise(shi.view);
		zcor*= fac;	// for mist, distance of point from camera
		
		if(shi.osatex) {
			if( (shi.mat->texco & TEXCO_REFL) ) {
				shi.dxview= -R.viewdx/fac;
				shi.dyview= -R.viewdy/fac;
			}
		}
		
		/* calcuate normals, texture coords, vertex colors, etc */
		if(facenr & 0x800000)
			shade_input_set_coords(&shi, 1.0, 1.0, 0, 2, 3);
		else 
			shade_input_set_coords(&shi, 1.0, 1.0, 0, 1, 2);

		/* this only avalailable for scanline */
		if(shi.mat->texco & TEXCO_WINDOW) {
			shi.winco[0]= -1.0f + 2.0f*x/(float)R.winx;
			shi.winco[1]= -1.0f + 2.0f*y/(float)R.winy;
			shi.winco[2]= 0.0;
			if(shi.osatex) {
				shi.dxwin[0]= 2.0/(float)R.winx;
				shi.dywin[1]= 2.0/(float)R.winy;
				shi.dxwin[1]= shi.dxwin[2]= 0.0;
				shi.dywin[0]= shi.dywin[2]= 0.0;
			}
		}
		/* after this the u and v AND shi.dxuv and shi.dyuv are incorrect */
		if(shi.mat->texco & TEXCO_STICKY) {
			VertRen *v2, *v3;
			float *s1, *s2, *s3;
			
			if(facenr & 0x800000) {
				v2= vlr->v3; v3= vlr->v4;
			} else {
				v2= vlr->v2; v3= vlr->v3;
			}
			
			s1= RE_vertren_get_sticky(&R, v1, 0);
			s2= RE_vertren_get_sticky(&R, v2, 0);
			s3= RE_vertren_get_sticky(&R, v3, 0);
			
			if(s1 && s2 && s3) {
				float Zmulx, Zmuly;
				float hox, hoy, l, dl, u, v;
				float s00, s01, s10, s11, detsh;
				
				/* XXXX */
				Zmulx= R.winx; Zmuly= R.winy;
				
				s00= v3->ho[0]/v3->ho[3] - v1->ho[0]/v1->ho[3];
				s01= v3->ho[1]/v3->ho[3] - v1->ho[1]/v1->ho[3];
				s10= v3->ho[0]/v3->ho[3] - v2->ho[0]/v2->ho[3];
				s11= v3->ho[1]/v3->ho[3] - v2->ho[1]/v2->ho[3];
				
				detsh= s00*s11-s10*s01;
				s00/= detsh; s01/=detsh; 
				s10/=detsh; s11/=detsh;
	
				/* recalc u and v again */
				hox= x/Zmulx -1.0;
				hoy= y/Zmuly -1.0;
				u= (hox - v3->ho[0]/v3->ho[3])*s11 - (hoy - v3->ho[1]/v3->ho[3])*s10;
				v= (hoy - v3->ho[1]/v3->ho[3])*s00 - (hox - v3->ho[0]/v3->ho[3])*s01;
				l= 1.0+u+v;
				
				shi.sticky[0]= l*s3[0]-u*s1[0]-v*s2[0];
				shi.sticky[1]= l*s3[1]-u*s1[1]-v*s2[1];
				shi.sticky[2]= 0.0;
				
				if(shi.osatex) {
					shi.dxuv[0]=  s11/Zmulx;
					shi.dxuv[1]=  - s01/Zmulx;
					shi.dyuv[0]=  - s10/Zmuly;
					shi.dyuv[1]=  s00/Zmuly;
					
					dl= shi.dxuv[0]+shi.dxuv[1];
					shi.dxsticky[0]= dl*s3[0]-shi.dxuv[0]*s1[0]-shi.dxuv[1]*s2[0];
					shi.dxsticky[1]= dl*s3[1]-shi.dxuv[0]*s1[1]-shi.dxuv[1]*s2[1];
					dl= shi.dyuv[0]+shi.dyuv[1];
					shi.dysticky[0]= dl*s3[0]-shi.dyuv[0]*s1[0]-shi.dyuv[1]*s2[0];
					shi.dysticky[1]= dl*s3[1]-shi.dyuv[0]*s1[1]-shi.dyuv[1]*s2[1];
				}
			}
		}
		
		/* ------  main shading loop -------- */
		VECCOPY(shi.vno, shi.vn);
		
		if(shi.mat->nodetree && shi.mat->use_nodes) {
			ntreeShaderExecTree(shi.mat->nodetree, &shi, shr);
		}
		else {
			/* copy all relevant material vars, note, keep this synced with render_types.h */
			memcpy(&shi.r, &shi.mat->r, 23*sizeof(float));
			shi.har= shi.mat->har;
			
//			if(passflag)
//				shade_lamp_loop_pass(&shi, shr, passflag);
//			else
				shade_material_loop(&shi, shr);
		}
		
		/* after shading and composit layers */
		if(shr->spec[0]<0.0f) shr->spec[0]= 0.0f;
		if(shr->spec[1]<0.0f) shr->spec[1]= 0.0f;
		if(shr->spec[2]<0.0f) shr->spec[2]= 0.0f;
		
		if(shr->diff[0]<0.0f) shr->diff[0]= 0.0f;
		if(shr->diff[1]<0.0f) shr->diff[1]= 0.0f;
		if(shr->diff[2]<0.0f) shr->diff[2]= 0.0f;
		
//		if(passflag==0) {
			VECADD(shr->combined, shr->diff, shr->spec);
//		}
		
		/* additional passes */
		QUATCOPY(shr->winspeed, shi.winspeed);
		VECCOPY(shr->nor, shi.vn);
		
		/* NOTE: this is not correct here, sky from raytrace gets corrected... */
		/* exposure correction */
		if(R.wrld.exp!=0.0 || R.wrld.range!=1.0) {
			if((shi.mat->mode & MA_SHLESS)==0) {
				shr->combined[0]= R.wrld.linfac*(1.0-exp( shr->combined[0]*R.wrld.logfac) );
				shr->combined[1]= R.wrld.linfac*(1.0-exp( shr->combined[1]*R.wrld.logfac) );
				shr->combined[2]= R.wrld.linfac*(1.0-exp( shr->combined[2]*R.wrld.logfac) );
			}
		}
		
		/* MIST */
		if((R.wrld.mode & WO_MIST) && (shi.mat->mode & MA_NOMIST)==0 ) {
			if(R.r.mode & R_ORTHO)
				alpha= mistfactor(-shi.co[2], shi.co);
			else
				alpha= mistfactor(zcor, shi.co);
		}
		else alpha= 1.0;

		if(shr->alpha!=1.0 || alpha!=1.0) {
			if(shi.mat->mode & MA_RAYTRANSP) {
				fac= alpha;	
				shr->combined[3]= shr->alpha;
			}
			else {
				fac= alpha*(shr->alpha);
				shr->combined[3]= fac;
			}			
			shr->combined[0]*= fac;
			shr->combined[1]*= fac;
			shr->combined[2]*= fac;
		}
		else shr->combined[3]= 1.0;
	}
	
	if(R.flag & R_LAMPHALO) {
		if(facenr<=0) {	/* calc view vector and put shi.co at far */
			if(R.r.mode & R_ORTHO) {
				/* x and y 3d coordinate can be derived from pixel coord and winmat */
				float fx= 2.0/(R.rectx*R.winmat[0][0]);
				float fy= 2.0/(R.recty*R.winmat[1][1]);
				
				shi.co[0]= (0.5 + x - 0.5*R.rectx)*fx - R.winmat[3][0]/R.winmat[0][0];
				shi.co[1]= (0.5 + y - 0.5*R.recty)*fy - R.winmat[3][1]/R.winmat[1][1];
			}
			
			calc_view_vector(shi.view, x, y);
			shi.co[2]= 0.0;
			
			renderspothalo(&shi, shr->combined, 1.0);
		}
		else
			renderspothalo(&shi, shr->combined, shr->combined[3]);
	}
	
	return vlr;
}

static void shadepixel_sky(ShadePixelInfo *shpi, float x, float y, int z, int facenr, int mask)
{
	VlakRen *vlr;
	float collector[4], rco[3];
	
	vlr= shadepixel(shpi, x, y, z, facenr, mask, rco);
	if(shpi->shr.combined[3] != 1.0) {
		
		/* bail out when raytrace transparency (sky included already) */
		if(vlr && (R.r.mode & R_RAYTRACE))
			if(vlr->mat->mode & MA_RAYTRANSP) return;
		
		renderSkyPixelFloat(collector, x, y, vlr?rco:NULL);
		addAlphaOverFloat(collector, shpi->shr.combined);
		QUATCOPY(shpi->shr.combined, collector);
	}
}

/* adds only alpha values */
static void edge_enhance_calc(RenderPart *pa, float *rectf)	
{
	/* use zbuffer to define edges, add it to the image */
	int y, x, col, *rz, *rz1, *rz2, *rz3;
	int zval1, zval2, zval3;
	float *rf;
	
	/* shift values in zbuffer 4 to the right, for filter we need multiplying with 12 max */
	rz= pa->rectz;
	if(rz==NULL) return;
	
	for(y=0; y<pa->recty; y++) {
		for(x=0; x<pa->rectx; x++, rz++) (*rz)>>= 4;
	}
	
	rz1= pa->rectz;
	rz2= rz1+pa->rectx;
	rz3= rz2+pa->rectx;
	
	rf= rectf+pa->rectx+1;
	
	for(y=0; y<pa->recty-2; y++) {
		for(x=0; x<pa->rectx-2; x++, rz1++, rz2++, rz3++, rf++) {
			
			/* prevent overflow with sky z values */
			zval1=   rz1[0] + 2*rz1[1] +   rz1[2];
			zval2=  2*rz2[0]           + 2*rz2[2];
			zval3=   rz3[0] + 2*rz3[1] +   rz3[2];
			
			col= abs ( 4*rz2[1] - (zval1 + zval2 + zval3)/3 );
			
			col >>= 5;
			if(col > (1<<16)) col= (1<<16);
			else col= (R.r.edgeint*col)>>8;
			
			if(col>0) {
				float fcol;
				
				if(col>255) fcol= 1.0f;
				else fcol= (float)col/255.0f;
				
				if(R.osa)
					*rf+= fcol/(float)R.osa;
				else
					*rf= fcol;
			}
		}
		rz1+= 2;
		rz2+= 2;
		rz3+= 2;
		rf+= 2;
	}
}

static void edge_enhance_add(RenderPart *pa, float *rectf, float *arect)
{
	float addcol[4];
	int pix;
	
	for(pix= pa->rectx*pa->recty; pix>0; pix--, arect++, rectf+=4) {
		if(*arect != 0.0f) {
			addcol[0]= *arect * R.r.edgeR;
			addcol[1]= *arect * R.r.edgeG;
			addcol[2]= *arect * R.r.edgeB;
			addcol[3]= *arect;
			addAlphaOverFloat(rectf, addcol);
		}
	}
}


/* ********************* MAINLOOPS ******************** */

/* osa version */
static void add_filt_passes(RenderLayer *rl, int curmask, int rectx, int offset, ShadeResult *shr)
{
	RenderPass *rpass;
	
	for(rpass= rl->passes.first; rpass; rpass= rpass->next) {
		float *fp, *col= NULL;
		int pixsize= 3;
		
		switch(rpass->passtype) {
			case SCE_PASS_RGBA:
				col= shr->col;
				pixsize= 4;
				break;
			case SCE_PASS_DIFFUSE:
				col= shr->diff;
				break;
			case SCE_PASS_SPEC:
				col= shr->spec;
				break;
			case SCE_PASS_SHADOW:
				col= shr->shad;
				break;
			case SCE_PASS_AO:
				col= shr->ao;
				break;
			case SCE_PASS_RAY:
				col= shr->ray;
				break;
			case SCE_PASS_NORMAL:
				col= shr->nor;
				break;
			case SCE_PASS_VECTOR:
			{
				/* add minimum speed in pixel */
				fp= rpass->rect + 4*offset;
				if( (ABS(shr->winspeed[0]) + ABS(shr->winspeed[1]))< (ABS(fp[0]) + ABS(fp[1])) ) {
					fp[0]= shr->winspeed[0];
					fp[1]= shr->winspeed[1];
				}
				if( (ABS(shr->winspeed[2]) + ABS(shr->winspeed[3]))< (ABS(fp[2]) + ABS(fp[3])) ) {
					fp[2]= shr->winspeed[2];
					fp[3]= shr->winspeed[3];
				}
			}
				break;
		}
		if(col) {
			fp= rpass->rect + pixsize*offset;
			add_filt_fmask_pixsize(curmask, col, fp, rectx, pixsize);
		}
	}
}

/* non-osa version */
static void add_passes(RenderLayer *rl, int offset, ShadeResult *shr)
{
	RenderPass *rpass;
	
	for(rpass= rl->passes.first; rpass; rpass= rpass->next) {
		float *fp, *col= NULL;
		int a, pixsize= 3;
		
		switch(rpass->passtype) {
			case SCE_PASS_RGBA:
				col= shr->col;
				pixsize= 4;
				break;
			case SCE_PASS_DIFFUSE:
				col= shr->diff;
				break;
			case SCE_PASS_SPEC:
				col= shr->spec;
				break;
			case SCE_PASS_SHADOW:
				col= shr->shad;
				break;
			case SCE_PASS_AO:
				col= shr->ao;
				break;
			case SCE_PASS_RAY:
				col= shr->ray;
				break;
			case SCE_PASS_NORMAL:
				col= shr->nor;
				break;
			case SCE_PASS_VECTOR:
				col= shr->winspeed;
				pixsize= 4;
				break;
		}
		if(col) {
			fp= rpass->rect + pixsize*offset;
			for(a=0; a<pixsize; a++)
				fp[a]= col[a];
		}
	}
}


static void shadeDA_tile(RenderPart *pa, RenderLayer *rl)
{
	RenderResult *rr= pa->result;
	ShadePixelInfo shpi;
	PixStr *ps;
	float xs, ys;
	float *fcol= shpi.shr.combined, *rf, *rectf= rl->rectf;
	long *rd, *rectdaps= pa->rectdaps;
	int zbuf, samp, curmask, face, mask, fullmask;
	int b, x, y, full_osa, seed, crop=0, offs=0, od, addpassflag;
	
	if(R.test_break()) return; 
	
	/* we set per pixel a fixed seed, for random AO and shadow samples */
	seed= pa->rectx*pa->disprect.ymin;

	fullmask= (1<<R.osa)-1;
	
	/* fill shadepixel info struct */
	shpi.thread= pa->thread;
	shpi.lay= rl->lay;
	shpi.passflag= 0;
	
	if(rl->passflag & ~(SCE_PASS_Z|SCE_PASS_NORMAL|SCE_PASS_VECTOR|SCE_PASS_COMBINED))
		shpi.passflag= rl->passflag;
	addpassflag= rl->passflag & ~(SCE_PASS_Z|SCE_PASS_COMBINED);
				
	/* filtered render, for now we assume only 1 filter size */
	if(pa->crop) {
		crop= 1;
		rectf+= 4*(pa->rectx + 1);
		rectdaps+= pa->rectx + 1;
		offs= pa->rectx + 1;
	}
	
	/* scanline updates have to be 2 lines behind */
	rr->renrect.ymin= 0;
	rr->renrect.ymax= -2*crop;
	
	for(y=pa->disprect.ymin+crop; y<pa->disprect.ymax-crop; y++, rr->renrect.ymax++) {
		rf= rectf;
		rd= rectdaps;
		od= offs;
		
		for(x=pa->disprect.xmin+crop; x<pa->disprect.xmax-crop; x++, rd++, rf+=4, od++) {
			BLI_thread_srandom(pa->thread, seed+x);
			
			ps= (PixStr *)(*rd);
			mask= 0;

			/* complex loop, because empty spots are sky, without mask */
			while(TRUE) {
				
				if(ps==NULL) {
					face= 0;
					curmask= (~mask) & fullmask;
					zbuf= 0x7FFFFFFF;
				}
				else {
					face= ps->facenr;
					curmask= ps->mask;
					zbuf= ps->z;
				}
				
				/* check osa level */
				if(face==0) full_osa= 0;
				else {
					VlakRen *vlr= RE_findOrAddVlak(&R, (face-1) & 0x7FFFFF);
					full_osa= (vlr->flag & R_FULL_OSA);
				}
				
				if(full_osa) {
					for(samp=0; samp<R.osa; samp++) {
						if(curmask & (1<<samp)) {
							xs= (float)x + R.jit[samp][0];
							ys= (float)y + R.jit[samp][1];
							shadepixel_sky(&shpi, xs, ys, zbuf, face, (1<<samp));
							
							if(R.do_gamma) {
								fcol[0]= gammaCorrect(fcol[0]);
								fcol[1]= gammaCorrect(fcol[1]);
								fcol[2]= gammaCorrect(fcol[2]);
							}
							add_filt_fmask(1<<samp, fcol, rf, pa->rectx);
							
							if(addpassflag)
								add_filt_passes(rl, curmask, pa->rectx, od, &shpi.shr);
						}
					}
				}
				else if(curmask) {
					b= R.samples->centmask[curmask];
					xs= (float)x+R.samples->centLut[b & 15];
					ys= (float)y+R.samples->centLut[b>>4];
					shadepixel_sky(&shpi, xs, ys, zbuf, face, curmask);
	
					if(R.do_gamma) {
						fcol[0]= gammaCorrect(fcol[0]);
						fcol[1]= gammaCorrect(fcol[1]);
						fcol[2]= gammaCorrect(fcol[2]);
					}
					add_filt_fmask(curmask, fcol, rf, pa->rectx);
					
					if(addpassflag)
						add_filt_passes(rl, curmask, pa->rectx, od, &shpi.shr);
				}
				
				mask |= curmask;
				
				if(ps==NULL) break;
				else ps= ps->next;
			}
		}
		
		rectf+= 4*pa->rectx;
		rectdaps+= pa->rectx;
		offs+= pa->rectx;
		seed+= pa->rectx;
		
		if(y&1) if(R.test_break()) break; 
	}
	
	if(R.do_gamma) {
		rectf= rl->rectf;
		for(y= pa->rectx*pa->recty; y>0; y--, rectf+=4) {
			rectf[0] = invGammaCorrect(rectf[0]);
			rectf[1] = invGammaCorrect(rectf[1]);
			rectf[2] = invGammaCorrect(rectf[2]);
		}
	}			
	
}

/* ************* pixel struct ******** */


static PixStrMain *addpsmain(ListBase *lb)
{
	PixStrMain *psm;
	
	psm= (PixStrMain *)RE_mallocN(sizeof(PixStrMain),"pixstrMain");
	BLI_addtail(lb, psm);
	
	psm->ps= (PixStr *)RE_mallocN(4096*sizeof(PixStr),"pixstr");
	psm->counter= 0;
	
	return psm;
}

static void freeps(ListBase *lb)
{
	PixStrMain *psm, *psmnext;
	
	for(psm= lb->first; psm; psm= psmnext) {
		psmnext= psm->next;
		if(psm->ps)
			RE_freeN(psm->ps);
		RE_freeN(psm);
	}
	lb->first= lb->last= NULL;
}

static void addps(ListBase *lb, long *rd, int facenr, int z, unsigned short mask)
{
	PixStrMain *psm;
	PixStr *ps, *last= NULL;
	
	if(*rd) {	
		ps= (PixStr *)(*rd);
		
		while(ps) {
			if( ps->facenr == facenr ) {
				ps->mask |= mask;
				return;
			}
			last= ps;
			ps= ps->next;
		}
	}
	
	/* make new PS (pixel struct) */
	psm= lb->last;
	
	if(psm->counter==4095)
		psm= addpsmain(lb);
	
	ps= psm->ps + psm->counter++;
	
	if(last) last->next= ps;
	else *rd= (long)ps;
	
	ps->next= NULL;
	ps->facenr= facenr;
	ps->z= z;
	ps->mask = mask;
}

static void make_pixelstructs(RenderPart *pa, ListBase *lb)
{
	long *rd= pa->rectdaps;
	int *rp= pa->rectp;
	int *rz= pa->rectz;
	int x, y;
	int mask= 1<<pa->sample;
	
	for(y=0; y<pa->recty; y++) {
		for(x=0; x<pa->rectx; x++, rd++, rp++) {
			if(*rp) {
				addps(lb, rd, *rp, *(rz+x), mask);
			}
		}
		rz+= pa->rectx;
	}
}

/* supposed to be fully threadable! */
void zbufshadeDA_tile(RenderPart *pa)
{
	RenderResult *rr= pa->result;
	RenderLayer *rl;
	ListBase psmlist= {NULL, NULL};
	float *edgerect= NULL;
	
	set_part_zbuf_clipflag(pa);
	
	/* allocate the necessary buffers */
				/* zbuffer inits these rects */
	pa->rectp= RE_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectp");
	pa->rectz= RE_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectz");
	if(R.r.mode & R_EDGE) edgerect= RE_callocN(sizeof(float)*pa->rectx*pa->recty, "rectedge");
	
	for(rl= rr->layers.first; rl; rl= rl->next) {
		/* indication for scanline updates */
		rr->renlay= rl;

		/* initialize pixelstructs */
		addpsmain(&psmlist);
		pa->rectdaps= RE_callocN(sizeof(long)*pa->rectx*pa->recty+4, "zbufDArectd");
		
		if(rl->layflag & SCE_LAY_SOLID) {
			for(pa->sample=0; pa->sample<R.osa; pa->sample++) {
				zbuffer_solid(pa, rl->lay, rl->layflag);
				make_pixelstructs(pa, &psmlist);
				
				if(R.r.mode & R_EDGE) edge_enhance_calc(pa, edgerect);
				if(R.test_break()) break; 
			}
		}
		else	/* need to clear rectz for next passes */
			fillrect(pa->rectz, pa->rectx, pa->recty, 0x7FFFFFFF);

		
		/* shades solid */
		if(rl->layflag & SCE_LAY_SOLID) 
			shadeDA_tile(pa, rl);
		
		/* transp layer */
		if(R.flag & R_ZTRA) {
			if(rl->layflag & SCE_LAY_ZTRA) {
				float *acolrect= RE_callocN(4*sizeof(float)*pa->rectx*pa->recty, "alpha layer");
				float *fcol= rl->rectf, *acol= acolrect;
				int x;
				
				/* swap for live updates */
				SWAP(float *, acolrect, rl->rectf);
				zbuffer_transp_shade(pa, rl->rectf, rl->lay, rl->layflag);
				SWAP(float *, acolrect, rl->rectf);
				
				for(x=pa->rectx*pa->recty; x>0; x--, acol+=4, fcol+=4) {
					addAlphaOverFloat(fcol, acol);
				}
				RE_freeN(acolrect);
			}
		}
		
		/* extra layers */
		if(R.r.mode & R_EDGE) 
			edge_enhance_add(pa, rl->rectf, edgerect);
		if(R.flag & R_HALO)
			if(rl->layflag & SCE_LAY_HALO)
				halo_tile(pa, rl->rectf, rl->lay);
		
		if(rl->passflag & SCE_PASS_Z)
			convert_zbuf_to_distbuf(pa, rl);
		
		/* free stuff within loop! */
		RE_freeN(pa->rectdaps); pa->rectdaps= NULL;
		freeps(&psmlist);
	}
	
	/* free all */
	RE_freeN(pa->rectp); pa->rectp= NULL;
	RE_freeN(pa->rectz); pa->rectz= NULL;
	
	if(edgerect) RE_freeN(edgerect);
	
	/* display active layer */
	rr->renlay= BLI_findlink(&rr->layers, R.r.actlay);

}


/* ------------------------------------------------------------------------ */

/* supposed to be fully threadable! */
void zbufshade_tile(RenderPart *pa)
{
	ShadePixelInfo shpi;
	RenderResult *rr= pa->result;
	RenderLayer *rl;
	int addpassflag;
	
	set_part_zbuf_clipflag(pa);
	
	/* zbuffer code clears/inits rects */
	pa->rectp= RE_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectp");
	pa->rectz= RE_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectz");
	
	shpi.thread= pa->thread;
	
	for(rl= rr->layers.first; rl; rl= rl->next) {
		/* indication for scanline updates */
		rr->renlay= rl;
		
		/* fill shadepixel info struct */
		shpi.lay= rl->lay;
		shpi.passflag= 0;
		
		if(rl->passflag & ~(SCE_PASS_Z|SCE_PASS_NORMAL|SCE_PASS_VECTOR|SCE_PASS_COMBINED))
			shpi.passflag= rl->passflag;
		addpassflag= rl->passflag & ~(SCE_PASS_Z|SCE_PASS_COMBINED);
		
		zbuffer_solid(pa, rl->lay, rl->layflag);
		
		if(!R.test_break()) {
			if(rl->layflag & SCE_LAY_SOLID) {
				float *fcol= rl->rectf;
				int x, y, *rp= pa->rectp, *rz= pa->rectz, offs=0;
				
				/* init scanline updates */
				rr->renrect.ymin=rr->renrect.ymax= 0;
				
				for(y=pa->disprect.ymin; y<pa->disprect.ymax; y++, rr->renrect.ymax++) {
					for(x=pa->disprect.xmin; x<pa->disprect.xmax; x++, rz++, rp++, fcol+=4, offs++) {
						shadepixel_sky(&shpi, (float)x, (float)y, *rz, *rp, 0);
						QUATCOPY(fcol, shpi.shr.combined);
						
						/* passes */
						if(addpassflag)
							add_passes(rl, offs, &shpi.shr);
					}
					if(y&1)
						if(R.test_break()) break; 
				}
			}
		}
		
		if(!R.test_break()) {
			if(R.flag & R_ZTRA) {
				if(rl->layflag & SCE_LAY_ZTRA) {
					float *acolrect= RE_callocN(4*sizeof(float)*pa->rectx*pa->recty, "alpha layer");
					float *fcol= rl->rectf, *acol= acolrect;
					int x;
					
					/* swap for live updates */
					SWAP(float *, acolrect, rl->rectf);
					zbuffer_transp_shade(pa, rl->rectf, rl->lay, rl->layflag);
					SWAP(float *, acolrect, rl->rectf);
					
					for(x=pa->rectx*pa->recty; x>0; x--, acol+=4, fcol+=4) {
						addAlphaOverFloat(fcol, acol);
					}
					RE_freeN(acolrect);
				}
			}
		}
		
		if(!R.test_break()) {
			if(R.r.mode & R_EDGE) {
				fillrect(pa->rectp, pa->rectx, pa->recty, 0);
				edge_enhance_calc(pa, (float *)pa->rectp);
				edge_enhance_add(pa, rl->rectf, (float *)pa->rectp);
			}
		}
		
		if(!R.test_break())
			if(R.flag & R_HALO)
				if(rl->layflag & SCE_LAY_HALO)
					halo_tile(pa, rl->rectf, rl->lay);
		
		if(rl->passflag & SCE_PASS_Z)
			convert_zbuf_to_distbuf(pa, rl);

	}

	/* display active layer */
	rr->renlay= BLI_findlink(&rr->layers, R.r.actlay);
	
	RE_freeN(pa->rectp); pa->rectp= NULL;
	RE_freeN(pa->rectz); pa->rectz= NULL;
}

/* ------------------------------------------------------------------------ */

static void renderhalo(HaloRen *har)	/* postprocess version */
{
#if 0
	float dist, xsq, ysq, xn, yn, colf[4], *rectft, *rtf;
	int *rectt, *rt;
	int minx, maxx, miny, maxy, x, y;
	char col[4];

	har->miny= miny= har->ys - har->rad/R.ycor;
	har->maxy= maxy= har->ys + har->rad/R.ycor;

	if(maxy<0);
	else if(R.recty<miny);
	else {
		minx= floor(har->xs-har->rad);
		maxx= ceil(har->xs+har->rad);
			
		if(maxx<0);
		else if(R.rectx<minx);
		else {
		
			if(minx<0) minx= 0;
			if(maxx>=R.rectx) maxx= R.rectx-1;
			if(miny<0) miny= 0;
			if(maxy>R.recty) maxy= R.recty;
	
			rectt= R.rectot+ R.rectx*miny;
			rectft= R.rectftot+ 4*R.rectx*miny;

			for(y=miny; y<maxy; y++) {
	
				rt= rectt+minx;
				rtf= rectft+4*minx;
				
				yn= (y - har->ys)*R.ycor;
				ysq= yn*yn;
				
				for(x=minx; x<=maxx; x++) {
					xn= x - har->xs;
					xsq= xn*xn;
					dist= xsq+ysq;
					if(dist<har->radsq) {
						
						shadeHaloFloat(har, colf, 0x7FFFFF, dist, xn, yn, har->flarec);
						if(R.rectftot) addalphaAddfacFloat(rtf, colf, har->add);
						else {
							std_floatcol_to_charcol(colf, col);
							addalphaAddfac((char *)rt, col, har->add);
						}
					}
					rt++;
					rtf+=4;
				}
	
				rectt+= R.rectx;
				rectft+= 4*R.rectx;
				
				if(R.test_break()) break; 
			}
		}
	}
#endif
} 
/* ------------------------------------------------------------------------ */

static void renderflare(HaloRen *har)
{
	extern float hashvectf[];
	HaloRen fla;
	Material *ma;
	float *rc, rad, alfa, visifac, vec[3];
	int b, type;
	
	fla= *har;
	fla.linec= fla.ringc= fla.flarec= 0;
	
	rad= har->rad;
	alfa= har->alfa;
	
	visifac= R.ycor*(har->pixels);
	/* all radials added / r^3  == 1.0! */
	visifac /= (har->rad*har->rad*har->rad);
	visifac*= visifac;

	ma= har->mat;
	
	/* first halo: just do */
	
	har->rad= rad*ma->flaresize*visifac;
	har->radsq= har->rad*har->rad;
	har->zs= fla.zs= 0;
	
	har->alfa= alfa*visifac;

	renderhalo(har);
	
	/* next halo's: the flares */
	rc= hashvectf + ma->seed2;
	
	for(b=1; b<har->flarec; b++) {
		
		fla.r= fabs(rc[0]);
		fla.g= fabs(rc[1]);
		fla.b= fabs(rc[2]);
		fla.alfa= ma->flareboost*fabs(alfa*visifac*rc[3]);
		fla.hard= 20.0 + fabs(70*rc[7]);
		fla.tex= 0;
		
		type= (int)(fabs(3.9*rc[6]));

		fla.rad= ma->subsize*sqrt(fabs(2.0*har->rad*rc[4]));
		
		if(type==3) {
			fla.rad*= 3.0;
			fla.rad+= R.rectx/10;
		}
		
		fla.radsq= fla.rad*fla.rad;
		
		vec[0]= 1.4*rc[5]*(har->xs-R.winx/2);
		vec[1]= 1.4*rc[5]*(har->ys-R.winy/2);
		vec[2]= 32.0*sqrt(vec[0]*vec[0] + vec[1]*vec[1] + 1.0);
		
		fla.xs= R.winx/2 + vec[0] + (1.2+rc[8])*R.rectx*vec[0]/vec[2];
		fla.ys= R.winy/2 + vec[1] + (1.2+rc[8])*R.rectx*vec[1]/vec[2];

		if(R.flag & R_SEC_FIELD) {
			if(R.r.mode & R_ODDFIELD) fla.ys += 0.5;
			else fla.ys -= 0.5;
		}
		if(type & 1) fla.type= HA_FLARECIRC;
		else fla.type= 0;
		renderhalo(&fla);

		fla.alfa*= 0.5;
		if(type & 2) fla.type= HA_FLARECIRC;
		else fla.type= 0;
		renderhalo(&fla);
		
		rc+= 7;
	}
}

/* needs recode... integrate this! */
void add_halo_flare(void)
{
	HaloRen *har = NULL;
	int a, mode;
	
	mode= R.r.mode;
	R.r.mode &= ~R_PANORAMA;
//	R.xstart= -R.afmx; 
//	R.ystart= -R.afmy;
//	R.xend= R.xstart+R.rectx-1;
//	R.yend= R.ystart+R.recty-1;

//	RE_setwindowclip(1,-1); /*  no jit:(-1) */
	project_renderdata(&R, projectverto, 0, 0);
	
	for(a=0; a<R.tothalo; a++) {
		if((a & 255)==0) har= R.bloha[a>>8];
		else har++;
		
		if(har->flarec) {
			renderflare(har);
		}
	}

	R.r.mode= mode;	

}


/* end of render.c */


