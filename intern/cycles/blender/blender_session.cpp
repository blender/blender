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

BlenderSession::BlenderSession(BL::RenderEngine b_engine_, BL::BlendData b_data_, BL::Scene b_scene_)
: b_engine(b_engine_), b_data(b_data_), b_scene(b_scene_), b_v3d(PointerRNA_NULL), b_rv3d(PointerRNA_NULL),
  b_rr(PointerRNA_NULL), b_rlay(PointerRNA_NULL)
{
	/* offline render */
	BL::RenderSettings r = b_scene.render();

	width = (int)(r.resolution_x()*r.resolution_percentage()/100);
	height = (int)(r.resolution_y()*r.resolution_percentage()/100);
	background = true;
	last_redraw_time = 0.0f;

	create_session();
}

BlenderSession::BlenderSession(BL::RenderEngine b_engine_, BL::BlendData b_data_, BL::Scene b_scene_,
	BL::SpaceView3D b_v3d_, BL::RegionView3D b_rv3d_, int width_, int height_)
: b_engine(b_engine_), b_data(b_data_), b_scene(b_scene_), b_v3d(b_v3d_), b_rv3d(b_rv3d_),
  b_rr(PointerRNA_NULL), b_rlay(PointerRNA_NULL)
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
	SessionParams session_params = BlenderSync::get_session_params(b_scene, background);

	/* reset status/progress */
	last_status= "";
	last_progress= -1.0f;

	/* create scene */
	scene = new Scene(scene_params);

	/* create sync */
	sync = new BlenderSync(b_data, b_scene, scene, !background);
	sync->sync_data(b_v3d);

	if(b_rv3d)
		sync->sync_view(b_v3d, b_rv3d, width, height);
	else
		sync->sync_camera(width, height);

	/* create session */
	session = new Session(session_params);
	session->scene = scene;
	session->progress.set_update_callback(function_bind(&BlenderSession::tag_redraw, this));
	session->progress.set_cancel_callback(function_bind(&BlenderSession::test_cancel, this));
	session->set_pause(BlenderSync::get_session_pause(b_scene, background));

	/* set buffer parameters */
	BufferParams buffer_params = BlenderSync::get_buffer_params(b_scene, b_rv3d, width, height);
	session->reset(buffer_params, session_params.samples);
}

void BlenderSession::free_session()
{
	delete sync;
	delete session;
}

void BlenderSession::render()
{
	/* get buffer parameters */
	BufferParams buffer_params = BlenderSync::get_buffer_params(b_scene, b_rv3d, width, height);
	int w = buffer_params.width, h = buffer_params.height;

	/* create render result */
	RenderResult *rrp = RE_engine_begin_result((RenderEngine*)b_engine.ptr.data, 0, 0, w, h);
	PointerRNA rrptr;
	RNA_pointer_create(NULL, &RNA_RenderResult, rrp, &rrptr);
	b_rr = BL::RenderResult(rrptr);

	BL::RenderSettings r = b_scene.render();
	BL::RenderResult::layers_iterator b_iter;
	BL::RenderLayers b_rr_layers(r.ptr);
	
	int active = 0;

	/* render each layer */
	for(b_rr.layers.begin(b_iter); b_iter != b_rr.layers.end(); ++b_iter, ++active) {
		/* single layer render */
		if(r.use_single_layer())
			active = b_rr_layers.active_index();

		/* set layer */
		b_rlay = *b_iter;

		/* update scene */
		sync->sync_data(b_v3d, active);

		/* render */
		session->start();
		session->wait();

		if(session->progress.get_cancel())
			break;

		/* write result */
		write_render_result();
	}

	/* delete render result */
	RE_engine_end_result((RenderEngine*)b_engine.ptr.data, (RenderResult*)b_rr.ptr.data);
}

void BlenderSession::write_render_result()
{
	/* get state */
	RenderBuffers *buffers = session->buffers;
	float exposure = scene->film->exposure;
	double total_time, sample_time;
	int sample;
	session->progress.get_sample(sample, total_time, sample_time);

	/* get pixels */
	float4 *pixels = buffers->copy_from_device(exposure, sample);

	if(!pixels)
		return;

	/* write pixels */
	rna_RenderLayer_rect_set(&b_rlay.ptr, (float*)pixels);
	RE_engine_update_result((RenderEngine*)b_engine.ptr.data, (RenderResult*)b_rr.ptr.data);

	delete [] pixels;
}

