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
  static constexpr eTreeStoreElemType element_type = TSE_STRIP;

  TreeElementStrip(TreeElement &legacy_te, Strip &strip);

  /** Strips have no owner ID, identify them by the strip itself. */
  static const void *persistent_ptr(Strip &strip)
  {
    return &strip;
  }

  bool expand_poll(const SpaceOutliner & /*soops*/) const override;
  void expand(SpaceOutliner & /*soops*/) const override;

  Strip &get_strip() const;
  std::optional<BIFIconID> get_icon() const override;
};

/* -------------------------------------------------------------------- */

class TreeElementStripData : public AbstractTreeElement {
 public:
  static constexpr eTreeStoreElemType element_type = TSE_STRIP_DATA;

  TreeElementStripData(TreeElement &legacy_te, StripData &strip);

  /** Strip data has no owner ID, identify it by the data itself. */
  static const void *persistent_ptr(StripData &strip)
  {
    return &strip;
  }

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_LIBRARY_DATA_DIRECT;
  }
};

/* -------------------------------------------------------------------- */

class TreeElementStripDuplicate : public AbstractTreeElement {
  Strip &strip_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_STRIP_DUP;

  TreeElementStripDuplicate(TreeElement &legacy_te, Strip &strip);

  /** Strips have no owner ID, identify them by the strip itself. */
  static const void *persistent_ptr(Strip &strip)
  {
    return &strip;
  }

  Strip &get_strip() const;

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_SEQ_STRIP_DUPLICATE;
  }
};

}  // namespace ed::outliner
}  // namespace blender
