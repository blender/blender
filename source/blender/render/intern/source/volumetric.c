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
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_voxel.h"

#include "RE_shader_ext.h"
#include "RE_raytrace.h"

#include "DNA_material_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_meta_types.h"

#include "BKE_global.h"

#include "render_types.h"
#include "pixelshading.h"
#include "shading.h"
#include "shadbuf.h"
#include "texture.h"
#include "volumetric.h"
#include "volume_precache.h"

#if defined( _MSC_VER ) && !defined( __cplusplus )
# define inline __inline
#endif // defined( _MSC_VER ) && !defined( __cplusplus )

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* luminance rec. 709 */
inline float luminance(float* col)
{
	return (0.212671f*col[0] + 0.71516f*col[1] + 0.072169f*col[2]);
}

/* tracing */
static float vol_get_shadow(ShadeInput *shi, LampRen *lar, float *co)
{
	float visibility = 1.f;
	
	if(lar->shb) {
		float dxco[3]={0.f, 0.f, 0.f}, dyco[3]={0.f, 0.f, 0.f};
		
		visibility = testshadowbuf(&R, lar->shb, co, dxco, dyco, 1.0, 0.0);		
	} else if (lar->mode & LA_SHAD_RAY) {
		/* trace shadow manually, no good lamp api atm */
		Isect is;
		
		copy_v3_v3(is.start, co);
		if(lar->type==LA_SUN || lar->type==LA_HEMI) {
			is.vec[0] = -lar->vec[0];
			is.vec[1] = -lar->vec[1];
			is.vec[2] = -lar->vec[2];
			is.labda = R.maxdist;
		} else {
			VECSUB( is.vec, lar->co, is.start );
			is.labda = len_v3( is.vec );
		}

		is.mode = RE_RAY_MIRROR;
		is.skip = RE_SKIP_VLR_RENDER_CHECK | RE_SKIP_VLR_NON_SOLID_MATERIAL;
		
		if(lar->mode & (LA_LAYER|LA_LAYER_SHADOW))
			is.lay= lar->lay;	
		else
			is.lay= -1;
			
		is.orig.ob = NULL;
		is.orig.face = NULL;
		is.last_hit = lar->last_hit[shi->thread];
		
		if(RE_rayobject_raycast(R.raytree,&is)) {
			visibility = 0.f;
		}
		
		lar->last_hit[shi->thread]= is.last_hit;
	}
	return visibility;
}

static int vol_get_bounds(ShadeInput *shi, float *co, float *vec, float *hitco, Isect *isect, int intersect_type)
{
	/* XXX TODO - get raytrace max distance from object instance's bounding box */
	/* need to account for scaling only, but keep coords in camera space...
	 * below code is WIP and doesn't work!
	sub_v3_v3v3(bb_dim, shi->obi->obr->boundbox[1], shi->obi->obr->boundbox[2]);
	mul_m3_v3(shi->obi->nmat, bb_dim);
	maxsize = len_v3(bb_dim);
	*/
	
	VECCOPY(isect->start, co);
	VECCOPY(isect->vec, vec );
	isect->labda = FLT_MAX;
	isect->mode= RE_RAY_MIRROR;
	isect->last_hit = NULL;
	isect->lay= -1;
	
	if (intersect_type == VOL_BOUNDS_DEPTH) {
		isect->skip = RE_SKIP_VLR_NEIGHBOUR;
		isect->orig.face = (void*)shi->vlr;
		isect->orig.ob = (void*)shi->obi;
	} else { // if (intersect_type == VOL_BOUNDS_SS) {
		isect->skip= 0;
		isect->orig.face= NULL;
		isect->orig.ob = NULL;
	}
	
	if(RE_rayobject_raycast(R.raytree, isect))
	{
		hitco[0] = isect->start[0] + isect->labda*isect->vec[0];
		hitco[1] = isect->start[1] + isect->labda*isect->vec[1];
		hitco[2] = isect->start[2] + isect->labda*isect->vec[2];
		return 1;
	} else {
		return 0;
	}
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
	
	copy_v3_v3(col, shr_new.combined);
	col[3] = shr_new.alpha;
}

