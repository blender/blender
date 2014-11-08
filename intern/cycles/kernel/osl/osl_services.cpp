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

#include <string.h>

#include "mesh.h"
#include "object.h"
#include "scene.h"

#include "osl_closures.h"
#include "osl_globals.h"
#include "osl_services.h"
#include "osl_shader.h"

#include "util_foreach.h"
#include "util_string.h"

#include "kernel_compat_cpu.h"
#include "kernel_globals.h"
#include "kernel_random.h"
#include "kernel_projection.h"
#include "kernel_differential.h"
#include "kernel_montecarlo.h"
#include "kernel_camera.h"

#include "geom/geom.h"

#include "kernel_projection.h"
#include "kernel_accumulate.h"
#include "kernel_shader.h"

#ifdef WITH_PTEX
#include <Ptexture.h>
#endif

CCL_NAMESPACE_BEGIN

/* RenderServices implementation */

#define COPY_MATRIX44(m1, m2)  { \
	CHECK_TYPE(m1, OSL::Matrix44*); \
	CHECK_TYPE(m2, Transform*); \
	memcpy(m1, m2, sizeof(*m2)); \
} (void)0

/* static ustrings */
ustring OSLRenderServices::u_distance("distance");
ustring OSLRenderServices::u_index("index");
ustring OSLRenderServices::u_world("world");
ustring OSLRenderServices::u_camera("camera");
ustring OSLRenderServices::u_screen("screen");
ustring OSLRenderServices::u_raster("raster");
ustring OSLRenderServices::u_ndc("NDC");
ustring OSLRenderServices::u_object_location("object:location");
ustring OSLRenderServices::u_object_index("object:index");
ustring OSLRenderServices::u_geom_dupli_generated("geom:dupli_generated");
ustring OSLRenderServices::u_geom_dupli_uv("geom:dupli_uv");
ustring OSLRenderServices::u_material_index("material:index");
ustring OSLRenderServices::u_object_random("object:random");
ustring OSLRenderServices::u_particle_index("particle:index");
ustring OSLRenderServices::u_particle_age("particle:age");
ustring OSLRenderServices::u_particle_lifetime("particle:lifetime");
ustring OSLRenderServices::u_particle_location("particle:location");
ustring OSLRenderServices::u_particle_rotation("particle:rotation");
ustring OSLRenderServices::u_particle_size("particle:size");
ustring OSLRenderServices::u_particle_velocity("particle:velocity");
ustring OSLRenderServices::u_particle_angular_velocity("particle:angular_velocity");
ustring OSLRenderServices::u_geom_numpolyvertices("geom:numpolyvertices");
ustring OSLRenderServices::u_geom_trianglevertices("geom:trianglevertices");
ustring OSLRenderServices::u_geom_polyvertices("geom:polyvertices");
ustring OSLRenderServices::u_geom_name("geom:name");
ustring OSLRenderServices::u_is_smooth("geom:is_smooth");
#ifdef __HAIR__
ustring OSLRenderServices::u_is_curve("geom:is_curve");
ustring OSLRenderServices::u_curve_thickness("geom:curve_thickness");
ustring OSLRenderServices::u_curve_tangent_normal("geom:curve_tangent_normal");
#endif
ustring OSLRenderServices::u_path_ray_length("path:ray_length");
ustring OSLRenderServices::u_path_ray_depth("path:ray_depth");
ustring OSLRenderServices::u_path_transparent_depth("path:transparent_depth");
ustring OSLRenderServices::u_trace("trace");
ustring OSLRenderServices::u_hit("hit");
ustring OSLRenderServices::u_hitdist("hitdist");
ustring OSLRenderServices::u_N("N");
ustring OSLRenderServices::u_Ng("Ng");
ustring OSLRenderServices::u_P("P");
ustring OSLRenderServices::u_I("I");
ustring OSLRenderServices::u_u("u");
ustring OSLRenderServices::u_v("v");
ustring OSLRenderServices::u_empty;

OSLRenderServices::OSLRenderServices()
{
	kernel_globals = NULL;
	osl_ts = NULL;

#ifdef WITH_PTEX
	size_t maxmem = 16384 * 1024;
	ptex_cache = PtexCache::create(0, maxmem);
#endif
}

OSLRenderServices::~OSLRenderServices()
{
#ifdef WITH_PTEX
	ptex_cache->release();
#endif
}

