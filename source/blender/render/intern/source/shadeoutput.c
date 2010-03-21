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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation
 * All rights reserved.
 *
 * Contributors: Hos, Robert Wenzlaff.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <float.h>
#include <math.h>
#include <string.h>


#include "BLI_math.h"

#include "BKE_colortools.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"

/* local include */
#include "occlusion.h"
#include "renderpipeline.h"
#include "render_types.h"
#include "pixelblending.h"
#include "rendercore.h"
#include "shadbuf.h"
#include "sss.h"
#include "texture.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

ListBase *get_lights(ShadeInput *shi)
{
	
	if(R.r.scemode & R_PREVIEWBUTS)
		return &R.lights;
	if(shi->light_override)
		return &shi->light_override->gobject;
	if(shi->mat && shi->mat->group)
		return &shi->mat->group->gobject;
	
	return &R.lights;
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

/* zcor is distance, co the 3d coordinate in eye space, return alpha */
float mistfactor(float zcor, float *co)	
{
	float fac, hi;
	
	fac= zcor - R.wrld.miststa;	/* zcor is calculated per pixel */

	/* fac= -co[2]-R.wrld.miststa; */

	if(fac>0.0f) {
		if(fac< R.wrld.mistdist) {
			
			fac= (fac/(R.wrld.mistdist));
			
			if(R.wrld.mistype==0) fac*= fac;
			else if(R.wrld.mistype==1);
			else fac= sqrt(fac);
		}
		else fac= 1.0f;
	}
	else fac= 0.0f;
	
	/* height switched off mist */
	if(R.wrld.misthi!=0.0f && fac!=0.0f) {
		/* at height misthi the mist is completely gone */

		hi= R.viewinv[0][2]*co[0]+R.viewinv[1][2]*co[1]+R.viewinv[2][2]*co[2]+R.viewinv[3][2];
		
		if(hi>R.wrld.misthi) fac= 0.0f;
		else if(hi>0.0f) {
			hi= (R.wrld.misthi-hi)/R.wrld.misthi;
			fac*= hi*hi;
		}
	}

	return (1.0f-fac)* (1.0f-R.wrld.misi);	
}

static void spothalo(struct LampRen *lar, ShadeInput *shi, float *intens)
{
	double a, b, c, disc, nray[3], npos[3];
	float t0, t1 = 0.0f, t2= 0.0f, t3, haint;
	float p1[3], p2[3], ladist, maxz = 0.0f, maxy = 0.0f;
	int snijp, doclip=1, use_yco=0;
	int ok1=0, ok2=0;
	
	*intens= 0.0f;
	haint= lar->haint;
	
	if(R.r.mode & R_ORTHO) {
		/* camera pos (view vector) cannot be used... */
		/* camera position (cox,coy,0) rotate around lamp */
		p1[0]= shi->co[0]-lar->co[0];
		p1[1]= shi->co[1]-lar->co[1];
		p1[2]= -lar->co[2];
		mul_m3_v3(lar->imat, p1);
		VECCOPY(npos, p1);	// npos is double!
		
		/* pre-scale */
		npos[2]*= lar->sh_zfac;
	}
	else {
		VECCOPY(npos, lar->sh_invcampos);	/* in initlamp calculated */
	}
	
	/* rotate view */
	VECCOPY(nray, shi->view);
	mul_m3_v3_double(lar->imat, nray);
	
	if(R.wrld.mode & WO_MIST) {
		/* patchy... */
		haint *= mistfactor(-lar->co[2], lar->co);
		if(haint==0.0f) {
			return;
		}
	}


	/* rotate maxz */
	if(shi->co[2]==0.0f) doclip= 0;	/* for when halo at sky */
	else {
		p1[0]= shi->co[0]-lar->co[0];
		p1[1]= shi->co[1]-lar->co[1];
		p1[2]= shi->co[2]-lar->co[2];
	
		maxz= lar->imat[0][2]*p1[0]+lar->imat[1][2]*p1[1]+lar->imat[2][2]*p1[2];
		maxz*= lar->sh_zfac;
		maxy= lar->imat[0][1]*p1[0]+lar->imat[1][1]*p1[1]+lar->imat[2][1]*p1[2];

		if( fabs(nray[2]) < DBL_EPSILON ) use_yco= 1;
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
	if (fabs(a) < DBL_EPSILON) {
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
		if(p1[2]<=0.0f) ok1= 1;
		if(p2[2]<=0.0f && t1!=t2) ok2= 1;
		
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
		if(t1<0.0f && t2<0.0f) return;
		
		if(t1<0.0f) t1= 0.0f;
		if(t2<0.0f) t2= 0.0f;
		
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
		c= len_v3v3(p1, p2);
		
		a/= ladist;
		a= sqrt(a);
		b/= ladist; 
		b= sqrt(b);
		c/= ladist;
		
		*intens= c*( (1.0-a)+(1.0-b) );

		/* WATCH IT: do not clip a,b en c at 1.0, this gives nasty little overflows
			at the edges (especially with narrow halos) */
		if(*intens<=0.0f) return;

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

void renderspothalo(ShadeInput *shi, float *col, float alpha)
{
	ListBase *lights;
	GroupObject *go;
	LampRen *lar;
	float i;
	
	if(alpha==0.0f) return;
	
	lights= get_lights(shi);
	for(go=lights->first; go; go= go->next) {
		lar= go->lampren;
		if(lar==NULL) continue;
		
		if(lar->type==LA_SPOT && (lar->mode & LA_HALO) && (lar->buftype != LA_SHADBUF_DEEP) && lar->haint>0) {
			
			if(lar->mode & LA_LAYER) 
				if(shi->vlr && (lar->lay & shi->obi->lay)==0) 
					continue;
			if((lar->lay & shi->lay)==0) 
				continue;
			
			spothalo(lar, shi, &i);
			if(i>0.0f) {
				col[3]+= i*alpha;			// all premul
				col[0]+= i*lar->r*alpha;
				col[1]+= i*lar->g*alpha;
				col[2]+= i*lar->b*alpha;	
			}
		}
	}
	/* clip alpha, is needed for unified 'alpha threshold' (vanillaRenderPipe.c) */
	if(col[3]>1.0f) col[3]= 1.0f;
}



/* ---------------- shaders ----------------------- */

static double Normalize_d(double *n)
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
	
	if(fac==0.0f) return 1.0f;
	
	t1= (view[0]*vn[0] + view[1]*vn[1] + view[2]*vn[2]);
	if(t1>0.0f)  t2= 1.0f+t1;
	else t2= 1.0f-t1;
	
	t2= grad + (1.0f-grad)*pow(t2, fac);
	
	if(t2<0.0f) return 0.0f;
	else if(t2>1.0f) return 1.0f;
	return t2;
}

static double saacos_d(double fac)
{
	if(fac<= -1.0f) return M_PI;
	else if(fac>=1.0f) return 0.0;
	else return acos(fac);
}

/* Stoke's form factor. Need doubles here for extreme small area sizes */
static float area_lamp_energy(float (*area)[3], float *co, float *vn)
{
	double fac;
	double vec[4][3];	/* vectors of rendered co to vertices lamp */
	double cross[4][3];	/* cross products of this */
	double rad[4];		/* angles between vecs */

	VECSUB(vec[0], co, area[0]);
	VECSUB(vec[1], co, area[1]);
	VECSUB(vec[2], co, area[2]);
	VECSUB(vec[3], co, area[3]);
	
	Normalize_d(vec[0]);
	Normalize_d(vec[1]);
	Normalize_d(vec[2]);
	Normalize_d(vec[3]);

	/* cross product */
	CROSS(cross[0], vec[0], vec[1]);
	CROSS(cross[1], vec[1], vec[2]);
	CROSS(cross[2], vec[2], vec[3]);
	CROSS(cross[3], vec[3], vec[0]);

	Normalize_d(cross[0]);
	Normalize_d(cross[1]);
	Normalize_d(cross[2]);
	Normalize_d(cross[3]);

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
	return fac;
}

static float area_lamp_energy_multisample(LampRen *lar, float *co, float *vn)
{
	/* corner vectors are moved around according lamp jitter */
	float *jitlamp= lar->jitter, vec[3];
	float area[4][3], intens= 0.0f;
	int a= lar->ray_totsamp;

	/* test if co is behind lamp */
	VECSUB(vec, co, lar->co);
	if(INPR(vec, lar->vec) < 0.0f)
		return 0.0f;

	while(a--) {
		vec[0]= jitlamp[0];
		vec[1]= jitlamp[1];
		vec[2]= 0.0f;
		mul_m3_v3(lar->mat, vec);
		
		VECADD(area[0], lar->area[0], vec);
		VECADD(area[1], lar->area[1], vec);
		VECADD(area[2], lar->area[2], vec);
		VECADD(area[3], lar->area[3], vec);
		
		intens+= area_lamp_energy(area, co, vn);
		
		jitlamp+= 2;
	}
	intens /= (float)lar->ray_totsamp;
	
	return pow(intens*lar->areasize, lar->k);	// corrected for buttons size and lar->dist^2
}

static float spec(float inp, int hard)	
{
	float b1;
	
	if(inp>=1.0f) return 1.0f;
	else if (inp<=0.0f) return 0.0f;
	
	b1= inp*inp;
	/* avoid FPE */
	if(b1<0.01f) b1= 0.01f;	
	
	if((hard & 1)==0)  inp= 1.0f;
	if(hard & 2)  inp*= b1;
	b1*= b1;
	if(hard & 4)  inp*= b1;
	b1*= b1;
	if(hard & 8)  inp*= b1;
	b1*= b1;
	if(hard & 16) inp*= b1;
	b1*= b1;

	/* avoid FPE */
	if(b1<0.001f) b1= 0.0f;	

	if(hard & 32) inp*= b1;
	b1*= b1;
	if(hard & 64) inp*=b1;
	b1*= b1;
	if(hard & 128) inp*=b1;

	if(b1<0.001f) b1= 0.0f;	

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
	normalize_v3(h);
	
	rslt = h[0]*n[0] + h[1]*n[1] + h[2]*n[2];
	if(tangent) rslt= sasqrt(1.0f - rslt*rslt);
		
	if( rslt > 0.0f ) rslt= spec(rslt, hard);
	else rslt = 0.0f;
	
	return rslt;
}


/* reduced cook torrance spec (for off-specular peak) */
static float CookTorr_Spec(float *n, float *l, float *v, int hard, int tangent)
{
	float i, nh, nv, h[3];

	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	normalize_v3(h);

	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2];
	if(tangent) nh= sasqrt(1.0f - nh*nh);
	else if(nh<0.0f) return 0.0f;
	
	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2];
	if(tangent) nv= sasqrt(1.0f - nv*nv);
	else if(nv<0.0f) nv= 0.0f;

	i= spec(nh, hard);

	i= i/(0.1+nv);
	return i;
}

/* Blinn spec */
static float Blinn_Spec(float *n, float *l, float *v, float refrac, float spec_power, int tangent)
{
	float i, nh, nv, nl, vh, h[3];
	float a, b, c, g=0.0f, p, f, ang;

	if(refrac < 1.0f) return 0.0f;
	if(spec_power == 0.0f) return 0.0f;
	
	/* conversion from 'hardness' (1-255) to 'spec_power' (50 maps at 0.1) */
	if(spec_power<100.0f)
		spec_power= sqrt(1.0f/spec_power);
	else spec_power= 10.0f/spec_power;
	
	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	normalize_v3(h);

	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if(tangent) nh= sasqrt(1.0f - nh*nh);
	else if(nh<0.0f) return 0.0f;

	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if(tangent) nv= sasqrt(1.0f - nv*nv);
	if(nv<=0.01f) nv= 0.01f;				/* hrms... */

	nl= n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if(tangent) nl= sasqrt(1.0f - nl*nl);
	if(nl<=0.01f) {
		return 0.0f;
	}

	vh= v[0]*h[0]+v[1]*h[1]+v[2]*h[2]; /* Dot product between view vector and half-way vector */
	if(vh<=0.0f) vh= 0.01f;

	a = 1.0f;
	b = (2.0f*nh*nv)/vh;
	c = (2.0f*nh*nl)/vh;

	if( a < b && a < c ) g = a;
	else if( b < a && b < c ) g = b;
	else if( c < a && c < b ) g = c;

	p = sqrt( (double)((refrac * refrac)+(vh*vh)-1.0f) );
	f = (((p-vh)*(p-vh))/((p+vh)*(p+vh)))*(1+((((vh*(p+vh))-1.0f)*((vh*(p+vh))-1.0f))/(((vh*(p-vh))+1.0f)*((vh*(p-vh))+1.0f))));
	ang = saacos(nh);

	i= f * g * exp((double)(-(ang*ang) / (2.0f*spec_power*spec_power)));
	if(i<0.0f) i= 0.0f;
	
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
	normalize_v3(h);
	
	rslt = h[0]*n[0] + h[1]*n[1] + h[2]*n[2];
	if(tangent) rslt = sasqrt(1.0f - rslt*rslt);
	
	ang = saacos( rslt ); 
	
	if( ang < size ) rslt = 1.0f;
	else if( ang >= (size + smooth) || smooth == 0.0f ) rslt = 0.0f;
	else rslt = 1.0f - ((ang - size) / smooth);
	
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
	normalize_v3(h);

	nh = n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if(tangent) nh = sasqrt(1.0f - nh*nh);
	if(nh<=0.0f) nh = 0.001f;
	
	nv = n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if(tangent) nv = sasqrt(1.0f - nv*nv);
	if(nv<=0.0f) nv = 0.001f;

	nl = n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if(tangent) nl = sasqrt(1.0f - nl*nl);
	if(nl<=0.0f) nl = 0.001f;

	angle = tan(saacos(nh));
	alpha = MAX2(rms, 0.001f);

	i= nl * (1.0f/(4.0f*M_PI*alpha*alpha)) * (exp( -(angle*angle)/(alpha*alpha))/(sqrt(nv*nl)));

	return i;
}

/* cartoon render diffuse */
static float Toon_Diff( float *n, float *l, float *v, float size, float smooth )
{
	float rslt, ang;

	rslt = n[0]*l[0] + n[1]*l[1] + n[2]*l[2];

	ang = saacos( (double)(rslt) );

	if( ang < size ) rslt = 1.0f;
	else if( ang >= (size + smooth) || smooth == 0.0f ) rslt = 0.0f;
	else rslt = 1.0f - ((ang - size) / smooth);

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
	normalize_v3(h);
	
	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if(nh<0.0f) nh = 0.0f;
	
	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if(nv<=0.0f) nv= 0.0f;
	
	realnl= n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if(realnl<=0.0f) return 0.0f;
	if(nl<0.0f) return 0.0f;		/* value from area light */
	
	vh= v[0]*h[0]+v[1]*h[1]+v[2]*h[2]; /* Dot product between view vector and halfway vector */
	if(vh<=0.0f) vh= 0.0f;
	
	Lit_A = saacos(realnl);
	View_A = saacos( nv );
	
	Lit_B[0] = l[0] - (realnl * n[0]);
	Lit_B[1] = l[1] - (realnl * n[1]);
	Lit_B[2] = l[2] - (realnl * n[2]);
	normalize_v3( Lit_B );
	
	View_B[0] = v[0] - (nv * n[0]);
	View_B[1] = v[1] - (nv * n[1]);
	View_B[2] = v[2] - (nv * n[2]);
	normalize_v3( View_B );
	
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
	
	A = 1.0f - (0.5f * ((rough * rough) / ((rough * rough) + 0.33f)));
	B = 0.45f * ((rough * rough) / ((rough * rough) + 0.09f));
	
	b*= 0.95f;	/* prevent tangens from shooting to inf, 'nl' can be not a dot product here. */
				/* overflow only happens with extreme size area light, and higher roughness */
	i = nl * ( A + ( B * t * sin(a) * tan(b) ) );
	
	return i;
}

/* Minnaert diffuse */
static float Minnaert_Diff(float nl, float *n, float *v, float darkness)
{

	float i, nv;

	/* nl = dot product between surface normal and light vector */
	if (nl <= 0.0f)
		return 0.0f;

	/* nv = dot product between surface normal and view vector */
	nv = n[0]*v[0]+n[1]*v[1]+n[2]*v[2];
	if (nv < 0.0f)
		nv = 0.0f;

	if (darkness <= 1.0f)
		i = nl * pow(MAX2(nv*nl, 0.1f), (darkness - 1.0f) ); /*The Real model*/
	else
		i = nl * pow( (1.001f - nv), (darkness  - 1.0f) ); /*Nvidia model*/

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
		if(ma->mode & (MA_FACETEXTURE_ALPHA))
			shi->alpha= shi->vcol[3];
	}
	
	if(ma->texco)
		do_material_tex(shi);

	if(ma->fresnel_tra!=0.0f) 
		shi->alpha*= fresnel_fac(shi->view, shi->vn, ma->fresnel_tra_i, ma->fresnel_tra);

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
	float fac=0.0f;
	
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

/* pure AO, check for raytrace and world should have been done */
/* preprocess, textures were not done, don't use shi->amb for that reason */
void ambient_occlusion(ShadeInput *shi)
{
	if((R.wrld.ao_gather_method == WO_AOGATHER_APPROX) && shi->mat->amb!=0.0f)
		sample_occ(&R, shi);
	else if((R.r.mode & R_RAYTRACE) && shi->mat->amb!=0.0f)
		ray_ao(shi, shi->ao, shi->env);
	else
		shi->ao[0]= shi->ao[1]= shi->ao[2]= 1.0f;
}


/* wrld mode was checked for */
static void ambient_occlusion_apply(ShadeInput *shi, ShadeResult *shr)
{
	float f= R.wrld.aoenergy;
	float tmp[3], tmpspec[3];

	if(!((R.r.mode & R_RAYTRACE) || R.wrld.ao_gather_method == WO_AOGATHER_APPROX))
		return;
	if(f == 0.0f)
		return;

	if(R.wrld.aomix==WO_AOADD) {
		shr->combined[0] += shi->ao[0]*shi->r*shi->refl*f;
		shr->combined[1] += shi->ao[1]*shi->g*shi->refl*f;
		shr->combined[2] += shi->ao[2]*shi->b*shi->refl*f;
	}
	else if(R.wrld.aomix==WO_AOMUL) {
		mul_v3_v3v3(tmp, shr->combined, shi->ao);
		mul_v3_v3v3(tmpspec, shr->spec, shi->ao);

		if(f == 1.0f) {
			copy_v3_v3(shr->combined, tmp);
			copy_v3_v3(shr->spec, tmpspec);
		}
		else {
			interp_v3_v3v3(shr->combined, shr->combined, tmp, f);
			interp_v3_v3v3(shr->spec, shr->spec, tmpspec, f);
		}
	}
}

void environment_lighting_apply(ShadeInput *shi, ShadeResult *shr)
{
	float f= R.wrld.ao_env_energy*shi->amb;

	if(!((R.r.mode & R_RAYTRACE) || R.wrld.ao_gather_method == WO_AOGATHER_APPROX))
		return;
	if(f == 0.0f)
		return;
	
	shr->combined[0] += shi->env[0]*shi->r*shi->refl*f;
	shr->combined[1] += shi->env[1]*shi->g*shi->refl*f;
	shr->combined[2] += shi->env[2]*shi->b*shi->refl*f;
}

static void indirect_lighting_apply(ShadeInput *shi, ShadeResult *shr)
{
	float f= R.wrld.ao_indirect_energy;

	if(!((R.r.mode & R_RAYTRACE) || R.wrld.ao_gather_method == WO_AOGATHER_APPROX))
		return;
	if(f == 0.0f)
		return;

	shr->combined[0] += shi->indirect[0]*shi->r*shi->refl*f;
	shr->combined[1] += shi->indirect[1]*shi->g*shi->refl*f;
	shr->combined[2] += shi->indirect[2]*shi->b*shi->refl*f;
}

/* result written in shadfac */
void lamp_get_shadow(LampRen *lar, ShadeInput *shi, float inp, float *shadfac, int do_real)
{
	LampShadowSubSample *lss= &(lar->shadsamp[shi->thread].s[shi->sample]);
	
	if(do_real || lss->samplenr!=shi->samplenr) {
		
		shadfac[0]= shadfac[1]= shadfac[2]= shadfac[3]= 1.0f;
		
		if(lar->shb) {
			if(lar->buftype==LA_SHADBUF_IRREGULAR)
				shadfac[3]= ISB_getshadow(shi, lar->shb);
			else
				shadfac[3] = testshadowbuf(&R, lar->shb, shi->co, shi->dxco, shi->dyco, inp, shi->mat->lbias);
		}
		else if(lar->mode & LA_SHAD_RAY) {
			ray_shadow(shi, lar, shadfac);
		}
		
		if(shi->depth==0) {
			QUATCOPY(lss->shadfac, shadfac);
			lss->samplenr= shi->samplenr;
		}
	}
	else {
		QUATCOPY(shadfac, lss->shadfac);
	}
}

/* lampdistance and spot angle, writes in lv and dist */
float lamp_get_visibility(LampRen *lar, float *co, float *lv, float *dist)
{
	if(lar->type==LA_SUN || lar->type==LA_HEMI) {
		*dist= 1.0f;
		VECCOPY(lv, lar->vec);
		return 1.0f;
	}
	else {
		float visifac= 1.0f, t;
		
		VECSUB(lv, co, lar->co);
		*dist= sqrt( INPR(lv, lv));
		t= 1.0f/dist[0];
		VECMUL(lv, t);
		
		/* area type has no quad or sphere option */
		if(lar->type==LA_AREA) {
			/* area is single sided */
			//if(INPR(lv, lar->vec) > 0.0f)
			//	visifac= 1.0f;
			//else
			//	visifac= 0.0f;
		}
		else {
			switch(lar->falloff_type)
			{
				case LA_FALLOFF_CONSTANT:
					visifac = 1.0f;
					break;
				case LA_FALLOFF_INVLINEAR:
					visifac = lar->dist/(lar->dist + dist[0]);
					break;
				case LA_FALLOFF_INVSQUARE:
					visifac = lar->dist / (lar->dist + dist[0]*dist[0]);
					break;
				case LA_FALLOFF_SLIDERS:
					if(lar->ld1>0.0f)
						visifac= lar->dist/(lar->dist+lar->ld1*dist[0]);
					if(lar->ld2>0.0f)
						visifac*= lar->distkw/(lar->distkw+lar->ld2*dist[0]*dist[0]);
					break;
				case LA_FALLOFF_CURVE:
					visifac = curvemapping_evaluateF(lar->curfalloff, 0, dist[0]/lar->dist);
					break;
			}
			
			if(lar->mode & LA_SPHERE) {
				float t= lar->dist - dist[0];
				if(t<=0.0f) 
					visifac= 0.0f;
				else
					visifac*= t/lar->dist;
			}
			
			if(visifac > 0.0f) {
				if(lar->type==LA_SPOT) {
					float inpr;
					
					if(lar->mode & LA_SQUARE) {
						if(lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2]>0.0f) {
							float lvrot[3], x;
							
							/* rotate view to lampspace */
							VECCOPY(lvrot, lv);
							mul_m3_v3(lar->imat, lvrot);
							
							x= MAX2(fabs(lvrot[0]/lvrot[2]) , fabs(lvrot[1]/lvrot[2]));
							/* 1.0f/(sqrt(1+x*x)) is equivalent to cos(atan(x)) */
							
							inpr= 1.0f/(sqrt(1.0f+x*x));
						}
						else inpr= 0.0f;
					}
					else {
						inpr= lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2];
					}
					
					t= lar->spotsi;
					if(inpr<=t) 
						visifac= 0.0f;
					else {
						t= inpr-t;
						if(t<lar->spotbl && lar->spotbl!=0.0f) {
							/* soft area */
							float i= t/lar->spotbl;
							t= i*i;
							inpr*= (3.0f*t-2.0f*t*i);
						}
						visifac*= inpr;
					}
				}
			}
		}
		if (visifac <= 0.001) visifac = 0.0f;
		return visifac;
	}
}

