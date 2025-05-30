/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "DNA_array_utils.hh"
#include "DNA_space_types.h"

#include "ED_screen.hh"
#include "ED_spreadsheet.hh"

#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "BKE_context.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface_c.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "spreadsheet_column.hh"
#include "spreadsheet_intern.hh"
#include "spreadsheet_row_filter.hh"

namespace blender::ed::spreadsheet {

static wmOperatorStatus row_filter_add_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);

  SpreadsheetRowFilter *row_filter = spreadsheet_row_filter_new();
  BLI_addtail(&sspreadsheet->row_filters, row_filter);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_SPREADSHEET, sspreadsheet);

  return OPERATOR_FINISHED;
}

static void SPREADSHEET_OT_add_row_filter_rule(wmOperatorType *ot)
{
  ot->name = "Add Row Filter";
  ot->description = "Add a filter to remove rows from the displayed data";
  ot->idname = "SPREADSHEET_OT_add_row_filter_rule";

  ot->exec = row_filter_add_exec;
  ot->poll = ED_operator_spreadsheet_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus row_filter_remove_exec(bContext *C, wmOperator *op)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);

  SpreadsheetRowFilter *row_filter = (SpreadsheetRowFilter *)BLI_findlink(
      &sspreadsheet->row_filters, RNA_int_get(op->ptr, "index"));
  if (row_filter == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BLI_remlink(&sspreadsheet->row_filters, row_filter);
  spreadsheet_row_filter_free(row_filter);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_SPREADSHEET, sspreadsheet);

  return OPERATOR_FINISHED;
}

static void SPREADSHEET_OT_remove_row_filter_rule(wmOperatorType *ot)
{
  ot->name = "Remove Row Filter";
  ot->description = "Remove a row filter from the rules";
  ot->idname = "SPREADSHEET_OT_remove_row_filter_rule";

  ot->exec = row_filter_remove_exec;
  ot->poll = ED_operator_spreadsheet_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, INT_MAX);
}

static wmOperatorStatus select_component_domain_invoke(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent * /*event*/)
{
  const auto component_type = bke::GeometryComponent::Type(RNA_int_get(op->ptr, "component_type"));
  bke::AttrDomain domain = bke::AttrDomain(RNA_int_get(op->ptr, "attribute_domain_type"));

  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  sspreadsheet->geometry_id.geometry_component_type = uint8_t(component_type);
  sspreadsheet->geometry_id.attribute_domain = uint8_t(domain);

  /* Refresh header and main region. */
  WM_main_add_notifier(NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  return OPERATOR_FINISHED;
}

static void SPREADSHEET_OT_change_spreadsheet_data_source(wmOperatorType *ot)
{
  ot->name = "Change Visible Data Source";
  ot->description = "Change visible data source in the spreadsheet";
  ot->idname = "SPREADSHEET_OT_change_spreadsheet_data_source";

  ot->invoke = select_component_domain_invoke;
  ot->poll = ED_operator_spreadsheet_active;

  RNA_def_int(ot->srna, "component_type", 0, 0, INT16_MAX, "Component Type", "", 0, INT16_MAX);
  RNA_def_int(ot->srna,
              "attribute_domain_type",
              0,
              0,
              INT16_MAX,
              "Attribute Domain Type",
              "",
              0,
              INT16_MAX);

  ot->flag = OPTYPE_INTERNAL;
}

struct ResizeColumnData {
  SpreadsheetColumn *column = nullptr;
  int2 initial_cursor_re;
  int initial_width_px;
};

static wmOperatorStatus resize_column_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion &region = *CTX_wm_region(C);
  SpaceSpreadsheet &sspreadsheet = *CTX_wm_space_spreadsheet(C);

  SpreadsheetTable &table = *get_active_table(sspreadsheet);
  ResizeColumnData &data = *static_cast<ResizeColumnData *>(op->customdata);

  auto cancel = [&]() {
    data.column->width = data.initial_width_px / SPREADSHEET_WIDTH_UNIT;
    MEM_delete(&data);
    ED_region_tag_redraw(&region);
    return OPERATOR_CANCELLED;
  };
  auto finish = [&]() {
    table.flag |= SPREADSHEET_TABLE_FLAG_MANUALLY_EDITED;
    MEM_delete(&data);
    ED_region_tag_redraw(&region);
    return OPERATOR_FINISHED;
  };

  const int2 cursor_re{event->mval[0], event->mval[1]};

  switch (event->type) {
    case RIGHTMOUSE:
    case EVT_ESCKEY: {
      return cancel();
    }
    case LEFTMOUSE: {
      return finish();
    }
    case MOUSEMOVE: {
      const int offset = cursor_re.x - data.initial_cursor_re.x;
      const float new_width_px = std::max<float>(SPREADSHEET_WIDTH_UNIT,
                                                 data.initial_width_px + offset);
      data.column->width = new_width_px / SPREADSHEET_WIDTH_UNIT;
      ED_region_tag_redraw(&region);
      return OPERATOR_RUNNING_MODAL;
    }
    default: {
      return OPERATOR_RUNNING_MODAL;
    }
  }
}

