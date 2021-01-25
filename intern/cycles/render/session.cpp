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

#include <limits.h>
#include <string.h>

#include "device/device.h"
#include "render/bake.h"
#include "render/buffers.h"
#include "render/camera.h"
#include "render/graph.h"
#include "render/integrator.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/session.h"

#include "util/util_foreach.h"
#include "util/util_function.h"
#include "util/util_logging.h"
#include "util/util_math.h"
#include "util/util_opengl.h"
#include "util/util_task.h"
#include "util/util_time.h"

CCL_NAMESPACE_BEGIN

/* Note about  preserve_tile_device option for tile manager:
 * progressive refine and viewport rendering does requires tiles to
 * always be allocated for the same device
 */
Session::Session(const SessionParams &params_)
    : params(params_),
      tile_manager(params.progressive,
                   params.samples,
                   params.tile_size,
                   params.start_resolution,
                   params.background == false || params.progressive_refine,
                   params.background,
                   params.tile_order,
                   max(params.device.multi_devices.size(), 1),
                   params.pixel_size),
      stats(),
      profiler()
{
  device_use_gl = ((params.device.type != DEVICE_CPU) && !params.background);

  TaskScheduler::init(params.threads);

  session_thread = NULL;
  scene = NULL;

  reset_time = 0.0;
  last_update_time = 0.0;

  delayed_reset.do_reset = false;
  delayed_reset.samples = 0;

  display_outdated = false;
  gpu_draw_ready = false;
  gpu_need_display_buffer_update = false;
  pause = false;

  buffers = NULL;
  display = NULL;

  /* Validate denoising parameters. */
  set_denoising(params.denoising);

  /* Create CPU/GPU devices. */
  device = Device::create(params.device, stats, profiler, params.background);

  if (!device->error_message().empty()) {
    progress.set_error(device->error_message());
    return;
  }

  /* Create buffers for interactive rendering. */
  if (!(params.background && !params.write_render_cb)) {
    buffers = new RenderBuffers(device);
    display = new DisplayBuffer(device, params.display_buffer_linear);
  }
}

Session::~Session()
{
  cancel();

  if (buffers && params.write_render_cb) {
    /* Copy to display buffer and write out image if requested */
    delete display;

    display = new DisplayBuffer(device, false);
    display->reset(buffers->params);
    copy_to_display_buffer(params.samples);

    int w = display->draw_width;
    int h = display->draw_height;
    uchar4 *pixels = display->rgba_byte.copy_from_device(0, w, h);
    params.write_render_cb((uchar *)pixels, w, h, 4);
  }

  /* clean up */
  tile_manager.device_free();

  delete buffers;
  delete display;
  delete scene;
  delete device;

  TaskScheduler::exit();
}

void Session::start()
{
  if (!session_thread) {
    session_thread = new thread(function_bind(&Session::run, this));
  }
}

void Session::cancel()
{
  if (session_thread) {
    /* wait for session thread to end */
    progress.set_cancel("Exiting");

    gpu_need_display_buffer_update = false;
    gpu_need_display_buffer_update_cond.notify_all();

    {
      thread_scoped_lock pause_lock(pause_mutex);
      pause = false;
    }
    pause_cond.notify_all();

    wait();
  }
}

bool Session::ready_to_reset()
{
  double dt = time_dt() - reset_time;

  if (!display_outdated)
    return (dt > params.reset_timeout);
  else
    return (dt > params.cancel_timeout);
}

/* GPU Session */

void Session::reset_gpu(BufferParams &buffer_params, int samples)
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

  gpu_need_display_buffer_update = false;
  gpu_need_display_buffer_update_cond.notify_all();

  pause_cond.notify_all();
}

bool Session::draw_gpu(BufferParams &buffer_params, DeviceDrawParams &draw_params)
{
  /* block for buffer access */
  thread_scoped_lock display_lock(display_mutex);

  /* first check we already rendered something */
  if (gpu_draw_ready) {
    /* then verify the buffers have the expected size, so we don't
     * draw previous results in a resized window */
    if (buffer_params.width == display->params.width &&
        buffer_params.height == display->params.height) {
      /* for CUDA we need to do tone-mapping still, since we can
       * only access GL buffers from the main thread. */
      if (gpu_need_display_buffer_update) {
        thread_scoped_lock buffers_lock(buffers_mutex);
        copy_to_display_buffer(tile_manager.state.sample);
        gpu_need_display_buffer_update = false;
        gpu_need_display_buffer_update_cond.notify_all();
      }

      display->draw(device, draw_params);

      if (display_outdated && (time_dt() - reset_time) > params.text_timeout)
        return false;

      return true;
    }
  }

  return false;
}

