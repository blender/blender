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

struct bAction;
struct AnimData;
struct NlaTrack;

namespace ed::outliner {

class TreeElementNLA final : public AbstractTreeElement {
  AnimData &anim_data_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_NLA;

  TreeElementNLA(TreeElement &legacy_te, AnimData &anim_data);

  /** NLA elements have no owner ID, identify them by the animation data. */
  static const void *persistent_ptr(AnimData &anim_data)
  {
    return &anim_data;
  }

  void expand(SpaceOutliner &space_outliner) const override;

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_NLA;
  }
};

class TreeElementNLATrack final : public AbstractTreeElement {
  NlaTrack &track_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_NLA_TRACK;

  TreeElementNLATrack(TreeElement &legacy_te, NlaTrack &track);

  /** NLA tracks have no owner ID, identify them by the track itself. */
  static const void *persistent_ptr(NlaTrack &track)
  {
    return &track;
  }

  void expand(SpaceOutliner &space_outliner) const override;

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_NLA;
  }
};

class TreeElementNLAAction final : public AbstractTreeElement {
 public:
  static constexpr eTreeStoreElemType element_type = TSE_NLA_ACTION;

  TreeElementNLAAction(TreeElement &legacy_te, const bAction &action);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(const bAction &action);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_ACTION;
  }
};

}  // namespace ed::outliner
}  // namespace blender
