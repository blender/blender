/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

/*
 * ShaderData, used in four steps:
 *
 * Setup from incoming ray, sampled position and background.
 * Execute for surface, volume or displacement.
 * Evaluate one or more closures.
 * Release.
 *
 */

#include "closure/bsdf_util.h"
#include "closure/bsdf.h"
#include "closure/emissive.h"

#include "svm/svm.h"

CCL_NAMESPACE_BEGIN

/* ShaderData setup from incoming ray */

#ifdef __OBJECT_MOTION__
ccl_device void shader_setup_object_transforms(KernelGlobals *kg, ShaderData *sd, float time)
{
	if(sd->flag & SD_OBJECT_MOTION) {
		sd->ob_tfm = object_fetch_transform_motion(kg, sd->object, time);
		sd->ob_itfm = transform_quick_inverse(sd->ob_tfm);
	}
	else {
		sd->ob_tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
		sd->ob_itfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
	}
}
#endif

ccl_device void shader_setup_from_ray(KernelGlobals *kg, ShaderData *sd,
	const Intersection *isect, const Ray *ray, int bounce, int transparent_bounce)
{
#ifdef __INSTANCING__
	sd->object = (isect->object == PRIM_NONE)? kernel_tex_fetch(__prim_object, isect->prim): isect->object;
#endif

	sd->type = isect->type;
	sd->flag = kernel_tex_fetch(__object_flag, sd->object);

	/* matrices and time */
#ifdef __OBJECT_MOTION__
	shader_setup_object_transforms(kg, sd, ray->time);
	sd->time = ray->time;
#endif

	sd->prim = kernel_tex_fetch(__prim_index, isect->prim);
	sd->ray_length = isect->t;
	sd->ray_depth = bounce;
	sd->transparent_depth = transparent_bounce;

#ifdef __UV__
	sd->u = isect->u;
	sd->v = isect->v;
#endif

#ifdef __HAIR__
	if(sd->type & PRIMITIVE_ALL_CURVE) {
		/* curve */
		float4 curvedata = kernel_tex_fetch(__curves, sd->prim);

		sd->shader = __float_as_int(curvedata.z);
		sd->P = bvh_curve_refine(kg, sd, isect, ray);
	}
	else
#endif
	if(sd->type & PRIMITIVE_TRIANGLE) {
		/* static triangle */
		float4 Ns = kernel_tex_fetch(__tri_normal, sd->prim);
		float3 Ng = make_float3(Ns.x, Ns.y, Ns.z);
		sd->shader = __float_as_int(Ns.w);

		/* vectors */
		sd->P = triangle_refine(kg, sd, isect, ray);
		sd->Ng = Ng;
		sd->N = Ng;
		
		/* smooth normal */
		if(sd->shader & SHADER_SMOOTH_NORMAL)
			sd->N = triangle_smooth_normal(kg, sd->prim, sd->u, sd->v);

#ifdef __DPDU__
		/* dPdu/dPdv */
		triangle_dPdudv(kg, sd->prim, &sd->dPdu, &sd->dPdv);
#endif
	}
	else {
		/* motion triangle */
		motion_triangle_shader_setup(kg, sd, isect, ray, false);
	}

	sd->I = -ray->D;

	sd->flag |= kernel_tex_fetch(__shader_flag, (sd->shader & SHADER_MASK)*2);

#ifdef __INSTANCING__
	if(isect->object != OBJECT_NONE) {
		/* instance transform */
		object_normal_transform(kg, sd, &sd->N);
		object_normal_transform(kg, sd, &sd->Ng);
#ifdef __DPDU__
		object_dir_transform(kg, sd, &sd->dPdu);
		object_dir_transform(kg, sd, &sd->dPdv);
#endif
	}
#endif

	/* backfacing test */
	bool backfacing = (dot(sd->Ng, sd->I) < 0.0f);

	if(backfacing) {
		sd->flag |= SD_BACKFACING;
		sd->Ng = -sd->Ng;
		sd->N = -sd->N;
#ifdef __DPDU__
		sd->dPdu = -sd->dPdu;
		sd->dPdv = -sd->dPdv;
#endif
	}

#ifdef __RAY_DIFFERENTIALS__
	/* differentials */
	differential_transfer(&sd->dP, ray->dP, ray->D, ray->dD, sd->Ng, isect->t);
	differential_incoming(&sd->dI, ray->dD);
	differential_dudv(&sd->du, &sd->dv, sd->dPdu, sd->dPdv, sd->dP, sd->Ng);
#endif
}