void Session::run_gpu()
{
  bool tiles_written = false;

  reset_time = time_dt();
  last_update_time = time_dt();
  last_display_time = last_update_time;

  progress.set_render_start_time();

  while (!progress.get_cancel()) {
    /* advance to next tile */
    bool no_tiles = !tile_manager.next();

    DeviceKernelStatus kernel_state = DEVICE_KERNEL_UNKNOWN;
    if (no_tiles) {
      kernel_state = device->get_active_kernel_switch_state();
    }

    if (params.background) {
      /* if no work left and in background mode, we can stop immediately */
      if (no_tiles) {
        progress.set_status("Finished");
        break;
      }
    }

    /* Don't go in pause mode when image was rendered with preview kernels
     * When feature kernels become available the session will be reset. */
    else if (no_tiles && kernel_state == DEVICE_KERNEL_WAITING_FOR_FEATURE_KERNEL) {
      time_sleep(0.1);
    }
    else if (no_tiles && kernel_state == DEVICE_KERNEL_FEATURE_KERNEL_AVAILABLE) {
      reset_gpu(tile_manager.params, params.samples);
    }

    else {
      /* if in interactive mode, and we are either paused or done for now,
       * wait for pause condition notify to wake up again */
      thread_scoped_lock pause_lock(pause_mutex);

      if (!pause && !tile_manager.done()) {
        /* reset could have happened after no_tiles was set, before this lock.
         * in this case we shall not wait for pause condition
         */
      }
      else if (pause || no_tiles) {
        update_status_time(pause, no_tiles);

        while (1) {
          scoped_timer pause_timer;
          pause_cond.wait(pause_lock);
          if (pause) {
            progress.add_skip_time(pause_timer, params.background);
          }

          update_status_time(pause, no_tiles);
          progress.set_update();

          if (!pause)
            break;
        }
      }

      if (progress.get_cancel())
        break;
    }

    if (!no_tiles) {
      /* update scene */
      scoped_timer update_timer;
      if (update_scene()) {
        profiler.reset(scene->shaders.size(), scene->objects.size());
      }
      progress.add_skip_time(update_timer, params.background);

      if (!device->error_message().empty())
        progress.set_error(device->error_message());

      if (progress.get_cancel())
        break;

      /* buffers mutex is locked entirely while rendering each
       * sample, and released/reacquired on each iteration to allow
       * reset and draw in between */
      thread_scoped_lock buffers_lock(buffers_mutex);

      /* update status and timing */
      update_status_time();

      /* render */
      bool delayed_denoise = false;
      const bool need_denoise = render_need_denoise(delayed_denoise);
      render(need_denoise);

      device->task_wait();

      if (!device->error_message().empty())
        progress.set_cancel(device->error_message());

      /* update status and timing */
      update_status_time();

      gpu_need_display_buffer_update = !delayed_denoise;
      gpu_draw_ready = true;
      progress.set_update();

      /* wait for until display buffer is updated */
      if (!params.background) {
        while (gpu_need_display_buffer_update) {
          if (progress.get_cancel())
            break;

          gpu_need_display_buffer_update_cond.wait(buffers_lock);
        }
      }

      if (!device->error_message().empty())
        progress.set_error(device->error_message());

      tiles_written = update_progressive_refine(progress.get_cancel());

      if (progress.get_cancel())
        break;
    }
  }

  if (!tiles_written)
    update_progressive_refine(true);
}

/* CPU Session */

void Session::reset_cpu(BufferParams &buffer_params, int samples)
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

bool Session::draw_cpu(BufferParams &buffer_params, DeviceDrawParams &draw_params)
{
  thread_scoped_lock display_lock(display_mutex);

  /* first check we already rendered something */
  if (display->draw_ready()) {
    /* then verify the buffers have the expected size, so we don't
     * draw previous results in a resized window */
    if (buffer_params.width == display->params.width &&
        buffer_params.height == display->params.height) {
      display->draw(device, draw_params);

      if (display_outdated && (time_dt() - reset_time) > params.text_timeout)
        return false;

      return true;
    }
  }

  return false;
}

