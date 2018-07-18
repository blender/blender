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

#include "render/background.h"
#include "render/camera.h"
#include "render/film.h"
#include "render/graph.h"
#include "render/integrator.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/nodes.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/shader.h"
#include "render/curves.h"

#include "device/device.h"

#include "blender/blender_sync.h"
#include "blender/blender_session.h"
#include "blender/blender_util.h"

#include "util/util_debug.h"
#include "util/util_foreach.h"
#include "util/util_opengl.h"
#include "util/util_hash.h"

CCL_NAMESPACE_BEGIN

/* Constructor */

BlenderSync::BlenderSync(BL::RenderEngine& b_engine,
                         BL::BlendData& b_data,
                         BL::Scene& b_scene,
                         Scene *scene,
                         bool preview,
                         Progress &progress)
: b_engine(b_engine),
  b_data(b_data),
  b_scene(b_scene),
  shader_map(&scene->shaders),
  object_map(&scene->objects),
  mesh_map(&scene->meshes),
  light_map(&scene->lights),
  particle_system_map(&scene->particle_systems),
  world_map(NULL),
  world_recalc(false),
  scene(scene),
  preview(preview),
  experimental(false),
  dicing_rate(1.0f),
  max_subdivisions(12),
  progress(progress)
{
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
	dicing_rate = preview ? RNA_float_get(&cscene, "preview_dicing_rate") : RNA_float_get(&cscene, "dicing_rate");
	max_subdivisions = RNA_int_get(&cscene, "max_subdivisions");
}

BlenderSync::~BlenderSync()
{
}

/* Sync */

void BlenderSync::sync_recalc(BL::Depsgraph& b_depsgraph)
{
	/* Sync recalc flags from blender to cycles. Actual update is done separate,
	 * so we can do it later on if doing it immediate is not suitable. */

	bool has_updated_objects = b_depsgraph.id_type_updated(BL::DriverTarget::id_type_OBJECT);
	bool dicing_prop_changed = false;

	if(experimental) {
		PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

		float updated_dicing_rate = preview ? RNA_float_get(&cscene, "preview_dicing_rate")
		                                    : RNA_float_get(&cscene, "dicing_rate");

		if(dicing_rate != updated_dicing_rate) {
			dicing_rate = updated_dicing_rate;
			dicing_prop_changed = true;
		}

		int updated_max_subdivisions = RNA_int_get(&cscene, "max_subdivisions");

		if(max_subdivisions != updated_max_subdivisions) {
			max_subdivisions = updated_max_subdivisions;
			dicing_prop_changed = true;
		}
	}

	/* Iterate over all IDs in this depsgraph. */
	BL::Depsgraph::updates_iterator b_update;
	for(b_depsgraph.updates.begin(b_update); b_update != b_depsgraph.updates.end(); ++b_update) {
		BL::ID b_id(b_update->id());

		/* Material */
		if (b_id.is_a(&RNA_Material)) {
			BL::Material b_mat(b_id);
			shader_map.set_recalc(b_mat);
		}
		/* Light */
		else if (b_id.is_a(&RNA_Light)) {
			BL::Light b_light(b_id);
			shader_map.set_recalc(b_light);
		}
		/* Object */
		else if (b_id.is_a(&RNA_Object)) {
			BL::Object b_ob(b_id);
			const bool updated_geometry = !b_update->is_dirty_geometry();

			if (!b_update->is_dirty_transform()) {
				object_map.set_recalc(b_ob);
				light_map.set_recalc(b_ob);
			}

			if(object_is_mesh(b_ob)) {
				if(updated_geometry ||
				   (dicing_prop_changed && object_subdivision_type(b_ob, preview, experimental) != Mesh::SUBDIVISION_NONE))
				{
					BL::ID key = BKE_object_is_modified(b_ob)? b_ob: b_ob.data();
					mesh_map.set_recalc(key);
				}
			}
			else if(object_is_light(b_ob)) {
				if(updated_geometry) {
					light_map.set_recalc(b_ob);
				}
			}

			if(updated_geometry) {
				BL::Object::particle_systems_iterator b_psys;
				for(b_ob.particle_systems.begin(b_psys); b_psys != b_ob.particle_systems.end(); ++b_psys)
					particle_system_map.set_recalc(b_ob);
			}
		}
		/* Mesh */
		else if (b_id.is_a(&RNA_Mesh)) {
			BL::Mesh b_mesh(b_id);
			mesh_map.set_recalc(b_mesh);
		}
		/* World */
		else if (b_id.is_a(&RNA_World)) {
			BL::World b_world(b_id);
			if(world_map == b_world.ptr.data) {
				world_recalc = true;
			}
		}
	}

	/* Updates shader with object dependency if objects changed. */
	if (has_updated_objects) {
		if(scene->default_background->has_object_dependency) {
			world_recalc = true;
		}

		foreach(Shader *shader, scene->shaders) {
			if (shader->has_object_dependency) {
				shader->need_sync_object = true;
			}
		}
	}
}

