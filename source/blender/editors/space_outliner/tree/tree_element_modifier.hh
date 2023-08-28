/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

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

namespace blender::ed::outliner {

class TreeElementModifierBase final : public AbstractTreeElement {
  Object &object_;

 public:
  TreeElementModifierBase(TreeElement &legacy_te, Object &object);
  void expand(SpaceOutliner &) const override;
};

class TreeElementModifier final : public AbstractTreeElement {
  /* Not needed right now, avoid unused member variable warning. */
  Object &object_;
  ModifierDataStoreElem &md_;

 public:
  TreeElementModifier(TreeElement &legacy_te, Object &object, ModifierDataStoreElem &md);
  void expand(SpaceOutliner &) const override;
};

}  // namespace blender::ed::outliner
