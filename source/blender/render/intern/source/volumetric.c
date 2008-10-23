/**
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
 * Contributor(s): Matt Ebb, Raul Hernandez.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"
#include "BLI_kdtree.h"

#include "PIL_time.h"

#include "RE_shader_ext.h"
#include "RE_raytrace.h"

#include "DNA_material_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "render_types.h"
#include "renderdatabase.h"
#include "pixelshading.h"
#include "shading.h"
#include "texture.h"

#if defined( _MSC_VER ) && !defined( __cplusplus )
# define inline __inline
#endif // defined( _MSC_VER ) && !defined( __cplusplus )

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static int vol_backface_intersect_check(Isect *is, int ob, RayFace *face)
{
	VlakRen *vlr = (VlakRen *)face;
	
	/* only consider faces away, so overlapping layers
	 * of foward facing geometry don't cause the ray to stop */
	return (INPR(is->vec, vlr->n) < 0.0f);
}

static int vol_frontface_intersect_check(Isect *is, int ob, RayFace *face)
{
	VlakRen *vlr = (VlakRen *)face;
	
	/* only consider faces away, so overlapping layers
	 * of foward facing geometry don't cause the ray to stop */
	return (INPR(is->vec, vlr->n) > 0.0f);
}

static int vol_always_intersect_check(Isect *is, int ob, RayFace *face)
{
	return 1;
}

#define VOL_IS_BACKFACE			1
#define VOL_IS_SAMEMATERIAL		2


#define VOL_BOUNDS_DEPTH	0
#define VOL_BOUNDS_SS		1

/* TODO: Box or sphere intersection types could speed things up */
static int vol_get_bounds(ShadeInput *shi, float *co, float *vec, float *hitco, Isect *isect, int intersect_type, int checkfunc)
{
	float maxsize = RE_ray_tree_max_size(R.raytree);
	int intersected=0;

	/* TODO: use object's bounding box to calculate max size */
	VECCOPY(isect->start, co);
	isect->end[0] = co[0] + vec[0] * maxsize;
	isect->end[1] = co[1] + vec[1] * maxsize;
	isect->end[2] = co[2] + vec[2] * maxsize;
	
	isect->mode= RE_RAY_MIRROR;
	isect->oborig= RAY_OBJECT_SET(&R, shi->obi);
	isect->face_last= NULL;
	isect->ob_last= 0;
	isect->lay= -1;
	
	if (intersect_type == VOL_BOUNDS_DEPTH) isect->faceorig= (RayFace*)shi->vlr;
	else if (intersect_type == VOL_BOUNDS_SS) isect->faceorig= NULL;
	
	if (checkfunc==VOL_IS_BACKFACE)
		intersected = RE_ray_tree_intersect_check(R.raytree, isect, vol_backface_intersect_check);
	else
		intersected = RE_ray_tree_intersect(R.raytree, isect);
	
	if(intersected)
	{
		float isvec[3];

		VECCOPY(isvec, isect->vec);
		hitco[0] = isect->start[0] + isect->labda*isvec[0];
		hitco[1] = isect->start[1] + isect->labda*isvec[1];
		hitco[2] = isect->start[2] + isect->labda*isvec[2];
		
		return 1;
	} else {
		return 0;
	}
}

#define STEPSIZE_VIEW	0
#define STEPSIZE_SHADE	1
float vol_get_stepsize(struct ShadeInput *shi, int context)
{
	if (shi->mat->vol_stepsize_type == MA_VOL_STEP_RANDOMIZED) {
		/* range between 0.75 and 1.25 */
		const float rnd = 0.5f * BLI_thread_frand(shi->thread) + 0.75f;
	
		if (context == STEPSIZE_VIEW)
			return shi->mat->vol_stepsize * rnd;
		else if (context == STEPSIZE_SHADE)
			return shi->mat->vol_shade_stepsize * rnd;
	}
	else {
		// MA_VOL_STEP_CONSTANT
		
		if (context == STEPSIZE_VIEW)
			return shi->mat->vol_stepsize;
		else if (context == STEPSIZE_SHADE)
			return shi->mat->vol_shade_stepsize;
	}
	
	return shi->mat->vol_stepsize;
}

float vol_get_density(struct ShadeInput *shi, float *co)
{
	float density = shi->mat->alpha;
	float density_scale = shi->mat->vol_density_scale;
	float col[3] = {0.0, 0.0, 0.0};
	
	if (shi->mat->flag & MA_IS_TEXTURED) {
		do_volume_tex(shi, co, MAP_ALPHA, col, &density);
	}
	
	return density * density_scale;
}

