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

#include "background.h"
#include "buffers.h"
#include "camera.h"
#include "device.h"
#include "integrator.h"
#include "film.h"
#include "light.h"
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
: b_engine(b_engine_), b_userpref(b_userpref_), b_data(b_data_), b_scene(b_scene_),
  b_v3d(PointerRNA_NULL), b_rv3d(PointerRNA_NULL)
{
	/* offline render */

	width = b_engine.resolution_x();
	height = b_engine.resolution_y();

	background = true;
	last_redraw_time = 0.0f;

	create_session();
}

BlenderSession::BlenderSession(BL::RenderEngine b_engine_, BL::UserPreferences b_userpref_,
	BL::BlendData b_data_, BL::Scene b_scene_,
	BL::SpaceView3D b_v3d_, BL::RegionView3D b_rv3d_, int width_, int height_)
: b_engine(b_engine_), b_userpref(b_userpref_), b_data(b_data_), b_scene(b_scene_),
  b_v3d(b_v3d_), b_rv3d(b_rv3d_)
{
	/* 3d view render */
	width = width_;
	height = height_;
	background = false;
	last_redraw_time = 0.0f;

	create_session();
	session->start();
}

BlenderSession::~BlenderSession()
{
	free_session();
}

void BlenderSession::create_session()
{
	SceneParams scene_params = BlenderSync::get_scene_params(b_scene, background);
	SessionParams session_params = BlenderSync::get_session_params(b_engine, b_userpref, b_scene, background);

	/* reset status/progress */
	last_status = "";
	last_progress = -1.0f;

	/* create scene */
	scene = new Scene(scene_params, session_params.device);

	/* create session */
	session = new Session(session_params);
	session->scene = scene;
	session->progress.set_update_callback(function_bind(&BlenderSession::tag_redraw, this));
	session->progress.set_cancel_callback(function_bind(&BlenderSession::test_cancel, this));
	session->set_pause(BlenderSync::get_session_pause(b_scene, background));

	/* create sync */
	sync = new BlenderSync(b_engine, b_data, b_scene, scene, !background, session->progress);
	sync->sync_data(b_v3d, b_engine.camera_override());

	if(b_rv3d)
		sync->sync_view(b_v3d, b_rv3d, width, height);
	else
		sync->sync_camera(b_engine.camera_override(), width, height);

	/* set buffer parameters */
	BufferParams buffer_params = BlenderSync::get_buffer_params(b_scene, b_v3d, b_rv3d, scene->camera, width, height);
	session->reset(buffer_params, session_params.samples);
}

void BlenderSession::reset_session(BL::BlendData b_data_, BL::Scene b_scene_)
{
	b_data = b_data_;
	b_scene = b_scene_;

	SceneParams scene_params = BlenderSync::get_scene_params(b_scene, background);
	SessionParams session_params = BlenderSync::get_session_params(b_engine, b_userpref, b_scene, background);

	if(scene->params.modified(scene_params) ||
	   session->params.modified(session_params))
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

	/* peak memory usage should show current render peak, not peak for all renders
	 * made by this render session
	 */
	session->stats.mem_peak = session->stats.mem_used;

	/* sync object should be re-created */
	sync = new BlenderSync(b_engine, b_data, b_scene, scene, !background, session->progress);
	sync->sync_data(b_v3d, b_engine.camera_override());
	sync->sync_camera(b_engine.camera_override(), width, height);

	BufferParams buffer_params = BlenderSync::get_buffer_params(b_scene, PointerRNA_NULL, PointerRNA_NULL, scene->camera, width, height);
	session->reset(buffer_params, session_params.samples);
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

		case BL::RenderPass::type_DIFFUSE_INDIRECT:
			return PASS_DIFFUSE_INDIRECT;
		case BL::RenderPass::type_GLOSSY_INDIRECT:
			return PASS_GLOSSY_INDIRECT;
		case BL::RenderPass::type_TRANSMISSION_INDIRECT:
			return PASS_TRANSMISSION_INDIRECT;

		case BL::RenderPass::type_DIFFUSE_COLOR:
			return PASS_DIFFUSE_COLOR;
		case BL::RenderPass::type_GLOSSY_COLOR:
			return PASS_GLOSSY_COLOR;
		case BL::RenderPass::type_TRANSMISSION_COLOR:
			return PASS_TRANSMISSION_COLOR;

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
		case BL::RenderPass::type_MIST:
			return PASS_NONE;
	}
	
	return PASS_NONE;
}

static BL::RenderResult begin_render_result(BL::RenderEngine b_engine, int x, int y, int w, int h, const char *layername)
{
	return b_engine.begin_result(x, y, w, h, layername);
}

static void end_render_result(BL::RenderEngine b_engine, BL::RenderResult b_rr, bool cancel = false)
{
	b_engine.end_result(b_rr, (int)cancel);
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
	BL::RenderLayer b_rlay = *b_single_rlay;

	if (do_update_only) {
		/* update only needed */
		update_render_result(b_rr, b_rlay, rtile);
		end_render_result(b_engine, b_rr, true);
	}
	else {
		/* write result */
		write_render_result(b_rr, b_rlay, rtile);
		end_render_result(b_engine, b_rr);
	}
}

void BlenderSession::write_render_tile(RenderTile& rtile)
{
	do_write_update_render_tile(rtile, false);
}

