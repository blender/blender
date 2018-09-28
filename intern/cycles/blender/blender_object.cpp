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
#include "render/integrator.h"
#include "render/graph.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/nodes.h"
#include "render/particles.h"
#include "render/shader.h"

#include "blender/blender_object_cull.h"
#include "blender/blender_sync.h"
#include "blender/blender_util.h"

#include "util/util_foreach.h"
#include "util/util_hash.h"
#include "util/util_logging.h"

CCL_NAMESPACE_BEGIN

/* Utilities */

bool BlenderSync::BKE_object_is_modified(BL::Object& b_ob)
{
	/* test if we can instance or if the object is modified */
	if(b_ob.type() == BL::Object::type_META) {
		/* multi-user and dupli metaballs are fused, can't instance */
		return true;
	}
	else if(ccl::BKE_object_is_modified(b_ob, b_scene, preview)) {
		/* modifiers */
		return true;
	}
	else {
		/* object level material links */
		BL::Object::material_slots_iterator slot;
		for(b_ob.material_slots.begin(slot); slot != b_ob.material_slots.end(); ++slot)
			if(slot->link() == BL::MaterialSlot::link_OBJECT)
				return true;
	}

	return false;
}

bool BlenderSync::object_is_mesh(BL::Object& b_ob)
{
	BL::ID b_ob_data = b_ob.data();

	if(!b_ob_data) {
		return false;
	}

	if(b_ob.type() == BL::Object::type_CURVE) {
		/* Skip exporting curves without faces, overhead can be
		 * significant if there are many for path animation. */
		BL::Curve b_curve(b_ob.data());

		return (b_curve.bevel_object() ||
		        b_curve.extrude() != 0.0f ||
		        b_curve.bevel_depth() != 0.0f ||
		        b_curve.dimensions() == BL::Curve::dimensions_2D ||
		        b_ob.modifiers.length());
	}
	else {
		return (b_ob_data.is_a(&RNA_Mesh) ||
		        b_ob_data.is_a(&RNA_Curve) ||
		        b_ob_data.is_a(&RNA_MetaBall));
	}
}

bool BlenderSync::object_is_light(BL::Object& b_ob)
{
	BL::ID b_ob_data = b_ob.data();

	return (b_ob_data && b_ob_data.is_a(&RNA_Lamp));
}

static uint object_ray_visibility(BL::Object& b_ob)
{
	PointerRNA cvisibility = RNA_pointer_get(&b_ob.ptr, "cycles_visibility");
	uint flag = 0;

	flag |= get_boolean(cvisibility, "camera")? PATH_RAY_CAMERA: 0;
	flag |= get_boolean(cvisibility, "diffuse")? PATH_RAY_DIFFUSE: 0;
	flag |= get_boolean(cvisibility, "glossy")? PATH_RAY_GLOSSY: 0;
	flag |= get_boolean(cvisibility, "transmission")? PATH_RAY_TRANSMIT: 0;
	flag |= get_boolean(cvisibility, "shadow")? PATH_RAY_SHADOW: 0;
	flag |= get_boolean(cvisibility, "scatter")? PATH_RAY_VOLUME_SCATTER: 0;

	return flag;
}

/* Light */