/* compute emission component, amount of radiance to add per segment
 * can be textured with 'emit' */
void vol_get_emission(ShadeInput *shi, float *em, float *co, float density)
{
	float emission = shi->mat->emit;
	float col[3] = {0.0, 0.0, 0.0};
	
	VECCOPY(col, &shi->mat->r);
	
	do_volume_tex(shi, co, MAP_EMIT+MAP_COL, col, &emission);
	
	em[0] = em[1] = em[2] = emission * density;
	VecMulVecf(em, em, col);
}

/* scattering multiplier, values above 1.0 are non-physical, 
 * but can be useful to tweak lighting */
void vol_get_scattering_fac(ShadeInput *shi, float *scatter_fac, float *co, float density)
{
	*scatter_fac = shi->mat->vol_scattering;
}

/* phase function - determines in which directions the light 
 * is scattered in the volume relative to incoming direction 
 * and view direction */
float vol_get_phasefunc(ShadeInput *shi, short phasefunc_type, float g, float *w, float *wp)
{
	const float costheta = Inpf(w, wp);
	
	if (phasefunc_type == MA_VOL_PH_ISOTROPIC) {
		return 1.f / (4.f * M_PI);
	}
	else if (phasefunc_type == MA_VOL_PH_MIEHAZY) {
		return (0.5f + 4.5f * powf(0.5 * (1.f + costheta), 8.f)) / (4.f*M_PI);
	}
	else if (phasefunc_type == MA_VOL_PH_MIEMURKY) {
		return (0.5f + 16.5f * powf(0.5 * (1.f + costheta), 32.f)) / (4.f*M_PI);
	}
	else if (phasefunc_type == MA_VOL_PH_RAYLEIGH) {
		return 3.f/(16.f*M_PI) * (1 + costheta * costheta);
	}
	else if (phasefunc_type == MA_VOL_PH_HG) {
		return 1.f / (4.f * M_PI) * (1.f - g*g) / powf(1.f + g*g - 2.f * g * costheta, 1.5f);
	}
	else if (phasefunc_type == MA_VOL_PH_SCHLICK) {
		const float k = 1.55f * g - .55f * g * g * g;
		const float kcostheta = k * costheta;
		return 1.f / (4.f * M_PI) * (1.f - k*k) / ((1.f - kcostheta) * (1.f - kcostheta));
	} else {
		return 1.0f;
	}
}

void vol_get_absorption(ShadeInput *shi, float *absorb_col, float *co)
{
	float dummy = 1.0f;
	float absorption = shi->mat->vol_absorption;
	
	VECCOPY(absorb_col, shi->mat->vol_absorption_col);
	
	if (shi->mat->flag & MA_IS_TEXTURED)
		do_volume_tex(shi, co, MAP_COLMIR, absorb_col, &dummy);
	
	absorb_col[0] = (1.0f - absorb_col[0]) * absorption;
	absorb_col[1] = (1.0f - absorb_col[1]) * absorption;
	absorb_col[2] = (1.0f - absorb_col[2]) * absorption;
}


static float D(ShadeInput *shi, int rgb, int x, int y, int z)
{
	const int res = shi->mat->vol_precache_resolution;
	CLAMP(x, 0, res-1);
	CLAMP(y, 0, res-1);
	CLAMP(z, 0, res-1);
	return shi->obi->volume_precache[rgb*res*res*res + x*res*res + y*res + z];
}

inline float lerp(float t, float v1, float v2) {
	return (1.f - t) * v1 + t * v2;
}

inline float do_lerp(float t, float a, float b) {
	if (a > 0.f && b > 0.f) return lerp(t, a, b);
	else if (a < 0.f) 		return b;
	else if (b < 0.f) 		return a;
}

