/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cmath>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

void IMB_convert_rgba_to_abgr(ImBuf *ibuf)
{
  size_t size;
  uchar rt, *cp = ibuf->byte_buffer.data;
  float rtf, *cpf = ibuf->float_buffer.data;

  if (ibuf->byte_buffer.data) {
    size = ibuf->x * ibuf->y;

    while (size-- > 0) {
      rt = cp[0];
      cp[0] = cp[3];
      cp[3] = rt;
      rt = cp[1];
      cp[1] = cp[2];
      cp[2] = rt;
      cp += 4;
    }
  }

  if (ibuf->float_buffer.data) {
    size = ibuf->x * ibuf->y;

    while (size-- > 0) {
      rtf = cpf[0];
      cpf[0] = cpf[3];
      cpf[3] = rtf;
      rtf = cpf[1];
      cpf[1] = cpf[2];
      cpf[2] = rtf;
      cpf += 4;
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Threaded Image Processing
 * \{ */

static void processor_apply_func(TaskPool *__restrict pool, void *taskdata)
{
  void (*do_thread)(void *) = (void (*)(void *))BLI_task_pool_user_data(pool);
  do_thread(taskdata);
}

void IMB_processor_apply_threaded(
    int buffer_lines,
    int handle_size,
    void *init_customdata,
    void(init_handle)(void *handle, int start_line, int tot_line, void *customdata),
    void *(do_thread)(void *))
{
  const int lines_per_task = 64;

  TaskPool *task_pool;

  void *handles;
  int total_tasks = (buffer_lines + lines_per_task - 1) / lines_per_task;
  int i, start_line;

  task_pool = BLI_task_pool_create(reinterpret_cast<void *>(do_thread), TASK_PRIORITY_HIGH);

  handles = MEM_callocN(handle_size * total_tasks, "processor apply threaded handles");

  start_line = 0;

  for (i = 0; i < total_tasks; i++) {
    int lines_per_current_task;
    void *handle = ((char *)handles) + handle_size * i;

    if (i < total_tasks - 1) {
      lines_per_current_task = lines_per_task;
    }
    else {
      lines_per_current_task = buffer_lines - start_line;
    }

    init_handle(handle, start_line, lines_per_current_task, init_customdata);

    BLI_task_pool_push(task_pool, processor_apply_func, handle, false, nullptr);

    start_line += lines_per_task;
  }

  /* work and wait until tasks are done */
  BLI_task_pool_work_and_wait(task_pool);

  /* Free memory. */
  MEM_freeN(handles);
  BLI_task_pool_free(task_pool);
}

struct ScanlineGlobalData {
  void *custom_data;
  ScanlineThreadFunc do_thread;
};

static void processor_apply_parallel(void *__restrict userdata,
                                     const int scanline,
                                     const TaskParallelTLS *__restrict /*tls*/)
{
  ScanlineGlobalData *data = static_cast<ScanlineGlobalData *>(userdata);
  data->do_thread(data->custom_data, scanline);
}

void IMB_processor_apply_threaded_scanlines(int total_scanlines,
                                            ScanlineThreadFunc do_thread,
                                            void *custom_data)
{
  TaskParallelSettings settings;
  ScanlineGlobalData data = {};
  data.do_thread = do_thread;
  data.custom_data = custom_data;

  BLI_parallel_range_settings_defaults(&settings);
  BLI_task_parallel_range(0, total_scanlines, &data, processor_apply_parallel, &settings);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Alpha-under
 * \{ */

void IMB_alpha_under_color_float(float *rect_float, int x, int y, float backcol[3])
{
  size_t a = size_t(x) * y;
  float *fp = rect_float;

  while (a--) {
    const float mul = 1.0f - fp[3];
    madd_v3_v3fl(fp, backcol, mul);
    fp[3] = 1.0f;

    fp += 4;
  }
}

void IMB_alpha_under_color_byte(uchar *rect, int x, int y, const float backcol[3])
{
  size_t a = size_t(x) * y;
  uchar *cp = rect;

  while (a--) {
    if (cp[3] == 255) {
      /* pass */
    }
    else if (cp[3] == 0) {
      cp[0] = backcol[0] * 255;
      cp[1] = backcol[1] * 255;
      cp[2] = backcol[2] * 255;
    }
    else {
      float alpha = cp[3] / 255.0;
      float mul = 1.0f - alpha;

      cp[0] = (cp[0] * alpha) + mul * backcol[0];
      cp[1] = (cp[1] * alpha) + mul * backcol[1];
      cp[2] = (cp[2] * alpha) + mul * backcol[2];
    }

    cp[3] = 255;

    cp += 4;
  }
}

/** \} */
