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

#include "kernel_compat_cpu.h"
#include "kernel_types.h"
#include "kernel_globals.h"
#include "kernel_object.h"

#include "osl_services.h"
#include "osl_shader.h"

#include "util_foreach.h"

#include <OSL/oslexec.h>
#include <oslexec_pvt.h>

CCL_NAMESPACE_BEGIN

tls_ptr(OSLGlobals::ThreadData, OSLGlobals::thread_data);

/* Threads */

void OSLShader::thread_init(KernelGlobals *kg)
{
	OSL::pvt::ShadingSystemImpl *ssi = (OSL::pvt::ShadingSystemImpl*)kg->osl.ss;

	OSLGlobals::ThreadData *tdata = new OSLGlobals::ThreadData();

	memset(&tdata->globals, 0, sizeof(OSL::ShaderGlobals));
	tdata->thread_info = ssi->create_thread_info();

	tls_set(kg->osl.thread_data, tdata);

	((OSLRenderServices*)ssi->renderer())->thread_init(kg);
}

void OSLShader::thread_free(KernelGlobals *kg)
{
	OSL::pvt::ShadingSystemImpl *ssi = (OSL::pvt::ShadingSystemImpl*)kg->osl.ss;

	OSLGlobals::ThreadData *tdata = tls_get(OSLGlobals::ThreadData, kg->osl.thread_data);

	ssi->destroy_thread_info(tdata->thread_info);

	delete tdata;
}

/* Globals */

#define TO_VEC3(v) (*(OSL::Vec3*)&(v))
#define TO_COLOR3(v) (*(OSL::Color3*)&(v))
#define TO_FLOAT3(v) make_float3(v[0], v[1], v[2])

static void shaderdata_to_shaderglobals(KernelGlobals *kg, ShaderData *sd,
	int path_flag, OSL::ShaderGlobals *globals)
{
	/* copy from shader data to shader globals */
	globals->P = TO_VEC3(sd->P);
	globals->dPdx = TO_VEC3(sd->dP.dx);
	globals->dPdy = TO_VEC3(sd->dP.dy);
	globals->I = TO_VEC3(sd->I);
	globals->dIdx = TO_VEC3(sd->dI.dx);
	globals->dIdy = TO_VEC3(sd->dI.dy);
	globals->N = TO_VEC3(sd->N);
	globals->Ng = TO_VEC3(sd->Ng);
	globals->u = sd->u;
	globals->dudx = sd->du.dx;
	globals->dudy = sd->du.dy;
	globals->v = sd->v;
	globals->dvdx = sd->dv.dx;
	globals->dvdy = sd->dv.dy;
	globals->dPdu = TO_VEC3(sd->dPdu);
	globals->dPdv = TO_VEC3(sd->dPdv);
	globals->surfacearea = (sd->object == ~0)? 1.0f: object_surface_area(kg, sd->object);

	/* booleans */
	globals->raytype = path_flag;
	globals->backfacing = (sd->flag & SD_BACKFACING);

	/* don't know yet if we need this */
	globals->flipHandedness = false;
	
	/* shader data to be used in services callbacks */
	globals->renderstate = sd; 

	/* hacky, we leave it to services to fetch actual object matrix */
	globals->shader2common = sd;
	globals->object2common = sd;

	/* must be set to NULL before execute */
	globals->Ci = NULL;
}

/* Surface */

