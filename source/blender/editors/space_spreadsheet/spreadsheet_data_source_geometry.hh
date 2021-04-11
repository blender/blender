/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <mutex>

#include "BLI_resource_scope.hh"

#include "BKE_geometry_set.hh"

#include "spreadsheet_data_source.hh"

struct bContext;

namespace blender::ed::spreadsheet {

class GeometryDataSource : public DataSource {
 private:
  Object *object_eval_;
  const GeometrySet geometry_set_;
  const GeometryComponent *component_;
  AttributeDomain domain_;

  /* Some data is computed on the fly only when it is requested. Computing it does not change the
   * logical state of this data source. Therefore, the corresponding methods are const and need to
   * be protected with a mutex. */
  mutable std::mutex mutex_;
  mutable ResourceScope scope_;

 public:
  GeometryDataSource(Object *object_eval,
                     GeometrySet geometry_set,
                     const GeometryComponentType component_type,
                     const AttributeDomain domain)
      : object_eval_(object_eval),
        geometry_set_(std::move(geometry_set)),
        component_(geometry_set_.get_component_for_read(component_type)),
        domain_(domain)
  {
  }

  Object *object_eval() const
  {
    return object_eval_;
  }

  Span<int64_t> get_selected_element_indices() const;

  void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &)> fn) const override;

  std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID &column_id) const override;

  int tot_rows() const override;
};

class InstancesDataSource : public DataSource {
  const GeometrySet geometry_set_;
  const InstancesComponent *component_;

 public:
  InstancesDataSource(GeometrySet geometry_set)
      : geometry_set_(std::move(geometry_set)),
        component_(geometry_set_.get_component_for_read<InstancesComponent>())
  {
  }

  void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &)> fn) const override;

  std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID &column_id) const override;

  int tot_rows() const override;
};

std::unique_ptr<DataSource> data_source_from_geometry(const bContext *C, Object *object_eval);

}  // namespace blender::ed::spreadsheet
