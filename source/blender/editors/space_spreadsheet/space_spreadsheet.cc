/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstring>

#include "BLI_listbase.h"

#include "BKE_global.h"
#include "BKE_lib_remap.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_spreadsheet.h"
#include "ED_viewer_path.hh"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "BLO_read_write.h"

#include "DEG_depsgraph_query.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLT_translation.h"

#include "BLF_api.h"

#include "spreadsheet_data_source_geometry.hh"
#include "spreadsheet_dataset_draw.hh"
#include "spreadsheet_intern.hh"
#include "spreadsheet_layout.hh"
#include "spreadsheet_row_filter.hh"
#include "spreadsheet_row_filter_ui.hh"

using namespace blender;
using namespace blender::ed::spreadsheet;

static SpaceLink *spreadsheet_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  SpaceSpreadsheet *spreadsheet_space = MEM_cnew<SpaceSpreadsheet>("spreadsheet space");
  spreadsheet_space->spacetype = SPACE_SPREADSHEET;

  spreadsheet_space->filter_flag = SPREADSHEET_FILTER_ENABLE;

  {
    /* Header. */
    ARegion *region = MEM_cnew<ARegion>("spreadsheet header");
    BLI_addtail(&spreadsheet_space->regionbase, region);
    region->regiontype = RGN_TYPE_HEADER;
    region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;
  }

  {
    /* Footer. */
    ARegion *region = MEM_cnew<ARegion>("spreadsheet footer region");
    BLI_addtail(&spreadsheet_space->regionbase, region);
    region->regiontype = RGN_TYPE_FOOTER;
    region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;
  }

  {
    /* Dataset Region */
    ARegion *region = MEM_cnew<ARegion>("spreadsheet dataset region");
    BLI_addtail(&spreadsheet_space->regionbase, region);
    region->regiontype = RGN_TYPE_TOOLS;
    region->alignment = RGN_ALIGN_LEFT;
  }

  {
    /* Properties region. */
    ARegion *region = MEM_cnew<ARegion>("spreadsheet right region");
    BLI_addtail(&spreadsheet_space->regionbase, region);
    region->regiontype = RGN_TYPE_UI;
    region->alignment = RGN_ALIGN_RIGHT;
    region->flag = RGN_FLAG_HIDDEN;
  }

  {
    /* Main window. */
    ARegion *region = MEM_cnew<ARegion>("spreadsheet main region");
    BLI_addtail(&spreadsheet_space->regionbase, region);
    region->regiontype = RGN_TYPE_WINDOW;
  }

  return (SpaceLink *)spreadsheet_space;
}

static void spreadsheet_free(SpaceLink *sl)
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)sl;

  MEM_delete(sspreadsheet->runtime);

  LISTBASE_FOREACH_MUTABLE (SpreadsheetRowFilter *, row_filter, &sspreadsheet->row_filters) {
    spreadsheet_row_filter_free(row_filter);
  }
  LISTBASE_FOREACH_MUTABLE (SpreadsheetColumn *, column, &sspreadsheet->columns) {
    spreadsheet_column_free(column);
  }
  BKE_viewer_path_clear(&sspreadsheet->viewer_path);
}

static void spreadsheet_init(wmWindowManager * /*wm*/, ScrArea *area)
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)area->spacedata.first;
  if (sspreadsheet->runtime == nullptr) {
    sspreadsheet->runtime = MEM_new<SpaceSpreadsheet_Runtime>(__func__);
  }
}