static bool is_hovering_header_row(const SpaceSpreadsheet &sspreadsheet,
                                   const ARegion &region,
                                   const int2 &cursor_re)
{
  const int region_height = BLI_rcti_size_y(&region.winrct);
  return cursor_re.y >= region_height - sspreadsheet.runtime->top_row_height &&
         cursor_re.y <= region_height;
}

SpreadsheetColumn *find_hovered_column_edge(SpaceSpreadsheet &sspreadsheet,
                                            ARegion &region,
                                            const int2 &cursor_re)
{
  SpreadsheetTable *table = get_active_table(sspreadsheet);
  if (!table) {
    return nullptr;
  }
  const float cursor_x_view = UI_view2d_region_to_view_x(&region.v2d, cursor_re.x);
  for (SpreadsheetColumn *column : Span{table->columns, table->num_columns}) {
    if (column->flag & SPREADSHEET_COLUMN_FLAG_UNAVAILABLE) {
      continue;
    }
    if (std::abs(cursor_x_view - column->runtime->right_x) < SPREADSHEET_EDGE_ACTION_ZONE) {
      return column;
    }
  }
  return nullptr;
}

SpreadsheetColumn *find_hovered_column(SpaceSpreadsheet &sspreadsheet,
                                       ARegion &region,
                                       const int2 &cursor_re)
{
  SpreadsheetTable *table = get_active_table(sspreadsheet);
  if (!table) {
    return nullptr;
  }
  const float cursor_x_view = UI_view2d_region_to_view_x(&region.v2d, cursor_re.x);
  for (SpreadsheetColumn *column : Span{table->columns, table->num_columns}) {
    if (column->flag & SPREADSHEET_COLUMN_FLAG_UNAVAILABLE) {
      continue;
    }
    if (cursor_x_view > column->runtime->left_x && cursor_x_view <= column->runtime->right_x) {
      return column;
    }
  }
  return nullptr;
}

SpreadsheetColumn *find_hovered_column_header_edge(SpaceSpreadsheet &sspreadsheet,
                                                   ARegion &region,
                                                   const int2 &cursor_re)
{
  if (!is_hovering_header_row(sspreadsheet, region, cursor_re)) {
    return nullptr;
  }
  return find_hovered_column_edge(sspreadsheet, region, cursor_re);
}

SpreadsheetColumn *find_hovered_column_header(SpaceSpreadsheet &sspreadsheet,
                                              ARegion &region,
                                              const int2 &cursor_re)
{
  if (!is_hovering_header_row(sspreadsheet, region, cursor_re)) {
    return nullptr;
  }
  return find_hovered_column(sspreadsheet, region, cursor_re);
}

static wmOperatorStatus resize_column_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion &region = *CTX_wm_region(C);
  SpaceSpreadsheet &sspreadsheet = *CTX_wm_space_spreadsheet(C);

  const int2 cursor_re{event->mval[0], event->mval[1]};
  SpreadsheetColumn *column_to_resize = find_hovered_column_header_edge(
      sspreadsheet, region, cursor_re);
  if (!column_to_resize) {
    return OPERATOR_PASS_THROUGH;
  }

  ResizeColumnData *data = MEM_new<ResizeColumnData>(__func__);
  data->column = column_to_resize;
  data->initial_cursor_re = cursor_re;
  data->initial_width_px = column_to_resize->width * SPREADSHEET_WIDTH_UNIT;
  op->customdata = data;

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static void SPREADSHEET_OT_resize_column(wmOperatorType *ot)
{
  ot->name = "Resize Column";
  ot->description = "Resize a spreadsheet column";
  ot->idname = "SPREADSHEET_OT_resize_column";

  ot->invoke = resize_column_invoke;
  ot->modal = resize_column_modal;
  ot->poll = ED_operator_spreadsheet_active;
  ot->flag = OPTYPE_INTERNAL;
}