void BlenderSync::sync_data(BL::RenderSettings& b_render,
                            BL::Depsgraph& b_depsgraph,
                            BL::SpaceView3D& b_v3d,
                            BL::Object& b_override,
                            int width, int height,
                            void **python_thread_state)
{
	BL::ViewLayer b_view_layer = b_depsgraph.view_layer_eval();

	sync_view_layer(b_v3d, b_view_layer);
	sync_integrator();
	sync_film();
	sync_shaders(b_depsgraph);
	sync_images();
	sync_curve_settings();

	mesh_synced.clear(); /* use for objects and motion sync */

	if(scene->need_motion() == Scene::MOTION_PASS ||
	   scene->need_motion() == Scene::MOTION_NONE ||
	   scene->camera->motion_position == Camera::MOTION_POSITION_CENTER)
	{
		sync_objects(b_depsgraph);
	}
	sync_motion(b_render,
	            b_depsgraph,
	            b_override,
	            width, height,
	            python_thread_state);

	mesh_synced.clear();

	free_data_after_sync(b_depsgraph);
}

/* Integrator */

void BlenderSync::sync_integrator()
{
	BL::RenderSettings r = b_scene.render();
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

	experimental = (get_enum(cscene, "feature_set") != 0);

	Integrator *integrator = scene->integrator;
	Integrator previntegrator = *integrator;

	integrator->max_bounce = get_int(cscene, "max_bounces");

	integrator->max_diffuse_bounce = get_int(cscene, "diffuse_bounces");
	integrator->max_glossy_bounce = get_int(cscene, "glossy_bounces");
	integrator->max_transmission_bounce = get_int(cscene, "transmission_bounces");
	integrator->max_volume_bounce = get_int(cscene, "volume_bounces");

	integrator->transparent_max_bounce = get_int(cscene, "transparent_max_bounces");

	integrator->volume_max_steps = get_int(cscene, "volume_max_steps");
	integrator->volume_step_size = get_float(cscene, "volume_step_size");

	integrator->caustics_reflective = get_boolean(cscene, "caustics_reflective");
	integrator->caustics_refractive = get_boolean(cscene, "caustics_refractive");
	integrator->filter_glossy = get_float(cscene, "blur_glossy");

	integrator->seed = get_int(cscene, "seed");
	if(get_boolean(cscene, "use_animated_seed")) {
		integrator->seed = hash_int_2d(b_scene.frame_current(),
		                               get_int(cscene, "seed"));
		if(b_scene.frame_subframe() != 0.0f) {
			/* TODO(sergey): Ideally should be some sort of hash_merge,
			 * but this is good enough for now.
			 */
			integrator->seed += hash_int_2d((int)(b_scene.frame_subframe() * (float)INT_MAX),
			                                get_int(cscene, "seed"));
		}
	}

	integrator->sampling_pattern = (SamplingPattern)get_enum(
	        cscene,
	        "sampling_pattern",
	        SAMPLING_NUM_PATTERNS,
	        SAMPLING_PATTERN_SOBOL);

	integrator->sample_clamp_direct = get_float(cscene, "sample_clamp_direct");
	integrator->sample_clamp_indirect = get_float(cscene, "sample_clamp_indirect");
	if(!preview) {
		if(integrator->motion_blur != r.use_motion_blur()) {
			scene->object_manager->tag_update(scene);
			scene->camera->tag_update();
		}

		integrator->motion_blur = r.use_motion_blur();
	}

	integrator->method = (Integrator::Method)get_enum(cscene,
	                                                  "progressive",
	                                                  Integrator::NUM_METHODS,
	                                                  Integrator::PATH);

	integrator->sample_all_lights_direct = get_boolean(cscene, "sample_all_lights_direct");
	integrator->sample_all_lights_indirect = get_boolean(cscene, "sample_all_lights_indirect");
	integrator->light_sampling_threshold = get_float(cscene, "light_sampling_threshold");

	int diffuse_samples = get_int(cscene, "diffuse_samples");
	int glossy_samples = get_int(cscene, "glossy_samples");
	int transmission_samples = get_int(cscene, "transmission_samples");
	int ao_samples = get_int(cscene, "ao_samples");
	int mesh_light_samples = get_int(cscene, "mesh_light_samples");
	int subsurface_samples = get_int(cscene, "subsurface_samples");
	int volume_samples = get_int(cscene, "volume_samples");

	if(get_boolean(cscene, "use_square_samples")) {
		integrator->diffuse_samples = diffuse_samples * diffuse_samples;
		integrator->glossy_samples = glossy_samples * glossy_samples;
		integrator->transmission_samples = transmission_samples * transmission_samples;
		integrator->ao_samples = ao_samples * ao_samples;
		integrator->mesh_light_samples = mesh_light_samples * mesh_light_samples;
		integrator->subsurface_samples = subsurface_samples * subsurface_samples;
		integrator->volume_samples = volume_samples * volume_samples;
	}
	else {
		integrator->diffuse_samples = diffuse_samples;
		integrator->glossy_samples = glossy_samples;
		integrator->transmission_samples = transmission_samples;
		integrator->ao_samples = ao_samples;
		integrator->mesh_light_samples = mesh_light_samples;
		integrator->subsurface_samples = subsurface_samples;
		integrator->volume_samples = volume_samples;
	}

	if(b_scene.render().use_simplify()) {
		if(preview) {
			integrator->ao_bounces = get_int(cscene, "ao_bounces");
		}
		else {
			integrator->ao_bounces = get_int(cscene, "ao_bounces_render");
		}
	}
	else {
		integrator->ao_bounces = 0;
	}

	if(integrator->modified(previntegrator))
		integrator->tag_update(scene);
}