void BlenderSync::sync_light(BL::Object& b_parent,
                             int persistent_id[OBJECT_PERSISTENT_ID_SIZE],
                             BL::Object& b_ob,
                             BL::DupliObject& b_dupli_ob,
                             Transform& tfm,
                             bool *use_portal)
{
	/* test if we need to sync */
	Light *light;
	ObjectKey key(b_parent, persistent_id, b_ob);

	if(!light_map.sync(&light, b_ob, b_parent, key)) {
		if(light->is_portal)
			*use_portal = true;
		return;
	}

	BL::Lamp b_lamp(b_ob.data());

	/* type */
	switch(b_lamp.type()) {
		case BL::Lamp::type_POINT: {
			BL::PointLamp b_point_lamp(b_lamp);
			light->size = b_point_lamp.shadow_soft_size();
			light->type = LIGHT_POINT;
			break;
		}
		case BL::Lamp::type_SPOT: {
			BL::SpotLamp b_spot_lamp(b_lamp);
			light->size = b_spot_lamp.shadow_soft_size();
			light->type = LIGHT_SPOT;
			light->spot_angle = b_spot_lamp.spot_size();
			light->spot_smooth = b_spot_lamp.spot_blend();
			break;
		}
		case BL::Lamp::type_HEMI: {
			light->type = LIGHT_DISTANT;
			light->size = 0.0f;
			break;
		}
		case BL::Lamp::type_SUN: {
			BL::SunLamp b_sun_lamp(b_lamp);
			light->size = b_sun_lamp.shadow_soft_size();
			light->type = LIGHT_DISTANT;
			break;
		}
		case BL::Lamp::type_AREA: {
			BL::AreaLamp b_area_lamp(b_lamp);
			light->size = 1.0f;
			light->axisu = transform_get_column(&tfm, 0);
			light->axisv = transform_get_column(&tfm, 1);
			light->sizeu = b_area_lamp.size();
			if(b_area_lamp.shape() == BL::AreaLamp::shape_RECTANGLE)
				light->sizev = b_area_lamp.size_y();
			else
				light->sizev = light->sizeu;
			light->type = LIGHT_AREA;
			break;
		}
	}

	/* location and (inverted!) direction */
	light->co = transform_get_column(&tfm, 3);
	light->dir = -transform_get_column(&tfm, 2);
	light->tfm = tfm;

	/* shader */
	vector<Shader*> used_shaders;
	find_shader(b_lamp, used_shaders, scene->default_light);
	light->shader = used_shaders[0];

	/* shadow */
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
	PointerRNA clamp = RNA_pointer_get(&b_lamp.ptr, "cycles");
	light->cast_shadow = get_boolean(clamp, "cast_shadow");
	light->use_mis = get_boolean(clamp, "use_multiple_importance_sampling");

	int samples = get_int(clamp, "samples");
	if(get_boolean(cscene, "use_square_samples"))
		light->samples = samples * samples;
	else
		light->samples = samples;

	light->max_bounces = get_int(clamp, "max_bounces");

	if(b_dupli_ob) {
		light->random_id = b_dupli_ob.random_id();
	}
	else {
		light->random_id = hash_int_2d(hash_string(b_ob.name().c_str()), 0);
	}

	if(light->type == LIGHT_AREA)
		light->is_portal = get_boolean(clamp, "is_portal");
	else
		light->is_portal = false;

	if(light->is_portal)
		*use_portal = true;

	/* visibility */
	uint visibility = object_ray_visibility(b_ob);
	light->use_diffuse = (visibility & PATH_RAY_DIFFUSE) != 0;
	light->use_glossy = (visibility & PATH_RAY_GLOSSY) != 0;
	light->use_transmission = (visibility & PATH_RAY_TRANSMIT) != 0;
	light->use_scatter = (visibility & PATH_RAY_VOLUME_SCATTER) != 0;

	/* tag */
	light->tag_update(scene);
}

void BlenderSync::sync_background_light(bool use_portal)
{
	BL::World b_world = b_scene.world();

	if(b_world) {
		PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
		PointerRNA cworld = RNA_pointer_get(&b_world.ptr, "cycles");

		enum SamplingMethod {
			SAMPLING_NONE = 0,
			SAMPLING_AUTOMATIC,
			SAMPLING_MANUAL,
			SAMPLING_NUM
		};
		int sampling_method = get_enum(cworld, "sampling_method", SAMPLING_NUM, SAMPLING_AUTOMATIC);
		bool sample_as_light = (sampling_method != SAMPLING_NONE);

		if(sample_as_light || use_portal) {
			/* test if we need to sync */
			Light *light;
			ObjectKey key(b_world, 0, b_world);

			if(light_map.sync(&light, b_world, b_world, key) ||
			    world_recalc ||
			    b_world.ptr.data != world_map)
			{
				light->type = LIGHT_BACKGROUND;
				if(sampling_method == SAMPLING_MANUAL) {
					light->map_resolution = get_int(cworld, "sample_map_resolution");
				}
				else {
					light->map_resolution = 0;
				}
				light->shader = scene->default_background;
				light->use_mis = sample_as_light;
				light->max_bounces = get_int(cworld, "max_bounces");

				int samples = get_int(cworld, "samples");
				if(get_boolean(cscene, "use_square_samples"))
					light->samples = samples * samples;
				else
					light->samples = samples;

				light->tag_update(scene);
				light_map.set_recalc(b_world);
			}
		}
	}

	world_map = b_world.ptr.data;
	world_recalc = false;
}