static void flatten_surface_closure_tree(ShaderData *sd, bool no_glossy,
	const OSL::ClosureColor *closure, float3 weight = make_float3(1.0f, 1.0f, 1.0f))
{
	/* OSL gives use a closure tree, we flatten it into arrays per
	 * closure type, for evaluation, sampling, etc later on. */

	if(closure->type == OSL::ClosureColor::COMPONENT) {
		OSL::ClosureComponent *comp = (OSL::ClosureComponent*)closure;
		OSL::ClosurePrimitive *prim = (OSL::ClosurePrimitive*)comp->data();

		if(prim) {
			FlatClosure flat;
			flat.prim = prim;
			flat.weight = weight;

			switch(prim->category()) {
				case ClosurePrimitive::BSDF: {
					if(sd->osl_closure.num_bsdf == MAX_OSL_CLOSURE)
						return;

					OSL::BSDFClosure *bsdf = (OSL::BSDFClosure*)prim;
					ustring scattering = bsdf->scattering();

					/* no caustics option */
					if(no_glossy && scattering == OSL::Labels::GLOSSY)
						return;

					/* sample weight */
					float albedo = bsdf->albedo(TO_VEC3(sd->I));
					float sample_weight = fabsf(average(weight))*albedo;
					float sample_sum = sd->osl_closure.bsdf_sample_sum + sample_weight;

					flat.sample_weight = sample_weight;
					sd->osl_closure.bsdf_sample_sum = sample_sum;

					/* scattering flags */
					if(scattering == OSL::Labels::DIFFUSE)
						sd->flag |= SD_BSDF|SD_BSDF_HAS_EVAL;
					else if(scattering == OSL::Labels::GLOSSY)
						sd->flag |= SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
					else
						sd->flag |= SD_BSDF;

					/* add */
					sd->osl_closure.bsdf[sd->osl_closure.num_bsdf++] = flat;
					break;
				}
				case ClosurePrimitive::Emissive: {
					if(sd->osl_closure.num_bsdf == MAX_OSL_CLOSURE)
						return;

					/* sample weight */
					float sample_weight = fabsf(average(weight));
					float sample_sum = sd->osl_closure.emissive_sample_sum + sample_weight;

					flat.sample_weight = sample_weight;
					sd->osl_closure.emissive_sample_sum = sample_sum;

					/* flag */
					sd->flag |= SD_EMISSION;

					sd->osl_closure.emissive[sd->osl_closure.num_emissive++] = flat;
					break;
				}
				case ClosurePrimitive::Holdout:
					sd->osl_closure.holdout_weight += weight;
					sd->flag |= SD_HOLDOUT;
					break;
				case ClosurePrimitive::BSSRDF:
				case ClosurePrimitive::Debug:
					break; /* not implemented */
				case ClosurePrimitive::Background:
				case ClosurePrimitive::Volume:
					break; /* not relevant */
			}
		}
	}
	else if(closure->type == OSL::ClosureColor::MUL) {
		OSL::ClosureMul *mul = (OSL::ClosureMul*)closure;
		flatten_surface_closure_tree(sd, no_glossy, mul->closure, TO_FLOAT3(mul->weight) * weight);
	}
	else if(closure->type == OSL::ClosureColor::ADD) {
		OSL::ClosureAdd *add = (OSL::ClosureAdd*)closure;
		flatten_surface_closure_tree(sd, no_glossy, add->closureA, weight);
		flatten_surface_closure_tree(sd, no_glossy, add->closureB, weight);
	}
}

void OSLShader::eval_surface(KernelGlobals *kg, ShaderData *sd, float randb, int path_flag)
{
	/* gather pointers */
	OSL::pvt::ShadingSystemImpl *ssi = (OSL::pvt::ShadingSystemImpl*)kg->osl.ss;
	OSLGlobals::ThreadData *tdata = tls_get(OSLGlobals::ThreadData, kg->osl.thread_data);
	OSL::ShaderGlobals *globals = &tdata->globals;
	OSL::pvt::ShadingContext *ctx = ssi->get_context(tdata->thread_info);

	/* setup shader globals from shader data */
	sd->osl_ctx = ctx;
	shaderdata_to_shaderglobals(kg, sd, path_flag, globals);

	/* execute shader for this point */
	if(kg->osl.surface_state[sd->shader])
		ctx->execute(OSL::pvt::ShadUseSurface, *(kg->osl.surface_state[sd->shader]), *globals);

	/* flatten closure tree */
	sd->osl_closure.bsdf_sample_sum = 0.0f;
	sd->osl_closure.emissive_sample_sum = 0.0f;
	sd->osl_closure.num_bsdf = 0;
	sd->osl_closure.num_emissive = 0;
	sd->osl_closure.holdout_weight = make_float3(0.0f, 0.0f, 0.0f);
	sd->osl_closure.randb = randb;

	if(globals->Ci) {
		bool no_glossy = (path_flag & PATH_RAY_DIFFUSE) && kernel_data.integrator.no_caustics;
		flatten_surface_closure_tree(sd, no_glossy, globals->Ci);
	}
}

/* Background */

