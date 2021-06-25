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

#include <array>

#include "BKE_geometry_set.hh"
#include "UI_interface.h"
#include "spreadsheet_dataset_layout.hh"

struct ARegion;
struct uiBlock;
struct View2D;
struct bContext;

namespace blender::ed::spreadsheet {

class DatasetDrawContext;

class DatasetRegionDrawer {
 public:
  const int row_height;
  float ymin_offset = 0;

  int xmin;
  int xmax;
  uiBlock &block;
  const View2D &v2d;
  DatasetDrawContext &draw_context;

  DatasetRegionDrawer(const ARegion *region, uiBlock &block, DatasetDrawContext &draw_context);

  void draw_hierarchy(const DatasetLayoutHierarchy &layout);

  void draw_attribute_domain_row(const DatasetComponentLayoutInfo &component,
                                 const DatasetAttrDomainLayoutInfo &domain_info);
  void draw_component_row(const DatasetComponentLayoutInfo &component_info);

 private:
  void draw_dataset_row(const int indentation,
                        const GeometryComponentType component,
                        const std::optional<AttributeDomain> domain,
                        const BIFIconID icon,
                        const char *label,
                        const bool is_active);
};

void draw_dataset_in_region(const bContext *C, ARegion *region);

}  // namespace blender::ed::spreadsheet
