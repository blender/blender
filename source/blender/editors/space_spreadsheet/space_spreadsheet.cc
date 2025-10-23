/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstring>
#include <fmt/format.h>

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_screen.hh"
#include "BKE_viewer_path.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_spreadsheet.hh"
#include "ED_viewer_path.hh"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "BLO_read_write.hh"

#include "DEG_depsgraph_query.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "BLT_translation.hh"

#include "BLF_api.hh"

#include "spreadsheet_column.hh"
#include "spreadsheet_data_source_geometry.hh"
#include "spreadsheet_intern.hh"
#include "spreadsheet_layout.hh"
#include "spreadsheet_row_filter.hh"
#include "spreadsheet_row_filter_ui.hh"
#include "spreadsheet_table.hh"

#include <sstream>

namespace blender::ed::spreadsheet {

static SpaceLink *spreadsheet_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  SpaceSpreadsheet *spreadsheet_space = MEM_callocN<SpaceSpreadsheet>("spreadsheet space");
  spreadsheet_space->runtime = MEM_new<SpaceSpreadsheet_Runtime>(__func__);
  spreadsheet_space->spacetype = SPACE_SPREADSHEET;

  spreadsheet_space->geometry_id.base.type = SPREADSHEET_TABLE_ID_TYPE_GEOMETRY;
  spreadsheet_space->filter_flag = SPREADSHEET_FILTER_ENABLE;

  {
    /* Header. */
    ARegion *region = BKE_area_region_new();
    BLI_addtail(&spreadsheet_space->regionbase, region);
    region->regiontype = RGN_TYPE_HEADER;
    region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;
  }

  {
    /* Footer. */
    ARegion *region = BKE_area_region_new();
    BLI_addtail(&spreadsheet_space->regionbase, region);
    region->regiontype = RGN_TYPE_FOOTER;
    region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;
  }

  {
    /* Dataset Region */
    ARegion *region = BKE_area_region_new();
    BLI_addtail(&spreadsheet_space->regionbase, region);
    region->regiontype = RGN_TYPE_TOOLS;
    region->alignment = RGN_ALIGN_LEFT;
  }

  {
    /* Properties region. */
    ARegion *region = BKE_area_region_new();
    BLI_addtail(&spreadsheet_space->regionbase, region);
    region->regiontype = RGN_TYPE_UI;
    region->alignment = RGN_ALIGN_RIGHT;
    region->flag = RGN_FLAG_HIDDEN;
  }

  {
    /* Main window. */
    ARegion *region = BKE_area_region_new();
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
  for (const int i : IndexRange(sspreadsheet->num_tables)) {
    spreadsheet_table_free(sspreadsheet->tables[i]);
  }
  MEM_SAFE_FREE(sspreadsheet->tables);
  spreadsheet_table_id_free_content(&sspreadsheet->geometry_id.base);
}

static void spreadsheet_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *spreadsheet_duplicate(SpaceLink *sl)
{
  const SpaceSpreadsheet *sspreadsheet_old = (SpaceSpreadsheet *)sl;
  SpaceSpreadsheet *sspreadsheet_new = (SpaceSpreadsheet *)MEM_dupallocN(sspreadsheet_old);
  sspreadsheet_new->runtime = MEM_new<SpaceSpreadsheet_Runtime>(__func__,
                                                                *sspreadsheet_old->runtime);

  BLI_listbase_clear(&sspreadsheet_new->row_filters);
  LISTBASE_FOREACH (const SpreadsheetRowFilter *, src_filter, &sspreadsheet_old->row_filters) {
    SpreadsheetRowFilter *new_filter = spreadsheet_row_filter_copy(src_filter);
    BLI_addtail(&sspreadsheet_new->row_filters, new_filter);
  }
  sspreadsheet_new->num_tables = sspreadsheet_old->num_tables;
  sspreadsheet_new->tables = MEM_calloc_arrayN<SpreadsheetTable *>(sspreadsheet_old->num_tables,
                                                                   __func__);
  for (const int i : IndexRange(sspreadsheet_old->num_tables)) {
    sspreadsheet_new->tables[i] = spreadsheet_table_copy(*sspreadsheet_old->tables[i]);
  }

  spreadsheet_table_id_copy_content_geometry(sspreadsheet_new->geometry_id,
                                             sspreadsheet_old->geometry_id);
  return (SpaceLink *)sspreadsheet_new;
}

static void spreadsheet_keymap(wmKeyConfig *keyconf)
{
  /* Entire editor only. */
  WM_keymap_ensure(keyconf, "Spreadsheet Generic", SPACE_SPREADSHEET, RGN_TYPE_WINDOW);
}

static void spreadsheet_id_remap(ScrArea * /*area*/,
                                 SpaceLink *slink,
                                 const blender::bke::id::IDRemapper &mappings)
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)slink;
  spreadsheet_table_id_remap_id(sspreadsheet->geometry_id.base, mappings);
  for (const int i : IndexRange(sspreadsheet->num_tables)) {
    spreadsheet_table_remap_id(*sspreadsheet->tables[i], mappings);
  }
}

