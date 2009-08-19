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
 * Contributor(s): Matt Ebb, Raul Fernandez Hernandez (Farsthary)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"
#include "BLI_voxel.h"

#include "RE_shader_ext.h"
#include "RE_raytrace.h"

#include "DNA_material_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"

#include "BKE_global.h"

#include "render_types.h"
#include "pixelshading.h"
#include "shading.h"
#include "texture.h"
#include "volumetric.h"

#if defined( _MSC_VER ) && !defined( __cplusplus )
# define inline __inline
#endif // defined( _MSC_VER ) && !defined( __cplusplus )

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


/* TODO: Box or sphere intersection types could speed things up */
static int vol_get_bounds(ShadeInput *shi, float *co, float *vec, float *hitco, Isect *isect, int intersect_type)
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

float vol_get_stepsize(struct ShadeInput *shi, int context)
{
	if (shi->mat->vol.stepsize_type == MA_VOL_STEP_RANDOMIZED) {
		/* range between 0.75 and 1.25 */
		const float rnd = 0.5f * BLI_thread_frand(shi->thread) + 0.75f;
	
		if (context == STEPSIZE_VIEW)
			return shi->mat->vol.stepsize * rnd;
		else if (context == STEPSIZE_SHADE)
			return shi->mat->vol.shade_stepsize * rnd;
	}
	else {	// MA_VOL_STEP_CONSTANT
		
		if (context == STEPSIZE_VIEW)
			return shi->mat->vol.stepsize;
		else if (context == STEPSIZE_SHADE)
			return shi->mat->vol.shade_stepsize;
	}
	
	return shi->mat->vol.stepsize;
}

/* trilinear interpolation */
static void vol_get_precached_scattering(ShadeInput *shi, float *scatter_col, float *co)
{
	VolumePrecache *vp = shi->obi->volume_precache;
	float bbmin[3], bbmax[3], dim[3];
	float sample_co[3];
	
	if (!vp) return;
	
	/* convert input coords to 0.0, 1.0 */
	VECCOPY(bbmin, shi->obi->obr->boundbox[0]);
	VECCOPY(bbmax, shi->obi->obr->boundbox[1]);
	VecSubf(dim, bbmax, bbmin);

	sample_co[0] = ((co[0] - bbmin[0]) / dim[0]);
	sample_co[1] = ((co[1] - bbmin[1]) / dim[1]);
	sample_co[2] = ((co[2] - bbmin[2]) / dim[2]);

	scatter_col[0] = voxel_sample_trilinear(vp->data_r, vp->res, sample_co);
	scatter_col[1] = voxel_sample_trilinear(vp->data_g, vp->res, sample_co);
	scatter_col[2] = voxel_sample_trilinear(vp->data_b, vp->res, sample_co);
}

float vol_get_density(struct ShadeInput *shi, float *co)
{
	float density = shi->mat->vol.density;
	float density_scale = shi->mat->vol.density_scale;
	float col[3] = {0.0, 0.0, 0.0};
	
	do_volume_tex(shi, co, MAP_DENSITY, col, &density);
	
	return density * density_scale;
}

/* scattering multiplier, values above 1.0 are non-physical, 
 * but can be useful to tweak lighting */
float vol_get_scattering_fac(ShadeInput *shi, float *co)
{
	float scatter = shi->mat->vol.scattering;
	float col[3] = {0.0, 0.0, 0.0};
	
	do_volume_tex(shi, co, MAP_SCATTERING, col, &scatter);
	
	return scatter;
}

/* compute emission component, amount of radiance to add per segment
 * can be textured with 'emit' */
void vol_get_emission(ShadeInput *shi, float *emission_col, float *co, float density)
{
	float emission = shi->mat->vol.emission;
	VECCOPY(emission_col, shi->mat->vol.emission_col);
	
	do_volume_tex(shi, co, MAP_EMISSION+MAP_EMISSION_COL, emission_col, &emission);
	
	emission_col[0] = emission_col[0] * emission * density;
	emission_col[1] = emission_col[1] * emission * density;
	emission_col[2] = emission_col[2] * emission * density;
}