bool Session::steal_tile(RenderTile &rtile, Device *tile_device, thread_scoped_lock &tile_lock)
{
  /* Devices that can get their tiles stolen don't steal tiles themselves.
   * Additionally, if there are no stealable tiles in flight, give up here. */
  if (tile_device->info.type == DEVICE_CPU || stealable_tiles == 0) {
    return false;
  }

  /* Wait until no other thread is trying to steal a tile. */
  while (tile_stealing_state != NOT_STEALING && stealable_tiles > 0) {
    /* Someone else is currently trying to get a tile.
     * Wait on the condition variable and try later. */
    tile_steal_cond.wait(tile_lock);
  }
  /* If another thread stole the last stealable tile in the meantime, give up. */
  if (stealable_tiles == 0) {
    return false;
  }

  /* There are stealable tiles in flight, so signal that one should be released. */
  tile_stealing_state = WAITING_FOR_TILE;

  /* Wait until a device notices the signal and releases its tile. */
  while (tile_stealing_state != GOT_TILE && stealable_tiles > 0) {
    tile_steal_cond.wait(tile_lock);
  }
  /* If the last stealable tile finished on its own, give up. */
  if (tile_stealing_state != GOT_TILE) {
    tile_stealing_state = NOT_STEALING;
    return false;
  }

  /* Successfully stole a tile, now move it to the new device. */
  rtile = stolen_tile;
  rtile.buffers->buffer.move_device(tile_device);
  rtile.buffer = rtile.buffers->buffer.device_pointer;
  rtile.stealing_state = RenderTile::NO_STEALING;
  rtile.num_samples -= (rtile.sample - rtile.start_sample);
  rtile.start_sample = rtile.sample;

  tile_stealing_state = NOT_STEALING;

  /* Poke any threads which might be waiting for NOT_STEALING above. */
  tile_steal_cond.notify_one();

  return true;
}

bool Session::get_tile_stolen()
{
  /* If tile_stealing_state is WAITING_FOR_TILE, atomically set it to RELEASING_TILE
   * and return true. */
  TileStealingState expected = WAITING_FOR_TILE;
  return tile_stealing_state.compare_exchange_weak(expected, RELEASING_TILE);
}

bool Session::acquire_tile(RenderTile &rtile, Device *tile_device, uint tile_types)
{
  if (progress.get_cancel()) {
    if (params.progressive_refine == false) {
      /* for progressive refine current sample should be finished for all tiles */
      return false;
    }
  }

  thread_scoped_lock tile_lock(tile_mutex);

  /* get next tile from manager */
  Tile *tile;
  int device_num = device->device_number(tile_device);

  while (!tile_manager.next_tile(tile, device_num, tile_types)) {
    /* Can only steal tiles on devices that support rendering
     * This is because denoising tiles cannot be stolen (see below)
     */
    if ((tile_types & (RenderTile::PATH_TRACE | RenderTile::BAKE)) &&
        steal_tile(rtile, tile_device, tile_lock)) {
      return true;
    }

    /* Wait for denoising tiles to become available */
    if ((tile_types & RenderTile::DENOISE) && !progress.get_cancel() && tile_manager.has_tiles()) {
      denoising_cond.wait(tile_lock);
      continue;
    }

    return false;
  }

  /* fill render tile */
  rtile.x = tile_manager.state.buffer.full_x + tile->x;
  rtile.y = tile_manager.state.buffer.full_y + tile->y;
  rtile.w = tile->w;
  rtile.h = tile->h;
  rtile.start_sample = tile_manager.state.sample;
  rtile.num_samples = tile_manager.state.num_samples;
  rtile.resolution = tile_manager.state.resolution_divider;
  rtile.tile_index = tile->index;
  rtile.stealing_state = RenderTile::NO_STEALING;

  if (tile->state == Tile::DENOISE) {
    rtile.task = RenderTile::DENOISE;
  }
  else {
    if (tile_device->info.type == DEVICE_CPU) {
      stealable_tiles++;
      rtile.stealing_state = RenderTile::CAN_BE_STOLEN;
    }

    if (read_bake_tile_cb) {
      rtile.task = RenderTile::BAKE;
    }
    else {
      rtile.task = RenderTile::PATH_TRACE;
    }
  }

  tile_lock.unlock();

  /* in case of a permanent buffer, return it, otherwise we will allocate
   * a new temporary buffer */
  if (buffers) {
    tile_manager.state.buffer.get_offset_stride(rtile.offset, rtile.stride);

    rtile.buffer = buffers->buffer.device_pointer;
    rtile.buffers = buffers;

    device->map_tile(tile_device, rtile);

    /* Reset copy state, since buffer contents change after the tile was acquired */
    buffers->map_neighbor_copied = false;

    /* This hack ensures that the copy in 'MultiDevice::map_neighbor_tiles' accounts
     * for the buffer resolution divider. */
    buffers->buffer.data_width = (buffers->params.width * buffers->params.get_passes_size()) /
                                 tile_manager.state.resolution_divider;
    buffers->buffer.data_height = buffers->params.height / tile_manager.state.resolution_divider;

    return true;
  }

  if (tile->buffers == NULL) {
    /* fill buffer parameters */
    BufferParams buffer_params = tile_manager.params;
    buffer_params.full_x = rtile.x;
    buffer_params.full_y = rtile.y;
    buffer_params.width = rtile.w;
    buffer_params.height = rtile.h;

    /* allocate buffers */
    tile->buffers = new RenderBuffers(tile_device);
    tile->buffers->reset(buffer_params);
  }
  else if (tile->buffers->buffer.device != tile_device) {
    /* Move buffer to current tile device again in case it was stolen before.
     * Not needed for denoising since that already handles mapping of tiles and
     * neighbors to its own device. */
    if (rtile.task != RenderTile::DENOISE) {
      tile->buffers->buffer.move_device(tile_device);
    }
  }

  tile->buffers->map_neighbor_copied = false;

  tile->buffers->params.get_offset_stride(rtile.offset, rtile.stride);

  rtile.buffer = tile->buffers->buffer.device_pointer;
  rtile.buffers = tile->buffers;
  rtile.sample = tile_manager.state.sample;

  if (read_bake_tile_cb) {
    /* This will read any passes needed as input for baking. */
    if (tile_manager.state.sample == tile_manager.range_start_sample) {
      {
        thread_scoped_lock tile_lock(tile_mutex);
        read_bake_tile_cb(rtile);
      }
      rtile.buffers->buffer.copy_to_device();
    }
  }
  else {
    /* This will tag tile as IN PROGRESS in blender-side render pipeline,
     * which is needed to highlight currently rendering tile before first
     * sample was processed for it. */
    update_tile_sample(rtile);
  }

  return true;
}

