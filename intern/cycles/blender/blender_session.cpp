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

#include <stdlib.h>

#include "background.h"
#include "buffers.h"
#include "camera.h"
#include "device.h"
#include "integrator.h"
#include "film.h"
#include "light.h"
#include "mesh.h"
#include "object.h"
#include "scene.h"
#include "session.h"
#include "shader.h"

#include "util_color.h"
#include "util_foreach.h"
#include "util_function.h"
#include "util_progress.h"
#include "util_time.h"

#include "blender_sync.h"
#include "blender_session.h"
#include "blender_util.h"

CCL_NAMESPACE_BEGIN

BlenderSession::BlenderSession(BL::RenderEngine b_engine_, BL::UserPreferences b_userpref_,
	BL::BlendData b_data_, BL::Scene b_scene_)
: b_engine(b_engine_), b_userpref(b_userpref_), b_data(b_data_), b_render(b_engine_.render()), b_scene(b_scene_),
  b_v3d(PointerRNA_NULL), b_rv3d(PointerRNA_NULL), python_thread_state(NULL)
{
	/* offline render */

	width = render_resolution_x(b_render);
	height = render_resolution_y(b_render);

	background = true;
	last_redraw_time = 0.0;
	start_resize_time = 0.0;
}

BlenderSession::BlenderSession(BL::RenderEngine b_engine_, BL::UserPreferences b_userpref_,
	BL::BlendData b_data_, BL::Scene b_scene_,
	BL::SpaceView3D b_v3d_, BL::RegionView3D b_rv3d_, int width_, int height_)
: b_engine(b_engine_), b_userpref(b_userpref_), b_data(b_data_), b_render(b_scene_.render()), b_scene(b_scene_),
  b_v3d(b_v3d_), b_rv3d(b_rv3d_), python_thread_state(NULL)
{
	/* 3d view render */

	width = width_;
	height = height_;
	background = false;
	last_redraw_time = 0.0;
	start_resize_time = 0.0;
}

BlenderSession::~BlenderSession()
{
	free_session();
}

void BlenderSession::create()
{
	create_session();

	if(b_v3d)
		session->start();
}

void BlenderSession::create_session()
{
	SceneParams scene_params = BlenderSync::get_scene_params(b_scene, background);
	SessionParams session_params = BlenderSync::get_session_params(b_engine, b_userpref, b_scene, background);
	bool session_pause = BlenderSync::get_session_pause(b_scene, background);

	/* reset status/progress */
	last_status = "";
	last_error = "";
	last_progress = -1.0f;
	start_resize_time = 0.0;

	/* create scene */
	scene = new Scene(scene_params, session_params.device);

	/* setup callbacks for builtin image support */
	scene->image_manager->builtin_image_info_cb = function_bind(&BlenderSession::builtin_image_info, this, _1, _2, _3, _4, _5, _6, _7);
	scene->image_manager->builtin_image_pixels_cb = function_bind(&BlenderSession::builtin_image_pixels, this, _1, _2, _3);
	scene->image_manager->builtin_image_float_pixels_cb = function_bind(&BlenderSession::builtin_image_float_pixels, this, _1, _2, _3);

	/* create session */
	session = new Session(session_params);
	session->scene = scene;
	session->progress.set_update_callback(function_bind(&BlenderSession::tag_redraw, this));
	session->progress.set_cancel_callback(function_bind(&BlenderSession::test_cancel, this));
	session->set_pause(session_pause);

	/* create sync */
	sync = new BlenderSync(b_engine, b_data, b_scene, scene, !background, session->progress, session_params.device.type == DEVICE_CPU);

	if(b_v3d) {
		if(session_pause == false) {
			/* full data sync */
			sync->sync_view(b_v3d, b_rv3d, width, height);
			sync->sync_data(b_v3d, b_engine.camera_override(), &python_thread_state);
		}
	}
	else {
		/* for final render we will do full data sync per render layer, only
		 * do some basic syncing here, no objects or materials for speed */
		sync->sync_render_layers(b_v3d, NULL);
		sync->sync_integrator();
		sync->sync_camera(b_render, b_engine.camera_override(), width, height);
	}

	/* set buffer parameters */
	BufferParams buffer_params = BlenderSync::get_buffer_params(b_render, b_scene, b_v3d, b_rv3d, scene->camera, width, height);
	session->reset(buffer_params, session_params.samples);

	b_engine.use_highlight_tiles(session_params.progressive_refine == false);
}