void vol_get_absorption(ShadeInput *shi, float *absorb_col, float *co)
{
	float absorption = shi->mat->vol.absorption;
	VECCOPY(absorb_col, shi->mat->vol.absorption_col);
	
	do_volume_tex(shi, co, MAP_ABSORPTION+MAP_ABSORPTION_COL, absorb_col, &absorption);
	
	absorb_col[0] = (1.0f - absorb_col[0]) * absorption;
	absorb_col[1] = (1.0f - absorb_col[1]) * absorption;
	absorb_col[2] = (1.0f - absorb_col[2]) * absorption;
}


/* phase function - determines in which directions the light 
 * is scattered in the volume relative to incoming direction 
 * and view direction */
float vol_get_phasefunc(ShadeInput *shi, short phasefunc_type, float g, float *w, float *wp)
{
	const float costheta = Inpf(w, wp);
	const float scale = M_PI;
	
	/*
	 * Scale constant is required, since Blender's shading system doesn't normalise for
	 * energy conservation - eg. scaling by 1/pi for a lambert shader.
	 * This makes volumes darker than other solid objects, for the same lighting intensity.
	 * To correct this, scale up the phase function values
	 * until Blender's shading system supports this better. --matt
	 */
	
	switch (phasefunc_type) {
		case MA_VOL_PH_MIEHAZY:
			return scale * (0.5f + 4.5f * powf(0.5 * (1.f + costheta), 8.f)) / (4.f*M_PI);
		case MA_VOL_PH_MIEMURKY:
			return scale * (0.5f + 16.5f * powf(0.5 * (1.f + costheta), 32.f)) / (4.f*M_PI);
		case MA_VOL_PH_RAYLEIGH:
			return scale * 3.f/(16.f*M_PI) * (1 + costheta * costheta);
		case MA_VOL_PH_HG:
			return scale * (1.f / (4.f * M_PI) * (1.f - g*g) / powf(1.f + g*g - 2.f * g * costheta, 1.5f));
		case MA_VOL_PH_SCHLICK:
		{
			const float k = 1.55f * g - .55f * g * g * g;
			const float kcostheta = k * costheta;
			return scale * (1.f / (4.f * M_PI) * (1.f - k*k) / ((1.f - kcostheta) * (1.f - kcostheta)));
		}
		case MA_VOL_PH_ISOTROPIC:
		default:
			return scale * (1.f / (4.f * M_PI));
	}
}

/* Compute attenuation, otherwise known as 'optical thickness', extinction, or tau.
 * Used in the relationship Transmittance = e^(-attenuation)
 */
void vol_get_attenuation_seg(ShadeInput *shi, float *tau, float *stepvec, float *co, float density)
{
	/* input density = density at co */
	float absorb_col[3];
	const float dist = VecLength(stepvec);
	
	vol_get_absorption(shi, absorb_col, co);
	
	/* homogenous volume within the sampled distance */
	tau[0] = tau[1] = tau[2] = dist * density;
		
	VecMulVecf(tau, tau, absorb_col);
}

/* Compute attenuation, otherwise known as 'optical thickness', extinction, or tau.
 * Used in the relationship Transmittance = e^(-attenuation)
 */
