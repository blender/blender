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
 * The Original Code is Copyright (C) 2006 Blender Foundation
 * All rights reserved.
 *
 * Contributors: Hos, Robert Wenzlaff.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/shadeoutput.c
 *  \ingroup render
 */


#include <stdio.h>
#include <float.h>
#include <math.h>
#include <string.h>

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_colortools.h"
#include "BKE_material.h"
#include "BKE_texture.h"


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

#include "shading.h" /* own include */

/* could enable at some point but for now there are far too many conversions */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

ListBase *get_lights(ShadeInput *shi)
{
	
	if (R.r.scemode & R_BUTS_PREVIEW)
		return &R.lights;
	if (shi->light_override)
		return &shi->light_override->gobject;
	if (shi->mat && shi->mat->group)
		return &shi->mat->group->gobject;
	
	return &R.lights;
}

#if 0
static void fogcolor(const float colf[3], float *rco, float *view)
{
	float alpha, stepsize, startdist, dist, hor[4], zen[3], vec[3], dview[3];
	float div=0.0f, distfac;
	
	hor[0]= R.wrld.horr; hor[1]= R.wrld.horg; hor[2]= R.wrld.horb;
	zen[0]= R.wrld.zenr; zen[1]= R.wrld.zeng; zen[2]= R.wrld.zenb;
	
	copy_v3_v3(vec, rco);
	
	/* we loop from cur coord to mist start in steps */
	stepsize= 1.0f;
	
	div= ABS(view[2]);
	dview[0]= view[0]/(stepsize*div);
	dview[1]= view[1]/(stepsize*div);
	dview[2]= -stepsize;

	startdist= -rco[2] + BLI_frand();
	for (dist= startdist; dist>R.wrld.miststa; dist-= stepsize) {
		
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
		
		sub_v3_v3(vec, dview);
	}
}
#endif

/* zcor is distance, co the 3d coordinate in eye space, return alpha */
float mistfactor(float zcor, float const co[3])
{
	float fac, hi;
	
	fac = zcor - R.wrld.miststa;	/* zcor is calculated per pixel */

	/* fac= -co[2]-R.wrld.miststa; */

	if (fac > 0.0f) {
		if (fac < R.wrld.mistdist) {
			
			fac = (fac / R.wrld.mistdist);
			
			if (R.wrld.mistype == 0) {
				fac *= fac;
			}
			else if (R.wrld.mistype == 1) {
				/* pass */
			}
			else {
				fac = sqrtf(fac);
			}
		}
		else {
			fac = 1.0f;
		}
	}
	else {
		fac = 0.0f;
	}
	
	/* height switched off mist */
	if (R.wrld.misthi!=0.0f && fac!=0.0f) {
		/* at height misthi the mist is completely gone */

		hi = R.viewinv[0][2] * co[0] +
		     R.viewinv[1][2] * co[1] +
		     R.viewinv[2][2] * co[2] +
		     R.viewinv[3][2];
		
		if (hi > R.wrld.misthi) {
			fac = 0.0f;
		}
		else if (hi>0.0f) {
			hi= (R.wrld.misthi-hi)/R.wrld.misthi;
			fac*= hi*hi;
		}
	}

	return (1.0f-fac)* (1.0f-R.wrld.misi);
}

static void spothalo(struct LampRen *lar, ShadeInput *shi, float *intens)
{
	double a, b, c, disc, nray[3], npos[3];
	double t0, t1 = 0.0f, t2= 0.0f, t3;
	float p1[3], p2[3], ladist, maxz = 0.0f, maxy = 0.0f, haint;
	int cuts;
	bool do_clip = true, use_yco = false;

	*intens= 0.0f;
	haint= lar->haint;
	
	if (R.r.mode & R_ORTHO) {
		/* camera pos (view vector) cannot be used... */
		/* camera position (cox,coy,0) rotate around lamp */
		p1[0]= shi->co[0]-lar->co[0];
		p1[1]= shi->co[1]-lar->co[1];
		p1[2]= -lar->co[2];
		mul_m3_v3(lar->imat, p1);
		copy_v3db_v3fl(npos, p1);  /* npos is double! */
		
		/* pre-scale */
		npos[2] *= (double)lar->sh_zfac;
	}
	else {
		copy_v3db_v3fl(npos, lar->sh_invcampos);	/* in initlamp calculated */
	}
	
	/* rotate view */
	copy_v3db_v3fl(nray, shi->view);
	mul_m3_v3_double(lar->imat, nray);
	
	if (R.wrld.mode & WO_MIST) {
		/* patchy... */
		haint *= mistfactor(-lar->co[2], lar->co);
		if (haint==0.0f) {
			return;
		}
	}


	/* rotate maxz */
	if (shi->co[2]==0.0f) {
		do_clip = false;  /* for when halo at sky */
	}
	else {
		p1[0]= shi->co[0]-lar->co[0];
		p1[1]= shi->co[1]-lar->co[1];
		p1[2]= shi->co[2]-lar->co[2];
	
		maxz= lar->imat[0][2]*p1[0]+lar->imat[1][2]*p1[1]+lar->imat[2][2]*p1[2];
		maxz*= lar->sh_zfac;
		maxy= lar->imat[0][1]*p1[0]+lar->imat[1][1]*p1[1]+lar->imat[2][1]*p1[2];

		if (fabs(nray[2]) < FLT_EPSILON) {
			use_yco = true;
		}
	}
	
	/* scale z to make sure volume is normalized */
	nray[2] *= (double)lar->sh_zfac;
	/* nray does not need normalization */
	
	ladist= lar->sh_zfac*lar->dist;
	
	/* solve */
	a = nray[0] * nray[0] + nray[1] * nray[1] - nray[2]*nray[2];
	b = nray[0] * npos[0] + nray[1] * npos[1] - nray[2]*npos[2];
	c = npos[0] * npos[0] + npos[1] * npos[1] - npos[2]*npos[2];

	cuts= 0;
	if (fabs(a) < DBL_EPSILON) {
		/*
		 * Only one intersection point...
		 */
		return;
	}
	else {
		disc = b*b - a*c;
		
		if (disc==0.0) {
			t1=t2= (-b)/ a;
			cuts= 2;
		}
		else if (disc > 0.0) {
			disc = sqrt(disc);
			t1 = (-b + disc) / a;
			t2 = (-b - disc) / a;
			cuts= 2;
		}
	}
	if (cuts==2) {
		int ok1=0, ok2=0;

		/* sort */
		if (t1>t2) {
			a= t1; t1= t2; t2= a;
		}

		/* z of intersection points with diabolo */
		p1[2]= npos[2] + t1*nray[2];
		p2[2]= npos[2] + t2*nray[2];

		/* evaluate both points */
		if (p1[2]<=0.0f) ok1= 1;
		if (p2[2]<=0.0f && t1!=t2) ok2= 1;
		
		/* at least 1 point with negative z */
		if (ok1==0 && ok2==0) return;
		
		/* intersction point with -ladist, the bottom of the cone */
		if (use_yco == false) {
			t3= ((double)(-ladist)-npos[2])/nray[2];
				
			/* de we have to replace one of the intersection points? */
			if (ok1) {
				if (p1[2]<-ladist) t1= t3;
			}
			else {
				t1= t3;
			}
			if (ok2) {
				if (p2[2]<-ladist) t2= t3;
			}
			else {
				t2= t3;
			}
		}
		else if (ok1==0 || ok2==0) return;
		
		/* at least 1 visible interesction point */
		if (t1<0.0 && t2<0.0) return;
		
		if (t1<0.0) t1= 0.0;
		if (t2<0.0) t2= 0.0;
		
		if (t1==t2) return;
		
		/* sort again to be sure */
		if (t1>t2) {
			a= t1; t1= t2; t2= a;
		}
		
		/* calculate t0: is the maximum visible z (when halo is intersected by face) */ 
		if (do_clip) {
			if (use_yco == false) t0 = ((double)maxz - npos[2]) / nray[2];
			else                  t0 = ((double)maxy - npos[1]) / nray[1];

			if (t0 < t1) return;
			if (t0 < t2) t2= t0;
		}

		/* calc points */
		p1[0]= npos[0] + t1*nray[0];
		p1[1]= npos[1] + t1*nray[1];
		p1[2]= npos[2] + t1*nray[2];
		p2[0]= npos[0] + t2*nray[0];
		p2[1]= npos[1] + t2*nray[1];
		p2[2]= npos[2] + t2*nray[2];
		
			
		/* now we have 2 points, make three lengths with it */
		
		a = len_v3(p1);
		b = len_v3(p2);
		c = len_v3v3(p1, p2);
		
		a/= ladist;
		a= sqrt(a);
		b/= ladist; 
		b= sqrt(b);
		c/= ladist;
		
		*intens= c*( (1.0-a)+(1.0-b) );

		/* WATCH IT: do not clip a,b en c at 1.0, this gives nasty little overflows
		 * at the edges (especially with narrow halos) */
		if (*intens<=0.0f) return;

		/* soft area */
		/* not needed because t0 has been used for p1/p2 as well */
		/* if (doclip && t0<t2) { */
		/* 	*intens *= (t0-t1)/(t2-t1); */
		/* } */
		
		*intens *= haint;
		
		if (lar->shb && lar->shb->shadhalostep) {
			*intens *= shadow_halo(lar, p1, p2);
		}
		
	}
}