/* trilinear interpolation */
static void vol_get_precached_scattering(ShadeInput *shi, float *scatter_col, float *co)
{
	const int res = shi->mat->vol_precache_resolution;
	float voxx, voxy, voxz;
	int vx, vy, vz;
	float dx, dy, dz;
	float d00, d10, d01, d11, d0, d1, d_final;
	float bbmin[3], bbmax[3], dim[3];
	int rgb;
	
	if (!shi->obi->volume_precache) return;
	
	VECCOPY(bbmin, shi->obi->obr->boundbox[0]);
	VECCOPY(bbmax, shi->obi->obr->boundbox[1]);
	VecSubf(dim, bbmax, bbmin);
	
	voxx = ((co[0] - bbmin[0]) / dim[0]) * res - 0.5f;
	voxy = ((co[1] - bbmin[1]) / dim[1]) * res - 0.5f;
	voxz = ((co[2] - bbmin[2]) / dim[2]) * res - 0.5f;
	
	vx = (int)voxx; vy = (int)voxy; vz = (int)voxz;
	
	dx = voxx - vx; dy = voxy - vy; dz = voxz - vz;
	
	for (rgb=0; rgb < 3; rgb++) {
		d00 = lerp(dx, D(shi, rgb, vx, vy, vz), 		D(shi, rgb, vx+1, vy, vz));
		d10 = lerp(dx, D(shi, rgb, vx, vy+1, vz), 		D(shi, rgb, vx+1, vy+1, vz));
		d01 = lerp(dx, D(shi, rgb, vx, vy, vz+1), 		D(shi, rgb, vx+1, vy, vz+1));
		d11 = lerp(dx, D(shi, rgb, vx, vy+1, vz+1), 	D(shi, rgb, vx+1, vy+1, vz+1));
		d0 = lerp(dy, d00, d10);
		d1 = lerp(dy, d01, d11);
		d_final = lerp(dz, d0, d1);
		
		scatter_col[rgb] = d_final;
	}
}


/* no interpolation */
static void vol_get_precached_scattering_nearest(ShadeInput *shi, float *scatter_col, float *co)
{
	const int res = shi->mat->vol_precache_resolution;
	int x,y,z;
	float bbmin[3], bbmax[3], dim[3];

	if (!shi->obi->volume_precache) return;
	
	VECCOPY(bbmin, shi->obi->obr->boundbox[0]);
	VECCOPY(bbmax, shi->obi->obr->boundbox[1]);
	VecSubf(dim, bbmax, bbmin);
	
	x = (int)(((co[0] - bbmin[0]) / dim[0]) * res);
	y = (int)(((co[1] - bbmin[1]) / dim[1]) * res);
	z = (int)(((co[2] - bbmin[2]) / dim[2]) * res);
	
	scatter_col[0] = shi->obi->volume_precache[0*res*res*res + x*res*res + y*res + z];
	scatter_col[1] = shi->obi->volume_precache[1*res*res*res + x*res*res + y*res + z];
	scatter_col[2] = shi->obi->volume_precache[2*res*res*res + x*res*res + y*res + z];
}


/* Compute attenuation, otherwise known as 'optical thickness', extinction, or tau.
 * Used in the relationship Transmittance = e^(-attenuation)
 */
void vol_get_attenuation(ShadeInput *shi, float *tau, float *co, float *endco, float density, float stepsize)
{
	/* input density = density at co */
	float dist;
	float absorb_col[3];
	int s, nsteps;
	float step_vec[3], step_sta[3], step_end[3];

	vol_get_absorption(shi, absorb_col, co);

	dist = VecLenf(co, endco);
	nsteps = (int)ceil(dist / stepsize);
	
	/* trigger for recalculating density */
	if (density < -0.001f) density = vol_get_density(shi, co);
	
	if (nsteps == 1) {
		/* homogenous volume within the sampled distance */
		tau[0] = tau[1] = tau[2] = dist * density;
		
		VecMulVecf(tau, tau, absorb_col);
		return;
	} else {
		tau[0] = tau[1] = tau[2] = 0.0;
	}
	
	VecSubf(step_vec, endco, co);
	VecMulf(step_vec, 1.0f / nsteps);
	
	VECCOPY(step_sta, co);
	VecAddf(step_end, step_sta, step_vec);
	
	for (s = 0;  s < nsteps; s++) {
		if (s > 0)
			density = vol_get_density(shi, step_sta);
		
		tau[0] += stepsize * density;
		tau[1] += stepsize * density;
		tau[2] += stepsize * density;
		
		if (s < nsteps-1) {
			VECCOPY(step_sta, step_end);
			VecAddf(step_end, step_end, step_vec);
		}
	}
	VecMulVecf(tau, tau, absorb_col);
}

