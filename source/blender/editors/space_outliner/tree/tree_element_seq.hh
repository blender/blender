/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_sequence_types.h"

#include "tree_element.hh"

namespace blender {

struct Strip;
struct StripData;

namespace ed::outliner {

class TreeElementStrip : public AbstractTreeElement {
  Strip &strip_;

 public:
  TreeElementStrip(TreeElement &legacy_te, Strip &strip);

  bool expand_poll(const SpaceOutliner & /*soops*/) const override;
  void expand(SpaceOutliner & /*soops*/) const override;

  Strip &get_strip() const;
  std::optional<BIFIconID> get_icon() const override;
};

/* -------------------------------------------------------------------- */

class TreeElementStripData : public AbstractTreeElement {
 public:
  TreeElementStripData(TreeElement &legacy_te, StripData &strip);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_LIBRARY_DATA_DIRECT;
  }
};

/* -------------------------------------------------------------------- */

class TreeElementStripDuplicate : public AbstractTreeElement {
  Strip &strip_;

 public:
  TreeElementStripDuplicate(TreeElement &legacy_te, Strip &strip);

  Strip &get_strip() const;

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_SEQ_STRIP_DUPLICATE;
  }
};

}  // namespace ed::outliner
}  // namespace blender