void BlenderSession::reset_session(BL::BlendData b_data_, BL::Scene b_scene_)
{
	b_data = b_data_;
	b_render = b_engine.render();
	b_scene = b_scene_;

	SceneParams scene_params = BlenderSync::get_scene_params(b_scene, background);
	SessionParams session_params = BlenderSync::get_session_params(b_engine, b_userpref, b_scene, background);

	width = render_resolution_x(b_render);
	height = render_resolution_y(b_render);

	if(scene->params.modified(scene_params) ||
	   session->params.modified(session_params) ||
	   !scene_params.persistent_data)
	{
		/* if scene or session parameters changed, it's easier to simply re-create
		 * them rather than trying to distinguish which settings need to be updated
		 */

		delete session;

		create_session();

		return;
	}

	session->progress.reset();
	scene->reset();

	session->tile_manager.set_tile_order(session_params.tile_order);

	/* peak memory usage should show current render peak, not peak for all renders
	 * made by this render session
	 */
	session->stats.mem_peak = session->stats.mem_used;

	/* sync object should be re-created */
	sync = new BlenderSync(b_engine, b_data, b_scene, scene, !background, session->progress, session_params.device.type == DEVICE_CPU);

	/* for final render we will do full data sync per render layer, only
	 * do some basic syncing here, no objects or materials for speed */
	sync->sync_render_layers(b_v3d, NULL);
	sync->sync_integrator();
	sync->sync_camera(b_render, b_engine.camera_override(), width, height);

	BufferParams buffer_params = BlenderSync::get_buffer_params(b_render, b_scene, PointerRNA_NULL, PointerRNA_NULL, scene->camera, width, height);
	session->reset(buffer_params, session_params.samples);

	b_engine.use_highlight_tiles(session_params.progressive_refine == false);

	/* reset time */
	start_resize_time = 0.0;
}

void BlenderSession::free_session()
{
	if(sync)
		delete sync;

	delete session;
}

static PassType get_pass_type(BL::RenderPass b_pass)
{
	switch(b_pass.type()) {
		case BL::RenderPass::type_COMBINED:
			return PASS_COMBINED;

		case BL::RenderPass::type_Z:
			return PASS_DEPTH;
		case BL::RenderPass::type_MIST:
			return PASS_MIST;
		case BL::RenderPass::type_NORMAL:
			return PASS_NORMAL;
		case BL::RenderPass::type_OBJECT_INDEX:
			return PASS_OBJECT_ID;
		case BL::RenderPass::type_UV:
			return PASS_UV;
		case BL::RenderPass::type_VECTOR:
			return PASS_MOTION;
		case BL::RenderPass::type_MATERIAL_INDEX:
			return PASS_MATERIAL_ID;

		case BL::RenderPass::type_DIFFUSE_DIRECT:
			return PASS_DIFFUSE_DIRECT;
		case BL::RenderPass::type_GLOSSY_DIRECT:
			return PASS_GLOSSY_DIRECT;
		case BL::RenderPass::type_TRANSMISSION_DIRECT:
			return PASS_TRANSMISSION_DIRECT;
		case BL::RenderPass::type_SUBSURFACE_DIRECT:
			return PASS_SUBSURFACE_DIRECT;

		case BL::RenderPass::type_DIFFUSE_INDIRECT:
			return PASS_DIFFUSE_INDIRECT;
		case BL::RenderPass::type_GLOSSY_INDIRECT:
			return PASS_GLOSSY_INDIRECT;
		case BL::RenderPass::type_TRANSMISSION_INDIRECT:
			return PASS_TRANSMISSION_INDIRECT;
		case BL::RenderPass::type_SUBSURFACE_INDIRECT:
			return PASS_SUBSURFACE_INDIRECT;

		case BL::RenderPass::type_DIFFUSE_COLOR:
			return PASS_DIFFUSE_COLOR;
		case BL::RenderPass::type_GLOSSY_COLOR:
			return PASS_GLOSSY_COLOR;
		case BL::RenderPass::type_TRANSMISSION_COLOR:
			return PASS_TRANSMISSION_COLOR;
		case BL::RenderPass::type_SUBSURFACE_COLOR:
			return PASS_SUBSURFACE_COLOR;

		case BL::RenderPass::type_EMIT:
			return PASS_EMISSION;
		case BL::RenderPass::type_ENVIRONMENT:
			return PASS_BACKGROUND;
		case BL::RenderPass::type_AO:
			return PASS_AO;
		case BL::RenderPass::type_SHADOW:
			return PASS_SHADOW;

		case BL::RenderPass::type_DIFFUSE:
		case BL::RenderPass::type_COLOR:
		case BL::RenderPass::type_REFRACTION:
		case BL::RenderPass::type_SPECULAR:
		case BL::RenderPass::type_REFLECTION:
			return PASS_NONE;
#ifdef WITH_CYCLES_DEBUG
		case BL::RenderPass::type_DEBUG:
		{
			if(b_pass.debug_type() == BL::RenderPass::debug_type_BVH_TRAVERSAL_STEPS)
				return PASS_BVH_TRAVERSAL_STEPS;
			break;
		}
#endif
	}
	
	return PASS_NONE;
}