void vol_shade_one_lamp(struct ShadeInput *shi, float *co, LampRen *lar, float *lacol, float stepsize, float density)
{
	float visifac, lv[3], lampdist;
	float tau[3], tr[3]={1.0,1.0,1.0};
	float hitco[3], *atten_co;
	float p;
	float scatter_fac;
	float shade_stepsize = vol_get_stepsize(shi, STEPSIZE_SHADE);
	float shadfac[4];
	
	if (lar->mode & LA_LAYER) if((lar->lay & shi->obi->lay)==0) return;
	if ((lar->lay & shi->lay)==0) return;
	if (lar->energy == 0.0) return;
	
	visifac= lamp_get_visibility(lar, co, lv, &lampdist);
	if(visifac==0.0f) return;

	lacol[0] = lar->r;
	lacol[1] = lar->g;
	lacol[2] = lar->b;
	
	if(lar->mode & LA_TEXTURE) {
		shi->osatex= 0;
		do_lamp_tex(lar, lv, shi, lacol, LA_TEXTURE);
	}

	VecMulf(lacol, visifac*lar->energy);

	if (ELEM(lar->type, LA_SUN, LA_HEMI))
		VECCOPY(lv, lar->vec);
	VecMulf(lv, -1.0f);
	
	p = vol_get_phasefunc(shi, shi->mat->vol_phasefunc_type, shi->mat->vol_phasefunc_g, shi->view, lv);
	VecMulf(lacol, p);
	
	if (shi->mat->vol_shadeflag & MA_VOL_ATTENUATED) {
		Isect is;
		
		/* find minimum of volume bounds, or lamp coord */
		if (vol_get_bounds(shi, co, lv, hitco, &is, VOL_BOUNDS_SS, 0)) {
			float dist = VecLenf(co, hitco);
			VlakRen *vlr = (VlakRen *)is.face;
			
			/* simple internal shadowing */
			if (vlr->mat->material_type == MA_SOLID) {
				lacol[0] = lacol[1] = lacol[2] = 0.0f;
				return;
			}

			if (ELEM(lar->type, LA_SUN, LA_HEMI))
				atten_co = hitco;
			else if ( lampdist < dist ) {
				atten_co = lar->co;
			} else
				atten_co = hitco;
			
			vol_get_attenuation(shi, tau, co, atten_co, density, shade_stepsize);
			tr[0] = exp(-tau[0]);
			tr[1] = exp(-tau[1]);
			tr[2] = exp(-tau[2]);
			
			VecMulVecf(lacol, lacol, tr);
		}
		else {
			/* Point is on the outside edge of the volume,
			 * therefore no attenuation, full transmission.
			 * Radiance from lamp remains unchanged */
		}
	}
	
	vol_get_scattering_fac(shi, &scatter_fac, co, density);
	VecMulf(lacol, scatter_fac);
}

/* single scattering only for now */
void vol_get_scattering(ShadeInput *shi, float *scatter, float *co, float stepsize, float density)
{
	GroupObject *go;
	ListBase *lights;
	LampRen *lar;
	float col[3] = {0.f, 0.f, 0.f};
	int i=0;

	for(go=R.lights.first; go; go= go->next)
	{
		float lacol[3] = {0.f, 0.f, 0.f};
	
		i++;
	
		lar= go->lampren;
		if (lar) {
			vol_shade_one_lamp(shi, co, lar, lacol, stepsize, density);
			VecAddf(col, col, lacol);
		}
	}
	
	VECCOPY(scatter, col);
}

