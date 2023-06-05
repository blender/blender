/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_next_private.hh"

#include "overlay_next_background.hh"
#include "overlay_next_empty.hh"
#include "overlay_next_grid.hh"
#include "overlay_next_metaball.hh"
#include "overlay_next_prepass.hh"

namespace blender::draw::overlay {

/**
 * Selection engine reuse most of the Overlay engine by creating selection IDs for each
 * selectable component and using a special shaders for drawing.
 */
class Instance {
  const SelectionType selection_type_;

 public:
  /* WORKAROUND: Legacy. Move to grid pass. */
  GPUUniformBuf *grid_ubo = nullptr;

  ShapeCache shapes;

  /** Global types. */
  Resources resources = {selection_type_,
                         overlay::ShaderModule::module_get(selection_type_, false /*TODO*/)};
  State state;

  /** Overlay types. */
  Background background;
  Prepass prepass;
  Metaballs metaballs = {selection_type_};
  Empties empties = {selection_type_};
  Grid grid;

  Instance(const SelectionType selection_type) : selection_type_(selection_type){};

  ~Instance()
  {
    DRW_UBO_FREE_SAFE(grid_ubo);
  }

  void init();
  void begin_sync();
  void object_sync(ObjectRef &ob_ref, Manager &manager);
  void end_sync();
  void draw(Manager &manager);

 private:
  bool object_is_edit_mode(const Object *ob);
};

}  // namespace blender::draw::overlay
