/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#include "render_types.h"

#include "BLI_ghash.h"

#include "BKE_colortools.h"

#include "RE_compositor.hh"
#include "RE_engine.h"

#include "render_result.h"

BaseRender::~BaseRender()
{
  if (engine) {
    RE_engine_free(engine);
  }

  render_result_free(result);

  BLI_rw_mutex_end(&resultmutex);
  BLI_mutex_end(&engine_draw_mutex);
}

Render::~Render()
{
  RE_compositor_free(*this);

  RE_blender_gpu_context_free(this);
  RE_system_gpu_context_free(this);

  BKE_curvemapping_free_data(&r.mblur_shutter_curve);

  render_result_free(pushedresult);
}

void Render::progress(float progress)
{
  if (progress_cb) {
    progress_cb(prh, progress);
  }
}

void Render::draw_lock()
{
  if (draw_lock_cb) {
    draw_lock_cb(dlh, true);
  }
}
void Render::draw_unlock()
{
  if (draw_lock_cb) {
    draw_lock_cb(dlh, false);
  }
}

bool Render::test_break()
{
  if (!test_break_cb) {
    return false;
  }

  return test_break_cb(tbh);
}

bool Render::prepare_viewlayer(struct ViewLayer *view_layer, struct Depsgraph *depsgraph)
{
  if (!prepare_viewlayer_cb) {
    return true;
  }

  return prepare_viewlayer_cb(prepare_vl_handle, view_layer, depsgraph);
}