static void spreadsheet_foreach_id(SpaceLink *space_link, LibraryForeachIDData *data)
{
  SpaceSpreadsheet *sspreadsheet = reinterpret_cast<SpaceSpreadsheet *>(space_link);
  spreadsheet_table_id_foreach_id(sspreadsheet->geometry_id.base, data);
  for (const int i : IndexRange(sspreadsheet->num_tables)) {
    spreadsheet_table_foreach_id(*sspreadsheet->tables[i], data);
  }
}

static void spreadsheet_main_region_init(wmWindowManager *wm, ARegion *region)
{
  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM | V2D_SCROLL_VERTICAL_HIDE |
                       V2D_SCROLL_HORIZONTAL_HIDE;
  region->v2d.align = V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y;
  region->v2d.keepzoom = V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT;
  region->v2d.keeptot = V2D_KEEPTOT_STRICT;
  region->v2d.minzoom = region->v2d.maxzoom = 1.0f;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_LIST, region->winx, region->winy);

  region->flag |= RGN_FLAG_INDICATE_OVERFLOW;

  {
    wmKeyMap *keymap = WM_keymap_ensure(
        wm->runtime->defaultconf, "View2D Buttons List", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
  }
  {
    wmKeyMap *keymap = WM_keymap_ensure(
        wm->runtime->defaultconf, "Spreadsheet Generic", SPACE_SPREADSHEET, RGN_TYPE_WINDOW);
    WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
  }
}

ID *get_current_id(const SpaceSpreadsheet *sspreadsheet)
{
  if (BLI_listbase_is_empty(&sspreadsheet->geometry_id.viewer_path.path)) {
    return nullptr;
  }
  ViewerPathElem *root_context = static_cast<ViewerPathElem *>(
      sspreadsheet->geometry_id.viewer_path.path.first);
  if (root_context->type != VIEWER_PATH_ELEM_TYPE_ID) {
    return nullptr;
  }
  IDViewerPathElem *id_elem = reinterpret_cast<IDViewerPathElem *>(root_context);
  return id_elem->id;
}

static void view_active_object(const bContext *C, SpaceSpreadsheet *sspreadsheet)
{
  BKE_viewer_path_clear(&sspreadsheet->geometry_id.viewer_path);
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  IDViewerPathElem *id_elem = BKE_viewer_path_elem_new_id();
  id_elem->id = &ob->id;
  BLI_addtail(&sspreadsheet->geometry_id.viewer_path.path, id_elem);
  ED_area_tag_redraw(CTX_wm_area(C));
}

