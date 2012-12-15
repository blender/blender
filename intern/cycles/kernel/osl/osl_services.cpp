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
#include "kernel_montecarlo.h"
#include "kernel_projection.h"
#include "kernel_differential.h"
#include "kernel_object.h"
#include "kernel_bvh.h"
#include "kernel_attribute.h"
#include "kernel_projection.h"
#include "kernel_triangle.h"
#include "kernel_accumulate.h"
#include "kernel_shader.h"

CCL_NAMESPACE_BEGIN

/* RenderServices implementation */

#define COPY_MATRIX44(m1, m2) memcpy(m1, m2, sizeof(*m2))

/* static ustrings */
ustring OSLRenderServices::u_distance("distance");
ustring OSLRenderServices::u_index("index");
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
ustring OSLRenderServices::u_path_ray_length("path:ray_length");
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
}

OSLRenderServices::~OSLRenderServices()
{
}

void OSLRenderServices::thread_init(KernelGlobals *kernel_globals_)
{
	kernel_globals = kernel_globals_;
}

bool OSLRenderServices::get_matrix(OSL::Matrix44 &result, OSL::TransformationPtr xform, float time)
{
	/* this is only used for shader and object space, we don't really have
	 * a concept of shader space, so we just use object space for both. */
	if (xform) {
		KernelGlobals *kg = kernel_globals;
		const ShaderData *sd = (const ShaderData *)xform;
		int object = sd->object;

		if (object != ~0) {
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

bool OSLRenderServices::get_inverse_matrix(OSL::Matrix44 &result, OSL::TransformationPtr xform, float time)
{
	/* this is only used for shader and object space, we don't really have
	 * a concept of shader space, so we just use object space for both. */
	if (xform) {
		KernelGlobals *kg = kernel_globals;
		const ShaderData *sd = (const ShaderData *)xform;
		int object = sd->object;

		if (object != ~0) {
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

bool OSLRenderServices::get_matrix(OSL::Matrix44 &result, ustring from, float time)
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

bool OSLRenderServices::get_inverse_matrix(OSL::Matrix44 &result, ustring to, float time)
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

bool OSLRenderServices::get_matrix(OSL::Matrix44 &result, OSL::TransformationPtr xform)
{
	/* this is only used for shader and object space, we don't really have
	 * a concept of shader space, so we just use object space for both. */
	if (xform) {
		const ShaderData *sd = (const ShaderData *)xform;
		int object = sd->object;

		if (object != ~0) {
#ifdef __OBJECT_MOTION__
			Transform tfm = sd->ob_tfm;
#else
			KernelGlobals *kg = kernel_globals;
			Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
#endif
			tfm = transform_transpose(tfm);
			COPY_MATRIX44(&result, &tfm);

			return true;
		}
	}

	return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::Matrix44 &result, OSL::TransformationPtr xform)
{
	/* this is only used for shader and object space, we don't really have
	 * a concept of shader space, so we just use object space for both. */
	if (xform) {
		const ShaderData *sd = (const ShaderData *)xform;
		int object = sd->object;

		if (object != ~0) {
#ifdef __OBJECT_MOTION__
			Transform tfm = sd->ob_itfm;
#else
			KernelGlobals *kg = kernel_globals;
			Transform tfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);
#endif
			tfm = transform_transpose(tfm);
			COPY_MATRIX44(&result, &tfm);

			return true;
		}
	}

	return false;
}

bool OSLRenderServices::get_matrix(OSL::Matrix44 &result, ustring from)
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

bool OSLRenderServices::get_inverse_matrix(OSL::Matrix44 &result, ustring to)
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

bool OSLRenderServices::get_array_attribute(void *renderstate, bool derivatives, 
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
	if(type.basetype == TypeDesc::INT && type.aggregate == TypeDesc::SCALAR && type.arraylen == 0) {
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

static bool get_mesh_attribute(KernelGlobals *kg, const ShaderData *sd, const OSLGlobals::Attribute& attr,
                               const TypeDesc& type, bool derivatives, void *val)
{
	if (attr.type == TypeDesc::TypePoint || attr.type == TypeDesc::TypeVector ||
	    attr.type == TypeDesc::TypeNormal || attr.type == TypeDesc::TypeColor)
	{
		float3 fval[3];
		fval[0] = triangle_attribute_float3(kg, sd, attr.elem, attr.offset,
		                                    (derivatives) ? &fval[1] : NULL, (derivatives) ? &fval[2] : NULL);
		return set_attribute_float3(fval, type, derivatives, val);
	}
	else if (attr.type == TypeDesc::TypeFloat) {
		float fval[3];
		fval[0] = triangle_attribute_float(kg, sd, attr.elem, attr.offset,
		                                   (derivatives) ? &fval[1] : NULL, (derivatives) ? &fval[2] : NULL);
		return set_attribute_float(fval, type, derivatives, val);
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
		uint particle_id = object_particle_id(kg, sd->object);
		float f = particle_index(kg, particle_id);
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_particle_age) {
		uint particle_id = object_particle_id(kg, sd->object);
		float f = particle_age(kg, particle_id);
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_particle_lifetime) {
		uint particle_id = object_particle_id(kg, sd->object);
		float f= particle_lifetime(kg, particle_id);
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_particle_location) {
		uint particle_id = object_particle_id(kg, sd->object);
		float3 f = particle_location(kg, particle_id);
		return set_attribute_float3(f, type, derivatives, val);
	}
#if 0	/* unsupported */
	else if (name == u_particle_rotation) {
		uint particle_id = object_particle_id(kg, sd->object);
		float4 f = particle_rotation(kg, particle_id);
		return set_attribute_float4(f, type, derivatives, val);
	}
#endif
	else if (name == u_particle_size) {
		uint particle_id = object_particle_id(kg, sd->object);
		float f = particle_size(kg, particle_id);
		return set_attribute_float(f, type, derivatives, val);
	}
	else if (name == u_particle_velocity) {
		uint particle_id = object_particle_id(kg, sd->object);
		float3 f = particle_velocity(kg, particle_id);
		return set_attribute_float3(f, type, derivatives, val);
	}
	else if (name == u_particle_angular_velocity) {
		uint particle_id = object_particle_id(kg, sd->object);
		float3 f = particle_angular_velocity(kg, particle_id);
		return set_attribute_float3(f, type, derivatives, val);
	}
	else if (name == u_geom_numpolyvertices) {
		return set_attribute_int(3, type, derivatives, val);
	}
	else if (name == u_geom_trianglevertices || name == u_geom_polyvertices) {
		float3 P[3];
		triangle_vertices(kg, sd->prim, P);

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
	else
		return false;
}

bool OSLRenderServices::get_background_attribute(KernelGlobals *kg, ShaderData *sd, ustring name,
                                                 TypeDesc type, bool derivatives, void *val)
{
	/* Ray Length */
	if (name == u_path_ray_length) {
		float f = sd->ray_length;
		return set_attribute_float(f, type, derivatives, val);
	}
	
	else
		return false;
}

bool OSLRenderServices::get_attribute(void *renderstate, bool derivatives, ustring object_name,
                                      TypeDesc type, ustring name, void *val)
{
	KernelGlobals *kg = kernel_globals;
	ShaderData *sd = (ShaderData *)renderstate;
	int object = sd->object;
	int tri = sd->prim;

	/* lookup of attribute on another object */
	if (object_name != u_empty) {
		OSLGlobals::ObjectNameMap::iterator it = kg->osl->object_name_map.find(object_name);

		if (it == kg->osl->object_name_map.end())
			return false;

		object = it->second;
		tri = ~0;
	}
	else if (object == ~0) {
		return get_background_attribute(kg, sd, name, type, derivatives, val);
	}

	/* find attribute on object */
	OSLGlobals::AttributeMap& attribute_map = kg->osl->attribute_map[object];
	OSLGlobals::AttributeMap::iterator it = attribute_map.find(name);

	if (it != attribute_map.end()) {
		const OSLGlobals::Attribute& attr = it->second;

		if (attr.elem != ATTR_ELEMENT_VALUE) {
			/* triangle and vertex attributes */
			if (tri != ~0)
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
                                     void *renderstate, void *val)
{
	return false; /* disabled by lockgeom */
}

bool OSLRenderServices::has_userdata(ustring name, TypeDesc type, void *renderstate)
{
	return false; /* never called by OSL */
}

bool OSLRenderServices::texture(ustring filename, TextureOpt &options,
                                OSL::ShaderGlobals *sg,
                                float s, float t, float dsdx, float dtdx,
                                float dsdy, float dtdy, float *result)
{
	OSL::TextureSystem *ts = kernel_globals->osl->ts;
	bool status = ts->texture(filename, options, s, t, dsdx, dtdx, dsdy, dtdy, result);

	if(!status) {
		if(options.nchannels == 3 || options.nchannels == 4) {
			result[0] = 1.0f;
			result[1] = 0.0f;
			result[2] = 1.0f;

			if(options.nchannels == 4)
				result[3] = 1.0f;
		}
	}

	return status;
}

bool OSLRenderServices::texture3d(ustring filename, TextureOpt &options,
                                  OSL::ShaderGlobals *sg, const OSL::Vec3 &P,
                                  const OSL::Vec3 &dPdx, const OSL::Vec3 &dPdy,
                                  const OSL::Vec3 &dPdz, float *result)
{
	OSL::TextureSystem *ts = kernel_globals->osl->ts;
	bool status = ts->texture3d(filename, options, P, dPdx, dPdy, dPdz, result);

	if(!status) {
		if(options.nchannels == 3 || options.nchannels == 4) {
			result[0] = 1.0f;
			result[1] = 0.0f;
			result[2] = 1.0f;

			if(options.nchannels == 4)
				result[3] = 1.0f;
		}

	}

	return status;
}

bool OSLRenderServices::environment(ustring filename, TextureOpt &options,
                                    OSL::ShaderGlobals *sg, const OSL::Vec3 &R,
                                    const OSL::Vec3 &dRdx, const OSL::Vec3 &dRdy, float *result)
{
	OSL::TextureSystem *ts = kernel_globals->osl->ts;
	bool status = ts->environment(filename, options, R, dRdx, dRdy, result);

	if(!status) {
		if(options.nchannels == 3 || options.nchannels == 4) {
			result[0] = 1.0f;
			result[1] = 0.0f;
			result[2] = 1.0f;

			if(options.nchannels == 4)
				result[3] = 1.0f;
		}
	}

	return status;
}

bool OSLRenderServices::get_texture_info(ustring filename, int subimage,
                                         ustring dataname,
                                         TypeDesc datatype, void *data)
{
	OSL::TextureSystem *ts = kernel_globals->osl->ts;
	return ts->get_texture_info(filename, subimage, dataname, datatype, data);
}

int OSLRenderServices::pointcloud_search(OSL::ShaderGlobals *sg, ustring filename, const OSL::Vec3 &center,
                                         float radius, int max_points, bool sort,
                                         size_t *out_indices, float *out_distances, int derivs_offset)
{
	return 0;
}

int OSLRenderServices::pointcloud_get(ustring filename, size_t *indices, int count,
                                      ustring attr_name, TypeDesc attr_type, void *out_data)
{
	return 0;
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
	ray.t = (options.maxdist == 1.0e30)? FLT_MAX: options.maxdist - options.mindist;
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

	/* raytrace */
	return scene_intersect(kernel_globals, &ray, ~0, &tracedata->isect);
}


bool OSLRenderServices::getmessage(OSL::ShaderGlobals *sg, ustring source, ustring name,
	TypeDesc type, void *val, bool derivatives)
{
	OSLTraceData *tracedata = (OSLTraceData*)sg->tracedata;

	if(source == u_trace && tracedata->init) {
		if(name == u_hit) {
			return set_attribute_int((tracedata->isect.prim != ~0), type, derivatives, val);
		}
		else if(tracedata->isect.prim != ~0) {
			if(name == u_hitdist) {
				float f[3] = {tracedata->isect.t, 0.0f, 0.0f};
				return set_attribute_float(f, type, derivatives, val);
			}
			else {
				KernelGlobals *kg = kernel_globals;
				ShaderData *sd = &tracedata->sd;

				if(!tracedata->setup) {
					/* lazy shader data setup */
					shader_setup_from_ray(kg, sd, &tracedata->isect, &tracedata->ray);
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
