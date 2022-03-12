/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender::ed::outliner {

struct TreeElementOverridesData {
  ID &id;
  IDOverrideLibraryProperty &override_property;
  bool is_rna_path_valid;
};

class TreeElementOverridesBase final : public AbstractTreeElement {
 public:
  ID &id;

 public:
  TreeElementOverridesBase(TreeElement &legacy_te, ID &id);

  void expand(SpaceOutliner &) const override;
};

class TreeElementOverridesProperty final : public AbstractTreeElement {
  IDOverrideLibraryProperty &override_prop_;

 public:
  TreeElementOverridesProperty(TreeElement &legacy_te, TreeElementOverridesData &override_data);
};

}  // namespace blender::ed::outliner
