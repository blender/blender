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

#include <array>

#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_volume.h"

#include "BLF_api.h"

#include "BLI_rect.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "WM_types.h"

#include "spreadsheet_dataset_draw.hh"
#include "spreadsheet_draw.hh"
#include "spreadsheet_intern.hh"

static int is_component_row_selected(struct uiBut *but, const void *arg)
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)arg;

  GeometryComponentType component = (GeometryComponentType)UI_but_datasetrow_component_get(but);
  AttributeDomain domain = (AttributeDomain)UI_but_datasetrow_domain_get(but);

  const bool is_component_selected = (GeometryComponentType)
                                         sspreadsheet->geometry_component_type == component;
  const bool is_domain_selected = (AttributeDomain)sspreadsheet->attribute_domain == domain;
  bool is_selected = is_component_selected && is_domain_selected;

  if (ELEM(component, GEO_COMPONENT_TYPE_VOLUME, GEO_COMPONENT_TYPE_INSTANCES)) {
    is_selected = is_component_selected;
  }

  return is_selected;
}

namespace blender::ed::spreadsheet {

/* -------------------------------------------------------------------- */
/* Draw Context */

class DatasetDrawContext {
  std::array<int, 2> mval_;

 public:
  const SpaceSpreadsheet *sspreadsheet;
  Object *object_eval;
  /* Current geometry set, changes per component. */
  GeometrySet current_geometry_set;

  DatasetDrawContext(const bContext *C);