void Session::update_tile_sample(RenderTile &rtile)
{
  thread_scoped_lock tile_lock(tile_mutex);

  if (update_render_tile_cb) {
    if (params.progressive_refine == false) {
      /* todo: optimize this by making it thread safe and removing lock */

      update_render_tile_cb(rtile, true);
    }
  }

  update_status_time();
}

void Session::release_tile(RenderTile &rtile, const bool need_denoise)
{
  thread_scoped_lock tile_lock(tile_mutex);

  if (rtile.stealing_state != RenderTile::NO_STEALING) {
    stealable_tiles--;
    if (rtile.stealing_state == RenderTile::WAS_STOLEN) {
      /* If the tile is being stolen, don't release it here - the new device will pick up where
       * the old one left off. */

      assert(tile_stealing_state == RELEASING_TILE);
      assert(rtile.sample < rtile.start_sample + rtile.num_samples);

      tile_stealing_state = GOT_TILE;
      stolen_tile = rtile;
      tile_steal_cond.notify_all();
      return;
    }
    else if (stealable_tiles == 0) {
      /* If this was the last stealable tile, wake up any threads still waiting for one. */
      tile_steal_cond.notify_all();
    }
  }

  progress.add_finished_tile(rtile.task == RenderTile::DENOISE);

  bool delete_tile;

  if (tile_manager.finish_tile(rtile.tile_index, need_denoise, delete_tile)) {
    /* Finished tile pixels write. */
    if (write_render_tile_cb && params.progressive_refine == false) {
      write_render_tile_cb(rtile);
    }

    if (delete_tile) {
      delete rtile.buffers;
      tile_manager.state.tiles[rtile.tile_index].buffers = NULL;
    }
  }
  else {
    /* In progress tile pixels update. */
    if (update_render_tile_cb && params.progressive_refine == false) {
      update_render_tile_cb(rtile, false);
    }
  }

  update_status_time();

  /* Notify denoising thread that a tile was finished. */
  denoising_cond.notify_all();
}

