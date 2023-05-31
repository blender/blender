/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <mutex>

#include "BLI_resource_scope.hh"

#include "BKE_geometry_set.hh"

#include "spreadsheet_data_source.hh"

struct bContext;

namespace blender::ed::spreadsheet {

/**
 * Contains additional named columns that should be displayed that are not stored on the geometry
 * directly. This is used for displaying the evaluated fields connected to a viewer node.
 */
class ExtraColumns {
 private:
  /** Maps column names to their data. The data is actually stored in the spreadsheet cache. */
  Map<std::string, GSpan> columns_;

 public:
  void add(std::string name, GSpan data)
  {
    columns_.add(std::move(name), data);
  }

  void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const;

  std::unique_ptr<ColumnValues> get_column_values(const SpreadsheetColumnID &column_id) const;
};

class GeometryDataSource : public DataSource {
 private:
  Object *object_eval_;
  const GeometrySet geometry_set_;
  const GeometryComponent *component_;
  eAttrDomain domain_;
  ExtraColumns extra_columns_;

  /* Some data is computed on the fly only when it is requested. Computing it does not change the
   * logical state of this data source. Therefore, the corresponding methods are const and need to
   * be protected with a mutex. */
  mutable std::mutex mutex_;
  mutable ResourceScope scope_;

 public:
  GeometryDataSource(Object *object_eval,
                     GeometrySet geometry_set,
                     const GeometryComponentType component_type,
                     const eAttrDomain domain,
                     ExtraColumns extra_columns = {})
      : object_eval_(object_eval),
        geometry_set_(std::move(geometry_set)),
        component_(geometry_set_.get_component_for_read(component_type)),
        domain_(domain),
        extra_columns_(std::move(extra_columns))
  {
  }

  Object *object_eval() const
  {
    return object_eval_;
  }

  bool has_selection_filter() const override;
  IndexMask apply_selection_filter(IndexMaskMemory &memory) const;

  void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const override;

  std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID &column_id) const override;

  int tot_rows() const override;
};

class VolumeDataSource : public DataSource {
  const GeometrySet geometry_set_;
  const VolumeComponent *component_;

 public:
  VolumeDataSource(GeometrySet geometry_set)
      : geometry_set_(std::move(geometry_set)),
        component_(geometry_set_.get_component_for_read<VolumeComponent>())
  {
  }

  void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const override;

  std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID &column_id) const override;

  int tot_rows() const override;
};

std::unique_ptr<DataSource> data_source_from_geometry(const bContext *C, Object *object_eval);

}  // namespace blender::ed::spreadsheet
