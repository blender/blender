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

#include <string.h>
#include <limits.h>

#include "buffers.h"
#include "camera.h"
#include "device.h"
#include "integrator.h"
#include "scene.h"
#include "session.h"
#include "bake.h"

#include "util_foreach.h"
#include "util_function.h"
#include "util_math.h"
#include "util_opengl.h"
#include "util_task.h"
#include "util_time.h"

CCL_NAMESPACE_BEGIN

/* Note about  preserve_tile_device option for tile manager:
 * progressive refine and viewport rendering does requires tiles to
 * always be allocated for the same device
 */
Session::Session(const SessionParams& params_)
: params(params_),
  tile_manager(params.progressive, params.samples, params.tile_size, params.start_resolution,
       params.background == false || params.progressive_refine, params.background, params.tile_order,
       max(params.device.multi_devices.size(), 1)),
  stats()
{
	device_use_gl = ((params.device.type != DEVICE_CPU) && !params.background);

	TaskScheduler::init(params.threads);

	device = Device::create(params.device, stats, params.background);

	if(params.background && params.output_path.empty()) {
		buffers = NULL;
		display = NULL;
	}
	else {
		buffers = new RenderBuffers(device);
		display = new DisplayBuffer(device, params.display_buffer_linear);
	}

	session_thread = NULL;
	scene = NULL;

	start_time = 0.0;
	reset_time = 0.0;
	preview_time = 0.0;
	paused_time = 0.0;
	last_update_time = 0.0;

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
		/* wait for session thread to end */
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

	if(!params.output_path.empty()) {
		/* tonemap and write out image if requested */
		delete display;

		display = new DisplayBuffer(device, false);
		display->reset(device, buffers->params);
		tonemap(params.samples);

		progress.set_status("Writing Image", params.output_path);
		display->write(device, params.output_path);
	}

	/* clean up */
	foreach(RenderBuffers *buffers, tile_buffers)
		delete buffers;

	delete buffers;
	delete display;
	delete scene;
	delete device;

	TaskScheduler::exit();
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
	thread_scoped_lock pause_lock(pause_mutex);

	/* block for buffer access and reset immediately. we can't do this
	 * in the thread, because we need to allocate an OpenGL buffer, and
	 * that only works in the main thread */
	thread_scoped_lock display_lock(display_mutex);
	thread_scoped_lock buffers_lock(buffers_mutex);

	display_outdated = true;
	reset_time = time_dt();

	reset_(buffer_params, samples);

	gpu_need_tonemap = false;
	gpu_need_tonemap_cond.notify_all();

	pause_cond.notify_all();
}

bool Session::draw_gpu(BufferParams& buffer_params, DeviceDrawParams& draw_params)
{
	/* block for buffer access */
	thread_scoped_lock display_lock(display_mutex);

	/* first check we already rendered something */
	if(gpu_draw_ready) {
		/* then verify the buffers have the expected size, so we don't
		 * draw previous results in a resized window */
		if(!buffer_params.modified(display->params)) {
			/* for CUDA we need to do tonemapping still, since we can
			 * only access GL buffers from the main thread */
			if(gpu_need_tonemap) {
				thread_scoped_lock buffers_lock(buffers_mutex);
				tonemap(tile_manager.state.sample);
				gpu_need_tonemap = false;
				gpu_need_tonemap_cond.notify_all();
			}

			display->draw(device, draw_params);

			if(display_outdated && (time_dt() - reset_time) > params.text_timeout)
				return false;

			return true;
		}
	}

	return false;
}