static ShaderEvalType get_shader_type(const string& pass_type)
{
	const char *shader_type = pass_type.c_str();

	/* data passes */
	if(strcmp(shader_type, "NORMAL")==0)
		return SHADER_EVAL_NORMAL;
	else if(strcmp(shader_type, "UV")==0)
		return SHADER_EVAL_UV;
	else if(strcmp(shader_type, "DIFFUSE_COLOR")==0)
		return SHADER_EVAL_DIFFUSE_COLOR;
	else if(strcmp(shader_type, "GLOSSY_COLOR")==0)
		return SHADER_EVAL_GLOSSY_COLOR;
	else if(strcmp(shader_type, "TRANSMISSION_COLOR")==0)
		return SHADER_EVAL_TRANSMISSION_COLOR;
	else if(strcmp(shader_type, "SUBSURFACE_COLOR")==0)
		return SHADER_EVAL_SUBSURFACE_COLOR;
	else if(strcmp(shader_type, "EMIT")==0)
		return SHADER_EVAL_EMISSION;

	/* light passes */
	else if(strcmp(shader_type, "AO")==0)
		return SHADER_EVAL_AO;
	else if(strcmp(shader_type, "COMBINED")==0)
		return SHADER_EVAL_COMBINED;
	else if(strcmp(shader_type, "SHADOW")==0)
		return SHADER_EVAL_SHADOW;
	else if(strcmp(shader_type, "DIFFUSE_DIRECT")==0)
		return SHADER_EVAL_DIFFUSE_DIRECT;
	else if(strcmp(shader_type, "GLOSSY_DIRECT")==0)
		return SHADER_EVAL_GLOSSY_DIRECT;
	else if(strcmp(shader_type, "TRANSMISSION_DIRECT")==0)
		return SHADER_EVAL_TRANSMISSION_DIRECT;
	else if(strcmp(shader_type, "SUBSURFACE_DIRECT")==0)
		return SHADER_EVAL_SUBSURFACE_DIRECT;
	else if(strcmp(shader_type, "DIFFUSE_INDIRECT")==0)
		return SHADER_EVAL_DIFFUSE_INDIRECT;
	else if(strcmp(shader_type, "GLOSSY_INDIRECT")==0)
		return SHADER_EVAL_GLOSSY_INDIRECT;
	else if(strcmp(shader_type, "TRANSMISSION_INDIRECT")==0)
		return SHADER_EVAL_TRANSMISSION_INDIRECT;
	else if(strcmp(shader_type, "SUBSURFACE_INDIRECT")==0)
		return SHADER_EVAL_SUBSURFACE_INDIRECT;

	/* extra */
	else if(strcmp(shader_type, "ENVIRONMENT")==0)
		return SHADER_EVAL_ENVIRONMENT;

	else
		return SHADER_EVAL_BAKE;
}

static BL::RenderResult begin_render_result(BL::RenderEngine b_engine, int x, int y, int w, int h, const char *layername)
{
	return b_engine.begin_result(x, y, w, h, layername);
}

static void end_render_result(BL::RenderEngine b_engine, BL::RenderResult b_rr, bool cancel, bool do_merge_results)
{
	b_engine.end_result(b_rr, (int)cancel, (int)do_merge_results);
}

void BlenderSession::do_write_update_render_tile(RenderTile& rtile, bool do_update_only)
{
	BufferParams& params = rtile.buffers->params;
	int x = params.full_x - session->tile_manager.params.full_x;
	int y = params.full_y - session->tile_manager.params.full_y;
	int w = params.width;
	int h = params.height;

	/* get render result */
	BL::RenderResult b_rr = begin_render_result(b_engine, x, y, w, h, b_rlay_name.c_str());

	/* can happen if the intersected rectangle gives 0 width or height */
	if (b_rr.ptr.data == NULL) {
		return;
	}

	BL::RenderResult::layers_iterator b_single_rlay;
	b_rr.layers.begin(b_single_rlay);

	/* layer will be missing if it was disabled in the UI */
	if(b_single_rlay == b_rr.layers.end())
		return;

	BL::RenderLayer b_rlay = *b_single_rlay;

	if (do_update_only) {
		/* update only needed */

		if (rtile.sample != 0) {
			/* sample would be zero at initial tile update, which is only needed
			 * to tag tile form blender side as IN PROGRESS for proper highlight
			 * no buffers should be sent to blender yet
			 */
			update_render_result(b_rr, b_rlay, rtile);
		}

		end_render_result(b_engine, b_rr, true, true);
	}
	else {
		/* write result */
		write_render_result(b_rr, b_rlay, rtile);
		end_render_result(b_engine, b_rr, false, true);
	}
}

