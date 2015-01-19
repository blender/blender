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
 * limitations under the License.
 */

#include "bvh.h"
#include "bvh_build.h"

#include "camera.h"
#include "curves.h"
#include "device.h"
#include "shader.h"
#include "light.h"
#include "mesh.h"
#include "object.h"
#include "scene.h"

#include "osl_globals.h"

#include "util_cache.h"
#include "util_foreach.h"
#include "util_logging.h"
#include "util_progress.h"
#include "util_set.h"

CCL_NAMESPACE_BEGIN

/* Triangle */

void Mesh::Triangle::bounds_grow(const float3 *verts, BoundBox& bounds) const
{
	bounds.grow(verts[v[0]]);
	bounds.grow(verts[v[1]]);
	bounds.grow(verts[v[2]]);
}

/* Curve */

void Mesh::Curve::bounds_grow(const int k, const float4 *curve_keys, BoundBox& bounds) const
{
	float3 P[4];

	P[0] = float4_to_float3(curve_keys[max(first_key + k - 1,first_key)]);
	P[1] = float4_to_float3(curve_keys[first_key + k]);
	P[2] = float4_to_float3(curve_keys[first_key + k + 1]);
	P[3] = float4_to_float3(curve_keys[min(first_key + k + 2, first_key + num_keys - 1)]);

	float3 lower;
	float3 upper;

	curvebounds(&lower.x, &upper.x, P, 0);
	curvebounds(&lower.y, &upper.y, P, 1);
	curvebounds(&lower.z, &upper.z, P, 2);

	float mr = max(curve_keys[first_key + k].w, curve_keys[first_key + k + 1].w);

	bounds.grow(lower, mr);
	bounds.grow(upper, mr);
}

/* Mesh */

Mesh::Mesh()
{
	need_update = true;
	need_update_rebuild = false;
	transform_applied = false;
	transform_negative_scaled = false;
	transform_normal = transform_identity();
	displacement_method = DISPLACE_BUMP;
	bounds = BoundBox::empty;

	motion_steps = 3;
	use_motion_blur = false;

	bvh = NULL;

	tri_offset = 0;
	vert_offset = 0;

	curve_offset = 0;
	curvekey_offset = 0;

	attributes.triangle_mesh = this;
	curve_attributes.curve_mesh = this;

	has_volume = false;
}

Mesh::~Mesh()
{
	delete bvh;
}

void Mesh::reserve(int numverts, int numtris, int numcurves, int numcurvekeys)
{
	/* reserve space to add verts and triangles later */
	verts.resize(numverts);
	triangles.resize(numtris);
	shader.resize(numtris);
	smooth.resize(numtris);
	curve_keys.resize(numcurvekeys);
	curves.resize(numcurves);

	attributes.reserve();
	curve_attributes.reserve();
}

void Mesh::clear()
{
	/* clear all verts and triangles */
	verts.clear();
	triangles.clear();
	shader.clear();
	smooth.clear();

	curve_keys.clear();
	curves.clear();

	attributes.clear();
	curve_attributes.clear();
	used_shaders.clear();

	transform_applied = false;
	transform_negative_scaled = false;
	transform_normal = transform_identity();
	geometry_synced = false;
}

int Mesh::split_vertex(int vertex)
{
	/* copy vertex location and vertex attributes */
	verts.push_back(verts[vertex]);

	foreach(Attribute& attr, attributes.attributes) {
		if(attr.element == ATTR_ELEMENT_VERTEX) {
			vector<char> tmp(attr.data_sizeof());
			memcpy(&tmp[0], attr.data() + tmp.size()*vertex, tmp.size());
			attr.add(&tmp[0]);
		}
	}

	return verts.size() - 1;
}

void Mesh::set_triangle(int i, int v0, int v1, int v2, int shader_, bool smooth_)
{
	Triangle tri;
	tri.v[0] = v0;
	tri.v[1] = v1;
	tri.v[2] = v2;

	triangles[i] = tri;
	shader[i] = shader_;
	smooth[i] = smooth_;
}

