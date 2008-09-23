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
 * Contributor(s): Farsthary (Raul FHernandez), Matt Ebb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "RE_shader_ext.h"
#include "RE_raytrace.h"

#include "DNA_material_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"

#include "BKE_global.h"

#include "render_types.h"
#include "shading.h"
#include "texture.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#if 0
static int vol_backface_intersect_check(Isect *is, int ob, RayFace *face)
{
	VlakRen *vlr = (VlakRen *)face;
	
	/* only consider faces away, so overlapping layers
	 * of foward facing geometry don't cause the ray to stop */
	return (INPR(is->vec, vlr->n) < 0.0f);
}
#endif

#define VOL_IS_SAMEOBJECT		1
#define VOL_IS_SAMEMATERIAL		2


#define VOL_BOUNDS_DEPTH	0
#define VOL_BOUNDS_SS		1

int vol_get_bounds(ShadeInput *shi, float *co, float *vec, float *hitco, int intersect_type)
{
	/* TODO: Box or sphere intersection types could speed things up */

	/* raytrace method */
	Isect isect;
	float maxsize = RE_ray_tree_max_size(R.raytree);

	/* TODO: use object's bounding box to calculate max size */
	VECCOPY(isect.start, co);
	isect.end[0] = co[0] + vec[0] * maxsize;
	isect.end[1] = co[1] + vec[1] * maxsize;
	isect.end[2] = co[2] + vec[2] * maxsize;
	
	if (intersect_type == VOL_BOUNDS_DEPTH) isect.faceorig= (RayFace*)shi->vlr;
	else if (intersect_type == VOL_BOUNDS_SS) isect.faceorig= NULL;
	
	isect.mode= RE_RAY_MIRROR;
	isect.oborig= RAY_OBJECT_SET(&R, shi->obi);
	isect.face_last= NULL;
	isect.ob_last= 0;
	isect.lay= -1;
	
	if(RE_ray_tree_intersect(R.raytree, &isect))
	{
		float isvec[3];

		VECCOPY(isvec, isect.vec);
		hitco[0] = isect.start[0] + isect.labda*isvec[0];
		hitco[1] = isect.start[1] + isect.labda*isvec[1];
		hitco[2] = isect.start[2] + isect.labda*isvec[2];
		
		return 1;
	} else {
		return 0;
	}
}

float vol_get_density(struct ShadeInput *shi, float *co)
{
	float density = shi->mat->alpha;
	float emit_fac=0.0f;
	float col[3] = {0.0, 0.0, 0.0};
	
	/* do any density gain stuff here */
	if (shi->mat->flag & MA_IS_TEXTURED)
		do_volume_tex(shi, co, col, &density, &emit_fac);
	
	return density;
}


/* compute emission component, amount of radiance to add per segment
 * can be textured with 'emit' */
void vol_get_emission(ShadeInput *shi, float *em, float *co, float density)
{
	float emission = shi->mat->emit;
	float col[3];
	float dens_dummy = 1.0f;
	
	VECCOPY(col, &shi->mat->r);
	
	do_volume_tex(shi, co, col, &dens_dummy, &emission);
	
	em[0] = em[1] = em[2] = emission;
	VecMulVecf(em, em, col);
}


/* Compute attenuation, otherwise known as 'optical thickness', extinction, or tau.
 * Used in the relationship Transmittance = e^(-attenuation)
 * can be textured with 'alpha' */
void vol_get_attenuation(ShadeInput *shi, float *tau, float *co, float *endco, float density, float stepsize)
{
	/* input density = density at co */
	float dist;
	float absorption = shi->mat->vol_absorption;
	int s, nsteps;
	float step_vec[3], step_sta[3], step_end[3];

	dist = VecLenf(co, endco);

	nsteps = (int)ceil(dist / stepsize);
	
	if (nsteps == 1) {
		/* homogenous volume within the sampled distance */
		tau[0] = tau[1] = tau[2] = dist * density;
		VecMulf(tau, absorption);
		return;
	} else {
		tau[0] = tau[1] = tau[2] = 0.0;
	}
	
	VecSubf(step_vec, endco, co);
	VecMulf(step_vec, 1.0f / nsteps);
	
	VECCOPY(step_sta, co);
	VecAddf(step_end, step_sta, step_vec);
	
	for (s = 0;  s < nsteps; s++) {
		
		if (s > 0) density = vol_get_density(shi, step_sta);
		
		tau[0] += stepsize * density;
		tau[1] += stepsize * density;
		tau[2] += stepsize * density;
		
		if (s < nsteps-1) {
			VECCOPY(step_sta, step_end);
			VecAddf(step_end, step_end, step_vec);
		}
	}
	VecMulf(tau, absorption);	
}

void vol_shade_one_lamp(struct ShadeInput *shi, float *co, LampRen *lar, float *col, float stepsize, float density)
{
	float visifac, lv[3], lampdist;
	float lacol[3];
	float tau[3], tr[3]={1.0,1.0,1.0};
	float hitco[3], *atten_co;
	
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


	if (shi->mat->vol_shadeflag & MA_VOL_ATTENUATED) {
		
		if (ELEM(lar->type, LA_SUN, LA_HEMI))
			VECCOPY(lv, lar->vec);
		VecMulf(lv, -1.0f);
		
		/* find minimum of volume bounds, or lamp coord */
		if (vol_get_bounds(shi, co, lv, hitco, VOL_BOUNDS_SS)) {
			float dist = VecLenf(co, hitco);
			
			if (ELEM(lar->type, LA_SUN, LA_HEMI))
				atten_co = hitco;
			else if ( lampdist < dist ) {
				atten_co = lar->co;
			} else
				atten_co = hitco;

			vol_get_attenuation(shi, tau, co, atten_co, density, shi->mat->vol_shade_stepsize);
			tr[0] = exp(-tau[0]);
			tr[1] = exp(-tau[1]);
			tr[2] = exp(-tau[2]);
			
			VecMulVecf(lacol, lacol, tr);
		}
		else {
			/* point is on the outside edge of the volume,
			 * therefore no attenuation, full transmission
			 * radiance from lamp remains unchanged */
		}
	}
	
	VecAddf(col, col, lacol);
}

