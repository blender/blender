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

#include "render/camera.h"
#include "device/device.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/curves.h"
#include "render/object.h"
#include "render/particles.h"
#include "render/scene.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_map.h"
#include "util/util_progress.h"
#include "util/util_vector.h"

#include "subd/subd_patch_table.h"

CCL_NAMESPACE_BEGIN

/* Object */

NODE_DEFINE(Object)
{
	NodeType* type = NodeType::add("object", create);

	SOCKET_NODE(mesh, "Mesh", &Mesh::node_type);
	SOCKET_TRANSFORM(tfm, "Transform", transform_identity());
	SOCKET_UINT(visibility, "Visibility", ~0);
	SOCKET_UINT(random_id, "Random ID", 0);
	SOCKET_INT(pass_id, "Pass ID", 0);
	SOCKET_BOOLEAN(use_holdout, "Use Holdout", false);
	SOCKET_BOOLEAN(hide_on_missing_motion, "Hide on Missing Motion", false);
	SOCKET_POINT(dupli_generated, "Dupli Generated", make_float3(0.0f, 0.0f, 0.0f));
	SOCKET_POINT2(dupli_uv, "Dupli UV", make_float2(0.0f, 0.0f));

	SOCKET_BOOLEAN(is_shadow_catcher, "Shadow Catcher", false);

	return type;
}

Object::Object()
: Node(node_type)
{
	particle_system = NULL;
	particle_index = 0;
	bounds = BoundBox::empty;
	motion.pre = transform_empty();
	motion.mid = transform_empty();
	motion.post = transform_empty();
	use_motion = false;
}

Object::~Object()
{
}

