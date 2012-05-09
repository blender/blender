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

#include "bvh.h"
#include "bvh_build.h"

#include "device.h"
#include "shader.h"
#include "light.h"
#include "mesh.h"
#include "object.h"
#include "scene.h"

#include "osl_globals.h"

#include "util_cache.h"
#include "util_foreach.h"
#include "util_progress.h"
#include "util_set.h"

CCL_NAMESPACE_BEGIN

/* Mesh */

Mesh::Mesh()
{
	need_update = true;
	transform_applied = false;
	transform_negative_scaled = false;
	displacement_method = DISPLACE_BUMP;
	bounds = BoundBox::empty;

	bvh = NULL;

	tri_offset = 0;
	vert_offset = 0;

	attributes.mesh = this;
}

Mesh::~Mesh()
{
	delete bvh;
}

void Mesh::reserve(int numverts, int numtris)
{
	/* reserve space to add verts and triangles later */
	verts.resize(numverts);
	triangles.resize(numtris);
	shader.resize(numtris);
	smooth.resize(numtris);
	attributes.reserve(numverts, numtris);
}

void Mesh::clear()
{
	/* clear all verts and triangles */
	verts.clear();
	triangles.clear();
	shader.clear();
	smooth.clear();

	attributes.clear();
	used_shaders.clear();

	transform_applied = false;
	transform_negative_scaled = false;
}

void Mesh::add_triangle(int v0, int v1, int v2, int shader_, bool smooth_)
{
	Triangle t;
	t.v[0] = v0;
	t.v[1] = v1;
	t.v[2] = v2;

	triangles.push_back(t);
	shader.push_back(shader_);
	smooth.push_back(smooth_);
}

void Mesh::compute_bounds()
{
	BoundBox bnds = BoundBox::empty;
	size_t verts_size = verts.size();

	for(size_t i = 0; i < verts_size; i++)
		bnds.grow(verts[i]);

	/* happens mostly on empty meshes */
	if(!bnds.valid())
		bnds.grow(make_float3(0.0f, 0.0f, 0.0f));

	bounds = bnds;
}

void Mesh::add_face_normals()
{
	/* don't compute if already there */
	if(attributes.find(ATTR_STD_FACE_NORMAL))
		return;

	/* get attributes */
	Attribute *attr_fN = attributes.add(ATTR_STD_FACE_NORMAL);
	float3 *fN = attr_fN->data_float3();

	/* compute face normals */
	size_t triangles_size = triangles.size();
	bool flip = transform_negative_scaled;

	if(triangles_size) {
		float3 *verts_ptr = &verts[0];
		Triangle *triangles_ptr = &triangles[0];

		for(size_t i = 0; i < triangles_size; i++) {
			Triangle t = triangles_ptr[i];
			float3 v0 = verts_ptr[t.v[0]];
			float3 v1 = verts_ptr[t.v[1]];
			float3 v2 = verts_ptr[t.v[2]];

			fN[i] = normalize(cross(v1 - v0, v2 - v0));

			if(flip)
				fN[i] = -fN[i];
		}
	}
}

void Mesh::add_vertex_normals()
{
	/* don't compute if already there */
	if(attributes.find(ATTR_STD_VERTEX_NORMAL))
		return;

	/* get attributes */
	Attribute *attr_fN = attributes.find(ATTR_STD_FACE_NORMAL);
	Attribute *attr_vN = attributes.add(ATTR_STD_VERTEX_NORMAL);

	float3 *fN = attr_fN->data_float3();
	float3 *vN = attr_vN->data_float3();

	/* compute vertex normals */
	memset(vN, 0, verts.size()*sizeof(float3));

	size_t verts_size = verts.size();
	size_t triangles_size = triangles.size();
	bool flip = transform_negative_scaled;

	if(triangles_size) {
		Triangle *triangles_ptr = &triangles[0];

		for(size_t i = 0; i < triangles_size; i++)
			for(size_t j = 0; j < 3; j++)
				vN[triangles_ptr[i].v[j]] += fN[i];
	}

	for(size_t i = 0; i < verts_size; i++) {
		vN[i] = normalize(vN[i]);
		if(flip)
			vN[i] = -vN[i];
	}
}