void BlenderSession::write_render_tile(RenderTile& rtile)
{
	do_write_update_render_tile(rtile, false);
}

void BlenderSession::update_render_tile(RenderTile& rtile)
{
	/* use final write for preview renders, otherwise render result wouldn't be
	 * be updated in blender side
	 * would need to be investigated a bit further, but for now shall be fine
	 */
	if (!b_engine.is_preview())
		do_write_update_render_tile(rtile, true);
	else
		do_write_update_render_tile(rtile, false);
}

void BlenderSession::render()
{
	/* set callback to write out render results */
	session->write_render_tile_cb = function_bind(&BlenderSession::write_render_tile, this, _1);
	session->update_render_tile_cb = function_bind(&BlenderSession::update_render_tile, this, _1);

	/* get buffer parameters */
	SessionParams session_params = BlenderSync::get_session_params(b_engine, b_userpref, b_scene, background);
	BufferParams buffer_params = BlenderSync::get_buffer_params(b_render, b_scene, b_v3d, b_rv3d, scene->camera, width, height);

	/* render each layer */
	BL::RenderSettings r = b_scene.render();
	BL::RenderSettings::layers_iterator b_iter;
	
	for(r.layers.begin(b_iter); b_iter != r.layers.end(); ++b_iter) {
		b_rlay_name = b_iter->name();

		/* temporary render result to find needed passes */
		BL::RenderResult b_rr = begin_render_result(b_engine, 0, 0, 1, 1, b_rlay_name.c_str());
		BL::RenderResult::layers_iterator b_single_rlay;
		b_rr.layers.begin(b_single_rlay);

		/* layer will be missing if it was disabled in the UI */
		if(b_single_rlay == b_rr.layers.end()) {
			end_render_result(b_engine, b_rr, true, false);
			continue;
		}

		BL::RenderLayer b_rlay = *b_single_rlay;

		/* add passes */
		vector<Pass> passes;
		Pass::add(PASS_COMBINED, passes);
#ifdef WITH_CYCLES_DEBUG
		Pass::add(PASS_BVH_TRAVERSAL_STEPS, passes);
#endif

		if(session_params.device.advanced_shading) {

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
		}

		/* free result without merging */
		end_render_result(b_engine, b_rr, true, false);

		buffer_params.passes = passes;
		scene->film->pass_alpha_threshold = b_iter->pass_alpha_threshold();
		scene->film->tag_passes_update(scene, passes);
		scene->film->tag_update(scene);
		scene->integrator->tag_update(scene);

		/* update scene */
		sync->sync_camera(b_render, b_engine.camera_override(), width, height);
		sync->sync_data(b_v3d, b_engine.camera_override(), &python_thread_state, b_rlay_name.c_str());

		/* update number of samples per layer */
		int samples = sync->get_layer_samples();
		bool bound_samples = sync->get_layer_bound_samples();

		if(samples != 0 && (!bound_samples || (samples < session_params.samples)))
			session->reset(buffer_params, samples);
		else
			session->reset(buffer_params, session_params.samples);

		/* render */
		session->start();
		session->wait();

		if(session->progress.get_cancel())
			break;
	}

	/* clear callback */
	session->write_render_tile_cb = NULL;
	session->update_render_tile_cb = NULL;

	/* free all memory used (host and device), so we wouldn't leave render
	 * engine with extra memory allocated
	 */

	session->device_free();

	delete sync;
	sync = NULL;
}

static void populate_bake_data(BakeData *data, BL::BakePixel pixel_array, const int num_pixels)
{
	BL::BakePixel bp = pixel_array;

	int i;
	for(i=0; i < num_pixels; i++) {
		data->set(i, bp.primitive_id(), bp.uv(), bp.du_dx(), bp.du_dy(), bp.dv_dx(), bp.dv_dy());
		bp = bp.next();
	}
}