/* Film */

void BlenderSync::sync_film()
{
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

	Film *film = scene->film;
	Film prevfilm = *film;

	film->exposure = get_float(cscene, "film_exposure");
	film->filter_type = (FilterType)get_enum(cscene,
	                                         "pixel_filter_type",
	                                         FILTER_NUM_TYPES,
	                                         FILTER_BLACKMAN_HARRIS);
	film->filter_width = (film->filter_type == FILTER_BOX)? 1.0f: get_float(cscene, "filter_width");

	if(b_scene.world()) {
		BL::WorldMistSettings b_mist = b_scene.world().mist_settings();

		film->mist_start = b_mist.start();
		film->mist_depth = b_mist.depth();

		switch(b_mist.falloff()) {
			case BL::WorldMistSettings::falloff_QUADRATIC:
				film->mist_falloff = 2.0f;
				break;
			case BL::WorldMistSettings::falloff_LINEAR:
				film->mist_falloff = 1.0f;
				break;
			case BL::WorldMistSettings::falloff_INVERSE_QUADRATIC:
				film->mist_falloff = 0.5f;
				break;
		}
	}

	if(film->modified(prevfilm))
		film->tag_update(scene);
}

/* Render Layer */

void BlenderSync::sync_view_layer(BL::SpaceView3D& /*b_v3d*/, BL::ViewLayer& b_view_layer)
{
	/* render layer */
	uint layer_override = get_layer(b_engine.layer_override());
	uint view_layers = layer_override ? layer_override : get_layer(b_scene.layers());

	view_layer.name = b_view_layer.name();

	view_layer.holdout_layer = 0;
	view_layer.exclude_layer = 0;

	view_layer.view_layer = view_layers & ~view_layer.exclude_layer;
	view_layer.view_layer |= view_layer.exclude_layer & view_layer.holdout_layer;

	view_layer.layer = (1 << 20) - 1;
	view_layer.layer |= view_layer.holdout_layer;

	view_layer.material_override = PointerRNA_NULL;
	view_layer.use_background_shader = b_view_layer.use_sky();
	view_layer.use_background_ao = b_view_layer.use_ao();
	view_layer.use_surfaces = b_view_layer.use_solid();
	view_layer.use_hair = b_view_layer.use_strand();

	view_layer.bound_samples = false;
	view_layer.samples = 0;
}

