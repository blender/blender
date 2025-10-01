/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <mutex>

#include "BLI_resource_scope.hh"

#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"
#include "BKE_volume_grid_fwd.hh"

#include "NOD_geometry_nodes_bundle_fwd.hh"
#include "NOD_geometry_nodes_closure_fwd.hh"
#include "NOD_geometry_nodes_list_fwd.hh"

#include "spreadsheet_data_source.hh"

struct bContext;

namespace blender::ed::spreadsheet {

class GeometryDataSource : public DataSource {
 private:
  /**
   * Object that contains original data for the geometry component. This is used for selection
   * filtering. May be null.
   */
  Object *object_orig_;
  const bke::GeometrySet geometry_set_;
  const bke::GeometryComponent *component_;
  bke::AttrDomain domain_;
  bool show_internal_attributes_;
  /* Layer index for grease pencil component. */
  int layer_index_;

  /* Some data is computed on the fly only when it is requested. Computing it does not change the
   * logical state of this data source. Therefore, the corresponding methods are const and need to
   * be protected with a mutex. */
  mutable Mutex mutex_;
  mutable ResourceScope scope_;

 public:
  GeometryDataSource(Object *object_orig,
                     bke::GeometrySet geometry_set,
                     const bke::GeometryComponent::Type component_type,
                     const bke::AttrDomain domain,
                     const bool show_internal_attributes,
                     const int layer_index = -1)
      : object_orig_(object_orig),
        geometry_set_(std::move(geometry_set)),
        component_(geometry_set_.get_component(component_type)),
        domain_(domain),
        show_internal_attributes_(show_internal_attributes),
        layer_index_(layer_index)
  {
  }

  bool has_selection_filter() const override;
  IndexMask apply_selection_filter(IndexMaskMemory &memory) const;

  void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const override;

  std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID &column_id) const override;

  int tot_rows() const override;

 private:
  std::optional<const bke::AttributeAccessor> get_component_attributes() const;
  bool display_attribute(StringRef name, bke::AttrDomain domain) const;
};

class VolumeDataSource : public DataSource {
  const bke::GeometrySet geometry_set_;
  const bke::VolumeComponent *component_;

 public:
  VolumeDataSource(bke::GeometrySet geometry_set)
      : geometry_set_(std::move(geometry_set)),
        component_(geometry_set_.get_component<bke::VolumeComponent>())
  {
  }

  void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const override;

  std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID &column_id) const override;

  int tot_rows() const override;
};

#ifdef WITH_OPENVDB

class VolumeGridDataSource : public DataSource {
  /** Using #unique_ptr so that `BKE_volume_grid_fwd.hh` can be used. */
  std::unique_ptr<bke::GVolumeGrid> grid_;

 public:
  VolumeGridDataSource(const bke::GVolumeGrid &grid);

  void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const override;

  std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID &column_id) const override;

  int tot_rows() const override;
};

#endif

class ListDataSource : public DataSource {
  nodes::ListPtr list_;

 public:
  ListDataSource(nodes::ListPtr list);

  void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const override;

  std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID &column_id) const override;

  int tot_rows() const override;
};

class BundleDataSource : public DataSource {
  nodes::BundlePtr bundle_;
  Vector<std::string> flat_item_keys_;
  Vector<const nodes::BundleItemValue *> flat_items_;

 public:
  BundleDataSource(nodes::BundlePtr bundle);

  void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const override;

  std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID &column_id) const override;

  int tot_rows() const override;

 private:
  void collect_flat_items(const nodes::Bundle &bundle, StringRef parent_path);
};

class ClosureSignatureDataSource : public DataSource {
  nodes::ClosurePtr closure_;
  SpreadsheetClosureInputOutput in_out_;

 public:
  ClosureSignatureDataSource(nodes::ClosurePtr closure, SpreadsheetClosureInputOutput in_out);

  void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const override;

  std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID &column_id) const override;

  int tot_rows() const override;
};

class SingleValueDataSource : public DataSource {
  GVArray value_gvarray_;

 public:
  SingleValueDataSource(const GPointer value);

  void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const override;

  std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID &column_id) const override;

  int tot_rows() const override;
};

int get_instance_reference_icon(const bke::InstanceReference &reference);

std::unique_ptr<DataSource> data_source_from_geometry(const bContext *C, Object *object_eval);

bke::GeometrySet get_geometry_set_for_instance_ids(const bke::GeometrySet &root_geometry,
                                                   const Span<SpreadsheetInstanceID> instance_ids);

}  // namespace blender::ed::spreadsheet
