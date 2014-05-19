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

#include "device.h"
#include "light.h"
#include "mesh.h"
#include "curves.h"
#include "object.h"
#include "particles.h"
#include "scene.h"

#include "util_foreach.h"
#include "util_map.h"
#include "util_progress.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

/* Object */

Object::Object()
{
	name = "";
	mesh = NULL;
	tfm = transform_identity();
	visibility = ~0;
	random_id = 0;
	pass_id = 0;
	particle_system = NULL;
	particle_index = 0;
	bounds = BoundBox::empty;
	motion.pre = transform_identity();
	motion.mid = transform_identity();
	motion.post = transform_identity();
	use_motion = false;
	use_holdout = false;
	dupli_generated = make_float3(0.0f, 0.0f, 0.0f);
	dupli_uv = make_float2(0.0f, 0.0f);
}

Object::~Object()
{
}

void Object::compute_bounds(bool motion_blur)
{
	BoundBox mbounds = mesh->bounds;

	if(motion_blur && use_motion) {
		DecompMotionTransform decomp;
		transform_motion_decompose(&decomp, &motion, &tfm);

		bounds = BoundBox::empty;

		/* todo: this is really terrible. according to pbrt there is a better
		 * way to find this iteratively, but did not find implementation yet
		 * or try to implement myself */
		for(float t = 0.0f; t < 1.0f; t += (1.0f/128.0f)) {
			Transform ttfm;

			transform_motion_interpolate(&ttfm, &decomp, t);
			bounds.grow(mbounds.transformed(&ttfm));
		}
	}
	else
		bounds = mbounds.transformed(&tfm);
}

void Object::apply_transform(bool apply_to_motion)
{
	if(!mesh || tfm == transform_identity())
		return;
	
	/* triangles */
	if(mesh->verts.size()) {
		/* store matrix to transform later. when accessing these as attributes we
		 * do not want the transform to be applied for consistency between static
		 * and dynamic BVH, so we do it on packing. */
		mesh->transform_normal = transform_transpose(transform_inverse(tfm));

		/* apply to mesh vertices */
		for(size_t i = 0; i < mesh->verts.size(); i++)
			mesh->verts[i] = transform_point(&tfm, mesh->verts[i]);
		
		if(apply_to_motion) {
			Attribute *attr = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

			if (attr) {
				size_t steps_size = mesh->verts.size() * (mesh->motion_steps - 1);
				float3 *vert_steps = attr->data_float3();

				for (size_t i = 0; i < steps_size; i++)
					vert_steps[i] = transform_point(&tfm, vert_steps[i]);
			}

			Attribute *attr_N = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);

			if(attr_N) {
				Transform ntfm = mesh->transform_normal;
				size_t steps_size = mesh->verts.size() * (mesh->motion_steps - 1);
				float3 *normal_steps = attr_N->data_float3();

				for (size_t i = 0; i < steps_size; i++)
					normal_steps[i] = normalize(transform_direction(&ntfm, normal_steps[i]));
			}
		}
	}

	/* curves */
	if(mesh->curve_keys.size()) {
		/* compute uniform scale */
		float3 c0 = transform_get_column(&tfm, 0);
		float3 c1 = transform_get_column(&tfm, 1);
		float3 c2 = transform_get_column(&tfm, 2);
		float scalar = pow(fabsf(dot(cross(c0, c1), c2)), 1.0f/3.0f);

		/* apply transform to curve keys */
		for(size_t i = 0; i < mesh->curve_keys.size(); i++) {
			float3 co = transform_point(&tfm, float4_to_float3(mesh->curve_keys[i]));
			float radius = mesh->curve_keys[i].w * scalar;

			/* scale for curve radius is only correct for uniform scale */
			mesh->curve_keys[i] = float3_to_float4(co);
			mesh->curve_keys[i].w = radius;
		}

		if(apply_to_motion) {
			Attribute *curve_attr = mesh->curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

			if (curve_attr) {
				/* apply transform to motion curve keys */
				size_t steps_size = mesh->curve_keys.size() * (mesh->motion_steps - 1);
				float4 *key_steps = curve_attr->data_float4();

				for (size_t i = 0; i < steps_size; i++) {
					float3 co = transform_point(&tfm, float4_to_float3(key_steps[i]));
					float radius = key_steps[i].w * scalar;

					/* scale for curve radius is only correct for uniform scale */
					key_steps[i] = float3_to_float4(co);
					key_steps[i].w = radius;
				}
			}
		}
	}

	/* we keep normals pointing in same direction on negative scale, notify
	 * mesh about this in it (re)calculates normals */
	if(transform_negative_scale(tfm))
		mesh->transform_negative_scaled = true;

	if(bounds.valid()) {
		mesh->compute_bounds();
		compute_bounds(false);
	}

	/* tfm is not reset to identity, all code that uses it needs to check the
	   transform_applied boolean */
}