static void spreadsheet_update_context(const bContext *C)
{
  using blender::ed::viewer_path::ViewerPathForGeometryNodesViewer;

  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  Object *active_object = CTX_data_active_object(C);
  Object *context_object = blender::ed::viewer_path::parse_object_only(
      sspreadsheet->geometry_id.viewer_path);
  switch (eSpaceSpreadsheet_ObjectEvalState(sspreadsheet->geometry_id.object_eval_state)) {
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
            blender::ed::viewer_path::parse_geometry_nodes_viewer(
                sspreadsheet->geometry_id.viewer_path);
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
        if (BKE_viewer_path_equal(&sspreadsheet->geometry_id.viewer_path,
                                  &workspace->viewer_path,
                                  VIEWER_PATH_EQUAL_FLAG_CONSIDER_UI_NAME))
        {
          /* Nothing changed. */
          break;
        }
        /* Update the viewer path from the workspace. */
        BKE_viewer_path_clear(&sspreadsheet->geometry_id.viewer_path);
        BKE_viewer_path_copy(&sspreadsheet->geometry_id.viewer_path, &workspace->viewer_path);
      }
      else {
        /* No active viewer node, change back to showing evaluated active object. */
        sspreadsheet->geometry_id.object_eval_state = SPREADSHEET_OBJECT_EVAL_STATE_EVALUATED;
        view_active_object(C, sspreadsheet);
      }

      break;
    }
  }
}

Object *spreadsheet_get_object_eval(const SpaceSpreadsheet *sspreadsheet,
                                    const Depsgraph *depsgraph)
{
  ID *used_id = get_current_id(sspreadsheet);
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
            OB_CURVES,
            OB_GREASE_PENCIL))
  {
    return nullptr;
  }

  Object *object_eval = DEG_get_evaluated(depsgraph, object_orig);
  if (object_eval == nullptr) {
    return nullptr;
  }

  return object_eval;
}

std::unique_ptr<DataSource> get_data_source(const bContext &C)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(&C);

  Object *object_eval = spreadsheet_get_object_eval(sspreadsheet, depsgraph);
  if (object_eval) {
    return data_source_from_geometry(&C, object_eval);
  }
  return {};
}

const SpreadsheetTableID *get_active_table_id(const SpaceSpreadsheet &sspreadsheet)
{
  return &sspreadsheet.geometry_id.base;
}

SpreadsheetTable *get_active_table(SpaceSpreadsheet &sspreadsheet)
{
  return const_cast<SpreadsheetTable *>(
      get_active_table(const_cast<const SpaceSpreadsheet &>(sspreadsheet)));
}

const SpreadsheetTable *get_active_table(const SpaceSpreadsheet &sspreadsheet)
{
  const SpreadsheetTableID *active_table_id = get_active_table_id(sspreadsheet);
  if (!active_table_id) {
    return nullptr;
  }
  return spreadsheet_table_find(sspreadsheet, *active_table_id);
}

static int get_index_column_width(const int tot_rows)
{
  const int fontid = BLF_default();
  BLF_size(fontid, UI_style_get_dpi()->widget.points * UI_SCALE_FAC);
  return std::to_string(std::max(0, tot_rows - 1)).size() * BLF_width(fontid, "0", 1) +
         UI_UNIT_X * 0.75;
}

static void update_visible_columns(SpreadsheetTable &table, DataSource &data_source)
{
  Set<std::reference_wrapper<const SpreadsheetColumnID>> handled_columns;
  Vector<SpreadsheetColumn *, 32> new_columns;
  for (SpreadsheetColumn *column : Span{table.columns, table.num_columns}) {
    if (handled_columns.add(*column->id)) {
      const bool has_data = data_source.get_column_values(*column->id) != nullptr;
      SET_FLAG_FROM_TEST(column->flag, !has_data, SPREADSHEET_COLUMN_FLAG_UNAVAILABLE);
      new_columns.append(column);
    }
  }

  data_source.foreach_default_column_ids(
      [&](const SpreadsheetColumnID &column_id, const bool is_extra) {
        if (handled_columns.contains(column_id)) {
          return;
        }
        std::unique_ptr<ColumnValues> values = data_source.get_column_values(column_id);
        if (!values) {
          return;
        }
        table.column_use_clock++;
        SpreadsheetColumn *column = spreadsheet_column_new(spreadsheet_column_id_copy(&column_id));
        if (is_extra) {
          new_columns.insert(0, column);
        }
        else {
          new_columns.append(column);
        }
        handled_columns.add(*column->id);
      });

  if (Span(table.columns, table.num_columns) == new_columns.as_span()) {
    /* Nothing changed. */
    return;
  }

  /* Update last used times of the columns to support garbage collection. */
  for (SpreadsheetColumn *column : new_columns) {
    const bool clock_was_reset = table.column_use_clock < column->last_used;
    if (clock_was_reset || column->is_available()) {
      column->last_used = table.column_use_clock;
    }
  }

  /* Update the stored column pointers. */
  MEM_SAFE_FREE(table.columns);
  table.columns = MEM_calloc_arrayN<SpreadsheetColumn *>(new_columns.size(), __func__);
  table.num_columns = new_columns.size();
  std::copy_n(new_columns.begin(), new_columns.size(), table.columns);

  /* Remove columns that have not been used for a while when there are too many. */
  spreadsheet_table_remove_unused_columns(table);
}