static void vol_trace_behind(ShadeInput *shi, VlakRen *vlr, float *co, float *col)
{
	Isect isect;
	
	VECCOPY(isect.start, co);
	VECCOPY(isect.vec, shi->view);
	isect.labda = FLT_MAX;
	
	isect.mode= RE_RAY_MIRROR;
	isect.skip = RE_SKIP_VLR_NEIGHBOUR | RE_SKIP_VLR_RENDER_CHECK;
	isect.orig.ob = (void*) shi->obi;
	isect.orig.face = (void*)vlr;
	isect.last_hit = NULL;
	isect.lay= -1;
	
	/* check to see if there's anything behind the volume, otherwise shade the sky */
	if(RE_rayobject_raycast(R.raytree, &isect)) {
		shade_intersection(shi, col, &isect);
	} else {
		shadeSkyView(col, co, shi->view, NULL, shi->thread);
		shadeSunView(col, shi->view);
	}
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
	sub_v3_v3v3(dim, bbmax, bbmin);

	sample_co[0] = ((co[0] - bbmin[0]) / dim[0]);
	sample_co[1] = ((co[1] - bbmin[1]) / dim[1]);
	sample_co[2] = ((co[2] - bbmin[2]) / dim[2]);

	scatter_col[0] = voxel_sample_triquadratic(vp->data_r, vp->res, sample_co);
	scatter_col[1] = voxel_sample_triquadratic(vp->data_g, vp->res, sample_co);
	scatter_col[2] = voxel_sample_triquadratic(vp->data_b, vp->res, sample_co);
}

/* Meta object density, brute force for now 
 * (might be good enough anyway, don't need huge number of metaobs to model volumetric objects */
static float metadensity(Object* ob, float* co)
{
	float mat[4][4], imat[4][4], dens = 0.f;
	MetaBall* mb = (MetaBall*)ob->data;
	MetaElem* ml;
	
	/* transform co to meta-element */
	float tco[3] = {co[0], co[1], co[2]};
	mul_m4_m4m4(mat, ob->obmat, R.viewmat);
	invert_m4_m4(imat, mat);
	mul_m4_v3(imat, tco);
	
	for (ml = mb->elems.first; ml; ml=ml->next) {
		float bmat[3][3], dist2;
		
		/* element rotation transform */
		float tp[3] = {ml->x - tco[0], ml->y - tco[1], ml->z - tco[2]};
		quat_to_mat3( bmat,ml->quat);
		transpose_m3(bmat);	// rot.only, so inverse == transpose
		mul_m3_v3(bmat, tp);
		
		/* MB_BALL default */
		switch (ml->type) {
			case MB_ELIPSOID:
				tp[0] /= ml->expx, tp[1] /= ml->expy, tp[2] /= ml->expz;
				break;
			case MB_CUBE:
				tp[2] = (tp[2] > ml->expz) ? (tp[2] - ml->expz) : ((tp[2] < -ml->expz) ? (tp[2] + ml->expz) : 0.f);
				// no break, xy as plane
			case MB_PLANE:
				tp[1] = (tp[1] > ml->expy) ? (tp[1] - ml->expy) : ((tp[1] < -ml->expy) ? (tp[1] + ml->expy) : 0.f);
				// no break, x as tube
			case MB_TUBE:
				tp[0] = (tp[0] > ml->expx) ? (tp[0] - ml->expx) : ((tp[0] < -ml->expx) ? (tp[0] + ml->expx) : 0.f);
		}
		
		/* ml->rad2 is not set */
		dist2 = 1.f - ((tp[0]*tp[0] + tp[1]*tp[1] + tp[2]*tp[2]) / (ml->rad*ml->rad));
		if (dist2 > 0.f)
			dens += (ml->flag & MB_NEGATIVE) ? -ml->s*dist2*dist2*dist2 : ml->s*dist2*dist2*dist2;
	}
	
	dens -= mb->thresh;
	return (dens < 0.f) ? 0.f : dens;
}

