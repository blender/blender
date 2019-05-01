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

#ifndef __UTIL_PROGRESS_H__
#define __UTIL_PROGRESS_H__

/* Progress
 *
 * Simple class to communicate progress status messages, timing information,
 * update notifications from a job running in another thread. All methods
 * except for the constructor/destructor are thread safe. */

#include "util/util_function.h"
#include "util/util_string.h"
#include "util/util_time.h"
#include "util/util_thread.h"

CCL_NAMESPACE_BEGIN

class Progress {
 public:
  Progress()
  {
    pixel_samples = 0;
    total_pixel_samples = 0;
    current_tile_sample = 0;
    rendered_tiles = 0;
    denoised_tiles = 0;
    start_time = time_dt();
    render_start_time = time_dt();
    end_time = 0.0;
    status = "Initializing";
    substatus = "";
    sync_status = "";
    sync_substatus = "";
    kernel_status = "";
    update_cb = function_null;
    cancel = false;
    cancel_message = "";
    error = false;
    error_message = "";
    cancel_cb = function_null;
  }

  Progress(Progress &progress)
  {
    *this = progress;
  }

  Progress &operator=(Progress &progress)
  {
    thread_scoped_lock lock(progress.progress_mutex);

    progress.get_status(status, substatus);

    pixel_samples = progress.pixel_samples;
    total_pixel_samples = progress.total_pixel_samples;
    current_tile_sample = progress.get_current_sample();

    return *this;
  }

  void reset()
  {
    pixel_samples = 0;
    total_pixel_samples = 0;
    current_tile_sample = 0;
    rendered_tiles = 0;
    denoised_tiles = 0;
    start_time = time_dt();
    render_start_time = time_dt();
    end_time = 0.0;
    status = "Initializing";
    substatus = "";
    sync_status = "";
    sync_substatus = "";
    kernel_status = "";
    cancel = false;
    cancel_message = "";
    error = false;
    error_message = "";
  }

  /* cancel */
  void set_cancel(const string &cancel_message_)
  {
    thread_scoped_lock lock(progress_mutex);
    cancel_message = cancel_message_;
    cancel = true;
  }

  bool get_cancel()
  {
    if (!cancel && cancel_cb)
      cancel_cb();

    return cancel;
  }

  string get_cancel_message()
  {
    thread_scoped_lock lock(progress_mutex);
    return cancel_message;
  }

  void set_cancel_callback(function<void()> function)
  {
    cancel_cb = function;
  }

  /* error */
  void set_error(const string &error_message_)
  {
    thread_scoped_lock lock(progress_mutex);
    error_message = error_message_;
    error = true;
    /* If error happens we also stop rendering. */
    cancel_message = error_message_;
    cancel = true;
  }

  bool get_error()
  {
    return error;
  }

  string get_error_message()
  {
    thread_scoped_lock lock(progress_mutex);
    return error_message;
  }

  /* tile and timing information */

  void set_start_time()
  {
    thread_scoped_lock lock(progress_mutex);

    start_time = time_dt();
    end_time = 0.0;
  }

  void set_render_start_time()
  {
    thread_scoped_lock lock(progress_mutex);

    render_start_time = time_dt();
  }

  void add_skip_time(const scoped_timer &start_timer, bool only_render)
  {
    double skip_time = time_dt() - start_timer.get_start();

    render_start_time += skip_time;
    if (!only_render) {
      start_time += skip_time;
    }
  }

  void get_time(double &total_time_, double &render_time_)
  {
    thread_scoped_lock lock(progress_mutex);

    double time = (end_time > 0) ? end_time : time_dt();

    total_time_ = time - start_time;
    render_time_ = time - render_start_time;
  }

  void set_end_time()
  {
    end_time = time_dt();
  }

  void reset_sample()
  {
    thread_scoped_lock lock(progress_mutex);

    pixel_samples = 0;
    current_tile_sample = 0;
    rendered_tiles = 0;
    denoised_tiles = 0;
  }

  void set_total_pixel_samples(uint64_t total_pixel_samples_)
  {
    thread_scoped_lock lock(progress_mutex);

    total_pixel_samples = total_pixel_samples_;
  }