static SpaceLink *spreadsheet_duplicate(SpaceLink *sl)
{
  const SpaceSpreadsheet *sspreadsheet_old = (SpaceSpreadsheet *)sl;
  SpaceSpreadsheet *sspreadsheet_new = (SpaceSpreadsheet *)MEM_dupallocN(sspreadsheet_old);
  if (sspreadsheet_old->runtime) {
    sspreadsheet_new->runtime = MEM_new<SpaceSpreadsheet_Runtime>(__func__,
                                                                  *sspreadsheet_old->runtime);
  }
  else {
    sspreadsheet_new->runtime = MEM_new<SpaceSpreadsheet_Runtime>(__func__);
  }

  BLI_listbase_clear(&sspreadsheet_new->row_filters);
  LISTBASE_FOREACH (const SpreadsheetRowFilter *, src_filter, &sspreadsheet_old->row_filters) {
    SpreadsheetRowFilter *new_filter = spreadsheet_row_filter_copy(src_filter);
    BLI_addtail(&sspreadsheet_new->row_filters, new_filter);
  }
  BLI_listbase_clear(&sspreadsheet_new->columns);
  LISTBASE_FOREACH (SpreadsheetColumn *, src_column, &sspreadsheet_old->columns) {
    SpreadsheetColumn *new_column = spreadsheet_column_copy(src_column);
    BLI_addtail(&sspreadsheet_new->columns, new_column);
  }

  BKE_viewer_path_copy(&sspreadsheet_new->viewer_path, &sspreadsheet_old->viewer_path);

  return (SpaceLink *)sspreadsheet_new;
}

static void spreadsheet_keymap(wmKeyConfig *keyconf)
{
  /* Entire editor only. */
  WM_keymap_ensure(keyconf, "Spreadsheet Generic", SPACE_SPREADSHEET, 0);
}

static void spreadsheet_id_remap(ScrArea * /*area*/, SpaceLink *slink, const IDRemapper *mappings)
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)slink;
  BKE_viewer_path_id_remap(&sspreadsheet->viewer_path, mappings);
}

static void spreadsheet_main_region_init(wmWindowManager *wm, ARegion *region)
{
  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM;
  region->v2d.align = V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y;
  region->v2d.keepzoom = V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT;
  region->v2d.keeptot = V2D_KEEPTOT_STRICT;
  region->v2d.minzoom = region->v2d.maxzoom = 1.0f;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_LIST, region->winx, region->winy);

  {
    wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "View2D Buttons List", 0, 0);
    WM_event_add_keymap_handler(&region->handlers, keymap);
  }
  {
    wmKeyMap *keymap = WM_keymap_ensure(
        wm->defaultconf, "Spreadsheet Generic", SPACE_SPREADSHEET, 0);
    WM_event_add_keymap_handler(&region->handlers, keymap);
  }
}

ID *ED_spreadsheet_get_current_id(const SpaceSpreadsheet *sspreadsheet)
{
  if (BLI_listbase_is_empty(&sspreadsheet->viewer_path.path)) {
    return nullptr;
  }
  ViewerPathElem *root_context = static_cast<ViewerPathElem *>(
      sspreadsheet->viewer_path.path.first);
  if (root_context->type != VIEWER_PATH_ELEM_TYPE_ID) {
    return nullptr;
  }
  IDViewerPathElem *id_elem = reinterpret_cast<IDViewerPathElem *>(root_context);
  return id_elem->id;
}

static void view_active_object(const bContext *C, SpaceSpreadsheet *sspreadsheet)
{
  BKE_viewer_path_clear(&sspreadsheet->viewer_path);
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  IDViewerPathElem *id_elem = BKE_viewer_path_elem_new_id();
  id_elem->id = &ob->id;
  BLI_addtail(&sspreadsheet->viewer_path.path, id_elem);
  ED_area_tag_redraw(CTX_wm_area(C));
}