float vol_get_density(struct ShadeInput *shi, float *co)
{
	float density = shi->mat->vol.density;
	float density_scale = shi->mat->vol.density_scale;
		
	if (shi->mat->mapto_textured & MAP_DENSITY)
		do_volume_tex(shi, co, MAP_DENSITY, NULL, &density);
	
	// if meta-object, modulate by metadensity without increasing it
	if (shi->obi->obr->ob->type == OB_MBALL) {
		const float md = metadensity(shi->obi->obr->ob, co);
		if (md < 1.f) density *= md;
 	}
	
	return density * density_scale;
}


/* Color of light that gets scattered out by the volume */
/* Uses same physically based scattering parameter as in transmission calculations, 
 * along with artificial reflection scale/reflection color tint */
void vol_get_reflection_color(ShadeInput *shi, float *ref_col, float *co)
{
	float scatter = shi->mat->vol.scattering;
	float reflection= shi->mat->vol.reflection;
	VECCOPY(ref_col, shi->mat->vol.reflection_col);
	
	if (shi->mat->mapto_textured & (MAP_SCATTERING+MAP_REFLECTION_COL))
		do_volume_tex(shi, co, MAP_SCATTERING+MAP_REFLECTION_COL, ref_col, &scatter);
	
	/* only one single float parameter at a time... :s */
	if (shi->mat->mapto_textured & (MAP_REFLECTION))
		do_volume_tex(shi, co, MAP_REFLECTION, NULL, &reflection);
	
	ref_col[0] = reflection * ref_col[0] * scatter;
	ref_col[1] = reflection * ref_col[1] * scatter;
	ref_col[2] = reflection * ref_col[2] * scatter;
}

/* compute emission component, amount of radiance to add per segment
 * can be textured with 'emit' */
void vol_get_emission(ShadeInput *shi, float *emission_col, float *co)
{
	float emission = shi->mat->vol.emission;
	VECCOPY(emission_col, shi->mat->vol.emission_col);
	
	if (shi->mat->mapto_textured & (MAP_EMISSION+MAP_EMISSION_COL))
		do_volume_tex(shi, co, MAP_EMISSION+MAP_EMISSION_COL, emission_col, &emission);
	
	emission_col[0] = emission_col[0] * emission;
	emission_col[1] = emission_col[1] * emission;
	emission_col[2] = emission_col[2] * emission;
}


/* A combination of scattering and absorption -> known as sigma T.
 * This can possibly use a specific scattering colour, 
 * and absorption multiplier factor too, but these parameters are left out for simplicity.
 * It's easy enough to get a good wide range of results with just these two parameters. */
void vol_get_sigma_t(ShadeInput *shi, float *sigma_t, float *co)
{
	/* technically absorption, but named transmission color 
	 * since it describes the effect of the coloring *after* absorption */
	float transmission_col[3] = {shi->mat->vol.transmission_col[0], shi->mat->vol.transmission_col[1], shi->mat->vol.transmission_col[2]};
	float scattering = shi->mat->vol.scattering;
	
	if (shi->mat->mapto_textured & (MAP_SCATTERING+MAP_TRANSMISSION_COL))
		do_volume_tex(shi, co, MAP_SCATTERING+MAP_TRANSMISSION_COL, transmission_col, &scattering);
	
	sigma_t[0] = (1.0f - transmission_col[0]) + scattering;
	sigma_t[1] = (1.0f - transmission_col[1]) + scattering;
	sigma_t[2] = (1.0f - transmission_col[2]) + scattering;
}

/* phase function - determines in which directions the light 
 * is scattered in the volume relative to incoming direction 
 * and view direction */