  float get_progress()
  {
    if (total_pixel_samples > 0) {
      return ((float)pixel_samples) / total_pixel_samples;
    }
    return 0.0f;
  }

  void add_samples(uint64_t pixel_samples_, int tile_sample)
  {
    thread_scoped_lock lock(progress_mutex);

    pixel_samples += pixel_samples_;
    current_tile_sample = tile_sample;
  }

  void add_samples_update(uint64_t pixel_samples_, int tile_sample)
  {
    add_samples(pixel_samples_, tile_sample);
    set_update();
  }

  void add_finished_tile(bool denoised)
  {
    thread_scoped_lock lock(progress_mutex);

    if (denoised) {
      denoised_tiles++;
    }
    else {
      rendered_tiles++;
    }
  }

  int get_current_sample()
  {
    thread_scoped_lock lock(progress_mutex);
    /* Note that the value here always belongs to the last tile that updated,
     * so it's only useful if there is only one active tile. */
    return current_tile_sample;
  }

  int get_rendered_tiles()
  {
    thread_scoped_lock lock(progress_mutex);
    return rendered_tiles;
  }

  int get_denoised_tiles()
  {
    thread_scoped_lock lock(progress_mutex);
    return denoised_tiles;
  }

  /* status messages */

  void set_status(const string &status_, const string &substatus_ = "")
  {
    {
      thread_scoped_lock lock(progress_mutex);
      status = status_;
      substatus = substatus_;
    }

    set_update();
  }

  void set_substatus(const string &substatus_)
  {
    {
      thread_scoped_lock lock(progress_mutex);
      substatus = substatus_;
    }

    set_update();
  }

  void set_sync_status(const string &status_, const string &substatus_ = "")
  {
    {
      thread_scoped_lock lock(progress_mutex);
      sync_status = status_;
      sync_substatus = substatus_;
    }

    set_update();
  }

  void set_sync_substatus(const string &substatus_)
  {
    {
      thread_scoped_lock lock(progress_mutex);
      sync_substatus = substatus_;
    }

    set_update();
  }

  void get_status(string &status_, string &substatus_)
  {
    thread_scoped_lock lock(progress_mutex);

    if (sync_status != "") {
      status_ = sync_status;
      substatus_ = sync_substatus;
    }
    else {
      status_ = status;
      substatus_ = substatus;
    }
  }

  /* kernel status */

  void set_kernel_status(const string &kernel_status_)
  {
    {
      thread_scoped_lock lock(progress_mutex);
      kernel_status = kernel_status_;
    }

    set_update();
  }

  void get_kernel_status(string &kernel_status_)
  {
    thread_scoped_lock lock(progress_mutex);
    kernel_status_ = kernel_status;
  }

  /* callback */

  void set_update()
  {
    if (update_cb) {
      thread_scoped_lock lock(update_mutex);
      update_cb();
    }
  }

  void set_update_callback(function<void()> function)
  {
    update_cb = function;
  }

 protected:
  thread_mutex progress_mutex;
  thread_mutex update_mutex;
  function<void()> update_cb;
  function<void()> cancel_cb;

  /* pixel_samples counts how many samples have been rendered over all pixel, not just per pixel.
   * This makes the progress estimate more accurate when tiles with different sizes are used.
   *
   * total_pixel_samples is the total amount of pixel samples that will be rendered. */
  uint64_t pixel_samples, total_pixel_samples;
  /* Stores the current sample count of the last tile that called the update function.
   * It's used to display the sample count if only one tile is active. */
  int current_tile_sample;
  /* Stores the number of tiles that's already finished.
   * Used to determine whether all but the last tile are finished rendering,
   * in which case the current_tile_sample is displayed. */
  int rendered_tiles, denoised_tiles;

  double start_time, render_start_time;
  /* End time written when render is done, so it doesn't keep increasing on redraws. */
  double end_time;

  string status;
  string substatus;

  string sync_status;
  string sync_substatus;

  string kernel_status;

  volatile bool cancel;
  string cancel_message;

  volatile bool error;
  string error_message;
};

CCL_NAMESPACE_END

#endif /* __UTIL_PROGRESS_H__ */