void Session::run_gpu()
{
	bool tiles_written = false;

	start_time = time_dt();
	reset_time = time_dt();
	paused_time = 0.0;
	last_update_time = time_dt();

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
			 * wait for pause condition notify to wake up again */
			thread_scoped_lock pause_lock(pause_mutex);

			if(!pause && !tile_manager.done()) {
				/* reset could have happened after no_tiles was set, before this lock.
				 * in this case we shall not wait for pause condition
				 */
			}
			else if(pause || no_tiles) {
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

			if(!device->error_message().empty())
				progress.set_error(device->error_message());

			if(progress.get_cancel())
				break;
		}

		if(!no_tiles) {
			/* buffers mutex is locked entirely while rendering each
			 * sample, and released/reacquired on each iteration to allow
			 * reset and draw in between */
			thread_scoped_lock buffers_lock(buffers_mutex);

			/* update status and timing */
			update_status_time();

			/* path trace */
			path_trace();

			device->task_wait();

			if(!device->error_message().empty())
				progress.set_cancel(device->error_message());

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

			if(!device->error_message().empty())
				progress.set_error(device->error_message());

			tiles_written = update_progressive_refine(progress.get_cancel());

			if(progress.get_cancel())
				break;
		}
	}

	if(!tiles_written)
		update_progressive_refine(true);
}

/* CPU Session */

void Session::reset_cpu(BufferParams& buffer_params, int samples)
{
	thread_scoped_lock reset_lock(delayed_reset.mutex);
	thread_scoped_lock pause_lock(pause_mutex);

	display_outdated = true;
	reset_time = time_dt();

	delayed_reset.params = buffer_params;
	delayed_reset.samples = samples;
	delayed_reset.do_reset = true;
	device->task_cancel();

	pause_cond.notify_all();
}

bool Session::draw_cpu(BufferParams& buffer_params, DeviceDrawParams& draw_params)
{
	thread_scoped_lock display_lock(display_mutex);

	/* first check we already rendered something */
	if(display->draw_ready()) {
		/* then verify the buffers have the expected size, so we don't
		 * draw previous results in a resized window */
		if(!buffer_params.modified(display->params)) {
			display->draw(device, draw_params);

			if(display_outdated && (time_dt() - reset_time) > params.text_timeout)
				return false;

			return true;
		}
	}

	return false;
}

bool Session::acquire_tile(Device *tile_device, RenderTile& rtile)
{
	if(progress.get_cancel()) {
		if(params.progressive_refine == false) {
			/* for progressive refine current sample should be finished for all tiles */
			return false;
		}
	}

	thread_scoped_lock tile_lock(tile_mutex);

	/* get next tile from manager */
	Tile tile;
	int device_num = device->device_number(tile_device);

	if(!tile_manager.next_tile(tile, device_num))
		return false;
	
	/* fill render tile */
	rtile.x = tile_manager.state.buffer.full_x + tile.x;
	rtile.y = tile_manager.state.buffer.full_y + tile.y;
	rtile.w = tile.w;
	rtile.h = tile.h;
	rtile.start_sample = tile_manager.state.sample;
	rtile.num_samples = tile_manager.state.num_samples;
	rtile.resolution = tile_manager.state.resolution_divider;

	tile_lock.unlock();

	/* in case of a permanent buffer, return it, otherwise we will allocate
	 * a new temporary buffer */
	if(!(params.background && params.output_path.empty())) {
		tile_manager.state.buffer.get_offset_stride(rtile.offset, rtile.stride);

		rtile.buffer = buffers->buffer.device_pointer;
		rtile.rng_state = buffers->rng_state.device_pointer;
		rtile.buffers = buffers;

		device->map_tile(tile_device, rtile);

		return true;
	}

	/* fill buffer parameters */
	BufferParams buffer_params = tile_manager.params;
	buffer_params.full_x = rtile.x;
	buffer_params.full_y = rtile.y;
	buffer_params.width = rtile.w;
	buffer_params.height = rtile.h;

	buffer_params.get_offset_stride(rtile.offset, rtile.stride);

	RenderBuffers *tilebuffers;

	/* allocate buffers */
	if(params.progressive_refine) {
		tile_lock.lock();

		if(tile_buffers.size() == 0)
			tile_buffers.resize(tile_manager.state.num_tiles, NULL);

		tilebuffers = tile_buffers[tile.index];
		if(tilebuffers == NULL) {
			tilebuffers = new RenderBuffers(tile_device);
			tile_buffers[tile.index] = tilebuffers;

			tilebuffers->reset(tile_device, buffer_params);
		}

		tile_lock.unlock();
	}
	else {
		tilebuffers = new RenderBuffers(tile_device);

		tilebuffers->reset(tile_device, buffer_params);
	}

	rtile.buffer = tilebuffers->buffer.device_pointer;
	rtile.rng_state = tilebuffers->rng_state.device_pointer;
	rtile.buffers = tilebuffers;

	/* this will tag tile as IN PROGRESS in blender-side render pipeline,
	 * which is needed to highlight currently rendering tile before first
	 * sample was processed for it
	 */
	update_tile_sample(rtile);

	return true;
}