static float3 flatten_background_closure_tree(const OSL::ClosureColor *closure)
{
	/* OSL gives use a closure tree, if we are shading for background there
	 * is only one supported closure type at the moment, which has no evaluation
	 * functions, so we just sum the weights */

	if(closure->type == OSL::ClosureColor::COMPONENT) {
		OSL::ClosureComponent *comp = (OSL::ClosureComponent*)closure;
		OSL::ClosurePrimitive *prim = (OSL::ClosurePrimitive*)comp->data();

		if(prim && prim->category() == OSL::ClosurePrimitive::Background)
			return make_float3(1.0f, 1.0f, 1.0f);
	}
	else if(closure->type == OSL::ClosureColor::MUL) {
		OSL::ClosureMul *mul = (OSL::ClosureMul*)closure;

		return TO_FLOAT3(mul->weight) * flatten_background_closure_tree(mul->closure);
	}
	else if(closure->type == OSL::ClosureColor::ADD) {
		OSL::ClosureAdd *add = (OSL::ClosureAdd*)closure;

		return flatten_background_closure_tree(add->closureA) +
			flatten_background_closure_tree(add->closureB);
	}

	return make_float3(0.0f, 0.0f, 0.0f);
}

float3 OSLShader::eval_background(KernelGlobals *kg, ShaderData *sd, int path_flag)
{
	/* gather pointers */
	OSL::pvt::ShadingSystemImpl *ssi = (OSL::pvt::ShadingSystemImpl*)kg->osl.ss;
	OSLGlobals::ThreadData *tdata = tls_get(OSLGlobals::ThreadData, kg->osl.thread_data);
	OSL::ShaderGlobals *globals = &tdata->globals;
	OSL::pvt::ShadingContext *ctx = ssi->get_context(tdata->thread_info);

	/* setup shader globals from shader data */
	sd->osl_ctx = ctx;
	shaderdata_to_shaderglobals(kg, sd, path_flag, globals);

	/* execute shader for this point */
	if(kg->osl.background_state)
		ctx->execute(OSL::pvt::ShadUseSurface, *kg->osl.background_state, *globals);

	/* return background color immediately */
	if(globals->Ci)
		return flatten_background_closure_tree(globals->Ci);

	return make_float3(0.0f, 0.0f, 0.0f);
}

/* Volume */

static void flatten_volume_closure_tree(ShaderData *sd,
	const OSL::ClosureColor *closure, float3 weight = make_float3(1.0f, 1.0f, 1.0f))
{
	/* OSL gives use a closure tree, we flatten it into arrays per
	 * closure type, for evaluation, sampling, etc later on. */

	if(closure->type == OSL::ClosureColor::COMPONENT) {
		OSL::ClosureComponent *comp = (OSL::ClosureComponent*)closure;
		OSL::ClosurePrimitive *prim = (OSL::ClosurePrimitive*)comp->data();

		if(prim) {
			FlatClosure flat;
			flat.prim = prim;
			flat.weight = weight;

			switch(prim->category()) {
				case ClosurePrimitive::Volume: {
					if(sd->osl_closure.num_bsdf == MAX_OSL_CLOSURE)
						return;

					/* sample weight */
					float sample_weight = fabsf(average(weight));
					float sample_sum = sd->osl_closure.volume_sample_sum + sample_weight;

					flat.sample_weight = sample_weight;
					sd->osl_closure.volume_sample_sum = sample_sum;

					/* add */
					sd->osl_closure.volume[sd->osl_closure.num_volume++] = flat;
					break;
				}
				case ClosurePrimitive::Holdout:
				case ClosurePrimitive::Debug:
					break; /* not implemented */
				case ClosurePrimitive::Background:
				case ClosurePrimitive::BSDF:
				case ClosurePrimitive::Emissive:
				case ClosurePrimitive::BSSRDF:
					break; /* not relevant */
			}
		}
	}
	else if(closure->type == OSL::ClosureColor::MUL) {
		OSL::ClosureMul *mul = (OSL::ClosureMul*)closure;
		flatten_volume_closure_tree(sd, mul->closure, TO_FLOAT3(mul->weight) * weight);
	}
	else if(closure->type == OSL::ClosureColor::ADD) {
		OSL::ClosureAdd *add = (OSL::ClosureAdd*)closure;
		flatten_volume_closure_tree(sd, add->closureA, weight);
		flatten_volume_closure_tree(sd, add->closureB, weight);
	}
}

void OSLShader::eval_volume(KernelGlobals *kg, ShaderData *sd, float randb, int path_flag)
{
	/* gather pointers */
	OSL::pvt::ShadingSystemImpl *ssi = (OSL::pvt::ShadingSystemImpl*)kg->osl.ss;
	OSLGlobals::ThreadData *tdata = tls_get(OSLGlobals::ThreadData, kg->osl.thread_data);
	OSL::ShaderGlobals *globals = &tdata->globals;
	OSL::pvt::ShadingContext *ctx = ssi->get_context(tdata->thread_info);

	/* setup shader globals from shader data */
	sd->osl_ctx = ctx;
	shaderdata_to_shaderglobals(kg, sd, path_flag, globals);

	/* execute shader */
	ctx->execute(OSL::pvt::ShadUseSurface, *(kg->osl.volume_state[sd->shader]), *globals);

	/* retrieve resulting closures */
	sd->osl_closure.volume_sample_sum = 0.0f;
	sd->osl_closure.num_volume = 0;
	sd->osl_closure.randb = randb;

	if(globals->Ci)
		flatten_volume_closure_tree(sd, globals->Ci);
}