/* Object */

Object *BlenderSync::sync_object(BL::Object& b_parent,
                                 int persistent_id[OBJECT_PERSISTENT_ID_SIZE],
                                 BL::DupliObject& b_dupli_ob,
                                 Transform& tfm,
                                 uint layer_flag,
                                 float motion_time,
                                 bool hide_tris,
                                 BlenderObjectCulling& culling,
                                 bool *use_portal)
{
	BL::Object b_ob = (b_dupli_ob ? b_dupli_ob.object() : b_parent);
	bool motion = motion_time != 0.0f;

	/* light is handled separately */
	if(object_is_light(b_ob)) {
		/* don't use lamps for excluded layers used as mask layer */
		if(!motion && !((layer_flag & render_layer.holdout_layer) && (layer_flag & render_layer.exclude_layer)))
			sync_light(b_parent, persistent_id, b_ob, b_dupli_ob, tfm, use_portal);

		return NULL;
	}

	/* only interested in object that we can create meshes from */
	if(!object_is_mesh(b_ob)) {
		return NULL;
	}

	/* Perform object culling. */
	if(culling.test(scene, b_ob, tfm)) {
		return NULL;
	}

	/* Visibility flags for both parent and child. */
	PointerRNA cobject = RNA_pointer_get(&b_ob.ptr, "cycles");
	bool use_holdout = (layer_flag & render_layer.holdout_layer) != 0 ||
	                   get_boolean(cobject, "is_holdout");
	uint visibility = object_ray_visibility(b_ob) & PATH_RAY_ALL_VISIBILITY;

	if(b_parent.ptr.data != b_ob.ptr.data) {
		visibility &= object_ray_visibility(b_parent);
	}

	/* Make holdout objects on excluded layer invisible for non-camera rays. */
	if(use_holdout && (layer_flag & render_layer.exclude_layer)) {
		visibility &= ~(PATH_RAY_ALL_VISIBILITY - PATH_RAY_CAMERA);
	}

	/* Hide objects not on render layer from camera rays. */
	if(!(layer_flag & render_layer.layer)) {
		visibility &= ~PATH_RAY_CAMERA;
	}

	/* Don't export completely invisible objects. */
	if(visibility == 0) {
		return NULL;
	}

	/* key to lookup object */
	ObjectKey key(b_parent, persistent_id, b_ob);
	Object *object;

	/* motion vector case */
	if(motion) {
		object = object_map.find(key);

		if(object && object->use_motion()) {
			/* Set transform at matching motion time step. */
			int time_index = object->motion_step(motion_time);
			if(time_index >= 0) {
				object->motion[time_index] = tfm;
			}

			/* mesh deformation */
			if(object->mesh)
				sync_mesh_motion(b_ob, object, motion_time);
		}

		return object;
	}

	/* test if we need to sync */
	bool object_updated = false;

	if(object_map.sync(&object, b_ob, b_parent, key))
		object_updated = true;

	/* mesh sync */
	object->mesh = sync_mesh(b_ob, object_updated, hide_tris);

	/* special case not tracked by object update flags */

	/* holdout */
	if(use_holdout != object->use_holdout) {
		object->use_holdout = use_holdout;
		scene->object_manager->tag_update(scene);
		object_updated = true;
	}

	if(visibility != object->visibility) {
		object->visibility = visibility;
		object_updated = true;
	}

	bool is_shadow_catcher = get_boolean(cobject, "is_shadow_catcher");
	if(is_shadow_catcher != object->is_shadow_catcher) {
		object->is_shadow_catcher = is_shadow_catcher;
		object_updated = true;
	}

	/* object sync
	 * transform comparison should not be needed, but duplis don't work perfect
	 * in the depsgraph and may not signal changes, so this is a workaround */
	if(object_updated || (object->mesh && object->mesh->need_update) || tfm != object->tfm) {
		object->name = b_ob.name().c_str();
		object->pass_id = b_ob.pass_index();
		object->tfm = tfm;
		object->motion.clear();

		/* motion blur */
		Scene::MotionType need_motion = scene->need_motion();
		if(need_motion != Scene::MOTION_NONE && object->mesh) {
			Mesh *mesh = object->mesh;
			mesh->use_motion_blur = false;
			mesh->motion_steps = 0;

			uint motion_steps;

			if(scene->need_motion() == Scene::MOTION_BLUR) {
				motion_steps = object_motion_steps(b_parent, b_ob);
				mesh->motion_steps = motion_steps;
				if(motion_steps && object_use_deform_motion(b_parent, b_ob)) {
					mesh->use_motion_blur = true;
				}
			}
			else {
				motion_steps = 3;
				mesh->motion_steps = motion_steps;
			}

			object->motion.clear();
			object->motion.resize(motion_steps, transform_empty());

			if(motion_steps) {
				object->motion[motion_steps/2] = tfm;

				for(size_t step = 0; step < motion_steps; step++) {
					motion_times.insert(object->motion_time(step));
				}
			}
		}

		/* dupli texture coordinates and random_id */
		if(b_dupli_ob) {
			object->dupli_generated = 0.5f*get_float3(b_dupli_ob.orco()) - make_float3(0.5f, 0.5f, 0.5f);
			object->dupli_uv = get_float2(b_dupli_ob.uv());
			object->random_id = b_dupli_ob.random_id();
		}
		else {
			object->dupli_generated = make_float3(0.0f, 0.0f, 0.0f);
			object->dupli_uv = make_float2(0.0f, 0.0f);
			object->random_id =  hash_int_2d(hash_string(object->name.c_str()), 0);
		}

		object->tag_update(scene);
	}

	return object;
}