void Object::tag_update(Scene *scene)
{
	if(mesh) {
		if(mesh->transform_applied)
			mesh->need_update = true;

		foreach(uint sindex, mesh->used_shaders) {
			Shader *shader = scene->shaders[sindex];

			if(shader->use_mis && shader->has_surface_emission)
				scene->light_manager->need_update = true;
		}
	}

	scene->curve_system_manager->need_update = true;
	scene->mesh_manager->need_update = true;
	scene->object_manager->need_update = true;
}

vector<float> Object::motion_times()
{
	/* compute times at which we sample motion for this object */
	vector<float> times;

	if(!mesh || mesh->motion_steps == 1)
		return times;

	int motion_steps = mesh->motion_steps;

	for(int step = 0; step < motion_steps; step++) {
		if(step != motion_steps / 2) {
			float time = 2.0f * step / (motion_steps - 1) - 1.0f;
			times.push_back(time);
		}
	}

	return times;
}

/* Object Manager */

ObjectManager::ObjectManager()
{
	need_update = true;
}

ObjectManager::~ObjectManager()
{
}

void ObjectManager::device_update_transforms(Device *device, DeviceScene *dscene, Scene *scene, uint *object_flag, Progress& progress)
{
	float4 *objects;
	float4 *objects_vector = NULL;
	int i = 0;
	map<Mesh*, float> surface_area_map;
	map<ParticleSystem*, int> particle_offset;
	Scene::MotionType need_motion = scene->need_motion(device->info.advanced_shading);
	bool have_motion = false;
	bool have_curves = false;

	objects = dscene->objects.resize(OBJECT_SIZE*scene->objects.size());
	if(need_motion == Scene::MOTION_PASS)
		objects_vector = dscene->objects_vector.resize(OBJECT_VECTOR_SIZE*scene->objects.size());

	/* particle system device offsets
	 * 0 is dummy particle, index starts at 1
	 */
	int numparticles = 1;
	foreach(ParticleSystem *psys, scene->particle_systems) {
		particle_offset[psys] = numparticles;
		numparticles += psys->particles.size();
	}

	foreach(Object *ob, scene->objects) {
		Mesh *mesh = ob->mesh;
		uint flag = 0;

		/* compute transformations */
		Transform tfm = ob->tfm;
		Transform itfm = transform_inverse(tfm);

		/* compute surface area. for uniform scale we can do avoid the many
		 * transform calls and share computation for instances */
		/* todo: correct for displacement, and move to a better place */
		float uniform_scale;
		float surface_area = 0.0f;
		float pass_id = ob->pass_id;
		float random_number = (float)ob->random_id * (1.0f/(float)0xFFFFFFFF);
		int particle_index = (ob->particle_system)? ob->particle_index + particle_offset[ob->particle_system]: 0;

		if(transform_uniform_scale(tfm, uniform_scale)) {
			map<Mesh*, float>::iterator it = surface_area_map.find(mesh);

			if(it == surface_area_map.end()) {
				foreach(Mesh::Triangle& t, mesh->triangles) {
					float3 p1 = mesh->verts[t.v[0]];
					float3 p2 = mesh->verts[t.v[1]];
					float3 p3 = mesh->verts[t.v[2]];

					surface_area += triangle_area(p1, p2, p3);
				}

				surface_area_map[mesh] = surface_area;
			}
			else
				surface_area = it->second;

			surface_area *= uniform_scale;
		}
		else {
			foreach(Mesh::Triangle& t, mesh->triangles) {
				float3 p1 = transform_point(&tfm, mesh->verts[t.v[0]]);
				float3 p2 = transform_point(&tfm, mesh->verts[t.v[1]]);
				float3 p3 = transform_point(&tfm, mesh->verts[t.v[2]]);

				surface_area += triangle_area(p1, p2, p3);
			}
		}

		/* pack in texture */
		int offset = i*OBJECT_SIZE;

		/* OBJECT_TRANSFORM */
		memcpy(&objects[offset], &tfm, sizeof(float4)*3);
		/* OBJECT_INVERSE_TRANSFORM */
		memcpy(&objects[offset+4], &itfm, sizeof(float4)*3);
		/* OBJECT_PROPERTIES */
		objects[offset+8] = make_float4(surface_area, pass_id, random_number, __int_as_float(particle_index));

		if(need_motion == Scene::MOTION_PASS) {
			/* motion transformations, is world/object space depending if mesh
			 * comes with deformed position in object space, or if we transform
			 * the shading point in world space */
			Transform mtfm_pre = ob->motion.pre;
			Transform mtfm_post = ob->motion.post;

			if(!mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION)) {
				mtfm_pre = mtfm_pre * itfm;
				mtfm_post = mtfm_post * itfm;
			}

			memcpy(&objects_vector[i*OBJECT_VECTOR_SIZE+0], &mtfm_pre, sizeof(float4)*3);
			memcpy(&objects_vector[i*OBJECT_VECTOR_SIZE+3], &mtfm_post, sizeof(float4)*3);
		}
#ifdef __OBJECT_MOTION__
		else if(need_motion == Scene::MOTION_BLUR) {
			if(ob->use_motion) {
				/* decompose transformations for interpolation */
				DecompMotionTransform decomp;

				transform_motion_decompose(&decomp, &ob->motion, &ob->tfm);
				memcpy(&objects[offset], &decomp, sizeof(float4)*8);
				flag |= SD_OBJECT_MOTION;
				have_motion = true;
			}
		}
#endif

		if(mesh->use_motion_blur)
			have_motion = true;

		/* dupli object coords and motion info */
		int totalsteps = mesh->motion_steps;
		int numsteps = (totalsteps - 1)/2;
		int numverts = mesh->verts.size();
		int numkeys = mesh->curve_keys.size();

		objects[offset+9] = make_float4(ob->dupli_generated[0], ob->dupli_generated[1], ob->dupli_generated[2], __int_as_float(numkeys));
		objects[offset+10] = make_float4(ob->dupli_uv[0], ob->dupli_uv[1], __int_as_float(numsteps), __int_as_float(numverts));

		/* object flag */
		if(ob->use_holdout)
			flag |= SD_HOLDOUT_MASK;
		object_flag[i] = flag;

		/* have curves */
		if(mesh->curves.size())
			have_curves = true;

		i++;

		if(progress.get_cancel()) return;
	}

	device->tex_alloc("__objects", dscene->objects);
	if(need_motion == Scene::MOTION_PASS)
		device->tex_alloc("__objects_vector", dscene->objects_vector);

	dscene->data.bvh.have_motion = have_motion;
	dscene->data.bvh.have_curves = have_curves;
	dscene->data.bvh.have_instancing = true;
}

void ObjectManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;
	
	device_free(device, dscene);

	need_update = false;

	if(scene->objects.size() == 0)
		return;

	/* object info flag */
	uint *object_flag = dscene->object_flag.resize(scene->objects.size());

	/* set object transform matrices, before applying static transforms */
	progress.set_status("Updating Objects", "Copying Transformations to device");
	device_update_transforms(device, dscene, scene, object_flag, progress);

	if(progress.get_cancel()) return;

	/* prepare for static BVH building */
	/* todo: do before to support getting object level coords? */
	if(scene->params.bvh_type == SceneParams::BVH_STATIC) {
		progress.set_status("Updating Objects", "Applying Static Transformations");
		apply_static_transforms(dscene, scene, object_flag, progress);
	}

	/* allocate object flag */
	device->tex_alloc("__object_flag", dscene->object_flag);
}

void ObjectManager::device_free(Device *device, DeviceScene *dscene)
{
	device->tex_free(dscene->objects);
	dscene->objects.clear();

	device->tex_free(dscene->objects_vector);
	dscene->objects_vector.clear();

	device->tex_free(dscene->object_flag);
	dscene->object_flag.clear();
}

void ObjectManager::apply_static_transforms(DeviceScene *dscene, Scene *scene, uint *object_flag, Progress& progress)
{
	/* todo: normals and displacement should be done before applying transform! */
	/* todo: create objects/meshes in right order! */

	/* counter mesh users */
	map<Mesh*, int> mesh_users;
#ifdef __OBJECT_MOTION__
	Scene::MotionType need_motion = scene->need_motion();
	bool motion_blur = need_motion == Scene::MOTION_BLUR;
	bool apply_to_motion = need_motion != Scene::MOTION_PASS;
#else
	bool motion_blur = false;
#endif
	int i = 0;
	bool have_instancing = false;

	foreach(Object *object, scene->objects) {
		map<Mesh*, int>::iterator it = mesh_users.find(object->mesh);

		if(it == mesh_users.end())
			mesh_users[object->mesh] = 1;
		else
			it->second++;
	}

	if(progress.get_cancel()) return;

	/* apply transforms for objects with single user meshes */
	foreach(Object *object, scene->objects) {
		if(mesh_users[object->mesh] == 1) {
			if(!(motion_blur && object->use_motion)) {
				if(!object->mesh->transform_applied) {
					object->apply_transform(apply_to_motion);
					object->mesh->transform_applied = true;

					if(progress.get_cancel()) return;
				}

				object_flag[i] |= SD_TRANSFORM_APPLIED;
			}
			else
				have_instancing = true;
		}
		else
			have_instancing = true;

		i++;
	}

	dscene->data.bvh.have_instancing = have_instancing;
}

void ObjectManager::tag_update(Scene *scene)
{
	need_update = true;
	scene->curve_system_manager->need_update = true;
	scene->mesh_manager->need_update = true;
	scene->light_manager->need_update = true;
}

CCL_NAMESPACE_END