/* Displacement */

void OSLShader::eval_displacement(KernelGlobals *kg, ShaderData *sd)
{
	/* gather pointers */
	OSL::pvt::ShadingSystemImpl *ssi = (OSL::pvt::ShadingSystemImpl*)kg->osl.ss;
	OSLGlobals::ThreadData *tdata = tls_get(OSLGlobals::ThreadData, kg->osl.thread_data);
	OSL::ShaderGlobals *globals = &tdata->globals;
	OSL::pvt::ShadingContext *ctx = ssi->get_context(tdata->thread_info);

	/* setup shader globals from shader data */
	sd->osl_ctx = ctx;
	shaderdata_to_shaderglobals(kg, sd, 0, globals);

	/* execute shader */
	ctx->execute(OSL::pvt::ShadUseSurface, *(kg->osl.displacement_state[sd->shader]), *globals);

	/* get back position */
	sd->P = TO_FLOAT3(globals->P);
}

void OSLShader::release(KernelGlobals *kg, const ShaderData *sd)
{
	OSL::pvt::ShadingSystemImpl *ssi = (OSL::pvt::ShadingSystemImpl*)kg->osl.ss;
	OSLGlobals::ThreadData *tdata = tls_get(OSLGlobals::ThreadData, kg->osl.thread_data);

	ssi->release_context((OSL::pvt::ShadingContext*)sd->osl_ctx, tdata->thread_info);
}

/* BSDF Closure */

int OSLShader::bsdf_sample(const ShaderData *sd, float randu, float randv, float3& eval, float3& omega_in, differential3& domega_in, float& pdf)
{
	OSL::BSDFClosure *sample_bsdf = NULL;
	int label = LABEL_NONE;
	float r = sd->osl_closure.randb*sd->osl_closure.bsdf_sample_sum;
	float sample_sum = 0.0f;

	pdf = 0.0f;

	if(sd->osl_closure.bsdf_sample_sum == 0.0f)
		return LABEL_NONE;

	/* find a closure to sample */
	for(int i = 0; i < sd->osl_closure.num_bsdf; i++) {
		const FlatClosure *flat = &sd->osl_closure.bsdf[i];
		sample_sum += flat->sample_weight;

		if(r > sample_sum)
			continue;

		/* sample BSDF closure */
		sample_bsdf = (OSL::BSDFClosure*)flat->prim;
		ustring ulabel;

		ulabel = sample_bsdf->sample(TO_VEC3(sd->Ng),
			TO_VEC3(sd->I), TO_VEC3(sd->dI.dx), TO_VEC3(sd->dI.dy),
			randu, randv,
			TO_VEC3(omega_in), TO_VEC3(domega_in.dx), TO_VEC3(domega_in.dy),
			pdf, TO_COLOR3(eval));

		/* convert OSL label */
		if(ulabel == OSL::Labels::REFLECT)
			label = LABEL_REFLECT;
		else if(ulabel == OSL::Labels::TRANSMIT)
			label = LABEL_TRANSMIT;
		else
			return LABEL_NONE; /* sampling failed */

		/* convert scattering to our bitflag label */
		ustring uscattering = sample_bsdf->scattering();

		if(uscattering == OSL::Labels::DIFFUSE)
			label |= LABEL_DIFFUSE;
		else if(uscattering == OSL::Labels::GLOSSY)
			label |= LABEL_GLOSSY;
		else if(uscattering == OSL::Labels::SINGULAR)
			label |= LABEL_SINGULAR;
		else
			label |= LABEL_STRAIGHT;

		/* eval + pdf */
		eval *= flat->weight;
		pdf *= flat->sample_weight;

		break;
	}

	if(!sample_bsdf || pdf == 0.0f)
		return LABEL_NONE;

	/* add eval/pdf from other BSDF closures */
	for(int i = 0; i < sd->osl_closure.num_bsdf; i++) {
		const FlatClosure *flat = &sd->osl_closure.bsdf[i];
		OSL::BSDFClosure *bsdf = (OSL::BSDFClosure*)flat->prim;

		if(bsdf != sample_bsdf) {
			OSL::Color3 bsdf_eval;
			float bsdf_pdf = 0.0f;

			if(dot(sd->Ng, omega_in) >= 0.0f)
				bsdf_eval = bsdf->eval_reflect(TO_VEC3(sd->I), TO_VEC3(omega_in), bsdf_pdf);
			else
				bsdf_eval = bsdf->eval_transmit(TO_VEC3(sd->I), TO_VEC3(omega_in), bsdf_pdf);

			if(bsdf_pdf != 0.0f) {
				eval += TO_FLOAT3(bsdf_eval)*flat->weight;
				pdf += bsdf_pdf*flat->sample_weight;
			}
		}
	}

	pdf *= 1.0f/(sd->osl_closure.bsdf_sample_sum);

	return label;
}

