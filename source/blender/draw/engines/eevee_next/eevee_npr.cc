/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 */

/**
 * NPR TODOs:
 * - Store eSpaceNode_ShaderFrom in the tree itself.
 * - Disable invalid node types in the NPR tree (BSDF, Shader To RGB, UVs?).
 */

#include "eevee_npr.hh"
#include "eevee_instance.hh"

#include "DNA_material_types.h"
#include "DRW_render.hh"
#include "NOD_shader.h"
#include "gpu_shader_create_info.hh"

#pragma once

#define NPR_SCREEN_SPACE 0

namespace blender::eevee {

void NPRModule::init() {}

void NPRModule::begin_sync()
{
  indices_.clear();
}

int NPRModule::sync_material(::Material *material)
{
  if (!material || !material->nodetree) {
    return 0;
  }

  bNodeTree *ntree = npr_tree_get(material);
  if (!ntree) {
    return 0;
  }

  if (indices_.contains(ntree)) {
    return indices_.lookup(ntree);
  }

  int index = indices_.size() + 1;
  indices_.add(ntree, index);
  return index;
}

void NPRModule::end_sync() {}

}  // namespace blender::eevee