float vol_get_phasefunc(ShadeInput *shi, float g, float *w, float *wp)
{
	const float normalize = 0.25f; // = 1.f/4.f = M_PI/(4.f*M_PI)
	
	/* normalization constant is 1/4 rather than 1/4pi, since
	 * Blender's shading system doesn't normalise for
	 * energy conservation - eg. multiplying by pdf ( 1/pi for a lambert brdf ).
	 * This means that lambert surfaces in Blender are pi times brighter than they 'should be'
	 * and therefore, with correct energy conservation, volumes will darker than other solid objects,
	 * for the same lighting intensity.
	 * To correct this, scale up the phase function values by pi
	 * until Blender's shading system supports this better. --matt
	 */
	
	if (g == 0.f) {	/* isotropic */
		return normalize * 1.f;
	} else {		/* schlick */
		const float k = 1.55f * g - .55f * g * g * g;
		const float kcostheta = k * dot_v3v3(w, wp);
		return normalize * (1.f - k*k) / ((1.f - kcostheta) * (1.f - kcostheta));
	}
	
	/*
	 * not used, but here for reference:
	switch (phasefunc_type) {
		case MA_VOL_PH_MIEHAZY:
			return normalize * (0.5f + 4.5f * powf(0.5 * (1.f + costheta), 8.f));
		case MA_VOL_PH_MIEMURKY:
			return normalize * (0.5f + 16.5f * powf(0.5 * (1.f + costheta), 32.f));
		case MA_VOL_PH_RAYLEIGH:
			return normalize * 3.f/4.f * (1 + costheta * costheta);
		case MA_VOL_PH_HG:
			return normalize * (1.f - g*g) / powf(1.f + g*g - 2.f * g * costheta, 1.5f));
		case MA_VOL_PH_SCHLICK:
		{
			const float k = 1.55f * g - .55f * g * g * g;
			const float kcostheta = k * costheta;
			return normalize * (1.f - k*k) / ((1.f - kcostheta) * (1.f - kcostheta));
		}
		case MA_VOL_PH_ISOTROPIC:
		default:
			return normalize * 1.f;
	}
	*/
}

/* Compute transmittance = e^(-attenuation) */
void vol_get_transmittance_seg(ShadeInput *shi, float *tr, float stepsize, float *co, float density)
{
	/* input density = density at co */
	float tau[3] = {0.f, 0.f, 0.f};
	const float stepd = density * stepsize;
	float sigma_t[3];
	
	vol_get_sigma_t(shi, sigma_t, co);
	
	/* homogenous volume within the sampled distance */
	tau[0] += stepd * sigma_t[0];
	tau[1] += stepd * sigma_t[1];
	tau[2] += stepd * sigma_t[2];
	
	tr[0] *= exp(-tau[0]);
	tr[1] *= exp(-tau[1]);
	tr[2] *= exp(-tau[2]);
}

/* Compute transmittance = e^(-attenuation) */
static void vol_get_transmittance(ShadeInput *shi, float *tr, float *co, float *endco)
{
	float p[3] = {co[0], co[1], co[2]};
	float step_vec[3] = {endco[0] - co[0], endco[1] - co[1], endco[2] - co[2]};
	float tau[3] = {0.f, 0.f, 0.f};

	float t0 = 0.f;
	float t1 = normalize_v3(step_vec);
	float pt0 = t0;
	
	t0 += shi->mat->vol.stepsize * ((shi->mat->vol.stepsize_type == MA_VOL_STEP_CONSTANT) ? 0.5f : BLI_thread_frand(shi->thread));
	p[0] += t0 * step_vec[0];
	p[1] += t0 * step_vec[1];
	p[2] += t0 * step_vec[2];
	mul_v3_fl(step_vec, shi->mat->vol.stepsize);

	for (; t0 < t1; pt0 = t0, t0 += shi->mat->vol.stepsize) {
		const float d = vol_get_density(shi, p);
		const float stepd = (t0 - pt0) * d;
		float sigma_t[3];
		
		vol_get_sigma_t(shi, sigma_t, co);
		
		tau[0] += stepd * sigma_t[0];
		tau[1] += stepd * sigma_t[1];
		tau[2] += stepd * sigma_t[2];
		
		add_v3_v3v3(p, p, step_vec);
	}
	
	/* return transmittance */
	tr[0] = expf(-tau[0]);
	tr[1] = expf(-tau[1]);
	tr[2] = expf(-tau[2]);
}

