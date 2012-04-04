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
#include <limits.h>

#include "buffers.h"
#include "camera.h"
#include "device.h"
#include "scene.h"
#include "session.h"

#include "util_foreach.h"
#include "util_function.h"
#include "util_time.h"

CCL_NAMESPACE_BEGIN

Session::Session(const SessionParams& params_)
: params(params_),
  tile_manager(params.progressive, params.samples, params.tile_size, params.min_size)
{
	device_use_gl = ((params.device.type != DEVICE_CPU) && !params.background);

	device = Device::create(params.device, params.background, params.threads);
	buffers = new RenderBuffers(device);
	display = new DisplayBuffer(device);

	session_thread = NULL;
	scene = NULL;

	start_time = 0.0;
	reset_time = 0.0;
	preview_time = 0.0;
	paused_time = 0.0;
	sample = 0;

	delayed_reset.do_reset = false;
	delayed_reset.samples = 0;

	display_outdated = false;
	gpu_draw_ready = false;
	gpu_need_tonemap = false;
	pause = false;
	kernels_loaded = false;
}

Session::~Session()
{
	if(session_thread) {
		progress.set_cancel("Exiting");

		gpu_need_tonemap = false;
		gpu_need_tonemap_cond.notify_all();

		{
			thread_scoped_lock pause_lock(pause_mutex);
			pause = false;
		}
		pause_cond.notify_all();

		wait();
	}

	if(params.output_path != "") {
		tonemap();

		progress.set_status("Writing Image", params.output_path);
		display->write(device, params.output_path);
	}

	delete buffers;
	delete display;
	delete scene;
	delete device;
}

void Session::start()
{
	session_thread = new thread(function_bind(&Session::run, this));
}

bool Session::ready_to_reset()
{
	double dt = time_dt() - reset_time;

	if(!display_outdated)
		return (dt > params.reset_timeout);
	else
		return (dt > params.cancel_timeout);
}

/* GPU Session */

void Session::reset_gpu(BufferParams& buffer_params, int samples)
{
	/* block for buffer acces and reset immediately. we can't do this
	   in the thread, because we need to allocate an OpenGL buffer, and
	   that only works in the main thread */
	thread_scoped_lock display_lock(display->mutex);
	thread_scoped_lock buffers_lock(buffers->mutex);

	display_outdated = true;
	reset_time = time_dt();

	reset_(buffer_params, samples);

	gpu_need_tonemap = false;
	gpu_need_tonemap_cond.notify_all();

	pause_cond.notify_all();
}

bool Session::draw_gpu(BufferParams& buffer_params)
{
	/* block for buffer access */
	thread_scoped_lock display_lock(display->mutex);

	/* first check we already rendered something */
	if(gpu_draw_ready) {
		/* then verify the buffers have the expected size, so we don't
		   draw previous results in a resized window */
		if(!buffer_params.modified(display->params)) {
			/* for CUDA we need to do tonemapping still, since we can
			   only access GL buffers from the main thread */
			if(gpu_need_tonemap) {
				thread_scoped_lock buffers_lock(buffers->mutex);
				tonemap();
				gpu_need_tonemap = false;
				gpu_need_tonemap_cond.notify_all();
			}

			display->draw(device);

			if(display_outdated && (time_dt() - reset_time) > params.text_timeout)
				return false;

			return true;
		}
	}

	return false;
}

void Session::run_gpu()
{
	start_time = time_dt();
	reset_time = time_dt();
	paused_time = 0.0;

	if(!params.background)
		progress.set_start_time(start_time + paused_time);

	while(!progress.get_cancel()) {
		/* advance to next tile */
		bool no_tiles = !tile_manager.next();

		if(params.background) {
			/* if no work left and in background mode, we can stop immediately */
			if(no_tiles) {
				progress.set_status("Finished");
				break;
			}
		}
		else {
			/* if in interactive mode, and we are either paused or done for now,
			   wait for pause condition notify to wake up again */
			thread_scoped_lock pause_lock(pause_mutex);

			if(pause || no_tiles) {
				update_status_time(pause, no_tiles);

				while(1) {
					double pause_start = time_dt();
					pause_cond.wait(pause_lock);
					paused_time += time_dt() - pause_start;

					if(!params.background)
						progress.set_start_time(start_time + paused_time);

					update_status_time(pause, no_tiles);
					progress.set_update();

					if(!pause)
						break;
				}
			}

			if(progress.get_cancel())
				break;
		}

		if(!no_tiles) {
			/* update scene */
			update_scene();

			if(device->error_message() != "")
				progress.set_cancel(device->error_message());

			if(progress.get_cancel())
				break;
		}

		if(!no_tiles) {
			/* buffers mutex is locked entirely while rendering each
			   sample, and released/reacquired on each iteration to allow
			   reset and draw in between */
			thread_scoped_lock buffers_lock(buffers->mutex);

			/* update status and timing */
			update_status_time();

			/* path trace */
			foreach(Tile& tile, tile_manager.state.tiles) {
				path_trace(tile);

				device->task_wait();

				if(device->error_message() != "")
					progress.set_cancel(device->error_message());

				if(progress.get_cancel())
					break;
			}

			/* update status and timing */
			update_status_time();

			gpu_need_tonemap = true;
			gpu_draw_ready = true;
			progress.set_update();

			/* wait for tonemap */
			if(!params.background) {
				while(gpu_need_tonemap) {
					if(progress.get_cancel())
						break;

					gpu_need_tonemap_cond.wait(buffers_lock);
				}
			}

			if(device->error_message() != "")
				progress.set_cancel(device->error_message());

			if(progress.get_cancel())
				break;
		}
	}
}

