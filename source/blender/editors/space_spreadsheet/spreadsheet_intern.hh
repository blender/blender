/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_geometry_set.hh"
#include "spreadsheet_cache.hh"

struct ARegionType;
struct bContext;

namespace blender::ed::spreadsheet {

struct SpaceSpreadsheet_Runtime {
 public:
  int visible_rows = 0;
  int tot_rows = 0;
  int tot_columns = 0;

  SpreadsheetCache cache;

  SpaceSpreadsheet_Runtime() = default;

  /* The cache is not copied currently. */
  SpaceSpreadsheet_Runtime(const SpaceSpreadsheet_Runtime &other)
      : visible_rows(other.visible_rows), tot_rows(other.tot_rows), tot_columns(other.tot_columns)
  {
  }
};

void spreadsheet_operatortypes();
Object *spreadsheet_get_object_eval(const SpaceSpreadsheet *sspreadsheet,
                                    const Depsgraph *depsgraph);

bke::GeometrySet spreadsheet_get_display_geometry_set(const SpaceSpreadsheet *sspreadsheet,
                                                      Object *object_eval);

void spreadsheet_data_set_region_panels_register(ARegionType &region_type);

}  // namespace blender::ed::spreadsheet