void Session::map_neighbor_tiles(RenderTileNeighbors &neighbors, Device *tile_device)
{
  thread_scoped_lock tile_lock(tile_mutex);

  const int4 image_region = make_int4(
      tile_manager.state.buffer.full_x,
      tile_manager.state.buffer.full_y,
      tile_manager.state.buffer.full_x + tile_manager.state.buffer.width,
      tile_manager.state.buffer.full_y + tile_manager.state.buffer.height);

  RenderTile &center_tile = neighbors.tiles[RenderTileNeighbors::CENTER];

  if (!tile_manager.schedule_denoising) {
    /* Fix up tile slices with overlap. */
    if (tile_manager.slice_overlap != 0) {
      int y = max(center_tile.y - tile_manager.slice_overlap, image_region.y);
      center_tile.h = min(center_tile.y + center_tile.h + tile_manager.slice_overlap,
                          image_region.w) -
                      y;
      center_tile.y = y;
    }

    /* Tiles are not being denoised individually, which means the entire image is processed. */
    neighbors.set_bounds_from_center();
  }
  else {
    int center_idx = center_tile.tile_index;
    assert(tile_manager.state.tiles[center_idx].state == Tile::DENOISE);

    for (int dy = -1, i = 0; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++, i++) {
        RenderTile &rtile = neighbors.tiles[i];
        int nindex = tile_manager.get_neighbor_index(center_idx, i);
        if (nindex >= 0) {
          Tile *tile = &tile_manager.state.tiles[nindex];

          rtile.x = image_region.x + tile->x;
          rtile.y = image_region.y + tile->y;
          rtile.w = tile->w;
          rtile.h = tile->h;

          if (buffers) {
            tile_manager.state.buffer.get_offset_stride(rtile.offset, rtile.stride);

            rtile.buffer = buffers->buffer.device_pointer;
            rtile.buffers = buffers;
          }
          else {
            assert(tile->buffers);
            tile->buffers->params.get_offset_stride(rtile.offset, rtile.stride);

            rtile.buffer = tile->buffers->buffer.device_pointer;
            rtile.buffers = tile->buffers;
          }
        }
        else {
          int px = center_tile.x + dx * params.tile_size.x;
          int py = center_tile.y + dy * params.tile_size.y;

          rtile.x = clamp(px, image_region.x, image_region.z);
          rtile.y = clamp(py, image_region.y, image_region.w);
          rtile.w = rtile.h = 0;

          rtile.buffer = (device_ptr)NULL;
          rtile.buffers = NULL;
        }
      }
    }
  }

  assert(center_tile.buffers);
  device->map_neighbor_tiles(tile_device, neighbors);

  /* The denoised result is written back to the original tile. */
  neighbors.target = center_tile;
}

void Session::unmap_neighbor_tiles(RenderTileNeighbors &neighbors, Device *tile_device)
{
  thread_scoped_lock tile_lock(tile_mutex);
  device->unmap_neighbor_tiles(tile_device, neighbors);
}

void Session::run_cpu()
{
  bool tiles_written = false;

  last_update_time = time_dt();
  last_display_time = last_update_time;

  {
    /* reset once to start */
    thread_scoped_lock reset_lock(delayed_reset.mutex);
    thread_scoped_lock buffers_lock(buffers_mutex);
    thread_scoped_lock display_lock(display_mutex);

    reset_(delayed_reset.params, delayed_reset.samples);
    delayed_reset.do_reset = false;
  }

  while (!progress.get_cancel()) {
    /* advance to next tile */
    bool no_tiles = !tile_manager.next();
    bool need_copy_to_display_buffer = false;

    DeviceKernelStatus kernel_state = DEVICE_KERNEL_UNKNOWN;
    if (no_tiles) {
      kernel_state = device->get_active_kernel_switch_state();
    }

    if (params.background) {
      /* if no work left and in background mode, we can stop immediately */
      if (no_tiles) {
        progress.set_status("Finished");
        break;
      }
    }

    /* Don't go in pause mode when preview kernels are used
     * When feature kernels become available the session will be reset. */
    else if (no_tiles && kernel_state == DEVICE_KERNEL_WAITING_FOR_FEATURE_KERNEL) {
      time_sleep(0.1);
    }
    else if (no_tiles && kernel_state == DEVICE_KERNEL_FEATURE_KERNEL_AVAILABLE) {
      reset_cpu(tile_manager.params, params.samples);
    }

    else {
      /* if in interactive mode, and we are either paused or done for now,
       * wait for pause condition notify to wake up again */
      thread_scoped_lock pause_lock(pause_mutex);

      if (!pause && delayed_reset.do_reset) {
        /* reset once to start */
        thread_scoped_lock reset_lock(delayed_reset.mutex);
        thread_scoped_lock buffers_lock(buffers_mutex);
        thread_scoped_lock display_lock(display_mutex);

        reset_(delayed_reset.params, delayed_reset.samples);
        delayed_reset.do_reset = false;
      }
      else if (pause || no_tiles) {
        update_status_time(pause, no_tiles);

        while (1) {
          scoped_timer pause_timer;
          pause_cond.wait(pause_lock);
          if (pause) {
            progress.add_skip_time(pause_timer, params.background);
          }

          update_status_time(pause, no_tiles);
          progress.set_update();

          if (!pause)
            break;
        }
      }

      if (progress.get_cancel())
        break;
    }

    if (!no_tiles) {
      /* update scene */
      scoped_timer update_timer;
      if (update_scene()) {
        profiler.reset(scene->shaders.size(), scene->objects.size());
      }
      progress.add_skip_time(update_timer, params.background);

      if (!device->error_message().empty())
        progress.set_error(device->error_message());

      if (progress.get_cancel())
        break;

      /* buffers mutex is locked entirely while rendering each
       * sample, and released/reacquired on each iteration to allow
       * reset and draw in between */
      thread_scoped_lock buffers_lock(buffers_mutex);

      /* update status and timing */
      update_status_time();

      /* render */
      bool delayed_denoise = false;
      const bool need_denoise = render_need_denoise(delayed_denoise);
      render(need_denoise);

      /* update status and timing */
      update_status_time();

      if (!params.background)
        need_copy_to_display_buffer = !delayed_denoise;

      if (!device->error_message().empty())
        progress.set_error(device->error_message());
    }

    device->task_wait();

    {
      thread_scoped_lock reset_lock(delayed_reset.mutex);
      thread_scoped_lock buffers_lock(buffers_mutex);
      thread_scoped_lock display_lock(display_mutex);

      if (delayed_reset.do_reset) {
        /* reset rendering if request from main thread */
        delayed_reset.do_reset = false;
        reset_(delayed_reset.params, delayed_reset.samples);
      }
      else if (need_copy_to_display_buffer) {
        /* Only copy to display_buffer if we do not reset, we don't
         * want to show the result of an incomplete sample */
        copy_to_display_buffer(tile_manager.state.sample);
      }

      if (!device->error_message().empty())
        progress.set_error(device->error_message());

      tiles_written = update_progressive_refine(progress.get_cancel());
    }

    progress.set_update();
  }

  if (!tiles_written)
    update_progressive_refine(true);
}