/* CPU Session */

void Session::reset_cpu(BufferParams& buffer_params, int samples)
{
	thread_scoped_lock reset_lock(delayed_reset.mutex);

	display_outdated = true;
	reset_time = time_dt();

	delayed_reset.params = buffer_params;
	delayed_reset.samples = samples;
	delayed_reset.do_reset = true;
	device->task_cancel();

	pause_cond.notify_all();
}

bool Session::draw_cpu(BufferParams& buffer_params)
{
	thread_scoped_lock display_lock(display->mutex);

	/* first check we already rendered something */
	if(display->draw_ready()) {
		/* then verify the buffers have the expected size, so we don't
		   draw previous results in a resized window */
		if(!buffer_params.modified(display->params)) {
			display->draw(device);

			if(display_outdated && (time_dt() - reset_time) > params.text_timeout)
				return false;

			return true;
		}
	}

	return false;
}

void Session::run_cpu()
{
	{
		/* reset once to start */
		thread_scoped_lock reset_lock(delayed_reset.mutex);
		thread_scoped_lock buffers_lock(buffers->mutex);
		thread_scoped_lock display_lock(display->mutex);

		reset_(delayed_reset.params, delayed_reset.samples);
		delayed_reset.do_reset = false;
	}

	while(!progress.get_cancel()) {
		/* advance to next tile */
		bool no_tiles = !tile_manager.next();
		bool need_tonemap = false;

		if(params.background) {
			/* if no work left and in background mode, we can stop immediately */
			if(no_tiles) {
				progress.set_status("Finished");
				break;
			}
		}
		else {
			/* if in interactive mode, and we are either paused or done for now,
			   wait for pause condition notify to wake up again */
			thread_scoped_lock pause_lock(pause_mutex);

			if(pause || no_tiles) {
				update_status_time(pause, no_tiles);

				while(1) {
					double pause_start = time_dt();
					pause_cond.wait(pause_lock);
					paused_time += time_dt() - pause_start;

					if(!params.background)
						progress.set_start_time(start_time + paused_time);

					update_status_time(pause, no_tiles);
					progress.set_update();

					if(!pause)
						break;
				}
			}

			if(progress.get_cancel())
				break;
		}

		if(!no_tiles) {
			/* buffers mutex is locked entirely while rendering each
			   sample, and released/reacquired on each iteration to allow
			   reset and draw in between */
			thread_scoped_lock buffers_lock(buffers->mutex);

			/* update scene */
			update_scene();

			if(device->error_message() != "")
				progress.set_cancel(device->error_message());

			if(progress.get_cancel())
				break;

			/* update status and timing */
			update_status_time();

			/* path trace */
			foreach(Tile& tile, tile_manager.state.tiles)
				path_trace(tile);

			/* update status and timing */
			update_status_time();

			if(!params.background)
				need_tonemap = true;

			if(device->error_message() != "")
				progress.set_cancel(device->error_message());
		}

		device->task_wait();

		{
			thread_scoped_lock reset_lock(delayed_reset.mutex);
			thread_scoped_lock buffers_lock(buffers->mutex);
			thread_scoped_lock display_lock(display->mutex);

			if(delayed_reset.do_reset) {
				/* reset rendering if request from main thread */
				delayed_reset.do_reset = false;
				reset_(delayed_reset.params, delayed_reset.samples);
			}
			else if(need_tonemap) {
				/* tonemap only if we do not reset, we don't we don't
				   want to show the result of an incomplete sample*/
				tonemap();
			}

			if(device->error_message() != "")
				progress.set_cancel(device->error_message());
		}

		progress.set_update();
	}
}

void Session::run()
{
	/* load kernels */
	if(!kernels_loaded) {
		progress.set_status("Loading render kernels (may take a few minutes the first time)");

		if(!device->load_kernels(params.experimental)) {
			string message = device->error_message();
			if(message == "")
				message = "Failed loading render kernel, see console for errors";

			progress.set_status("Error", message);
			progress.set_update();
			return;
		}

		kernels_loaded = true;
	}

	/* session thread loop */
	progress.set_status("Waiting for render to start");

	/* run */
	if(!progress.get_cancel()) {
		if(device_use_gl)
			run_gpu();
		else
			run_cpu();
	}

	/* progress update */
	if(progress.get_cancel())
		progress.set_status("Cancel", progress.get_cancel_message());
	else
		progress.set_update();
}