static bool object_render_hide_original(BL::Object::type_enum ob_type,
                                        BL::Object::dupli_type_enum dupli_type)
{
	/* metaball exception, they duplicate self */
	if(ob_type == BL::Object::type_META)
		return false;

	return (dupli_type == BL::Object::dupli_type_VERTS ||
	        dupli_type == BL::Object::dupli_type_FACES ||
	        dupli_type == BL::Object::dupli_type_FRAMES);
}

static bool object_render_hide(BL::Object& b_ob,
                               bool top_level,
                               bool parent_hide,
                               bool& hide_triangles)
{
	/* check if we should render or hide particle emitter */
	BL::Object::particle_systems_iterator b_psys;

	bool hair_present = false;
	bool show_emitter = false;
	bool hide_emitter = false;
	bool hide_as_dupli_parent = false;
	bool hide_as_dupli_child_original = false;

	for(b_ob.particle_systems.begin(b_psys); b_psys != b_ob.particle_systems.end(); ++b_psys) {
		if((b_psys->settings().render_type() == BL::ParticleSettings::render_type_PATH) &&
		   (b_psys->settings().type()==BL::ParticleSettings::type_HAIR))
			hair_present = true;

		if(b_psys->settings().use_render_emitter())
			show_emitter = true;
		else
			hide_emitter = true;
	}

	if(show_emitter)
		hide_emitter = false;

	/* duplicators hidden by default, except dupliframes which duplicate self */
	if(b_ob.is_duplicator())
		if(top_level || b_ob.dupli_type() != BL::Object::dupli_type_FRAMES)
			hide_as_dupli_parent = true;

	/* hide original object for duplis */
	BL::Object parent = b_ob.parent();
	while(parent) {
		if(object_render_hide_original(b_ob.type(),
		                               parent.dupli_type()))
		{
			if(parent_hide) {
				hide_as_dupli_child_original = true;
				break;
			}
		}
		parent = parent.parent();
	}

	hide_triangles = hide_emitter;

	if(show_emitter) {
		return false;
	}
	else if(hair_present) {
		return hide_as_dupli_child_original;
	}
	else {
		return (hide_as_dupli_parent || hide_as_dupli_child_original);
	}
}