/* ShaderData setup from BSSRDF scatter */

#ifdef __SUBSURFACE__
ccl_device_inline void shader_setup_from_subsurface(KernelGlobals *kg, ShaderData *sd,
	const Intersection *isect, const Ray *ray)
{
	bool backfacing = sd->flag & SD_BACKFACING;

	/* object, matrices, time, ray_length stay the same */
	sd->flag = kernel_tex_fetch(__object_flag, sd->object);
	sd->prim = kernel_tex_fetch(__prim_index, isect->prim);
	sd->type = isect->type;

#ifdef __UV__
	sd->u = isect->u;
	sd->v = isect->v;
#endif

	/* fetch triangle data */
	if(sd->type == PRIMITIVE_TRIANGLE) {
		float4 Ns = kernel_tex_fetch(__tri_normal, sd->prim);
		float3 Ng = make_float3(Ns.x, Ns.y, Ns.z);
		sd->shader = __float_as_int(Ns.w);

		/* static triangle */
		sd->P = triangle_refine_subsurface(kg, sd, isect, ray);
		sd->Ng = Ng;
		sd->N = Ng;

		if(sd->shader & SHADER_SMOOTH_NORMAL)
			sd->N = triangle_smooth_normal(kg, sd->prim, sd->u, sd->v);

#ifdef __DPDU__
		/* dPdu/dPdv */
		triangle_dPdudv(kg, sd->prim, &sd->dPdu, &sd->dPdv);
#endif
	}
	else {
		/* motion triangle */
		motion_triangle_shader_setup(kg, sd, isect, ray, true);
	}

	sd->flag |= kernel_tex_fetch(__shader_flag, (sd->shader & SHADER_MASK)*2);

#ifdef __INSTANCING__
	if(isect->object != OBJECT_NONE) {
		/* instance transform */
		object_normal_transform(kg, sd, &sd->N);
		object_normal_transform(kg, sd, &sd->Ng);
#ifdef __DPDU__
		object_dir_transform(kg, sd, &sd->dPdu);
		object_dir_transform(kg, sd, &sd->dPdv);
#endif
	}
#endif

	/* backfacing test */
	if(backfacing) {
		sd->flag |= SD_BACKFACING;
		sd->Ng = -sd->Ng;
		sd->N = -sd->N;
#ifdef __DPDU__
		sd->dPdu = -sd->dPdu;
		sd->dPdv = -sd->dPdv;
#endif
	}

	/* should not get used in principle as the shading will only use a diffuse
	 * BSDF, but the shader might still access it */
	sd->I = sd->N;

#ifdef __RAY_DIFFERENTIALS__
	/* differentials */
	differential_dudv(&sd->du, &sd->dv, sd->dPdu, sd->dPdv, sd->dP, sd->Ng);
	/* don't modify dP and dI */
#endif
}
#endif

/* ShaderData setup from position sampled on mesh */

