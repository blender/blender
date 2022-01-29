/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup spoutliner
 */

#pragma once

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