static bool object_render_hide_duplis(BL::Object& b_ob)
{
	BL::Object parent = b_ob.parent();

	return (parent && object_render_hide_original(b_ob.type(), parent.dupli_type()));
}

/* Object Loop */

void BlenderSync::sync_objects(float motion_time)
{
	/* layer data */
	uint scene_layer = render_layer.scene_layer;
	bool motion = motion_time != 0.0f;

	if(!motion) {
		/* prepare for sync */
		light_map.pre_sync();
		mesh_map.pre_sync();
		object_map.pre_sync();
		particle_system_map.pre_sync();
		motion_times.clear();
	}
	else {
		mesh_motion_synced.clear();
	}

	/* initialize culling */
	BlenderObjectCulling culling(scene, b_scene);

	/* object loop */
	BL::Scene::object_bases_iterator b_base;
	BL::Scene b_sce = b_scene;
	/* modifier result type (not exposed as enum in C++ API)
	 * 1 : DAG_EVAL_PREVIEW
	 * 2 : DAG_EVAL_RENDER
	 */
	int dupli_settings = (render_layer.use_viewport_visibility) ? 1 : 2;

	bool cancel = false;
	bool use_portal = false;

	uint layer_override = get_layer(b_engine.layer_override());
	for(; b_sce && !cancel; b_sce = b_sce.background_set()) {
		/* Render layer's scene_layer is affected by local view already,
		 * which is not a desired behavior here.
		 */
		uint scene_layers = layer_override ? layer_override : get_layer(b_scene.layers());
		for(b_sce.object_bases.begin(b_base); b_base != b_sce.object_bases.end() && !cancel; ++b_base) {
			BL::Object b_ob = b_base->object();
			bool hide = (render_layer.use_viewport_visibility)? b_ob.hide(): b_ob.hide_render();
			uint ob_layer = get_layer(b_base->layers(),
			                          b_base->layers_local_view(),
			                          object_is_light(b_ob),
			                          scene_layers);
			hide = hide || !(ob_layer & scene_layer);

			if(!hide) {
				progress.set_sync_status("Synchronizing object", b_ob.name());

				/* load per-object culling data */
				culling.init_object(scene, b_ob);

				if(b_ob.is_duplicator() && !object_render_hide_duplis(b_ob)) {
					/* dupli objects */
					b_ob.dupli_list_create(b_data, b_scene, dupli_settings);

					BL::Object::dupli_list_iterator b_dup;

					for(b_ob.dupli_list.begin(b_dup); b_dup != b_ob.dupli_list.end(); ++b_dup) {
						Transform tfm = get_transform(b_dup->matrix());
						BL::Object b_dup_ob = b_dup->object();
						bool dup_hide = (render_layer.use_viewport_visibility)? b_dup_ob.hide(): b_dup_ob.hide_render();
						bool in_dupli_group = (b_dup->type() == BL::DupliObject::type_GROUP);
						bool hide_tris;

						if(!(b_dup->hide() || dup_hide || object_render_hide(b_dup_ob, false, in_dupli_group, hide_tris))) {
							/* the persistent_id allows us to match dupli objects
							 * between frames and updates */
							BL::Array<int, OBJECT_PERSISTENT_ID_SIZE> persistent_id = b_dup->persistent_id();

							/* sync object and mesh or light data */
							Object *object = sync_object(b_ob,
							                             persistent_id.data,
							                             *b_dup,
							                             tfm,
							                             ob_layer,
							                             motion_time,
							                             hide_tris,
							                             culling,
							                             &use_portal);

							/* sync possible particle data, note particle_id
							 * starts counting at 1, first is dummy particle */
							if(!motion && object) {
								sync_dupli_particle(b_ob, *b_dup, object);
							}

						}
					}

					b_ob.dupli_list_clear();
				}

				/* test if object needs to be hidden */
				bool hide_tris;

				if(!object_render_hide(b_ob, true, true, hide_tris)) {
					/* object itself */
					Transform tfm = get_transform(b_ob.matrix_world());
					BL::DupliObject b_empty_dupli_ob(PointerRNA_NULL);
					sync_object(b_ob,
					            NULL,
					            b_empty_dupli_ob,
					            tfm,
					            ob_layer,
					            motion_time,
					            hide_tris,
					            culling,
					            &use_portal);
				}
			}

			cancel = progress.get_cancel();
		}
	}

	progress.set_sync_status("");

	if(!cancel && !motion) {
		sync_background_light(use_portal);

		/* handle removed data and modified pointers */
		if(light_map.post_sync())
			scene->light_manager->tag_update(scene);
		if(mesh_map.post_sync())
			scene->mesh_manager->tag_update(scene);
		if(object_map.post_sync())
			scene->object_manager->tag_update(scene);
		if(particle_system_map.post_sync())
			scene->particle_system_manager->tag_update(scene);
	}

	if(motion)
		mesh_motion_synced.clear();
}

