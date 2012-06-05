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
	   a concept of shader space, so we just use object space for both. */
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
	   a concept of shader space, so we just use object space for both. */
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

bool OSLRenderServices::get_array_attribute(void *renderstate, bool derivatives, 
                                            ustring object, TypeDesc type, ustring name,
                                            int index, void *val)
{
	return false;
}

static bool get_mesh_attribute(KernelGlobals *kg, const ShaderData *sd,
                               const OSLGlobals::Attribute& attr, bool derivatives, void *val)
{
	if (attr.type == TypeDesc::TypeFloat) {
		float *fval = (float *)val;
		fval[0] = triangle_attribute_float(kg, sd, attr.elem, attr.offset,
		                                   (derivatives) ? &fval[1] : NULL, (derivatives) ? &fval[2] : NULL);
	}
	else {
		/* todo: this won't work when float3 has w component */
		float3 *fval = (float3 *)val;
		fval[0] = triangle_attribute_float3(kg, sd, attr.elem, attr.offset,
		                                    (derivatives) ? &fval[1] : NULL, (derivatives) ? &fval[2] : NULL);
	}

	return true;
}

static bool get_mesh_attribute_convert(KernelGlobals *kg, const ShaderData *sd,
                                       const OSLGlobals::Attribute& attr, const TypeDesc& type, bool derivatives, void *val)
{
	if (attr.type == TypeDesc::TypeFloat) {
		float tmp[3];
		float3 *fval = (float3 *)val;

		get_mesh_attribute(kg, sd, attr, derivatives, tmp);

		fval[0] = make_float3(tmp[0], tmp[0], tmp[0]);
		if (derivatives) {
			fval[1] = make_float3(tmp[1], tmp[1], tmp[1]);
			fval[2] = make_float3(tmp[2], tmp[2], tmp[2]);
		}

		return true;
	}
	else if (attr.type == TypeDesc::TypePoint || attr.type == TypeDesc::TypeVector ||
	         attr.type == TypeDesc::TypeNormal || attr.type == TypeDesc::TypeColor)
	{
		float3 tmp[3];
		float *fval = (float *)val;

		get_mesh_attribute(kg, sd, attr, derivatives, tmp);

		fval[0] = average(tmp[0]);
		if (derivatives) {
			fval[1] = average(tmp[1]);
			fval[2] = average(tmp[2]);
		}

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

bool OSLRenderServices::get_attribute(void *renderstate, bool derivatives, ustring object_name,
                                      TypeDesc type, ustring name, void *val)
{
	KernelGlobals *kg = kernel_globals;
	const ShaderData *sd = (const ShaderData *)renderstate;
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

	if (it == attribute_map.end())
		return false;

	/* type mistmatch? */
	const OSLGlobals::Attribute& attr = it->second;

	if (attr.elem != ATTR_ELEMENT_VALUE) {
		/* triangle and vertex attributes */
		if (tri != ~0) {
			if (attr.type == type || (attr.type == TypeDesc::TypeColor &&
			                          (type == TypeDesc::TypePoint || type == TypeDesc::TypeVector || type == TypeDesc::TypeNormal)))
			{
				return get_mesh_attribute(kg, sd, attr, derivatives, val);
			}
			else {
				return get_mesh_attribute_convert(kg, sd, attr, type, derivatives, val);
			}
		}
	}
	else {
		/* object attribute */
		get_object_attribute(attr, derivatives, val);
		return true;
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

void *OSLRenderServices::get_pointcloud_attr_query(ustring *attr_names,
                                                   TypeDesc *attr_types, int nattrs)
{
#ifdef WITH_PARTIO
	m_attr_queries.push_back(AttrQuery());
	AttrQuery &query = m_attr_queries.back();

	/* make space for what we need. the only reason to use
	   std::vector is to skip the delete */
	query.attr_names.resize(nattrs);
	query.attr_partio_types.resize(nattrs);
	/* capacity will keep the length of the smallest array passed
	   to the query. Just to prevent buffer overruns */
	query.capacity = -1;

	for (int i = 0; i < nattrs; ++i) {
		query.attr_names[i] = attr_names[i];

		TypeDesc element_type = attr_types[i].elementtype();

		if (query.capacity < 0)
			query.capacity = attr_types[i].numelements();
		else
			query.capacity = min(query.capacity, (int)attr_types[i].numelements());

		/* convert the OSL (OIIO) type to the equivalent Partio type so
		   we can do a fast check at query time. */
		if (element_type == TypeDesc::TypeFloat) {
			query.attr_partio_types[i] = Partio::FLOAT;
		}
		else if (element_type == TypeDesc::TypeInt) {
			query.attr_partio_types[i] = Partio::INT;
		}
		else if (element_type == TypeDesc::TypeColor  || element_type == TypeDesc::TypePoint ||
		         element_type == TypeDesc::TypeVector || element_type == TypeDesc::TypeNormal)
		{
			query.attr_partio_types[i] = Partio::VECTOR;
		}
		else {
			return NULL;  /* report some error of unknown type */
		}
	}

	/* this is valid until the end of RenderServices */
	return &query;
#else
	return NULL;
#endif
}

#ifdef WITH_PARTIO
Partio::ParticlesData *OSLRenderServices::get_pointcloud(ustring filename)
{
	return Partio::readCached(filename.c_str(), true);
}

#endif

int OSLRenderServices::pointcloud(ustring filename, const OSL::Vec3 &center, float radius,
                                  int max_points, void *_attr_query, void **attr_outdata)
{
	/* todo: this code has never been tested, and most likely does not
	   work. it's based on the example code in OSL */

#ifdef WITH_PARTIO
	/* query Partio for this pointcloud lookup using cached attr_query */
	if (!_attr_query)
		return 0;

	AttrQuery *attr_query = (AttrQuery *)_attr_query;
	if (attr_query->capacity < max_points)
		return 0;

	/* get the pointcloud entry for the given filename */
	Partio::ParticlesData *cloud = get_pointcloud(filename);

	/* now we have to look up all the attributes in the file. we can't do this
	   before hand cause we never know what we are going to load. */
	int nattrs = attr_query->attr_names.size();
	Partio::ParticleAttribute *attr = (Partio::ParticleAttribute *)alloca(sizeof(Partio::ParticleAttribute) * nattrs);

	for (int i = 0; i < nattrs; ++i) {
		/* special case attributes */
		if (attr_query->attr_names[i] == u_distance || attr_query->attr_names[i] == u_index)
			continue;

		/* lookup the attribute by name*/
		if (!cloud->attributeInfo(attr_query->attr_names[i].c_str(), attr[i])) {
			/* issue an error here and return, types don't match */
			Partio::endCachedAccess(cloud);
			cloud->release();
			return 0;
		}
	}

	std::vector<Partio::ParticleIndex> indices;
	std::vector<float> dist2;

	Partio::beginCachedAccess(cloud);

	/* finally, do the lookup */
	cloud->findNPoints((const float *)&center, max_points, radius, indices, dist2);
	int count = indices.size();

	/* retrieve the attributes directly to user space */
	for (int j = 0; j < nattrs; ++j) {
		/* special cases */
		if (attr_query->attr_names[j] == u_distance) {
			for (int i = 0; i < count; ++i)
				((float *)attr_outdata[j])[i] = sqrtf(dist2[i]);
		}
		else if (attr_query->attr_names[j] == u_index) {
			for (int i = 0; i < count; ++i)
				((int *)attr_outdata[j])[i] = indices[i];
		}
		else {
			/* note we make a single call per attribute, we don't loop over the
			   points. Partio does it, so it is there that we have to care about
			   performance */
			cloud->data(attr[j], count, &indices[0], true, attr_outdata[j]);
		}
	}

	Partio::endCachedAccess(cloud);
	cloud->release();

	return count;
#else
	return 0;
#endif
}

CCL_NAMESPACE_END