/* Images */
void BlenderSync::sync_images()
{
	/* Sync is a convention for this API, but currently it frees unused buffers. */

	const bool is_interface_locked = b_engine.render() &&
	                                 b_engine.render().use_lock_interface();
	if(is_interface_locked == false && BlenderSession::headless == false) {
		/* If interface is not locked, it's possible image is needed for
		 * the display.
		 */
		return;
	}
	/* Free buffers used by images which are not needed for render. */
	BL::BlendData::images_iterator b_image;
	for(b_data.images.begin(b_image);
	    b_image != b_data.images.end();
	    ++b_image)
	{
		/* TODO(sergey): Consider making it an utility function to check
		 * whether image is considered builtin.
		 */
		const bool is_builtin = b_image->packed_file() ||
		                        b_image->source() == BL::Image::source_GENERATED ||
		                        b_image->source() == BL::Image::source_MOVIE ||
		                        b_engine.is_preview();
		if(is_builtin == false) {
			b_image->buffers_free();
		}
		/* TODO(sergey): Free builtin images not used by any shader. */
	}
}

/* Passes */
PassType BlenderSync::get_pass_type(BL::RenderPass& b_pass)
{
	string name = b_pass.name();
#define MAP_PASS(passname, passtype) if(name == passname) return passtype;
	/* NOTE: Keep in sync with defined names from DNA_scene_types.h */
	MAP_PASS("Combined", PASS_COMBINED);
	MAP_PASS("Depth", PASS_DEPTH);
	MAP_PASS("Mist", PASS_MIST);
	MAP_PASS("Normal", PASS_NORMAL);
	MAP_PASS("IndexOB", PASS_OBJECT_ID);
	MAP_PASS("UV", PASS_UV);
	MAP_PASS("Vector", PASS_MOTION);
	MAP_PASS("IndexMA", PASS_MATERIAL_ID);

	MAP_PASS("DiffDir", PASS_DIFFUSE_DIRECT);
	MAP_PASS("GlossDir", PASS_GLOSSY_DIRECT);
	MAP_PASS("TransDir", PASS_TRANSMISSION_DIRECT);
	MAP_PASS("SubsurfaceDir", PASS_SUBSURFACE_DIRECT);
	MAP_PASS("VolumeDir", PASS_VOLUME_DIRECT);

	MAP_PASS("DiffInd", PASS_DIFFUSE_INDIRECT);
	MAP_PASS("GlossInd", PASS_GLOSSY_INDIRECT);
	MAP_PASS("TransInd", PASS_TRANSMISSION_INDIRECT);
	MAP_PASS("SubsurfaceInd", PASS_SUBSURFACE_INDIRECT);
	MAP_PASS("VolumeInd", PASS_VOLUME_INDIRECT);

	MAP_PASS("DiffCol", PASS_DIFFUSE_COLOR);
	MAP_PASS("GlossCol", PASS_GLOSSY_COLOR);
	MAP_PASS("TransCol", PASS_TRANSMISSION_COLOR);
	MAP_PASS("SubsurfaceCol", PASS_SUBSURFACE_COLOR);

	MAP_PASS("Emit", PASS_EMISSION);
	MAP_PASS("Env", PASS_BACKGROUND);
	MAP_PASS("AO", PASS_AO);
	MAP_PASS("Shadow", PASS_SHADOW);

#ifdef __KERNEL_DEBUG__
	MAP_PASS("Debug BVH Traversed Nodes", PASS_BVH_TRAVERSED_NODES);
	MAP_PASS("Debug BVH Traversed Instances", PASS_BVH_TRAVERSED_INSTANCES);
	MAP_PASS("Debug BVH Intersections", PASS_BVH_INTERSECTIONS);
	MAP_PASS("Debug Ray Bounces", PASS_RAY_BOUNCES);
#endif
	MAP_PASS("Debug Render Time", PASS_RENDER_TIME);
#undef MAP_PASS

	return PASS_NONE;
}