static wmOperatorStatus fit_column_invoke(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  SpaceSpreadsheet &sspreadsheet = *CTX_wm_space_spreadsheet(C);
  ARegion &region = *CTX_wm_region(C);

  std::unique_ptr<DataSource> data_source = get_data_source(*C);
  if (!data_source) {
    return OPERATOR_CANCELLED;
  }
  const int2 cursor_re{event->mval[0], event->mval[1]};
  SpreadsheetColumn *column = find_hovered_column_header_edge(sspreadsheet, region, cursor_re);
  if (!column) {
    return OPERATOR_PASS_THROUGH;
  }

  std::unique_ptr<ColumnValues> values = data_source->get_column_values(*column->id);
  if (!values) {
    return OPERATOR_CANCELLED;
  }

  SpreadsheetTable &table = *get_active_table(sspreadsheet);
  table.flag |= SPREADSHEET_TABLE_FLAG_MANUALLY_EDITED;

  const float width_px = values->fit_column_width_px();
  column->width = width_px / SPREADSHEET_WIDTH_UNIT;

  ED_region_tag_redraw(&region);
  return OPERATOR_FINISHED;
}

static void SPREADSHEET_OT_fit_column(wmOperatorType *ot)
{
  ot->name = "Fit Column";
  ot->description = "Resize a spreadsheet column to the width of the data";
  ot->idname = "SPREADSHEET_OT_fit_column";

  ot->invoke = fit_column_invoke;
  ot->poll = ED_operator_spreadsheet_active;
  ot->flag = OPTYPE_INTERNAL;
}

struct ReorderColumnData {
  SpreadsheetColumn *column = nullptr;
  int initial_cursor_x_view = 0;
  View2DEdgePanData pan_data{};
};

static std::optional<int> find_first_available_column_index(const SpreadsheetTable &table)
{
  for (int i = 0; i < table.num_columns; i++) {
    if (!(table.columns[i]->flag & SPREADSHEET_COLUMN_FLAG_UNAVAILABLE)) {
      return i;
    }
  }
  return std::nullopt;
}

static std::optional<int> find_last_available_column_index(const SpreadsheetTable &table)
{
  for (int i = table.num_columns - 1; i >= 0; i--) {
    if (!(table.columns[i]->flag & SPREADSHEET_COLUMN_FLAG_UNAVAILABLE)) {
      return i;
    }
  }
  return std::nullopt;
}

