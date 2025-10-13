/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstring>
#include <fmt/format.h>
#include <sstream>

#include "BLI_listbase.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_spreadsheet.hh"

#include "spreadsheet_intern.hh"
#include "spreadsheet_row_filter_ui.hh"

namespace blender::ed::spreadsheet {

static void filter_panel_id_fn(void * /*row_filter_v*/, char *r_name)
{
  /* All row filters use the same panel ID. */
  BLI_strncpy_utf8(r_name, "SPREADSHEET_PT_filter", BKE_ST_MAXNAME);
}

static std::string operation_string(const eSpreadsheetColumnValueType data_type,
                                    const eSpreadsheetFilterOperation operation)
{
  if (ELEM(data_type, SPREADSHEET_VALUE_TYPE_BOOL, SPREADSHEET_VALUE_TYPE_INSTANCES)) {
    return "=";
  }

  switch (operation) {
    case SPREADSHEET_ROW_FILTER_EQUAL:
      return "=";
    case SPREADSHEET_ROW_FILTER_GREATER:
      return ">";
    case SPREADSHEET_ROW_FILTER_LESS:
      return "<";
  }
  BLI_assert_unreachable();
  return "";
}

static std::string value_string(const SpreadsheetRowFilter &row_filter,
                                const eSpreadsheetColumnValueType data_type)
{
  switch (data_type) {
    case SPREADSHEET_VALUE_TYPE_INT8:
    case SPREADSHEET_VALUE_TYPE_INT32:
    case SPREADSHEET_VALUE_TYPE_INT64:
      return std::to_string(row_filter.value_int);
    case SPREADSHEET_VALUE_TYPE_FLOAT: {
      std::ostringstream result;
      result.precision(3);
      result << std::fixed << row_filter.value_float;
      return result.str();
    }
    case SPREADSHEET_VALUE_TYPE_INT32_2D: {
      std::ostringstream result;
      result << "(" << row_filter.value_int2[0] << ", " << row_filter.value_int2[1] << ")";
      return result.str();
    }
    case SPREADSHEET_VALUE_TYPE_INT32_3D: {
      std::ostringstream result;
      return fmt::format("({}, {}, {})",
                         row_filter.value_int3[0],
                         row_filter.value_int3[1],
                         row_filter.value_int3[2]);
    }
    case SPREADSHEET_VALUE_TYPE_FLOAT2: {
      std::ostringstream result;
      result.precision(3);
      result << std::fixed << "(" << row_filter.value_float2[0] << ", "
             << row_filter.value_float2[1] << ")";
      return result.str();
    }
    case SPREADSHEET_VALUE_TYPE_FLOAT3: {
      std::ostringstream result;
      result.precision(3);
      result << std::fixed << "(" << row_filter.value_float3[0] << ", "
             << row_filter.value_float3[1] << ", " << row_filter.value_float3[2] << ")";
      return result.str();
    }
    case SPREADSHEET_VALUE_TYPE_BOOL:
      return (row_filter.flag & SPREADSHEET_ROW_FILTER_BOOL_VALUE) ? IFACE_("True") :
                                                                     IFACE_("False");
    case SPREADSHEET_VALUE_TYPE_INSTANCES:
      if (row_filter.value_string != nullptr) {
        return row_filter.value_string;
      }
      return "";
    case SPREADSHEET_VALUE_TYPE_COLOR:
    case SPREADSHEET_VALUE_TYPE_BYTE_COLOR: {
      std::ostringstream result;
      result.precision(3);
      result << std::fixed << "(" << row_filter.value_color[0] << ", " << row_filter.value_color[1]
             << ", " << row_filter.value_color[2] << ", " << row_filter.value_color[3] << ")";
      return result.str();
    }
    case SPREADSHEET_VALUE_TYPE_STRING:
      return row_filter.value_string;
    case SPREADSHEET_VALUE_TYPE_QUATERNION:
    case SPREADSHEET_VALUE_TYPE_FLOAT4X4:
    case SPREADSHEET_VALUE_TYPE_BUNDLE_ITEM:
    case SPREADSHEET_VALUE_TYPE_UNKNOWN:
      return "";
  }
  BLI_assert_unreachable();
  return "";
}

static const SpreadsheetColumn *lookup_visible_column_for_filter(
    const SpaceSpreadsheet &sspreadsheet, const StringRef column_name)
{
  const SpreadsheetTable *table = get_active_table(sspreadsheet);
  if (!table) {
    return nullptr;
  }
  for (const SpreadsheetColumn *column : Span{table->columns, table->num_columns}) {
    if (column->display_name == column_name) {
      return column;
    }
  }
  return nullptr;
}

static void spreadsheet_filter_panel_draw_header(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  PointerRNA *filter_ptr = UI_panel_custom_data_get(panel);
  const SpreadsheetRowFilter *filter = (SpreadsheetRowFilter *)filter_ptr->data;
  const StringRef column_name = filter->column_name;
  const eSpreadsheetFilterOperation operation = (eSpreadsheetFilterOperation)filter->operation;

  const SpreadsheetColumn *column = lookup_visible_column_for_filter(*sspreadsheet, column_name);
  if (!(sspreadsheet->filter_flag & SPREADSHEET_FILTER_ENABLE) ||
      (column == nullptr && !column_name.is_empty()))
  {
    layout->active_set(false);
  }

  uiLayout *row = &layout->row(true);
  row->emboss_set(ui::EmbossType::None);
  row->prop(filter_ptr, "enabled", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);

  if (column_name.is_empty()) {
    row->label(IFACE_("Filter"), ICON_NONE);
  }
  else if (column == nullptr) {
    row->label(column_name.data(), ICON_NONE);
  }
  else {
    const eSpreadsheetColumnValueType data_type = (eSpreadsheetColumnValueType)column->data_type;
    std::stringstream ss;
    ss << column_name;
    ss << " ";
    ss << operation_string(data_type, operation);
    ss << " ";
    ss << value_string(*filter, data_type);
    row->label(ss.str(), ICON_NONE);
  }

  row = &layout->row(true);
  row->emboss_set(ui::EmbossType::None);
  const int current_index = BLI_findindex(&sspreadsheet->row_filters, filter);
  PointerRNA op_ptr = row->op("SPREADSHEET_OT_remove_row_filter_rule", "", ICON_X);
  RNA_int_set(&op_ptr, "index", current_index);
  /* Some padding so the X isn't too close to the drag icon. */
  layout->separator(0.25f);
}

static void spreadsheet_filter_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  PointerRNA *filter_ptr = UI_panel_custom_data_get(panel);
  SpreadsheetRowFilter *filter = (SpreadsheetRowFilter *)filter_ptr->data;
  const StringRef column_name = filter->column_name;
  const eSpreadsheetFilterOperation operation = (eSpreadsheetFilterOperation)filter->operation;