void Session::run()
{
  if (params.use_profiling && (params.device.type == DEVICE_CPU)) {
    profiler.start();
  }

  /* session thread loop */
  progress.set_status("Waiting for render to start");

  /* run */
  if (!progress.get_cancel()) {
    /* reset number of rendered samples */
    progress.reset_sample();

    if (device_use_gl)
      run_gpu();
    else
      run_cpu();
  }

  profiler.stop();

  /* progress update */
  if (progress.get_cancel())
    progress.set_status(progress.get_cancel_message());
  else
    progress.set_update();
}

bool Session::draw(BufferParams &buffer_params, DeviceDrawParams &draw_params)
{
  if (device_use_gl)
    return draw_gpu(buffer_params, draw_params);
  else
    return draw_cpu(buffer_params, draw_params);
}

void Session::reset_(BufferParams &buffer_params, int samples)
{
  if (buffers && buffer_params.modified(tile_manager.params)) {
    gpu_draw_ready = false;
    buffers->reset(buffer_params);
    if (display) {
      display->reset(buffer_params);
    }
  }

  tile_manager.reset(buffer_params, samples);
  stealable_tiles = 0;
  tile_stealing_state = NOT_STEALING;
  progress.reset_sample();

  bool show_progress = params.background || tile_manager.get_num_effective_samples() != INT_MAX;
  progress.set_total_pixel_samples(show_progress ? tile_manager.state.total_pixel_samples : 0);

  if (!params.background)
    progress.set_start_time();
  progress.set_render_start_time();
}

void Session::reset(BufferParams &buffer_params, int samples)
{
  if (device_use_gl)
    reset_gpu(buffer_params, samples);
  else
    reset_cpu(buffer_params, samples);
}

void Session::set_samples(int samples)
{
  if (samples != params.samples) {
    params.samples = samples;
    tile_manager.set_samples(samples);

    pause_cond.notify_all();
  }
}

void Session::set_pause(bool pause_)
{
  bool notify = false;

  {
    thread_scoped_lock pause_lock(pause_mutex);

    if (pause != pause_) {
      pause = pause_;
      notify = true;
    }
  }

  if (session_thread) {
    if (notify) {
      pause_cond.notify_all();
    }
  }
  else if (pause_) {
    update_status_time(pause_);
  }
}

void Session::set_denoising(const DenoiseParams &denoising)
{
  bool need_denoise = denoising.need_denoising_task();

  /* Lock buffers so no denoising operation is triggered while the settings are changed here. */
  thread_scoped_lock buffers_lock(buffers_mutex);
  params.denoising = denoising;

  if (!(params.device.denoisers & denoising.type)) {
    if (need_denoise) {
      progress.set_error("Denoiser type not supported by compute device");
    }

    params.denoising.use = false;
    need_denoise = false;
  }

  // TODO(pmours): Query the required overlap value for denoising from the device?
  tile_manager.slice_overlap = need_denoise && !params.background ? 64 : 0;

  /* Schedule per tile denoising for final renders if we are either denoising or
   * need prefiltered passes for the native denoiser. */
  tile_manager.schedule_denoising = need_denoise && !buffers;
}

void Session::set_denoising_start_sample(int sample)
{
  if (sample != params.denoising.start_sample) {
    params.denoising.start_sample = sample;

    pause_cond.notify_all();
  }
}

void Session::wait()
{
  if (session_thread) {
    session_thread->join();
    delete session_thread;
  }

  session_thread = NULL;
}