static void spreadsheet_update_context(const bContext *C)
{
  using blender::ed::viewer_path::ViewerPathForGeometryNodesViewer;

  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  Object *active_object = CTX_data_active_object(C);
  Object *context_object = blender::ed::viewer_path::parse_object_only(sspreadsheet->viewer_path);
  switch (eSpaceSpreadsheet_ObjectEvalState(sspreadsheet->object_eval_state)) {
    case SPREADSHEET_OBJECT_EVAL_STATE_ORIGINAL:
    case SPREADSHEET_OBJECT_EVAL_STATE_EVALUATED: {
      if (sspreadsheet->flag & SPREADSHEET_FLAG_PINNED) {
        if (context_object == nullptr) {
          /* Object is not available anymore, so clear the pinning. */
          sspreadsheet->flag &= ~SPREADSHEET_FLAG_PINNED;
        }
        else {
          /* The object is still pinned, do nothing. */
          break;
        }
      }
      else {
        if (active_object != context_object) {
          /* The active object has changed, so view the new active object. */
          view_active_object(C, sspreadsheet);
        }
        else {
          /* Nothing changed. */
          break;
        }
      }
      break;
    }
    case SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE: {
      WorkSpace *workspace = CTX_wm_workspace(C);
      if (sspreadsheet->flag & SPREADSHEET_FLAG_PINNED) {
        const std::optional<ViewerPathForGeometryNodesViewer> parsed_path =
            blender::ed::viewer_path::parse_geometry_nodes_viewer(sspreadsheet->viewer_path);
        if (parsed_path.has_value()) {
          if (blender::ed::viewer_path::exists_geometry_nodes_viewer(*parsed_path)) {
            /* The pinned path is still valid, do nothing. */
            break;
          }
          /* The pinned path does not exist anymore, clear pinning. */
          sspreadsheet->flag &= ~SPREADSHEET_FLAG_PINNED;
        }
        else {
          /* Unknown pinned path, clear pinning. */
          sspreadsheet->flag &= ~SPREADSHEET_FLAG_PINNED;
        }
      }
      /* Now try to update the viewer path from the workspace. */
      const std::optional<ViewerPathForGeometryNodesViewer> workspace_parsed_path =
          blender::ed::viewer_path::parse_geometry_nodes_viewer(workspace->viewer_path);
      if (workspace_parsed_path.has_value()) {
        if (BKE_viewer_path_equal(&sspreadsheet->viewer_path, &workspace->viewer_path)) {
          /* Nothing changed. */
          break;
        }
        /* Update the viewer path from the workspace. */
        BKE_viewer_path_clear(&sspreadsheet->viewer_path);
        BKE_viewer_path_copy(&sspreadsheet->viewer_path, &workspace->viewer_path);
      }
      else {
        /* No active viewer node, change back to showing evaluated active object. */
        sspreadsheet->object_eval_state = SPREADSHEET_OBJECT_EVAL_STATE_EVALUATED;
        view_active_object(C, sspreadsheet);
      }

      break;
    }
  }
}

Object *spreadsheet_get_object_eval(const SpaceSpreadsheet *sspreadsheet,
                                    const Depsgraph *depsgraph)
{
  ID *used_id = ED_spreadsheet_get_current_id(sspreadsheet);
  if (used_id == nullptr) {
    return nullptr;
  }
  const ID_Type id_type = GS(used_id->name);
  if (id_type != ID_OB) {
    return nullptr;
  }
  Object *object_orig = (Object *)used_id;
  if (!ELEM(object_orig->type,
            OB_MESH,
            OB_POINTCLOUD,
            OB_VOLUME,
            OB_CURVES_LEGACY,
            OB_FONT,
            OB_CURVES))
  {
    return nullptr;
  }

  Object *object_eval = DEG_get_evaluated_object(depsgraph, object_orig);
  if (object_eval == nullptr) {
    return nullptr;
  }

  return object_eval;
}

static std::unique_ptr<DataSource> get_data_source(const bContext *C)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);

  Object *object_eval = spreadsheet_get_object_eval(sspreadsheet, depsgraph);
  if (object_eval) {
    return data_source_from_geometry(C, object_eval);
  }
  return {};
}