static void spreadsheet_main_region_draw(const bContext *C, ARegion *region)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  spreadsheet_update_context(C);

  std::unique_ptr<DataSource> data_source = get_data_source(*C);
  if (!data_source) {
    data_source = std::make_unique<DataSource>();
  }

  const SpreadsheetTableID *active_table_id = get_active_table_id(*sspreadsheet);
  SpreadsheetTable *table = spreadsheet_table_find(*sspreadsheet, *active_table_id);
  if (!table) {
    spreadsheet_table_remove_unused(*sspreadsheet);
    table = spreadsheet_table_new(spreadsheet_table_id_copy(*active_table_id));
    spreadsheet_table_add(*sspreadsheet, table);
  }
  if (table) {
    /* Move to the front of the tables list to make it cheaper to find the table in future. */
    spreadsheet_table_move_to_front(*sspreadsheet, *table);
  }

  /* Update the last used time on the table. */
  if (table->last_used < sspreadsheet->table_use_clock || sspreadsheet->table_use_clock == 0) {
    sspreadsheet->table_use_clock++;
    /* Handle clock overflow by just resetting all clocks. */
    if (sspreadsheet->table_use_clock == 0) {
      for (SpreadsheetTable *table : Span(sspreadsheet->tables, sspreadsheet->num_tables)) {
        table->last_used = sspreadsheet->table_use_clock;
      }
    }
    table->last_used = sspreadsheet->table_use_clock;
  }

  update_visible_columns(*table, *data_source);

  SpreadsheetLayout spreadsheet_layout;
  ResourceScope scope;

  const int tot_rows = data_source->tot_rows();
  spreadsheet_layout.index_column_width = get_index_column_width(tot_rows);

  int x = spreadsheet_layout.index_column_width;

  for (SpreadsheetColumn *column : Span{table->columns, table->num_columns}) {
    std::unique_ptr<ColumnValues> values_ptr = data_source->get_column_values(*column->id);
    if (!values_ptr) {
      continue;
    }
    const ColumnValues *values = scope.add(std::move(values_ptr));
    const eSpreadsheetColumnValueType column_type = values->type();

    if (column->width <= 0.0f || column_type != column->data_type) {
      column->width = values->fit_column_width_px(100) / SPREADSHEET_WIDTH_UNIT;
    }
    const int width_in_pixels = column->width * SPREADSHEET_WIDTH_UNIT;
    spreadsheet_layout.columns.append({values, width_in_pixels});

    column->runtime->left_x = x;
    x += width_in_pixels;
    column->runtime->right_x = x;

    spreadsheet_column_assign_runtime_data(column, column_type, values->name());
  }

  spreadsheet_layout.row_indices = spreadsheet_filter_rows(
      *sspreadsheet, spreadsheet_layout, *data_source, scope);

  sspreadsheet->runtime->tot_columns = spreadsheet_layout.columns.size();
  sspreadsheet->runtime->tot_rows = tot_rows;
  sspreadsheet->runtime->visible_rows = spreadsheet_layout.row_indices.size();

  std::unique_ptr<SpreadsheetDrawer> drawer = spreadsheet_drawer_from_layout(spreadsheet_layout);
  draw_spreadsheet_in_region(C, region, *drawer);

  sspreadsheet->runtime->top_row_height = drawer->top_row_height;
  sspreadsheet->runtime->left_column_width = drawer->left_column_width;

  rcti mask;
  UI_view2d_mask_from_win(&region->v2d, &mask);
  mask.ymax -= sspreadsheet->runtime->top_row_height;
  ED_region_draw_overflow_indication(CTX_wm_area(C), region, &mask);

  /* Tag other regions for redraw, because the main region updates data for them. */
  ARegion *footer = BKE_area_find_region_type(CTX_wm_area(C), RGN_TYPE_FOOTER);
  ED_region_tag_redraw(footer);
  ARegion *sidebar = BKE_area_find_region_type(CTX_wm_area(C), RGN_TYPE_UI);
  ED_region_tag_redraw(sidebar);
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
    case NC_GPENCIL: {
      ED_region_tag_redraw(region);
      break;
    }
    case NC_VIEWER_PATH: {
      if (sspreadsheet->geometry_id.object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE)
      {
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
    case NC_GPENCIL: {
      ED_region_tag_redraw(region);
      break;
    }
    case NC_VIEWER_PATH: {
      if (sspreadsheet->geometry_id.object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE)
      {
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

  uiBlock *block = UI_block_begin(C, region, __func__, ui::EmbossType::Emboss);
  const uiStyle *style = UI_style_get_dpi();
  uiLayout &layout = ui::block_layout(block,
                                      ui::LayoutDirection::Horizontal,
                                      ui::LayoutType::Header,
                                      UI_HEADER_OFFSET,
                                      region->winy - (region->winy - UI_UNIT_Y) / 2.0f,
                                      region->winx,
                                      1,
                                      0,
                                      style);
  layout.separator_spacer();
  layout.alignment_set(ui::LayoutAlign::Right);
  layout.label(stats_str, ICON_NONE);
  ui::block_layout_resolve(block);
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
      wm->runtime->defaultconf, "Spreadsheet Generic", SPACE_SPREADSHEET, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
}

static void spreadsheet_right_region_free(ARegion * /*region*/) {}

static void spreadsheet_right_region_listener(const wmRegionListenerParams * /*params*/) {}

static void spreadsheet_blend_read_data(BlendDataReader *reader, SpaceLink *sl)
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)sl;

  sspreadsheet->runtime = MEM_new<SpaceSpreadsheet_Runtime>(__func__);
  BLO_read_struct_list(reader, SpreadsheetRowFilter, &sspreadsheet->row_filters);
  LISTBASE_FOREACH (SpreadsheetRowFilter *, row_filter, &sspreadsheet->row_filters) {
    BLO_read_string(reader, &row_filter->value_string);
  }

  BLO_read_pointer_array(
      reader, sspreadsheet->num_tables, reinterpret_cast<void **>(&sspreadsheet->tables));
  for (const int i : IndexRange(sspreadsheet->num_tables)) {
    BLO_read_struct(reader, SpreadsheetTable, &sspreadsheet->tables[i]);
    spreadsheet_table_blend_read(reader, sspreadsheet->tables[i]);
  }

  spreadsheet_table_id_blend_read(reader, &sspreadsheet->geometry_id.base);
}

static void spreadsheet_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  BLO_write_struct(writer, SpaceSpreadsheet, sl);
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)sl;

  LISTBASE_FOREACH (SpreadsheetRowFilter *, row_filter, &sspreadsheet->row_filters) {
    BLO_write_struct(writer, SpreadsheetRowFilter, row_filter);
    BLO_write_string(writer, row_filter->value_string);
  }

  BLO_write_pointer_array(writer, sspreadsheet->num_tables, sspreadsheet->tables);
  for (const int i : IndexRange(sspreadsheet->num_tables)) {
    spreadsheet_table_blend_write(writer, sspreadsheet->tables[i]);
  }

  spreadsheet_table_id_blend_write_content_geometry(writer, &sspreadsheet->geometry_id);
}

