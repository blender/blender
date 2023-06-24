/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include <string>

#include "UI_resources.h"

#include "tree_element.hh"

namespace blender::ed::outliner {

/**
 * A basic, general purpose tree element to just display a label and an icon. Can be used to group
 * together items underneath as well of course.
 *
 * Make sure to give this a unique index, so the element can be identified uniquely. Otherwise
 * glitches like multiple highlighted elements happen, that share all state (e.g. collapsed,
 * selected, etc.).
 */
class TreeElementLabel final : public AbstractTreeElement {
  const std::string label_;
  BIFIconID icon_ = ICON_NONE;

 public:
  TreeElementLabel(TreeElement &legacy_te, const char *label);

  void setIcon(BIFIconID icon);
  std::optional<BIFIconID> getIcon() const override;
};

}  // namespace blender::ed::outliner