static float get_default_column_width(const ColumnValues &values)
{
  if (values.default_width > 0.0f) {
    return values.default_width;
  }
  static const float float_width = 3;
  switch (values.type()) {
    case SPREADSHEET_VALUE_TYPE_BOOL:
      return 2.0f;
    case SPREADSHEET_VALUE_TYPE_INT8:
    case SPREADSHEET_VALUE_TYPE_INT32:
      return float_width;
    case SPREADSHEET_VALUE_TYPE_FLOAT:
      return float_width;
    case SPREADSHEET_VALUE_TYPE_INT32_2D:
    case SPREADSHEET_VALUE_TYPE_FLOAT2:
      return 2.0f * float_width;
    case SPREADSHEET_VALUE_TYPE_FLOAT3:
      return 3.0f * float_width;
    case SPREADSHEET_VALUE_TYPE_COLOR:
    case SPREADSHEET_VALUE_TYPE_BYTE_COLOR:
    case SPREADSHEET_VALUE_TYPE_QUATERNION:
      return 4.0f * float_width;
    case SPREADSHEET_VALUE_TYPE_INSTANCES:
      return 8.0f;
    case SPREADSHEET_VALUE_TYPE_STRING:
      return 5.0f;
    case SPREADSHEET_VALUE_TYPE_UNKNOWN:
      return 2.0f;
  }
  return float_width;
}

static float get_column_width(const ColumnValues &values)
{
  float data_width = get_default_column_width(values);
  const int fontid = UI_style_get()->widget.uifont_id;
  BLF_size(fontid, UI_DEFAULT_TEXT_POINTS * UI_SCALE_FAC);
  const StringRefNull name = values.name();
  const float name_width = BLF_width(fontid, name.data(), name.size());
  return std::max<float>(name_width / UI_UNIT_X + 1.0f, data_width);
}

static float get_column_width_in_pixels(const ColumnValues &values)
{
  return get_column_width(values) * SPREADSHEET_WIDTH_UNIT;
}

static int get_index_column_width(const int tot_rows)
{
  const int fontid = UI_style_get()->widget.uifont_id;
  BLF_size(fontid, UI_style_get_dpi()->widget.points * UI_SCALE_FAC);
  return std::to_string(std::max(0, tot_rows - 1)).size() * BLF_width(fontid, "0", 1) +
         UI_UNIT_X * 0.75;
}

static void update_visible_columns(ListBase &columns, DataSource &data_source)
{
  Set<SpreadsheetColumnID> used_ids;
  LISTBASE_FOREACH_MUTABLE (SpreadsheetColumn *, column, &columns) {
    std::unique_ptr<ColumnValues> values = data_source.get_column_values(*column->id);
    /* Remove columns that don't exist anymore. */
    if (!values) {
      BLI_remlink(&columns, column);
      spreadsheet_column_free(column);
      continue;
    }

    if (!used_ids.add(*column->id)) {
      /* Remove duplicate columns for now. */
      BLI_remlink(&columns, column);
      spreadsheet_column_free(column);
      continue;
    }
  }

  data_source.foreach_default_column_ids(
      [&](const SpreadsheetColumnID &column_id, const bool is_extra) {
        std::unique_ptr<ColumnValues> values = data_source.get_column_values(column_id);
        if (values) {
          if (used_ids.add(column_id)) {
            SpreadsheetColumnID *new_id = spreadsheet_column_id_copy(&column_id);
            SpreadsheetColumn *new_column = spreadsheet_column_new(new_id);
            if (is_extra) {
              BLI_addhead(&columns, new_column);
            }
            else {
              BLI_addtail(&columns, new_column);
            }
          }
        }
      });
}

