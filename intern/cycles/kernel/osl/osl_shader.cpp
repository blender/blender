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

#include "osl_closures.h"
#include "osl_globals.h"
#include "osl_services.h"
#include "osl_shader.h"

#include "util_foreach.h"

#include "attribute.h"

#include <OSL/oslexec.h>

CCL_NAMESPACE_BEGIN

/* Threads */

void OSLShader::thread_init(KernelGlobals *kg, KernelGlobals *kernel_globals, OSLGlobals *osl_globals)
{
	/* no osl used? */
	if(!osl_globals->use) {
		kg->osl = NULL;
		return;
	}

	/* per thread kernel data init*/
	kg->osl = osl_globals;
	kg->osl->services->thread_init(kernel_globals);

	OSL::ShadingSystem *ss = kg->osl->ss;
	OSLThreadData *tdata = new OSLThreadData();

	memset(&tdata->globals, 0, sizeof(OSL::ShaderGlobals));
	tdata->globals.tracedata = &tdata->tracedata;
	tdata->globals.flipHandedness = false;
	tdata->thread_info = ss->create_thread_info();

	for(int i = 0; i < SHADER_CONTEXT_NUM; i++)
		tdata->context[i] = ss->get_context(tdata->thread_info);

	kg->osl_ss = (OSLShadingSystem*)ss;
	kg->osl_tdata = tdata;
}

void OSLShader::thread_free(KernelGlobals *kg)
{
	if(!kg->osl)
		return;

	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)kg->osl_ss;
	OSLThreadData *tdata = kg->osl_tdata;

	for(int i = 0; i < SHADER_CONTEXT_NUM; i++)
		ss->release_context(tdata->context[i]);

	ss->destroy_thread_info(tdata->thread_info);

	delete tdata;

	kg->osl = NULL;
	kg->osl_ss = NULL;
	kg->osl_tdata = NULL;
}

/* Globals */

static void shaderdata_to_shaderglobals(KernelGlobals *kg, ShaderData *sd,
                                        int path_flag, OSLThreadData *tdata)
{
	OSL::ShaderGlobals *globals = &tdata->globals;

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
	globals->surfacearea = (sd->object == ~0) ? 1.0f : object_surface_area(kg, sd->object);
	globals->time = sd->time;

	/* booleans */
	globals->raytype = path_flag;
	globals->backfacing = (sd->flag & SD_BACKFACING);

	/* shader data to be used in services callbacks */
	globals->renderstate = sd; 

	/* hacky, we leave it to services to fetch actual object matrix */
	globals->shader2common = sd;
	globals->object2common = sd;

	/* must be set to NULL before execute */
	globals->Ci = NULL;

	/* clear trace data */
	tdata->tracedata.init = false;
}

/* Surface */