void vol_shade_one_lamp(struct ShadeInput *shi, float *co, LampRen *lar, float *lacol)
{
	float visifac, lv[3], lampdist;
	float tr[3]={1.0,1.0,1.0};
	float hitco[3], *atten_co;
	float p, ref_col[3];
	
	if (lar->mode & LA_LAYER) if((lar->lay & shi->obi->lay)==0) return;
	if ((lar->lay & shi->lay)==0) return;
	if (lar->energy == 0.0) return;
	
	if ((visifac= lamp_get_visibility(lar, co, lv, &lampdist)) == 0.f) return;
	
	copy_v3_v3(lacol, &lar->r);
	
	if(lar->mode & LA_TEXTURE) {
		shi->osatex= 0;
		do_lamp_tex(lar, lv, shi, lacol, LA_TEXTURE);
	}

	mul_v3_fl(lacol, visifac);

	if (ELEM(lar->type, LA_SUN, LA_HEMI))
		VECCOPY(lv, lar->vec);
	mul_v3_fl(lv, -1.0f);
	
	if (shi->mat->vol.shade_type == MA_VOL_SHADE_SHADOWED) {
		mul_v3_fl(lacol, vol_get_shadow(shi, lar, co));
	}
	else if (ELEM3(shi->mat->vol.shade_type, MA_VOL_SHADE_SHADED, MA_VOL_SHADE_MULTIPLE, MA_VOL_SHADE_SHADEDPLUSMULTIPLE))
	{
		Isect is;
		
		if (shi->mat->vol.shadeflag & MA_VOL_RECV_EXT_SHADOW) {
			mul_v3_fl(lacol, vol_get_shadow(shi, lar, co));
			if (luminance(lacol) < 0.001f) return;
		}
		
		/* find minimum of volume bounds, or lamp coord */
		if (vol_get_bounds(shi, co, lv, hitco, &is, VOL_BOUNDS_SS)) {
			float dist = len_v3v3(co, hitco);
			VlakRen *vlr = (VlakRen *)is.hit.face;
			
			/* simple internal shadowing */
			if (vlr->mat->material_type == MA_TYPE_SURFACE) {
				lacol[0] = lacol[1] = lacol[2] = 0.0f;
				return;
			}

			if (ELEM(lar->type, LA_SUN, LA_HEMI))
				/* infinite lights, can never be inside volume */
				atten_co = hitco;
			else if ( lampdist < dist ) {
				atten_co = lar->co;
			} else
				atten_co = hitco;
			
			vol_get_transmittance(shi, tr, co, atten_co);
			
			mul_v3_v3v3(lacol, lacol, tr);
		}
		else {
			/* Point is on the outside edge of the volume,
			 * therefore no attenuation, full transmission.
			 * Radiance from lamp remains unchanged */
		}
	}
	
	if (luminance(lacol) < 0.001f) return;
	
	p = vol_get_phasefunc(shi, shi->mat->vol.asymmetry, shi->view, lv);
	
	/* physically based scattering with non-physically based RGB gain */
	vol_get_reflection_color(shi, ref_col, co);
	
	lacol[0] *= p * ref_col[0];
	lacol[1] *= p * ref_col[1];
	lacol[2] *= p * ref_col[2];
}

/* single scattering only for now */
void vol_get_scattering(ShadeInput *shi, float *scatter_col, float *co)
{
	ListBase *lights;
	GroupObject *go;
	LampRen *lar;
	
	scatter_col[0] = scatter_col[1] = scatter_col[2] = 0.f;
	
	lights= get_lights(shi);
	for(go=lights->first; go; go= go->next)
	{
		float lacol[3] = {0.f, 0.f, 0.f};
		lar= go->lampren;
		
		if (lar) {
			vol_shade_one_lamp(shi, co, lar, lacol);
			add_v3_v3v3(scatter_col, scatter_col, lacol);
		}
	}
}

	
/*
The main volumetric integrator, using an emission/absorption/scattering model.

Incoming radiance = 

outgoing radiance from behind surface * beam transmittance/attenuation
+ added radiance from all points along the ray due to participating media
	--> radiance for each segment = 
		(radiance added by scattering + radiance added by emission) * beam transmittance/attenuation
*/