/* function returns raw diff, spec and full shadowed diff in the 'shad' pass */
static void shade_one_light(LampRen *lar, ShadeInput *shi, ShadeResult *shr, int passflag)
{
	Material *ma= shi->mat;
	VlakRen *vlr= shi->vlr;
	float lv[3], lampdist, lacol[3], shadfac[4], lashdw[3];
	float i, is, i_noshad, inp, *vn, *view, vnor[3], phongcorr=1.0f;
	float visifac;
	
	vn= shi->vn;
	view= shi->view;
	
	
	if (lar->energy == 0.0) return;
	/* only shadow lamps shouldn't affect shadow-less materials at all */
	if ((lar->mode & LA_ONLYSHADOW) && (!(ma->mode & MA_SHADOW) || !(R.r.mode & R_SHADOW)))
		return;
	/* optimisation, don't render fully black lamps */
	if (!(lar->mode & LA_TEXTURE) && (lar->r + lar->g + lar->b == 0.0f))
		return;
	
	/* lampdist, spot angle, area side, ... */
	visifac= lamp_get_visibility(lar, shi->co, lv, &lampdist);
	if(visifac==0.0f)
		return;
	
	if(lar->type==LA_SPOT) {
		if(lar->mode & LA_OSATEX) {
			shi->osatex= 1;	/* signal for multitex() */
			
			shi->dxlv[0]= lv[0] - (shi->co[0]-lar->co[0]+shi->dxco[0])/lampdist;
			shi->dxlv[1]= lv[1] - (shi->co[1]-lar->co[1]+shi->dxco[1])/lampdist;
			shi->dxlv[2]= lv[2] - (shi->co[2]-lar->co[2]+shi->dxco[2])/lampdist;
			
			shi->dylv[0]= lv[0] - (shi->co[0]-lar->co[0]+shi->dyco[0])/lampdist;
			shi->dylv[1]= lv[1] - (shi->co[1]-lar->co[1]+shi->dyco[1])/lampdist;
			shi->dylv[2]= lv[2] - (shi->co[2]-lar->co[2]+shi->dyco[2])/lampdist;
		}
	}
	
	/* lamp color texture */
	lacol[0]= lar->r;
	lacol[1]= lar->g;
	lacol[2]= lar->b;
	
	lashdw[0]= lar->shdwr;
	lashdw[1]= lar->shdwg;
	lashdw[2]= lar->shdwb;
	
	if(lar->mode & LA_TEXTURE)	do_lamp_tex(lar, lv, shi, lacol, LA_TEXTURE);
	if(lar->mode & LA_SHAD_TEX)	do_lamp_tex(lar, lv, shi, lashdw, LA_SHAD_TEX);

		/* tangent case; calculate fake face normal, aligned with lampvector */	
		/* note, vnor==vn is used as tangent trigger for buffer shadow */
	if(vlr->flag & R_TANGENT) {
		float cross[3], nstrand[3], blend;

		if(ma->mode & MA_STR_SURFDIFF) {
			cross_v3_v3v3(cross, shi->surfnor, vn);
			cross_v3_v3v3(nstrand, vn, cross);

			blend= INPR(nstrand, shi->surfnor);
			blend= 1.0f - blend;
			CLAMP(blend, 0.0f, 1.0f);

			interp_v3_v3v3(vnor, nstrand, shi->surfnor, blend);
			normalize_v3(vnor);
		}
		else {
			cross_v3_v3v3(cross, lv, vn);
			cross_v3_v3v3(vnor, cross, vn);
			normalize_v3(vnor);
		}

		if(ma->strand_surfnor > 0.0f) {
			if(ma->strand_surfnor > shi->surfdist) {
				blend= (ma->strand_surfnor - shi->surfdist)/ma->strand_surfnor;
				interp_v3_v3v3(vnor, vnor, shi->surfnor, blend);
				normalize_v3(vnor);
			}
		}

		vnor[0]= -vnor[0];vnor[1]= -vnor[1];vnor[2]= -vnor[2];
		vn= vnor;
	}
	else if (ma->mode & MA_TANGENT_V) {
		float cross[3];
		cross_v3_v3v3(cross, lv, shi->tang);
		cross_v3_v3v3(vnor, cross, shi->tang);
		normalize_v3(vnor);
		vnor[0]= -vnor[0];vnor[1]= -vnor[1];vnor[2]= -vnor[2];
		vn= vnor;
	}
	
	/* dot product and reflectivity */
	/* inp = dotproduct, is = shader result, i = lamp energy (with shadow), i_noshad = i without shadow */
	inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];
	
	/* phong threshold to prevent backfacing faces having artefacts on ray shadow (terminator problem) */
	/* this complex construction screams for a nicer implementation! (ton) */
	if(R.r.mode & R_SHADOW) {
		if(ma->mode & MA_SHADOW) {
			if(lar->type==LA_HEMI || lar->type==LA_AREA);
			else if((ma->mode & MA_RAYBIAS) && (lar->mode & LA_SHAD_RAY) && (vlr->flag & R_SMOOTH)) {
				float thresh= shi->obr->ob->smoothresh;
				if(inp>thresh)
					phongcorr= (inp-thresh)/(inp*(1.0f-thresh));
				else
					phongcorr= 0.0f;
			}
			else if(ma->sbias!=0.0f && ((lar->mode & LA_SHAD_RAY) || lar->shb)) {
				if(inp>ma->sbias)
					phongcorr= (inp-ma->sbias)/(inp*(1.0f-ma->sbias));
				else
					phongcorr= 0.0f;
			}
		}
	}
	
	/* diffuse shaders */
	if(lar->mode & LA_NO_DIFF) {
		is= 0.0f;	// skip shaders
	}
	else if(lar->type==LA_HEMI) {
		is= 0.5f*inp + 0.5f;
	}
	else {
		
		if(lar->type==LA_AREA)
			inp= area_lamp_energy_multisample(lar, shi->co, vn);
		
		/* diffuse shaders (oren nayer gets inp from area light) */
		if(ma->diff_shader==MA_DIFF_ORENNAYAR) is= OrenNayar_Diff(inp, vn, lv, view, ma->roughness);
		else if(ma->diff_shader==MA_DIFF_TOON) is= Toon_Diff(vn, lv, view, ma->param[0], ma->param[1]);
		else if(ma->diff_shader==MA_DIFF_MINNAERT) is= Minnaert_Diff(inp, vn, view, ma->darkness);
		else if(ma->diff_shader==MA_DIFF_FRESNEL) is= Fresnel_Diff(vn, lv, view, ma->param[0], ma->param[1]);
		else is= inp;	// Lambert
	}
	
	/* 'is' is diffuse */
	if((ma->shade_flag & MA_CUBIC) && is>0.0f && is<1.0f)
		is= 3.0*is*is - 2.0*is*is*is;	// nicer termination of shades

	i= is*phongcorr;
	
	if(i>0.0f) {
		i*= visifac*shi->refl;
	}
	i_noshad= i;
	
	vn= shi->vn;	// bring back original vector, we use special specular shaders for tangent
	if(ma->mode & MA_TANGENT_V)
		vn= shi->tang;
	
	/* init transp shadow */
	shadfac[0]= shadfac[1]= shadfac[2]= shadfac[3]= 1.0f;
	
	/* shadow and spec, (visifac==0 outside spot) */
	if(visifac> 0.0f) {
		
		if((R.r.mode & R_SHADOW)) {
			if(ma->mode & MA_SHADOW) {
				if(lar->shb || (lar->mode & LA_SHAD_RAY)) {
					
					if(vn==vnor)	/* tangent trigger */
						lamp_get_shadow(lar, shi, INPR(shi->vn, lv), shadfac, shi->depth);
					else
						lamp_get_shadow(lar, shi, inp, shadfac, shi->depth);
						
					/* warning, here it skips the loop */
					if((lar->mode & LA_ONLYSHADOW) && i>0.0) {
						
						shadfac[3]= i*lar->energy*(1.0f-shadfac[3]);
						shr->shad[0] -= shadfac[3]*shi->r*(1.0f-lashdw[0]);
						shr->shad[1] -= shadfac[3]*shi->g*(1.0f-lashdw[1]);
						shr->shad[2] -= shadfac[3]*shi->b*(1.0f-lashdw[2]);
						
						shr->spec[0] -= shadfac[3]*shi->specr*(1.0f-lashdw[0]);
						shr->spec[1] -= shadfac[3]*shi->specg*(1.0f-lashdw[1]);
						shr->spec[2] -= shadfac[3]*shi->specb*(1.0f-lashdw[2]);
						
						return;
					}
					
					i*= shadfac[3];
				}
			}
		}
		
		/* in case 'no diffuse' we still do most calculus, spec can be in shadow.*/
		if(!(lar->mode & LA_NO_DIFF)) {
			if(i>0.0f) {
				if(ma->mode & MA_SHADOW_TRA)
					add_to_diffuse(shr->shad, shi, is, i*shadfac[0]*lacol[0], i*shadfac[1]*lacol[1], i*shadfac[2]*lacol[2]);
				else
					add_to_diffuse(shr->shad, shi, is, i*lacol[0], i*lacol[1], i*lacol[2]);
			}
			/* add light for colored shadow */
			if (i_noshad>i && !(lashdw[0]==0 && lashdw[1]==0 && lashdw[2]==0)) {
				add_to_diffuse(shr->shad, shi, is, lashdw[0]*(i_noshad-i)*lacol[0], lashdw[1]*(i_noshad-i)*lacol[1], lashdw[2]*(i_noshad-i)*lacol[2]);
			}
			if(i_noshad>0.0f) {
				if(passflag & (SCE_PASS_DIFFUSE|SCE_PASS_SHADOW)) {
					if(ma->mode & MA_SHADOW_TRA)
						add_to_diffuse(shr->diff, shi, is, i_noshad*shadfac[0]*lacol[0], i_noshad*shadfac[1]*lacol[1], i_noshad*shadfac[2]*lacol[2]);
					else
						add_to_diffuse(shr->diff, shi, is, i_noshad*lacol[0], i_noshad*lacol[1], i_noshad*lacol[2]);
				}
				else
					VECCOPY(shr->diff, shr->shad);
			}
		}
		
		/* specularity */
		shadfac[3]*= phongcorr;	/* note, shadfac not allowed to be stored nonlocal */
		
		if(shadfac[3]>0.0f && shi->spec!=0.0f && !(lar->mode & LA_NO_SPEC) && !(lar->mode & LA_ONLYSHADOW)) {
			
			if(!(passflag & (SCE_PASS_COMBINED|SCE_PASS_SPEC)));
			else if(lar->type==LA_HEMI) {
				float t;
				/* hemi uses no spec shaders (yet) */
				
				lv[0]+= view[0];
				lv[1]+= view[1];
				lv[2]+= view[2];
				
				normalize_v3(lv);
				
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
				
				t= shadfac[3]*shi->spec*visifac*specfac;
				
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

static void shade_lamp_loop_only_shadow(ShadeInput *shi, ShadeResult *shr)
{
	
	if(R.r.mode & R_SHADOW) {
		ListBase *lights;
		LampRen *lar;
		GroupObject *go;
		float inpr, lv[3];
		float *view, shadfac[4];
		float ir, accum, visifac, lampdist;
		

		view= shi->view;

		accum= ir= 0.0f;
		
		lights= get_lights(shi);
		for(go=lights->first; go; go= go->next) {
			lar= go->lampren;
			if(lar==NULL) continue;
			
			/* yafray: ignore shading by photonlights, not used in Blender */
			if (lar->type==LA_YF_PHOTON) continue;
			
			if(lar->mode & LA_LAYER) if((lar->lay & shi->obi->lay)==0) continue;
			if((lar->lay & shi->lay)==0) continue;
			
			if(lar->shb || (lar->mode & LA_SHAD_RAY)) {
				visifac= lamp_get_visibility(lar, shi->co, lv, &lampdist);
				if(visifac <= 0.0f) {
					ir+= 1.0f;
					accum+= 1.0f;
					continue;
				}
				inpr= INPR(shi->vn, lv);
				if(inpr <= 0.0f) {
					ir+= 1.0f;
					accum+= 1.0f;
					continue;
				}				
				lamp_get_shadow(lar, shi, inpr, shadfac, shi->depth);

				ir+= 1.0f;
				accum+= (1.0f-visifac) + (visifac)*rgb_to_grayscale(shadfac)*shadfac[3];
			}
		}
		if(ir>0.0f) {
			accum/= ir;
			shr->alpha= (shi->mat->alpha)*(1.0f-accum);
		}
		else shr->alpha= shi->mat->alpha;
	}
	
	/* quite disputable this...  also note it doesn't mirror-raytrace */	
	if((R.wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT)) && shi->amb!=0.0f) {
		float f;
		
		if(R.wrld.mode & WO_AMB_OCC) {
			f= R.wrld.aoenergy*shi->amb;

			if(R.wrld.aomix==WO_AOADD) {
				f= f*(1.0f - rgb_to_grayscale(shi->ao));
				shr->alpha= (shr->alpha + f)*f;
			}
			else
				shr->alpha= (1.0f - f)*shr->alpha + f*(1.0f - (1.0f - shr->alpha)*rgb_to_grayscale(shi->ao));
		}

		if(R.wrld.mode & WO_ENV_LIGHT) {
			f= R.wrld.ao_env_energy*shi->amb*(1.0f - rgb_to_grayscale(shi->env));
			shr->alpha= (shr->alpha + f)*f;
		}
	}
}

/* let's map negative light as if it mirrors positive light, otherwise negative values disappear */
static void wrld_exposure_correct(float *diff)
{
	
	diff[0]= R.wrld.linfac*(1.0f-exp( diff[0]*R.wrld.logfac) );
	diff[1]= R.wrld.linfac*(1.0f-exp( diff[1]*R.wrld.logfac) );
	diff[2]= R.wrld.linfac*(1.0f-exp( diff[2]*R.wrld.logfac) );
}

void shade_lamp_loop(ShadeInput *shi, ShadeResult *shr)
{
	Material *ma= shi->mat;
	int passflag= shi->passflag;
	
	memset(shr, 0, sizeof(ShadeResult));
	
	/* separate loop */
	if(ma->mode & MA_ONLYSHADOW) {
		shade_lamp_loop_only_shadow(shi, shr);
		return;
	}
	
	/* envmap hack, always reset */
	shi->refcol[0]= shi->refcol[1]= shi->refcol[2]= shi->refcol[3]= 0.0f;
	
	/* material color itself */
	if(passflag & (SCE_PASS_COMBINED|SCE_PASS_RGBA)) {
		if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
			shi->r= shi->vcol[0];
			shi->g= shi->vcol[1];
			shi->b= shi->vcol[2];
			if(ma->mode & (MA_FACETEXTURE_ALPHA))
				shi->alpha= shi->vcol[3];
		}
		if(ma->texco)
			do_material_tex(shi);
		
		shr->col[0]= shi->r*shi->alpha;
		shr->col[1]= shi->g*shi->alpha;
		shr->col[2]= shi->b*shi->alpha;
		shr->col[3]= shi->alpha;

		if((ma->sss_flag & MA_DIFF_SSS) && !sss_pass_done(&R, ma)) {
			if(ma->sss_texfac == 0.0f) {
				shi->r= shi->g= shi->b= shi->alpha= 1.0f;
				shr->col[0]= shr->col[1]= shr->col[2]= shr->col[3]= 1.0f;
			}
			else {
				shi->r= pow(shi->r, ma->sss_texfac);
				shi->g= pow(shi->g, ma->sss_texfac);
				shi->b= pow(shi->b, ma->sss_texfac);
				shi->alpha= pow(shi->alpha, ma->sss_texfac);
				
				shr->col[0]= pow(shr->col[0], ma->sss_texfac);
				shr->col[1]= pow(shr->col[1], ma->sss_texfac);
				shr->col[2]= pow(shr->col[2], ma->sss_texfac);
				shr->col[3]= pow(shr->col[3], ma->sss_texfac);
			}
		}
	}
	
	if(ma->mode & MA_SHLESS) {
		shr->combined[0]= shi->r;
		shr->combined[1]= shi->g;
		shr->combined[2]= shi->b;
		shr->alpha= shi->alpha;
		return;
	}

	if( (ma->mode & (MA_VERTEXCOL|MA_VERTEXCOLP))== MA_VERTEXCOL ) {	// vertexcolor light
		shr->emit[0]= shi->r*(shi->emit+shi->vcol[0]);
		shr->emit[1]= shi->g*(shi->emit+shi->vcol[1]);
		shr->emit[2]= shi->b*(shi->emit+shi->vcol[2]);
	}
	else {
		shr->emit[0]= shi->r*shi->emit;
		shr->emit[1]= shi->g*shi->emit;
		shr->emit[2]= shi->b*shi->emit;
	}
	
	/* AO pass */
	if(R.wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT)) {
		if(((passflag & SCE_PASS_COMBINED) && (shi->combinedflag & (SCE_PASS_AO|SCE_PASS_ENVIRONMENT|SCE_PASS_INDIRECT)))
			|| (passflag & (SCE_PASS_AO|SCE_PASS_ENVIRONMENT|SCE_PASS_INDIRECT))) {
			/* AO was calculated for scanline already */
			if(shi->depth)
				ambient_occlusion(shi);
			VECCOPY(shr->ao, shi->ao);
			VECCOPY(shr->env, shi->env); // XXX multiply
			VECCOPY(shr->indirect, shi->indirect); // XXX multiply
		}
	}
	
	/* lighting pass */
	if(passflag & (SCE_PASS_COMBINED|SCE_PASS_DIFFUSE|SCE_PASS_SPEC|SCE_PASS_SHADOW)) {
		GroupObject *go;
		ListBase *lights;
		LampRen *lar;
		
		lights= get_lights(shi);
		for(go=lights->first; go; go= go->next) {
			lar= go->lampren;
			if(lar==NULL) continue;
			
			/* yafray: ignore shading by photonlights, not used in Blender */
			if (lar->type==LA_YF_PHOTON) continue;
			
			/* test for lamp layer */
			if(lar->mode & LA_LAYER) if((lar->lay & shi->obi->lay)==0) continue;
			if((lar->lay & shi->lay)==0) continue;
			
			/* accumulates in shr->diff and shr->spec and shr->shad (diffuse with shadow!) */
			shade_one_light(lar, shi, shr, passflag);
		}

		/*this check is to prevent only shadow lamps from producing negative
		  colors.*/
		if (shr->spec[0] < 0) shr->spec[0] = 0;
		if (shr->spec[1] < 0) shr->spec[1] = 0;
		if (shr->spec[2] < 0) shr->spec[2] = 0;

		if (shr->shad[0] < 0) shr->shad[0] = 0;
		if (shr->shad[1] < 0) shr->shad[1] = 0;
		if (shr->shad[2] < 0) shr->shad[2] = 0;
						
		if(ma->sss_flag & MA_DIFF_SSS) {
			float sss[3], col[3], invalpha, texfac= ma->sss_texfac;

			/* this will return false in the preprocess stage */
			if(sample_sss(&R, ma, shi->co, sss)) {
				invalpha= (shr->col[3] > FLT_EPSILON)? 1.0f/shr->col[3]: 1.0f;

				if(texfac==0.0f) {
					VECCOPY(col, shr->col);
					mul_v3_fl(col, invalpha);
				}
				else if(texfac==1.0f) {
					col[0]= col[1]= col[2]= 1.0f;
					mul_v3_fl(col, invalpha);
				}
				else {
					VECCOPY(col, shr->col);
					mul_v3_fl(col, invalpha);
					col[0]= pow(col[0], 1.0f-texfac);
					col[1]= pow(col[1], 1.0f-texfac);
					col[2]= pow(col[2], 1.0f-texfac);
				}

				shr->diff[0]= sss[0]*col[0];
				shr->diff[1]= sss[1]*col[1];
				shr->diff[2]= sss[2]*col[2];

				if(shi->combinedflag & SCE_PASS_SHADOW)	{
					shr->shad[0]= shr->diff[0];
					shr->shad[1]= shr->diff[1];
					shr->shad[2]= shr->diff[2];
				}
			}
		}
		
		if(shi->combinedflag & SCE_PASS_SHADOW)	
			VECCOPY(shr->combined, shr->shad) 	/* note, no ';' ! */
		else
			VECCOPY(shr->combined, shr->diff);
			
		/* calculate shadow pass, we use a multiplication mask */
		if(passflag & SCE_PASS_SHADOW) {
			if(shr->diff[0]!=0.0f) shr->shad[0]= shr->shad[0]/shr->diff[0];
			if(shr->diff[1]!=0.0f) shr->shad[1]= shr->shad[1]/shr->diff[1];
			if(shr->diff[2]!=0.0f) shr->shad[2]= shr->shad[2]/shr->diff[2];
		}
		
		/* exposure correction */
		if((R.wrld.exp!=0.0f || R.wrld.range!=1.0f) && !R.sss_points) {
			wrld_exposure_correct(shr->combined);	/* has no spec! */
			wrld_exposure_correct(shr->spec);
		}
	}
	
	/* alpha in end, spec can influence it */
	if(passflag & (SCE_PASS_COMBINED)) {
		if(ma->fresnel_tra!=0.0f) 
			shi->alpha*= fresnel_fac(shi->view, shi->vn, ma->fresnel_tra_i, ma->fresnel_tra);
			
		/* note: shi->mode! */
		if(shi->mode & MA_TRANSP) {
			if(shi->spectra!=0.0f) {
				float t = MAX3(shr->spec[0], shr->spec[1], shr->spec[2]);
				t *= shi->spectra;
				if(t>1.0f) t= 1.0f;
				shi->alpha= (1.0f-t)*shi->alpha+t;
			}
		}
	}
	shr->alpha= shi->alpha;
	
	/* from now stuff everything in shr->combined: ambient, AO, radio, ramps, exposure */
	if(!(ma->sss_flag & MA_DIFF_SSS) || !sss_pass_done(&R, ma)) {
		/* add AO in combined? */
		if(R.wrld.mode & WO_AMB_OCC)
			if(shi->combinedflag & SCE_PASS_AO)
				ambient_occlusion_apply(shi, shr);

		if(R.wrld.mode & WO_ENV_LIGHT)
			if(shi->combinedflag & SCE_PASS_ENVIRONMENT)
				environment_lighting_apply(shi, shr);

		if(R.wrld.mode & WO_INDIRECT_LIGHT)
			if(shi->combinedflag & SCE_PASS_INDIRECT)
				indirect_lighting_apply(shi, shr);
		
		shr->combined[0]+= shi->ambr;
		shr->combined[1]+= shi->ambg;
		shr->combined[2]+= shi->ambb;
		
		if(ma->mode & MA_RAMP_COL) ramp_diffuse_result(shr->combined, shi);
	}

	if(ma->mode & MA_RAMP_SPEC) ramp_spec_result(shr->spec, shr->spec+1, shr->spec+2, shi);
	
	/* refcol is for envmap only */
	if(shi->refcol[0]!=0.0f) {
		float result[3];
		
		result[0]= shi->mirr*shi->refcol[1] + (1.0f - shi->mirr*shi->refcol[0])*shr->combined[0];
		result[1]= shi->mirg*shi->refcol[2] + (1.0f - shi->mirg*shi->refcol[0])*shr->combined[1];
		result[2]= shi->mirb*shi->refcol[3] + (1.0f - shi->mirb*shi->refcol[0])*shr->combined[2];
		
		if(passflag & SCE_PASS_REFLECT)
			VECSUB(shr->refl, result, shr->combined);
		
		if(shi->combinedflag & SCE_PASS_REFLECT)
			VECCOPY(shr->combined, result);
			
	}
	
	/* and add emit and spec */
	if(shi->combinedflag & SCE_PASS_EMIT)
		VECADD(shr->combined, shr->combined, shr->emit);
	if(shi->combinedflag & SCE_PASS_SPEC)
		VECADD(shr->combined, shr->combined, shr->spec);
	
	/* modulate by the object color */
	if((ma->shade_flag & MA_OBCOLOR) && shi->obr->ob) {
		if(!(ma->sss_flag & MA_DIFF_SSS) || !sss_pass_done(&R, ma)) {
			float obcol[4];

			QUATCOPY(obcol, shi->obr->ob->col);
			CLAMP(obcol[3], 0.0f, 1.0f);

			shr->combined[0] *= obcol[0];
			shr->combined[1] *= obcol[1];
			shr->combined[2] *= obcol[2];
			shr->alpha *= obcol[3];
		}
	}

	shr->combined[3]= shr->alpha;
}