ccl_device void shader_setup_from_sample(KernelGlobals *kg, ShaderData *sd,
	const float3 P, const float3 Ng, const float3 I,
	int shader, int object, int prim, float u, float v, float t, float time, int bounce, int transparent_bounce)
{
	/* vectors */
	sd->P = P;
	sd->N = Ng;
	sd->Ng = Ng;
	sd->I = I;
	sd->shader = shader;
	sd->type = (prim == PRIM_NONE)? PRIMITIVE_NONE: PRIMITIVE_TRIANGLE;

	/* primitive */
#ifdef __INSTANCING__
	sd->object = object;
#endif
	/* currently no access to bvh prim index for strand sd->prim*/
	sd->prim = prim;
#ifdef __UV__
	sd->u = u;
	sd->v = v;
#endif
	sd->ray_length = t;
	sd->ray_depth = bounce;
	sd->transparent_depth = transparent_bounce;

	/* detect instancing, for non-instanced the object index is -object-1 */
#ifdef __INSTANCING__
	bool instanced = false;

	if(sd->prim != PRIM_NONE) {
		if(sd->object >= 0)
			instanced = true;
		else
#endif
			sd->object = ~sd->object;
#ifdef __INSTANCING__
	}
#endif

	sd->flag = kernel_tex_fetch(__shader_flag, (sd->shader & SHADER_MASK)*2);
	if(sd->object != OBJECT_NONE) {
		sd->flag |= kernel_tex_fetch(__object_flag, sd->object);

#ifdef __OBJECT_MOTION__
		shader_setup_object_transforms(kg, sd, time);
	}

	sd->time = time;
#else
	}
#endif

	if(sd->type & PRIMITIVE_TRIANGLE) {
		/* smooth normal */
		if(sd->shader & SHADER_SMOOTH_NORMAL) {
			sd->N = triangle_smooth_normal(kg, sd->prim, sd->u, sd->v);

#ifdef __INSTANCING__
			if(instanced)
				object_normal_transform(kg, sd, &sd->N);
#endif
		}

		/* dPdu/dPdv */
#ifdef __DPDU__
		triangle_dPdudv(kg, sd->prim, &sd->dPdu, &sd->dPdv);

#ifdef __INSTANCING__
		if(instanced) {
			object_dir_transform(kg, sd, &sd->dPdu);
			object_dir_transform(kg, sd, &sd->dPdv);
		}
#endif
#endif
	}
	else {
#ifdef __DPDU__
		sd->dPdu = make_float3(0.0f, 0.0f, 0.0f);
		sd->dPdv = make_float3(0.0f, 0.0f, 0.0f);
#endif
	}

	/* backfacing test */
	if(sd->prim != PRIM_NONE) {
		bool backfacing = (dot(sd->Ng, sd->I) < 0.0f);

		if(backfacing) {
			sd->flag |= SD_BACKFACING;
			sd->Ng = -sd->Ng;
			sd->N = -sd->N;
#ifdef __DPDU__
			sd->dPdu = -sd->dPdu;
			sd->dPdv = -sd->dPdv;
#endif
		}
	}

#ifdef __RAY_DIFFERENTIALS__
	/* no ray differentials here yet */
	sd->dP = differential3_zero();
	sd->dI = differential3_zero();
	sd->du = differential_zero();
	sd->dv = differential_zero();
#endif
}

/* ShaderData setup for displacement */

ccl_device void shader_setup_from_displace(KernelGlobals *kg, ShaderData *sd,
	int object, int prim, float u, float v)
{
	float3 P, Ng, I = make_float3(0.0f, 0.0f, 0.0f);
	int shader;

	triangle_point_normal(kg, prim, u, v, &P, &Ng, &shader);

	/* force smooth shading for displacement */
	shader |= SHADER_SMOOTH_NORMAL;

	/* watch out: no instance transform currently */

	shader_setup_from_sample(kg, sd, P, Ng, I, shader, object, prim, u, v, 0.0f, TIME_INVALID, 0, 0);
}

/* ShaderData setup from ray into background */

