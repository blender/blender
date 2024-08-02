/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 *
 * A depth pass that write surface depth when it is needed.
 * It is also used for selecting non overlay-only objects.
 */

#pragma once

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

class Prepass {
 private:
  PassMain ps_ = {"prepass"};

 public:
  void begin_sync(Resources &res, const State &state)
  {
    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | state.clipping_state);
    ps_.shader_set(res.shaders.depth_mesh.get());
    res.select_bind(ps_);
  }

  void object_sync(Manager &manager, const ObjectRef &ob_ref, Resources &res)
  {
    /* TODO(fclem) This function should contain what `basic_cache_populate` contained. */

    gpu::Batch *geom = DRW_cache_object_surface_get(ob_ref.object);
    if (geom) {
      ResourceHandle res_handle = manager.resource_handle(ob_ref);
      ps_.draw(geom, res_handle, res.select_id(ob_ref).get());
    }
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    /* Should be fine to use the line buffer since the prepass only writes to the depth buffer. */
    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }
};

}  // namespace blender::draw::overlay