static void volumeintegrate(struct ShadeInput *shi, float *col, float *co, float *endco)
{
	float tr[3] = {1.0f, 1.0f, 1.0f};			/* total transmittance */
	float radiance[3] = {0.f, 0.f, 0.f}, d_radiance[3] = {0.f, 0.f, 0.f};
	float stepsize = vol_get_stepsize(shi, STEPSIZE_VIEW);
	int nsteps;
	float vec[3], stepvec[3] = {0.0, 0.0, 0.0};
	float tau[3], emit_col[3], scatter_col[3] = {0.0, 0.0, 0.0};
	int s;
	float step_sta[3], step_end[3], step_mid[3];
	float alpha;
	float density = vol_get_density(shi, co);
	
	/* multiply col_behind with beam transmittance over entire distance */
	vol_get_attenuation(shi, tau, co, endco, density, stepsize);
	tr[0] *= exp(-tau[0]);
	tr[1] *= exp(-tau[1]);
	tr[2] *= exp(-tau[2]);
	VecMulVecf(radiance, tr, col);	
	tr[0] = tr[1] = tr[2] = 1.0f;
	
	/* ray marching */
	nsteps = (int)ceil(VecLenf(co, endco) / stepsize);
	
	VecSubf(vec, endco, co);
	VECCOPY(stepvec, vec);
	VecMulf(stepvec, 1.0f / nsteps);
	
	VECCOPY(step_sta, co);
	VecAddf(step_end, step_sta, stepvec);
	
	/* get radiance from all points along the ray due to participating media */
	for (s = 0; s < nsteps; s++) {

		if (s > 0) density = vol_get_density(shi, step_sta);
		
		/* there's only any use in shading here
		 * if there's actually some density to shade! */
		if (density > 0.01f) {
		
			/* transmittance component (alpha) */
			vol_get_attenuation(shi, tau, step_sta, step_end, density, stepsize);
			tr[0] *= exp(-tau[0]);
			tr[1] *= exp(-tau[1]);
			tr[2] *= exp(-tau[2]);
			
			step_mid[0] = step_sta[0] + (stepvec[0] * 0.5);
			step_mid[1] = step_sta[1] + (stepvec[1] * 0.5);
			step_mid[2] = step_sta[2] + (stepvec[2] * 0.5);
		
			/* incoming light via emission or scattering (additive) */
			vol_get_emission(shi, emit_col, step_mid, density);
			
			if ((shi->mat->vol_shadeflag & MA_VOL_PRECACHESHADING) &&
				(shi->mat->vol_shadeflag & MA_VOL_ATTENUATED)) {
				if (G.rt==0)
					vol_get_precached_scattering(shi, scatter_col, step_mid);
				else
					vol_get_precached_scattering_nearest(shi, scatter_col, step_mid);
			} else
				vol_get_scattering(shi, scatter_col, step_mid, stepsize, density);
						
			VecMulf(scatter_col, density);
			VecAddf(d_radiance, emit_col, scatter_col);
			
			/*   Lv += Tr * (Lve() + Ld) */
			VecMulVecf(d_radiance, tr, d_radiance);
			VecMulf(d_radiance, stepsize);
			
			VecAddf(radiance, radiance, d_radiance);	
		}

		VECCOPY(step_sta, step_end);
		VecAddf(step_end, step_end, stepvec);
	}
	
	col[0] = radiance[0];
	col[1] = radiance[1];
	col[2] = radiance[2];
	
	alpha = 1.0f -(tr[0] + tr[1] + tr[2]) * 0.333f;
	col[3] = alpha;
	
	/*
	Incoming radiance = 
		  outgoing radiance from behind surface * beam transmittance/attenuation
		
		+ added radiance from all points along the ray due to participating media
		    --> radiance for each segment = 
					  radiance added by scattering 
					+ radiance added by emission
					* beam transmittance/attenuation
	
	
	-- To find transmittance:
		compute optical thickness with tau (perhaps involving monte carlo integration)
		return exp(-tau)
		
	-- To find radiance from segments along the way:
		find radiance for one step:
		  - loop over lights and weight by phase function
		*/
}

static void shade_intersection(ShadeInput *shi, float *col, Isect *is)
{
	ShadeInput shi_new;
	ShadeResult shr_new;
	
	memset(&shi_new, 0, sizeof(ShadeInput)); 
	
	shi_new.mask= shi->mask;
	shi_new.osatex= shi->osatex;
	shi_new.thread= shi->thread;
	shi_new.depth= 1;
	shi_new.volume_depth= shi->volume_depth + 1;
	shi_new.xs= shi->xs;
	shi_new.ys= shi->ys;
	shi_new.lay= shi->lay;
	shi_new.passflag= SCE_PASS_COMBINED; /* result of tracing needs no pass info */
	shi_new.combinedflag= 0xFFFFFF;		 /* ray trace does all options */
	shi_new.light_override= shi->light_override;
	shi_new.mat_override= shi->mat_override;
	
	VECCOPY(shi_new.camera_co, is->start);
	
	memset(&shr_new, 0, sizeof(ShadeResult));

	/* hardcoded limit of 100 for now - prevents problems in weird geometry */
	if (shi->volume_depth < 100) {
		shade_ray(is, &shi_new, &shr_new);
	}
	
	col[0] = shr_new.combined[0];
	col[1] = shr_new.combined[1];
	col[2] = shr_new.combined[2];
	col[3] = shr_new.alpha;
}

