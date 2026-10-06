/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_outliner_types.h"

#include "tree_element.hh"

namespace blender {

struct GpencilModifierData;
struct ModifierData;
struct Object;

namespace ed::outliner {

class TreeElementModifierBase final : public AbstractTreeElement {
  Object &object_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_MODIFIER_BASE;

  TreeElementModifierBase(TreeElement &legacy_te, Object &object);
  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Object &object);

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_MODIFIER_DATA;
  }
};

class TreeElementModifier final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  Object &object_;
  ModifierData &modifier_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_MODIFIER;

  TreeElementModifier(TreeElement &legacy_te, Object &object, ModifierData &modifier);
  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Object &object, ModifierData &modifier);

  std::optional<BIFIconID> get_icon() const override;

 private:
  /** Add a #TreeElementLinkedObject child for \a object, if the modifier references one. */
  void add_linked_object(Object *object) const;
};

}  // namespace ed::outliner
}  // namespace blender