void Object::compute_bounds(bool motion_blur)
{
	BoundBox mbounds = mesh->bounds;

	if(motion_blur && use_motion) {
		MotionTransform mtfm = motion;

		if(hide_on_missing_motion) {
			/* Hide objects that have no valid previous or next transform, for
			 * example particle that stop existing. TODO: add support for this
			 * case in the kernel so we don't get render artifacts. */
			if(mtfm.pre == transform_empty() ||
			   mtfm.post == transform_empty()) {
				bounds = BoundBox::empty;
				return;
			}
		}

		/* In case of missing motion information for previous/next frame,
		 * assume there is no motion. */
		if(mtfm.pre == transform_empty()) {
			mtfm.pre = tfm;
		}
		if(mtfm.post == transform_empty()) {
			mtfm.post = tfm;
		}

		DecompMotionTransform decomp;
		transform_motion_decompose(&decomp, &mtfm, &tfm);

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
	else {
		if(mesh->transform_applied) {
			bounds = mbounds;
		}
		else {
			bounds = mbounds.transformed(&tfm);
		}
	}
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

			if(attr) {
				size_t steps_size = mesh->verts.size() * (mesh->motion_steps - 1);
				float3 *vert_steps = attr->data_float3();

				for(size_t i = 0; i < steps_size; i++)
					vert_steps[i] = transform_point(&tfm, vert_steps[i]);
			}

			Attribute *attr_N = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);

			if(attr_N) {
				Transform ntfm = mesh->transform_normal;
				size_t steps_size = mesh->verts.size() * (mesh->motion_steps - 1);
				float3 *normal_steps = attr_N->data_float3();

				for(size_t i = 0; i < steps_size; i++)
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
		float scalar = powf(fabsf(dot(cross(c0, c1), c2)), 1.0f/3.0f);

		/* apply transform to curve keys */
		for(size_t i = 0; i < mesh->curve_keys.size(); i++) {
			float3 co = transform_point(&tfm, mesh->curve_keys[i]);
			float radius = mesh->curve_radius[i] * scalar;

			/* scale for curve radius is only correct for uniform scale */
			mesh->curve_keys[i] = co;
			mesh->curve_radius[i] = radius;
		}

		if(apply_to_motion) {
			Attribute *curve_attr = mesh->curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

			if(curve_attr) {
				/* apply transform to motion curve keys */
				size_t steps_size = mesh->curve_keys.size() * (mesh->motion_steps - 1);
				float4 *key_steps = curve_attr->data_float4();

				for(size_t i = 0; i < steps_size; i++) {
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
	 * transform_applied boolean */
}

void Object::tag_update(Scene *scene)
{
	if(mesh) {
		if(mesh->transform_applied)
			mesh->need_update = true;

		foreach(Shader *shader, mesh->used_shaders) {
			if(shader->use_mis && shader->has_surface_emission)
				scene->light_manager->need_update = true;
		}
	}

	scene->camera->need_flags_update = true;
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

bool Object::is_traceable()
{
	/* Mesh itself can be empty,can skip all such objects. */
	if(!bounds.valid() || bounds.size() == make_float3(0.0f, 0.0f, 0.0f)) {
		return false;
	}
	/* TODO(sergey): Check for mesh vertices/curves. visibility flags. */
	return true;
}

/* Object Manager */

ObjectManager::ObjectManager()
{
	need_update = true;
	need_flags_update = true;
}

ObjectManager::~ObjectManager()
{
}

void ObjectManager::device_update_object_transform(UpdateObejctTransformState *state,
                                                   Object *ob,
                                                   int object_index)
{
	float4 *objects = state->objects;
	float4 *objects_vector = state->objects_vector;

	Mesh *mesh = ob->mesh;
	uint flag = 0;

	/* Compute transformations. */
	Transform tfm = ob->tfm;
	Transform itfm = transform_inverse(tfm);

	/* Compute surface area. for uniform scale we can do avoid the many
	 * transform calls and share computation for instances.
	 *
	 * TODO(brecht): Correct for displacement, and move to a better place.
	 */
	float uniform_scale;
	float surface_area = 0.0f;
	float pass_id = ob->pass_id;
	float random_number = (float)ob->random_id * (1.0f/(float)0xFFFFFFFF);
	int particle_index = (ob->particle_system)
	        ? ob->particle_index + state->particle_offset[ob->particle_system]
	        : 0;

	if(transform_uniform_scale(tfm, uniform_scale)) {
		map<Mesh*, float>::iterator it;

		/* NOTE: This isn't fully optimal and could in theory lead to multiple
		 * threads calculating area of the same mesh in parallel. However, this
		 * also prevents suspending all the threads when some mesh's area is
		 * not yet known.
		 */
		state->surface_area_lock.lock();
		it = state->surface_area_map.find(mesh);
		state->surface_area_lock.unlock();

		if(it == state->surface_area_map.end()) {
			size_t num_triangles = mesh->num_triangles();
			for(size_t j = 0; j < num_triangles; j++) {
				Mesh::Triangle t = mesh->get_triangle(j);
				float3 p1 = mesh->verts[t.v[0]];
				float3 p2 = mesh->verts[t.v[1]];
				float3 p3 = mesh->verts[t.v[2]];

				surface_area += triangle_area(p1, p2, p3);
			}

			state->surface_area_lock.lock();
			state->surface_area_map[mesh] = surface_area;
			state->surface_area_lock.unlock();
		}
		else {
			surface_area = it->second;
		}

		surface_area *= uniform_scale;
	}
	else {
		size_t num_triangles = mesh->num_triangles();
		for(size_t j = 0; j < num_triangles; j++) {
			Mesh::Triangle t = mesh->get_triangle(j);
			float3 p1 = transform_point(&tfm, mesh->verts[t.v[0]]);
			float3 p2 = transform_point(&tfm, mesh->verts[t.v[1]]);
			float3 p3 = transform_point(&tfm, mesh->verts[t.v[2]]);

			surface_area += triangle_area(p1, p2, p3);
		}
	}

	/* Pack in texture. */
	int offset = object_index*OBJECT_SIZE;

	/* OBJECT_TRANSFORM */
	memcpy(&objects[offset], &tfm, sizeof(float4)*3);
	/* OBJECT_INVERSE_TRANSFORM */
	memcpy(&objects[offset+4], &itfm, sizeof(float4)*3);
	/* OBJECT_PROPERTIES */
	objects[offset+8] = make_float4(surface_area, pass_id, random_number, __int_as_float(particle_index));

	if(state->need_motion == Scene::MOTION_PASS) {
		/* Motion transformations, is world/object space depending if mesh
		 * comes with deformed position in object space, or if we transform
		 * the shading point in world space.
		 */
		MotionTransform mtfm = ob->motion;

		/* In case of missing motion information for previous/next frame,
		 * assume there is no motion. */
		if(!ob->use_motion || mtfm.pre == transform_empty()) {
			mtfm.pre = ob->tfm;
		}
		if(!ob->use_motion || mtfm.post == transform_empty()) {
			mtfm.post = ob->tfm;
		}

		if(!mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION)) {
			mtfm.pre = mtfm.pre * itfm;
			mtfm.post = mtfm.post * itfm;
		}
		else {
			flag |= SD_OBJECT_HAS_VERTEX_MOTION;
		}

		memcpy(&objects_vector[object_index*OBJECT_VECTOR_SIZE+0], &mtfm.pre, sizeof(float4)*3);
		memcpy(&objects_vector[object_index*OBJECT_VECTOR_SIZE+3], &mtfm.post, sizeof(float4)*3);
	}
#ifdef __OBJECT_MOTION__
	else if(state->need_motion == Scene::MOTION_BLUR) {
		if(ob->use_motion) {
			/* decompose transformations for interpolation. */
			DecompMotionTransform decomp;

			transform_motion_decompose(&decomp, &ob->motion, &ob->tfm);
			memcpy(&objects[offset], &decomp, sizeof(float4)*8);
			flag |= SD_OBJECT_MOTION;
			state->have_motion = true;
		}
	}
#endif

	if(mesh->use_motion_blur) {
		state->have_motion = true;
	}

	/* Dupli object coords and motion info. */
	int totalsteps = mesh->motion_steps;
	int numsteps = (totalsteps - 1)/2;
	int numverts = mesh->verts.size();
	int numkeys = mesh->curve_keys.size();

	objects[offset+9] = make_float4(ob->dupli_generated[0], ob->dupli_generated[1], ob->dupli_generated[2], __int_as_float(numkeys));
	objects[offset+10] = make_float4(ob->dupli_uv[0], ob->dupli_uv[1], __int_as_float(numsteps), __int_as_float(numverts));

	/* Object flag. */
	if(ob->use_holdout) {
		flag |= SD_OBJECT_HOLDOUT_MASK;
	}
	state->object_flag[object_index] = flag;

	/* Have curves. */
	if(mesh->num_curves()) {
		state->have_curves = true;
	}
}

bool ObjectManager::device_update_object_transform_pop_work(
        UpdateObejctTransformState *state,
        int *start_index,
        int *num_objects)
{
	/* Tweakable parameter, number of objects per chunk.
	 * Too small value will cause some extra overhead due to spin lock,
	 * too big value might not use all threads nicely.
	 */
	static const int OBJECTS_PER_TASK = 32;
	bool have_work = false;
	state->queue_lock.lock();
	int num_scene_objects = state->scene->objects.size();
	if(state->queue_start_object < num_scene_objects) {
		int count = min(OBJECTS_PER_TASK,
		                num_scene_objects - state->queue_start_object);
		*start_index = state->queue_start_object;
		*num_objects = count;
		state->queue_start_object += count;
		have_work = true;
	}
	state->queue_lock.unlock();
	return have_work;
}

void ObjectManager::device_update_object_transform_task(
        UpdateObejctTransformState *state)
{
	int start_index, num_objects;
	while(device_update_object_transform_pop_work(state,
	                                              &start_index,
	                                              &num_objects))
	{
		for(int i = 0; i < num_objects; ++i) {
			const int object_index = start_index + i;
			Object *ob = state->scene->objects[object_index];
			device_update_object_transform(state, ob, object_index);
		}
	}
}

void ObjectManager::device_update_transforms(Device *device,
                                             DeviceScene *dscene,
                                             Scene *scene,
                                             uint *object_flag,
                                             Progress& progress)
{
	UpdateObejctTransformState state;
	state.need_motion = scene->need_motion(device->info.advanced_shading);
	state.have_motion = false;
	state.have_curves = false;
	state.scene = scene;
	state.queue_start_object = 0;

	state.object_flag = object_flag;
	state.objects = dscene->objects.resize(OBJECT_SIZE*scene->objects.size());
	if(state.need_motion == Scene::MOTION_PASS) {
		state.objects_vector = dscene->objects_vector.resize(OBJECT_VECTOR_SIZE*scene->objects.size());
	}
	else {
		state.objects_vector = NULL;
	}

	/* Particle system device offsets
	 * 0 is dummy particle, index starts at 1.
	 */
	int numparticles = 1;
	foreach(ParticleSystem *psys, scene->particle_systems) {
		state.particle_offset[psys] = numparticles;
		numparticles += psys->particles.size();
	}

	/* NOTE: If it's just a handful of objects we deal with them in a single
	 * thread to avoid threading overhead. However, this threshold is might
	 * need some tweaks to make mid-complex scenes optimal.
	 */
	if(scene->objects.size() < 64) {
		int object_index = 0;
		foreach(Object *ob, scene->objects) {
			device_update_object_transform(&state, ob, object_index);
			object_index++;
			if(progress.get_cancel()) {
				return;
			}
		}
	}
	else {
		const int num_threads = TaskScheduler::num_threads();
		TaskPool pool;
		for(int i = 0; i < num_threads; ++i) {
			pool.push(function_bind(
			        &ObjectManager::device_update_object_transform_task,
			        this,
			        &state));
		}
		pool.wait_work();
		if(progress.get_cancel()) {
			return;
		}
	}

	device->tex_alloc("__objects", dscene->objects);
	if(state.need_motion == Scene::MOTION_PASS) {
		device->tex_alloc("__objects_vector", dscene->objects_vector);
	}

	dscene->data.bvh.have_motion = state.have_motion;
	dscene->data.bvh.have_curves = state.have_curves;
	dscene->data.bvh.have_instancing = true;
}

void ObjectManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;

	VLOG(1) << "Total " << scene->objects.size() << " objects.";

	device_free(device, dscene);

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
}

void ObjectManager::device_update_flags(Device *device,
                                        DeviceScene *dscene,
                                        Scene *scene,
                                        Progress& /*progress*/,
                                        bool bounds_valid)
{
	if(!need_update && !need_flags_update)
		return;

	need_update = false;
	need_flags_update = false;

	if(scene->objects.size() == 0)
		return;

	/* object info flag */
	uint *object_flag = dscene->object_flag.get_data();

	vector<Object *> volume_objects;
	bool has_volume_objects = false;
	foreach(Object *object, scene->objects) {
		if(object->mesh->has_volume) {
			if(bounds_valid) {
				volume_objects.push_back(object);
			}
			has_volume_objects = true;
		}
	}

	int object_index = 0;
	foreach(Object *object, scene->objects) {
		if(object->mesh->has_volume) {
			object_flag[object_index] |= SD_OBJECT_HAS_VOLUME;
		}
		else {
			object_flag[object_index] &= ~SD_OBJECT_HAS_VOLUME;
		}
		if(object->is_shadow_catcher) {
			object_flag[object_index] |= SD_OBJECT_SHADOW_CATCHER;
		}
		else {
			object_flag[object_index] &= ~SD_OBJECT_SHADOW_CATCHER;
		}

		if(bounds_valid) {
			foreach(Object *volume_object, volume_objects) {
				if(object == volume_object) {
					continue;
				}
				if(object->bounds.intersects(volume_object->bounds)) {
					object_flag[object_index] |= SD_OBJECT_INTERSECTS_VOLUME;
					break;
				}
			}
		}
		else if(has_volume_objects) {
			/* Not really valid, but can't make more reliable in the case
			 * of bounds not being up to date.
			 */
			object_flag[object_index] |= SD_OBJECT_INTERSECTS_VOLUME;
		}
		++object_index;
	}

	/* allocate object flag */
	device->tex_alloc("__object_flag", dscene->object_flag);
}

void ObjectManager::device_update_patch_map_offsets(Device *device, DeviceScene *dscene, Scene *scene)
{
	if(scene->objects.size() == 0) {
		return;
	}

	uint4* objects = (uint4*)dscene->objects.get_data();

	bool update = false;

	int object_index = 0;
	foreach(Object *object, scene->objects) {
		int offset = object_index*OBJECT_SIZE + 11;

		Mesh* mesh = object->mesh;

		if(mesh->patch_table) {
			uint patch_map_offset = 2*(mesh->patch_table_offset + mesh->patch_table->total_size() -
			                           mesh->patch_table->num_nodes * PATCH_NODE_SIZE) - mesh->patch_offset;

			if(objects[offset].x != patch_map_offset) {
				objects[offset].x = patch_map_offset;
				update = true;
			}
		}

		object_index++;
	}

	if(update) {
		device->tex_free(dscene->objects);
		device->tex_alloc("__objects", dscene->objects);
	}
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
	bool apply_to_motion = false;
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
		/* Annoying feedback loop here: we can't use is_instanced() because
		 * it'll use uninitialized transform_applied flag.
		 *
		 * Could be solved by moving reference counter to Mesh.
		 */
		if((mesh_users[object->mesh] == 1 && !object->mesh->has_surface_bssrdf) &&
		   !object->mesh->has_true_displacement() && object->mesh->subdivision_type == Mesh::SUBDIVISION_NONE)
		{
			if(!(motion_blur && object->use_motion)) {
				if(!object->mesh->transform_applied) {
					object->apply_transform(apply_to_motion);
					object->mesh->transform_applied = true;

					if(progress.get_cancel()) return;
				}

				object_flag[i] |= SD_OBJECT_TRANSFORM_APPLIED;
				if(object->mesh->transform_negative_scaled)
					object_flag[i] |= SD_OBJECT_NEGATIVE_SCALE_APPLIED;
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