static void flatten_surface_closure_tree(ShaderData *sd, bool no_glossy,
                                         const OSL::ClosureColor *closure, float3 weight = make_float3(1.0f, 1.0f, 1.0f))
{
	/* OSL gives us a closure tree, we flatten it into arrays per
	 * closure type, for evaluation, sampling, etc later on. */

	if (closure->type == OSL::ClosureColor::COMPONENT) {
		OSL::ClosureComponent *comp = (OSL::ClosureComponent *)closure;
		OSL::ClosurePrimitive *prim = (OSL::ClosurePrimitive *)comp->data();

		if (prim) {
			ShaderClosure sc;
			sc.weight = weight;

			switch (prim->category()) {
				case OSL::ClosurePrimitive::BSDF: {
					CBSDFClosure *bsdf = (CBSDFClosure *)prim;
					int scattering = bsdf->scattering();

					/* no caustics option */
					if (no_glossy && scattering == LABEL_GLOSSY)
						return;

					/* sample weight */
					float sample_weight = fabsf(average(weight));

					sc.sample_weight = sample_weight;

					sc.type = bsdf->sc.type;
					sc.N = bsdf->sc.N;
					sc.T = bsdf->sc.T;
					sc.data0 = bsdf->sc.data0;
					sc.data1 = bsdf->sc.data1;
					sc.prim = bsdf->sc.prim;

					/* add */
					if(sc.sample_weight > 1e-5f && sd->num_closure < MAX_CLOSURE) {
						sd->closure[sd->num_closure++] = sc;
						sd->flag |= bsdf->shaderdata_flag();
					}
					break;
				}
				case OSL::ClosurePrimitive::Emissive: {
					/* sample weight */
					float sample_weight = fabsf(average(weight));

					sc.sample_weight = sample_weight;
					sc.type = CLOSURE_EMISSION_ID;
					sc.prim = NULL;

					/* flag */
					if(sd->num_closure < MAX_CLOSURE) {
						sd->closure[sd->num_closure++] = sc;
						sd->flag |= SD_EMISSION;
					}
					break;
				}
				case AmbientOcclusion: {
					/* sample weight */
					float sample_weight = fabsf(average(weight));

					sc.sample_weight = sample_weight;
					sc.type = CLOSURE_AMBIENT_OCCLUSION_ID;
					sc.prim = NULL;

					if(sd->num_closure < MAX_CLOSURE) {
						sd->closure[sd->num_closure++] = sc;
						sd->flag |= SD_AO;
					}
					break;
				}
				case OSL::ClosurePrimitive::Holdout:
					sc.sample_weight = 0.0f;
					sc.type = CLOSURE_HOLDOUT_ID;
					sc.prim = NULL;

					if(sd->num_closure < MAX_CLOSURE) {
						sd->closure[sd->num_closure++] = sc;
						sd->flag |= SD_HOLDOUT;
					}
					break;
				case OSL::ClosurePrimitive::BSSRDF:
				case OSL::ClosurePrimitive::Debug:
					break; /* not implemented */
				case OSL::ClosurePrimitive::Background:
				case OSL::ClosurePrimitive::Volume:
					break; /* not relevant */
			}
		}
	}
	else if (closure->type == OSL::ClosureColor::MUL) {
		OSL::ClosureMul *mul = (OSL::ClosureMul *)closure;
		flatten_surface_closure_tree(sd, no_glossy, mul->closure, TO_FLOAT3(mul->weight) * weight);
	}
	else if (closure->type == OSL::ClosureColor::ADD) {
		OSL::ClosureAdd *add = (OSL::ClosureAdd *)closure;
		flatten_surface_closure_tree(sd, no_glossy, add->closureA, weight);
		flatten_surface_closure_tree(sd, no_glossy, add->closureB, weight);
	}
}

void OSLShader::eval_surface(KernelGlobals *kg, ShaderData *sd, float randb, int path_flag, ShaderContext ctx)
{
	/* setup shader globals from shader data */
	OSLThreadData *tdata = kg->osl_tdata;
	shaderdata_to_shaderglobals(kg, sd, path_flag, tdata);

	/* execute shader for this point */
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)kg->osl_ss;
	OSL::ShaderGlobals *globals = &tdata->globals;
	OSL::ShadingContext *octx = tdata->context[(int)ctx];
	int shader = sd->shader & SHADER_MASK;

	if (kg->osl->surface_state[shader])
		ss->execute(*octx, *(kg->osl->surface_state[shader]), *globals);

	/* flatten closure tree */
	sd->num_closure = 0;
	sd->randb_closure = randb;

	if (globals->Ci) {
		bool no_glossy = (path_flag & PATH_RAY_DIFFUSE) && kernel_data.integrator.no_caustics;
		flatten_surface_closure_tree(sd, no_glossy, globals->Ci);
	}
}

/* Background */

static float3 flatten_background_closure_tree(const OSL::ClosureColor *closure)
{
	/* OSL gives us a closure tree, if we are shading for background there
	 * is only one supported closure type at the moment, which has no evaluation
	 * functions, so we just sum the weights */

	if (closure->type == OSL::ClosureColor::COMPONENT) {
		OSL::ClosureComponent *comp = (OSL::ClosureComponent *)closure;
		OSL::ClosurePrimitive *prim = (OSL::ClosurePrimitive *)comp->data();

		if (prim && prim->category() == OSL::ClosurePrimitive::Background)
			return make_float3(1.0f, 1.0f, 1.0f);
	}
	else if (closure->type == OSL::ClosureColor::MUL) {
		OSL::ClosureMul *mul = (OSL::ClosureMul *)closure;

		return TO_FLOAT3(mul->weight) * flatten_background_closure_tree(mul->closure);
	}
	else if (closure->type == OSL::ClosureColor::ADD) {
		OSL::ClosureAdd *add = (OSL::ClosureAdd *)closure;

		return flatten_background_closure_tree(add->closureA) +
		       flatten_background_closure_tree(add->closureB);
	}

	return make_float3(0.0f, 0.0f, 0.0f);
}