float3 OSLShader::bsdf_eval(const ShaderData *sd, const float3& omega_in, float& pdf)
{
	float3 eval = make_float3(0.0f, 0.0f, 0.0f);

	pdf = 0.0f;

	for(int i = 0; i < sd->osl_closure.num_bsdf; i++) {
		const FlatClosure *flat = &sd->osl_closure.bsdf[i];
		OSL::BSDFClosure *bsdf = (OSL::BSDFClosure*)flat->prim;
		OSL::Color3 bsdf_eval;
		float bsdf_pdf = 0.0f;

		if(dot(sd->Ng, omega_in) >= 0.0f)
			bsdf_eval = bsdf->eval_reflect(TO_VEC3(sd->I), TO_VEC3(omega_in), bsdf_pdf);
		else
			bsdf_eval = bsdf->eval_transmit(TO_VEC3(sd->I), TO_VEC3(omega_in), bsdf_pdf);

		if(bsdf_pdf != 0.0f) {
			eval += TO_FLOAT3(bsdf_eval)*flat->weight;
			pdf += bsdf_pdf*flat->sample_weight;
		}
	}

	pdf *= 1.0f/sd->osl_closure.bsdf_sample_sum;

	return eval;
}

/* Emissive Closure */

float3 OSLShader::emissive_eval(const ShaderData *sd)
{
	float3 eval = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < sd->osl_closure.num_emissive; i++) {
		const FlatClosure *flat = &sd->osl_closure.emissive[i];
		OSL::EmissiveClosure *emissive = (OSL::EmissiveClosure*)flat->prim;
		OSL::Color3 emissive_eval = emissive->eval(TO_VEC3(sd->Ng), TO_VEC3(sd->I));
		eval += TO_FLOAT3(emissive_eval)*flat->weight;
	}

	return eval;
}

void OSLShader::emissive_sample(const ShaderData *sd, float randu, float randv, float3 *eval, float3 *I, float *pdf)
{
	float r = sd->osl_closure.randb*sd->osl_closure.emissive_sample_sum;
	float sample_sum = 0.0f;

	*pdf = 0.0f;

	if(sd->osl_closure.emissive_sample_sum == 0.0f)
		return;

	/* find a closure to sample */
	for(int i = 0; i < sd->osl_closure.num_emissive; i++) {
		const FlatClosure *flat = &sd->osl_closure.emissive[i];
		sample_sum += flat->sample_weight;

		if(r <= sample_sum) {
			/* sample emissive closure */
			OSL::EmissiveClosure *emissive = (OSL::EmissiveClosure*)flat->prim;
			emissive->sample(TO_VEC3(sd->Ng), randu, randv, TO_VEC3(*I), *pdf);
			*eval = flat->weight;
			*pdf *= flat->sample_weight/sd->osl_closure.emissive_sample_sum;
			return;
		}
	}
}

/* Volume Closure */

float3 OSLShader::volume_eval_phase(const ShaderData *sd, const float3 omega_in, const float3 omega_out)
{
	float3 eval = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < sd->osl_closure.num_volume; i++) {
		const FlatClosure *flat = &sd->osl_closure.volume[i];
		OSL::VolumeClosure *volume = (OSL::VolumeClosure*)flat->prim;
		OSL::Color3 volume_eval = volume->eval_phase(TO_VEC3(omega_in), TO_VEC3(omega_out));
		eval += TO_FLOAT3(volume_eval)*flat->weight;
	}

	return eval;
}

/* Holdout Closure */

float3 OSLShader::holdout_eval(const ShaderData *sd)
{
	return sd->osl_closure.holdout_weight;
}

CCL_NAMESPACE_END