void BlenderSession::bake(BL::Object b_object, const string& pass_type, BL::BakePixel pixel_array, const size_t num_pixels, const int depth, float result[])
{
	ShaderEvalType shader_type = get_shader_type(pass_type);
	size_t object_index = OBJECT_NONE;
	int tri_offset = 0;

	/* ensure kernels are loaded before we do any scene updates */
	session->load_kernels();

	if(session->progress.get_cancel())
		return;

	if(shader_type == SHADER_EVAL_UV) {
		/* force UV to be available */
		Pass::add(PASS_UV, scene->film->passes);
	}

	if(BakeManager::is_light_pass(shader_type)) {
		/* force use_light_pass to be true */
		Pass::add(PASS_LIGHT, scene->film->passes);
	}

	/* create device and update scene */
	scene->film->tag_update(scene);
	scene->integrator->tag_update(scene);

	/* update scene */
	sync->sync_camera(b_render, b_engine.camera_override(), width, height);
	sync->sync_data(b_v3d, b_engine.camera_override(), &python_thread_state);

	/* get buffer parameters */
	SessionParams session_params = BlenderSync::get_session_params(b_engine, b_userpref, b_scene, background);
	BufferParams buffer_params = BlenderSync::get_buffer_params(b_render, b_scene, b_v3d, b_rv3d, scene->camera, width, height);

	scene->bake_manager->set_shader_limit((size_t)b_engine.tile_x(), (size_t)b_engine.tile_y());
	scene->bake_manager->set_baking(true);

	/* set number of samples */
	session->tile_manager.set_samples(session_params.samples);
	session->reset(buffer_params, session_params.samples);
	session->update_scene();

	/* find object index. todo: is arbitrary - copied from mesh_displace.cpp */
	for(size_t i = 0; i < scene->objects.size(); i++) {
		if(strcmp(scene->objects[i]->name.c_str(), b_object.name().c_str()) == 0) {
			object_index = i;
			tri_offset = scene->objects[i]->mesh->tri_offset;
			break;
		}
	}

	/* when used, non-instanced convention: object = ~object */
	int object = ~object_index;

	BakeData *bake_data = scene->bake_manager->init(object, tri_offset, num_pixels);

	populate_bake_data(bake_data, pixel_array, num_pixels);

	/* set number of samples */
	session->tile_manager.set_samples(session_params.samples);
	session->reset(buffer_params, session_params.samples);
	session->update_scene();

	session->progress.set_update_callback(function_bind(&BlenderSession::update_bake_progress, this));

	scene->bake_manager->bake(scene->device, &scene->dscene, scene, session->progress, shader_type, bake_data, result);

	/* free all memory used (host and device), so we wouldn't leave render
	 * engine with extra memory allocated
	 */

	session->device_free();

	delete sync;
	sync = NULL;
}

void BlenderSession::do_write_update_render_result(BL::RenderResult b_rr, BL::RenderLayer b_rlay, RenderTile& rtile, bool do_update_only)
{
	RenderBuffers *buffers = rtile.buffers;

	/* copy data from device */
	if(!buffers->copy_from_device())
		return;

	BufferParams& params = buffers->params;
	float exposure = scene->film->exposure;

	vector<float> pixels(params.width*params.height*4);

	if (!do_update_only) {
		/* copy each pass */
		BL::RenderLayer::passes_iterator b_iter;

		for(b_rlay.passes.begin(b_iter); b_iter != b_rlay.passes.end(); ++b_iter) {
			BL::RenderPass b_pass(*b_iter);

			/* find matching pass type */
			PassType pass_type = get_pass_type(b_pass);
			int components = b_pass.channels();

			/* copy pixels */
			if(!buffers->get_pass_rect(pass_type, exposure, rtile.sample, components, &pixels[0]))
				memset(&pixels[0], 0, pixels.size()*sizeof(float));

			b_pass.rect(&pixels[0]);
		}
	}

	/* copy combined pass */
	if(buffers->get_pass_rect(PASS_COMBINED, exposure, rtile.sample, 4, &pixels[0]))
		b_rlay.rect(&pixels[0]);

	/* tag result as updated */
	b_engine.update_result(b_rr);
}

void BlenderSession::write_render_result(BL::RenderResult b_rr, BL::RenderLayer b_rlay, RenderTile& rtile)
{
	do_write_update_render_result(b_rr, b_rlay, rtile, false);
}

void BlenderSession::update_render_result(BL::RenderResult b_rr, BL::RenderLayer b_rlay, RenderTile& rtile)
{
	do_write_update_render_result(b_rr, b_rlay, rtile, true);
}