void Mesh::add_triangle(int v0, int v1, int v2, int shader_, bool smooth_)
{
	Triangle tri;
	tri.v[0] = v0;
	tri.v[1] = v1;
	tri.v[2] = v2;

	triangles.push_back(tri);
	shader.push_back(shader_);
	smooth.push_back(smooth_);
}

void Mesh::add_curve_key(float3 co, float radius)
{
	float4 key = float3_to_float4(co);
	key.w = radius;

	curve_keys.push_back(key);
}

void Mesh::add_curve(int first_key, int num_keys, int shader)
{
	Curve curve;
	curve.first_key = first_key;
	curve.num_keys = num_keys;
	curve.shader = shader;

	curves.push_back(curve);
}

void Mesh::compute_bounds()
{
	BoundBox bnds = BoundBox::empty;
	size_t verts_size = verts.size();
	size_t curve_keys_size = curve_keys.size();

	if(verts_size + curve_keys_size > 0) {
		for(size_t i = 0; i < verts_size; i++)
			bnds.grow(verts[i]);

		for(size_t i = 0; i < curve_keys_size; i++)
			bnds.grow(float4_to_float3(curve_keys[i]), curve_keys[i].w);

		Attribute *attr = attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
		if (use_motion_blur && attr) {
			size_t steps_size = verts.size() * (motion_steps - 1);
			float3 *vert_steps = attr->data_float3();
	
			for (size_t i = 0; i < steps_size; i++)
				bnds.grow(vert_steps[i]);
		}

		Attribute *curve_attr = curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
		if(use_motion_blur && curve_attr) {
			size_t steps_size = curve_keys.size() * (motion_steps - 1);
			float3 *key_steps = curve_attr->data_float3();
	
			for (size_t i = 0; i < steps_size; i++)
				bnds.grow(key_steps[i]);
		}

		if(!bnds.valid()) {
			bnds = BoundBox::empty;

			/* skip nan or inf coordinates */
			for(size_t i = 0; i < verts_size; i++)
				bnds.grow_safe(verts[i]);

			for(size_t i = 0; i < curve_keys_size; i++)
				bnds.grow_safe(float4_to_float3(curve_keys[i]), curve_keys[i].w);
			
			if (use_motion_blur && attr) {
				size_t steps_size = verts.size() * (motion_steps - 1);
				float3 *vert_steps = attr->data_float3();
		
				for (size_t i = 0; i < steps_size; i++)
					bnds.grow_safe(vert_steps[i]);
			}

			if (use_motion_blur && curve_attr) {
				size_t steps_size = curve_keys.size() * (motion_steps - 1);
				float3 *key_steps = curve_attr->data_float3();
		
				for (size_t i = 0; i < steps_size; i++)
					bnds.grow_safe(key_steps[i]);
			}
		}
	}

	if(!bnds.valid()) {
		/* empty mesh */
		bnds.grow(make_float3(0.0f, 0.0f, 0.0f));
	}

	bounds = bnds;
}

static float3 compute_face_normal(const Mesh::Triangle& t, float3 *verts)
{
	float3 v0 = verts[t.v[0]];
	float3 v1 = verts[t.v[1]];
	float3 v2 = verts[t.v[2]];

	float3 norm = cross(v1 - v0, v2 - v0);
	float normlen = len(norm);

	if(normlen == 0.0f)
		return make_float3(0.0f, 0.0f, 0.0f);

	return norm / normlen;
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
			fN[i] = compute_face_normal(triangles_ptr[i], verts_ptr);

			if(flip)
				fN[i] = -fN[i];
		}
	}

	/* expected to be in local space */
	if(transform_applied) {
		Transform ntfm = transform_inverse(transform_normal);

		for(size_t i = 0; i < triangles_size; i++)
			fN[i] = normalize(transform_direction(&ntfm, fN[i]));
	}
}