void renderspothalo(ShadeInput *shi, float col[4], float alpha)
{
	ListBase *lights;
	GroupObject *go;
	LampRen *lar;
	float i;
	
	if (alpha==0.0f) return;
	
	lights= get_lights(shi);
	for (go=lights->first; go; go= go->next) {
		lar= go->lampren;
		if (lar==NULL) continue;
		
		if (lar->type==LA_SPOT && (lar->mode & LA_HALO) && (lar->buftype != LA_SHADBUF_DEEP) && lar->haint>0) {
			
			if (lar->mode & LA_LAYER) 
				if (shi->vlr && (lar->lay & shi->obi->lay)==0) 
					continue;
			if ((lar->lay & shi->lay)==0) 
				continue;
			
			spothalo(lar, shi, &i);
			if (i > 0.0f) {
				const float i_alpha = i * alpha;
				col[0] += i_alpha * lar->r;
				col[1] += i_alpha * lar->g;
				col[2] += i_alpha * lar->b;
				col[3] += i_alpha;  /* all premul */
			}
		}
	}
	/* clip alpha, is needed for unified 'alpha threshold' (vanillaRenderPipe.c) */
	if (col[3]>1.0f) col[3]= 1.0f;
}



/* ---------------- shaders ----------------------- */

static double Normalize_d(double *n)
{
	double d;
	
	d= n[0]*n[0]+n[1]*n[1]+n[2]*n[2];

	if (d>0.00000000000000001) {
		d= sqrt(d);

		n[0]/=d; 
		n[1]/=d; 
		n[2]/=d;
	}
	else {
		n[0]=n[1]=n[2]= 0.0;
		d= 0.0;
	}
	return d;
}

/* mix of 'real' fresnel and allowing control. grad defines blending gradient */
float fresnel_fac(const float view[3], const float vn[3], float grad, float fac)
{
	float t1, t2;
	
	if (fac==0.0f) return 1.0f;
	
	t1 = dot_v3v3(view, vn);
	if (t1>0.0f)  t2= 1.0f+t1;
	else t2= 1.0f-t1;
	
	t2= grad + (1.0f-grad)*powf(t2, fac);
	
	if (t2<0.0f) return 0.0f;
	else if (t2>1.0f) return 1.0f;
	return t2;
}

static double saacos_d(double fac)
{
	if (fac<= -1.0) return M_PI;
	else if (fac>=1.0) return 0.0;
	else return acos(fac);
}

/* Stoke's form factor. Need doubles here for extreme small area sizes */
static float area_lamp_energy(float (*area)[3], const float co[3], const float vn[3])
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
#define CROSS(dest, a, b) \
	{ \
		dest[0]= a[1] * b[2] - a[2] * b[1]; \
		dest[1]= a[2] * b[0] - a[0] * b[2]; \
		dest[2]= a[0] * b[1] - a[1] * b[0]; \
	} (void)0

	CROSS(cross[0], vec[0], vec[1]);
	CROSS(cross[1], vec[1], vec[2]);
	CROSS(cross[2], vec[2], vec[3]);
	CROSS(cross[3], vec[3], vec[0]);

#undef CROSS

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

	if (fac<=0.0) return 0.0;
	return fac;
}

static float area_lamp_energy_multisample(LampRen *lar, const float co[3], float *vn)
{
	/* corner vectors are moved around according lamp jitter */
	float *jitlamp= lar->jitter, vec[3];
	float area[4][3], intens= 0.0f;
	int a= lar->ray_totsamp;

	/* test if co is behind lamp */
	sub_v3_v3v3(vec, co, lar->co);
	if (dot_v3v3(vec, lar->vec) < 0.0f)
		return 0.0f;

	while (a--) {
		vec[0]= jitlamp[0];
		vec[1]= jitlamp[1];
		vec[2]= 0.0f;
		mul_m3_v3(lar->mat, vec);
		
		add_v3_v3v3(area[0], lar->area[0], vec);
		add_v3_v3v3(area[1], lar->area[1], vec);
		add_v3_v3v3(area[2], lar->area[2], vec);
		add_v3_v3v3(area[3], lar->area[3], vec);
		
		intens+= area_lamp_energy(area, co, vn);
		
		jitlamp+= 2;
	}
	intens /= (float)lar->ray_totsamp;
	
	return pow(intens * lar->areasize, lar->k);	/* corrected for buttons size and lar->dist^2 */
}

static float spec(float inp, int hard)	
{
	float b1;
	
	if (inp>=1.0f) return 1.0f;
	else if (inp<=0.0f) return 0.0f;
	
	b1= inp*inp;
	/* avoid FPE */
	if (b1<0.01f) b1= 0.01f;
	
	if ((hard & 1)==0)  inp= 1.0f;
	if (hard & 2)  inp*= b1;
	b1*= b1;
	if (hard & 4)  inp*= b1;
	b1*= b1;
	if (hard & 8)  inp*= b1;
	b1*= b1;
	if (hard & 16) inp*= b1;
	b1*= b1;

	/* avoid FPE */
	if (b1<0.001f) b1= 0.0f;

	if (hard & 32) inp*= b1;
	b1*= b1;
	if (hard & 64) inp*=b1;
	b1*= b1;
	if (hard & 128) inp*=b1;

	if (b1<0.001f) b1= 0.0f;

	if (hard & 256) {
		b1*= b1;
		inp*=b1;
	}

	return inp;
}

static float Phong_Spec(const float n[3], const float l[3], const float v[3], int hard, int tangent )
{
	float h[3];
	float rslt;
	
	h[0] = l[0] + v[0];
	h[1] = l[1] + v[1];
	h[2] = l[2] + v[2];
	normalize_v3(h);
	
	rslt = h[0]*n[0] + h[1]*n[1] + h[2]*n[2];
	if (tangent) rslt= sasqrt(1.0f - rslt*rslt);
		
	if ( rslt > 0.0f ) rslt= spec(rslt, hard);
	else rslt = 0.0f;
	
	return rslt;
}


/* reduced cook torrance spec (for off-specular peak) */
static float CookTorr_Spec(const float n[3], const float l[3], const float v[3], int hard, int tangent)
{
	float i, nh, nv, h[3];

	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	normalize_v3(h);

	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2];
	if (tangent) nh= sasqrt(1.0f - nh*nh);
	else if (nh<0.0f) return 0.0f;
	
	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2];
	if (tangent) nv= sasqrt(1.0f - nv*nv);
	else if (nv<0.0f) nv= 0.0f;

	i= spec(nh, hard);

	i= i/(0.1f+nv);
	return i;
}

/* Blinn spec */
static float Blinn_Spec(const float n[3], const float l[3], const float v[3], float refrac, float spec_power, int tangent)
{
	float i, nh, nv, nl, vh, h[3];
	float a, b, c, g=0.0f, p, f, ang;

	if (refrac < 1.0f) return 0.0f;
	if (spec_power == 0.0f) return 0.0f;
	
	/* conversion from 'hardness' (1-255) to 'spec_power' (50 maps at 0.1) */
	if (spec_power<100.0f)
		spec_power = sqrtf(1.0f / spec_power);
	else spec_power= 10.0f/spec_power;
	
	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	normalize_v3(h);

	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if (tangent) nh= sasqrt(1.0f - nh*nh);
	else if (nh<0.0f) return 0.0f;

	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if (tangent) nv= sasqrt(1.0f - nv*nv);
	if (nv<=0.01f) nv= 0.01f;				/* hrms... */

	nl= n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if (tangent) nl= sasqrt(1.0f - nl*nl);
	if (nl<=0.01f) {
		return 0.0f;
	}

	vh= v[0]*h[0]+v[1]*h[1]+v[2]*h[2]; /* Dot product between view vector and half-way vector */
	if (vh<=0.0f) vh= 0.01f;

	a = 1.0f;
	b = (2.0f*nh*nv)/vh;
	c = (2.0f*nh*nl)/vh;

	if ( a < b && a < c ) g = a;
	else if ( b < a && b < c ) g = b;
	else if ( c < a && c < b ) g = c;

	p = sqrt((double)((refrac * refrac)+(vh * vh) - 1.0f));
	f = (((p-vh)*(p-vh))/((p+vh)*(p+vh)))*(1+((((vh*(p+vh))-1.0f)*((vh*(p+vh))-1.0f))/(((vh*(p-vh))+1.0f)*((vh*(p-vh))+1.0f))));
	ang = saacos(nh);

	i= f * g * exp((double)(-(ang*ang) / (2.0f*spec_power*spec_power)));
	if (i<0.0f) i= 0.0f;
	
	return i;
}