void OSLRenderServices::thread_init(KernelGlobals *kernel_globals_, OSL::TextureSystem *osl_ts_)
{
	kernel_globals = kernel_globals_;
	osl_ts = osl_ts_;
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, OSL::TransformationPtr xform, float time)
{
	/* this is only used for shader and object space, we don't really have
	 * a concept of shader space, so we just use object space for both. */
	if (xform) {
		const ShaderData *sd = (const ShaderData *)xform;
		KernelGlobals *kg = sd->osl_globals;
		int object = sd->object;

		if (object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
			Transform tfm;

			if(time == sd->time)
				tfm = sd->ob_tfm;
			else
				tfm = object_fetch_transform_motion_test(kg, object, time, NULL);
#else
			Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
#endif
			tfm = transform_transpose(tfm);
			COPY_MATRIX44(&result, &tfm);

			return true;
		}
	}

	return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, OSL::TransformationPtr xform, float time)
{
	/* this is only used for shader and object space, we don't really have
	 * a concept of shader space, so we just use object space for both. */
	if (xform) {
		const ShaderData *sd = (const ShaderData *)xform;
		KernelGlobals *kg = sd->osl_globals;
		int object = sd->object;

		if (object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
			Transform itfm;

			if(time == sd->time)
				itfm = sd->ob_itfm;
			else
				object_fetch_transform_motion_test(kg, object, time, &itfm);
#else
			Transform itfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);
#endif
			itfm = transform_transpose(itfm);
			COPY_MATRIX44(&result, &itfm);

			return true;
		}
	}

	return false;
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, ustring from, float time)
{
	KernelGlobals *kg = kernel_globals;

	if (from == u_ndc) {
		Transform tfm = transform_transpose(transform_quick_inverse(kernel_data.cam.worldtondc));
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (from == u_raster) {
		Transform tfm = transform_transpose(kernel_data.cam.rastertoworld);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (from == u_screen) {
		Transform tfm = transform_transpose(kernel_data.cam.screentoworld);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (from == u_camera) {
		Transform tfm = transform_transpose(kernel_data.cam.cameratoworld);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (from == u_world) {
		result.makeIdentity();
		return true;
	}

	return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, ustring to, float time)
{
	KernelGlobals *kg = kernel_globals;

	if (to == u_ndc) {
		Transform tfm = transform_transpose(kernel_data.cam.worldtondc);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (to == u_raster) {
		Transform tfm = transform_transpose(kernel_data.cam.worldtoraster);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (to == u_screen) {
		Transform tfm = transform_transpose(kernel_data.cam.worldtoscreen);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (to == u_camera) {
		Transform tfm = transform_transpose(kernel_data.cam.worldtocamera);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (to == u_world) {
		result.makeIdentity();
		return true;
	}

	return false;
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, OSL::TransformationPtr xform)
{
	/* this is only used for shader and object space, we don't really have
	 * a concept of shader space, so we just use object space for both. */
	if (xform) {
		const ShaderData *sd = (const ShaderData *)xform;
		int object = sd->object;

		if (object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
			Transform tfm = sd->ob_tfm;
#else
			KernelGlobals *kg = sd->osl_globals;
			Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
#endif
			tfm = transform_transpose(tfm);
			COPY_MATRIX44(&result, &tfm);

			return true;
		}
	}

	return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, OSL::TransformationPtr xform)
{
	/* this is only used for shader and object space, we don't really have
	 * a concept of shader space, so we just use object space for both. */
	if (xform) {
		const ShaderData *sd = (const ShaderData *)xform;
		int object = sd->object;

		if (object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
			Transform tfm = sd->ob_itfm;
#else
			KernelGlobals *kg = sd->osl_globals;
			Transform tfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);
#endif
			tfm = transform_transpose(tfm);
			COPY_MATRIX44(&result, &tfm);

			return true;
		}
	}

	return false;
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, ustring from)
{
	KernelGlobals *kg = kernel_globals;

	if (from == u_ndc) {
		Transform tfm = transform_transpose(transform_quick_inverse(kernel_data.cam.worldtondc));
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (from == u_raster) {
		Transform tfm = transform_transpose(kernel_data.cam.rastertoworld);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (from == u_screen) {
		Transform tfm = transform_transpose(kernel_data.cam.screentoworld);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (from == u_camera) {
		Transform tfm = transform_transpose(kernel_data.cam.cameratoworld);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}

	return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, ustring to)
{
	KernelGlobals *kg = kernel_globals;
	
	if (to == u_ndc) {
		Transform tfm = transform_transpose(kernel_data.cam.worldtondc);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (to == u_raster) {
		Transform tfm = transform_transpose(kernel_data.cam.worldtoraster);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (to == u_screen) {
		Transform tfm = transform_transpose(kernel_data.cam.worldtoscreen);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	else if (to == u_camera) {
		Transform tfm = transform_transpose(kernel_data.cam.worldtocamera);
		COPY_MATRIX44(&result, &tfm);
		return true;
	}
	
	return false;
}

bool OSLRenderServices::get_array_attribute(OSL::ShaderGlobals *sg, bool derivatives, 
                                            ustring object, TypeDesc type, ustring name,
                                            int index, void *val)
{
	return false;
}

static bool set_attribute_float3(float3 f[3], TypeDesc type, bool derivatives, void *val)
{
	if (type == TypeDesc::TypePoint || type == TypeDesc::TypeVector ||
	    type == TypeDesc::TypeNormal || type == TypeDesc::TypeColor)
	{
		float *fval = (float *)val;

		fval[0] = f[0].x;
		fval[1] = f[0].y;
		fval[2] = f[0].z;

		if (derivatives) {
			fval[3] = f[1].x;
			fval[4] = f[1].y;
			fval[5] = f[1].z;

			fval[6] = f[2].x;
			fval[7] = f[2].y;
			fval[8] = f[2].z;
		}

		return true;
	}
	else if(type == TypeDesc::TypeFloat) {
		float *fval = (float *)val;
		fval[0] = average(f[0]);

		if (derivatives) {
			fval[1] = average(f[1]);
			fval[2] = average(f[2]);
		}

		return true;
	}

	return false;
}

static bool set_attribute_float3(float3 f, TypeDesc type, bool derivatives, void *val)
{
	float3 fv[3];

	fv[0] = f;
	fv[1] = make_float3(0.0f, 0.0f, 0.0f);
	fv[2] = make_float3(0.0f, 0.0f, 0.0f);

	return set_attribute_float3(fv, type, derivatives, val);
}

static bool set_attribute_float(float f[3], TypeDesc type, bool derivatives, void *val)
{
	if (type == TypeDesc::TypePoint || type == TypeDesc::TypeVector ||
	    type == TypeDesc::TypeNormal || type == TypeDesc::TypeColor)
	{
		float *fval = (float *)val;
		fval[0] = f[0];
		fval[1] = f[1];
		fval[2] = f[2];

		if (derivatives) {
			fval[3] = f[1];
			fval[4] = f[1];
			fval[5] = f[1];

			fval[6] = f[2];
			fval[7] = f[2];
			fval[8] = f[2];
		}

		return true;
	}
	else if(type == TypeDesc::TypeFloat) {
		float *fval = (float *)val;
		fval[0] = f[0];

		if (derivatives) {
			fval[1] = f[1];
			fval[2] = f[2];
		}

		return true;
	}

	return false;
}

static bool set_attribute_float(float f, TypeDesc type, bool derivatives, void *val)
{
	float fv[3];

	fv[0] = f;
	fv[1] = 0.0f;
	fv[2] = 0.0f;

	return set_attribute_float(fv, type, derivatives, val);
}

static bool set_attribute_int(int i, TypeDesc type, bool derivatives, void *val)
{
	if(type.basetype == TypeDesc::INT && type.aggregate == TypeDesc::SCALAR && type.arraylen == 0) {
		int *ival = (int *)val;
		ival[0] = i;

		if (derivatives) {
			ival[1] = 0;
			ival[2] = 0;
		}

		return true;
	}

	return false;
}

static bool set_attribute_string(ustring str, TypeDesc type, bool derivatives, void *val)
{
	if(type.basetype == TypeDesc::STRING && type.aggregate == TypeDesc::SCALAR && type.arraylen == 0) {
		ustring *sval = (ustring *)val;
		sval[0] = str;

		if (derivatives) {
			sval[1] = OSLRenderServices::u_empty;
			sval[2] = OSLRenderServices::u_empty;
		}

		return true;
	}

	return false;
}

static bool set_attribute_float3_3(float3 P[3], TypeDesc type, bool derivatives, void *val)
{
	if(type.vecsemantics == TypeDesc::POINT && type.arraylen >= 3) {
		float *fval = (float *)val;

		fval[0] = P[0].x;
		fval[1] = P[0].y;
		fval[2] = P[0].z;

		fval[3] = P[1].x;
		fval[4] = P[1].y;
		fval[5] = P[1].z;

		fval[6] = P[2].x;
		fval[7] = P[2].y;
		fval[8] = P[2].z;

		if(type.arraylen > 3)
			memset(fval + 3*3, 0, sizeof(float)*3*(type.arraylen - 3));
		if (derivatives)
			memset(fval + type.arraylen*3, 0, sizeof(float)*2*3*type.arraylen);

		return true;
	}

	return false;
}

static bool set_attribute_matrix(const Transform& tfm, TypeDesc type, void *val)
{
	if(type == TypeDesc::TypeMatrix) {
		Transform transpose = transform_transpose(tfm);
		memcpy(val, &transpose, sizeof(Transform));
		return true;
	}

	return false;
}

static bool get_mesh_element_attribute(KernelGlobals *kg, const ShaderData *sd, const OSLGlobals::Attribute& attr,
                               const TypeDesc& type, bool derivatives, void *val)
{
	if (attr.type == TypeDesc::TypePoint || attr.type == TypeDesc::TypeVector ||
	    attr.type == TypeDesc::TypeNormal || attr.type == TypeDesc::TypeColor)
	{
		float3 fval[3];
		fval[0] = primitive_attribute_float3(kg, sd, attr.elem, attr.offset,
		                                     (derivatives) ? &fval[1] : NULL, (derivatives) ? &fval[2] : NULL);
		return set_attribute_float3(fval, type, derivatives, val);
	}
	else if (attr.type == TypeDesc::TypeFloat) {
		float fval[3];
		fval[0] = primitive_attribute_float(kg, sd, attr.elem, attr.offset,
		                                    (derivatives) ? &fval[1] : NULL, (derivatives) ? &fval[2] : NULL);
		return set_attribute_float(fval, type, derivatives, val);
	}
	else {
		return false;
	}
}

static bool get_mesh_attribute(KernelGlobals *kg, const ShaderData *sd, const OSLGlobals::Attribute& attr,
                               const TypeDesc& type, bool derivatives, void *val)
{
	if (attr.type == TypeDesc::TypeMatrix) {
		Transform tfm = primitive_attribute_matrix(kg, sd, attr.offset);
		return set_attribute_matrix(tfm, type, val);
	}
	else {
		return false;
	}
}

static void get_object_attribute(const OSLGlobals::Attribute& attr, bool derivatives, void *val)
{
	size_t datasize = attr.value.datasize();

	memcpy(val, attr.value.data(), datasize);
	if (derivatives)
		memset((char *)val + datasize, 0, datasize * 2);
}

bool OSLRenderServices::get_object_standard_attribute(KernelGlobals *kg, ShaderData *sd, ustring name,
                                                      TypeDesc type, bool derivatives, void *val)
{
	/* todo: turn this into hash table? */

	/* Object Attributes */
	if (name == u_object_location) {
		float3 f = object_location(kg, sd);
		return set_attribute_float3(f, type, derivatives, val);
	}
	else if (name == u_object_index) {
		float f = object_pass_id(kg, sd->object);
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_geom_dupli_generated) {
		float3 f = object_dupli_generated(kg, sd->object);
		return set_attribute_float3(f, type, derivatives, val);
	}
	else if (name == u_geom_dupli_uv) {
		float3 f = object_dupli_uv(kg, sd->object);
		return set_attribute_float3(f, type, derivatives, val);
	}
	else if (name == u_material_index) {
		float f = shader_pass_id(kg, sd);
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_object_random) {
		float f = object_random_number(kg, sd->object);
		return set_attribute_float(f, type, derivatives, val);
	}

	/* Particle Attributes */
	else if (name == u_particle_index) {
		int particle_id = object_particle_id(kg, sd->object);
		float f = particle_index(kg, particle_id);
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_particle_age) {
		int particle_id = object_particle_id(kg, sd->object);
		float f = particle_age(kg, particle_id);
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_particle_lifetime) {
		int particle_id = object_particle_id(kg, sd->object);
		float f = particle_lifetime(kg, particle_id);
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_particle_location) {
		int particle_id = object_particle_id(kg, sd->object);
		float3 f = particle_location(kg, particle_id);
		return set_attribute_float3(f, type, derivatives, val);
	}
#if 0	/* unsupported */
	else if (name == u_particle_rotation) {
		int particle_id = object_particle_id(kg, sd->object);
		float4 f = particle_rotation(kg, particle_id);
		return set_attribute_float4(f, type, derivatives, val);
	}
#endif
	else if (name == u_particle_size) {
		int particle_id = object_particle_id(kg, sd->object);
		float f = particle_size(kg, particle_id);
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_particle_velocity) {
		int particle_id = object_particle_id(kg, sd->object);
		float3 f = particle_velocity(kg, particle_id);
		return set_attribute_float3(f, type, derivatives, val);
	}
	else if (name == u_particle_angular_velocity) {
		int particle_id = object_particle_id(kg, sd->object);
		float3 f = particle_angular_velocity(kg, particle_id);
		return set_attribute_float3(f, type, derivatives, val);
	}
	
	/* Geometry Attributes */
	else if (name == u_geom_numpolyvertices) {
		return set_attribute_int(3, type, derivatives, val);
	}
	else if ((name == u_geom_trianglevertices || name == u_geom_polyvertices)
#ifdef __HAIR__
		     && sd->type & PRIMITIVE_ALL_TRIANGLE)
#else
		)
#endif
	{
		float3 P[3];

		if(sd->type & PRIMITIVE_TRIANGLE)
			triangle_vertices(kg, sd->prim, P);
		else
			motion_triangle_vertices(kg, sd->object, sd->prim, sd->time, P);

		if(!(sd->flag & SD_TRANSFORM_APPLIED)) {
			object_position_transform(kg, sd, &P[0]);
			object_position_transform(kg, sd, &P[1]);
			object_position_transform(kg, sd, &P[2]);
		}

		return set_attribute_float3_3(P, type, derivatives, val);
	}
	else if(name == u_geom_name) {
		ustring object_name = kg->osl->object_names[sd->object];
		return set_attribute_string(object_name, type, derivatives, val);
	}
	else if (name == u_is_smooth) {
		float f = ((sd->shader & SHADER_SMOOTH_NORMAL) != 0);
		return set_attribute_float(f, type, derivatives, val);
	}
#ifdef __HAIR__
	/* Hair Attributes */
	else if (name == u_is_curve) {
		float f = (sd->type & PRIMITIVE_ALL_CURVE) != 0;
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_curve_thickness) {
		float f = curve_thickness(kg, sd);
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_curve_tangent_normal) {
		float3 f = curve_tangent_normal(kg, sd);
		return set_attribute_float3(f, type, derivatives, val);
	}
#endif
	else
		return false;
}

bool OSLRenderServices::get_background_attribute(KernelGlobals *kg, ShaderData *sd, ustring name,
                                                 TypeDesc type, bool derivatives, void *val)
{
	if (name == u_path_ray_length) {
		/* Ray Length */
		float f = sd->ray_length;
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_path_ray_depth) {
		/* Ray Depth */
		int f = sd->ray_depth;
		return set_attribute_int(f, type, derivatives, val);
	}
	else if (name == u_path_transparent_depth) {
		/* Transparent Ray Depth */
		int f = sd->transparent_depth;
		return set_attribute_int(f, type, derivatives, val);
	}
	else if (name == u_ndc) {
		/* NDC coordinates with special exception for otho */
		OSLThreadData *tdata = kg->osl_tdata;
		OSL::ShaderGlobals *globals = &tdata->globals;
		float3 ndc[3];

		if((globals->raytype & PATH_RAY_CAMERA) && sd->object == OBJECT_NONE && kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
			ndc[0] = camera_world_to_ndc(kg, sd, sd->ray_P);

			if(derivatives) {
				ndc[1] = camera_world_to_ndc(kg, sd, sd->ray_P + sd->ray_dP.dx) - ndc[0];
				ndc[2] = camera_world_to_ndc(kg, sd, sd->ray_P + sd->ray_dP.dy) - ndc[0];
			}
		}
		else {
			ndc[0] = camera_world_to_ndc(kg, sd, sd->P);

			if(derivatives) {
				ndc[1] = camera_world_to_ndc(kg, sd, sd->P + sd->dP.dx) - ndc[0];
				ndc[2] = camera_world_to_ndc(kg, sd, sd->P + sd->dP.dy) - ndc[0];
			}
		}

		return set_attribute_float3(ndc, type, derivatives, val);
	}
	else
		return false;
}

bool OSLRenderServices::get_attribute(OSL::ShaderGlobals *sg, bool derivatives, ustring object_name,
                                      TypeDesc type, ustring name, void *val)
{
	if (sg->renderstate == NULL)
		return false;

	ShaderData *sd = (ShaderData *)(sg->renderstate);
	return get_attribute(sd, derivatives, object_name, type, name, val);
}

bool OSLRenderServices::get_attribute(ShaderData *sd, bool derivatives, ustring object_name,
                                      TypeDesc type, ustring name, void *val)
{
	KernelGlobals *kg = sd->osl_globals;
	bool is_curve;
	int object;

	/* lookup of attribute on another object */
	if (object_name != u_empty) {
		OSLGlobals::ObjectNameMap::iterator it = kg->osl->object_name_map.find(object_name);

		if (it == kg->osl->object_name_map.end())
			return false;

		object = it->second;
		is_curve = false;
	}
	else {
		object = sd->object;
		is_curve = (sd->type & PRIMITIVE_ALL_CURVE) != 0;

		if (object == OBJECT_NONE)
			return get_background_attribute(kg, sd, name, type, derivatives, val);
	}

	/* find attribute on object */
	object = object*ATTR_PRIM_TYPES + (is_curve == true);
	OSLGlobals::AttributeMap& attribute_map = kg->osl->attribute_map[object];
	OSLGlobals::AttributeMap::iterator it = attribute_map.find(name);

	if (it != attribute_map.end()) {
		const OSLGlobals::Attribute& attr = it->second;

		if (attr.elem != ATTR_ELEMENT_OBJECT) {
			/* triangle and vertex attributes */
			if(get_mesh_element_attribute(kg, sd, attr, type, derivatives, val))
				return true;
			else
				return get_mesh_attribute(kg, sd, attr, type, derivatives, val);
		}
		else {
			/* object attribute */
			get_object_attribute(attr, derivatives, val);
			return true;
		}
	}
	else {
		/* not found in attribute, check standard object info */
		bool is_std_object_attribute = get_object_standard_attribute(kg, sd, name, type, derivatives, val);

		if (is_std_object_attribute)
			return true;

		return get_background_attribute(kg, sd, name, type, derivatives, val);
	}

	return false;
}

bool OSLRenderServices::get_userdata(bool derivatives, ustring name, TypeDesc type, 
                                     OSL::ShaderGlobals *sg, void *val)
{
	return false; /* disabled by lockgeom */
}

bool OSLRenderServices::has_userdata(ustring name, TypeDesc type, OSL::ShaderGlobals *sg)
{
	return false; /* never called by OSL */
}

bool OSLRenderServices::texture(ustring filename, TextureOpt &options,
                                OSL::ShaderGlobals *sg,
                                float s, float t, float dsdx, float dtdx,
                                float dsdy, float dtdy, int nchannels, float *result)
{
	OSL::TextureSystem *ts = osl_ts;
	ShaderData *sd = (ShaderData *)(sg->renderstate);
	KernelGlobals *kg = sd->osl_globals;

#ifdef WITH_PTEX
	/* todo: this is just a quick hack, only works with particular files and options */
	if(string_endswith(filename.string(), ".ptx")) {
		float2 uv;
		int faceid;

		if(!primitive_ptex(kg, sd, &uv, &faceid))
			return false;

		float u = uv.x;
		float v = uv.y;
		float dudx = 0.0f;
		float dvdx = 0.0f;
		float dudy = 0.0f;
		float dvdy = 0.0f;

		Ptex::String error;
		PtexPtr<PtexTexture> r(ptex_cache->get(filename.c_str(), error));

		if(!r) {
			//std::cerr << error.c_str() << std::endl;
			return false;
		}

		bool mipmaplerp = false;
		float sharpness = 1.0f;
		PtexFilter::Options opts(PtexFilter::f_bicubic, mipmaplerp, sharpness);
		PtexPtr<PtexFilter> f(PtexFilter::getFilter(r, opts));

		f->eval(result, options.firstchannel, nchannels, faceid, u, v, dudx, dvdx, dudy, dvdy);

		for(int c = r->numChannels(); c < nchannels; c++)
			result[c] = result[0];

		return true;
	}
#endif
	bool status;

	if(filename[0] == '@' && filename.find('.') == -1) {
		int slot = atoi(filename.c_str() + 1);
		float4 rgba = kernel_tex_image_interp(slot, s, 1.0f - t);

		result[0] = rgba[0];
		if(nchannels > 1)
			result[1] = rgba[1];
		if(nchannels > 2)
			result[2] = rgba[2];
		if(nchannels > 3)
			result[3] = rgba[3];
		status = true;
	}
	else {
		OSLThreadData *tdata = kg->osl_tdata;
		OIIO::TextureSystem::Perthread *thread_info = tdata->oiio_thread_info;

		OIIO::TextureSystem::TextureHandle *th = ts->get_texture_handle(filename, thread_info);

#if OIIO_VERSION < 10500
		status = ts->texture(th, thread_info,
		                     options, s, t, dsdx, dtdx, dsdy, dtdy,
		                     result);
#else
		status = ts->texture(th, thread_info,
		                     options, s, t, dsdx, dtdx, dsdy, dtdy,
		                     nchannels, result);
#endif
	}

	if(!status) {
		if(nchannels == 3 || nchannels == 4) {
			result[0] = 1.0f;
			result[1] = 0.0f;
			result[2] = 1.0f;

			if(nchannels == 4)
				result[3] = 1.0f;
		}
	}

	return status;
}

bool OSLRenderServices::texture3d(ustring filename, TextureOpt &options,
                                  OSL::ShaderGlobals *sg, const OSL::Vec3 &P,
                                  const OSL::Vec3 &dPdx, const OSL::Vec3 &dPdy,
                                  const OSL::Vec3 &dPdz, int nchannels, float *result)
{
	OSL::TextureSystem *ts = osl_ts;
	ShaderData *sd = (ShaderData *)(sg->renderstate);
	KernelGlobals *kg = sd->osl_globals;
	OSLThreadData *tdata = kg->osl_tdata;
	OIIO::TextureSystem::Perthread *thread_info = tdata->oiio_thread_info;

	OIIO::TextureSystem::TextureHandle *th =  ts->get_texture_handle(filename, thread_info);

#if OIIO_VERSION < 10500
	bool status = ts->texture3d(th, thread_info,
	                            options, P, dPdx, dPdy, dPdz, result);
#else
	bool status = ts->texture3d(th, thread_info,
	                            options, P, dPdx, dPdy, dPdz,
	                            nchannels, result);
#endif

	if(!status) {
		if(nchannels == 3 || nchannels == 4) {
			result[0] = 1.0f;
			result[1] = 0.0f;
			result[2] = 1.0f;

			if(nchannels == 4)
				result[3] = 1.0f;
		}

	}

	return status;
}

bool OSLRenderServices::environment(ustring filename, TextureOpt &options,
                                    OSL::ShaderGlobals *sg, const OSL::Vec3 &R,
                                    const OSL::Vec3 &dRdx, const OSL::Vec3 &dRdy,
                                    int nchannels, float *result)
{
	OSL::TextureSystem *ts = osl_ts;
	ShaderData *sd = (ShaderData *)(sg->renderstate);
	KernelGlobals *kg = sd->osl_globals;
	OSLThreadData *tdata = kg->osl_tdata;
	OIIO::TextureSystem::Perthread *thread_info = tdata->oiio_thread_info;

	OIIO::TextureSystem::TextureHandle *th =  ts->get_texture_handle(filename, thread_info);

#if OIIO_VERSION < 10500
	bool status = ts->environment(th, thread_info,
	                              options, R, dRdx, dRdy, result);
#else
	bool status = ts->environment(th, thread_info,
	                              options, R, dRdx, dRdy,
	                              nchannels, result);
#endif

	if(!status) {
		if(nchannels == 3 || nchannels == 4) {
			result[0] = 1.0f;
			result[1] = 0.0f;
			result[2] = 1.0f;

			if(nchannels == 4)
				result[3] = 1.0f;
		}
	}

	return status;
}

bool OSLRenderServices::get_texture_info(OSL::ShaderGlobals *sg, ustring filename, int subimage,
                                         ustring dataname,
                                         TypeDesc datatype, void *data)
{
	OSL::TextureSystem *ts = osl_ts;
	return ts->get_texture_info(filename, subimage, dataname, datatype, data);
}

int OSLRenderServices::pointcloud_search(OSL::ShaderGlobals *sg, ustring filename, const OSL::Vec3 &center,
                                         float radius, int max_points, bool sort,
                                         size_t *out_indices, float *out_distances, int derivs_offset)
{
	return 0;
}

int OSLRenderServices::pointcloud_get(OSL::ShaderGlobals *sg, ustring filename, size_t *indices, int count,
                                      ustring attr_name, TypeDesc attr_type, void *out_data)
{
	return 0;
}

bool OSLRenderServices::pointcloud_write(OSL::ShaderGlobals *sg,
                                         ustring filename, const OSL::Vec3 &pos,
                                         int nattribs, const ustring *names,
                                         const TypeDesc *types,
                                         const void **data)
{
	return false;
}

bool OSLRenderServices::trace(TraceOpt &options, OSL::ShaderGlobals *sg,
	const OSL::Vec3 &P, const OSL::Vec3 &dPdx,
	const OSL::Vec3 &dPdy, const OSL::Vec3 &R,
	const OSL::Vec3 &dRdx, const OSL::Vec3 &dRdy)
{
	/* todo: options.shader support, maybe options.traceset */
	ShaderData *sd = (ShaderData *)(sg->renderstate);

	/* setup ray */
	Ray ray;

	ray.P = TO_FLOAT3(P);
	ray.D = TO_FLOAT3(R);
	ray.t = (options.maxdist == 1.0e30f)? FLT_MAX: options.maxdist - options.mindist;
	ray.time = sd->time;

	if(options.mindist == 0.0f) {
		/* avoid self-intersections */
		if(ray.P == sd->P) {
			bool transmit = (dot(sd->Ng, ray.D) < 0.0f);
			ray.P = ray_offset(sd->P, (transmit)? -sd->Ng: sd->Ng);
		}
	}
	else {
		/* offset for minimum distance */
		ray.P += options.mindist*ray.D;
	}

	/* ray differentials */
	ray.dP.dx = TO_FLOAT3(dPdx);
	ray.dP.dy = TO_FLOAT3(dPdy);
	ray.dD.dx = TO_FLOAT3(dRdx);
	ray.dD.dy = TO_FLOAT3(dRdy);

	/* allocate trace data */
	OSLTraceData *tracedata = (OSLTraceData*)sg->tracedata;
	tracedata->ray = ray;
	tracedata->setup = false;
	tracedata->init = true;
	tracedata->sd.osl_globals = sd->osl_globals;

	/* raytrace */
	return scene_intersect(sd->osl_globals, &ray, PATH_RAY_ALL_VISIBILITY, &tracedata->isect, NULL, 0.0f, 0.0f);
}


bool OSLRenderServices::getmessage(OSL::ShaderGlobals *sg, ustring source, ustring name,
	TypeDesc type, void *val, bool derivatives)
{
	OSLTraceData *tracedata = (OSLTraceData*)sg->tracedata;

	if(source == u_trace && tracedata->init) {
		if(name == u_hit) {
			return set_attribute_int((tracedata->isect.prim != PRIM_NONE), type, derivatives, val);
		}
		else if(tracedata->isect.prim != PRIM_NONE) {
			if(name == u_hitdist) {
				float f[3] = {tracedata->isect.t, 0.0f, 0.0f};
				return set_attribute_float(f, type, derivatives, val);
			}
			else {
				ShaderData *sd = &tracedata->sd;
				KernelGlobals *kg = sd->osl_globals;

				if(!tracedata->setup) {
					/* lazy shader data setup */
					ShaderData *original_sd = (ShaderData *)(sg->renderstate);
					int bounce = original_sd->ray_depth + 1;
					int transparent_bounce = original_sd->transparent_depth;

					shader_setup_from_ray(kg, sd, &tracedata->isect, &tracedata->ray, bounce, transparent_bounce);
					tracedata->setup = true;
				}

				if(name == u_N) {
					return set_attribute_float3(sd->N, type, derivatives, val);
				}
				else if(name == u_Ng) {
					return set_attribute_float3(sd->Ng, type, derivatives, val);
				}
				else if(name == u_P) {
					float3 f[3] = {sd->P, sd->dP.dx, sd->dP.dy};
					return set_attribute_float3(f, type, derivatives, val);
				}
				else if(name == u_I) {
					float3 f[3] = {sd->I, sd->dI.dx, sd->dI.dy};
					return set_attribute_float3(f, type, derivatives, val);
				}
				else if(name == u_u) {
					float f[3] = {sd->u, sd->du.dx, sd->du.dy};
					return set_attribute_float(f, type, derivatives, val);
				}
				else if(name == u_v) {
					float f[3] = {sd->v, sd->dv.dx, sd->dv.dy};
					return set_attribute_float(f, type, derivatives, val);
				}

				return get_attribute(sd, derivatives, u_empty, type, name, val);
			}
		}
	}

	return false;
}

CCL_NAMESPACE_END