void Mesh::add_vertex_normals()
{
	bool flip = transform_negative_scaled;
	size_t verts_size = verts.size();
	size_t triangles_size = triangles.size();

	/* static vertex normals */
	if(!attributes.find(ATTR_STD_VERTEX_NORMAL)) {
		/* get attributes */
		Attribute *attr_fN = attributes.find(ATTR_STD_FACE_NORMAL);
		Attribute *attr_vN = attributes.add(ATTR_STD_VERTEX_NORMAL);

		float3 *fN = attr_fN->data_float3();
		float3 *vN = attr_vN->data_float3();

		/* compute vertex normals */
		memset(vN, 0, verts.size()*sizeof(float3));

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

	/* motion vertex normals */
	Attribute *attr_mP = attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
	Attribute *attr_mN = attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);

	if(has_motion_blur() && attr_mP && !attr_mN) {
		/* create attribute */
		attr_mN = attributes.add(ATTR_STD_MOTION_VERTEX_NORMAL);

		for(int step = 0; step < motion_steps - 1; step++) {
			float3 *mP = attr_mP->data_float3() + step*verts.size();
			float3 *mN = attr_mN->data_float3() + step*verts.size();

			/* compute */
			memset(mN, 0, verts.size()*sizeof(float3));

			if(triangles_size) {
				Triangle *triangles_ptr = &triangles[0];

				for(size_t i = 0; i < triangles_size; i++) {
					for(size_t j = 0; j < 3; j++) {
						float3 fN = compute_face_normal(triangles_ptr[i], mP);
						mN[triangles_ptr[i].v[j]] += fN;
					}
				}
			}

			for(size_t i = 0; i < verts_size; i++) {
				mN[i] = normalize(mN[i]);
				if(flip)
					mN[i] = -mN[i];
			}
		}
	}
}

