/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender {

struct Object;
struct ParticleSystem;

namespace ed::outliner {

class TreeElementParticleSystem final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  // Object &object_;
  ParticleSystem &psys_;

 public:
  TreeElementParticleSystem(TreeElement &legacy_te, Object &object, ParticleSystem &psys);
  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_PARTICLES;
  }
};

}  // namespace ed::outliner
}  // namespace blender
