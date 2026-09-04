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

enum ModifierDataStoreType { MODIFIER_TYPE, GPENCIL_MODIFIER_TYPE };

struct ModifierDataStoreElem {
  union {
    ModifierData *md;
    GpencilModifierData *gp_md;
  };
  ModifierDataStoreType type;

  ModifierDataStoreElem(ModifierData *md_) : md(md_), type(MODIFIER_TYPE) {}
  ModifierDataStoreElem(GpencilModifierData *md_) : gp_md(md_), type(GPENCIL_MODIFIER_TYPE) {}
};

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
  ModifierDataStoreElem &md_;

 public:
  static constexpr eTreeStoreElemType element_type = TSE_MODIFIER;

  TreeElementModifier(TreeElement &legacy_te, Object &object, ModifierDataStoreElem &md);
  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(Object &object, ModifierDataStoreElem &md);

  std::optional<BIFIconID> get_icon() const override;

 private:
  /** Add a #TreeElementLinkedObject child for \a object, if the modifier references one. */
  void add_linked_object(Object *object) const;
};

}  // namespace ed::outliner
}  // namespace blender