static void spreadsheet_main_region_draw(const bContext *C, ARegion *region)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  sspreadsheet->runtime->cache.set_all_unused();
  spreadsheet_update_context(C);

  std::unique_ptr<DataSource> data_source = get_data_source(C);
  if (!data_source) {
    data_source = std::make_unique<DataSource>();
  }

  update_visible_columns(sspreadsheet->columns, *data_source);

  SpreadsheetLayout spreadsheet_layout;
  ResourceScope scope;

  LISTBASE_FOREACH (SpreadsheetColumn *, column, &sspreadsheet->columns) {
    std::unique_ptr<ColumnValues> values_ptr = data_source->get_column_values(*column->id);
    /* Should have been removed before if it does not exist anymore. */
    BLI_assert(values_ptr);
    const ColumnValues *values = scope.add(std::move(values_ptr));
    const int width = get_column_width_in_pixels(*values);
    spreadsheet_layout.columns.append({values, width});

    spreadsheet_column_assign_runtime_data(column, values->type(), values->name());
  }

  const int tot_rows = data_source->tot_rows();
  spreadsheet_layout.index_column_width = get_index_column_width(tot_rows);
  spreadsheet_layout.row_indices = spreadsheet_filter_rows(
      *sspreadsheet, spreadsheet_layout, *data_source, scope);

  sspreadsheet->runtime->tot_columns = spreadsheet_layout.columns.size();
  sspreadsheet->runtime->tot_rows = tot_rows;
  sspreadsheet->runtime->visible_rows = spreadsheet_layout.row_indices.size();

  std::unique_ptr<SpreadsheetDrawer> drawer = spreadsheet_drawer_from_layout(spreadsheet_layout);
  draw_spreadsheet_in_region(C, region, *drawer);

  /* Tag other regions for redraw, because the main region updates data for them. */
  ARegion *footer = BKE_area_find_region_type(CTX_wm_area(C), RGN_TYPE_FOOTER);
  ED_region_tag_redraw(footer);
  ARegion *sidebar = BKE_area_find_region_type(CTX_wm_area(C), RGN_TYPE_UI);
  ED_region_tag_redraw(sidebar);

  /* Free all cache items that have not been used. */
  sspreadsheet->runtime->cache.remove_all_unused();
}

static void spreadsheet_main_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;
  SpaceSpreadsheet *sspreadsheet = static_cast<SpaceSpreadsheet *>(params->area->spacedata.first);

  switch (wmn->category) {
    case NC_SCENE: {
      switch (wmn->data) {
        case ND_MODE:
        case ND_FRAME:
        case ND_OB_ACTIVE: {
          ED_region_tag_redraw(region);
          break;
        }
      }
      break;
    }
    case NC_OBJECT: {
      ED_region_tag_redraw(region);
      break;
    }
    case NC_SPACE: {
      if (wmn->data == ND_SPACE_SPREADSHEET) {
        ED_region_tag_redraw(region);
      }
      break;
    }
    case NC_TEXTURE:
    case NC_GEOM: {
      ED_region_tag_redraw(region);
      break;
    }
    case NC_VIEWER_PATH: {
      if (sspreadsheet->object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE) {
        ED_region_tag_redraw(region);
      }
      break;
    }
  }
}

static void spreadsheet_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void spreadsheet_header_region_draw(const bContext *C, ARegion *region)
{
  spreadsheet_update_context(C);
  ED_region_header(C, region);
}

static void spreadsheet_header_region_free(ARegion * /*region*/) {}

static void spreadsheet_header_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;
  SpaceSpreadsheet *sspreadsheet = static_cast<SpaceSpreadsheet *>(params->area->spacedata.first);

  switch (wmn->category) {
    case NC_SCENE: {
      switch (wmn->data) {
        case ND_MODE:
        case ND_OB_ACTIVE: {
          ED_region_tag_redraw(region);
          break;
        }
      }
      break;
    }
    case NC_OBJECT: {
      ED_region_tag_redraw(region);
      break;
    }
    case NC_SPACE: {
      if (wmn->data == ND_SPACE_SPREADSHEET) {
        ED_region_tag_redraw(region);
      }
      break;
    }
    case NC_GEOM: {
      ED_region_tag_redraw(region);
      break;
    }
    case NC_VIEWER_PATH: {
      if (sspreadsheet->object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE) {
        ED_region_tag_redraw(region);
      }
      break;
    }
  }
}

