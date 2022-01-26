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

#include <limits>

#include "RNA_types.h"

#include "tree_element.hh"

struct PointerRNA;

namespace blender::ed::outliner {

/**
 * Base class for common behavior of RNA tree elements.
 */
class TreeElementRNACommon : public AbstractTreeElement {
 protected:
  constexpr static int max_index = std::numeric_limits<short>::max();
  PointerRNA rna_ptr_;

 public:
  TreeElementRNACommon(TreeElement &legacy_te, PointerRNA &rna_ptr);
  bool isExpandValid() const override;
  bool expandPoll(const SpaceOutliner &) const override;

  bool isRNAValid() const;
};

/* -------------------------------------------------------------------- */

class TreeElementRNAStruct : public TreeElementRNACommon {
 public:
  TreeElementRNAStruct(TreeElement &legacy_te, PointerRNA &rna_ptr);
  void expand(SpaceOutliner &space_outliner) const override;
};

/* -------------------------------------------------------------------- */

class TreeElementRNAProperty : public TreeElementRNACommon {
 private:
  PropertyRNA *rna_prop_ = nullptr;

 public:
  TreeElementRNAProperty(TreeElement &legacy_te, PointerRNA &rna_ptr, int index);
  void expand(SpaceOutliner &space_outliner) const override;
};

/* -------------------------------------------------------------------- */

class TreeElementRNAArrayElement : public TreeElementRNACommon {
 public:
  TreeElementRNAArrayElement(TreeElement &legacy_te, PointerRNA &rna_ptr, int index);
};

}  // namespace blender::ed::outliner
