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

#ifndef __SESSION_H__
#define __SESSION_H__

#include "buffers.h"
#include "device.h"
#include "tile.h"

#include "util_progress.h"
#include "util_thread.h"

CCL_NAMESPACE_BEGIN

class BufferParams;
class Device;
class DeviceScene;
class DisplayBuffer;
class Progress;
class RenderBuffers;
class Scene;

/* Session Parameters */

class SessionParams {
public:
	DeviceInfo device;
	bool background;
	string output_path;

	bool progressive;
	bool experimental;
	int samples;
	int tile_size;
	int min_size;
	int threads;

	double cancel_timeout;
	double reset_timeout;
	double text_timeout;

	SessionParams()
	{
		background = false;
		output_path = "";

		progressive = false;
		experimental = false;
		samples = INT_MAX;
		tile_size = 64;
		min_size = 64;
		threads = 0;

		cancel_timeout = 0.1;
		reset_timeout = 0.1;
		text_timeout = 1.0;
	}

	bool modified(const SessionParams& params)
	{ return !(device.type == params.device.type
		&& device.id == params.device.id
		&& background == params.background
		&& output_path == params.output_path
		/* && samples == params.samples */
		&& progressive == params.progressive
		&& experimental == params.experimental
		&& tile_size == params.tile_size
		&& min_size == params.min_size
		&& threads == params.threads
		&& cancel_timeout == params.cancel_timeout
		&& reset_timeout == params.reset_timeout
		&& text_timeout == params.text_timeout); }

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
	int sample;

	Session(const SessionParams& params);
	~Session();

	void start();
	bool draw(BufferParams& params);
	void wait();

	bool ready_to_reset();
	void reset(BufferParams& params, int samples);
	void set_samples(int samples);
	void set_pause(bool pause);

protected:
	struct DelayedReset {
		thread_mutex mutex;
		bool do_reset;
		BufferParams params;
		int samples;
	} delayed_reset;

	void run();

	void update_scene();
	void update_status_time(bool show_pause = false, bool show_done = false);

	void tonemap();
	void path_trace(Tile& tile);
	void reset_(BufferParams& params, int samples);

	void run_cpu();
	bool draw_cpu(BufferParams& params);
	void reset_cpu(BufferParams& params, int samples);

	void run_gpu();
	bool draw_gpu(BufferParams& params);
	void reset_gpu(BufferParams& params, int samples);

	TileManager tile_manager;
	bool device_use_gl;

	thread *session_thread;

	volatile bool display_outdated;

	volatile bool gpu_draw_ready;
	volatile bool gpu_need_tonemap;
	thread_condition_variable gpu_need_tonemap_cond;

	bool pause;
	thread_condition_variable pause_cond;
	thread_mutex pause_mutex;

	double start_time;
	double reset_time;
	double preview_time;
	double paused_time;
};

CCL_NAMESPACE_END

#endif /* __SESSION_H__ */