  const SpreadsheetColumn *column = lookup_visible_column_for_filter(*sspreadsheet, column_name);
  if (!(sspreadsheet->filter_flag & SPREADSHEET_FILTER_ENABLE) ||
      !(filter->flag & SPREADSHEET_ROW_FILTER_ENABLED) ||
      (column == nullptr && !column_name.is_empty()))
  {
    layout->active_set(false);
  }

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  layout->prop(filter_ptr, "column_name", UI_ITEM_NONE, IFACE_("Column"), ICON_NONE);

  /* Don't draw settings for filters with no corresponding visible column. */
  if (column == nullptr || column_name.is_empty()) {
    return;
  }

  switch (static_cast<eSpreadsheetColumnValueType>(column->data_type)) {
    case SPREADSHEET_VALUE_TYPE_INT8:
      layout->prop(filter_ptr, "operation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      layout->prop(filter_ptr, "value_int8", UI_ITEM_NONE, IFACE_("Value"), ICON_NONE);
      break;
    case SPREADSHEET_VALUE_TYPE_INT32:
    case SPREADSHEET_VALUE_TYPE_INT64:
      layout->prop(filter_ptr, "operation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      layout->prop(filter_ptr, "value_int", UI_ITEM_NONE, IFACE_("Value"), ICON_NONE);
      break;
    case SPREADSHEET_VALUE_TYPE_INT32_2D:
      layout->prop(filter_ptr, "operation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      layout->prop(filter_ptr, "value_int2", UI_ITEM_NONE, IFACE_("Value"), ICON_NONE);
      break;
    case SPREADSHEET_VALUE_TYPE_INT32_3D:
      layout->prop(filter_ptr, "operation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      layout->prop(filter_ptr, "value_int3", UI_ITEM_NONE, IFACE_("Value"), ICON_NONE);
      break;
    case SPREADSHEET_VALUE_TYPE_FLOAT:
      layout->prop(filter_ptr, "operation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      layout->prop(filter_ptr, "value_float", UI_ITEM_NONE, IFACE_("Value"), ICON_NONE);
      if (operation == SPREADSHEET_ROW_FILTER_EQUAL) {
        layout->prop(filter_ptr, "threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      }
      break;
    case SPREADSHEET_VALUE_TYPE_FLOAT2:
      layout->prop(filter_ptr, "operation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      layout->prop(filter_ptr, "value_float2", UI_ITEM_NONE, IFACE_("Value"), ICON_NONE);
      if (operation == SPREADSHEET_ROW_FILTER_EQUAL) {
        layout->prop(filter_ptr, "threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      }
      break;
    case SPREADSHEET_VALUE_TYPE_FLOAT3:
      layout->prop(filter_ptr, "operation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      layout->prop(filter_ptr, "value_float3", UI_ITEM_NONE, IFACE_("Value"), ICON_NONE);
      if (operation == SPREADSHEET_ROW_FILTER_EQUAL) {
        layout->prop(filter_ptr, "threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      }
      break;
    case SPREADSHEET_VALUE_TYPE_BOOL:
      layout->prop(filter_ptr, "value_boolean", UI_ITEM_NONE, IFACE_("Value"), ICON_NONE);
      break;
    case SPREADSHEET_VALUE_TYPE_INSTANCES:
      layout->prop(filter_ptr, "value_string", UI_ITEM_NONE, IFACE_("Value"), ICON_NONE);
      break;
    case SPREADSHEET_VALUE_TYPE_COLOR:
    case SPREADSHEET_VALUE_TYPE_BYTE_COLOR:
      layout->prop(filter_ptr, "operation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      layout->prop(filter_ptr, "value_color", UI_ITEM_NONE, IFACE_("Value"), ICON_NONE);
      if (operation == SPREADSHEET_ROW_FILTER_EQUAL) {
        layout->prop(filter_ptr, "threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      }
      break;
    case SPREADSHEET_VALUE_TYPE_STRING:
      layout->prop(filter_ptr, "value_string", UI_ITEM_NONE, IFACE_("Value"), ICON_NONE);
      break;
    case SPREADSHEET_VALUE_TYPE_UNKNOWN:
    case SPREADSHEET_VALUE_TYPE_QUATERNION:
    case SPREADSHEET_VALUE_TYPE_FLOAT4X4:
    case SPREADSHEET_VALUE_TYPE_BUNDLE_ITEM:
      layout->label(IFACE_("Unsupported column type"), ICON_ERROR);
      break;
  }
}

static void spreadsheet_row_filters_layout(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  ARegion *region = CTX_wm_region(C);
  bScreen *screen = CTX_wm_screen(C);
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  ListBase *row_filters = &sspreadsheet->row_filters;

  if (!(sspreadsheet->filter_flag & SPREADSHEET_FILTER_ENABLE)) {
    layout->active_set(false);
  }

  layout->op("SPREADSHEET_OT_add_row_filter_rule", std::nullopt, ICON_ADD);

  const bool panels_match = UI_panel_list_matches_data(region, row_filters, filter_panel_id_fn);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    LISTBASE_FOREACH (SpreadsheetRowFilter *, row_filter, row_filters) {
      char panel_idname[MAX_NAME];
      filter_panel_id_fn(row_filter, panel_idname);

      PointerRNA *filter_ptr = MEM_new<PointerRNA>("panel customdata");
      *filter_ptr = RNA_pointer_create_discrete(
          &screen->id, &RNA_SpreadsheetRowFilter, row_filter);

      UI_panel_add_instanced(C, region, &region->panels, panel_idname, filter_ptr);
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel_iter = (Panel *)region->panels.first;
    LISTBASE_FOREACH (SpreadsheetRowFilter *, row_filter, row_filters) {

      /* Move to the next instanced panel corresponding to the next filter. */
      while ((panel_iter->type == nullptr) || !(panel_iter->type->flag & PANEL_TYPE_INSTANCED)) {
        panel_iter = panel_iter->next;
        BLI_assert(panel_iter != nullptr); /* There shouldn't be fewer panels than filters. */
      }

      PointerRNA *filter_ptr = MEM_new<PointerRNA>("panel customdata");
      *filter_ptr = RNA_pointer_create_discrete(
          &screen->id, &RNA_SpreadsheetRowFilter, row_filter);
      UI_panel_custom_data_set(panel_iter, filter_ptr);

      panel_iter = panel_iter->next;
    }
  }
}

static void filter_reorder(bContext *C, Panel *panel, int new_index)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  ListBase *row_filters = &sspreadsheet->row_filters;
  PointerRNA *filter_ptr = UI_panel_custom_data_get(panel);
  SpreadsheetRowFilter *filter = (SpreadsheetRowFilter *)filter_ptr->data;

  int current_index = BLI_findindex(row_filters, filter);
  BLI_assert(current_index >= 0);
  BLI_assert(new_index >= 0);

  BLI_listbase_link_move(row_filters, filter, new_index - current_index);
}

static short get_filter_expand_flag(const bContext * /*C*/, Panel *panel)
{
  PointerRNA *filter_ptr = UI_panel_custom_data_get(panel);
  SpreadsheetRowFilter *filter = (SpreadsheetRowFilter *)filter_ptr->data;

  return short(filter->flag) & SPREADSHEET_ROW_FILTER_UI_EXPAND;
}

static void set_filter_expand_flag(const bContext * /*C*/, Panel *panel, short expand_flag)
{
  PointerRNA *filter_ptr = UI_panel_custom_data_get(panel);
  SpreadsheetRowFilter *filter = (SpreadsheetRowFilter *)filter_ptr->data;

  SET_FLAG_FROM_TEST(filter->flag,
                     expand_flag & SPREADSHEET_ROW_FILTER_UI_EXPAND,
                     SPREADSHEET_ROW_FILTER_UI_EXPAND);
}

void register_row_filter_panels(ARegionType &region_type)
{
  {
    PanelType *panel_type = MEM_callocN<PanelType>(__func__);
    STRNCPY_UTF8(panel_type->idname, "SPREADSHEET_PT_row_filters");
    STRNCPY_UTF8(panel_type->label, N_("Filters"));
    STRNCPY_UTF8(panel_type->category, "Filters");
    STRNCPY_UTF8(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
    panel_type->flag = PANEL_TYPE_NO_HEADER;
    panel_type->draw = spreadsheet_row_filters_layout;
    BLI_addtail(&region_type.paneltypes, panel_type);
  }

  {
    PanelType *panel_type = MEM_callocN<PanelType>(__func__);
    STRNCPY_UTF8(panel_type->idname, "SPREADSHEET_PT_filter");
    STRNCPY_UTF8(panel_type->label, "");
    STRNCPY_UTF8(panel_type->category, "Filters");
    STRNCPY_UTF8(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
    panel_type->flag = PANEL_TYPE_INSTANCED | PANEL_TYPE_HEADER_EXPAND;
    panel_type->draw_header = spreadsheet_filter_panel_draw_header;
    panel_type->draw = spreadsheet_filter_panel_draw;
    panel_type->get_list_data_expand_flag = get_filter_expand_flag;
    panel_type->set_list_data_expand_flag = set_filter_expand_flag;
    panel_type->reorder = filter_reorder;
    BLI_addtail(&region_type.paneltypes, panel_type);
  }
}

}  // namespace blender::ed::spreadsheet
