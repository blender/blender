/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender {

struct bAction;
struct AnimData;
struct NlaTrack;

namespace ed::outliner {

class TreeElementNLA final : public AbstractTreeElement {
  AnimData &anim_data_;

 public:
  TreeElementNLA(TreeElement &legacy_te, AnimData &anim_data);

  void expand(SpaceOutliner &space_outliner) const override;

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_NLA;
  }
};

class TreeElementNLATrack final : public AbstractTreeElement {
  NlaTrack &track_;

 public:
  TreeElementNLATrack(TreeElement &legacy_te, NlaTrack &track);

  void expand(SpaceOutliner &space_outliner) const override;

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_NLA;
  }
};

class TreeElementNLAAction final : public AbstractTreeElement {
 public:
  TreeElementNLAAction(TreeElement &legacy_te, const bAction &action);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_ACTION;
  }
};

}  // namespace ed::outliner
}  // namespace blender