bool Session::draw(BufferParams& buffer_params)
{
	if(device_use_gl)
		return draw_gpu(buffer_params);
	else
		return draw_cpu(buffer_params);
}

void Session::reset_(BufferParams& buffer_params, int samples)
{
	if(buffer_params.modified(buffers->params)) {
		gpu_draw_ready = false;
		buffers->reset(device, buffer_params);
		display->reset(device, buffer_params);
	}

	tile_manager.reset(buffer_params, samples);

	start_time = time_dt();
	preview_time = 0.0;
	paused_time = 0.0;
	sample = 0;

	if(!params.background)
		progress.set_start_time(start_time + paused_time);
}

void Session::reset(BufferParams& buffer_params, int samples)
{
	if(device_use_gl)
		reset_gpu(buffer_params, samples);
	else
		reset_cpu(buffer_params, samples);
}

void Session::set_samples(int samples)
{
	if(samples != params.samples) {
		params.samples = samples;
		tile_manager.set_samples(samples);

		{
			thread_scoped_lock pause_lock(pause_mutex);
		}
		pause_cond.notify_all();
	}
}

void Session::set_pause(bool pause_)
{
	bool notify = false;

	{
		thread_scoped_lock pause_lock(pause_mutex);

		if(pause != pause_) {
			pause = pause_;
			notify = true;
		}
	}

	if(notify)
		pause_cond.notify_all();
}

void Session::wait()
{
	session_thread->join();
	delete session_thread;

	session_thread = NULL;
}

void Session::update_scene()
{
	thread_scoped_lock scene_lock(scene->mutex);

	progress.set_status("Updating Scene");

	/* update camera if dimensions changed for progressive render. the camera
	   knows nothing about progressive or cropped rendering, it just gets the
	   image dimensions passed in */
	Camera *cam = scene->camera;
	int width = tile_manager.state.buffer.full_width;
	int height = tile_manager.state.buffer.full_height;

	if(width != cam->width || height != cam->height) {
		cam->width = width;
		cam->height = height;
		cam->tag_update();
	}

	/* update scene */
	if(scene->need_update())
		scene->device_update(device, progress);
}

void Session::update_status_time(bool show_pause, bool show_done)
{
	int sample = tile_manager.state.sample;
	int resolution = tile_manager.state.resolution;

	/* update status */
	string status, substatus;

	if(!params.progressive)
		substatus = "Path Tracing";
	else if(params.samples == INT_MAX)
		substatus = string_printf("Path Tracing Sample %d", sample+1);
	else
		substatus = string_printf("Path Tracing Sample %d/%d", sample+1, params.samples);
	
	if(show_pause)
		status = "Paused";
	else if(show_done)
		status = "Done";
	else
		status = "Rendering";

	progress.set_status(status, substatus);

	/* update timing */
	if(preview_time == 0.0 && resolution == 1)
		preview_time = time_dt();
	
	double sample_time = (sample == 0)? 0.0: (time_dt() - preview_time - paused_time)/(sample);

	/* negative can happen when we pause a bit before rendering, can discard that */
	if(preview_time < 0.0) preview_time = 0.0;

	progress.set_sample(sample + 1, sample_time);
}

void Session::path_trace(Tile& tile)
{
	/* add path trace task */
	DeviceTask task(DeviceTask::PATH_TRACE);

	task.x = tile_manager.state.buffer.full_x + tile.x;
	task.y = tile_manager.state.buffer.full_y + tile.y;
	task.w = tile.w;
	task.h = tile.h;
	task.buffer = buffers->buffer.device_pointer;
	task.rng_state = buffers->rng_state.device_pointer;
	task.sample = tile_manager.state.sample;
	task.resolution = tile_manager.state.resolution;
	tile_manager.state.buffer.get_offset_stride(task.offset, task.stride);

	device->task_add(task);
}

void Session::tonemap()
{
	/* add tonemap task */
	DeviceTask task(DeviceTask::TONEMAP);

	task.x = tile_manager.state.buffer.full_x;
	task.y = tile_manager.state.buffer.full_y;
	task.w = tile_manager.state.buffer.width;
	task.h = tile_manager.state.buffer.height;
	task.rgba = display->rgba.device_pointer;
	task.buffer = buffers->buffer.device_pointer;
	task.sample = tile_manager.state.sample;
	task.resolution = tile_manager.state.resolution;
	tile_manager.state.buffer.get_offset_stride(task.offset, task.stride);

	if(task.w > 0 && task.h > 0) {
		device->task_add(task);
		device->task_wait();

		/* set display to new size */
		display->draw_set(task.w, task.h);
	}

	display_outdated = false;
}

CCL_NAMESPACE_END

