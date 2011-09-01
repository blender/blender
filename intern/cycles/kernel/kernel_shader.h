/*
 * Copyright 2011, Blender Foundation.
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

#include "svm/bsdf.h"
#include "svm/emissive.h"
#include "svm/volume.h"
#include "svm/svm_bsdf.h"
#include "svm/svm.h"

#ifdef WITH_OSL
#include "osl_shader.h"
#endif

CCL_NAMESPACE_BEGIN

/* ShaderData setup from incoming ray */

__device_inline void shader_setup_from_ray(KernelGlobals *kg, ShaderData *sd,
	const Intersection *isect, const Ray *ray)
{
	/* fetch triangle data */
	int prim = kernel_tex_fetch(__prim_index, isect->prim);
	float4 Ns = kernel_tex_fetch(__tri_normal, prim);
	float3 Ng = make_float3(Ns.x, Ns.y, Ns.z);
	int shader = __float_as_int(Ns.w);

	/* vectors */
	sd->P = bvh_triangle_refine(kg, isect, ray);
	sd->Ng = Ng;
	sd->N = Ng;
	sd->I = -ray->D;
	sd->shader = shader;
	sd->flag = 0;

	/* triangle */
#ifdef __INSTANCING__
	sd->object = isect->object;
#endif
	sd->prim = prim;
#ifdef __UV__
	sd->u = isect->u;
	sd->v = isect->v;
#endif

	/* smooth normal */
	if(sd->shader < 0) {
		sd->N = triangle_smooth_normal(kg, sd->prim, sd->u, sd->v);
		sd->shader = -sd->shader;
	}

#ifdef __DPDU__
	/* dPdu/dPdv */
	triangle_dPdudv(kg, &sd->dPdu, &sd->dPdv, sd->prim);
#endif

#ifdef __INSTANCING__
	if(sd->object != ~0) {
		/* instance transform */
		object_normal_transform(kg, sd->object, &sd->N);
		object_normal_transform(kg, sd->object, &sd->Ng);
#ifdef __DPDU__
		object_dir_transform(kg, sd->object, &sd->dPdu);
		object_dir_transform(kg, sd->object, &sd->dPdv);
#endif
	}
	else {
		/* non-instanced object index */
		sd->object = kernel_tex_fetch(__prim_object, isect->prim);
	}
#endif

	/* backfacing test */
	bool backfacing = (dot(sd->Ng, sd->I) < 0.0f);

	if(backfacing) {
		sd->flag = SD_BACKFACING;
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

/* ShaderData setup from position sampled on mesh */

__device void shader_setup_from_sample(KernelGlobals *kg, ShaderData *sd,
	const float3 P, const float3 Ng, const float3 I,
	int shader, int object, int prim,  float u, float v)
{
	/* vectors */
	sd->P = P;
	sd->N = Ng;
	sd->Ng = Ng;
	sd->I = I;
	sd->shader = shader;
	sd->flag = 0;

	/* primitive */
#ifdef __INSTANCING__
	sd->object = object;
#endif
	sd->prim = prim;
#ifdef __UV__
	sd->u = u;
	sd->v = v;
#endif

	/* detect instancing, for non-instanced the object index is -object-1 */
#ifdef __INSTANCING__
	bool instanced = false;

	if(sd->prim != ~0) {
		if(sd->object >= 0)
			instanced = true;
		else
#endif
			sd->object = -sd->object-1;
#ifdef __INSTANCING__
	}
#endif

	/* smooth normal */
	if(sd->shader < 0) {
		sd->N = triangle_smooth_normal(kg, sd->prim, sd->u, sd->v);
		sd->shader = -sd->shader;

#ifdef __INSTANCING__
		if(instanced)
			object_normal_transform(kg, sd->object, &sd->N);
#endif
	}

#ifdef __DPDU__
	/* dPdu/dPdv */
	if(sd->prim == ~0) {
		sd->dPdu = make_float3(0.0f, 0.0f, 0.0f);
		sd->dPdv = make_float3(0.0f, 0.0f, 0.0f);
	}
	else {
		triangle_dPdudv(kg, &sd->dPdu, &sd->dPdv, sd->prim);

#ifdef __INSTANCING__
		if(instanced) {
			object_dir_transform(kg, sd->object, &sd->dPdu);
			object_dir_transform(kg, sd->object, &sd->dPdv);
		}
#endif
	}
#endif

	/* backfacing test */
	if(sd->prim != ~0) {
		bool backfacing = (dot(sd->Ng, sd->I) < 0.0f);

		if(backfacing) {
			sd->flag = SD_BACKFACING;
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
	sd->dP.dx = make_float3(0.0f, 0.0f, 0.0f);
	sd->dP.dy = make_float3(0.0f, 0.0f, 0.0f);
	sd->dI.dx = make_float3(0.0f, 0.0f, 0.0f);
	sd->dI.dy = make_float3(0.0f, 0.0f, 0.0f);
	sd->du.dx = 0.0f;
	sd->du.dy = 0.0f;
	sd->dv.dx = 0.0f;
	sd->dv.dy = 0.0f;
#endif
}

/* ShaderData setup for displacement */

__device void shader_setup_from_displace(KernelGlobals *kg, ShaderData *sd,
	int object, int prim, float u, float v)
{
	float3 P, Ng, I = make_float3(0.0f, 0.0f, 0.0f);
	int shader;

	P = triangle_point_MT(kg, prim, u, v);
	Ng = triangle_normal_MT(kg, prim, &shader);

	/* force smooth shading for displacement */
	if(shader >= 0)
		shader = -shader;

	/* watch out: no instance transform currently */

	shader_setup_from_sample(kg, sd, P, Ng, I, shader, object, prim, u, v);
}

/* ShaderData setup from ray into background */

__device_inline void shader_setup_from_background(KernelGlobals *kg, ShaderData *sd, const Ray *ray)
{
	/* vectors */
	sd->P = ray->D;
	sd->N = -sd->P;
	sd->Ng = -sd->P;
	sd->I = -sd->P;
	sd->shader = kernel_data.background.shader;
	sd->flag = 0;

#ifdef __INSTANCING__
	sd->object = ~0;
#endif
	sd->prim = ~0;
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

/* BSDF */

__device int shader_bsdf_sample(KernelGlobals *kg, const ShaderData *sd,
	float randu, float randv, float3 *eval,
	float3 *omega_in, differential3 *domega_in, float *pdf)
{
	int label;

	*pdf = 0.0f;

#ifdef WITH_OSL
	if(kg->osl.use)
		label = OSLShader::bsdf_sample(sd, randu, randv, *eval, *omega_in, *domega_in, *pdf);
	else
#endif
		label = svm_bsdf_sample(sd, randu, randv, eval, omega_in, domega_in, pdf);

	return label;
}

__device float3 shader_bsdf_eval(KernelGlobals *kg, const ShaderData *sd,
	const float3 omega_in, float *pdf)
{
	float3 eval;

	*pdf = 0.0f;

#ifdef WITH_OSL
	if(kg->osl.use)
		eval = OSLShader::bsdf_eval(sd, omega_in, *pdf);
	else
#endif
		eval = svm_bsdf_eval(sd, omega_in, pdf);

	return eval;
}

__device void shader_bsdf_blur(KernelGlobals *kg, ShaderData *sd, float roughness)
{
#ifdef WITH_OSL
	if(!kg->osl.use)
#endif
		svm_bsdf_blur(sd, roughness);
}

/* Emission */

__device float3 shader_emissive_eval(KernelGlobals *kg, ShaderData *sd)
{
#ifdef WITH_OSL
	if(kg->osl.use) {
		return OSLShader::emissive_eval(sd);
	}
	else
#endif
	{
		return svm_emissive_eval(sd);
	}
}

__device void shader_emissive_sample(KernelGlobals *kg, ShaderData *sd,
	float randu, float randv, float3 *eval, float3 *I, float *pdf)
{
#ifdef WITH_OSL
	if(kg->osl.use) {
		OSLShader::emissive_sample(sd, randu, randv, eval, I, pdf);
	}
	else
#endif
	{
		svm_emissive_sample(sd, randu, randv, eval, I, pdf);
	}
}

/* Holdout */

__device float3 shader_holdout_eval(KernelGlobals *kg, ShaderData *sd)
{
#ifdef WITH_OSL
	if(kg->osl.use) {
		return OSLShader::holdout_eval(sd);
	}
	else
#endif
	{
#ifdef __SVM__
		if(sd->svm_closure == CLOSURE_HOLDOUT_ID)
			return make_float3(1.0f, 1.0f, 1.0f);
		else
#endif
			return make_float3(0.0f, 0.0f, 0.0f);
	}
}

/* Surface Evaluation */

__device void shader_eval_surface(KernelGlobals *kg, ShaderData *sd,
	float randb, int path_flag)
{
#ifdef WITH_OSL
	if(kg->osl.use) {
		OSLShader::eval_surface(kg, sd, randb, path_flag);
	}
	else
#endif
	{
#ifdef __SVM__
		svm_eval_nodes(kg, sd, SHADER_TYPE_SURFACE, randb, path_flag);
#else
		bsdf_diffuse_setup(sd, sd->N);
		sd->svm_closure_weight = make_float3(0.8f, 0.8f, 0.8f);
#endif

#ifdef __CAUSTICS_TRICKS__
		/* caustic tricks */
		if((path_flag & PATH_RAY_DIFFUSE) && (sd->flag & SD_BSDF_GLOSSY)) {
			if(kernel_data.integrator.no_caustics) {
				sd->flag &= ~(SD_BSDF_GLOSSY|SD_BSDF_HAS_EVAL|SD_EMISSION);
				sd->svm_closure = NBUILTIN_CLOSURES;
				sd->svm_closure_weight = make_float3(0.0f, 0.0f, 0.0f);
			}
			else if(kernel_data.integrator.blur_caustics > 0.0f)
				shader_bsdf_blur(kg, sd, kernel_data.integrator.blur_caustics);
		}
#endif
	}
}

/* Background Evaluation */

__device float3 shader_eval_background(KernelGlobals *kg, ShaderData *sd, int path_flag)
{
#ifdef WITH_OSL
	if(kg->osl.use) {
		return OSLShader::eval_background(kg, sd, path_flag);
	}
	else
#endif
	{
#ifdef __SVM__
		svm_eval_nodes(kg, sd, SHADER_TYPE_SURFACE, 0.0f, path_flag);
#else
		sd->svm_closure_weight = make_float3(0.8f, 0.8f, 0.8f);
#endif

		return sd->svm_closure_weight;
	}
}

/* Volume */

__device float3 shader_volume_eval_phase(KernelGlobals *kg, ShaderData *sd,
	float3 omega_in, float3 omega_out)
{
#ifdef WITH_OSL
	if(kg->osl.use) {
		OSLShader::volume_eval_phase(sd, omega_in, omega_out);
	}
	else
#endif
	{
		return volume_eval_phase(sd, omega_in, omega_out);
	}
}

/* Volume Evaluation */

__device void shader_eval_volume(KernelGlobals *kg, ShaderData *sd,
	float randb, int path_flag)
{
#ifdef WITH_OSL
	if(kg->osl.use) {
		OSLShader::eval_volume(kg, sd, randb, path_flag);
	}
	else
#endif
	{
#ifdef __SVM__
		svm_eval_nodes(kg, sd, SHADER_TYPE_VOLUME, randb, path_flag);
#endif
	}
}

/* Displacement Evaluation */

__device void shader_eval_displacement(KernelGlobals *kg, ShaderData *sd)
{
	/* this will modify sd->P */

#ifdef WITH_OSL
	if(kg->osl.use) {
		OSLShader::eval_displacement(kg, sd);
	}
	else
#endif
	{
#ifdef __SVM__
		svm_eval_nodes(kg, sd, SHADER_TYPE_DISPLACEMENT, 0.0f, 0);
#endif
	}
}

/* Free ShaderData */

__device void shader_release(KernelGlobals *kg, ShaderData *sd)
{
#ifdef WITH_OSL
	if(kg->osl.use)
		OSLShader::release(kg, sd);
#endif
}

CCL_NAMESPACE_END