void BlenderSession::synchronize()
{
	/* only used for viewport render */
	if(!b_v3d)
		return;

	/* on session/scene parameter changes, we recreate session entirely */
	SceneParams scene_params = BlenderSync::get_scene_params(b_scene, background);
	SessionParams session_params = BlenderSync::get_session_params(b_engine, b_userpref, b_scene, background);
	bool session_pause = BlenderSync::get_session_pause(b_scene, background);

	if(session->params.modified(session_params) ||
	   scene->params.modified(scene_params))
	{
		free_session();
		create_session();
		session->start();
		return;
	}

	/* increase samples, but never decrease */
	session->set_samples(session_params.samples);
	session->set_pause(session_pause);

	/* copy recalc flags, outside of mutex so we can decide to do the real
	 * synchronization at a later time to not block on running updates */
	sync->sync_recalc();

	/* don't do synchronization if on pause */
	if(session_pause) {
		tag_update();
		return;
	}

	/* try to acquire mutex. if we don't want to or can't, come back later */
	if(!session->ready_to_reset() || !session->scene->mutex.try_lock()) {
		tag_update();
		return;
	}

	/* data and camera synchronize */
	sync->sync_data(b_v3d, b_engine.camera_override(), &python_thread_state);

	if(b_rv3d)
		sync->sync_view(b_v3d, b_rv3d, width, height);
	else
		sync->sync_camera(b_render, b_engine.camera_override(), width, height);

	/* unlock */
	session->scene->mutex.unlock();

	/* reset if needed */
	if(scene->need_reset()) {
		BufferParams buffer_params = BlenderSync::get_buffer_params(b_render, b_scene, b_v3d, b_rv3d, scene->camera, width, height);
		session->reset(buffer_params, session_params.samples);

		/* reset time */
		start_resize_time = 0.0;
	}
}

bool BlenderSession::draw(int w, int h)
{
	/* pause in redraw in case update is not being called due to final render */
	session->set_pause(BlenderSync::get_session_pause(b_scene, background));

	/* before drawing, we verify camera and viewport size changes, because
	 * we do not get update callbacks for those, we must detect them here */
	if(session->ready_to_reset()) {
		bool reset = false;

		/* if dimensions changed, reset */
		if(width != w || height != h) {
			if(start_resize_time == 0.0) {
				/* don't react immediately to resizes to avoid flickery resizing
				 * of the viewport, and some window managers changing the window
				 * size temporarily on unminimize */
				start_resize_time = time_dt();
				tag_redraw();
			}
			else if(time_dt() - start_resize_time < 0.2) {
				tag_redraw();
			}
			else {
				width = w;
				height = h;
				reset = true;
			}
		}

		/* try to acquire mutex. if we can't, come back later */
		if(!session->scene->mutex.try_lock()) {
			tag_update();
		}
		else {
			/* update camera from 3d view */

			sync->sync_view(b_v3d, b_rv3d, width, height);

			if(scene->camera->need_update)
				reset = true;

			session->scene->mutex.unlock();
		}

		/* reset if requested */
		if(reset) {
			SessionParams session_params = BlenderSync::get_session_params(b_engine, b_userpref, b_scene, background);
			BufferParams buffer_params = BlenderSync::get_buffer_params(b_render, b_scene, b_v3d, b_rv3d, scene->camera, width, height);
			bool session_pause = BlenderSync::get_session_pause(b_scene, background);

			if(session_pause == false) {
				session->reset(buffer_params, session_params.samples);
				start_resize_time = 0.0;
			}
		}
	}
	else {
		tag_update();
	}

	/* update status and progress for 3d view draw */
	update_status_progress();

	/* draw */
	BufferParams buffer_params = BlenderSync::get_buffer_params(b_render, b_scene, b_v3d, b_rv3d, scene->camera, width, height);
	DeviceDrawParams draw_params;

	if(session->params.display_buffer_linear) {
		draw_params.bind_display_space_shader_cb = function_bind(&BL::RenderEngine::bind_display_space_shader, &b_engine, b_scene);
		draw_params.unbind_display_space_shader_cb = function_bind(&BL::RenderEngine::unbind_display_space_shader, &b_engine);
	}

	return !session->draw(buffer_params, draw_params);
}

void BlenderSession::get_status(string& status, string& substatus)
{
	session->progress.get_status(status, substatus);
}

void BlenderSession::get_progress(float& progress, double& total_time)
{
	double tile_time;
	int tile, sample, samples_per_tile;
	int tile_total = session->tile_manager.state.num_tiles;

	session->progress.get_tile(tile, total_time, tile_time);

	sample = session->progress.get_sample();
	samples_per_tile = session->tile_manager.num_samples;

	if(samples_per_tile && tile_total)
		progress = ((float)sample / (float)(tile_total * samples_per_tile));
	else
		progress = 0.0;
}

void BlenderSession::update_bake_progress()
{
	float progress;
	int sample, samples_per_task, parts_total;

	sample = session->progress.get_sample();
	samples_per_task = scene->bake_manager->num_samples;
	parts_total = scene->bake_manager->num_parts;

	if(samples_per_task)
		progress = ((float)sample / (float)(parts_total * samples_per_task));
	else
		progress = 0.0;

	if(progress != last_progress) {
		b_engine.update_progress(progress);
		last_progress = progress;
	}
}