ccl_device_inline void shader_setup_from_background(KernelGlobals *kg, ShaderData *sd, const Ray *ray, int bounce, int transparent_bounce)
{
	/* vectors */
	sd->P = ray->D;
	sd->N = -ray->D;
	sd->Ng = -ray->D;
	sd->I = -ray->D;
	sd->shader = kernel_data.background.surface_shader;
	sd->flag = kernel_tex_fetch(__shader_flag, (sd->shader & SHADER_MASK)*2);
#ifdef __OBJECT_MOTION__
	sd->time = ray->time;
#endif
	sd->ray_length = 0.0f;
	sd->ray_depth = bounce;
	sd->transparent_depth = transparent_bounce;

#ifdef __INSTANCING__
	sd->object = PRIM_NONE;
#endif
	sd->prim = PRIM_NONE;
#ifdef __UV__
	sd->u = 0.0f;
	sd->v = 0.0f;
#endif

#ifdef __DPDU__
	/* dPdu/dPdv */
	sd->dPdu = make_float3(0.0f, 0.0f, 0.0f);
	sd->dPdv = make_float3(0.0f, 0.0f, 0.0f);
#endif

#ifdef __RAY_DIFFERENTIALS__
	/* differentials */
	sd->dP = ray->dD;
	differential_incoming(&sd->dI, sd->dP);
	sd->du.dx = 0.0f;
	sd->du.dy = 0.0f;
	sd->dv.dx = 0.0f;
	sd->dv.dy = 0.0f;
#endif
}

/* ShaderData setup from point inside volume */

ccl_device_inline void shader_setup_from_volume(KernelGlobals *kg, ShaderData *sd, const Ray *ray, int bounce, int transparent_bounce)
{
	/* vectors */
	sd->P = ray->P;
	sd->N = -ray->D;  
	sd->Ng = -ray->D;
	sd->I = -ray->D;
	sd->shader = SHADER_NONE;
	sd->flag = 0;
#ifdef __OBJECT_MOTION__
	sd->time = ray->time;
#endif
	sd->ray_length = 0.0f; /* todo: can we set this to some useful value? */
	sd->ray_depth = bounce;
	sd->transparent_depth = transparent_bounce;

#ifdef __INSTANCING__
	sd->object = PRIM_NONE; /* todo: fill this for texture coordinates */
#endif
	sd->prim = PRIM_NONE;
	sd->type = PRIMITIVE_NONE;

#ifdef __UV__
	sd->u = 0.0f;
	sd->v = 0.0f;
#endif

#ifdef __DPDU__
	/* dPdu/dPdv */
	sd->dPdu = make_float3(0.0f, 0.0f, 0.0f);
	sd->dPdv = make_float3(0.0f, 0.0f, 0.0f);
#endif

#ifdef __RAY_DIFFERENTIALS__
	/* differentials */
	sd->dP = ray->dD;
	differential_incoming(&sd->dI, sd->dP);
	sd->du = differential_zero();
	sd->dv = differential_zero();
#endif

	/* for NDC coordinates */
	sd->ray_P = ray->P;
	sd->ray_dP = ray->dP;
}

/* Merging */

#if defined(__BRANCHED_PATH__) || defined(__VOLUME__)
ccl_device void shader_merge_closures(ShaderData *sd)
{
	/* merge identical closures, better when we sample a single closure at a time */
	for(int i = 0; i < sd->num_closure; i++) {
		ShaderClosure *sci = &sd->closure[i];

		for(int j = i + 1; j < sd->num_closure; j++) {
			ShaderClosure *scj = &sd->closure[j];

#ifdef __OSL__
			if(sci->prim || scj->prim)
				continue;
#endif

			if(!(sci->type == scj->type && sci->data0 == scj->data0 && sci->data1 == scj->data1))
				continue;

			if(CLOSURE_IS_BSDF_OR_BSSRDF(sci->type)) {
				if(sci->N != scj->N)
					continue;
				else if(CLOSURE_IS_BSDF_ANISOTROPIC(sci->type) && sci->T != scj->T)
					continue;
			}

			sci->weight += scj->weight;
			sci->sample_weight += scj->sample_weight;

			int size = sd->num_closure - (j+1);
			if(size > 0) {
				for(int k = 0; k < size; k++) {
					scj[k] = scj[k+1];
				}
			}

			sd->num_closure--;
			j--;
		}
	}
}
#endif

/* BSDF */