/* cartoon render spec */
static float Toon_Spec(const float n[3], const float l[3], const float v[3], float size, float smooth, int tangent)
{
	float h[3];
	float ang;
	float rslt;
	
	h[0] = l[0] + v[0];
	h[1] = l[1] + v[1];
	h[2] = l[2] + v[2];
	normalize_v3(h);
	
	rslt = h[0]*n[0] + h[1]*n[1] + h[2]*n[2];
	if (tangent) rslt = sasqrt(1.0f - rslt*rslt);
	
	ang = saacos( rslt ); 
	
	if ( ang < size ) rslt = 1.0f;
	else if ( ang >= (size + smooth) || smooth == 0.0f ) rslt = 0.0f;
	else rslt = 1.0f - ((ang - size) / smooth);
	
	return rslt;
}

/* Ward isotropic gaussian spec */
static float WardIso_Spec(const float n[3], const float l[3], const float v[3], float rms, int tangent)
{
	float i, nh, nv, nl, h[3], angle, alpha;


	/* half-way vector */
	h[0] = l[0] + v[0];
	h[1] = l[1] + v[1];
	h[2] = l[2] + v[2];
	normalize_v3(h);

	nh = n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if (tangent) nh = sasqrt(1.0f - nh*nh);
	if (nh<=0.0f) nh = 0.001f;
	
	nv = n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if (tangent) nv = sasqrt(1.0f - nv*nv);
	if (nv<=0.0f) nv = 0.001f;

	nl = n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if (tangent) nl = sasqrt(1.0f - nl*nl);
	if (nl<=0.0f) nl = 0.001f;

	angle = tanf(saacos(nh));
	alpha = MAX2(rms, 0.001f);

	i= nl * (1.0f/(4.0f*(float)M_PI*alpha*alpha)) * (expf( -(angle*angle)/(alpha*alpha))/(sqrtf(nv*nl)));

	return i;
}

/* cartoon render diffuse */
static float Toon_Diff(const float n[3], const float l[3], const float UNUSED(v[3]), float size, float smooth)
{
	float rslt, ang;

	rslt = n[0]*l[0] + n[1]*l[1] + n[2]*l[2];

	ang = saacos(rslt);

	if ( ang < size ) rslt = 1.0f;
	else if ( ang >= (size + smooth) || smooth == 0.0f ) rslt = 0.0f;
	else rslt = 1.0f - ((ang - size) / smooth);

	return rslt;
}

/* Oren Nayar diffuse */

