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

struct AnimData;

namespace ed::outliner {

class TreeElementDriverBase final : public AbstractTreeElement {
  AnimData &anim_data_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_DRIVER_BASE;

  TreeElementDriverBase(TreeElement &legacy_te, AnimData &anim_data);

  /** Driver bases have no owner ID, identify them by the animation data. */
  static const void *persistent_ptr(AnimData &anim_data)
  {
    return &anim_data;
  }

  void expand(SpaceOutliner &space_outliner) const override;

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_DRIVER;
  }
};

}  // namespace ed::outliner
}  // namespace blender