void BlenderSession::update_status_progress()
{
	string timestatus, status, substatus;
	string scene = "";
	float progress;
	double total_time, remaining_time = 0;
	char time_str[128];
	float mem_used = (float)session->stats.mem_used / 1024.0f / 1024.0f;
	float mem_peak = (float)session->stats.mem_peak / 1024.0f / 1024.0f;
	int samples = session->tile_manager.state.sample + 1;
	int total_samples = session->tile_manager.num_samples;

	get_status(status, substatus);
	get_progress(progress, total_time);

	if(background) {
		if(progress > 0)
			remaining_time = (1.0 - (double)progress) * (total_time / (double)progress);

		scene += " | " + b_scene.name();
		if(b_rlay_name != "")
			scene += ", "  + b_rlay_name;
	}
	else {
		BLI_timestr(total_time, time_str, sizeof(time_str));
		timestatus = "Time:" + string(time_str) + " | ";

		if(samples > 0 && total_samples != USHRT_MAX)
			remaining_time = (total_samples - samples) * (total_time / samples);
	}

	if(remaining_time > 0) {
		BLI_timestr(remaining_time, time_str, sizeof(time_str));
		timestatus += "Remaining:" + string(time_str) + " | ";
	}

	timestatus += string_printf("Mem:%.2fM, Peak:%.2fM", (double)mem_used, (double)mem_peak);

	if(status.size() > 0)
		status = " | " + status;
	if(substatus.size() > 0)
		status += " | " + substatus;

	if(status != last_status) {
		b_engine.update_stats("", (timestatus + scene + status).c_str());
		b_engine.update_memory_stats(mem_used, mem_peak);
		last_status = status;
	}
	if(progress != last_progress) {
		b_engine.update_progress(progress);
		last_progress = progress;
	}

	if (session->progress.get_error()) {
		string error = session->progress.get_error_message();
		if(error != last_error) {
			/* TODO(sergey): Currently C++ RNA API doesn't let us to
			 * use mnemonic name for the variable. Would be nice to
			 * have this figured out.
			 *
			 * For until then, 1 << 5 means RPT_ERROR.
			 */
			b_engine.report(1 << 5, error.c_str());
			b_engine.error_set(error.c_str());
			last_error = error;
		}
	}
}

void BlenderSession::tag_update()
{
	/* tell blender that we want to get another update callback */
	b_engine.tag_update();
}

void BlenderSession::tag_redraw()
{
	if(background) {
		/* update stats and progress, only for background here because
		 * in 3d view we do it in draw for thread safety reasons */
		update_status_progress();

		/* offline render, redraw if timeout passed */
		if(time_dt() - last_redraw_time > 1.0) {
			b_engine.tag_redraw();
			last_redraw_time = time_dt();
		}
	}
	else {
		/* tell blender that we want to redraw */
		b_engine.tag_redraw();
	}
}

void BlenderSession::test_cancel()
{
	/* test if we need to cancel rendering */
	if(background)
		if(b_engine.test_break())
			session->progress.set_cancel("Cancelled");
}

/* builtin image file name is actually an image datablock name with
 * absolute sequence frame number concatenated via '@' character
 *
 * this function splits frame from builtin name
 */
int BlenderSession::builtin_image_frame(const string &builtin_name)
{
	int last = builtin_name.find_last_of('@');
	return atoi(builtin_name.substr(last + 1, builtin_name.size() - last - 1).c_str());
}

void BlenderSession::builtin_image_info(const string &builtin_name, void *builtin_data, bool &is_float, int &width, int &height, int &depth, int &channels)
{
	/* empty image */
	is_float = false;
	width = 0;
	height = 0;
	depth = 0;
	channels = 0;

	if(!builtin_data)
		return;

	/* recover ID pointer */
	PointerRNA ptr;
	RNA_id_pointer_create((ID*)builtin_data, &ptr);
	BL::ID b_id(ptr);

	if(b_id.is_a(&RNA_Image)) {
		/* image data */
		BL::Image b_image(b_id);

		is_float = b_image.is_float();
		width = b_image.size()[0];
		height = b_image.size()[1];
		depth = 1;
		channels = b_image.channels();
	}
	else if(b_id.is_a(&RNA_Object)) {
		/* smoke volume data */
		BL::Object b_ob(b_id);
		BL::SmokeDomainSettings b_domain = object_smoke_domain_find(b_ob);

		if(!b_domain)
			return;

		if(builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_DENSITY) ||
		   builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_FLAME))
			channels = 1;
		else if(builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_COLOR))
			channels = 4;
		else
			return;

		int3 resolution = get_int3(b_domain.domain_resolution());
		int amplify = (b_domain.use_high_resolution())? b_domain.amplify() + 1: 1;

		width = resolution.x * amplify;
		height = resolution.y * amplify;
		depth = resolution.z * amplify;

		is_float = true;
	}
}