static void vol_trace_behind(ShadeInput *shi, VlakRen *vlr, float *co, float *col)
{
	Isect isect;
	float maxsize = RE_ray_tree_max_size(R.raytree);

	VECCOPY(isect.start, co);
	isect.end[0] = isect.start[0] + shi->view[0] * maxsize;
	isect.end[1] = isect.start[1] + shi->view[1] * maxsize;
	isect.end[2] = isect.start[2] + shi->view[2] * maxsize;

	isect.faceorig= (RayFace *)vlr;
	
	isect.mode= RE_RAY_MIRROR;
	isect.oborig= RAY_OBJECT_SET(&R, shi->obi);
	isect.face_last= NULL;
	isect.ob_last= 0;
	isect.lay= -1;
	
	/* check to see if there's anything behind the volume, otherwise shade the sky */
	if(RE_ray_tree_intersect(R.raytree, &isect)) {
		shade_intersection(shi, col, &isect);
	} else {
		shadeSkyView(col, co, shi->view, NULL);
		shadeSunView(col, shi->view);
	}
}

/* the main entry point for volume shading */
void volume_trace(struct ShadeInput *shi, struct ShadeResult *shr)
{
	float hitco[3], col[4] = {0.f,0.f,0.f,0.f};
	Isect is;

	memset(shr, 0, sizeof(ShadeResult));

	/* if 1st hit normal is facing away from the camera, 
	 * then we're inside the volume already. */
	if (shi->flippednor) {
		/* trace behind the 1st hit point */
		vol_trace_behind(shi, shi->vlr, shi->co, col);
		
		/* shade volume from 'camera' to 1st hit point */
		volumeintegrate(shi, col, shi->camera_co, shi->co);
		
		shr->combined[0] = col[0];
		shr->combined[1] = col[1];
		shr->combined[2] = col[2];
		
		//if (col[3] > 1.0f)
		col[3] = 1.0f;
		shr->combined[3] = col[3];
		shr->alpha = col[3];
		
		VECCOPY(shr->diff, shr->combined);
	}
	/* trace to find a backface, the other side bounds of the volume */
	/* (ray intersect ignores front faces here) */
	else if (vol_get_bounds(shi, shi->co, shi->view, hitco, &is, VOL_BOUNDS_DEPTH, 0)) {
		VlakRen *vlr = (VlakRen *)is.face;
		
		/* if it's another face in the same material */
		if (vlr->mat == shi->mat) {
			/* trace behind the 2nd (raytrace) hit point */
			vol_trace_behind(shi, (VlakRen *)is.face, hitco, col);
		} else {
			shade_intersection(shi, col, &is);
		}
	
		/* shade volume from 1st hit point to 2nd hit point */
		volumeintegrate(shi, col, shi->co, hitco);
		
		shr->combined[0] = col[0];
		shr->combined[1] = col[1];
		shr->combined[2] = col[2];
		
		//if (col[3] > 1.0f) 
		col[3] = 1.0f;
		shr->combined[3] = col[3];
		shr->alpha = col[3];
		
		VECCOPY(shr->diff, shr->combined);
	}
	else {
		shr->combined[0] = 0.0f;
		shr->combined[1] = 0.0f;
		shr->combined[2] = 0.0f;
		shr->combined[3] = shr->alpha =  1.0f;
	}
}

/* Traces a shadow through the object, 
 * pretty much gets the transmission over a ray path */
void volume_trace_shadow(struct ShadeInput *shi, struct ShadeResult *shr, struct Isect *last_is)
{
	float hitco[3];
	float tr[3] = {1.0,1.0,1.0};
	float tau[3] = {0.0,0.0,0.0};
	Isect is;
	float shade_stepsize = vol_get_stepsize(shi, STEPSIZE_SHADE);

	memset(shr, 0, sizeof(ShadeResult));
	
	/* if 1st hit normal is facing away from the camera, 
	 * then we're inside the volume already. */
	if (shi->flippednor) {
	
		vol_get_attenuation(shi, tau, last_is->start, shi->co, -1.0f, shade_stepsize);
		tr[0] = exp(-tau[0]);
		tr[1] = exp(-tau[1]);
		tr[2] = exp(-tau[2]);
		
		shr->combined[0] = tr[0];
		shr->combined[1] = tr[1];
		shr->combined[2] = tr[2];
		
		shr->combined[3] = 1.0f -(tr[0] + tr[1] + tr[2]) * 0.333f;
		shr->alpha = shr->combined[3];
	}
	/* trace to find a backface, the other side bounds of the volume */
	/* (ray intersect ignores front faces here) */
	else if (vol_get_bounds(shi, shi->co, shi->view, hitco, &is, VOL_BOUNDS_DEPTH, 0)) {
		
		vol_get_attenuation(shi, tau, shi->co, hitco, -1.0f, shade_stepsize);
		tr[0] = exp(-tau[0]);
		tr[1] = exp(-tau[1]);
		tr[2] = exp(-tau[2]);
		
		shr->combined[0] = tr[0];
		shr->combined[1] = tr[1];
		shr->combined[2] = tr[2];
		
		shr->combined[3] = 1.0f -(tr[0] + tr[1] + tr[2]) * 0.333f;
		shr->alpha = shr->combined[3];

	}
	else {
		shr->combined[0] = 0.0f;
		shr->combined[1] = 0.0f;
		shr->combined[2] = 0.0f;
		shr->combined[3] = shr->alpha =  0.0f;
	}
}