static void spreadsheet_cursor(wmWindow *win, ScrArea *area, ARegion *region)
{
  SpaceSpreadsheet &sspreadsheet = *static_cast<SpaceSpreadsheet *>(area->spacedata.first);

  const int2 cursor_re{win->eventstate->xy[0] - region->winrct.xmin,
                       win->eventstate->xy[1] - region->winrct.ymin};
  if (find_hovered_column_header_edge(sspreadsheet, *region, cursor_re)) {
    WM_cursor_set(win, WM_CURSOR_X_MOVE);
    return;
  }
  if (find_hovered_column_header(sspreadsheet, *region, cursor_re)) {
    WM_cursor_set(win, WM_CURSOR_HAND);
    return;
  }
  WM_cursor_set(win, WM_CURSOR_DEFAULT);
}

void register_spacetype()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_SPREADSHEET;
  STRNCPY_UTF8(st->name, "Spreadsheet");

  st->create = spreadsheet_create;
  st->free = spreadsheet_free;
  st->init = spreadsheet_init;
  st->duplicate = spreadsheet_duplicate;
  st->operatortypes = spreadsheet_operatortypes;
  st->keymap = spreadsheet_keymap;
  st->id_remap = spreadsheet_id_remap;
  st->foreach_id = spreadsheet_foreach_id;
  st->blend_read_data = spreadsheet_blend_read_data;
  st->blend_read_after_liblink = nullptr;
  st->blend_write = spreadsheet_blend_write;

  /* regions: main window */
  art = MEM_callocN<ARegionType>("spacetype spreadsheet region");
  art->regionid = RGN_TYPE_WINDOW;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES;
  art->lock = REGION_DRAW_LOCK_ALL;

  art->init = spreadsheet_main_region_init;
  art->draw = spreadsheet_main_region_draw;
  art->listener = spreadsheet_main_region_listener;
  art->cursor = spreadsheet_cursor;
  art->event_cursor = true;
  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN<ARegionType>("spacetype spreadsheet header region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = 0;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER | ED_KEYMAP_FRAMES;
  art->lock = REGION_DRAW_LOCK_ALL;

  art->init = spreadsheet_header_region_init;
  art->draw = spreadsheet_header_region_draw;
  art->free = spreadsheet_header_region_free;
  art->listener = spreadsheet_header_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* regions: footer */
  art = MEM_callocN<ARegionType>("spacetype spreadsheet footer region");
  art->regionid = RGN_TYPE_FOOTER;
  art->prefsizey = HEADERY;
  art->keymapflag = 0;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER | ED_KEYMAP_FRAMES;
  art->lock = REGION_DRAW_LOCK_ALL;

  art->init = spreadsheet_footer_region_init;
  art->draw = spreadsheet_footer_region_draw;
  art->free = spreadsheet_footer_region_free;
  art->listener = spreadsheet_footer_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* regions: right panel buttons */
  art = MEM_callocN<ARegionType>("spacetype spreadsheet right region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->lock = REGION_DRAW_LOCK_ALL;

  art->init = spreadsheet_sidebar_init;
  art->layout = ED_region_panels_layout;
  art->draw = ED_region_panels_draw;
  art->free = spreadsheet_right_region_free;
  art->listener = spreadsheet_right_region_listener;
  BLI_addhead(&st->regiontypes, art);

  register_row_filter_panels(*art);

  /* regions: channels */
  art = MEM_callocN<ARegionType>("spreadsheet dataset region");
  art->regionid = RGN_TYPE_TOOLS;
  art->prefsizex = 150 + V2D_SCROLL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->lock = REGION_DRAW_LOCK_ALL;
  art->init = ED_region_panels_init;
  art->draw = spreadsheet_dataset_region_draw;
  art->listener = spreadsheet_dataset_region_listener;
  spreadsheet_data_set_region_panels_register(*art);
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(std::move(st));
}

}  // namespace blender::ed::spreadsheet