/* 'nl' is either dot product, or return value of area light */
/* in latter case, only last multiplication uses 'nl' */
static float OrenNayar_Diff(float nl, const float n[3], const float l[3], const float v[3], float rough )
{
	float i/*, nh*/, nv /*, vh */, realnl, h[3];
	float a, b, t, A, B;
	float Lit_A, View_A, Lit_B[3], View_B[3];
	
	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	normalize_v3(h);
	
	/* nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; */ /* Dot product between surface normal and half-way vector */
	/* if (nh<0.0f) nh = 0.0f; */
	
	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if (nv<=0.0f) nv= 0.0f;
	
	realnl= n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if (realnl<=0.0f) return 0.0f;
	if (nl<0.0f) return 0.0f;		/* value from area light */
	
	/* vh= v[0]*h[0]+v[1]*h[1]+v[2]*h[2]; */ /* Dot product between view vector and halfway vector */
	/* if (vh<=0.0f) vh= 0.0f; */
	
	Lit_A = saacos(realnl);
	View_A = saacos( nv );
	
	Lit_B[0] = l[0] - (realnl * n[0]);
	Lit_B[1] = l[1] - (realnl * n[1]);
	Lit_B[2] = l[2] - (realnl * n[2]);
	normalize_v3(Lit_B);
	
	View_B[0] = v[0] - (nv * n[0]);
	View_B[1] = v[1] - (nv * n[1]);
	View_B[2] = v[2] - (nv * n[2]);
	normalize_v3(View_B);
	
	t = Lit_B[0]*View_B[0] + Lit_B[1]*View_B[1] + Lit_B[2]*View_B[2];
	if ( t < 0 ) t = 0;
	
	if ( Lit_A > View_A ) {
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
	i = nl * ( A + ( B * t * sinf(a) * tanf(b) ) );
	
	return i;
}

/* Minnaert diffuse */
static float Minnaert_Diff(float nl, const float n[3], const float v[3], float darkness)
{
	float i, nv;

	/* nl = dot product between surface normal and light vector */
	if (nl <= 0.0f)
		return 0.0f;

	/* nv = dot product between surface normal and view vector */
	nv = dot_v3v3(n, v);
	if (nv < 0.0f)
		nv = 0.0f;

	if (darkness <= 1.0f)
		i = nl * pow(max_ff(nv * nl, 0.1f), (darkness - 1.0f) ); /*The Real model*/
	else
		i = nl * pow( (1.001f - nv), (darkness  - 1.0f) ); /*Nvidia model*/

	return i;
}

static float Fresnel_Diff(float *vn, float *lv, float *UNUSED(view), float fac_i, float fac)
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
	if (shi->osatex) {
		if (shi->vlr->flag & R_SMOOTH) {
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

	if (ma->mode & (MA_FACETEXTURE)) {
		shi->r= shi->vcol[0];
		shi->g= shi->vcol[1];
		shi->b= shi->vcol[2];
		if (ma->mode & (MA_FACETEXTURE_ALPHA))
			shi->alpha= shi->vcol[3];
	}
	else if (ma->mode & (MA_VERTEXCOLP)) {
		float neg_alpha = 1.0f - shi->vcol[3];
		shi->r= shi->r*neg_alpha + shi->vcol[0]*shi->vcol[3];
		shi->g= shi->g*neg_alpha + shi->vcol[1]*shi->vcol[3];
		shi->b= shi->b*neg_alpha + shi->vcol[2]*shi->vcol[3];
	}
	
	if (ma->texco)
		do_material_tex(shi, &R);

	if (ma->fresnel_tra!=0.0f) 
		shi->alpha*= fresnel_fac(shi->view, shi->vn, ma->fresnel_tra_i, ma->fresnel_tra);
	
	if (!(shi->mode & MA_TRANSP)) shi->alpha= 1.0f;
	
	shr->diff[0]= shi->r;
	shr->diff[1]= shi->g;
	shr->diff[2]= shi->b;
	shr->alpha= shi->alpha;

	/* modulate by the object color */
	if ((ma->shade_flag & MA_OBCOLOR) && shi->obr->ob) {
		float obcol[4];

		copy_v4_v4(obcol, shi->obr->ob->col);
		CLAMP(obcol[3], 0.0f, 1.0f);

		shr->diff[0] *= obcol[0];
		shr->diff[1] *= obcol[1];
		shr->diff[2] *= obcol[2];
		if (shi->mode & MA_TRANSP) shr->alpha *= obcol[3];
	}

	copy_v3_v3(shr->diffshad, shr->diff);
}

/* ramp for at end of shade */
static void ramp_diffuse_result(float *diff, ShadeInput *shi)
{
	Material *ma= shi->mat;
	float col[4];

	if (ma->ramp_col) {
		if (ma->rampin_col==MA_RAMP_IN_RESULT) {
			float fac = rgb_to_grayscale(diff);
			do_colorband(ma->ramp_col, fac, col);
			
			/* blending method */
			fac= col[3]*ma->rampfac_col;
			
			ramp_blend(ma->rampblend_col, diff, fac, col);
		}
	}
}

/* r,g,b denote energy, ramp is used with different values to make new material color */
static void add_to_diffuse(float *diff, ShadeInput *shi, float is, float r, float g, float b)
{
	Material *ma= shi->mat;

	if (ma->ramp_col && (ma->mode & MA_RAMP_COL)) {
		
		/* MA_RAMP_IN_RESULT is exceptional */
		if (ma->rampin_col==MA_RAMP_IN_RESULT) {
			/* normal add */
			diff[0] += r * shi->r;
			diff[1] += g * shi->g;
			diff[2] += b * shi->b;
		}
		else {
			float colt[3], col[4];
			float fac;

			/* input */
			switch (ma->rampin_col) {
			case MA_RAMP_IN_ENERGY:
				/* should use 'rgb_to_grayscale' but we only have a vector version */
				fac= 0.3f*r + 0.58f*g + 0.12f*b;
				break;
			case MA_RAMP_IN_SHADER:
				fac= is;
				break;
			case MA_RAMP_IN_NOR:
				fac= shi->view[0]*shi->vn[0] + shi->view[1]*shi->vn[1] + shi->view[2]*shi->vn[2];
				break;
			default:
				fac= 0.0f;
				break;
			}
	
			do_colorband(ma->ramp_col, fac, col);
			
			/* blending method */
			fac= col[3]*ma->rampfac_col;
			colt[0]= shi->r;
			colt[1]= shi->g;
			colt[2]= shi->b;

			ramp_blend(ma->rampblend_col, colt, fac, col);

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

static void ramp_spec_result(float spec_col[3], ShadeInput *shi)
{
	Material *ma= shi->mat;

	if (ma->ramp_spec && (ma->rampin_spec==MA_RAMP_IN_RESULT)) {
		float col[4];
		float fac = rgb_to_grayscale(spec_col);

		do_colorband(ma->ramp_spec, fac, col);
		
		/* blending method */
		fac= col[3]*ma->rampfac_spec;
		
		ramp_blend(ma->rampblend_spec, spec_col, fac, col);
		
	}
}

/* is = dot product shade, t = spec energy */
static void do_specular_ramp(ShadeInput *shi, float is, float t, float spec[3])
{
	Material *ma= shi->mat;

	spec[0]= shi->specr;
	spec[1]= shi->specg;
	spec[2]= shi->specb;

	/* MA_RAMP_IN_RESULT is exception */
	if (ma->ramp_spec && (ma->rampin_spec!=MA_RAMP_IN_RESULT)) {
		float fac;
		float col[4];

		/* input */
		switch (ma->rampin_spec) {
		case MA_RAMP_IN_ENERGY:
			fac= t;
			break;
		case MA_RAMP_IN_SHADER:
			fac= is;
			break;
		case MA_RAMP_IN_NOR:
			fac= shi->view[0]*shi->vn[0] + shi->view[1]*shi->vn[1] + shi->view[2]*shi->vn[2];
			break;
		default:
			fac= 0.0f;
			break;
		}
		
		do_colorband(ma->ramp_spec, fac, col);
		
		/* blending method */
		fac= col[3]*ma->rampfac_spec;
		
		ramp_blend(ma->rampblend_spec, spec, fac, col);
	}
}

/* pure AO, check for raytrace and world should have been done */
/* preprocess, textures were not done, don't use shi->amb for that reason */
void ambient_occlusion(ShadeInput *shi)
{
	if ((R.wrld.ao_gather_method == WO_AOGATHER_APPROX) && shi->mat->amb!=0.0f) {
		sample_occ(&R, shi);
	}
	else if ((R.r.mode & R_RAYTRACE) && shi->mat->amb!=0.0f) {
		ray_ao(shi, shi->ao, shi->env);
	}
	else {
		shi->ao[0]= shi->ao[1]= shi->ao[2]= 1.0f;
		zero_v3(shi->env);
		zero_v3(shi->indirect);
	}
}


/* wrld mode was checked for */
static void ambient_occlusion_apply(ShadeInput *shi, ShadeResult *shr)
{
	float f= R.wrld.aoenergy;
	float tmp[3], tmpspec[3];

	if (!((R.r.mode & R_RAYTRACE) || R.wrld.ao_gather_method == WO_AOGATHER_APPROX))
		return;
	if (f == 0.0f)
		return;

	if (R.wrld.aomix==WO_AOADD) {
		shr->combined[0] += shi->ao[0]*shi->r*shi->refl*f;
		shr->combined[1] += shi->ao[1]*shi->g*shi->refl*f;
		shr->combined[2] += shi->ao[2]*shi->b*shi->refl*f;
	}
	else if (R.wrld.aomix==WO_AOMUL) {
		mul_v3_v3v3(tmp, shr->combined, shi->ao);
		mul_v3_v3v3(tmpspec, shr->spec, shi->ao);

		if (f == 1.0f) {
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

	if (!((R.r.mode & R_RAYTRACE) || R.wrld.ao_gather_method == WO_AOGATHER_APPROX))
		return;
	if (f == 0.0f)
		return;
	
	shr->combined[0] += shi->env[0]*shi->r*shi->refl*f;
	shr->combined[1] += shi->env[1]*shi->g*shi->refl*f;
	shr->combined[2] += shi->env[2]*shi->b*shi->refl*f;
}

static void indirect_lighting_apply(ShadeInput *shi, ShadeResult *shr)
{
	float f= R.wrld.ao_indirect_energy;

	if (!((R.r.mode & R_RAYTRACE) || R.wrld.ao_gather_method == WO_AOGATHER_APPROX))
		return;
	if (f == 0.0f)
		return;

	shr->combined[0] += shi->indirect[0]*shi->r*shi->refl*f;
	shr->combined[1] += shi->indirect[1]*shi->g*shi->refl*f;
	shr->combined[2] += shi->indirect[2]*shi->b*shi->refl*f;
}

/* result written in shadfac */
void lamp_get_shadow(LampRen *lar, ShadeInput *shi, float inp, float shadfac[4], int do_real)
{
	LampShadowSubSample *lss= &(lar->shadsamp[shi->thread].s[shi->sample]);
	
	if (do_real || lss->samplenr!=shi->samplenr) {
		
		shadfac[0]= shadfac[1]= shadfac[2]= shadfac[3]= 1.0f;
		
		if (lar->shb) {
			if (lar->buftype==LA_SHADBUF_IRREGULAR)
				shadfac[3]= ISB_getshadow(shi, lar->shb);
			else
				shadfac[3] = testshadowbuf(&R, lar->shb, shi->co, shi->dxco, shi->dyco, inp, shi->mat->lbias);
		}
		else if (lar->mode & LA_SHAD_RAY) {
			ray_shadow(shi, lar, shadfac);
		}
		
		if (shi->depth==0) {
			copy_v4_v4(lss->shadfac, shadfac);
			lss->samplenr= shi->samplenr;
		}
	}
	else {
		copy_v4_v4(shadfac, lss->shadfac);
	}
}

/* lampdistance and spot angle, writes in lv and dist */
float lamp_get_visibility(LampRen *lar, const float co[3], float lv[3], float *dist)
{
	if (lar->type==LA_SUN || lar->type==LA_HEMI) {
		*dist= 1.0f;
		copy_v3_v3(lv, lar->vec);
		return 1.0f;
	}
	else {
		float visifac= 1.0f, t;
		
		sub_v3_v3v3(lv, co, lar->co);
		*dist = len_v3(lv);
		t = 1.0f / (*dist);
		mul_v3_fl(lv, t);
		
		/* area type has no quad or sphere option */
		if (lar->type==LA_AREA) {
			/* area is single sided */
			//if (dot_v3v3(lv, lar->vec) > 0.0f)
			//	visifac= 1.0f;
			//else
			//	visifac= 0.0f;
		}
		else {
			switch (lar->falloff_type) {
				case LA_FALLOFF_CONSTANT:
					visifac = 1.0f;
					break;
				case LA_FALLOFF_INVLINEAR:
					visifac = lar->dist/(lar->dist + dist[0]);
					break;
				case LA_FALLOFF_INVSQUARE:
					/* NOTE: This seems to be a hack since commit r12045 says this
					 * option is similar to old Quad, but with slight changes.
					 * Correct inv square would be (which would be old Quad):
					 * visifac = lar->distkw / (lar->distkw + dist[0]*dist[0]);
					 */
					visifac = lar->dist / (lar->dist + dist[0]*dist[0]);
					break;
				case LA_FALLOFF_SLIDERS:
					if (lar->ld1>0.0f)
						visifac= lar->dist/(lar->dist+lar->ld1*dist[0]);
					if (lar->ld2>0.0f)
						visifac*= lar->distkw/(lar->distkw+lar->ld2*dist[0]*dist[0]);
					break;
				case LA_FALLOFF_CURVE:
					/* curvemapping_initialize is called from #add_render_lamp */
					visifac = curvemapping_evaluateF(lar->curfalloff, 0, dist[0]/lar->dist);
					break;
			}
			
			if (lar->mode & LA_SPHERE) {
				float t= lar->dist - dist[0];
				if (t<=0.0f) 
					visifac= 0.0f;
				else
					visifac*= t/lar->dist;
			}
			
			if (visifac > 0.0f) {
				if (lar->type==LA_SPOT) {
					float inpr;
					
					if (lar->mode & LA_SQUARE) {
						if (dot_v3v3(lv, lar->vec) > 0.0f) {
							float lvrot[3], x;
							
							/* rotate view to lampspace */
							copy_v3_v3(lvrot, lv);
							mul_m3_v3(lar->imat, lvrot);
							
							x = max_ff(fabsf(lvrot[0]/lvrot[2]), fabsf(lvrot[1]/lvrot[2]));
							/* 1.0f/(sqrt(1+x*x)) is equivalent to cos(atan(x)) */
							
							inpr = 1.0f / (sqrtf(1.0f + x * x));
						}
						else inpr= 0.0f;
					}
					else {
						inpr= lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2];
					}
					
					t= lar->spotsi;
					if (inpr<=t) 
						visifac= 0.0f;
					else {
						t= inpr-t;
						if (t<lar->spotbl && lar->spotbl!=0.0f) {
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
		if (visifac <= 0.001f) visifac = 0.0f;
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
	
	
	if (lar->energy == 0.0f) return;
	/* only shadow lamps shouldn't affect shadow-less materials at all */
	if ((lar->mode & LA_ONLYSHADOW) && (!(ma->mode & MA_SHADOW) || !(R.r.mode & R_SHADOW)))
		return;
	/* optimization, don't render fully black lamps */
	if (!(lar->mode & LA_TEXTURE) && (lar->r + lar->g + lar->b == 0.0f))
		return;
	
	/* lampdist, spot angle, area side, ... */
	visifac= lamp_get_visibility(lar, shi->co, lv, &lampdist);
	if (visifac==0.0f)
		return;
	
	if (lar->type==LA_SPOT) {
		if (lar->mode & LA_OSATEX) {
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
	
	if (lar->mode & LA_TEXTURE)	do_lamp_tex(lar, lv, shi, lacol, LA_TEXTURE);
	if (lar->mode & LA_SHAD_TEX)	do_lamp_tex(lar, lv, shi, lashdw, LA_SHAD_TEX);

		/* tangent case; calculate fake face normal, aligned with lampvector */
		/* note, vnor==vn is used as tangent trigger for buffer shadow */
	if (vlr->flag & R_TANGENT) {
		float cross[3], nstrand[3], blend;

		if (ma->mode & MA_STR_SURFDIFF) {
			cross_v3_v3v3(cross, shi->surfnor, vn);
			cross_v3_v3v3(nstrand, vn, cross);

			blend= dot_v3v3(nstrand, shi->surfnor);
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

		if (ma->strand_surfnor > 0.0f) {
			if (ma->strand_surfnor > shi->surfdist) {
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
	inp= dot_v3v3(vn, lv);

	/* phong threshold to prevent backfacing faces having artifacts on ray shadow (terminator problem) */
	/* this complex construction screams for a nicer implementation! (ton) */
	if (R.r.mode & R_SHADOW) {
		if (ma->mode & MA_SHADOW) {
			if (lar->type == LA_HEMI || lar->type == LA_AREA) {
				/* pass */
			}
			else if ((ma->mode & MA_RAYBIAS) && (lar->mode & LA_SHAD_RAY) && (vlr->flag & R_SMOOTH)) {
				float thresh= shi->obr->ob->smoothresh;
				if (inp>thresh)
					phongcorr= (inp-thresh)/(inp*(1.0f-thresh));
				else
					phongcorr= 0.0f;
			}
			else if (ma->sbias!=0.0f && ((lar->mode & LA_SHAD_RAY) || lar->shb)) {
				if (inp>ma->sbias)
					phongcorr= (inp-ma->sbias)/(inp*(1.0f-ma->sbias));
				else
					phongcorr= 0.0f;
			}
		}
	}
	
	/* diffuse shaders */
	if (lar->mode & LA_NO_DIFF) {
		is = 0.0f;  /* skip shaders */
	}
	else if (lar->type==LA_HEMI) {
		is = 0.5f * inp + 0.5f;
	}
	else {
		
		if (lar->type==LA_AREA)
			inp= area_lamp_energy_multisample(lar, shi->co, vn);
		
		/* diffuse shaders (oren nayer gets inp from area light) */
		if (ma->diff_shader==MA_DIFF_ORENNAYAR) is= OrenNayar_Diff(inp, vn, lv, view, ma->roughness);
		else if (ma->diff_shader==MA_DIFF_TOON) is= Toon_Diff(vn, lv, view, ma->param[0], ma->param[1]);
		else if (ma->diff_shader==MA_DIFF_MINNAERT) is= Minnaert_Diff(inp, vn, view, ma->darkness);
		else if (ma->diff_shader==MA_DIFF_FRESNEL) is= Fresnel_Diff(vn, lv, view, ma->param[0], ma->param[1]);
		else is= inp;  /* Lambert */
	}

	/* 'is' is diffuse */
	if ((ma->shade_flag & MA_CUBIC) && is > 0.0f && is < 1.0f) {
		is= 3.0f * is * is - 2.0f * is * is * is;  /* nicer termination of shades */
	}

	i= is*phongcorr;
	
	if (i>0.0f) {
		i*= visifac*shi->refl;
	}
	i_noshad= i;
	
	vn = shi->vn;  /* bring back original vector, we use special specular shaders for tangent */
	if (ma->mode & MA_TANGENT_V)
		vn= shi->tang;
	
	/* init transp shadow */
	shadfac[0]= shadfac[1]= shadfac[2]= shadfac[3]= 1.0f;
	
	/* shadow and spec, (visifac==0 outside spot) */
	if (visifac> 0.0f) {
		
		if ((R.r.mode & R_SHADOW)) {
			if (ma->mode & MA_SHADOW) {
				if (lar->shb || (lar->mode & LA_SHAD_RAY)) {
					
					if (vn==vnor)	/* tangent trigger */
						lamp_get_shadow(lar, shi, dot_v3v3(shi->vn, lv), shadfac, shi->depth);
					else
						lamp_get_shadow(lar, shi, inp, shadfac, shi->depth);
						
					/* warning, here it skips the loop */
					if ((lar->mode & LA_ONLYSHADOW) && i>0.0f) {
						
						shadfac[3]= i*lar->energy*(1.0f-shadfac[3]);
						shr->shad[0] -= shadfac[3]*shi->r*(1.0f-lashdw[0]);
						shr->shad[1] -= shadfac[3]*shi->g*(1.0f-lashdw[1]);
						shr->shad[2] -= shadfac[3]*shi->b*(1.0f-lashdw[2]);
						
						if (!(lar->mode & LA_NO_SPEC)) {
							shr->spec[0] -= shadfac[3]*shi->specr*(1.0f-lashdw[0]);
							shr->spec[1] -= shadfac[3]*shi->specg*(1.0f-lashdw[1]);
							shr->spec[2] -= shadfac[3]*shi->specb*(1.0f-lashdw[2]);
						}
						
						return;
					}
					
					i*= shadfac[3];
					shr->shad[3] = shadfac[3]; /* store this for possible check in troublesome cases */
				}
			}
		}
		
		/* in case 'no diffuse' we still do most calculus, spec can be in shadow.*/
		if (!(lar->mode & LA_NO_DIFF)) {
			if (i>0.0f) {
				if (ma->mode & MA_SHADOW_TRA)
					add_to_diffuse(shr->shad, shi, is, i*shadfac[0]*lacol[0], i*shadfac[1]*lacol[1], i*shadfac[2]*lacol[2]);
				else
					add_to_diffuse(shr->shad, shi, is, i*lacol[0], i*lacol[1], i*lacol[2]);
			}
			/* add light for colored shadow */
			if (i_noshad>i && !(lashdw[0]==0 && lashdw[1]==0 && lashdw[2]==0)) {
				add_to_diffuse(shr->shad, shi, is, lashdw[0]*(i_noshad-i)*lacol[0], lashdw[1]*(i_noshad-i)*lacol[1], lashdw[2]*(i_noshad-i)*lacol[2]);
			}
			if (i_noshad>0.0f) {
				if (passflag & (SCE_PASS_DIFFUSE|SCE_PASS_SHADOW) ||
				    ((passflag & SCE_PASS_COMBINED) && !(shi->combinedflag & SCE_PASS_SHADOW)))
				{
					add_to_diffuse(shr->diff, shi, is, i_noshad*lacol[0], i_noshad*lacol[1], i_noshad*lacol[2]);
				}
				else {
					copy_v3_v3(shr->diff, shr->shad);
				}
			}
		}
		
		/* specularity */
		shadfac[3]*= phongcorr;	/* note, shadfac not allowed to be stored nonlocal */
		
		if (shadfac[3]>0.0f && shi->spec!=0.0f && !(lar->mode & LA_NO_SPEC) && !(lar->mode & LA_ONLYSHADOW)) {
			
			if (!(passflag & (SCE_PASS_COMBINED | SCE_PASS_SPEC))) {
				/* pass */
			}
			else if (lar->type == LA_HEMI) {
				float t;
				/* hemi uses no spec shaders (yet) */
				
				lv[0]+= view[0];
				lv[1]+= view[1];
				lv[2]+= view[2];
				
				normalize_v3(lv);
				
				t= vn[0]*lv[0]+vn[1]*lv[1]+vn[2]*lv[2];
				
				if (lar->type==LA_HEMI) {
					t= 0.5f*t+0.5f;
				}
				
				t= shadfac[3]*shi->spec*spec(t, shi->har);
				
				shr->spec[0]+= t*(lacol[0] * shi->specr);
				shr->spec[1]+= t*(lacol[1] * shi->specg);
				shr->spec[2]+= t*(lacol[2] * shi->specb);
			}
			else {
				/* specular shaders */
				float specfac, t;
				
				if (ma->spec_shader==MA_SPEC_PHONG) 
					specfac= Phong_Spec(vn, lv, view, shi->har, (vlr->flag & R_TANGENT) || (ma->mode & MA_TANGENT_V));
				else if (ma->spec_shader==MA_SPEC_COOKTORR) 
					specfac= CookTorr_Spec(vn, lv, view, shi->har, (vlr->flag & R_TANGENT) || (ma->mode & MA_TANGENT_V));
				else if (ma->spec_shader==MA_SPEC_BLINN) 
					specfac= Blinn_Spec(vn, lv, view, ma->refrac, (float)shi->har, (vlr->flag & R_TANGENT) || (ma->mode & MA_TANGENT_V));
				else if (ma->spec_shader==MA_SPEC_WARDISO)
					specfac= WardIso_Spec( vn, lv, view, ma->rms, (vlr->flag & R_TANGENT) || (ma->mode & MA_TANGENT_V));
				else 
					specfac= Toon_Spec(vn, lv, view, ma->param[2], ma->param[3], (vlr->flag & R_TANGENT) || (ma->mode & MA_TANGENT_V));
				
				/* area lamp correction */
				if (lar->type==LA_AREA) specfac*= inp;
				
				t= shadfac[3]*shi->spec*visifac*specfac;
				
				if (ma->mode & MA_RAMP_SPEC) {
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
	
	if (R.r.mode & R_SHADOW) {
		ListBase *lights;
		LampRen *lar;
		GroupObject *go;
		float inpr, lv[3];
		float /* *view, */ shadfac[4];
		float ir, accum, visifac, lampdist;
		float shaded = 0.0f, lightness = 0.0f;
		

		/* view= shi->view; */ /* UNUSED */
		accum= ir= 0.0f;
		
		lights= get_lights(shi);
		for (go=lights->first; go; go= go->next) {
			lar= go->lampren;
			if (lar==NULL) continue;
			
			/* yafray: ignore shading by photonlights, not used in Blender */
			if (lar->type==LA_YF_PHOTON) continue;
			
			if (lar->mode & LA_LAYER) if ((lar->lay & shi->obi->lay)==0) continue;
			if ((lar->lay & shi->lay)==0) continue;
			
			if (lar->shb || (lar->mode & LA_SHAD_RAY)) {
				visifac= lamp_get_visibility(lar, shi->co, lv, &lampdist);
				ir+= 1.0f;

				if (visifac <= 0.0f) {
					if (shi->mat->shadowonly_flag == MA_SO_OLD)
						accum+= 1.0f;

					continue;
				}
				inpr= dot_v3v3(shi->vn, lv);
				if (inpr <= 0.0f) {
					if (shi->mat->shadowonly_flag == MA_SO_OLD)
						accum+= 1.0f;

					continue;
				}

				lamp_get_shadow(lar, shi, inpr, shadfac, shi->depth);

				if (shi->mat->shadowonly_flag == MA_SO_OLD) {
					/* Old "Shadows Only" */
					accum+= (1.0f-visifac) + (visifac)*rgb_to_grayscale(shadfac)*shadfac[3];
				}
				else {
					shaded += rgb_to_grayscale(shadfac)*shadfac[3] * visifac * lar->energy;

					if (shi->mat->shadowonly_flag == MA_SO_SHADOW) {
						lightness += visifac * lar->energy;
					}
				}
			}
		}

		/* Apply shadows as alpha */
		if (ir>0.0f) {
			if (shi->mat->shadowonly_flag == MA_SO_OLD) {
				accum = 1.0f - accum/ir;
			}
			else {
				if (shi->mat->shadowonly_flag == MA_SO_SHADOW) {
					if (lightness > 0.0f) {
						/* Get shadow value from between 0.0f and non-shadowed lightness */
						accum = (lightness - shaded) / (lightness);
					}
					else {
						accum = 0.0f;
					}
				}
				else { /* shadowonly_flag == MA_SO_SHADED */
					/* Use shaded value */
					accum = 1.0f - shaded;
			}}

			shr->alpha= (shi->alpha)*(accum);
			if (shr->alpha<0.0f) shr->alpha=0.0f;
		}
		else {
			/* If "fully shaded", use full alpha even on areas that have no lights */
			if (shi->mat->shadowonly_flag == MA_SO_SHADED) shr->alpha=shi->alpha;
			else shr->alpha= 0.f;
		}
	}
	
	/* quite disputable this...  also note it doesn't mirror-raytrace */
	if ((R.wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT)) && shi->amb!=0.0f) {
		float f;
		
		if (R.wrld.mode & WO_AMB_OCC) {
			f= R.wrld.aoenergy*shi->amb;
			
			if (R.wrld.aomix==WO_AOADD) {
				if (shi->mat->shadowonly_flag == MA_SO_OLD) {
					f= f*(1.0f - rgb_to_grayscale(shi->ao));
					shr->alpha= (shr->alpha + f)*f;
				}
				else {
					shr->alpha -= f*rgb_to_grayscale(shi->ao);
					if (shr->alpha<0.0f) shr->alpha=0.0f;
				}
			}
			else /* AO Multiply */
				shr->alpha= (1.0f - f)*shr->alpha + f*(1.0f - (1.0f - shr->alpha)*rgb_to_grayscale(shi->ao));
		}

		if (R.wrld.mode & WO_ENV_LIGHT) {
			if (shi->mat->shadowonly_flag == MA_SO_OLD) {
				f= R.wrld.ao_env_energy*shi->amb*(1.0f - rgb_to_grayscale(shi->env));
				shr->alpha= (shr->alpha + f)*f;
			}
			else {
				f= R.wrld.ao_env_energy*shi->amb;
				shr->alpha -= f*rgb_to_grayscale(shi->env);
				if (shr->alpha<0.0f) shr->alpha=0.0f;
			}
		}
	}
}

/* let's map negative light as if it mirrors positive light, otherwise negative values disappear */
static void wrld_exposure_correct(float diff[3])
{
	
	diff[0]= R.wrld.linfac*(1.0f-expf( diff[0]*R.wrld.logfac) );
	diff[1]= R.wrld.linfac*(1.0f-expf( diff[1]*R.wrld.logfac) );
	diff[2]= R.wrld.linfac*(1.0f-expf( diff[2]*R.wrld.logfac) );
}

void shade_lamp_loop(ShadeInput *shi, ShadeResult *shr)
{
	/* Passes which might need to know material color.
	 *
	 * It seems to be faster to just calculate material color
	 * even if the pass doesn't really need it than trying to
	 * figure out whether color is really needed or not.
	 */
	const int color_passes =
		SCE_PASS_COMBINED | SCE_PASS_RGBA | SCE_PASS_DIFFUSE | SCE_PASS_SPEC |
		SCE_PASS_REFLECT | SCE_PASS_NORMAL | SCE_PASS_REFRACT | SCE_PASS_EMIT;

	Material *ma= shi->mat;
	int passflag= shi->passflag;

	memset(shr, 0, sizeof(ShadeResult));
	
	if (!(shi->mode & MA_TRANSP)) shi->alpha = 1.0f;
	
	/* separate loop */
	if (ma->mode & MA_ONLYSHADOW) {
		shade_lamp_loop_only_shadow(shi, shr);
		return;
	}
	
	/* envmap hack, always reset */
	shi->refcol[0]= shi->refcol[1]= shi->refcol[2]= shi->refcol[3]= 0.0f;
	
	/* material color itself */
	if (passflag & color_passes) {
		if (ma->mode & (MA_FACETEXTURE)) {
			shi->r= shi->vcol[0];
			shi->g= shi->vcol[1];
			shi->b= shi->vcol[2];
			if (ma->mode & (MA_FACETEXTURE_ALPHA))
				shi->alpha= shi->vcol[3];
		}
#ifdef WITH_FREESTYLE
		else if (ma->vcol_alpha) {
			shi->r= shi->vcol[0];
			shi->g= shi->vcol[1];
			shi->b= shi->vcol[2];
			shi->alpha= shi->vcol[3];
		}
#endif
		else if (ma->mode & (MA_VERTEXCOLP)) {
			float neg_alpha = 1.0f - shi->vcol[3];
			shi->r= shi->r*neg_alpha + shi->vcol[0]*shi->vcol[3];
			shi->g= shi->g*neg_alpha + shi->vcol[1]*shi->vcol[3];
			shi->b= shi->b*neg_alpha + shi->vcol[2]*shi->vcol[3];
		}
		if (ma->texco) {
			do_material_tex(shi, &R);
			if (!(shi->mode & MA_TRANSP)) shi->alpha = 1.0f;
		}
		
		shr->col[0]= shi->r*shi->alpha;
		shr->col[1]= shi->g*shi->alpha;
		shr->col[2]= shi->b*shi->alpha;
		shr->col[3]= shi->alpha;

		if ((ma->sss_flag & MA_DIFF_SSS) && !sss_pass_done(&R, ma)) {
			if (ma->sss_texfac == 0.0f) {
				shi->r= shi->g= shi->b= shi->alpha= 1.0f;
				shr->col[0]= shr->col[1]= shr->col[2]= shr->col[3]= 1.0f;
			}
			else {
				shi->r= pow(max_ff(shi->r, 0.0f), ma->sss_texfac);
				shi->g= pow(max_ff(shi->g, 0.0f), ma->sss_texfac);
				shi->b= pow(max_ff(shi->b, 0.0f), ma->sss_texfac);
				shi->alpha= pow(max_ff(shi->alpha, 0.0f), ma->sss_texfac);
				
				shr->col[0]= pow(max_ff(shr->col[0], 0.0f), ma->sss_texfac);
				shr->col[1]= pow(max_ff(shr->col[1], 0.0f), ma->sss_texfac);
				shr->col[2]= pow(max_ff(shr->col[2], 0.0f), ma->sss_texfac);
				shr->col[3]= pow(max_ff(shr->col[3], 0.0f), ma->sss_texfac);
			}
		}
	}
	
	if (ma->mode & MA_SHLESS) {
		shr->combined[0]= shi->r;
		shr->combined[1]= shi->g;
		shr->combined[2]= shi->b;
		shr->alpha= shi->alpha;
		return;
	}

	if ( (ma->mode & (MA_VERTEXCOL|MA_VERTEXCOLP))== MA_VERTEXCOL ) {	/* vertexcolor light */
		shr->emit[0]= shi->r*(shi->emit+shi->vcol[0]*shi->vcol[3]);
		shr->emit[1]= shi->g*(shi->emit+shi->vcol[1]*shi->vcol[3]);
		shr->emit[2]= shi->b*(shi->emit+shi->vcol[2]*shi->vcol[3]);
	}
	else {
		shr->emit[0]= shi->r*shi->emit;
		shr->emit[1]= shi->g*shi->emit;
		shr->emit[2]= shi->b*shi->emit;
	}
	
	/* AO pass */
	if (((passflag & SCE_PASS_COMBINED) && (shi->combinedflag & (SCE_PASS_AO|SCE_PASS_ENVIRONMENT|SCE_PASS_INDIRECT))) ||
	    (passflag & (SCE_PASS_AO|SCE_PASS_ENVIRONMENT|SCE_PASS_INDIRECT))) {
		if ((R.wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT)) && (R.r.mode & R_SHADOW)) {
			/* AO was calculated for scanline already */
			if (shi->depth || shi->volume_depth)
				ambient_occlusion(shi);
			copy_v3_v3(shr->ao, shi->ao);
			copy_v3_v3(shr->env, shi->env); /* XXX multiply */
			copy_v3_v3(shr->indirect, shi->indirect); /* XXX multiply */
		}
		else {
			shr->ao[0]= shr->ao[1]= shr->ao[2]= 1.0f;
			zero_v3(shr->env);
			zero_v3(shr->indirect);
		}
	}
	
	/* lighting pass */
	if (passflag & (SCE_PASS_COMBINED|SCE_PASS_DIFFUSE|SCE_PASS_SPEC|SCE_PASS_SHADOW)) {
		GroupObject *go;
		ListBase *lights;
		LampRen *lar;
		
		lights= get_lights(shi);
		for (go=lights->first; go; go= go->next) {
			lar= go->lampren;
			if (lar==NULL) continue;
			
			/* yafray: ignore shading by photonlights, not used in Blender */
			if (lar->type==LA_YF_PHOTON) continue;
			
			/* test for lamp layer */
			if (lar->mode & LA_LAYER) if ((lar->lay & shi->obi->lay)==0) continue;
			if ((lar->lay & shi->lay)==0) continue;
			
			/* accumulates in shr->diff and shr->spec and shr->shad (diffuse with shadow!) */
			shade_one_light(lar, shi, shr, passflag);
		}

		/* this check is to prevent only shadow lamps from producing negative
		 * colors.*/
		if (shr->spec[0] < 0) shr->spec[0] = 0;
		if (shr->spec[1] < 0) shr->spec[1] = 0;
		if (shr->spec[2] < 0) shr->spec[2] = 0;

		if (shr->shad[0] < 0) shr->shad[0] = 0;
		if (shr->shad[1] < 0) shr->shad[1] = 0;
		if (shr->shad[2] < 0) shr->shad[2] = 0;
						
		if (ma->sss_flag & MA_DIFF_SSS) {
			float sss[3], col[3], invalpha, texfac= ma->sss_texfac;

			/* this will return false in the preprocess stage */
			if (sample_sss(&R, ma, shi->co, sss)) {
				invalpha= (shr->col[3] > FLT_EPSILON)? 1.0f/shr->col[3]: 1.0f;

				if (texfac==0.0f) {
					copy_v3_v3(col, shr->col);
					mul_v3_fl(col, invalpha);
				}
				else if (texfac==1.0f) {
					col[0]= col[1]= col[2]= 1.0f;
					mul_v3_fl(col, invalpha);
				}
				else {
					copy_v3_v3(col, shr->col);
					mul_v3_fl(col, invalpha);
					col[0]= pow(max_ff(col[0], 0.0f), 1.0f-texfac);
					col[1]= pow(max_ff(col[1], 0.0f), 1.0f-texfac);
					col[2]= pow(max_ff(col[2], 0.0f), 1.0f-texfac);
				}

				shr->diff[0]= sss[0]*col[0];
				shr->diff[1]= sss[1]*col[1];
				shr->diff[2]= sss[2]*col[2];

				if (shi->combinedflag & SCE_PASS_SHADOW) {
					shr->shad[0]= shr->diff[0];
					shr->shad[1]= shr->diff[1];
					shr->shad[2]= shr->diff[2];
				}
			}
		}
		
		if (shi->combinedflag & SCE_PASS_SHADOW)
			copy_v3_v3(shr->diffshad, shr->shad);
		else
			copy_v3_v3(shr->diffshad, shr->diff);

		copy_v3_v3(shr->combined, shr->diffshad);
			
		/* calculate shadow pass, we use a multiplication mask */
		/* if diff = 0,0,0 it doesn't matter what the shadow pass is, so leave it as is */
		if (passflag & SCE_PASS_SHADOW && !(shr->diff[0]==0.0f && shr->diff[1]==0.0f && shr->diff[2]==0.0f)) {
			if (shr->diff[0]!=0.0f) shr->shad[0]= shr->shad[0]/shr->diff[0];
			/* can't determine proper shadow from shad/diff (0/0), so use shadow intensity */
			else if (shr->shad[0]==0.0f) shr->shad[0]= shr->shad[3];

			if (shr->diff[1]!=0.0f) shr->shad[1]= shr->shad[1]/shr->diff[1];
			else if (shr->shad[1]==0.0f) shr->shad[1]= shr->shad[3];

			if (shr->diff[2]!=0.0f) shr->shad[2]= shr->shad[2]/shr->diff[2];
			else if (shr->shad[2]==0.0f) shr->shad[2]= shr->shad[3];
		}
		
		/* exposure correction */
		if ((R.wrld.exp!=0.0f || R.wrld.range!=1.0f) && !R.sss_points) {
			wrld_exposure_correct(shr->combined);	/* has no spec! */
			wrld_exposure_correct(shr->spec);
		}
	}
	
	/* alpha in end, spec can influence it */
	if (passflag & (SCE_PASS_COMBINED)) {
		if ((ma->fresnel_tra!=0.0f) && (shi->mode & MA_TRANSP))
			shi->alpha*= fresnel_fac(shi->view, shi->vn, ma->fresnel_tra_i, ma->fresnel_tra);
			
		/* note: shi->mode! */
		if (shi->mode & MA_TRANSP && (shi->mode & (MA_ZTRANSP|MA_RAYTRANSP))) {
			if (shi->spectra!=0.0f) {
				float t = max_fff(shr->spec[0], shr->spec[1], shr->spec[2]);
				t *= shi->spectra;
				if (t>1.0f) t= 1.0f;
				shi->alpha= (1.0f-t)*shi->alpha+t;
			}
		}
	}
	shr->alpha= shi->alpha;
	
	/* from now stuff everything in shr->combined: ambient, AO, ramps, exposure */
	if (!(ma->sss_flag & MA_DIFF_SSS) || !sss_pass_done(&R, ma)) {
		if (R.r.mode & R_SHADOW) {
			/* add AO in combined? */
			if (R.wrld.mode & WO_AMB_OCC)
				if (shi->combinedflag & SCE_PASS_AO)
					ambient_occlusion_apply(shi, shr);

			if (R.wrld.mode & WO_ENV_LIGHT)
				if (shi->combinedflag & SCE_PASS_ENVIRONMENT)
					environment_lighting_apply(shi, shr);

			if (R.wrld.mode & WO_INDIRECT_LIGHT)
				if (shi->combinedflag & SCE_PASS_INDIRECT)
					indirect_lighting_apply(shi, shr);
		}
		
		shr->combined[0]+= shi->ambr;
		shr->combined[1]+= shi->ambg;
		shr->combined[2]+= shi->ambb;
		
		if (ma->mode & MA_RAMP_COL) ramp_diffuse_result(shr->combined, shi);
	}

	if (ma->mode & MA_RAMP_SPEC) ramp_spec_result(shr->spec, shi);
	
	/* refcol is for envmap only */
	if (shi->refcol[0]!=0.0f) {
		float result[3];
		
		result[0]= shi->mirr*shi->refcol[1] + (1.0f - shi->mirr*shi->refcol[0])*shr->combined[0];
		result[1]= shi->mirg*shi->refcol[2] + (1.0f - shi->mirg*shi->refcol[0])*shr->combined[1];
		result[2]= shi->mirb*shi->refcol[3] + (1.0f - shi->mirb*shi->refcol[0])*shr->combined[2];
		
		if (passflag & SCE_PASS_REFLECT)
			sub_v3_v3v3(shr->refl, result, shr->combined);
		
		if (shi->combinedflag & SCE_PASS_REFLECT)
			copy_v3_v3(shr->combined, result);
			
	}
	
	/* and add emit and spec */
	if (shi->combinedflag & SCE_PASS_EMIT)
		add_v3_v3(shr->combined, shr->emit);
	if (shi->combinedflag & SCE_PASS_SPEC)
		add_v3_v3(shr->combined, shr->spec);
	
	/* modulate by the object color */
	if ((ma->shade_flag & MA_OBCOLOR) && shi->obr->ob) {
		if (!(ma->sss_flag & MA_DIFF_SSS) || !sss_pass_done(&R, ma)) {
			float obcol[4];

			copy_v4_v4(obcol, shi->obr->ob->col);
			CLAMP(obcol[3], 0.0f, 1.0f);

			shr->combined[0] *= obcol[0];
			shr->combined[1] *= obcol[1];
			shr->combined[2] *= obcol[2];
			if (shi->mode & MA_TRANSP) shr->alpha *= obcol[3];
		}
	}

	shr->combined[3]= shr->alpha;
}

/* used for "Lamp Data" shader node */
static float lamp_get_data_internal(ShadeInput *shi, GroupObject *go, float col[4], float lv[3], float *dist, float shadow[4])
{
	LampRen *lar = go->lampren;
	float visifac, inp;

	if (!lar || lar->type == LA_YF_PHOTON
	    || ((lar->mode & LA_LAYER) && (lar->lay & shi->obi->lay) == 0)
	    || (lar->lay & shi->lay) == 0)
		return 0.0f;

	if (lar->mode & LA_TEXTURE)
		do_lamp_tex(lar, lv, shi, col, LA_TEXTURE);

	visifac = lamp_get_visibility(lar, shi->co, lv, dist);

	if (visifac == 0.0f
	    || lar->type == LA_HEMI
	    || (lar->type != LA_SPOT && !(lar->mode & LA_SHAD_RAY))
	    || (R.r.scemode & R_BUTS_PREVIEW))
		return visifac;

	inp = dot_v3v3(shi->vn, lv);

	if (inp > 0.0f) {
		float shadfac[4];

		shadow[0] = lar->shdwr;
		shadow[1] = lar->shdwg;
		shadow[2] = lar->shdwb;

		if (lar->mode & LA_SHAD_TEX)
			do_lamp_tex(lar, lv, shi, shadow, LA_SHAD_TEX);

		lamp_get_shadow(lar, shi, inp, shadfac, shi->depth);

		shadow[0] = 1.0f - ((1.0f - shadfac[0] * shadfac[3]) * (1.0f - shadow[0]));
		shadow[1] = 1.0f - ((1.0f - shadfac[1] * shadfac[3]) * (1.0f - shadow[1]));
		shadow[2] = 1.0f - ((1.0f - shadfac[2] * shadfac[3]) * (1.0f - shadow[2]));
	}

	return visifac;
}

float RE_lamp_get_data(ShadeInput *shi, Object *lamp_obj, float col[4], float lv[3], float *dist, float shadow[4])
{
	col[0] = col[1] = col[2] = 0.0f;
	col[3] = 1.0f;
	copy_v3_v3(lv, shi->vn);
	*dist = 1.0f;
	shadow[0] = shadow[1] = shadow[2] = shadow[3] = 1.0f;

	if (lamp_obj->type == OB_LAMP) {
		GroupObject *go;
		Lamp *lamp = (Lamp *)lamp_obj->data;

		col[0] = lamp->r * lamp->energy;
		col[1] = lamp->g * lamp->energy;
		col[2] = lamp->b * lamp->energy;

		if (R.r.scemode & R_BUTS_PREVIEW) {
			for (go = R.lights.first; go; go = go->next) {
				/* "Lamp.002" is main key light of material preview */
				if (strcmp(go->ob->id.name + 2, "Lamp.002") == 0)
					return lamp_get_data_internal(shi, go, col, lv, dist, shadow);
			}
			return 0.0f;
		}

		if (shi->light_override) {
			for (go = shi->light_override->gobject.first; go; go = go->next) {
				if (go->ob == lamp_obj)
					return lamp_get_data_internal(shi, go, col, lv, dist, shadow);
			}
		}

		if (shi->mat && shi->mat->group) {
			for (go = shi->mat->group->gobject.first; go; go = go->next) {
				if (go->ob == lamp_obj)
					return lamp_get_data_internal(shi, go, col, lv, dist, shadow);
			}
		}

		for (go = R.lights.first; go; go = go->next) {
			if (go->ob == lamp_obj)
				return lamp_get_data_internal(shi, go, col, lv, dist, shadow);
		}
	}

	return 0.0f;
}
