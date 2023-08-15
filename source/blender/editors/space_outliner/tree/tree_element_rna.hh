/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include <limits>

#include "RNA_types.hh"

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
  bool expandPoll(const SpaceOutliner &) const override;

  const PointerRNA &getPointerRNA() const;
  /**
   * If this element represents a property or is part of a property (array element), this returns
   * the property. Otherwise nullptr.
   */
  virtual PropertyRNA *getPropertyRNA() const;

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

  PropertyRNA *getPropertyRNA() const override;
};

/* -------------------------------------------------------------------- */

class TreeElementRNAArrayElement : public TreeElementRNACommon {
 public:
  TreeElementRNAArrayElement(TreeElement &legacy_te, PointerRNA &rna_ptr, int index);

  PropertyRNA *getPropertyRNA() const override;
};

}  // namespace blender::ed::outliner