bool Session::update_scene()
{
  thread_scoped_lock scene_lock(scene->mutex);

  /* update camera if dimensions changed for progressive render. the camera
   * knows nothing about progressive or cropped rendering, it just gets the
   * image dimensions passed in */
  Camera *cam = scene->camera;
  int width = tile_manager.state.buffer.full_width;
  int height = tile_manager.state.buffer.full_height;
  int resolution = tile_manager.state.resolution_divider;

  cam->set_screen_size_and_resolution(width, height, resolution);

  /* number of samples is needed by multi jittered
   * sampling pattern and by baking */
  Integrator *integrator = scene->integrator;
  BakeManager *bake_manager = scene->bake_manager;

  if (integrator->get_sampling_pattern() != SAMPLING_PATTERN_SOBOL || bake_manager->get_baking()) {
    integrator->set_aa_samples(tile_manager.num_samples);
  }

  bool kernel_switch_needed = false;
  if (scene->update(progress, kernel_switch_needed)) {
    if (kernel_switch_needed) {
      reset(tile_manager.params, params.samples);
    }
    return true;
  }
  return false;
}

void Session::update_status_time(bool show_pause, bool show_done)
{
  int progressive_sample = tile_manager.state.sample;
  int num_samples = tile_manager.get_num_effective_samples();

  int tile = progress.get_rendered_tiles();
  int num_tiles = tile_manager.state.num_tiles;

  /* update status */
  string status, substatus;

  if (!params.progressive) {
    const bool is_cpu = params.device.type == DEVICE_CPU;
    const bool rendering_finished = (tile == num_tiles);
    const bool is_last_tile = (tile + 1) == num_tiles;

    substatus = string_printf("Rendered %d/%d Tiles", tile, num_tiles);

    if (!rendering_finished && (device->show_samples() || (is_cpu && is_last_tile))) {
      /* Some devices automatically support showing the sample number:
       * - CUDADevice
       * - OpenCLDevice when using the megakernel (the split kernel renders multiple
       *   samples at the same time, so the current sample isn't really defined)
       * - CPUDevice when using one thread
       * For these devices, the current sample is always shown.
       *
       * The other option is when the last tile is currently being rendered by the CPU.
       */
      substatus += string_printf(", Sample %d/%d", progress.get_current_sample(), num_samples);
    }
    if (params.denoising.use && params.denoising.type != DENOISER_OPENIMAGEDENOISE) {
      substatus += string_printf(", Denoised %d tiles", progress.get_denoised_tiles());
    }
    else if (params.denoising.store_passes && params.denoising.type == DENOISER_NLM) {
      substatus += string_printf(", Prefiltered %d tiles", progress.get_denoised_tiles());
    }
  }
  else if (tile_manager.num_samples == Integrator::MAX_SAMPLES)
    substatus = string_printf("Path Tracing Sample %d", progressive_sample + 1);
  else
    substatus = string_printf("Path Tracing Sample %d/%d", progressive_sample + 1, num_samples);

  if (show_pause) {
    status = "Rendering Paused";
  }
  else if (show_done) {
    status = "Rendering Done";
    progress.set_end_time(); /* Save end time so that further calls to get_time are accurate. */
  }
  else {
    status = substatus;
    substatus.clear();
  }

  progress.set_status(status, substatus);
}

bool Session::render_need_denoise(bool &delayed)
{
  delayed = false;

  /* Not supported yet for baking. */
  if (read_bake_tile_cb) {
    return false;
  }

  /* Denoising enabled? */
  if (!params.denoising.need_denoising_task()) {
    return false;
  }

  if (params.background) {
    /* Background render, only denoise when rendering the last sample. */
    return tile_manager.done();
  }

  /* Viewport render. */

  /* It can happen that denoising was already enabled, but the scene still needs an update. */
  if (scene->film->is_modified() || !scene->film->get_denoising_data_offset()) {
    return false;
  }

  /* Immediately denoise when we reach the start sample or last sample. */
  const int num_samples_finished = tile_manager.state.sample + 1;
  if (num_samples_finished == params.denoising.start_sample ||
      num_samples_finished == params.samples) {
    return true;
  }

  /* Do not denoise until the sample at which denoising should start is reached. */
  if (num_samples_finished < params.denoising.start_sample) {
    return false;
  }

  /* Avoid excessive denoising in viewport after reaching a certain amount of samples. */
  delayed = (tile_manager.state.sample >= 20 &&
             (time_dt() - last_display_time) < params.progressive_update_timeout);
  return !delayed;
}