/* shadows -> trace a ray to find blocker geometry
   - if blocker is outside the volume, use standard shadow functions
   - if blocker is inside the volume, use raytracing
    -- (deep shadow maps could potentially slot in here too I suppose)
   - attenuate from current point, to blocked point or volume bounds
*/

/* single scattering only for now */
void vol_get_scattering(ShadeInput *shi, float *scatter, float *co, float stepsize, float density)
{
	GroupObject *go;
	ListBase *lights;
	LampRen *lar;
	float col[3] = {0.f, 0.f, 0.f};
	
	lights= get_lights(shi);
	for(go=lights->first; go; go= go->next)
	{
		float lacol[3] = {0.f, 0.f, 0.f};
	
		lar= go->lampren;
		if (lar==NULL) continue;
		
		vol_shade_one_lamp(shi, co, lar, lacol, stepsize, density);
		
		/* isotropic phase function */
		VecMulf(lacol, 1.0f / (4.f * M_PI));
	
		VecMulf(lacol, density);
		
		VecAddf(col, col, lacol);
	}
	
	VECCOPY(scatter, col);
}



static void volumeintegrate(struct ShadeInput *shi, float *col, float *co, float *endco)
{
	float tr[3] = {1.f, 1.f, 1.f};			/* total transmittance */
	float radiance[3] = {0.f, 0.f, 0.f}, d_radiance[3] = {0.f, 0.f, 0.f};
	float stepsize = shi->mat->vol_stepsize;
	int nsteps;
	float vec[3], stepvec[3] = {0.0, 0.0, 0.0};
	float step_tau[3], step_emit[3], step_scatter[3] = {0.0, 0.0, 0.0};
	int s;
	float step_sta[3], step_end[3];
	
	/* multiply col_behind with beam transmittance over entire distance */
/*
	// get col_behind
	
	// get total transmittance
	vol_get_attenuation(shi, total_tau, start, dist, stepsize);
	total_tr[0] = exp(-total_tau[0]);
	total_tr[1] = exp(-total_tau[1]);
	total_tr[2] = exp(-total_tau[2]);
	VecMulVecf(radiance, total_tr, col_behind);
*/	
	
	/* ray marching */
	nsteps = (int)ceil(VecLenf(co, endco) / stepsize);
	
	VecSubf(vec, endco, co);
	VECCOPY(stepvec, vec);
	VecMulf(stepvec, 1.0f / nsteps);
	
	VECCOPY(step_sta, co);
	VecAddf(step_end, step_sta, stepvec);
	
	
	/* get radiance from all points along the ray due to participating media */
	for (s = 0; s < nsteps; s++) {
		float density = vol_get_density(shi, step_sta);
	
		/* *** transmittance and emission *** */
		
		/* transmittance component (alpha) */
		vol_get_attenuation(shi, step_tau, step_sta, step_end, density, stepsize);
		tr[0] *= exp(-step_tau[0]);
		tr[1] *= exp(-step_tau[1]);
		tr[2] *= exp(-step_tau[2]);
		
		/* Terminate raymarching if transmittance is small */
		//if (rgb_to_luminance(tr[0], tr[1], tr[2]) < 1e-3) break;
		
		/* incoming light via emission or scattering (additive) */
		vol_get_emission(shi, step_emit, step_sta, density);
		vol_get_scattering(shi, step_scatter, step_end, stepsize, density);
		
		VecAddf(d_radiance, step_emit, step_scatter);
		
		/*   Lv += Tr * (Lve() + Ld) */
		VecMulVecf(d_radiance, tr, d_radiance);
		VecAddf(radiance, radiance, d_radiance);	

		VECCOPY(step_sta, step_end);
		VecAddf(step_end, step_end, stepvec);
	}
	
	VecMulf(radiance, stepsize);
	VECCOPY(col, radiance);
	
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

		  - single scattering
		    : integrate over sphere
		
		then multiply each step for final exit radiance
		*/
}

void volume_trace(struct ShadeInput *shi, struct ShadeResult *shr)
{
	float hitco[3], col[3];

	memset(shr, 0, sizeof(ShadeResult));

	if (vol_get_bounds(shi, shi->co, shi->view, hitco, VOL_BOUNDS_DEPTH)) {
		
		volumeintegrate(shi, col, shi->co, hitco);
		
		/* hit */
		shr->alpha = 1.0f;
		shr->combined[0] = col[0];
		shr->combined[1] = col[1];
		shr->combined[2] = col[2];
		
		QUATCOPY(shr->diff, shr->combined);
	}
	else {
		/* no hit */
		shr->combined[0] = 0.0f;
		shr->combined[1] = 0.0f;
		shr->combined[2] = 0.0f;
		shr->combined[3] = shr->alpha =  0.0f;
	}
}