float3 OSLShader::eval_background(KernelGlobals *kg, ShaderData *sd, int path_flag, ShaderContext ctx)
{
	/* setup shader globals from shader data */
	OSLThreadData *tdata = kg->osl_tdata;
	shaderdata_to_shaderglobals(kg, sd, path_flag, tdata);

	/* execute shader for this point */
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)kg->osl_ss;
	OSL::ShaderGlobals *globals = &tdata->globals;
	OSL::ShadingContext *octx = tdata->context[(int)ctx];

	if (kg->osl->background_state)
		ss->execute(*octx, *(kg->osl->background_state), *globals);

	/* return background color immediately */
	if (globals->Ci)
		return flatten_background_closure_tree(globals->Ci);

	return make_float3(0.0f, 0.0f, 0.0f);
}

/* Volume */

static void flatten_volume_closure_tree(ShaderData *sd,
                                        const OSL::ClosureColor *closure, float3 weight = make_float3(1.0f, 1.0f, 1.0f))
{
	/* OSL gives us a closure tree, we flatten it into arrays per
	 * closure type, for evaluation, sampling, etc later on. */

	if (closure->type == OSL::ClosureColor::COMPONENT) {
		OSL::ClosureComponent *comp = (OSL::ClosureComponent *)closure;
		OSL::ClosurePrimitive *prim = (OSL::ClosurePrimitive *)comp->data();

		if (prim) {
			ShaderClosure sc;
			sc.weight = weight;

			switch (prim->category()) {
				case OSL::ClosurePrimitive::Volume: {
					/* sample weight */
					float sample_weight = fabsf(average(weight));

					sc.sample_weight = sample_weight;
					sc.type = CLOSURE_VOLUME_ID;
					sc.prim = NULL;

					/* add */
					if(sc.sample_weight > 1e-5f && sd->num_closure < MAX_CLOSURE)
						sd->closure[sd->num_closure++] = sc;
					break;
				}
				case OSL::ClosurePrimitive::Holdout:
				case OSL::ClosurePrimitive::Debug:
					break; /* not implemented */
				case OSL::ClosurePrimitive::Background:
				case OSL::ClosurePrimitive::BSDF:
				case OSL::ClosurePrimitive::Emissive:
				case OSL::ClosurePrimitive::BSSRDF:
					break; /* not relevant */
			}
		}
	}
	else if (closure->type == OSL::ClosureColor::MUL) {
		OSL::ClosureMul *mul = (OSL::ClosureMul *)closure;
		flatten_volume_closure_tree(sd, mul->closure, TO_FLOAT3(mul->weight) * weight);
	}
	else if (closure->type == OSL::ClosureColor::ADD) {
		OSL::ClosureAdd *add = (OSL::ClosureAdd *)closure;
		flatten_volume_closure_tree(sd, add->closureA, weight);
		flatten_volume_closure_tree(sd, add->closureB, weight);
	}
}

void OSLShader::eval_volume(KernelGlobals *kg, ShaderData *sd, float randb, int path_flag, ShaderContext ctx)
{
	/* setup shader globals from shader data */
	OSLThreadData *tdata = kg->osl_tdata;
	shaderdata_to_shaderglobals(kg, sd, path_flag, tdata);

	/* execute shader */
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)kg->osl_ss;
	OSL::ShaderGlobals *globals = &tdata->globals;
	OSL::ShadingContext *octx = tdata->context[(int)ctx];
	int shader = sd->shader & SHADER_MASK;

	if (kg->osl->volume_state[shader])
		ss->execute(*octx, *(kg->osl->volume_state[shader]), *globals);

	if (globals->Ci)
		flatten_volume_closure_tree(sd, globals->Ci);
}

