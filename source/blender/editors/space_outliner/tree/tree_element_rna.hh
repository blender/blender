/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include <limits>

#include "DNA_outliner_types.h"

#include "RNA_types.hh"

#include "tree_element.hh"

namespace blender {

struct PointerRNA;

namespace ed::outliner {

/**
 * Base class for common behavior of RNA tree elements.
 */
class TreeElementRNACommon : public AbstractTreeElement {
 protected:
  constexpr static int max_index = std::numeric_limits<short>::max();
  PointerRNA rna_ptr_;

 public:
  TreeElementRNACommon(TreeElement &legacy_te, PointerRNA &rna_ptr);
  bool expand_poll(const SpaceOutliner & /*soops*/) const override;

  /**
   * RNA elements are identified by their owning ID where there is one, and by the data they point
   * at otherwise. Overloaded for the sub-types that additionally take an array index.
   * See #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(PointerRNA &rna_ptr);
  static ID *owner_id(PointerRNA &rna_ptr, int index);
  static const void *persistent_ptr(PointerRNA &rna_ptr);
  static const void *persistent_ptr(PointerRNA &rna_ptr, int index);

  const PointerRNA &get_pointer_rna() const;
  /**
   * If this element represents a property or is part of a property (array element), this returns
   * the property. Otherwise nullptr.
   */
  virtual PropertyRNA *get_property_rna() const;

  bool is_rna_valid() const;
};

/* -------------------------------------------------------------------- */

class TreeElementRNAStruct : public TreeElementRNACommon {
 public:
  static constexpr eTreeStoreElemType element_type = TSE_RNA_STRUCT;

  TreeElementRNAStruct(TreeElement &legacy_te, PointerRNA &rna_ptr);
  void expand(SpaceOutliner &space_outliner) const override;

  std::optional<BIFIconID> get_icon() const override;
};

/* -------------------------------------------------------------------- */

class TreeElementRNAProperty : public TreeElementRNACommon {
 private:
  PropertyRNA *rna_prop_ = nullptr;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_RNA_PROPERTY;

  TreeElementRNAProperty(TreeElement &legacy_te, PointerRNA &rna_ptr, int index);
  void expand(SpaceOutliner &space_outliner) const override;

  PropertyRNA *get_property_rna() const override;
};

/* -------------------------------------------------------------------- */

class TreeElementRNAArrayElement : public TreeElementRNACommon {
 public:
  static constexpr eTreeStoreElemType element_type = TSE_RNA_ARRAY_ELEM;

  TreeElementRNAArrayElement(TreeElement &legacy_te, PointerRNA &rna_ptr, int index);

  PropertyRNA *get_property_rna() const override;
};

}  // namespace ed::outliner
}  // namespace blender