void Session::render(bool need_denoise)
{
  if (buffers && tile_manager.state.sample == tile_manager.range_start_sample) {
    /* Clear buffers. */
    buffers->zero();
  }

  if (tile_manager.state.buffer.width == 0 || tile_manager.state.buffer.height == 0) {
    return; /* Avoid empty launches. */
  }

  /* Add path trace task. */
  DeviceTask task(DeviceTask::RENDER);

  task.acquire_tile = function_bind(&Session::acquire_tile, this, _2, _1, _3);
  task.release_tile = function_bind(&Session::release_tile, this, _1, need_denoise);
  task.map_neighbor_tiles = function_bind(&Session::map_neighbor_tiles, this, _1, _2);
  task.unmap_neighbor_tiles = function_bind(&Session::unmap_neighbor_tiles, this, _1, _2);
  task.get_cancel = function_bind(&Progress::get_cancel, &this->progress);
  task.update_tile_sample = function_bind(&Session::update_tile_sample, this, _1);
  task.update_progress_sample = function_bind(&Progress::add_samples, &this->progress, _1, _2);
  task.get_tile_stolen = function_bind(&Session::get_tile_stolen, this);
  task.need_finish_queue = params.progressive_refine;
  task.integrator_branched = scene->integrator->get_method() == Integrator::BRANCHED_PATH;

  task.adaptive_sampling.use = (scene->integrator->get_sampling_pattern() ==
                                SAMPLING_PATTERN_PMJ) &&
                               scene->dscene.data.film.pass_adaptive_aux_buffer;
  task.adaptive_sampling.min_samples = scene->dscene.data.integrator.adaptive_min_samples;
  task.adaptive_sampling.adaptive_step = scene->dscene.data.integrator.adaptive_step;

  /* Acquire render tiles by default. */
  task.tile_types = RenderTile::PATH_TRACE;

  if (need_denoise) {
    task.denoising = params.denoising;

    task.pass_stride = scene->film->get_pass_stride();
    task.target_pass_stride = task.pass_stride;
    task.pass_denoising_data = scene->film->get_denoising_data_offset();
    task.pass_denoising_clean = scene->film->get_denoising_clean_offset();

    task.denoising_from_render = true;

    if (tile_manager.schedule_denoising) {
      /* Acquire denoising tiles during rendering. */
      task.tile_types |= RenderTile::DENOISE;
    }
    else {
      assert(buffers);

      /* Schedule rendering and wait for it to finish. */
      device->task_add(task);
      device->task_wait();

      /* Then run denoising on the whole image at once. */
      task.type = DeviceTask::DENOISE_BUFFER;
      task.x = tile_manager.state.buffer.full_x;
      task.y = tile_manager.state.buffer.full_y;
      task.w = tile_manager.state.buffer.width;
      task.h = tile_manager.state.buffer.height;
      task.buffer = buffers->buffer.device_pointer;
      task.sample = tile_manager.state.sample;
      task.num_samples = tile_manager.state.num_samples;
      tile_manager.state.buffer.get_offset_stride(task.offset, task.stride);
      task.buffers = buffers;
    }
  }

  device->task_add(task);
}

void Session::copy_to_display_buffer(int sample)
{
  /* add film conversion task */
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

  if (task.w > 0 && task.h > 0) {
    device->task_add(task);
    device->task_wait();

    /* set display to new size */
    display->draw_set(task.w, task.h);

    last_display_time = time_dt();
  }

  display_outdated = false;
}

bool Session::update_progressive_refine(bool cancel)
{
  int sample = tile_manager.state.sample + 1;
  bool write = sample == tile_manager.num_samples || cancel;

  double current_time = time_dt();

  if (current_time - last_update_time < params.progressive_update_timeout) {
    /* if last sample was processed, we need to write buffers anyway  */
    if (!write && sample != 1)
      return false;
  }

  if (params.progressive_refine) {
    foreach (Tile &tile, tile_manager.state.tiles) {
      if (!tile.buffers) {
        continue;
      }

      RenderTile rtile;
      rtile.x = tile_manager.state.buffer.full_x + tile.x;
      rtile.y = tile_manager.state.buffer.full_y + tile.y;
      rtile.w = tile.w;
      rtile.h = tile.h;
      rtile.sample = sample;
      rtile.buffers = tile.buffers;

      if (write) {
        if (write_render_tile_cb)
          write_render_tile_cb(rtile);
      }
      else {
        if (update_render_tile_cb)
          update_render_tile_cb(rtile, true);
      }
    }
  }

  last_update_time = current_time;

  return write;
}

void Session::device_free()
{
  scene->device_free();

  tile_manager.device_free();

  /* used from background render only, so no need to
   * re-create render/display buffers here
   */
}

void Session::collect_statistics(RenderStats *render_stats)
{
  scene->collect_statistics(render_stats);
  if (params.use_profiling && (params.device.type == DEVICE_CPU)) {
    render_stats->collect_profiling(scene, profiler);
  }
}

CCL_NAMESPACE_END