int BlenderSync::get_denoising_pass(BL::RenderPass& b_pass)
{
	string name = b_pass.name();
	if(name.substr(0, 10) != "Denoising ") {
		return -1;
	}
	name = name.substr(10);

#define MAP_PASS(passname, offset) if(name == passname) return offset;
	MAP_PASS("Normal", DENOISING_PASS_NORMAL);
	MAP_PASS("Normal Variance", DENOISING_PASS_NORMAL_VAR);
	MAP_PASS("Albedo", DENOISING_PASS_ALBEDO);
	MAP_PASS("Albedo Variance", DENOISING_PASS_ALBEDO_VAR);
	MAP_PASS("Depth", DENOISING_PASS_DEPTH);
	MAP_PASS("Depth Variance", DENOISING_PASS_DEPTH_VAR);
	MAP_PASS("Shadow A", DENOISING_PASS_SHADOW_A);
	MAP_PASS("Shadow B", DENOISING_PASS_SHADOW_B);
	MAP_PASS("Image", DENOISING_PASS_COLOR);
	MAP_PASS("Image Variance", DENOISING_PASS_COLOR_VAR);
#undef MAP_PASS

	return -1;
}

array<Pass> BlenderSync::sync_render_passes(BL::RenderLayer& b_rlay,
                                            BL::ViewLayer& b_view_layer,
                                            const SessionParams &session_params)
{
	array<Pass> passes;
	Pass::add(PASS_COMBINED, passes);

	if(!session_params.device.advanced_shading) {
		return passes;
	}

	/* loop over passes */
	BL::RenderLayer::passes_iterator b_pass_iter;

	for(b_rlay.passes.begin(b_pass_iter); b_pass_iter != b_rlay.passes.end(); ++b_pass_iter) {
		BL::RenderPass b_pass(*b_pass_iter);
		PassType pass_type = get_pass_type(b_pass);

		if(pass_type == PASS_MOTION && scene->integrator->motion_blur)
			continue;
		if(pass_type != PASS_NONE)
			Pass::add(pass_type, passes);
	}

	PointerRNA crp = RNA_pointer_get(&b_view_layer.ptr, "cycles");
	if(get_boolean(crp, "denoising_store_passes") &&
	   get_boolean(crp, "use_denoising"))
	{
		b_engine.add_pass("Denoising Normal",          3, "XYZ", b_view_layer.name().c_str());
		b_engine.add_pass("Denoising Normal Variance", 3, "XYZ", b_view_layer.name().c_str());
		b_engine.add_pass("Denoising Albedo",          3, "RGB", b_view_layer.name().c_str());
		b_engine.add_pass("Denoising Albedo Variance", 3, "RGB", b_view_layer.name().c_str());
		b_engine.add_pass("Denoising Depth",           1, "Z",   b_view_layer.name().c_str());
		b_engine.add_pass("Denoising Depth Variance",  1, "Z",   b_view_layer.name().c_str());
		b_engine.add_pass("Denoising Shadow A",        3, "XYV", b_view_layer.name().c_str());
		b_engine.add_pass("Denoising Shadow B",        3, "XYV", b_view_layer.name().c_str());
		b_engine.add_pass("Denoising Image",           3, "RGB", b_view_layer.name().c_str());
		b_engine.add_pass("Denoising Image Variance",  3, "RGB", b_view_layer.name().c_str());
	}
#ifdef __KERNEL_DEBUG__
	if(get_boolean(crp, "pass_debug_bvh_traversed_nodes")) {
		b_engine.add_pass("Debug BVH Traversed Nodes", 1, "X", b_view_layer.name().c_str());
		Pass::add(PASS_BVH_TRAVERSED_NODES, passes);
	}
	if(get_boolean(crp, "pass_debug_bvh_traversed_instances")) {
		b_engine.add_pass("Debug BVH Traversed Instances", 1, "X", b_view_layer.name().c_str());
		Pass::add(PASS_BVH_TRAVERSED_INSTANCES, passes);
	}
	if(get_boolean(crp, "pass_debug_bvh_intersections")) {
		b_engine.add_pass("Debug BVH Intersections", 1, "X", b_view_layer.name().c_str());
		Pass::add(PASS_BVH_INTERSECTIONS, passes);
	}
	if(get_boolean(crp, "pass_debug_ray_bounces")) {
		b_engine.add_pass("Debug Ray Bounces", 1, "X", b_view_layer.name().c_str());
		Pass::add(PASS_RAY_BOUNCES, passes);
	}
#endif
	if(get_boolean(crp, "pass_debug_render_time")) {
		b_engine.add_pass("Debug Render Time", 1, "X", b_view_layer.name().c_str());
		Pass::add(PASS_RENDER_TIME, passes);
	}
	if(get_boolean(crp, "use_pass_volume_direct")) {
		b_engine.add_pass("VolumeDir", 3, "RGB", b_view_layer.name().c_str());
		Pass::add(PASS_VOLUME_DIRECT, passes);
	}
	if(get_boolean(crp, "use_pass_volume_indirect")) {
		b_engine.add_pass("VolumeInd", 3, "RGB", b_view_layer.name().c_str());
		Pass::add(PASS_VOLUME_INDIRECT, passes);
	}

	return passes;
}