void Mesh::pack_normals(Scene *scene, float4 *normal, float4 *vnormal)
{
	Attribute *attr_fN = attributes.find(ATTR_STD_FACE_NORMAL);
	Attribute *attr_vN = attributes.find(ATTR_STD_VERTEX_NORMAL);

	float3 *fN = attr_fN->data_float3();
	float3 *vN = attr_vN->data_float3();
	int shader_id = 0;
	uint last_shader = -1;
	bool last_smooth = false;

	size_t triangles_size = triangles.size();
	uint *shader_ptr = (shader.size())? &shader[0]: NULL;

	for(size_t i = 0; i < triangles_size; i++) {
		normal[i].x = fN[i].x;
		normal[i].y = fN[i].y;
		normal[i].z = fN[i].z;

		/* stuff shader id in here too */
		if(shader_ptr[i] != last_shader || last_smooth != smooth[i]) {
			last_shader = shader_ptr[i];
			last_smooth = smooth[i];
			shader_id = scene->shader_manager->get_shader_id(last_shader, this, last_smooth);
		}

		normal[i].w = __int_as_float(shader_id);
	}

	size_t verts_size = verts.size();

	for(size_t i = 0; i < verts_size; i++)
		vnormal[i] = make_float4(vN[i].x, vN[i].y, vN[i].z, 0.0f);
}

void Mesh::pack_verts(float4 *tri_verts, float4 *tri_vindex, size_t vert_offset)
{
	size_t verts_size = verts.size();

	if(verts_size) {
		float3 *verts_ptr = &verts[0];

		for(size_t i = 0; i < verts_size; i++) {
			float3 p = verts_ptr[i];
			tri_verts[i] = make_float4(p.x, p.y, p.z, 0.0f);
		}
	}

	size_t triangles_size = triangles.size();

	if(triangles_size) {
		Triangle *triangles_ptr = &triangles[0];

		for(size_t i = 0; i < triangles_size; i++) {
			Triangle t = triangles_ptr[i];

			tri_vindex[i] = make_float4(
				__int_as_float(t.v[0] + vert_offset),
				__int_as_float(t.v[1] + vert_offset),
				__int_as_float(t.v[2] + vert_offset),
				0);
		}
	}
}

void Mesh::compute_bvh(SceneParams *params, Progress *progress, int n, int total)
{
	if(progress->get_cancel())
		return;

	compute_bounds();

	if(!transform_applied) {
		string msg = "Updating Mesh BVH ";
		if(name == "")
			msg += string_printf("%u/%u", (uint)(n+1), (uint)total);
		else
			msg += string_printf("%s %u/%u", name.c_str(), (uint)(n+1), (uint)total);

		Object object;
		object.mesh = this;

		vector<Object*> objects;
		objects.push_back(&object);

		if(bvh && !need_update_rebuild) {
			progress->set_status(msg, "Refitting BVH");
			bvh->objects = objects;
			bvh->refit(*progress);
		}
		else {
			progress->set_status(msg, "Building BVH");

			BVHParams bparams;
			bparams.use_cache = params->use_bvh_cache;
			bparams.use_spatial_split = params->use_bvh_spatial_split;
			bparams.use_qbvh = params->use_qbvh;

			delete bvh;
			bvh = BVH::create(bparams, objects);
			bvh->build(*progress);
		}
	}

	need_update = false;
	need_update_rebuild = false;
}

void Mesh::tag_update(Scene *scene, bool rebuild)
{
	need_update = true;

	if(rebuild) {
		need_update_rebuild = true;
		scene->light_manager->need_update = true;
	}
	else {
		foreach(uint sindex, used_shaders)
			if(scene->shaders[sindex]->has_surface_emission)
				scene->light_manager->need_update = true;
	}

	scene->mesh_manager->need_update = true;
	scene->object_manager->need_update = true;
}

/* Mesh Manager */