static void spreadsheet_footer_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void spreadsheet_footer_region_draw(const bContext *C, ARegion *region)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  SpaceSpreadsheet_Runtime *runtime = sspreadsheet->runtime;
  std::stringstream ss;
  ss << IFACE_("Rows:") << " ";
  if (runtime->visible_rows != runtime->tot_rows) {
    char visible_rows_str[BLI_STR_FORMAT_INT32_GROUPED_SIZE];
    BLI_str_format_int_grouped(visible_rows_str, runtime->visible_rows);
    ss << visible_rows_str << " / ";
  }
  char tot_rows_str[BLI_STR_FORMAT_INT32_GROUPED_SIZE];
  BLI_str_format_int_grouped(tot_rows_str, runtime->tot_rows);
  ss << tot_rows_str << "   |   " << IFACE_("Columns:") << " " << runtime->tot_columns;
  std::string stats_str = ss.str();

  UI_ThemeClearColor(TH_BACK);

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  const uiStyle *style = UI_style_get_dpi();
  uiLayout *layout = UI_block_layout(block,
                                     UI_LAYOUT_HORIZONTAL,
                                     UI_LAYOUT_HEADER,
                                     UI_HEADER_OFFSET,
                                     region->winy - (region->winy - UI_UNIT_Y) / 2.0f,
                                     region->winx,
                                     1,
                                     0,
                                     style);
  uiItemSpacer(layout);
  uiLayoutSetAlignment(layout, UI_LAYOUT_ALIGN_RIGHT);
  uiItemL(layout, stats_str.c_str(), ICON_NONE);
  UI_block_layout_resolve(block, nullptr, nullptr);
  UI_block_align_end(block);
  UI_block_end(C, block);
  UI_block_draw(C, block);
}

static void spreadsheet_footer_region_free(ARegion * /*region*/) {}

static void spreadsheet_footer_region_listener(const wmRegionListenerParams * /*params*/) {}

static void spreadsheet_dataset_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  switch (wmn->category) {
    case NC_SCENE: {
      switch (wmn->data) {
        case ND_FRAME:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    }
    case NC_TEXTURE:
      ED_region_tag_redraw(region);
      break;
  }

  spreadsheet_header_region_listener(params);
}

static void spreadsheet_dataset_region_draw(const bContext *C, ARegion *region)
{
  spreadsheet_update_context(C);
  ED_region_panels(C, region);
}

static void spreadsheet_sidebar_init(wmWindowManager *wm, ARegion *region)
{
  UI_panel_category_active_set_default(region, "Filters");
  ED_region_panels_init(wm, region);

  wmKeyMap *keymap = WM_keymap_ensure(
      wm->defaultconf, "Spreadsheet Generic", SPACE_SPREADSHEET, 0);
  WM_event_add_keymap_handler(&region->handlers, keymap);
}

static void spreadsheet_right_region_free(ARegion * /*region*/) {}

static void spreadsheet_right_region_listener(const wmRegionListenerParams * /*params*/) {}

static void spreadsheet_blend_read_data(BlendDataReader *reader, SpaceLink *sl)
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)sl;

  sspreadsheet->runtime = nullptr;
  BLO_read_list(reader, &sspreadsheet->row_filters);
  LISTBASE_FOREACH (SpreadsheetRowFilter *, row_filter, &sspreadsheet->row_filters) {
    BLO_read_data_address(reader, &row_filter->value_string);
  }
  BLO_read_list(reader, &sspreadsheet->columns);
  LISTBASE_FOREACH (SpreadsheetColumn *, column, &sspreadsheet->columns) {
    BLO_read_data_address(reader, &column->id);
    BLO_read_data_address(reader, &column->id->name);
    /* While the display name is technically runtime data, it is loaded here, otherwise the row
     * filters might not now their type if their region draws before the main region.
     * This would ideally be cleared here. */
    BLO_read_data_address(reader, &column->display_name);
  }

  BKE_viewer_path_blend_read_data(reader, &sspreadsheet->viewer_path);
}