ccl_device_inline void _shader_bsdf_multi_eval(KernelGlobals *kg, const ShaderData *sd, const float3 omega_in, float *pdf,
	int skip_bsdf, BsdfEval *result_eval, float sum_pdf, float sum_sample_weight)
{
	/* this is the veach one-sample model with balance heuristic, some pdf
	 * factors drop out when using balance heuristic weighting */
	for(int i = 0; i< sd->num_closure; i++) {
		if(i == skip_bsdf)
			continue;

		const ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_BSDF(sc->type)) {
			float bsdf_pdf = 0.0f;
			float3 eval = bsdf_eval(kg, sd, sc, omega_in, &bsdf_pdf);

			if(bsdf_pdf != 0.0f) {
				bsdf_eval_accum(result_eval, sc->type, eval*sc->weight);
				sum_pdf += bsdf_pdf*sc->sample_weight;
			}

			sum_sample_weight += sc->sample_weight;
		}
	}

	*pdf = (sum_sample_weight > 0.0f)? sum_pdf/sum_sample_weight: 0.0f;
}

ccl_device void shader_bsdf_eval(KernelGlobals *kg, const ShaderData *sd,
	const float3 omega_in, BsdfEval *eval, float *pdf)
{
	bsdf_eval_init(eval, NBUILTIN_CLOSURES, make_float3(0.0f, 0.0f, 0.0f), kernel_data.film.use_light_pass);

	_shader_bsdf_multi_eval(kg, sd, omega_in, pdf, -1, eval, 0.0f, 0.0f);
}

ccl_device int shader_bsdf_sample(KernelGlobals *kg, const ShaderData *sd,
	float randu, float randv, BsdfEval *bsdf_eval,
	float3 *omega_in, differential3 *domega_in, float *pdf)
{
	int sampled = 0;

	if(sd->num_closure > 1) {
		/* pick a BSDF closure based on sample weights */
		float sum = 0.0f;

		for(sampled = 0; sampled < sd->num_closure; sampled++) {
			const ShaderClosure *sc = &sd->closure[sampled];
			
			if(CLOSURE_IS_BSDF(sc->type))
				sum += sc->sample_weight;
		}

		float r = sd->randb_closure*sum;
		sum = 0.0f;

		for(sampled = 0; sampled < sd->num_closure; sampled++) {
			const ShaderClosure *sc = &sd->closure[sampled];
			
			if(CLOSURE_IS_BSDF(sc->type)) {
				sum += sc->sample_weight;

				if(r <= sum)
					break;
			}
		}

		if(sampled == sd->num_closure) {
			*pdf = 0.0f;
			return LABEL_NONE;
		}
	}

	const ShaderClosure *sc = &sd->closure[sampled];
	int label;
	float3 eval;

	*pdf = 0.0f;
	label = bsdf_sample(kg, sd, sc, randu, randv, &eval, omega_in, domega_in, pdf);

	if(*pdf != 0.0f) {
		bsdf_eval_init(bsdf_eval, sc->type, eval*sc->weight, kernel_data.film.use_light_pass);

		if(sd->num_closure > 1) {
			float sweight = sc->sample_weight;
			_shader_bsdf_multi_eval(kg, sd, *omega_in, pdf, sampled, bsdf_eval, *pdf*sweight, sweight);
		}
	}

	return label;
}

ccl_device int shader_bsdf_sample_closure(KernelGlobals *kg, const ShaderData *sd,
	const ShaderClosure *sc, float randu, float randv, BsdfEval *bsdf_eval,
	float3 *omega_in, differential3 *domega_in, float *pdf)
{
	int label;
	float3 eval;

	*pdf = 0.0f;
	label = bsdf_sample(kg, sd, sc, randu, randv, &eval, omega_in, domega_in, pdf);

	if(*pdf != 0.0f)
		bsdf_eval_init(bsdf_eval, sc->type, eval*sc->weight, kernel_data.film.use_light_pass);

	return label;
}

ccl_device void shader_bsdf_blur(KernelGlobals *kg, ShaderData *sd, float roughness)
{
	for(int i = 0; i< sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_BSDF(sc->type))
			bsdf_blur(kg, sc, roughness);
	}
}

ccl_device float3 shader_bsdf_transparency(KernelGlobals *kg, ShaderData *sd)
{
	float3 eval = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i< sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(sc->type == CLOSURE_BSDF_TRANSPARENT_ID) // todo: make this work for osl
			eval += sc->weight;
	}

	return eval;
}