/* Recursive test for intersections, from a point inside the mesh, to outside
 * Number of intersections (depth) determine if a point is inside or outside the mesh */
int intersect_outside_volume(RayTree *tree, Isect *isect, float *offset, int limit, int depth)
{
	if (limit == 0) return depth;
	
	if (RE_ray_tree_intersect(tree, isect)) {
		float hitco[3];
		
		hitco[0] = isect->start[0] + isect->labda*isect->vec[0];
		hitco[1] = isect->start[1] + isect->labda*isect->vec[1];
		hitco[2] = isect->start[2] + isect->labda*isect->vec[2];
		VecAddf(isect->start, hitco, offset);

		return intersect_outside_volume(tree, isect, offset, limit-1, depth+1);
	} else {
		return depth;
	}
}

/* Uses ray tracing to check if a point is inside or outside an ObjectInstanceRen */
int point_inside_obi(RayTree *tree, ObjectInstanceRen *obi, float *co)
{
	float maxsize = RE_ray_tree_max_size(tree);
	int intersected;
	Isect isect;
	float vec[3] = {0.0f,0.0f,1.0f};
	int final_depth=0, depth=0, limit=20;
	
	/* set up the isect */
	memset(&isect, 0, sizeof(isect));
	VECCOPY(isect.start, co);
	isect.end[0] = co[0] + vec[0] * maxsize;
	isect.end[1] = co[1] + vec[1] * maxsize;
	isect.end[2] = co[2] + vec[2] * maxsize;
	
	/* and give it a little offset to prevent self-intersections */
	VecMulf(vec, 1e-5);
	VecAddf(isect.start, isect.start, vec);
	
	isect.mode= RE_RAY_MIRROR;
	isect.face_last= NULL;
	isect.lay= -1;
	
	final_depth = intersect_outside_volume(tree, &isect, vec, limit, depth);
	
	/* even number of intersections: point is outside
	 * odd number: point is inside */
	if (final_depth % 2 == 0) return 0;
	else return 1;
}

static int inside_check_func(Isect *is, int ob, RayFace *face)
{
	return 1;
}
static void vlr_face_coords(RayFace *face, float **v1, float **v2, float **v3, float **v4)
{
	VlakRen *vlr= (VlakRen*)face;

	*v1 = (vlr->v1)? vlr->v1->co: NULL;
	*v2 = (vlr->v2)? vlr->v2->co: NULL;
	*v3 = (vlr->v3)? vlr->v3->co: NULL;
	*v4 = (vlr->v4)? vlr->v4->co: NULL;
}

RayTree *create_raytree_obi(ObjectInstanceRen *obi, float *bbmin, float *bbmax)
{
	int v;
	VlakRen *vlr= NULL;
	
	/* create empty raytree */
	RayTree *tree = RE_ray_tree_create(64, obi->obr->totvlak, bbmin, bbmax,
		vlr_face_coords, inside_check_func, NULL, NULL);
	
	/* fill it with faces */
	for(v=0; v<obi->obr->totvlak; v++) {
		if((v & 255)==0)
			vlr= obi->obr->vlaknodes[v>>8].vlak;
		else
			vlr++;
	
		RE_ray_tree_add_face(tree, 0, vlr);
	}
	
	RE_ray_tree_done(tree);
	
	return tree;
}

/* Precache a volume into a 3D voxel grid.
 * The voxel grid is stored in the ObjectInstanceRen, 
 * in camera space, aligned with the ObjectRen's bounding box.
 * Resolution is defined by the user.
 */