void BlenderSync::free_data_after_sync(BL::Depsgraph& b_depsgraph)
{
	/* When viewport display is not needed during render we can force some
	 * caches to be releases from blender side in order to reduce peak memory
	 * footprint during synchronization process.
	 */
	const bool is_interface_locked = b_engine.render() &&
	                                 b_engine.render().use_lock_interface();
	const bool can_free_caches = BlenderSession::headless || is_interface_locked;
	if (!can_free_caches) {
		return;
	}
	/* TODO(sergey): We can actually remove the whole dependency graph,
	 * but that will need some API support first.
	 */
	BL::Depsgraph::objects_iterator b_ob;
	for(b_depsgraph.objects.begin(b_ob);
	    b_ob != b_depsgraph.objects.end();
	    ++b_ob)
	{
		b_ob->cache_release();
	}
}

/* Scene Parameters */

SceneParams BlenderSync::get_scene_params(BL::Scene& b_scene,
                                          bool background)
{
	BL::RenderSettings r = b_scene.render();
	SceneParams params;
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
	const bool shadingsystem = RNA_boolean_get(&cscene, "shading_system");

	if(shadingsystem == 0)
		params.shadingsystem = SHADINGSYSTEM_SVM;
	else if(shadingsystem == 1)
		params.shadingsystem = SHADINGSYSTEM_OSL;

	if(background || DebugFlags().viewport_static_bvh)
		params.bvh_type = SceneParams::BVH_STATIC;
	else
		params.bvh_type = SceneParams::BVH_DYNAMIC;

	params.use_bvh_spatial_split = RNA_boolean_get(&cscene, "debug_use_spatial_splits");
	params.use_bvh_unaligned_nodes = RNA_boolean_get(&cscene, "debug_use_hair_bvh");
	params.num_bvh_time_steps = RNA_int_get(&cscene, "debug_bvh_time_steps");

	if(background && params.shadingsystem != SHADINGSYSTEM_OSL)
		params.persistent_data = r.use_persistent_data();
	else
		params.persistent_data = false;

	int texture_limit;
	if(background) {
		texture_limit = RNA_enum_get(&cscene, "texture_limit_render");
	}
	else {
		texture_limit = RNA_enum_get(&cscene, "texture_limit");
	}
	if(texture_limit > 0 && b_scene.render().use_simplify()) {
		params.texture_limit = 1 << (texture_limit + 6);
	}
	else {
		params.texture_limit = 0;
	}

	params.bvh_layout = DebugFlags().cpu.bvh_layout;

	return params;
}

