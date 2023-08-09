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