static void spreadsheet_blend_read_lib(BlendLibReader *reader, ID *parent_id, SpaceLink *sl)
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)sl;
  BKE_viewer_path_blend_read_lib(reader, parent_id, &sspreadsheet->viewer_path);
}

static void spreadsheet_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  BLO_write_struct(writer, SpaceSpreadsheet, sl);
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)sl;

  LISTBASE_FOREACH (SpreadsheetRowFilter *, row_filter, &sspreadsheet->row_filters) {
    BLO_write_struct(writer, SpreadsheetRowFilter, row_filter);
    BLO_write_string(writer, row_filter->value_string);
  }

  LISTBASE_FOREACH (SpreadsheetColumn *, column, &sspreadsheet->columns) {
    BLO_write_struct(writer, SpreadsheetColumn, column);
    BLO_write_struct(writer, SpreadsheetColumnID, column->id);
    BLO_write_string(writer, column->id->name);
    /* While the display name is technically runtime data, we write it here, otherwise the row
     * filters might not now their type if their region draws before the main region.
     * This would ideally be cleared here. */
    BLO_write_string(writer, column->display_name);
  }

  BKE_viewer_path_blend_write(writer, &sspreadsheet->viewer_path);
}

void ED_spacetype_spreadsheet()
{
  SpaceType *st = MEM_cnew<SpaceType>("spacetype spreadsheet");
  ARegionType *art;

  st->spaceid = SPACE_SPREADSHEET;
  STRNCPY(st->name, "Spreadsheet");

  st->create = spreadsheet_create;
  st->free = spreadsheet_free;
  st->init = spreadsheet_init;
  st->duplicate = spreadsheet_duplicate;
  st->operatortypes = spreadsheet_operatortypes;
  st->keymap = spreadsheet_keymap;
  st->id_remap = spreadsheet_id_remap;
  st->blend_read_data = spreadsheet_blend_read_data;
  st->blend_read_lib = spreadsheet_blend_read_lib;
  st->blend_write = spreadsheet_blend_write;

  /* regions: main window */
  art = MEM_cnew<ARegionType>("spacetype spreadsheet region");
  art->regionid = RGN_TYPE_WINDOW;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;
  art->lock = 1;

  art->init = spreadsheet_main_region_init;
  art->draw = spreadsheet_main_region_draw;
  art->listener = spreadsheet_main_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_cnew<ARegionType>("spacetype spreadsheet header region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = 0;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
  art->lock = 1;

  art->init = spreadsheet_header_region_init;
  art->draw = spreadsheet_header_region_draw;
  art->free = spreadsheet_header_region_free;
  art->listener = spreadsheet_header_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* regions: footer */
  art = MEM_cnew<ARegionType>("spacetype spreadsheet footer region");
  art->regionid = RGN_TYPE_FOOTER;
  art->prefsizey = HEADERY;
  art->keymapflag = 0;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
  art->lock = 1;

  art->init = spreadsheet_footer_region_init;
  art->draw = spreadsheet_footer_region_draw;
  art->free = spreadsheet_footer_region_free;
  art->listener = spreadsheet_footer_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* regions: right panel buttons */
  art = MEM_cnew<ARegionType>("spacetype spreadsheet right region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->lock = 1;

  art->init = spreadsheet_sidebar_init;
  art->layout = ED_region_panels_layout;
  art->draw = ED_region_panels_draw;
  art->free = spreadsheet_right_region_free;
  art->listener = spreadsheet_right_region_listener;
  BLI_addhead(&st->regiontypes, art);

  register_row_filter_panels(*art);

  /* regions: channels */
  art = MEM_cnew<ARegionType>("spreadsheet dataset region");
  art->regionid = RGN_TYPE_TOOLS;
  art->prefsizex = 150 + V2D_SCROLL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI;
  art->lock = 1;
  art->init = ED_region_panels_init;
  art->draw = spreadsheet_dataset_region_draw;
  art->listener = spreadsheet_dataset_region_listener;
  blender::ed::spreadsheet::spreadsheet_data_set_region_panels_register(*art);
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);
}