/* Session Parameters */

bool BlenderSync::get_session_pause(BL::Scene& b_scene, bool background)
{
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
	return (background)? false: get_boolean(cscene, "preview_pause");
}

SessionParams BlenderSync::get_session_params(BL::RenderEngine& b_engine,
                                              BL::UserPreferences& b_userpref,
                                              BL::Scene& b_scene,
                                              bool background)
{
	SessionParams params;
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

	/* feature set */
	params.experimental = (get_enum(cscene, "feature_set") != 0);

	/* threads */
	BL::RenderSettings b_r = b_scene.render();
	if(b_r.threads_mode() == BL::RenderSettings::threads_mode_FIXED)
		params.threads = b_r.threads();
	else
		params.threads = 0;

	/* Background */
	params.background = background;

	/* device type */
	vector<DeviceInfo>& devices = Device::available_devices();

	/* device default CPU */
	foreach(DeviceInfo& device, devices) {
		if(device.type == DEVICE_CPU) {
			params.device = device;
			break;
		}
	}

	if(get_enum(cscene, "device") == 2) {
		/* find network device */
		foreach(DeviceInfo& info, devices)
			if(info.type == DEVICE_NETWORK)
				params.device = info;
	}
	else if(get_enum(cscene, "device") == 1) {
		PointerRNA b_preferences;

		BL::UserPreferences::addons_iterator b_addon_iter;
		for(b_userpref.addons.begin(b_addon_iter); b_addon_iter != b_userpref.addons.end(); ++b_addon_iter) {
			if(b_addon_iter->module() == "cycles") {
				b_preferences = b_addon_iter->preferences().ptr;
				break;
			}
		}

		enum ComputeDevice {
			COMPUTE_DEVICE_CPU = 0,
			COMPUTE_DEVICE_CUDA = 1,
			COMPUTE_DEVICE_OPENCL = 2,
			COMPUTE_DEVICE_NUM = 3,
		};

		ComputeDevice compute_device = (ComputeDevice)get_enum(b_preferences,
		                                                       "compute_device_type",
		                                                       COMPUTE_DEVICE_NUM,
		                                                       COMPUTE_DEVICE_CPU);

		if(compute_device != COMPUTE_DEVICE_CPU) {
			vector<DeviceInfo> used_devices;
			RNA_BEGIN(&b_preferences, device, "devices") {
				ComputeDevice device_type = (ComputeDevice)get_enum(device,
				                                                    "type",
				                                                    COMPUTE_DEVICE_NUM,
				                                                    COMPUTE_DEVICE_CPU);

				if(get_boolean(device, "use") &&
				   (device_type == compute_device || device_type == COMPUTE_DEVICE_CPU)) {
					string id = get_string(device, "id");
					foreach(DeviceInfo& info, devices) {
						if(info.id == id) {
							used_devices.push_back(info);
							break;
						}
					}
				}
			} RNA_END

			if(used_devices.size() == 1) {
				params.device = used_devices[0];
			}
			else if(used_devices.size() > 1) {
				params.device = Device::get_multi_device(used_devices,
				                                         params.threads,
				                                         params.background);
			}
			/* Else keep using the CPU device that was set before. */
		}
	}

	/* samples */
	int samples = get_int(cscene, "samples");
	int aa_samples = get_int(cscene, "aa_samples");
	int preview_samples = get_int(cscene, "preview_samples");
	int preview_aa_samples = get_int(cscene, "preview_aa_samples");

	if(get_boolean(cscene, "use_square_samples")) {
		aa_samples = aa_samples * aa_samples;
		preview_aa_samples = preview_aa_samples * preview_aa_samples;

		samples = samples * samples;
		preview_samples = preview_samples * preview_samples;
	}

	if(get_enum(cscene, "progressive") == 0) {
		if(background) {
			params.samples = aa_samples;
		}
		else {
			params.samples = preview_aa_samples;
			if(params.samples == 0)
				params.samples = INT_MAX;
		}
	}
	else {
		if(background) {
			params.samples = samples;
		}
		else {
			params.samples = preview_samples;
			if(params.samples == 0)
				params.samples = INT_MAX;
		}
	}

	/* tiles */
	if(params.device.type != DEVICE_CPU && !background) {
		/* currently GPU could be much slower than CPU when using tiles,
		 * still need to be investigated, but meanwhile make it possible
		 * to work in viewport smoothly
		 */
		int debug_tile_size = get_int(cscene, "debug_tile_size");

		params.tile_size = make_int2(debug_tile_size, debug_tile_size);
	}
	else {
		int tile_x = b_engine.tile_x();
		int tile_y = b_engine.tile_y();

		params.tile_size = make_int2(tile_x, tile_y);
	}

	if((BlenderSession::headless == false) && background) {
		params.tile_order = (TileOrder)get_enum(cscene, "tile_order");
	}
	else {
		params.tile_order = TILE_BOTTOM_TO_TOP;
	}

	/* other parameters */
	params.start_resolution = get_int(cscene, "preview_start_resolution");
	params.pixel_size = b_engine.get_preview_pixel_size(b_scene);

	/* other parameters */
	params.cancel_timeout = (double)get_float(cscene, "debug_cancel_timeout");
	params.reset_timeout = (double)get_float(cscene, "debug_reset_timeout");
	params.text_timeout = (double)get_float(cscene, "debug_text_timeout");

	/* progressive refine */
	params.progressive_refine = (b_engine.is_preview() ||
	                             get_boolean(cscene, "use_progressive_refine")) &&
	                            !b_r.use_save_buffers();

	if(params.progressive_refine) {
		BL::Scene::view_layers_iterator b_view_layer;
		for(b_scene.view_layers.begin(b_view_layer); b_view_layer != b_scene.view_layers.end(); ++b_view_layer) {
			PointerRNA crl = RNA_pointer_get(&b_view_layer->ptr, "cycles");
			if(get_boolean(crl, "use_denoising")) {
				params.progressive_refine = false;
			}
		}
	}

	if(background) {
		if(params.progressive_refine)
			params.progressive = true;
		else
			params.progressive = false;

		params.start_resolution = INT_MAX;
		params.pixel_size = 1;
	}
	else
		params.progressive = true;

	/* shading system - scene level needs full refresh */
	const bool shadingsystem = RNA_boolean_get(&cscene, "shading_system");

	if(shadingsystem == 0)
		params.shadingsystem = SHADINGSYSTEM_SVM;
	else if(shadingsystem == 1)
		params.shadingsystem = SHADINGSYSTEM_OSL;

	/* color managagement */
	params.display_buffer_linear = GLEW_ARB_half_float_pixel &&
	                               b_engine.support_display_space_shader(b_scene);

	if(b_engine.is_preview()) {
		/* For preview rendering we're using same timeout as
		 * blender's job update.
		 */
		params.progressive_update_timeout = 0.1;
	}

	return params;
}

CCL_NAMESPACE_END