MeshManager::MeshManager()
{
	bvh = NULL;
	need_update = true;
}

MeshManager::~MeshManager()
{
	delete bvh;
}

void MeshManager::update_osl_attributes(Device *device, Scene *scene, vector<AttributeRequestSet>& mesh_attributes)
{
#ifdef WITH_OSL
	/* for OSL, a hash map is used to lookup the attribute by name. */
	OSLGlobals *og = (OSLGlobals*)device->osl_memory();

	og->object_name_map.clear();
	og->attribute_map.clear();

	og->attribute_map.resize(scene->objects.size());

	for(size_t i = 0; i < scene->objects.size(); i++) {
		/* set object name to object index map */
		Object *object = scene->objects[i];
		og->object_name_map[object->name] = i;

		/* set object attributes */
		foreach(ParamValue& attr, object->attributes) {
			OSLGlobals::Attribute osl_attr;

			osl_attr.type = attr.type();
			osl_attr.elem = ATTR_ELEMENT_VALUE;
			osl_attr.value = attr;

			og->attribute_map[i][attr.name()] = osl_attr;
		}

		/* find mesh attributes */
		size_t j;

		for(j = 0; j < scene->meshes.size(); j++)
			if(scene->meshes[j] == object->mesh)
				break;

		AttributeRequestSet& attributes = mesh_attributes[j];

		/* set object attributes */
		foreach(AttributeRequest& req, attributes.requests) {
			OSLGlobals::Attribute osl_attr;

			osl_attr.elem = req.element;
			osl_attr.offset = req.offset;

			if(req.type == TypeDesc::TypeFloat)
				osl_attr.type = TypeDesc::TypeFloat;
			else
				osl_attr.type = TypeDesc::TypeColor;

			if(req.std != ATTR_STD_NONE) {
				/* if standard attribute, add lookup by std:: name convention */
				ustring stdname = ustring(string("std::") + Attribute::standard_name(req.std).c_str());
				og->attribute_map[i][stdname] = osl_attr;
			}
			else if(req.name != ustring()) {
				/* add lookup by mesh attribute name */
				og->attribute_map[i][req.name] = osl_attr;
			}
		}
	}
#endif
}

void MeshManager::update_svm_attributes(Device *device, DeviceScene *dscene, Scene *scene, vector<AttributeRequestSet>& mesh_attributes)
{
	/* for SVM, the attributes_map table is used to lookup the offset of an
	 * attribute, based on a unique shader attribute id. */

	/* compute array stride */
	int attr_map_stride = 0;

	for(size_t i = 0; i < scene->meshes.size(); i++)
		attr_map_stride = max(attr_map_stride, mesh_attributes[i].size()+1);

	if(attr_map_stride == 0)
		return;
	
	/* create attribute map */
	uint4 *attr_map = dscene->attributes_map.resize(attr_map_stride*scene->objects.size());
	memset(attr_map, 0, dscene->attributes_map.size()*sizeof(uint));

	for(size_t i = 0; i < scene->objects.size(); i++) {
		Object *object = scene->objects[i];

		/* find mesh attributes */
		size_t j;

		for(j = 0; j < scene->meshes.size(); j++)
			if(scene->meshes[j] == object->mesh)
				break;

		AttributeRequestSet& attributes = mesh_attributes[j];

		/* set object attributes */
		int index = i*attr_map_stride;

		foreach(AttributeRequest& req, attributes.requests) {
			uint id;

			if(req.std == ATTR_STD_NONE)
				id = scene->shader_manager->get_attribute_id(req.name);
			else
				id = scene->shader_manager->get_attribute_id(req.std);

			attr_map[index].x = id;
			attr_map[index].y = req.element;
			attr_map[index].z = req.offset;

			if(req.type == TypeDesc::TypeFloat)
				attr_map[index].w = NODE_ATTR_FLOAT;
			else
				attr_map[index].w = NODE_ATTR_FLOAT3;

			index++;
		}

		/* terminator */
		attr_map[index].x = ATTR_STD_NONE;
		attr_map[index].y = 0;
		attr_map[index].z = 0;
		attr_map[index].w = 0;
	}

	/* copy to device */
	dscene->data.bvh.attributes_map_stride = attr_map_stride;
	device->tex_alloc("__attributes_map", dscene->attributes_map);
}

