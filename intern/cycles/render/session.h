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

#ifndef __SESSION_H__
#define __SESSION_H__

#include "render/buffers.h"
#include "device/device.h"
#include "render/shader.h"
#include "render/tile.h"

#include "util/util_progress.h"
#include "util/util_stats.h"
#include "util/util_thread.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class BufferParams;
class Device;
class DeviceScene;
class DeviceRequestedFeatures;
class DisplayBuffer;
class Progress;
class RenderBuffers;
class Scene;

/* Session Parameters */

class SessionParams {
public:
	DeviceInfo device;
	bool background;
	bool progressive_refine;

	bool progressive;
	bool experimental;
	int samples;
	int2 tile_size;
	TileOrder tile_order;
	int start_resolution;
	int pixel_size;
	int threads;

	bool display_buffer_linear;

	bool use_denoising;
	int denoising_radius;
	float denoising_strength;
	float denoising_feature_strength;
	bool denoising_relative_pca;

	double cancel_timeout;
	double reset_timeout;
	double text_timeout;
	double progressive_update_timeout;

	ShadingSystem shadingsystem;

	function<bool(const uchar *pixels,
	              int width,
	              int height,
	              int channels)> write_render_cb;

	SessionParams()
	{
		background = false;
		progressive_refine = false;

		progressive = false;
		experimental = false;
		samples = INT_MAX;
		tile_size = make_int2(64, 64);
		start_resolution = INT_MAX;
		pixel_size = 1;
		threads = 0;

		use_denoising = false;
		denoising_radius = 8;
		denoising_strength = 0.0f;
		denoising_feature_strength = 0.0f;
		denoising_relative_pca = false;

		display_buffer_linear = false;

		cancel_timeout = 0.1;
		reset_timeout = 0.1;
		text_timeout = 1.0;
		progressive_update_timeout = 1.0;

		shadingsystem = SHADINGSYSTEM_SVM;
		tile_order = TILE_CENTER;
	}

	bool modified(const SessionParams& params)
	{ return !(device == params.device
		&& background == params.background
		&& progressive_refine == params.progressive_refine
		/* && samples == params.samples */
		&& progressive == params.progressive
		&& experimental == params.experimental
		&& tile_size == params.tile_size
		&& start_resolution == params.start_resolution
		&& pixel_size == params.pixel_size
		&& threads == params.threads
		&& display_buffer_linear == params.display_buffer_linear
		&& cancel_timeout == params.cancel_timeout
		&& reset_timeout == params.reset_timeout
		&& text_timeout == params.text_timeout
		&& progressive_update_timeout == params.progressive_update_timeout
		&& tile_order == params.tile_order
		&& shadingsystem == params.shadingsystem); }

};

/* Session
 *
 * This is the class that contains the session thread, running the render
 * control loop and dispatching tasks. */

class Session {
public:
	Device *device;
	Scene *scene;
	RenderBuffers *buffers;
	DisplayBuffer *display;
	Progress progress;
	SessionParams params;
	TileManager tile_manager;
	Stats stats;

	function<void(RenderTile&)> write_render_tile_cb;
	function<void(RenderTile&, bool)> update_render_tile_cb;

	explicit Session(const SessionParams& params);
	~Session();

	void start();
	bool draw(BufferParams& params, DeviceDrawParams& draw_params);
	void wait();

	bool ready_to_reset();
	void reset(BufferParams& params, int samples);
	void set_samples(int samples);
	void set_pause(bool pause);

	void update_scene();
	void load_kernels(bool lock_scene=true);

	void device_free();

	/* Returns the rendering progress or 0 if no progress can be determined
	 * (for example, when rendering with unlimited samples). */
	float get_progress();

protected:
	struct DelayedReset {
		thread_mutex mutex;
		bool do_reset;
		BufferParams params;
		int samples;
	} delayed_reset;

	void run();

	void update_status_time(bool show_pause = false, bool show_done = false);

	void tonemap(int sample);
	void render();
	void reset_(BufferParams& params, int samples);

	void run_cpu();
	bool draw_cpu(BufferParams& params, DeviceDrawParams& draw_params);
	void reset_cpu(BufferParams& params, int samples);

	void run_gpu();
	bool draw_gpu(BufferParams& params, DeviceDrawParams& draw_params);
	void reset_gpu(BufferParams& params, int samples);

	bool acquire_tile(Device *tile_device, RenderTile& tile);
	void update_tile_sample(RenderTile& tile);
	void release_tile(RenderTile& tile);

	void map_neighbor_tiles(RenderTile *tiles, Device *tile_device);
	void unmap_neighbor_tiles(RenderTile *tiles, Device *tile_device);

	bool device_use_gl;

	thread *session_thread;

	volatile bool display_outdated;

	volatile bool gpu_draw_ready;
	volatile bool gpu_need_tonemap;
	thread_condition_variable gpu_need_tonemap_cond;

	bool pause;
	thread_condition_variable pause_cond;
	thread_mutex pause_mutex;
	thread_mutex tile_mutex;
	thread_mutex buffers_mutex;
	thread_mutex display_mutex;

	bool kernels_loaded;
	DeviceRequestedFeatures loaded_kernel_features;

	double reset_time;

	/* progressive refine */
	double last_update_time;
	bool update_progressive_refine(bool cancel);

	DeviceRequestedFeatures get_requested_device_features();

	/* ** Split kernel routines ** */

	/* Maximumnumber of closure during session lifetime. */
	int max_closure_global;

	/* Get maximum number of closures to be used in kernel. */
	int get_max_closure_count();
};

CCL_NAMESPACE_END

#endif /* __SESSION_H__ */