ccl_device float3 shader_bsdf_alpha(KernelGlobals *kg, ShaderData *sd)
{
	float3 alpha = make_float3(1.0f, 1.0f, 1.0f) - shader_bsdf_transparency(kg, sd);

	alpha = max(alpha, make_float3(0.0f, 0.0f, 0.0f));
	alpha = min(alpha, make_float3(1.0f, 1.0f, 1.0f));
	
	return alpha;
}

ccl_device float3 shader_bsdf_diffuse(KernelGlobals *kg, ShaderData *sd)
{
	float3 eval = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i< sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_BSDF_DIFFUSE(sc->type))
			eval += sc->weight;
	}

	return eval;
}

ccl_device float3 shader_bsdf_glossy(KernelGlobals *kg, ShaderData *sd)
{
	float3 eval = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i< sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_BSDF_GLOSSY(sc->type))
			eval += sc->weight;
	}

	return eval;
}

ccl_device float3 shader_bsdf_transmission(KernelGlobals *kg, ShaderData *sd)
{
	float3 eval = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i< sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_BSDF_TRANSMISSION(sc->type))
			eval += sc->weight;
	}

	return eval;
}

ccl_device float3 shader_bsdf_subsurface(KernelGlobals *kg, ShaderData *sd)
{
	float3 eval = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i< sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_BSSRDF(sc->type))
			eval += sc->weight;
	}

	return eval;
}

ccl_device float3 shader_bsdf_ao(KernelGlobals *kg, ShaderData *sd, float ao_factor, float3 *N_)
{
	float3 eval = make_float3(0.0f, 0.0f, 0.0f);
	float3 N = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i< sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_BSDF_DIFFUSE(sc->type)) {
			eval += sc->weight*ao_factor;
			N += sc->N*average(sc->weight);
		}
		else if(CLOSURE_IS_AMBIENT_OCCLUSION(sc->type)) {
			eval += sc->weight;
			N += sd->N*average(sc->weight);
		}
	}

	if(is_zero(N))
		N = sd->N;
	else
		N = normalize(N);

	*N_ = N;
	return eval;
}

ccl_device float3 shader_bssrdf_sum(ShaderData *sd, float3 *N_, float *texture_blur_)
{
	float3 eval = make_float3(0.0f, 0.0f, 0.0f);
	float3 N = make_float3(0.0f, 0.0f, 0.0f);
	float texture_blur = 0.0f, weight_sum = 0.0f;

	for(int i = 0; i< sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_BSSRDF(sc->type)) {
			float avg_weight = fabsf(average(sc->weight));

			N += sc->N*avg_weight;
			eval += sc->weight;
			texture_blur += sc->data1*avg_weight;
			weight_sum += avg_weight;
		}
	}

	if(N_)
		*N_ = (is_zero(N))? sd->N: normalize(N);

	if(texture_blur_)
		*texture_blur_ = texture_blur/weight_sum;
	
	return eval;
}

/* Emission */

ccl_device float3 emissive_eval(KernelGlobals *kg, ShaderData *sd, ShaderClosure *sc)
{
	return emissive_simple_eval(sd->Ng, sd->I);
}

ccl_device float3 shader_emissive_eval(KernelGlobals *kg, ShaderData *sd)
{
	float3 eval;
	eval = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_EMISSION(sc->type))
			eval += emissive_eval(kg, sd, sc)*sc->weight;
	}

	return eval;
}

/* Holdout */

ccl_device float3 shader_holdout_eval(KernelGlobals *kg, ShaderData *sd)
{
	float3 weight = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_HOLDOUT(sc->type))
			weight += sc->weight;
	}

	return weight;
}

/* Surface Evaluation */

ccl_device void shader_eval_surface(KernelGlobals *kg, ShaderData *sd,
	float randb, int path_flag, ShaderContext ctx)
{
	sd->num_closure = 0;
	sd->randb_closure = randb;

#ifdef __OSL__
	if(kg->osl)
		OSLShader::eval_surface(kg, sd, path_flag, ctx);
	else
#endif
	{
#ifdef __SVM__
		svm_eval_nodes(kg, sd, SHADER_TYPE_SURFACE, path_flag);
#else
		sd->closure.weight = make_float3(0.8f, 0.8f, 0.8f);
		sd->closure.N = sd->N;
		sd->flag |= bsdf_diffuse_setup(&sd->closure);
#endif
	}
}