void Session::update_tile_sample(RenderTile& rtile)
{
	thread_scoped_lock tile_lock(tile_mutex);

	if(update_render_tile_cb) {
		if(params.progressive_refine == false) {
			/* todo: optimize this by making it thread safe and removing lock */

			update_render_tile_cb(rtile);
		}
	}

	update_status_time();
}

void Session::release_tile(RenderTile& rtile)
{
	thread_scoped_lock tile_lock(tile_mutex);

	if(write_render_tile_cb) {
		if(params.progressive_refine == false) {
			/* todo: optimize this by making it thread safe and removing lock */
			write_render_tile_cb(rtile);

			delete rtile.buffers;
		}
	}

	update_status_time();
}

void Session::run_cpu()
{
	bool tiles_written = false;

	last_update_time = time_dt();

	{
		/* reset once to start */
		thread_scoped_lock reset_lock(delayed_reset.mutex);
		thread_scoped_lock buffers_lock(buffers_mutex);
		thread_scoped_lock display_lock(display_mutex);

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
			 * wait for pause condition notify to wake up again */
			thread_scoped_lock pause_lock(pause_mutex);

			if(!pause && delayed_reset.do_reset) {
				/* reset once to start */
				thread_scoped_lock reset_lock(delayed_reset.mutex);
				thread_scoped_lock buffers_lock(buffers_mutex);
				thread_scoped_lock display_lock(display_mutex);

				reset_(delayed_reset.params, delayed_reset.samples);
				delayed_reset.do_reset = false;
			}
			else if(pause || no_tiles) {
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
			 * sample, and released/reacquired on each iteration to allow
			 * reset and draw in between */
			thread_scoped_lock buffers_lock(buffers_mutex);

			/* update scene */
			update_scene();

			if(!device->error_message().empty())
				progress.set_error(device->error_message());

			if(progress.get_cancel())
				break;

			/* update status and timing */
			update_status_time();

			/* path trace */
			path_trace();

			/* update status and timing */
			update_status_time();

			if(!params.background)
				need_tonemap = true;

			if(!device->error_message().empty())
				progress.set_error(device->error_message());
		}

		device->task_wait();

		{
			thread_scoped_lock reset_lock(delayed_reset.mutex);
			thread_scoped_lock buffers_lock(buffers_mutex);
			thread_scoped_lock display_lock(display_mutex);

			if(delayed_reset.do_reset) {
				/* reset rendering if request from main thread */
				delayed_reset.do_reset = false;
				reset_(delayed_reset.params, delayed_reset.samples);
			}
			else if(need_tonemap) {
				/* tonemap only if we do not reset, we don't we don't
				 * want to show the result of an incomplete sample */
				tonemap(tile_manager.state.sample);
			}

			if(!device->error_message().empty())
				progress.set_error(device->error_message());

			tiles_written = update_progressive_refine(progress.get_cancel());
		}

		progress.set_update();
	}

	if(!tiles_written)
		update_progressive_refine(true);
}

