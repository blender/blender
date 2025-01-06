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

class TreeElementSequence : public AbstractTreeElement {
  Strip &sequence_;

 public:
  TreeElementSequence(TreeElement &legacy_te, Strip &sequence);

  bool expand_poll(const SpaceOutliner &) const override;
  void expand(SpaceOutliner &) const override;

  Strip &get_sequence() const;
  SequenceType get_sequence_type() const;
};

/* -------------------------------------------------------------------- */

class TreeElementSequenceStrip : public AbstractTreeElement {
 public:
  TreeElementSequenceStrip(TreeElement &legacy_te, StripData &strip);
};

/* -------------------------------------------------------------------- */

class TreeElementSequenceStripDuplicate : public AbstractTreeElement {
  Strip &sequence_;

 public:
  TreeElementSequenceStripDuplicate(TreeElement &legacy_te, Strip &sequence);

  Strip &get_sequence() const;
};

}  // namespace blender::ed::outliner