/* For ease of use, I've also introduced a 'reflection' and 'reflection color' parameter, which isn't 
 * physically correct. This works as an RGB tint/gain on out-scattered light, but doesn't affect the light 
 * that is transmitted through the volume. While having wavelength dependent absorption/scattering is more correct,
 * it also makes it harder to control the overall look of the volume since colouring the outscattered light results
 * in the inverse colour being transmitted through the rest of the volume.
 */
static void volumeintegrate(struct ShadeInput *shi, float *col, float *co, float *endco)
{
	float radiance[3] = {0.f, 0.f, 0.f};
	float tr[3] = {1.f, 1.f, 1.f};
	float p[3] = {co[0], co[1], co[2]};
	float step_vec[3] = {endco[0] - co[0], endco[1] - co[1], endco[2] - co[2]};
	const float stepsize = shi->mat->vol.stepsize;
	
	float t0 = 0.f;
	float pt0 = t0;
	float t1 = normalize_v3(step_vec);	/* returns vector length */
	
	t0 += stepsize * ((shi->mat->vol.stepsize_type == MA_VOL_STEP_CONSTANT) ? 0.5f : BLI_thread_frand(shi->thread));
	p[0] += t0 * step_vec[0];
	p[1] += t0 * step_vec[1];
	p[2] += t0 * step_vec[2];
	mul_v3_fl(step_vec, stepsize);
	
	for (; t0 < t1; pt0 = t0, t0 += stepsize) {
		const float density = vol_get_density(shi, p);
		
		if (density > 0.01f) {
			float scatter_col[3], emit_col[3];
			const float stepd = (t0 - pt0) * density;
			
			/* transmittance component (alpha) */
			vol_get_transmittance_seg(shi, tr, stepsize, co, density);
			
			if (luminance(tr) < shi->mat->vol.depth_cutoff) break;
			
			vol_get_emission(shi, emit_col, p);
			
			if (shi->obi->volume_precache) {
				float p2[3];
				
				p2[0] = p[0] + (step_vec[0] * 0.5);
				p2[1] = p[1] + (step_vec[1] * 0.5);
				p2[2] = p[2] + (step_vec[2] * 0.5);
				
				vol_get_precached_scattering(shi, scatter_col, p2);
			} else
				vol_get_scattering(shi, scatter_col, p);
			
			radiance[0] += stepd * tr[0] * (emit_col[0] + scatter_col[0]);
			radiance[1] += stepd * tr[1] * (emit_col[1] + scatter_col[1]);
			radiance[2] += stepd * tr[2] * (emit_col[2] + scatter_col[2]);
		}
		add_v3_v3v3(p, p, step_vec);
	}
	
	/* multiply original color (from behind volume) with transmittance over entire distance */
	mul_v3_v3v3(col, tr, col);
	add_v3_v3v3(col, col, radiance);
	
	/* alpha <-- transmission luminance */
	col[3] = 1.0f - luminance(tr);
}

