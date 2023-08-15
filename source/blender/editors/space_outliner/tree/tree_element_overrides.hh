/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "RNA_types.hh"

#include "BLI_string_ref.hh"

#include "tree_element.hh"

struct ID;
struct IDOverrideLibraryProperty;
struct IDOverrideLibraryPropertyOperation;

namespace blender::ed::outliner {

struct TreeElementOverridesData {
  ID &id;
  IDOverrideLibraryProperty &override_property;
  PointerRNA &override_rna_ptr;
  PropertyRNA &override_rna_prop;

  bool is_rna_path_valid;

  /* In case the property references a specific operation. Only used for collection overrides
   * currently, where a single override may add/remove multiple collection items (only add
   * currently). */
  IDOverrideLibraryPropertyOperation *operation = nullptr;
};

class TreeElementOverridesBase final : public AbstractTreeElement {
 public:
  ID &id;

 public:
  TreeElementOverridesBase(TreeElement &legacy_te, ID &id);

  void expand(SpaceOutliner &) const override;

  StringRefNull getWarning() const override;
};

/**
 * Represent a single overridden property. Collection properties may support multiple override
 * operations, e.g. to insert/remove multiple collection items. For these multiple operation cases,
 * use #TreeElementOverridesPropertyOperation.
 */
class TreeElementOverridesProperty : public AbstractTreeElement {
 public:
  PointerRNA override_rna_ptr;
  PropertyRNA &override_rna_prop;

  StringRefNull rna_path;
  bool is_rna_path_valid;

 public:
  TreeElementOverridesProperty(TreeElement &legacy_te, TreeElementOverridesData &override_data);

  StringRefNull getWarning() const override;

  bool isCollectionOperation() const;
};

/**
 * Represent a single operation within an overridden property. While usually a single override
 * property represents a single operation (changing the value), a single overridden collection
 * property may have multiple operations, e.g. to insert or remove collection items.
 *
 * Inherits from the override property class since it should look/behave mostly the same.
 */
class TreeElementOverridesPropertyOperation final : public TreeElementOverridesProperty {
  /** See #TreeElementOverridesData::operation. Operations are recreated as part of the diffing
   * (e.g. on undo pushes) so store a copy of the data here. */
  std::unique_ptr<IDOverrideLibraryPropertyOperation> operation_;

 public:
  TreeElementOverridesPropertyOperation(TreeElement &legacy_te,
                                        TreeElementOverridesData &override_data);

  /** Return a short string to display in the right column of the properties mode, indicating what
   * the override operation did (e.g. added or removed a collection item). */
  StringRefNull getOverrideOperationLabel() const;
  std::optional<BIFIconID> getIcon() const override;

 private:
  std::optional<PointerRNA> get_collection_ptr() const;
};

}  // namespace blender::ed::outliner