void MeshManager::device_update_attributes(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	progress.set_status("Updating Mesh", "Computing attributes");

	/* gather per mesh requested attributes. as meshes may have multiple
	 * shaders assigned, this merges the requested attributes that have
	 * been set per shader by the shader manager */
	vector<AttributeRequestSet> mesh_attributes(scene->meshes.size());

	for(size_t i = 0; i < scene->meshes.size(); i++) {
		Mesh *mesh = scene->meshes[i];

		scene->need_global_attributes(mesh_attributes[i]);

		foreach(uint sindex, mesh->used_shaders) {
			Shader *shader = scene->shaders[sindex];
			mesh_attributes[i].add(shader->attributes);
		}
	}

	/* mesh attribute are stored in a single array per data type. here we fill
	 * those arrays, and set the offset and element type to create attribute
	 * maps next */
	vector<float> attr_float;
	vector<float4> attr_float3;

	for(size_t i = 0; i < scene->meshes.size(); i++) {
		Mesh *mesh = scene->meshes[i];
		AttributeRequestSet& attributes = mesh_attributes[i];

		/* todo: we now store std and name attributes from requests even if
		   they actually refer to the same mesh attributes, optimize */
		foreach(AttributeRequest& req, attributes.requests) {
			Attribute *mattr = mesh->attributes.find(req);

			/* todo: get rid of this exception */
			if(!mattr && req.std == ATTR_STD_GENERATED) {
				mattr = mesh->attributes.add(ATTR_STD_GENERATED);
				if(mesh->verts.size())
					memcpy(mattr->data_float3(), &mesh->verts[0], sizeof(float3)*mesh->verts.size());
			}

			/* attribute not found */
			if(!mattr) {
				req.element = ATTR_ELEMENT_NONE;
				req.offset = 0;
				continue;
			}

			/* we abuse AttributeRequest to pass on info like element and
			   offset, it doesn't really make sense but is convenient */

			/* store element and type */
			if(mattr->element == Attribute::VERTEX)
				req.element = ATTR_ELEMENT_VERTEX;
			else if(mattr->element == Attribute::FACE)
				req.element = ATTR_ELEMENT_FACE;
			else if(mattr->element == Attribute::CORNER)
				req.element = ATTR_ELEMENT_CORNER;

			req.type = mattr->type;

			/* store attribute data in arrays */
			size_t size = mattr->element_size(mesh->verts.size(), mesh->triangles.size());

			if(mattr->type == TypeDesc::TypeFloat) {
				float *data = mattr->data_float();
				req.offset = attr_float.size();

				attr_float.resize(attr_float.size() + size);

				for(size_t k = 0; k < size; k++)
					attr_float[req.offset+k] = data[k];
			}
			else {
				float3 *data = mattr->data_float3();
				req.offset = attr_float3.size();

				attr_float3.resize(attr_float3.size() + size);

				for(size_t k = 0; k < size; k++)
					attr_float3[req.offset+k] = float3_to_float4(data[k]);
			}

			/* mesh vertex/triangle index is global, not per object, so we sneak
			   a correction for that in here */
			if(req.element == ATTR_ELEMENT_VERTEX)
				req.offset -= mesh->vert_offset;
			else if(mattr->element == Attribute::FACE)
				req.offset -= mesh->tri_offset;
			else if(mattr->element == Attribute::CORNER)
				req.offset -= 3*mesh->tri_offset;

			if(progress.get_cancel()) return;
		}
	}

	/* create attribute lookup maps */
	if(scene->params.shadingsystem == SceneParams::OSL)
		update_osl_attributes(device, scene, mesh_attributes);
	else
		update_svm_attributes(device, dscene, scene, mesh_attributes);

	if(progress.get_cancel()) return;

	/* copy to device */
	progress.set_status("Updating Mesh", "Copying Attributes to device");

	if(attr_float.size()) {
		dscene->attributes_float.copy(&attr_float[0], attr_float.size());
		device->tex_alloc("__attributes_float", dscene->attributes_float);
	}
	if(attr_float3.size()) {
		dscene->attributes_float3.copy(&attr_float3[0], attr_float3.size());
		device->tex_alloc("__attributes_float3", dscene->attributes_float3);
	}
}