/* the main entry point for volume shading */
static void volume_trace(struct ShadeInput *shi, struct ShadeResult *shr, int inside_volume)
{
	float hitco[3], col[4] = {0.f,0.f,0.f,0.f};
	float *startco, *endco;
	int trace_behind = 1;
	const int ztransp= ((shi->depth==0) && (shi->mat->mode & MA_TRANSP) && (shi->mat->mode & MA_ZTRANSP));
	Isect is;

	/* check for shading an internal face a volume object directly */
	if (inside_volume == VOL_SHADE_INSIDE)
		trace_behind = 0;
	else if (inside_volume == VOL_SHADE_OUTSIDE) {
		if (shi->flippednor)
			inside_volume = VOL_SHADE_INSIDE;
	}
	
	if (ztransp && inside_volume == VOL_SHADE_INSIDE) {
		MatInside *mi;
		int render_this=0;
		
		/* don't render the backfaces of ztransp volume materials.
		 
		 * volume shading renders the internal volume from between the
		 * ' view intersection of the solid volume to the
		 * intersection on the other side, as part of the shading of
		 * the front face.
		 
		 * Because ztransp renders both front and back faces independently
		 * this will double up, so here we prevent rendering the backface as well, 
		 * which would otherwise render the volume in between the camera and the backface
		 * --matt */
		
		for (mi=R.render_volumes_inside.first; mi; mi=mi->next) {
			/* weak... */
			if (mi->ma == shi->mat) render_this=1;
		}
		if (!render_this) return;
	}
	

	if (inside_volume == VOL_SHADE_INSIDE)
	{
		startco = shi->camera_co;
		endco = shi->co;
		
		if (trace_behind) {
			if (!ztransp)
				/* trace behind the volume object */
				vol_trace_behind(shi, shi->vlr, endco, col);
		} else {
			/* we're tracing through the volume between the camera 
			 * and a solid surface, so use that pre-shaded radiance */
			QUATCOPY(col, shr->combined);
		}
		
		/* shade volume from 'camera' to 1st hit point */
		volumeintegrate(shi, col, startco, endco);
	}
	/* trace to find a backface, the other side bounds of the volume */
	/* (ray intersect ignores front faces here) */
	else if (vol_get_bounds(shi, shi->co, shi->view, hitco, &is, VOL_BOUNDS_DEPTH))
	{
		VlakRen *vlr = (VlakRen *)is.hit.face;
		
		startco = shi->co;
		endco = hitco;
		
		if (!ztransp) {
			/* if it's another face in the same material */
			if (vlr->mat == shi->mat) {
				/* trace behind the 2nd (raytrace) hit point */
				vol_trace_behind(shi, (VlakRen *)is.hit.face, endco, col);
			} else {
				shade_intersection(shi, col, &is);
			}
		}
		
		/* shade volume from 1st hit point to 2nd hit point */
		volumeintegrate(shi, col, startco, endco);
	}
	
	if (ztransp)
		col[3] = col[3]>1.f?1.f:col[3];
	else
		col[3] = 1.f;
	
	copy_v3_v3(shr->combined, col);
	shr->alpha = col[3];
	
	VECCOPY(shr->diff, shr->combined);
}

/* Traces a shadow through the object, 
 * pretty much gets the transmission over a ray path */
void shade_volume_shadow(struct ShadeInput *shi, struct ShadeResult *shr, struct Isect *last_is)
{
	float hitco[3];
	float tr[3] = {1.0,1.0,1.0};
	Isect is;
	float *startco, *endco;
	float density=0.f;

	memset(shr, 0, sizeof(ShadeResult));
	
	/* if 1st hit normal is facing away from the camera, 
	 * then we're inside the volume already. */
	if (shi->flippednor) {
		startco = last_is->start;
		endco = shi->co;
	}
	/* trace to find a backface, the other side bounds of the volume */
	/* (ray intersect ignores front faces here) */
	else if (vol_get_bounds(shi, shi->co, shi->view, hitco, &is, VOL_BOUNDS_DEPTH)) {
		startco = shi->co;
		endco = hitco;
	}
	else {
		shr->combined[0] = shr->combined[1] = shr->combined[2] = 0.f;
		shr->alpha = shr->combined[3] = 1.f;
		return;
	}
	
	density = vol_get_density(shi, startco);
	vol_get_transmittance(shi, tr, startco, endco);
	
	copy_v3_v3(shr->combined, tr);
	shr->combined[3] = 1.0f - luminance(tr);
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
	ObjectInstanceRen *obi_backup;
	float prev_alpha = shr->alpha;
	
	//if (BLI_countlist(&R.render_volumes_inside) == 0) return;
	
	/* XXX: extend to multiple volumes perhaps later */
	mat_backup = shi->mat;
	obi_backup = shi->obi;
	
	m = R.render_volumes_inside.first;
	shi->mat = m->ma;
	shi->obi = m->obi;
	shi->obr = m->obi->obr;
	
	volume_trace(shi, shr, VOL_SHADE_INSIDE);
	shr->alpha += prev_alpha;
	CLAMP(shr->alpha, 0.f, 1.f);
	
	shi->mat = mat_backup;
	shi->obi = obi_backup;
	shi->obr = obi_backup->obr;
}