void BlenderSession::synchronize()
{
	/* on session/scene parameter changes, we recreate session entirely */
	SceneParams scene_params = BlenderSync::get_scene_params(b_scene, background);
	SessionParams session_params = BlenderSync::get_session_params(b_scene, background);

	if(session->params.modified(session_params) ||
	   scene->params.modified(scene_params)) {
		free_session();
		create_session();
		session->start();
		return;
	}

	/* increase samples, but never decrease */
	session->set_samples(session_params.samples);
	session->set_pause(BlenderSync::get_session_pause(b_scene, background));

	/* copy recalc flags, outside of mutex so we can decide to do the real
	   synchronization at a later time to not block on running updates */
	sync->sync_recalc();

	/* try to acquire mutex. if we don't want to or can't, come back later */
	if(!session->ready_to_reset() || !session->scene->mutex.try_lock()) {
		tag_update();
		return;
	}

	/* data and camera synchronize */
	sync->sync_data(b_v3d);

	if(b_rv3d)
		sync->sync_view(b_v3d, b_rv3d, width, height);
	else
		sync->sync_camera(width, height);

	/* unlock */
	session->scene->mutex.unlock();

	/* reset if needed */
	if(scene->need_reset()) {
		BufferParams buffer_params = BlenderSync::get_buffer_params(b_scene, b_rv3d, width, height);
		session->reset(buffer_params, session_params.samples);
	}
}

bool BlenderSession::draw(int w, int h)
{
	/* before drawing, we verify camera and viewport size changes, because
	   we do not get update callbacks for those, we must detect them here */
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
			SessionParams session_params = BlenderSync::get_session_params(b_scene, background);
			BufferParams buffer_params = BlenderSync::get_buffer_params(b_scene, b_rv3d, width, height);

			session->reset(buffer_params, session_params.samples);
		}
	}

	/* update status and progress for 3d view draw */
	update_status_progress();

	/* draw */
	BufferParams buffer_params = BlenderSync::get_buffer_params(b_scene, b_rv3d, width, height);

	return !session->draw(buffer_params);
}

void BlenderSession::get_status(string& status, string& substatus)
{
	session->progress.get_status(status, substatus);
}

void BlenderSession::get_progress(float& progress, double& total_time)
{
	double sample_time;
	int sample;

	session->progress.get_sample(sample, total_time, sample_time);
	progress = ((float)sample/(float)session->params.samples);
}

void BlenderSession::update_status_progress()
{
	string status, substatus;
	float progress;
	double total_time;
	char time_str[128];

	get_status(status, substatus);
	get_progress(progress, total_time);

	BLI_timestr(total_time, time_str);
	status = "Elapsed: " + string(time_str) + " | " + status;

	if(substatus.size() > 0)
		status += " | " + substatus;

	if(status != last_status) {
		RE_engine_update_stats((RenderEngine*)b_engine.ptr.data, "", status.c_str());
		last_status = status;
	}
	if(progress != last_progress) {
		RE_engine_update_progress((RenderEngine*)b_engine.ptr.data, progress);
		last_progress = progress;
	}
}

void BlenderSession::tag_update()
{
	/* tell blender that we want to get another update callback */
	engine_tag_update((RenderEngine*)b_engine.ptr.data);
}

void BlenderSession::tag_redraw()
{
	if(background) {
		/* update stats and progress, only for background here because
		   in 3d view we do it in draw for thread safety reasons */
		update_status_progress();

		/* offline render, redraw if timeout passed */
		if(time_dt() - last_redraw_time > 1.0f) {
			write_render_result();
			engine_tag_redraw((RenderEngine*)b_engine.ptr.data);
			last_redraw_time = time_dt();
		}
	}
	else {
		/* tell blender that we want to redraw */
		engine_tag_redraw((RenderEngine*)b_engine.ptr.data);
	}
}

void BlenderSession::test_cancel()
{
	/* test if we need to cancel rendering */
	if(background)
		if(RE_engine_test_break((RenderEngine*)b_engine.ptr.data))
			session->progress.set_cancel("Cancelled");
}

CCL_NAMESPACE_END