void Mesh::pack_normals(Scene *scene, uint *tri_shader, float4 *vnormal)
{
	Attribute *attr_vN = attributes.find(ATTR_STD_VERTEX_NORMAL);

	float3 *vN = attr_vN->data_float3();
	uint shader_id = 0;
	uint last_shader = -1;
	bool last_smooth = false;

	size_t triangles_size = triangles.size();
	uint *shader_ptr = (shader.size())? &shader[0]: NULL;

	bool do_transform = transform_applied;
	Transform ntfm = transform_normal;

	/* save shader */
	for(size_t i = 0; i < triangles_size; i++) {
		if(shader_ptr[i] != last_shader || last_smooth != smooth[i]) {
			last_shader = shader_ptr[i];
			last_smooth = smooth[i];
			shader_id = scene->shader_manager->get_shader_id(last_shader, this, last_smooth);
		}

		tri_shader[i] = shader_id;
	}

	size_t verts_size = verts.size();

	for(size_t i = 0; i < verts_size; i++) {
		float3 vNi = vN[i];

		if(do_transform)
			vNi = normalize(transform_direction(&ntfm, vNi));

		vnormal[i] = make_float4(vNi.x, vNi.y, vNi.z, 0.0f);
	}
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

void Mesh::pack_curves(Scene *scene, float4 *curve_key_co, float4 *curve_data, size_t curvekey_offset)
{
	size_t curve_keys_size = curve_keys.size();
	float4 *keys_ptr = NULL;

	/* pack curve keys */
	if(curve_keys_size) {
		keys_ptr = &curve_keys[0];

		for(size_t i = 0; i < curve_keys_size; i++)
			curve_key_co[i] = keys_ptr[i];
	}

	/* pack curve segments */
	size_t curve_num = curves.size();

	if(curve_num) {
		Curve *curve_ptr = &curves[0];
		int shader_id = 0;
		
		for(size_t i = 0; i < curve_num; i++) {
			Curve curve = curve_ptr[i];
			shader_id = scene->shader_manager->get_shader_id(curve.shader, this, false);

			curve_data[i] = make_float4(
				__int_as_float(curve.first_key + curvekey_offset),
				__int_as_float(curve.num_keys),
				__int_as_float(shader_id),
				0.0f);
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

bool Mesh::has_motion_blur() const
{
	return (use_motion_blur &&
	        (attributes.find(ATTR_STD_MOTION_VERTEX_POSITION) ||
	         curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION)));
}

/* Mesh Manager */

MeshManager::MeshManager()
{
	bvh = NULL;
	need_update = true;
	need_flags_update = true;
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
	og->object_names.clear();

	og->attribute_map.resize(scene->objects.size()*ATTR_PRIM_TYPES);

	for(size_t i = 0; i < scene->objects.size(); i++) {
		/* set object name to object index map */
		Object *object = scene->objects[i];
		og->object_name_map[object->name] = i;
		og->object_names.push_back(object->name);

		/* set object attributes */
		foreach(ParamValue& attr, object->attributes) {
			OSLGlobals::Attribute osl_attr;

			osl_attr.type = attr.type();
			osl_attr.elem = ATTR_ELEMENT_OBJECT;
			osl_attr.value = attr;
			osl_attr.offset = 0;

			og->attribute_map[i*ATTR_PRIM_TYPES][attr.name()] = osl_attr;
			og->attribute_map[i*ATTR_PRIM_TYPES + ATTR_PRIM_CURVE][attr.name()] = osl_attr;
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

			if(req.triangle_element != ATTR_ELEMENT_NONE) {
				osl_attr.elem = req.triangle_element;
				osl_attr.offset = req.triangle_offset;

				if(req.triangle_type == TypeDesc::TypeFloat)
					osl_attr.type = TypeDesc::TypeFloat;
				else if(req.triangle_type == TypeDesc::TypeMatrix)
					osl_attr.type = TypeDesc::TypeMatrix;
				else
					osl_attr.type = TypeDesc::TypeColor;

				if(req.std != ATTR_STD_NONE) {
					/* if standard attribute, add lookup by geom: name convention */
					ustring stdname(string("geom:") + string(Attribute::standard_name(req.std)));
					og->attribute_map[i*ATTR_PRIM_TYPES][stdname] = osl_attr;
				}
				else if(req.name != ustring()) {
					/* add lookup by mesh attribute name */
					og->attribute_map[i*ATTR_PRIM_TYPES][req.name] = osl_attr;
				}
			}

			if(req.curve_element != ATTR_ELEMENT_NONE) {
				osl_attr.elem = req.curve_element;
				osl_attr.offset = req.curve_offset;

				if(req.curve_type == TypeDesc::TypeFloat)
					osl_attr.type = TypeDesc::TypeFloat;
				else if(req.curve_type == TypeDesc::TypeMatrix)
					osl_attr.type = TypeDesc::TypeMatrix;
				else
					osl_attr.type = TypeDesc::TypeColor;

				if(req.std != ATTR_STD_NONE) {
					/* if standard attribute, add lookup by geom: name convention */
					ustring stdname(string("geom:") + string(Attribute::standard_name(req.std)));
					og->attribute_map[i*ATTR_PRIM_TYPES + ATTR_PRIM_CURVE][stdname] = osl_attr;
				}
				else if(req.name != ustring()) {
					/* add lookup by mesh attribute name */
					og->attribute_map[i*ATTR_PRIM_TYPES + ATTR_PRIM_CURVE][req.name] = osl_attr;
				}
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
		attr_map_stride = max(attr_map_stride, (mesh_attributes[i].size() + 1)*ATTR_PRIM_TYPES);

	if(attr_map_stride == 0)
		return;
	
	/* create attribute map */
	uint4 *attr_map = dscene->attributes_map.resize(attr_map_stride*scene->objects.size());
	memset(attr_map, 0, dscene->attributes_map.size()*sizeof(uint));

	for(size_t i = 0; i < scene->objects.size(); i++) {
		Object *object = scene->objects[i];
		Mesh *mesh = object->mesh;

		/* find mesh attributes */
		size_t j;

		for(j = 0; j < scene->meshes.size(); j++)
			if(scene->meshes[j] == mesh)
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

			if(mesh->triangles.size()) {
				attr_map[index].x = id;
				attr_map[index].y = req.triangle_element;
				attr_map[index].z = as_uint(req.triangle_offset);

				if(req.triangle_type == TypeDesc::TypeFloat)
					attr_map[index].w = NODE_ATTR_FLOAT;
				else if(req.triangle_type == TypeDesc::TypeMatrix)
					attr_map[index].w = NODE_ATTR_MATRIX;
				else
					attr_map[index].w = NODE_ATTR_FLOAT3;
			}

			index++;

			if(mesh->curves.size()) {
				attr_map[index].x = id;
				attr_map[index].y = req.curve_element;
				attr_map[index].z = as_uint(req.curve_offset);

				if(req.curve_type == TypeDesc::TypeFloat)
					attr_map[index].w = NODE_ATTR_FLOAT;
				else if(req.curve_type == TypeDesc::TypeMatrix)
					attr_map[index].w = NODE_ATTR_MATRIX;
				else
					attr_map[index].w = NODE_ATTR_FLOAT3;
			}

			index++;
		}

		/* terminator */
		attr_map[index].x = ATTR_STD_NONE;
		attr_map[index].y = 0;
		attr_map[index].z = 0;
		attr_map[index].w = 0;

		index++;

		attr_map[index].x = ATTR_STD_NONE;
		attr_map[index].y = 0;
		attr_map[index].z = 0;
		attr_map[index].w = 0;

		index++;
	}

	/* copy to device */
	dscene->data.bvh.attributes_map_stride = attr_map_stride;
	device->tex_alloc("__attributes_map", dscene->attributes_map);
}

static void update_attribute_element_offset(Mesh *mesh, vector<float>& attr_float, vector<float4>& attr_float3, vector<uchar4>& attr_uchar4,
	Attribute *mattr, TypeDesc& type, int& offset, AttributeElement& element)
{
	if(mattr) {
		/* store element and type */
		element = mattr->element;
		type = mattr->type;

		/* store attribute data in arrays */
		size_t size = mattr->element_size(
			mesh->verts.size(),
			mesh->triangles.size(),
			mesh->motion_steps,
			mesh->curves.size(),
			mesh->curve_keys.size());

		if(mattr->element == ATTR_ELEMENT_VOXEL) {
			/* store slot in offset value */
			VoxelAttribute *voxel_data = mattr->data_voxel();
			offset = voxel_data->slot;
		}
		else if(mattr->element == ATTR_ELEMENT_CORNER_BYTE) {
			uchar4 *data = mattr->data_uchar4();
			offset = attr_uchar4.size();

			attr_uchar4.resize(attr_uchar4.size() + size);

			for(size_t k = 0; k < size; k++)
				attr_uchar4[offset+k] = data[k];
		}
		else if(mattr->type == TypeDesc::TypeFloat) {
			float *data = mattr->data_float();
			offset = attr_float.size();

			attr_float.resize(attr_float.size() + size);

			for(size_t k = 0; k < size; k++)
				attr_float[offset+k] = data[k];
		}
		else if(mattr->type == TypeDesc::TypeMatrix) {
			Transform *tfm = mattr->data_transform();
			offset = attr_float3.size();

			attr_float3.resize(attr_float3.size() + size*4);

			for(size_t k = 0; k < size*4; k++)
				attr_float3[offset+k] = (&tfm->x)[k];
		}
		else {
			float4 *data = mattr->data_float4();
			offset = attr_float3.size();

			attr_float3.resize(attr_float3.size() + size);

			for(size_t k = 0; k < size; k++)
				attr_float3[offset+k] = data[k];
		}

		/* mesh vertex/curve index is global, not per object, so we sneak
		 * a correction for that in here */
		if(element == ATTR_ELEMENT_VERTEX)
			offset -= mesh->vert_offset;
		else if(element == ATTR_ELEMENT_VERTEX_MOTION)
			offset -= mesh->vert_offset;
		else if(element == ATTR_ELEMENT_FACE)
			offset -= mesh->tri_offset;
		else if(element == ATTR_ELEMENT_CORNER || element == ATTR_ELEMENT_CORNER_BYTE)
			offset -= 3*mesh->tri_offset;
		else if(element == ATTR_ELEMENT_CURVE)
			offset -= mesh->curve_offset;
		else if(element == ATTR_ELEMENT_CURVE_KEY)
			offset -= mesh->curvekey_offset;
		else if(element == ATTR_ELEMENT_CURVE_KEY_MOTION)
			offset -= mesh->curvekey_offset;
	}
	else {
		/* attribute not found */
		element = ATTR_ELEMENT_NONE;
		offset = 0;
	}
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
	vector<uchar4> attr_uchar4;

	for(size_t i = 0; i < scene->meshes.size(); i++) {
		Mesh *mesh = scene->meshes[i];
		AttributeRequestSet& attributes = mesh_attributes[i];

		/* todo: we now store std and name attributes from requests even if
		 * they actually refer to the same mesh attributes, optimize */
		foreach(AttributeRequest& req, attributes.requests) {
			Attribute *triangle_mattr = mesh->attributes.find(req);
			Attribute *curve_mattr = mesh->curve_attributes.find(req);

			/* todo: get rid of this exception, it's only here for giving some
			 * working texture coordinate for subdivision as we can't preserve
			 * any attributes yet */
			if(!triangle_mattr && req.std == ATTR_STD_GENERATED) {
				triangle_mattr = mesh->attributes.add(ATTR_STD_GENERATED);
				if(mesh->verts.size())
					memcpy(triangle_mattr->data_float3(), &mesh->verts[0], sizeof(float3)*mesh->verts.size());
			}

			update_attribute_element_offset(mesh, attr_float, attr_float3, attr_uchar4, triangle_mattr,
				req.triangle_type, req.triangle_offset, req.triangle_element);

			update_attribute_element_offset(mesh, attr_float, attr_float3, attr_uchar4, curve_mattr,
				req.curve_type, req.curve_offset, req.curve_element);
	
			if(progress.get_cancel()) return;
		}
	}

	/* create attribute lookup maps */
	if(scene->shader_manager->use_osl())
		update_osl_attributes(device, scene, mesh_attributes);

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
	if(attr_uchar4.size()) {
		dscene->attributes_uchar4.copy(&attr_uchar4[0], attr_uchar4.size());
		device->tex_alloc("__attributes_uchar4", dscene->attributes_uchar4);
	}
}

void MeshManager::device_update_mesh(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	/* count and update offsets */
	size_t vert_size = 0;
	size_t tri_size = 0;

	size_t curve_key_size = 0;
	size_t curve_size = 0;

	foreach(Mesh *mesh, scene->meshes) {
		mesh->vert_offset = vert_size;
		mesh->tri_offset = tri_size;

		mesh->curvekey_offset = curve_key_size;
		mesh->curve_offset = curve_size;

		vert_size += mesh->verts.size();
		tri_size += mesh->triangles.size();

		curve_key_size += mesh->curve_keys.size();
		curve_size += mesh->curves.size();
	}

	if(tri_size != 0) {
		/* normals */
		progress.set_status("Updating Mesh", "Computing normals");

		uint *tri_shader = dscene->tri_shader.resize(tri_size);
		float4 *vnormal = dscene->tri_vnormal.resize(vert_size);
		float4 *tri_verts = dscene->tri_verts.resize(vert_size);
		float4 *tri_vindex = dscene->tri_vindex.resize(tri_size);

		foreach(Mesh *mesh, scene->meshes) {
			mesh->pack_normals(scene, &tri_shader[mesh->tri_offset], &vnormal[mesh->vert_offset]);
			mesh->pack_verts(&tri_verts[mesh->vert_offset], &tri_vindex[mesh->tri_offset], mesh->vert_offset);

			if(progress.get_cancel()) return;
		}

		/* vertex coordinates */
		progress.set_status("Updating Mesh", "Copying Mesh to device");

		device->tex_alloc("__tri_shader", dscene->tri_shader);
		device->tex_alloc("__tri_vnormal", dscene->tri_vnormal);
		device->tex_alloc("__tri_verts", dscene->tri_verts);
		device->tex_alloc("__tri_vindex", dscene->tri_vindex);
	}

	if(curve_size != 0) {
		progress.set_status("Updating Mesh", "Copying Strands to device");

		float4 *curve_keys = dscene->curve_keys.resize(curve_key_size);
		float4 *curves = dscene->curves.resize(curve_size);

		foreach(Mesh *mesh, scene->meshes) {
			mesh->pack_curves(scene, &curve_keys[mesh->curvekey_offset], &curves[mesh->curve_offset], mesh->curvekey_offset);
			if(progress.get_cancel()) return;
		}

		device->tex_alloc("__curve_keys", dscene->curve_keys);
		device->tex_alloc("__curves", dscene->curves);
	}
}

void MeshManager::device_update_bvh(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	/* bvh build */
	progress.set_status("Updating Scene BVH", "Building");

	VLOG(1) << (scene->params.use_qbvh ? "Using QBVH optimization structure"
	                                   : "Using regular BVH optimization structure");

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
	if(pack.prim_type.size()) {
		dscene->prim_type.reference((uint*)&pack.prim_type[0], pack.prim_type.size());
		device->tex_alloc("__prim_type", dscene->prim_type);
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
	dscene->data.bvh.use_qbvh = scene->params.use_qbvh;
}

void MeshManager::device_update_flags(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update && !need_flags_update) {
		return;
	}
	/* update flags */
	foreach(Mesh *mesh, scene->meshes) {
		mesh->has_volume = false;
		foreach(uint shader, mesh->used_shaders) {
			if(scene->shaders[shader]->has_volume) {
				mesh->has_volume = true;
			}
		}
	}
	need_flags_update = false;
}

void MeshManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;

	/* update normals */
	foreach(Mesh *mesh, scene->meshes) {
		foreach(uint shader, mesh->used_shaders) {
			if(scene->shaders[shader]->need_update_attributes)
				mesh->need_update = true;
		}

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
		if(mesh->need_update && displace(device, dscene, scene, mesh, progress))
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
			pool.push(function_bind(&Mesh::compute_bvh,
			                        mesh,
			                        &scene->params,
			                        &progress,
			                        i,
			                        num_bvh));
			i++;
		}
	}

	pool.wait_work();
	
	foreach(Shader *shader, scene->shaders)
		shader->need_update_attributes = false;

#ifdef __OBJECT_MOTION__
	Scene::MotionType need_motion = scene->need_motion(device->info.advanced_shading);
	bool motion_blur = need_motion == Scene::MOTION_BLUR;
#else
	bool motion_blur = false;
#endif

	/* update obejcts */
	vector<Object *> volume_objects;
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
	device->tex_free(dscene->prim_type);
	device->tex_free(dscene->prim_visibility);
	device->tex_free(dscene->prim_index);
	device->tex_free(dscene->prim_object);
	device->tex_free(dscene->tri_shader);
	device->tex_free(dscene->tri_vnormal);
	device->tex_free(dscene->tri_vindex);
	device->tex_free(dscene->tri_verts);
	device->tex_free(dscene->curves);
	device->tex_free(dscene->curve_keys);
	device->tex_free(dscene->attributes_map);
	device->tex_free(dscene->attributes_float);
	device->tex_free(dscene->attributes_float3);
	device->tex_free(dscene->attributes_uchar4);

	dscene->bvh_nodes.clear();
	dscene->object_node.clear();
	dscene->tri_woop.clear();
	dscene->prim_type.clear();
	dscene->prim_visibility.clear();
	dscene->prim_index.clear();
	dscene->prim_object.clear();
	dscene->tri_shader.clear();
	dscene->tri_vnormal.clear();
	dscene->tri_vindex.clear();
	dscene->tri_verts.clear();
	dscene->curves.clear();
	dscene->curve_keys.clear();
	dscene->attributes_map.clear();
	dscene->attributes_float.clear();
	dscene->attributes_float3.clear();
	dscene->attributes_uchar4.clear();

#ifdef WITH_OSL
	OSLGlobals *og = (OSLGlobals*)device->osl_memory();

	if(og) {
		og->object_name_map.clear();
		og->attribute_map.clear();
		og->object_names.clear();
	}
#endif
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