/* Background Evaluation */

ccl_device float3 shader_eval_background(KernelGlobals *kg, ShaderData *sd, int path_flag, ShaderContext ctx)
{
	sd->num_closure = 0;
	sd->randb_closure = 0.0f;

#ifdef __OSL__
	if(kg->osl) {
		return OSLShader::eval_background(kg, sd, path_flag, ctx);
	}
	else
#endif

	{
#ifdef __SVM__
		svm_eval_nodes(kg, sd, SHADER_TYPE_SURFACE, path_flag);

		float3 eval = make_float3(0.0f, 0.0f, 0.0f);

		for(int i = 0; i< sd->num_closure; i++) {
			const ShaderClosure *sc = &sd->closure[i];

			if(CLOSURE_IS_BACKGROUND(sc->type))
				eval += sc->weight;
		}

		return eval;
#else
		return make_float3(0.8f, 0.8f, 0.8f);
#endif
	}
}

/* Volume */

#ifdef __VOLUME__

ccl_device_inline void _shader_volume_phase_multi_eval(const ShaderData *sd, const float3 omega_in, float *pdf,
	int skip_phase, BsdfEval *result_eval, float sum_pdf, float sum_sample_weight)
{
	for(int i = 0; i< sd->num_closure; i++) {
		if(i == skip_phase)
			continue;

		const ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_PHASE(sc->type)) {
			float phase_pdf = 0.0f;
			float3 eval = volume_phase_eval(sd, sc, omega_in, &phase_pdf);

			if(phase_pdf != 0.0f) {
				bsdf_eval_accum(result_eval, sc->type, eval);
				sum_pdf += phase_pdf;
			}

			sum_sample_weight += sc->sample_weight;
		}
	}

	*pdf = (sum_sample_weight > 0.0f)? sum_pdf/sum_sample_weight: 0.0f;
}

ccl_device void shader_volume_phase_eval(KernelGlobals *kg, const ShaderData *sd,
	const float3 omega_in, BsdfEval *eval, float *pdf)
{
	bsdf_eval_init(eval, NBUILTIN_CLOSURES, make_float3(0.0f, 0.0f, 0.0f), kernel_data.film.use_light_pass);

	_shader_volume_phase_multi_eval(sd, omega_in, pdf, -1, eval, 0.0f, 0.0f);
}

ccl_device int shader_volume_phase_sample(KernelGlobals *kg, const ShaderData *sd,
	float randu, float randv, BsdfEval *phase_eval,
	float3 *omega_in, differential3 *domega_in, float *pdf)
{
	int sampled = 0;

	if(sd->num_closure > 1) {
		/* pick a phase closure based on sample weights */
		float sum = 0.0f;

		for(sampled = 0; sampled < sd->num_closure; sampled++) {
			const ShaderClosure *sc = &sd->closure[sampled];
			
			if(CLOSURE_IS_PHASE(sc->type))
				sum += sc->sample_weight;
		}

		float r = sd->randb_closure*sum;
		sum = 0.0f;

		for(sampled = 0; sampled < sd->num_closure; sampled++) {
			const ShaderClosure *sc = &sd->closure[sampled];
			
			if(CLOSURE_IS_PHASE(sc->type)) {
				sum += sc->sample_weight;

				if(r <= sum)
					break;
			}
		}

		if(sampled == sd->num_closure) {
			*pdf = 0.0f;
			return LABEL_NONE;
		}
	}

	/* todo: this isn't quite correct, we don't weight anisotropy properly
	 * depending on color channels, even if this is perhaps not a common case */
	const ShaderClosure *sc = &sd->closure[sampled];
	int label;
	float3 eval;

	*pdf = 0.0f;
	label = volume_phase_sample(sd, sc, randu, randv, &eval, omega_in, domega_in, pdf);

	if(*pdf != 0.0f) {
		bsdf_eval_init(phase_eval, sc->type, eval, kernel_data.film.use_light_pass);
	}

	return label;
}