void vol_precache_objectinstance(Render *re, ObjectInstanceRen *obi, Material *ma, float *bbmin, float *bbmax)
{
	int x, y, z;

	float co[3], voxel[3], scatter_col[3];
	ShadeInput shi;
	float view[3] = {0.0,0.0,-1.0};
	float density;
	float stepsize;
	
	float resf, res_3f;
	int res_2, res_3;
	
	float i = 1.0f;
	double time, lasttime= PIL_check_seconds_timer();
	const int res = ma->vol_precache_resolution;
	RayTree *tree;
	
	R = *re;

	/* create a raytree with just the faces of the instanced ObjectRen, 
	 * used for checking if the cached point is inside or outside. */
	tree = create_raytree_obi(obi, bbmin, bbmax);
	if (!tree) return;

	/* Need a shadeinput to calculate scattering */
	memset(&shi, 0, sizeof(ShadeInput)); 
	shi.depth= 1;
	shi.mask= 1;
	shi.mat = ma;
	shi.vlr = NULL;
	memcpy(&shi.r, &shi.mat->r, 23*sizeof(float));	// note, keep this synced with render_types.h
	shi.har= shi.mat->har;
	shi.obi= obi;
	shi.obr= obi->obr;
	shi.lay = re->scene->lay;
	VECCOPY(shi.view, view);
	
	stepsize = vol_get_stepsize(&shi, STEPSIZE_VIEW);

	resf = (float)res;
	res_2 = res*res;
	res_3 = res*res*res;
	res_3f = (float)res_3;
	
	VecSubf(voxel, bbmax, bbmin);
	if ((voxel[0] < FLT_EPSILON) || (voxel[1] < FLT_EPSILON) || (voxel[2] < FLT_EPSILON))
		return;
	VecMulf(voxel, 1.0f/res);
	
	obi->volume_precache = MEM_callocN(sizeof(float)*res_3*3, "volume light cache");
	
	/* Iterate over the 3d voxel grid, and fill the voxels with scattering information
	 *
	 * It's stored in memory as 3 big float grids next to each other, one for each RGB channel.
	 * I'm guessing the memory alignment may work out better this way for the purposes
	 * of doing linear interpolation, but I haven't actually tested this theory! :)
	 */
	for (x=0; x < res; x++) {
		co[0] = bbmin[0] + (voxel[0] * x);
		
		for (y=0; y < res; y++) {
			co[1] = bbmin[1] + (voxel[1] * y);
			
			for (z=0; z < res; z++) {
				co[2] = bbmin[2] + (voxel[2] * z);
			
				time= PIL_check_seconds_timer();
				i++;
			
				/* display progress every second */
				if(re->test_break()) {
					if(tree) {
						RE_ray_tree_free(tree);
						tree= NULL;
					}
					return;
				}
				if(time-lasttime>1.0f) {
					char str[64];
					sprintf(str, "Precaching volume: %d%%", (int)(100.0f * (i / res_3f)));
					re->i.infostr= str;
					re->stats_draw(&re->i);
					re->i.infostr= NULL;
					lasttime= time;
				}
				
				/* don't bother if the point is not inside the volume mesh */
				if (!point_inside_obi(tree, obi, co))
					continue;

				density = vol_get_density(&shi, co);
				vol_get_scattering(&shi, scatter_col, co, stepsize, density);
			
				obi->volume_precache[0*res_3 + x*res_2 + y*res + z] = scatter_col[0];
				obi->volume_precache[1*res_3 + x*res_2 + y*res + z] = scatter_col[1];
				obi->volume_precache[2*res_3 + x*res_2 + y*res + z] = scatter_col[2];
			}
		}
	}

	if(tree) {
		RE_ray_tree_free(tree);
		tree= NULL;
	}
	

}

/* loop through all objects (and their associated materials)
 * marked for pre-caching in convertblender.c, and pre-cache them */
void volume_precache(Render *re)
{
	ObjectInstanceRen *obi;
	VolPrecache *vp;

	for(vp= re->vol_precache_obs.first; vp; vp= vp->next) {
		for(obi= re->instancetable.first; obi; obi= obi->next) {
			if (obi->obr == vp->obr) 
				vol_precache_objectinstance(re, obi, vp->ma, obi->obr->boundbox[0], obi->obr->boundbox[1]);
		}
	}
	
	re->i.infostr= NULL;
	re->stats_draw(&re->i);
}

void free_volume_precache(Render *re)
{
	ObjectInstanceRen *obi;
	
	for(obi= re->instancetable.first; obi; obi= obi->next) {
		if (obi->volume_precache)
			MEM_freeN(obi->volume_precache);
	}
	
	BLI_freelistN(&re->vol_precache_obs);
}