void Session::load_kernels()
{
	thread_scoped_lock scene_lock(scene->mutex);

	if(!kernels_loaded) {
		progress.set_status("Loading render kernels (may take a few minutes the first time)");

		if(!device->load_kernels(params.experimental)) {
			string message = device->error_message();
			if(message.empty())
				message = "Failed loading render kernel, see console for errors";

			progress.set_error(message);
			progress.set_status("Error", message);
			progress.set_update();
			return;
		}

		kernels_loaded = true;
	}
}

void Session::run()
{
	/* load kernels */
	load_kernels();

	/* session thread loop */
	progress.set_status("Waiting for render to start");

	/* run */
	if(!progress.get_cancel()) {
		/* reset number of rendered samples */
		progress.reset_sample();

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

bool Session::draw(BufferParams& buffer_params, DeviceDrawParams &draw_params)
{
	if(device_use_gl)
		return draw_gpu(buffer_params, draw_params);
	else
		return draw_cpu(buffer_params, draw_params);
}

void Session::reset_(BufferParams& buffer_params, int samples)
{
	if(buffers) {
		if(buffer_params.modified(buffers->params)) {
			gpu_draw_ready = false;
			buffers->reset(device, buffer_params);
			display->reset(device, buffer_params);
		}
	}

	tile_manager.reset(buffer_params, samples);

	start_time = time_dt();
	preview_time = 0.0;
	paused_time = 0.0;

	if(!params.background)
		progress.set_start_time(start_time + paused_time);
}

void Session::reset(BufferParams& buffer_params, int samples)
{
	if(device_use_gl)
		reset_gpu(buffer_params, samples);
	else
		reset_cpu(buffer_params, samples);

	if(params.progressive_refine) {
		thread_scoped_lock buffers_lock(buffers_mutex);

		foreach(RenderBuffers *buffers, tile_buffers)
			delete buffers;

		tile_buffers.clear();
	}
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

	/* update camera if dimensions changed for progressive render. the camera
	 * knows nothing about progressive or cropped rendering, it just gets the
	 * image dimensions passed in */
	Camera *cam = scene->camera;
	int width = tile_manager.state.buffer.full_width;
	int height = tile_manager.state.buffer.full_height;
	int resolution = tile_manager.state.resolution_divider;

	if(width != cam->width || height != cam->height) {
		cam->width = width;
		cam->height = height;
		cam->resolution = resolution;
		cam->tag_update();
	}

	/* number of samples is needed by multi jittered
	 * sampling pattern and by baking */
	Integrator *integrator = scene->integrator;
	BakeManager *bake_manager = scene->bake_manager;

	if(integrator->sampling_pattern == SAMPLING_PATTERN_CMJ ||
	   bake_manager->get_baking())
	{
		int aa_samples = tile_manager.num_samples;

		if(aa_samples != integrator->aa_samples) {
			integrator->aa_samples = aa_samples;
			integrator->tag_update(scene);
		}
	}

	/* update scene */
	if(scene->need_update()) {
		progress.set_status("Updating Scene");
		scene->device_update(device, progress);
	}
}

void Session::update_status_time(bool show_pause, bool show_done)
{
	int sample = tile_manager.state.sample;
	int resolution = tile_manager.state.resolution_divider;
	int num_tiles = tile_manager.state.num_tiles;
	int tile = tile_manager.state.num_rendered_tiles;

	/* update status */
	string status, substatus;

	if(!params.progressive) {
		bool is_gpu = params.device.type == DEVICE_CUDA || params.device.type == DEVICE_OPENCL;
		bool is_multidevice = params.device.multi_devices.size() > 1;
		bool is_cpu = params.device.type == DEVICE_CPU;

		substatus = string_printf("Path Tracing Tile %d/%d", tile, num_tiles);

		if((is_gpu && !is_multidevice) || (is_cpu && num_tiles == 1)) {
			/* when rendering on GPU multithreading happens within single tile, as in
			 * tiles are handling sequentially and in this case we could display
			 * currently rendering sample number
			 * this helps a lot from feedback point of view.
			 * also display the info on CPU, when using 1 tile only
			 */

			int sample = progress.get_sample(), num_samples = tile_manager.num_samples;

			if(tile > 1) {
				/* sample counter is global for all tiles, subtract samples
				 * from already finished tiles to get sample counter for
				 * current tile only
				 */
				sample -= (tile - 1) * num_samples;
			}

			substatus += string_printf(", Sample %d/%d", sample, num_samples);
		}
	}
	else if(tile_manager.num_samples == USHRT_MAX)
		substatus = string_printf("Path Tracing Sample %d", sample+1);
	else
		substatus = string_printf("Path Tracing Sample %d/%d", sample+1, tile_manager.num_samples);
	
	if(show_pause) {
		status = "Paused";
	}
	else if(show_done) {
		status = "Done";
	}
	else {
		status = substatus;
		substatus.clear();
	}

	progress.set_status(status, substatus);

	/* update timing */
	if(preview_time == 0.0 && resolution == 1)
		preview_time = time_dt();
	
	double tile_time = (tile == 0 || sample == 0)? 0.0: (time_dt() - preview_time - paused_time) / sample;

	/* negative can happen when we pause a bit before rendering, can discard that */
	if(preview_time < 0.0) preview_time = 0.0;

	progress.set_tile(tile, tile_time);
}

void Session::update_progress_sample()
{
	progress.increment_sample();
}

void Session::path_trace()
{
	/* add path trace task */
	DeviceTask task(DeviceTask::PATH_TRACE);
	
	task.acquire_tile = function_bind(&Session::acquire_tile, this, _1, _2);
	task.release_tile = function_bind(&Session::release_tile, this, _1);
	task.get_cancel = function_bind(&Progress::get_cancel, &this->progress);
	task.update_tile_sample = function_bind(&Session::update_tile_sample, this, _1);
	task.update_progress_sample = function_bind(&Session::update_progress_sample, this);
	task.need_finish_queue = params.progressive_refine;
	task.integrator_branched = scene->integrator->method == Integrator::BRANCHED_PATH;

	device->task_add(task);
}

void Session::tonemap(int sample)
{
	/* add tonemap task */
	DeviceTask task(DeviceTask::FILM_CONVERT);

	task.x = tile_manager.state.buffer.full_x;
	task.y = tile_manager.state.buffer.full_y;
	task.w = tile_manager.state.buffer.width;
	task.h = tile_manager.state.buffer.height;
	task.rgba_byte = display->rgba_byte.device_pointer;
	task.rgba_half = display->rgba_half.device_pointer;
	task.buffer = buffers->buffer.device_pointer;
	task.sample = sample;
	tile_manager.state.buffer.get_offset_stride(task.offset, task.stride);

	if(task.w > 0 && task.h > 0) {
		device->task_add(task);
		device->task_wait();

		/* set display to new size */
		display->draw_set(task.w, task.h);
	}

	display_outdated = false;
}

bool Session::update_progressive_refine(bool cancel)
{
	int sample = tile_manager.state.sample + 1;
	bool write = sample == tile_manager.num_samples || cancel;

	double current_time = time_dt();

	if (current_time - last_update_time < 1.0) {
		/* if last sample was processed, we need to write buffers anyway  */
		if (!write)
			return false;
	}

	if(params.progressive_refine) {
		foreach(RenderBuffers *buffers, tile_buffers) {
			RenderTile rtile;
			rtile.buffers = buffers;
			rtile.sample = sample;

			if(write)
				write_render_tile_cb(rtile);
			else
				update_render_tile_cb(rtile);
		}
	}

	last_update_time = current_time;

	return write;
}

void Session::device_free()
{
	scene->device_free();

	foreach(RenderBuffers *buffers, tile_buffers)
		delete buffers;

	tile_buffers.clear();

	/* used from background render only, so no need to
	 * re-create render/display buffers here
	 */
}

CCL_NAMESPACE_END