ccl_device int shader_phase_sample_closure(KernelGlobals *kg, const ShaderData *sd,
	const ShaderClosure *sc, float randu, float randv, BsdfEval *phase_eval,
	float3 *omega_in, differential3 *domega_in, float *pdf)
{
	int label;
	float3 eval;

	*pdf = 0.0f;
	label = volume_phase_sample(sd, sc, randu, randv, &eval, omega_in, domega_in, pdf);

	if(*pdf != 0.0f)
		bsdf_eval_init(phase_eval, sc->type, eval, kernel_data.film.use_light_pass);

	return label;
}

/* Volume Evaluation */

ccl_device void shader_eval_volume(KernelGlobals *kg, ShaderData *sd,
	VolumeStack *stack, int path_flag, ShaderContext ctx)
{
	/* reset closures once at the start, we will be accumulating the closures
	 * for all volumes in the stack into a single array of closures */
	sd->num_closure = 0;
	sd->flag = 0;

	for(int i = 0; stack[i].shader != SHADER_NONE; i++) {
		/* setup shaderdata from stack. it's mostly setup already in
		 * shader_setup_from_volume, this switching should be quick */
		sd->object = stack[i].object;
		sd->shader = stack[i].shader;

		sd->flag &= ~(SD_SHADER_FLAGS|SD_OBJECT_FLAGS);
		sd->flag |= kernel_tex_fetch(__shader_flag, (sd->shader & SHADER_MASK)*2);

		if(sd->object != OBJECT_NONE) {
			sd->flag |= kernel_tex_fetch(__object_flag, sd->object);

#ifdef __OBJECT_MOTION__
			/* todo: this is inefficient for motion blur, we should be
			 * caching matrices instead of recomputing them each step */
			shader_setup_object_transforms(kg, sd, sd->time);
#endif
		}

		/* evaluate shader */
#ifdef __SVM__
#ifdef __OSL__
		if(kg->osl) {
			OSLShader::eval_volume(kg, sd, path_flag, ctx);
		}
		else
#endif
		{
			svm_eval_nodes(kg, sd, SHADER_TYPE_VOLUME, path_flag);
		}
#endif

		/* merge closures to avoid exceeding number of closures limit */
		if(i > 0)
			shader_merge_closures(sd);
	}
}

#endif

/* Displacement Evaluation */

ccl_device void shader_eval_displacement(KernelGlobals *kg, ShaderData *sd, ShaderContext ctx)
{
	sd->num_closure = 0;
	sd->randb_closure = 0.0f;

	/* this will modify sd->P */
#ifdef __SVM__
#ifdef __OSL__
	if(kg->osl)
		OSLShader::eval_displacement(kg, sd, ctx);
	else
#endif
	{
		svm_eval_nodes(kg, sd, SHADER_TYPE_DISPLACEMENT, 0);
	}
#endif
}

/* Transparent Shadows */

#ifdef __TRANSPARENT_SHADOWS__
ccl_device bool shader_transparent_shadow(KernelGlobals *kg, Intersection *isect)
{
	int prim = kernel_tex_fetch(__prim_index, isect->prim);
	int shader = 0;

#ifdef __HAIR__
	if(kernel_tex_fetch(__prim_type, isect->prim) & PRIMITIVE_ALL_TRIANGLE) {
#endif
		float4 Ns = kernel_tex_fetch(__tri_normal, prim);
		shader = __float_as_int(Ns.w);
#ifdef __HAIR__
	}
	else {
		float4 str = kernel_tex_fetch(__curves, prim);
		shader = __float_as_int(str.z);
	}
#endif
	int flag = kernel_tex_fetch(__shader_flag, (shader & SHADER_MASK)*2);

	return (flag & SD_HAS_TRANSPARENT_SHADOW) != 0;
}
#endif

CCL_NAMESPACE_END