bool BlenderSession::builtin_image_pixels(const string &builtin_name, void *builtin_data, unsigned char *pixels)
{
	if(!builtin_data)
		return false;

	int frame = builtin_image_frame(builtin_name);

	PointerRNA ptr;
	RNA_id_pointer_create((ID*)builtin_data, &ptr);
	BL::Image b_image(ptr);

	int width = b_image.size()[0];
	int height = b_image.size()[1];
	int channels = b_image.channels();

	unsigned char *image_pixels;
	image_pixels = image_get_pixels_for_frame(b_image, frame);

	if(image_pixels) {
		memcpy(pixels, image_pixels, width * height * channels * sizeof(unsigned char));
		MEM_freeN(image_pixels);
	}
	else {
		if(channels == 1) {
			memset(pixels, 0, width * height * sizeof(unsigned char));
		}
		else {
			unsigned char *cp = pixels;
			for(int i = 0; i < width * height; i++, cp += channels) {
				cp[0] = 255;
				cp[1] = 0;
				cp[2] = 255;
				if(channels == 4)
					cp[3] = 255;
			}
		}
	}

	/* premultiply, byte images are always straight for blender */
	unsigned char *cp = pixels;
	for(int i = 0; i < width * height; i++, cp += channels) {
		cp[0] = (cp[0] * cp[3]) >> 8;
		cp[1] = (cp[1] * cp[3]) >> 8;
		cp[2] = (cp[2] * cp[3]) >> 8;
	}

	return true;
}

bool BlenderSession::builtin_image_float_pixels(const string &builtin_name, void *builtin_data, float *pixels)
{
	if(!builtin_data)
		return false;

	PointerRNA ptr;
	RNA_id_pointer_create((ID*)builtin_data, &ptr);
	BL::ID b_id(ptr);

	if(b_id.is_a(&RNA_Image)) {
		/* image data */
		BL::Image b_image(b_id);
		int frame = builtin_image_frame(builtin_name);

		int width = b_image.size()[0];
		int height = b_image.size()[1];
		int channels = b_image.channels();

		float *image_pixels;
		image_pixels = image_get_float_pixels_for_frame(b_image, frame);

		if(image_pixels) {
			memcpy(pixels, image_pixels, width * height * channels * sizeof(float));
			MEM_freeN(image_pixels);
		}
		else {
			if(channels == 1) {
				memset(pixels, 0, width * height * sizeof(float));
			}
			else {
				float *fp = pixels;
				for(int i = 0; i < width * height; i++, fp += channels) {
					fp[0] = 1.0f;
					fp[1] = 0.0f;
					fp[2] = 1.0f;
					if(channels == 4)
						fp[3] = 1.0f;
				}
			}
		}

		return true;
	}
	else if(b_id.is_a(&RNA_Object)) {
		/* smoke volume data */
		BL::Object b_ob(b_id);
		BL::SmokeDomainSettings b_domain = object_smoke_domain_find(b_ob);

		if(!b_domain)
			return false;

		int3 resolution = get_int3(b_domain.domain_resolution());
		int length, amplify = (b_domain.use_high_resolution())? b_domain.amplify() + 1: 1;

		int width = resolution.x * amplify;
		int height = resolution.y * amplify;
		int depth = resolution.z * amplify;

		if(builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_DENSITY)) {
			SmokeDomainSettings_density_grid_get_length(&b_domain.ptr, &length);

			if(length == width*height*depth) {
				SmokeDomainSettings_density_grid_get(&b_domain.ptr, pixels);
				return true;
			}
		}
		else if(builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_FLAME)) {
			/* this is in range 0..1, and interpreted by the OpenGL smoke viewer
			 * as 1500..3000 K with the first part faded to zero density */
			SmokeDomainSettings_flame_grid_get_length(&b_domain.ptr, &length);

			if(length == width*height*depth) {
				SmokeDomainSettings_flame_grid_get(&b_domain.ptr, pixels);
				return true;
			}
		}
		else if(builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_COLOR)) {
			/* the RGB is "premultiplied" by density for better interpolation results */
			SmokeDomainSettings_color_grid_get_length(&b_domain.ptr, &length);

			if(length == width*height*depth*4) {
				SmokeDomainSettings_color_grid_get(&b_domain.ptr, pixels);
				return true;
			}
		}

		fprintf(stderr, "Cycles error: unexpected smoke volume resolution, skipping\n");
	}

	return false;
}

CCL_NAMESPACE_END

