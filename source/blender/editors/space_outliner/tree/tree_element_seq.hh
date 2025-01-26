/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_sequence_types.h"

#include "tree_element.hh"

struct Strip;
struct StripData;

namespace blender::ed::outliner {

class TreeElementStrip : public AbstractTreeElement {
  Strip &strip_;

 public:
  TreeElementStrip(TreeElement &legacy_te, Strip &strip);

  bool expand_poll(const SpaceOutliner & /*soops*/) const override;
  void expand(SpaceOutliner & /*soops*/) const override;

  Strip &get_strip() const;
  StripType get_strip_type() const;
};

/* -------------------------------------------------------------------- */

class TreeElementStripData : public AbstractTreeElement {
 public:
  TreeElementStripData(TreeElement &legacy_te, StripData &strip);
};

/* -------------------------------------------------------------------- */

class TreeElementStripDuplicate : public AbstractTreeElement {
  Strip &strip_;

 public:
  TreeElementStripDuplicate(TreeElement &legacy_te, Strip &strip);

  Strip &get_strip() const;
};

}  // namespace blender::ed::outliner