  GeometrySet geometry_set_from_component(GeometryComponentType component);
  const std::array<int, 2> &cursor_mval() const;
};

DatasetDrawContext::DatasetDrawContext(const bContext *C)
    : sspreadsheet(CTX_wm_space_spreadsheet(C)),
      object_eval(spreadsheet_get_object_eval(sspreadsheet, CTX_data_depsgraph_pointer(C)))
{
  const wmWindow *win = CTX_wm_window(C);
  const ARegion *region = CTX_wm_region(C);
  mval_ = {win->eventstate->xy[0] - region->winrct.xmin,
           win->eventstate->xy[1] - region->winrct.ymin};
}

GeometrySet DatasetDrawContext::geometry_set_from_component(GeometryComponentType component)
{
  return spreadsheet_get_display_geometry_set(sspreadsheet, object_eval, component);
}

const std::array<int, 2> &DatasetDrawContext::cursor_mval() const
{
  return mval_;
}

/* -------------------------------------------------------------------- */
/* Drawer */

DatasetRegionDrawer::DatasetRegionDrawer(const ARegion *region,
                                         uiBlock &block,
                                         DatasetDrawContext &draw_context)
    : row_height(UI_UNIT_Y),
      xmin(region->v2d.cur.xmin),
      xmax(region->v2d.cur.xmax),
      block(block),
      v2d(region->v2d),
      draw_context(draw_context)
{
}

void DatasetRegionDrawer::draw_hierarchy(const DatasetLayoutHierarchy &layout)
{
  for (const DatasetComponentLayoutInfo &component : layout.components) {
    draw_context.current_geometry_set = draw_context.geometry_set_from_component(component.type);

    draw_component_row(component);

    /* Iterate attribute domains, skip unset ones (storage has to be in a enum-based, fixed size
     * array so uses optionals to support skipping enum values that shouldn't be displayed for a
     * component). */
    for (const auto &optional_domain : component.attr_domains) {
      if (!optional_domain) {
        continue;
      }

      const DatasetAttrDomainLayoutInfo &domain_info = *optional_domain;
      draw_attribute_domain_row(component, domain_info);
    }
  }
}

static int element_count_from_instances(const GeometrySet &geometry_set)
{
  if (geometry_set.has_instances()) {
    const InstancesComponent *instances_component =
        geometry_set.get_component_for_read<InstancesComponent>();
    return instances_component->instances_amount();
  }
  return 0;
}

static int element_count_from_volume(const GeometrySet &geometry_set)
{
  if (const Volume *volume = geometry_set.get_volume_for_read()) {
    return BKE_volume_num_grids(volume);
  }
  return 0;
}

static int element_count_from_component_domain(const GeometrySet &geometry_set,
                                               GeometryComponentType component,
                                               AttributeDomain domain)
{
  if (geometry_set.has_mesh() && component == GEO_COMPONENT_TYPE_MESH) {
    const MeshComponent *mesh_component = geometry_set.get_component_for_read<MeshComponent>();
    return mesh_component->attribute_domain_size(domain);
  }

  if (geometry_set.has_pointcloud() && component == GEO_COMPONENT_TYPE_POINT_CLOUD) {
    const PointCloudComponent *point_cloud_component =
        geometry_set.get_component_for_read<PointCloudComponent>();
    return point_cloud_component->attribute_domain_size(domain);
  }

  if (geometry_set.has_volume() && component == GEO_COMPONENT_TYPE_VOLUME) {
    const VolumeComponent *volume_component =
        geometry_set.get_component_for_read<VolumeComponent>();
    return volume_component->attribute_domain_size(domain);
  }

  if (geometry_set.has_curve() && component == GEO_COMPONENT_TYPE_CURVE) {
    const CurveComponent *curve_component = geometry_set.get_component_for_read<CurveComponent>();
    return curve_component->attribute_domain_size(domain);
  }

  return 0;
}

void DatasetRegionDrawer::draw_dataset_row(const int indentation,
                                           const GeometryComponentType component,
                                           const std::optional<AttributeDomain> domain,
                                           BIFIconID icon,
                                           const char *label,
                                           const bool is_active)
{

  const float row_height = UI_UNIT_Y;
  const float padding_x = UI_UNIT_X * 0.25f;

  const rctf rect = {float(xmin) + padding_x,
                     float(xmax) - V2D_SCROLL_HANDLE_WIDTH,
                     ymin_offset - row_height,
                     ymin_offset};

  char element_count[7];
  if (component == GEO_COMPONENT_TYPE_INSTANCES) {
    BLI_str_format_attribute_domain_size(
        element_count, element_count_from_instances(draw_context.current_geometry_set));
  }
  if (component == GEO_COMPONENT_TYPE_VOLUME) {
    BLI_str_format_attribute_domain_size(
        element_count, element_count_from_volume(draw_context.current_geometry_set));
  }
  else {
    BLI_str_format_attribute_domain_size(
        element_count,
        domain ? element_count_from_component_domain(
                     draw_context.current_geometry_set, component, *domain) :
                 0);
  }

  std::string label_and_element_count = label;
  label_and_element_count += UI_SEP_CHAR;
  label_and_element_count += element_count;

  uiBut *bt = uiDefIconTextButO(&block,
                                UI_BTYPE_DATASETROW,
                                "SPREADSHEET_OT_change_spreadsheet_data_source",
                                0,
                                icon,
                                label,
                                rect.xmin,
                                rect.ymin,
                                BLI_rctf_size_x(&rect),
                                BLI_rctf_size_y(&rect),
                                nullptr);

  UI_but_datasetrow_indentation_set(bt, indentation);

  if (is_active) {
    UI_but_hint_drawstr_set(bt, element_count);
    UI_but_datasetrow_component_set(bt, component);
    if (domain) {
      UI_but_datasetrow_domain_set(bt, *domain);
    }
    UI_but_func_pushed_state_set(bt, &is_component_row_selected, draw_context.sspreadsheet);

    PointerRNA *but_ptr = UI_but_operator_ptr_get((uiBut *)bt);
    RNA_int_set(but_ptr, "component_type", component);
    if (domain) {
      RNA_int_set(but_ptr, "attribute_domain_type", *domain);
    }
  }

  ymin_offset -= row_height;
}

void DatasetRegionDrawer::draw_component_row(const DatasetComponentLayoutInfo &component_info)
{
  if (ELEM(component_info.type, GEO_COMPONENT_TYPE_VOLUME, GEO_COMPONENT_TYPE_INSTANCES)) {
    draw_dataset_row(
        0, component_info.type, std::nullopt, component_info.icon, component_info.label, true);
  }
  else {
    draw_dataset_row(
        0, component_info.type, std::nullopt, component_info.icon, component_info.label, false);
  }
}

void DatasetRegionDrawer::draw_attribute_domain_row(
    const DatasetComponentLayoutInfo &component_info,
    const DatasetAttrDomainLayoutInfo &domain_info)
{
  draw_dataset_row(
      1, component_info.type, domain_info.type, domain_info.icon, domain_info.label, true);
}

/* -------------------------------------------------------------------- */
/* Drawer */

void draw_dataset_in_region(const bContext *C, ARegion *region)
{
  DatasetDrawContext draw_context{C};
  if (!draw_context.object_eval) {
    /* No object means nothing to display. Keep the region empty. */
    return;
  }

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);

  DatasetRegionDrawer drawer{region, *block, draw_context};

  /* Start with an offset to align buttons to spreadsheet rows. Use spreadsheet drawing info for
   * that. */
  drawer.ymin_offset = -SpreadsheetDrawer().top_row_height + drawer.row_height;

  const DatasetLayoutHierarchy hierarchy = dataset_layout_hierarchy();
  drawer.draw_hierarchy(hierarchy);
#ifndef NDEBUG
  dataset_layout_hierarchy_sanity_check(hierarchy);
#endif

  UI_block_end(C, block);
  UI_view2d_totRect_set(&region->v2d, region->winx, abs(drawer.ymin_offset));
  UI_block_draw(C, block);
}

}  // namespace blender::ed::spreadsheet
