/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_outliner_types.h"

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
  static constexpr eTreeStoreElemType element_type = TSE_LINKED_PSYS;

  TreeElementParticleSystem(TreeElement &legacy_te, Object &object, ParticleSystem &psys);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Object &object, ParticleSystem &psys);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_PARTICLES;
  }
};

}  // namespace ed::outliner
}  // namespace blender
