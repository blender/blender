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

#include "osl_services.h"
#include "osl_shader.h"

#include "util_foreach.h"
#include "util_string.h"

#include "kernel_compat_cpu.h"
#include "kernel_globals.h"
#include "kernel_object.h"
#include "kernel_triangle.h"

CCL_NAMESPACE_BEGIN

/* RenderServices implementation */

#define TO_MATRIX44(m) (*(OSL::Matrix44 *)&(m))

/* static ustrings */
ustring OSLRenderServices::u_distance("distance");
ustring OSLRenderServices::u_index("index");
ustring OSLRenderServices::u_camera("camera");
ustring OSLRenderServices::u_screen("screen");
ustring OSLRenderServices::u_raster("raster");
ustring OSLRenderServices::u_ndc("NDC");
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
			Transform tfm = object_fetch_transform(kg, object, time, OBJECT_TRANSFORM);
			tfm = transform_transpose(tfm);
			result = TO_MATRIX44(tfm);

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
			Transform tfm = object_fetch_transform(kg, object, time, OBJECT_INVERSE_TRANSFORM);
			tfm = transform_transpose(tfm);
			result = TO_MATRIX44(tfm);

			return true;
		}
	}

	return false;
}

bool OSLRenderServices::get_matrix(OSL::Matrix44 &result, ustring from, float time)
{
	KernelGlobals *kg = kernel_globals;

	if (from == u_ndc) {
		Transform tfm = transform_transpose(kernel_data.cam.ndctoworld);
		result = TO_MATRIX44(tfm);
		return true;
	}
	else if (from == u_raster) {
		Transform tfm = transform_transpose(kernel_data.cam.rastertoworld);
		result = TO_MATRIX44(tfm);
		return true;
	}
	else if (from == u_screen) {
		Transform tfm = transform_transpose(kernel_data.cam.screentoworld);
		result = TO_MATRIX44(tfm);
		return true;
	}
	else if (from == u_camera) {
		Transform tfm = transform_transpose(kernel_data.cam.cameratoworld);
		result = TO_MATRIX44(tfm);
		return true;
	}

	return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::Matrix44 &result, ustring to, float time)
{
	KernelGlobals *kg = kernel_globals;

	if (to == u_ndc) {
		Transform tfm = transform_transpose(kernel_data.cam.worldtondc);
		result = TO_MATRIX44(tfm);
		return true;
	}
	else if (to == u_raster) {
		Transform tfm = transform_transpose(kernel_data.cam.worldtoraster);
		result = TO_MATRIX44(tfm);
		return true;
	}
	else if (to == u_screen) {
		Transform tfm = transform_transpose(kernel_data.cam.worldtoscreen);
		result = TO_MATRIX44(tfm);
		return true;
	}
	else if (to == u_camera) {
		Transform tfm = transform_transpose(kernel_data.cam.worldtocamera);
		result = TO_MATRIX44(tfm);
		return true;
	}

	return false;
}

bool OSLRenderServices::get_matrix(OSL::Matrix44 &result, OSL::TransformationPtr xform)
{
	// XXX implementation
	return true;
}

bool OSLRenderServices::get_matrix(OSL::Matrix44 &result, ustring from)
{
	// XXX implementation
	return true;
}

bool OSLRenderServices::get_array_attribute(void *renderstate, bool derivatives, 
                                            ustring object, TypeDesc type, ustring name,
                                            int index, void *val)
{
	return false;
}

static void set_attribute_float3(float3 f[3], TypeDesc type, bool derivatives, void *val)
{
	if (type == TypeDesc::TypePoint || type == TypeDesc::TypeVector ||
	        type == TypeDesc::TypeNormal || type == TypeDesc::TypeColor) {
		float3 *fval = (float3 *)val;
		fval[0] = f[0];
		if (derivatives) {
			fval[1] = f[1];
			fval[2] = f[2];
		}
	}
	else {
		float *fval = (float *)val;
		fval[0] = average(f[0]);
		if (derivatives) {
			fval[1] = average(f[1]);
			fval[2] = average(f[2]);
		}
	}
}

static void set_attribute_float(float f[3], TypeDesc type, bool derivatives, void *val)
{
	if (type == TypeDesc::TypePoint || type == TypeDesc::TypeVector ||
	        type == TypeDesc::TypeNormal || type == TypeDesc::TypeColor) {
		float3 *fval = (float3 *)val;
		fval[0] = make_float3(f[0], f[0], f[0]);
		if (derivatives) {
			fval[1] = make_float3(f[1], f[2], f[1]);
			fval[2] = make_float3(f[2], f[2], f[2]);
		}
	}
	else {
		float *fval = (float *)val;
		fval[0] = f[0];
		if (derivatives) {
			fval[1] = f[1];
			fval[2] = f[2];
		}
	}
}

static bool get_mesh_attribute(KernelGlobals *kg, const ShaderData *sd, const OSLGlobals::Attribute& attr,
                               const TypeDesc& type, bool derivatives, void *val)
{
	if (attr.type == TypeDesc::TypeFloat) {
		float fval[3];
		fval[0] = triangle_attribute_float(kg, sd, attr.elem, attr.offset,
		                                   (derivatives) ? &fval[1] : NULL, (derivatives) ? &fval[2] : NULL);
		set_attribute_float(fval, type, derivatives, val);
		return true;
	}
	else if (attr.type == TypeDesc::TypePoint || attr.type == TypeDesc::TypeVector ||
	         attr.type == TypeDesc::TypeNormal || attr.type == TypeDesc::TypeColor) {
		/* todo: this won't work when float3 has w component */
		float3 fval[3];
		fval[0] = triangle_attribute_float3(kg, sd, attr.elem, attr.offset,
		                                    (derivatives) ? &fval[1] : NULL, (derivatives) ? &fval[2] : NULL);
		set_attribute_float3(fval, type, derivatives, val);
		return true;
	}
	else
		return false;
}