void vol_get_attenuation(ShadeInput *shi, float *tau, float *co, float *endco, float density, float stepsize)
{
	/* input density = density at co */
	float absorb_col[3];
	int s, nsteps;
	float step_vec[3], step_sta[3], step_end[3];
	const float dist = VecLenf(co, endco);

	vol_get_absorption(shi, absorb_col, co);

	nsteps = (int)((dist / stepsize) + 0.5);
	
	tau[0] = tau[1] = tau[2] = 0.0;
	
	VecSubf(step_vec, endco, co);
	VecMulf(step_vec, 1.0f / nsteps);
	
	VecCopyf(step_sta, co);
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
	
	p = vol_get_phasefunc(shi, shi->mat->vol.phasefunc_type, shi->mat->vol.phasefunc_g, shi->view, lv);
	VecMulf(lacol, p);
	
	if (shi->mat->vol.shade_type != MA_VOL_SHADE_NONE) {
		Isect is;
		
		/* find minimum of volume bounds, or lamp coord */
		if (vol_get_bounds(shi, co, lv, hitco, &is, VOL_BOUNDS_SS)) {
			float dist = VecLenf(co, hitco);
			VlakRen *vlr = (VlakRen *)is.face;
			
			/* simple internal shadowing */
			if (vlr->mat->material_type == MA_TYPE_SURFACE) {
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
	
	scatter_fac = vol_get_scattering_fac(shi, co);
	VecMulf(lacol, scatter_fac);
}

/* single scattering only for now */
void vol_get_scattering(ShadeInput *shi, float *scatter, float *co, float stepsize, float density)
{
	ListBase *lights;
	GroupObject *go;
	LampRen *lar;
	float col[3] = {0.f, 0.f, 0.f};
	
	lights= get_lights(shi);
	for(go=lights->first; go; go= go->next)
	{
		float lacol[3] = {0.f, 0.f, 0.f};
		lar= go->lampren;
		
		if (lar) {
			vol_shade_one_lamp(shi, co, lar, lacol, stepsize, density);
			VecAddf(col, col, lacol);
		}
	}
	
	VECCOPY(scatter, col);
}

	
/*
The main volumetric integrator, using an emission/absorption/scattering model.

Incoming radiance = 

outgoing radiance from behind surface * beam transmittance/attenuation
+ added radiance from all points along the ray due to participating media
	--> radiance for each segment = 
		(radiance added by scattering + radiance added by emission) * beam transmittance/attenuation

-- To find transmittance:
	compute optical thickness with tau (perhaps involving monte carlo integration)
	transmittance = exp(-tau)
	
-- To find radiance from segments along the way:
	find radiance for one step: 
	- loop over lights and weight by phase function
*/
static void volumeintegrate(struct ShadeInput *shi, float *col, float *co, float *endco)
{
	float tr[3] = {1.0f, 1.0f, 1.0f};
	float radiance[3] = {0.f, 0.f, 0.f}, d_radiance[3] = {0.f, 0.f, 0.f}, radiance_behind[3];
	float stepsize = vol_get_stepsize(shi, STEPSIZE_VIEW);
	int nsteps, s;
	float tau[3], emit_col[3], scatter_col[3] = {0.0, 0.0, 0.0};
	float stepvec[3], step_sta[3], step_end[3], step_mid[3];
	float density;
	const float depth_cutoff = shi->mat->vol.depth_cutoff;

	/* ray marching */
	nsteps = (int)((VecLenf(co, endco) / stepsize) + 0.5);
	
	VecSubf(stepvec, endco, co);
	VecMulf(stepvec, 1.0f / nsteps);
	VecCopyf(step_sta, co);
	VecAddf(step_end, step_sta, stepvec);
	
	/* get radiance from all points along the ray due to participating media */
	for (s = 0; s < nsteps; s++) {

		density = vol_get_density(shi, step_sta);
		
		/* there's only any use in shading here if there's actually some density to shade! */
		if (density > 0.01f) {
		
			/* transmittance component (alpha) */
			vol_get_attenuation_seg(shi, tau, stepvec, co, density);
			tr[0] *= exp(-tau[0]);
			tr[1] *= exp(-tau[1]);
			tr[2] *= exp(-tau[2]);
			
			step_mid[0] = step_sta[0] + (stepvec[0] * 0.5);
			step_mid[1] = step_sta[1] + (stepvec[1] * 0.5);
			step_mid[2] = step_sta[2] + (stepvec[2] * 0.5);
		
			/* incoming light via emission or scattering (additive) */
			vol_get_emission(shi, emit_col, step_mid, density);
			
			if (using_lightcache(shi->mat)) {
				vol_get_precached_scattering(shi, scatter_col, step_mid);
			} else
				vol_get_scattering(shi, scatter_col, step_mid, stepsize, density);
						
			VecMulf(scatter_col, density);
			VecAddf(d_radiance, emit_col, scatter_col);
			
			/*   Lv += Tr * (Lve() + Ld) */
			VecMulVecf(d_radiance, tr, d_radiance);
			VecMulf(d_radiance, stepsize);
			
			VecAddf(radiance, radiance, d_radiance);	
		}

		VecCopyf(step_sta, step_end);
		VecAddf(step_end, step_end, stepvec);
		
		/* luminance rec. 709 */
		if ((0.2126*tr[0] + 0.7152*tr[1] + 0.0722*tr[2]) < depth_cutoff) break;	
	}
	
	/* multiply original color (behind volume) with beam transmittance over entire distance */
	VecMulVecf(radiance_behind, tr, col);	
	VecAddf(radiance, radiance, radiance_behind);
	
	VecCopyf(col, radiance);
	col[3] = 1.0f -(tr[0] + tr[1] + tr[2]) * 0.333f;
}

static void shade_intersection(ShadeInput *shi, float *col, Isect *is)
{
	ShadeInput shi_new;
	ShadeResult shr_new;
	
	memset(&shi_new, 0, sizeof(ShadeInput)); 
	
	shi_new.mask= shi->mask;
	shi_new.osatex= shi->osatex;
	shi_new.thread= shi->thread;
	shi_new.depth = shi->depth + 1;
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
		shadeSkyView(col, co, shi->view, NULL, shi->thread);
		shadeSunView(col, shi->view);
	}
}

/* the main entry point for volume shading */
static void volume_trace(struct ShadeInput *shi, struct ShadeResult *shr, int inside_volume)
{
	float hitco[3], col[4] = {0.f,0.f,0.f,0.f};
	int trace_behind = 1;
	Isect is;

	/* check for shading an internal face a volume object directly */
	if (inside_volume == VOL_SHADE_INSIDE) {
		trace_behind = 0;
	}
	if (inside_volume == VOL_SHADE_OUTSIDE) {
		if (shi->flippednor)
			inside_volume = VOL_SHADE_INSIDE;
	}

	if (inside_volume == VOL_SHADE_INSIDE) {
		
		if (trace_behind) {
			/* trace behind the volume object */
			vol_trace_behind(shi, shi->vlr, shi->co, col);
		} else {
			/* we're tracing through the volume between the camera 
			 * and a solid surface, so use that pre-shaded radiance */
			QUATCOPY(col, shr->combined);
		}
		
		/* shade volume from 'camera' to 1st hit point */
		volumeintegrate(shi, col, shi->camera_co, shi->co);
		
		VecCopyf(shr->combined, col);
		
		if (shi->mat->vol.shadeflag & MA_VOL_USEALPHA) {
			if (col[3] > 1.0f)
				col[3] = 1.0f;
		}
		else
			col[3] = 1.0f;
		shr->combined[3] = col[3];
		shr->alpha = col[3];
		
		VECCOPY(shr->diff, shr->combined);
	}
	/* trace to find a backface, the other side bounds of the volume */
	/* (ray intersect ignores front faces here) */
	else if (vol_get_bounds(shi, shi->co, shi->view, hitco, &is, VOL_BOUNDS_DEPTH)) {
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
		
		VecCopyf(shr->combined, col);
		
		if (shi->mat->vol.shadeflag & MA_VOL_USEALPHA) {
			if (col[3] > 1.0f)
				col[3] = 1.0f;
		}
		else
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
void shade_volume_shadow(struct ShadeInput *shi, struct ShadeResult *shr, struct Isect *last_is)
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
		
		
		VecCopyf(shr->combined, tr);
		
		shr->combined[3] = 1.0f -(tr[0] + tr[1] + tr[2]) * 0.333f;
		shr->alpha = shr->combined[3];
	}
	/* trace to find a backface, the other side bounds of the volume */
	/* (ray intersect ignores front faces here) */
	else if (vol_get_bounds(shi, shi->co, shi->view, hitco, &is, VOL_BOUNDS_DEPTH)) {
		
		vol_get_attenuation(shi, tau, shi->co, hitco, -1.0f, shade_stepsize);
		tr[0] = exp(-tau[0]);
		tr[1] = exp(-tau[1]);
		tr[2] = exp(-tau[2]);
		
		VecCopyf(shr->combined, tr);
		
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


/* delivers a fully filled in ShadeResult, for all passes */
void shade_volume_outside(ShadeInput *shi, ShadeResult *shr)
{
	memset(shr, 0, sizeof(ShadeResult));
	
	volume_trace(shi, shr, VOL_SHADE_OUTSIDE);
}


void shade_volume_inside(ShadeInput *shi, ShadeResult *shr)
{
	MatInside *m;
	Material *mat_backup;
	
	if (BLI_countlist(&R.render_volumes_inside) == 0) return;
	
	mat_backup = shi->mat;
	
//	for (m=R.render_volumes_inside.first; m; m=m->next) {
//		printf("matinside: ma: %s \n", m->ma->id.name+2);
//	}

	m = R.render_volumes_inside.first;
	shi->mat = m->ma;
	
	volume_trace(shi, shr, VOL_SHADE_INSIDE);

	shi->mat = mat_backup;
}