void MeshManager::device_update_mesh(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	/* count and update offsets */
	size_t vert_size = 0;
	size_t tri_size = 0;

	foreach(Mesh *mesh, scene->meshes) {
		mesh->vert_offset = vert_size;
		mesh->tri_offset = tri_size;

		vert_size += mesh->verts.size();
		tri_size += mesh->triangles.size();
	}

	if(tri_size == 0)
		return;

	/* normals */
	progress.set_status("Updating Mesh", "Computing normals");

	float4 *normal = dscene->tri_normal.resize(tri_size);
	float4 *vnormal = dscene->tri_vnormal.resize(vert_size);
	float4 *tri_verts = dscene->tri_verts.resize(vert_size);
	float4 *tri_vindex = dscene->tri_vindex.resize(tri_size);

	foreach(Mesh *mesh, scene->meshes) {
		mesh->pack_normals(scene, &normal[mesh->tri_offset], &vnormal[mesh->vert_offset]);
		mesh->pack_verts(&tri_verts[mesh->vert_offset], &tri_vindex[mesh->tri_offset], mesh->vert_offset);

		if(progress.get_cancel()) return;
	}

	/* vertex coordinates */
	progress.set_status("Updating Mesh", "Copying Mesh to device");

	device->tex_alloc("__tri_normal", dscene->tri_normal);
	device->tex_alloc("__tri_vnormal", dscene->tri_vnormal);
	device->tex_alloc("__tri_verts", dscene->tri_verts);
	device->tex_alloc("__tri_vindex", dscene->tri_vindex);
}

void MeshManager::device_update_bvh(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	/* bvh build */
	progress.set_status("Updating Scene BVH", "Building");

	BVHParams bparams;
	bparams.top_level = true;
	bparams.use_qbvh = scene->params.use_qbvh;
	bparams.use_spatial_split = scene->params.use_bvh_spatial_split;
	bparams.use_cache = scene->params.use_bvh_cache;

	delete bvh;
	bvh = BVH::create(bparams, scene->objects);
	bvh->build(progress);

	if(progress.get_cancel()) return;

	/* copy to device */
	progress.set_status("Updating Scene BVH", "Copying BVH to device");

	PackedBVH& pack = bvh->pack;

	if(pack.nodes.size()) {
		dscene->bvh_nodes.reference((float4*)&pack.nodes[0], pack.nodes.size());
		device->tex_alloc("__bvh_nodes", dscene->bvh_nodes);
	}
	if(pack.object_node.size()) {
		dscene->object_node.reference((uint*)&pack.object_node[0], pack.object_node.size());
		device->tex_alloc("__object_node", dscene->object_node);
	}
	if(pack.tri_woop.size()) {
		dscene->tri_woop.reference(&pack.tri_woop[0], pack.tri_woop.size());
		device->tex_alloc("__tri_woop", dscene->tri_woop);
	}
	if(pack.prim_visibility.size()) {
		dscene->prim_visibility.reference((uint*)&pack.prim_visibility[0], pack.prim_visibility.size());
		device->tex_alloc("__prim_visibility", dscene->prim_visibility);
	}
	if(pack.prim_index.size()) {
		dscene->prim_index.reference((uint*)&pack.prim_index[0], pack.prim_index.size());
		device->tex_alloc("__prim_index", dscene->prim_index);
	}
	if(pack.prim_object.size()) {
		dscene->prim_object.reference((uint*)&pack.prim_object[0], pack.prim_object.size());
		device->tex_alloc("__prim_object", dscene->prim_object);
	}

	dscene->data.bvh.root = pack.root_index;
}

void MeshManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;

	/* update normals */
	foreach(Mesh *mesh, scene->meshes) {
		foreach(uint shader, mesh->used_shaders)
			if(scene->shaders[shader]->need_update_attributes)
				mesh->need_update = true;

		if(mesh->need_update) {
			mesh->add_face_normals();
			mesh->add_vertex_normals();

			if(progress.get_cancel()) return;
		}
	}

	/* device update */
	device_free(device, dscene);

	device_update_mesh(device, dscene, scene, progress);
	if(progress.get_cancel()) return;

	device_update_attributes(device, dscene, scene, progress);
	if(progress.get_cancel()) return;

	/* update displacement */
	bool displacement_done = false;

	foreach(Mesh *mesh, scene->meshes)
		if(mesh->need_update && displace(device, scene, mesh, progress))
			displacement_done = true;

	/* todo: properly handle cancel halfway displacement */
	if(progress.get_cancel()) return;

	/* device re-update after displacement */
	if(displacement_done) {
		device_free(device, dscene);

		device_update_mesh(device, dscene, scene, progress);
		if(progress.get_cancel()) return;

		device_update_attributes(device, dscene, scene, progress);
		if(progress.get_cancel()) return;
	}

	/* update bvh */
	size_t i = 0, num_bvh = 0;

	foreach(Mesh *mesh, scene->meshes)
		if(mesh->need_update && !mesh->transform_applied)
			num_bvh++;

	TaskPool pool;

	foreach(Mesh *mesh, scene->meshes) {
		if(mesh->need_update) {
			pool.push(function_bind(&Mesh::compute_bvh, mesh, &scene->params, &progress, i, num_bvh));
			i++;
		}
	}

	pool.wait_work();
	
	foreach(Shader *shader, scene->shaders)
		shader->need_update_attributes = false;

	bool motion_blur = scene->need_motion() == Scene::MOTION_BLUR;

	foreach(Object *object, scene->objects)
		object->compute_bounds(motion_blur);

	if(progress.get_cancel()) return;

	device_update_bvh(device, dscene, scene, progress);

	need_update = false;
}

void MeshManager::device_free(Device *device, DeviceScene *dscene)
{
	device->tex_free(dscene->bvh_nodes);
	device->tex_free(dscene->object_node);
	device->tex_free(dscene->tri_woop);
	device->tex_free(dscene->prim_visibility);
	device->tex_free(dscene->prim_index);
	device->tex_free(dscene->prim_object);
	device->tex_free(dscene->tri_normal);
	device->tex_free(dscene->tri_vnormal);
	device->tex_free(dscene->tri_vindex);
	device->tex_free(dscene->tri_verts);
	device->tex_free(dscene->attributes_map);
	device->tex_free(dscene->attributes_float);
	device->tex_free(dscene->attributes_float3);

	dscene->bvh_nodes.clear();
	dscene->object_node.clear();
	dscene->tri_woop.clear();
	dscene->prim_visibility.clear();
	dscene->prim_index.clear();
	dscene->prim_object.clear();
	dscene->tri_normal.clear();
	dscene->tri_vnormal.clear();
	dscene->tri_vindex.clear();
	dscene->tri_verts.clear();
	dscene->attributes_map.clear();
	dscene->attributes_float.clear();
	dscene->attributes_float3.clear();
}

void MeshManager::tag_update(Scene *scene)
{
	need_update = true;
	scene->object_manager->need_update = true;
}

bool Mesh::need_attribute(Scene *scene, AttributeStandard std)
{
	if(std == ATTR_STD_NONE)
		return false;
	
	if(scene->need_global_attribute(std))
		return true;

	foreach(uint shader, used_shaders)
		if(scene->shaders[shader]->attributes.find(std))
			return true;
	
	return false;
}

bool Mesh::need_attribute(Scene *scene, ustring name)
{
	if(name == ustring())
		return false;

	foreach(uint shader, used_shaders)
		if(scene->shaders[shader]->attributes.find(name))
			return true;
	
	return false;
}

CCL_NAMESPACE_END