static wmOperatorStatus reorder_columns_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceSpreadsheet &sspreadsheet = *CTX_wm_space_spreadsheet(C);
  ARegion &region = *CTX_wm_region(C);

  const int2 cursor_re{event->mval[0], event->mval[1]};

  if (find_hovered_column_edge(sspreadsheet, region, cursor_re)) {
    return OPERATOR_PASS_THROUGH;
  }

  SpreadsheetColumn *column_to_move = find_hovered_column_header(sspreadsheet, region, cursor_re);
  if (!column_to_move) {
    return OPERATOR_PASS_THROUGH;
  }

  WM_cursor_set(CTX_wm_window(C), WM_CURSOR_HAND_CLOSED);

  SpreadsheetTable *table = get_active_table(sspreadsheet);
  const int old_index = Span{table->columns, table->num_columns}.first_index(column_to_move);

  ReorderColumnData *data = MEM_new<ReorderColumnData>(__func__);
  data->column = column_to_move;
  data->initial_cursor_x_view = UI_view2d_region_to_view_x(&region.v2d, cursor_re.x);
  op->customdata = data;

  ReorderColumnVisualizationData &visualization_data =
      sspreadsheet.runtime->reorder_column_visualization_data.emplace();
  visualization_data.old_index = old_index;
  visualization_data.new_index = old_index;
  visualization_data.current_offset_x_px = 0;

  UI_view2d_edge_pan_init(C, &data->pan_data, 0, 0, 1, 26, 0.5f, 0.0f);
  /* Limit to horizontal panning. */
  data->pan_data.limit.xmin = region.v2d.tot.xmin;
  data->pan_data.limit.xmax = region.v2d.tot.xmax;
  data->pan_data.limit.ymin = region.v2d.cur.ymin;
  data->pan_data.limit.ymax = region.v2d.cur.ymax;

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus reorder_columns_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceSpreadsheet &sspreadsheet = *CTX_wm_space_spreadsheet(C);
  ARegion &region = *CTX_wm_region(C);

  const int2 cursor_re{event->mval[0], event->mval[1]};
  ReorderColumnData &data = *static_cast<ReorderColumnData *>(op->customdata);

  SpreadsheetTable &table = *get_active_table(sspreadsheet);
  Span<SpreadsheetColumn *> columns(table.columns, table.num_columns);

  const int old_index = columns.first_index(data.column);
  int new_index = 0;

  SpreadsheetColumn *hovered_column = find_hovered_column(sspreadsheet, region, cursor_re);
  if (hovered_column) {
    new_index = columns.first_index(hovered_column);
  }
  else {
    if (cursor_re.x > sspreadsheet.runtime->left_column_width) {
      new_index = *find_last_available_column_index(table);
    }
    else {
      new_index = *find_first_available_column_index(table);
    }
  }

  auto cleanup_on_finish = [&]() {
    sspreadsheet.runtime->reorder_column_visualization_data.reset();
    MEM_delete(&data);
    ED_region_tag_redraw(&region);
    WM_cursor_set(CTX_wm_window(C), WM_CURSOR_DEFAULT);
  };

  switch (event->type) {
    case RIGHTMOUSE:
    case EVT_ESCKEY: {
      UI_view2d_edge_pan_cancel(C, &data.pan_data);
      cleanup_on_finish();
      return OPERATOR_CANCELLED;
    }
    case LEFTMOUSE: {
      if (old_index != new_index) {
        dna::array::move_index(table.columns, table.num_columns, old_index, new_index);
      }
      table.flag |= SPREADSHEET_TABLE_FLAG_MANUALLY_EDITED;
      cleanup_on_finish();
      return OPERATOR_FINISHED;
    }
    case MOUSEMOVE: {
      UI_view2d_edge_pan_apply(C, &data.pan_data, event->xy);

      ReorderColumnVisualizationData &visualization_data =
          *sspreadsheet.runtime->reorder_column_visualization_data;
      visualization_data.new_index = new_index;
      visualization_data.current_offset_x_px = UI_view2d_region_to_view_x(&region.v2d,
                                                                          cursor_re.x) -
                                               data.initial_cursor_x_view;
      ED_region_tag_redraw(&region);
      return OPERATOR_RUNNING_MODAL;
    }
    case WHEELLEFTMOUSE:
    case WHEELRIGHTMOUSE: {
      if (BLI_rcti_isect_pt_v(&region.winrct, event->xy)) {
        /* Support scrolling left and right. */
        return OPERATOR_PASS_THROUGH;
      }
      return OPERATOR_RUNNING_MODAL;
    }
    default: {
      return OPERATOR_RUNNING_MODAL;
    }
  }
}

static void SPREADSHEET_OT_reorder_columns(wmOperatorType *ot)
{
  ot->name = "Reorder Columns";
  ot->description = "Change the order of columns";
  ot->idname = "SPREADSHEET_OT_reorder_columns";

  ot->poll = ED_operator_spreadsheet_active;
  ot->invoke = reorder_columns_invoke;
  ot->modal = reorder_columns_modal;
  ot->flag = OPTYPE_INTERNAL;
}

void spreadsheet_operatortypes()
{
  WM_operatortype_append(SPREADSHEET_OT_add_row_filter_rule);
  WM_operatortype_append(SPREADSHEET_OT_remove_row_filter_rule);
  WM_operatortype_append(SPREADSHEET_OT_change_spreadsheet_data_source);
  WM_operatortype_append(SPREADSHEET_OT_resize_column);
  WM_operatortype_append(SPREADSHEET_OT_fit_column);
  WM_operatortype_append(SPREADSHEET_OT_reorder_columns);
}

}  // namespace blender::ed::spreadsheet