void BlenderSession::update_render_tile(RenderTile& rtile)
{
	do_write_update_render_tile(rtile, true);
}

void BlenderSession::render()
{
	/* set callback to write out render results */
	session->write_render_tile_cb = function_bind(&BlenderSession::write_render_tile, this, _1);
	session->update_render_tile_cb = function_bind(&BlenderSession::update_render_tile, this, _1);

	/* get buffer parameters */
	SessionParams session_params = BlenderSync::get_session_params(b_engine, b_userpref, b_scene, background);
	BufferParams buffer_params = BlenderSync::get_buffer_params(b_scene, b_v3d, b_rv3d, scene->camera, width, height);

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
			end_render_result(b_engine, b_rr, true);
			continue;
		}

		BL::RenderLayer b_rlay = *b_single_rlay;

		/* add passes */
		vector<Pass> passes;
		Pass::add(PASS_COMBINED, passes);

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
		end_render_result(b_engine, b_rr, true);

		buffer_params.passes = passes;
		scene->film->tag_passes_update(scene, passes);
		scene->film->tag_update(scene);
		scene->integrator->tag_update(scene);

		/* update scene */
		sync->sync_data(b_v3d, b_engine.camera_override(), b_rlay_name.c_str());

		/* update session */
		int samples = sync->get_layer_samples();
		session->reset(buffer_params, (samples == 0)? session_params.samples: samples);

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
			if(buffers->get_pass_rect(pass_type, exposure, rtile.sample, components, &pixels[0]))
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
	/* on session/scene parameter changes, we recreate session entirely */
	SceneParams scene_params = BlenderSync::get_scene_params(b_scene, background);
	SessionParams session_params = BlenderSync::get_session_params(b_engine, b_userpref, b_scene, background);

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
	session->set_pause(BlenderSync::get_session_pause(b_scene, background));

	/* copy recalc flags, outside of mutex so we can decide to do the real
	 * synchronization at a later time to not block on running updates */
	sync->sync_recalc();

	/* try to acquire mutex. if we don't want to or can't, come back later */
	if(!session->ready_to_reset() || !session->scene->mutex.try_lock()) {
		tag_update();
		return;
	}

	/* data and camera synchronize */
	sync->sync_data(b_v3d, b_engine.camera_override());

	if(b_rv3d)
		sync->sync_view(b_v3d, b_rv3d, width, height);
	else
		sync->sync_camera(b_engine.camera_override(), width, height);

	/* unlock */
	session->scene->mutex.unlock();

	/* reset if needed */
	if(scene->need_reset()) {
		BufferParams buffer_params = BlenderSync::get_buffer_params(b_scene, b_v3d, b_rv3d, scene->camera, width, height);
		session->reset(buffer_params, session_params.samples);
	}
}

bool BlenderSession::draw(int w, int h)
{
	/* before drawing, we verify camera and viewport size changes, because
	 * we do not get update callbacks for those, we must detect them here */
	if(session->ready_to_reset()) {
		bool reset = false;

		/* try to acquire mutex. if we can't, come back later */
		if(!session->scene->mutex.try_lock()) {
			tag_update();
		}
		else {
			/* update camera from 3d view */
			bool need_update = scene->camera->need_update;

			sync->sync_view(b_v3d, b_rv3d, w, h);

			if(scene->camera->need_update && !need_update)
				reset = true;

			session->scene->mutex.unlock();
		}

		/* if dimensions changed, reset */
		if(width != w || height != h) {
			width = w;
			height = h;
			reset = true;
		}

		/* reset if requested */
		if(reset) {
			SessionParams session_params = BlenderSync::get_session_params(b_engine, b_userpref, b_scene, background);
			BufferParams buffer_params = BlenderSync::get_buffer_params(b_scene, b_v3d, b_rv3d, scene->camera, w, h);

			session->reset(buffer_params, session_params.samples);
		}
	}

	/* update status and progress for 3d view draw */
	update_status_progress();

	/* draw */
	BufferParams buffer_params = BlenderSync::get_buffer_params(b_scene, b_v3d, b_rv3d, scene->camera, width, height);

	return !session->draw(buffer_params);
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
	samples_per_tile = session->params.samples;

	if(samples_per_tile)
		progress = ((float)sample/(float)(tile_total * samples_per_tile));
	else
		progress = 0.0;
}

void BlenderSession::update_status_progress()
{
	string timestatus, status, substatus;
	float progress;
	double total_time;
	char time_str[128];
	float mem_used = (float)session->stats.mem_used / 1024.0f / 1024.0f;
	float mem_peak = (float)session->stats.mem_peak / 1024.0f / 1024.0f;

	get_status(status, substatus);
	get_progress(progress, total_time);

	timestatus = string_printf("Mem: %.2fM, Peak: %.2fM | ", mem_used, mem_peak);

	timestatus += b_scene.name();
	if(b_rlay_name != "")
		timestatus += ", "  + b_rlay_name;
	timestatus += " | ";

	BLI_timestr(total_time, time_str);
	timestatus += "Elapsed: " + string(time_str) + " | ";

	if(substatus.size() > 0)
		status += " | " + substatus;

	if(status != last_status) {
		b_engine.update_stats("", (timestatus + status).c_str());
		b_engine.update_memory_stats(mem_used, mem_peak);
		last_status = status;
	}
	if(progress != last_progress) {
		b_engine.update_progress(progress);
		last_progress = progress;
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

CCL_NAMESPACE_END

