/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "RNA_types.h"

#include "BLI_string_ref.hh"

#include "tree_element.hh"

struct ID;
struct IDOverrideLibraryProperty;

namespace blender::ed::outliner {

struct TreeElementOverridesData {
  ID &id;
  IDOverrideLibraryProperty &override_property;
  PointerRNA &override_rna_ptr;
  PropertyRNA &override_rna_prop;

  bool is_rna_path_valid;
};

class TreeElementOverridesBase final : public AbstractTreeElement {
 public:
  ID &id;

 public:
  TreeElementOverridesBase(TreeElement &legacy_te, ID &id);

  void expand(SpaceOutliner &) const override;

  StringRefNull getWarning() const override;
};

class TreeElementOverridesProperty final : public AbstractTreeElement {
 public:
  PointerRNA override_rna_ptr;
  PropertyRNA &override_rna_prop;

  StringRefNull rna_path;
  bool is_rna_path_valid;

 public:
  TreeElementOverridesProperty(TreeElement &legacy_te, TreeElementOverridesData &override_data);

  StringRefNull getWarning() const override;
};

}  // namespace blender::ed::outliner
