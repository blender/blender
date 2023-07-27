/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

struct ParticleSystem;

namespace blender::ed::outliner {

class TreeElementParticleSystem final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Object &object_;
  ParticleSystem &psys_;

 public:
  TreeElementParticleSystem(TreeElement &legacy_te, Object &object, ParticleSystem &psys);
};

}  // namespace blender::ed::outliner