/* Displacement */

void OSLShader::eval_displacement(KernelGlobals *kg, ShaderData *sd, ShaderContext ctx)
{
	/* setup shader globals from shader data */
	OSLThreadData *tdata = kg->osl_tdata;
	shaderdata_to_shaderglobals(kg, sd, 0, tdata);

	/* execute shader */
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)kg->osl_ss;
	OSL::ShaderGlobals *globals = &tdata->globals;
	OSL::ShadingContext *octx = tdata->context[(int)ctx];
	int shader = sd->shader & SHADER_MASK;

	if (kg->osl->displacement_state[shader])
		ss->execute(*octx, *(kg->osl->displacement_state[shader]), *globals);

	/* get back position */
	sd->P = TO_FLOAT3(globals->P);
}

/* BSDF Closure */

int OSLShader::bsdf_sample(const ShaderData *sd, const ShaderClosure *sc, float randu, float randv, float3& eval, float3& omega_in, differential3& domega_in, float& pdf)
{
	CBSDFClosure *sample_bsdf = (CBSDFClosure *)sc->prim;

	pdf = 0.0f;

	return sample_bsdf->sample(sd->Ng,
	                           sd->I, sd->dI.dx, sd->dI.dy,
	                           randu, randv,
	                           omega_in, domega_in.dx, domega_in.dy,
	                           pdf, eval);
}

float3 OSLShader::bsdf_eval(const ShaderData *sd, const ShaderClosure *sc, const float3& omega_in, float& pdf)
{
	CBSDFClosure *bsdf = (CBSDFClosure *)sc->prim;
	float3 bsdf_eval;

	if (dot(sd->Ng, omega_in) >= 0.0f)
		bsdf_eval = bsdf->eval_reflect(sd->I, omega_in, pdf);
	else
		bsdf_eval = bsdf->eval_transmit(sd->I, omega_in, pdf);
	
	return bsdf_eval;
}

void OSLShader::bsdf_blur(ShaderClosure *sc, float roughness)
{
	CBSDFClosure *bsdf = (CBSDFClosure *)sc->prim;
	bsdf->blur(roughness);
}

/* Emissive Closure */

float3 OSLShader::emissive_eval(const ShaderData *sd, const ShaderClosure *sc)
{
	OSL::EmissiveClosure *emissive = (OSL::EmissiveClosure *)sc->prim;
	OSL::Color3 emissive_eval = emissive->eval(TO_VEC3(sd->Ng), TO_VEC3(sd->I));

	return TO_FLOAT3(emissive_eval);
}

/* Volume Closure */

float3 OSLShader::volume_eval_phase(const ShaderClosure *sc, const float3 omega_in, const float3 omega_out)
{
	OSL::VolumeClosure *volume = (OSL::VolumeClosure *)sc->prim;
	OSL::Color3 volume_eval = volume->eval_phase(TO_VEC3(omega_in), TO_VEC3(omega_out));
	return TO_FLOAT3(volume_eval) * sc->weight;
}

/* Attributes */

int OSLShader::find_attribute(KernelGlobals *kg, const ShaderData *sd, uint id, AttributeElement *elem)
{
	/* for OSL, a hash map is used to lookup the attribute by name. */
	int object = sd->object*ATTR_PRIM_TYPES;
#ifdef __HAIR__
	if(sd->segment != ~0) object += ATTR_PRIM_CURVE;
#endif

	OSLGlobals::AttributeMap &attr_map = kg->osl->attribute_map[object];
	ustring stdname(std::string("geom:") + std::string(Attribute::standard_name((AttributeStandard)id)));
	OSLGlobals::AttributeMap::const_iterator it = attr_map.find(stdname);

	if (it != attr_map.end()) {
		const OSLGlobals::Attribute &osl_attr = it->second;
		*elem = osl_attr.elem;
		/* return result */
		return (osl_attr.elem == ATTR_ELEMENT_NONE) ? (int)ATTR_STD_NOT_FOUND : osl_attr.offset;
	}
	else
		return (int)ATTR_STD_NOT_FOUND;
}

CCL_NAMESPACE_END