static void get_object_attribute(const OSLGlobals::Attribute& attr, bool derivatives, void *val)
{
	size_t datasize = attr.value.datasize();

	memcpy(val, attr.value.data(), datasize);
	if (derivatives)
		memset((char *)val + datasize, 0, datasize * 2);
}

static bool get_object_standard_attribute(KernelGlobals *kg, ShaderData *sd, ustring name,
                                          TypeDesc type, bool derivatives, void *val)
{
	/* Object Attributes */
	if (name == "std::object_location") {
		float3 fval[3];
		fval[0] = object_location(kg, sd);
		fval[1] = fval[2] = make_float3(0.0, 0.0, 0.0);	/* derivates set to 0 */
		set_attribute_float3(fval, type, derivatives, val);
		return true;
	}
	else if (name == "std::object_index") {
		float fval[3];
		fval[0] = object_pass_id(kg, sd->object);
		fval[1] = fval[2] = 0.0;	/* derivates set to 0 */
		set_attribute_float(fval, type, derivatives, val);
		return true;
	}
	else if (name == "std::material_index") {
		float fval[3];
		fval[0] = shader_pass_id(kg, sd);
		fval[1] = fval[2] = 0.0;	/* derivates set to 0 */
		set_attribute_float(fval, type, derivatives, val);
		return true;
	}
	else if (name == "std::object_random") {
		float fval[3];
		fval[0] = object_random_number(kg, sd->object);
		fval[1] = fval[2] = 0.0;	/* derivates set to 0 */
		set_attribute_float(fval, type, derivatives, val);
		return true;
	}

	/* Particle Attributes */
	else if (name == "std::particle_index") {
		float fval[3];
		uint particle_id = object_particle_id(kg, sd->object);
		fval[0] = particle_index(kg, particle_id);
		fval[1] = fval[2] = 0.0;	/* derivates set to 0 */
		set_attribute_float(fval, type, derivatives, val);
		return true;
	}
	else if (name == "std::particle_age") {
		float fval[3];
		uint particle_id = object_particle_id(kg, sd->object);
		fval[0] = particle_age(kg, particle_id);
		fval[1] = fval[2] = 0.0;	/* derivates set to 0 */
		set_attribute_float(fval, type, derivatives, val);
		return true;
	}
	else if (name == "std::particle_lifetime") {
		float fval[3];
		uint particle_id = object_particle_id(kg, sd->object);
		fval[0] = particle_lifetime(kg, particle_id);
		fval[1] = fval[2] = 0.0;	/* derivates set to 0 */
		set_attribute_float(fval, type, derivatives, val);
		return true;
	}
	else if (name == "std::particle_location") {
		float3 fval[3];
		uint particle_id = object_particle_id(kg, sd->object);
		fval[0] = particle_location(kg, particle_id);
		fval[1] = fval[2] = make_float3(0.0, 0.0, 0.0);	/* derivates set to 0 */
		set_attribute_float3(fval, type, derivatives, val);
		return true;
	}
#if 0	/* unsupported */
	else if (name == "std::particle_rotation") {
		float4 fval[3];
		uint particle_id = object_particle_id(kg, sd->object);
		fval[0] = particle_rotation(kg, particle_id);
		fval[1] = fval[2] = make_float4(0.0, 0.0, 0.0, 0.0);	/* derivates set to 0 */
		set_attribute_float4(fval, type, derivatives, val);
		return true;
	}
#endif
	else if (name == "std::particle_size") {
		float fval[3];
		uint particle_id = object_particle_id(kg, sd->object);
		fval[0] = particle_size(kg, particle_id);
		fval[1] = fval[2] = 0.0;	/* derivates set to 0 */
		set_attribute_float(fval, type, derivatives, val);
		return true;
	}
	else if (name == "std::particle_velocity") {
		float3 fval[3];
		uint particle_id = object_particle_id(kg, sd->object);
		fval[0] = particle_velocity(kg, particle_id);
		fval[1] = fval[2] = make_float3(0.0, 0.0, 0.0);	/* derivates set to 0 */
		set_attribute_float3(fval, type, derivatives, val);
		return true;
	}
	else if (name == "std::particle_angular_velocity") {
		float3 fval[3];
		uint particle_id = object_particle_id(kg, sd->object);
		fval[0] = particle_angular_velocity(kg, particle_id);
		fval[1] = fval[2] = make_float3(0.0, 0.0, 0.0);	/* derivates set to 0 */
		set_attribute_float3(fval, type, derivatives, val);
		return true;
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
		OSLGlobals::ObjectNameMap::iterator it = kg->osl.object_name_map.find(object_name);

		if (it == kg->osl.object_name_map.end())
			return false;

		object = it->second;
		tri = ~0;
	}
	else if (object == ~0) {
		/* no background attributes supported */
		return false;
	}

	/* find attribute on object */
	OSLGlobals::AttributeMap& attribute_map = kg->osl.attribute_map[object];
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
		return get_object_standard_attribute(kg, sd, name, type, derivatives, val);
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

int OSLRenderServices::pointcloud_search(OSL::ShaderGlobals *sg, ustring filename, const OSL::Vec3 &center,
		float radius, int max_points, bool sort, size_t *out_indices, float *out_distances, int derivs_offset)
{
    return 0;
}

int OSLRenderServices::pointcloud_get(ustring filename, size_t *indices, int count,
		ustring attr_name, TypeDesc attr_type, void *out_data)
{
    return 0;
}

CCL_NAMESPACE_END