void BlenderSync::sync_motion(BL::RenderSettings& b_render,
                              BL::Object& b_override,
                              int width, int height,
                              void **python_thread_state)
{
	if(scene->need_motion() == Scene::MOTION_NONE)
		return;

	/* get camera object here to deal with camera switch */
	BL::Object b_cam = b_scene.camera();
	if(b_override)
		b_cam = b_override;

	Camera prevcam = *(scene->camera);

	int frame_center = b_scene.frame_current();
	float subframe_center = b_scene.frame_subframe();
	float frame_center_delta = 0.0f;

	if(scene->need_motion() != Scene::MOTION_PASS &&
	   scene->camera->motion_position != Camera::MOTION_POSITION_CENTER)
	{
		float shuttertime = scene->camera->shuttertime;
		if(scene->camera->motion_position == Camera::MOTION_POSITION_END) {
			frame_center_delta = -shuttertime * 0.5f;
		}
		else {
			assert(scene->camera->motion_position == Camera::MOTION_POSITION_START);
			frame_center_delta = shuttertime * 0.5f;
		}
		float time = frame_center + subframe_center + frame_center_delta;
		int frame = (int)floorf(time);
		float subframe = time - frame;
		python_thread_state_restore(python_thread_state);
		b_engine.frame_set(frame, subframe);
		python_thread_state_save(python_thread_state);
		sync_camera_motion(b_render, b_cam, width, height, 0.0f);
		sync_objects(0.0f);
	}

	/* always sample these times for camera motion */
	motion_times.insert(-1.0f);
	motion_times.insert(1.0f);

	/* note iteration over motion_times set happens in sorted order */
	foreach(float relative_time, motion_times) {
		/* center time is already handled. */
		if(relative_time == 0.0f) {
			continue;
		}

		VLOG(1) << "Synchronizing motion for the relative time "
		        << relative_time << ".";

		/* fixed shutter time to get previous and next frame for motion pass */
		float shuttertime = scene->motion_shutter_time();

		/* compute frame and subframe time */
		float time = frame_center + subframe_center + frame_center_delta + relative_time * shuttertime * 0.5f;
		int frame = (int)floorf(time);
		float subframe = time - frame;

		/* change frame */
		python_thread_state_restore(python_thread_state);
		b_engine.frame_set(frame, subframe);
		python_thread_state_save(python_thread_state);

		/* sync camera, only supports two times at the moment */
		if(relative_time == -1.0f || relative_time == 1.0f) {
			sync_camera_motion(b_render,
			                   b_cam,
			                   width, height,
			                   relative_time);
		}

		/* sync object */
		sync_objects(relative_time);
	}

	/* we need to set the python thread state again because this
	 * function assumes it is being executed from python and will
	 * try to save the thread state */
	python_thread_state_restore(python_thread_state);
	b_engine.frame_set(frame_center, subframe_center);
	python_thread_state_save(python_thread_state);

	/* tag camera for motion update */
	if(scene->camera->motion_modified(prevcam))
		scene->camera->tag_update();
}

CCL_NAMESPACE_END
