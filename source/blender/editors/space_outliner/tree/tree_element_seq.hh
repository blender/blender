/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_sequence_types.h"

#include "tree_element.hh"

struct Sequence;
struct Strip;

namespace blender::ed::outliner {

class TreeElementSequence : public AbstractTreeElement {
  Sequence &sequence_;

 public:
  TreeElementSequence(TreeElement &legacy_te, Sequence &sequence);

  bool expandPoll(const SpaceOutliner &) const override;
  void expand(SpaceOutliner &) const override;

  Sequence &getSequence() const;
  SequenceType getSequenceType() const;
};

/* -------------------------------------------------------------------- */

class TreeElementSequenceStrip : public AbstractTreeElement {
 public:
  TreeElementSequenceStrip(TreeElement &legacy_te, Strip &strip);
};

/* -------------------------------------------------------------------- */

class TreeElementSequenceStripDuplicate : public AbstractTreeElement {
  Sequence &sequence_;

 public:
  TreeElementSequenceStripDuplicate(TreeElement &legacy_te, Sequence &sequence);

  Sequence &getSequence() const;
};

}  // namespace blender::ed::outliner
