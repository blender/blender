/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_geometry_set.hh"

#include "BKE_node_socket_value.hh"
#include "DNA_space_types.h"

struct ARegionType;
struct Depsgraph;
struct Object;
struct SpaceSpreadsheet;
struct ARegion;
struct SpreadsheetColumn;
struct bContext;
namespace blender::nodes {
class Bundle;
}
namespace blender::nodes::geo_eval_log {
class ViewerNodeLog;
}

#define SPREADSHEET_EDGE_ACTION_ZONE (UI_UNIT_X * 0.3f)

namespace blender::ed::spreadsheet {

class DataSource;

struct ReorderColumnVisualizationData {
  int old_index = 0;
  int new_index = 0;
  int current_offset_x_px = 0;
};

struct SpaceSpreadsheet_Runtime {
 public:
  int visible_rows = 0;
  int tot_rows = 0;
  int tot_columns = 0;
  int top_row_height = 0;
  int left_column_width = 0;

  std::optional<ReorderColumnVisualizationData> reorder_column_visualization_data;

  SpaceSpreadsheet_Runtime() = default;

  SpaceSpreadsheet_Runtime(const SpaceSpreadsheet_Runtime &other)
      : visible_rows(other.visible_rows), tot_rows(other.tot_rows), tot_columns(other.tot_columns)
  {
  }
};

void spreadsheet_operatortypes();
Object *spreadsheet_get_object_eval(const SpaceSpreadsheet *sspreadsheet,
                                    const Depsgraph *depsgraph);

const nodes::geo_eval_log::ViewerNodeLog *viewer_node_log_lookup(
    const SpaceSpreadsheet &sspreadsheet);

bke::SocketValueVariant geometry_display_data_get(const SpaceSpreadsheet *sspreadsheet,
                                                  Object *object_eval);
std::optional<bke::GeometrySet> root_geometry_set_get(const SpaceSpreadsheet *sspreadsheet,
                                                      Object *object_eval);

void spreadsheet_data_set_region_panels_register(ARegionType &region_type);

/** Find the column edge that the cursor is hovering in the header row. */
SpreadsheetColumn *find_hovered_column_header_edge(SpaceSpreadsheet &sspreadsheet,
                                                   ARegion &region,
                                                   const int2 &cursor_re);

/** Find the column that the cursor is hovering in the header row. */
SpreadsheetColumn *find_hovered_column_header(SpaceSpreadsheet &sspreadsheet,
                                              ARegion &region,
                                              const int2 &cursor_re);

/** Find the column edge that the cursor is hovering. */
SpreadsheetColumn *find_hovered_column_edge(SpaceSpreadsheet &sspreadsheet,
                                            ARegion &region,
                                            const int2 &cursor_re);

/** Find the column that the cursor is hovering. */
SpreadsheetColumn *find_hovered_column(SpaceSpreadsheet &sspreadsheet,
                                       ARegion &region,
                                       const int2 &cursor_re);

/**
 * Get the data that is currently displayed in the spreadsheet.
 */
std::unique_ptr<DataSource> get_data_source(const bContext &C);

/**
 * Get the ID of the table that should be displayed. This is used to look up the table from
 * #SpaceSpreadsheet::tables.
 */
const SpreadsheetTableID *get_active_table_id(const SpaceSpreadsheet &sspreadsheet);

}  // namespace blender::ed::spreadsheet
