/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstring>

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_screen.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLT_translation.h"

#include "WM_api.h"
#include "WM_types.h"

#include "spreadsheet_column.hh"
#include "spreadsheet_intern.hh"
#include "spreadsheet_row_filter.hh"
#include "spreadsheet_row_filter_ui.hh"

using namespace blender;
using namespace blender::ed::spreadsheet;

static void filter_panel_id_fn(void *UNUSED(row_filter_v), char *r_name)
{
  /* All row filters use the same panel ID. */
  BLI_snprintf(r_name, BKE_ST_MAXNAME, "SPREADSHEET_PT_filter");
}

static std::string operation_string(const eSpreadsheetColumnValueType data_type,
                                    const eSpreadsheetFilterOperation operation)
{
  if (ELEM(data_type,
           SPREADSHEET_VALUE_TYPE_BOOL,
           SPREADSHEET_VALUE_TYPE_INSTANCES,
           SPREADSHEET_VALUE_TYPE_COLOR)) {
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
    case SPREADSHEET_VALUE_TYPE_INT32:
      return std::to_string(row_filter.value_int);
    case SPREADSHEET_VALUE_TYPE_FLOAT: {
      std::ostringstream result;
      result.precision(3);
      result << std::fixed << row_filter.value_float;
      return result.str();
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
    case SPREADSHEET_VALUE_TYPE_COLOR: {
      std::ostringstream result;
      result.precision(3);
      result << std::fixed << "(" << row_filter.value_color[0] << ", " << row_filter.value_color[1]
             << ", " << row_filter.value_color[2] << ", " << row_filter.value_color[3] << ")";
      return result.str();
    }
    case SPREADSHEET_VALUE_TYPE_STRING:
      return row_filter.value_string;
    case SPREADSHEET_VALUE_TYPE_UNKNOWN:
      return "";
  }
  BLI_assert_unreachable();
  return "";
}

static SpreadsheetColumn *lookup_visible_column_for_filter(const SpaceSpreadsheet &sspreadsheet,
                                                           const StringRef column_name)
{
  LISTBASE_FOREACH (SpreadsheetColumn *, column, &sspreadsheet.columns) {
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
      (column == nullptr && !column_name.is_empty())) {
    uiLayoutSetActive(layout, false);
  }

  uiLayout *row = uiLayoutRow(layout, true);
  uiLayoutSetEmboss(row, UI_EMBOSS_NONE);
  uiItemR(row, filter_ptr, "enabled", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);

  if (column_name.is_empty()) {
    uiItemL(row, IFACE_("Filter"), ICON_NONE);
  }
  else if (column == nullptr) {
    uiItemL(row, column_name.data(), ICON_NONE);
  }
  else {
    const eSpreadsheetColumnValueType data_type = (eSpreadsheetColumnValueType)column->data_type;
    std::stringstream ss;
    ss << column_name;
    ss << " ";
    ss << operation_string(data_type, operation);
    ss << " ";
    ss << value_string(*filter, data_type);
    uiItemL(row, ss.str().c_str(), ICON_NONE);
  }

  row = uiLayoutRow(layout, true);
  uiLayoutSetEmboss(row, UI_EMBOSS_NONE);
  const int current_index = BLI_findindex(&sspreadsheet->row_filters, filter);
  uiItemIntO(row, "", ICON_X, "SPREADSHEET_OT_remove_row_filter_rule", "index", current_index);

  /* Some padding so the X isn't too close to the drag icon. */
  uiItemS_ex(layout, 0.25f);
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
      (column == nullptr && !column_name.is_empty())) {
    uiLayoutSetActive(layout, false);
  }

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiItemR(layout, filter_ptr, "column_name", 0, IFACE_("Column"), ICON_NONE);

  /* Don't draw settings for filters with no corresponding visible column. */
  if (column == nullptr || column_name.is_empty()) {
    return;
  }

  switch (static_cast<eSpreadsheetColumnValueType>(column->data_type)) {
    case SPREADSHEET_VALUE_TYPE_INT32:
      uiItemR(layout, filter_ptr, "operation", 0, nullptr, ICON_NONE);
      uiItemR(layout, filter_ptr, "value_int", 0, IFACE_("Value"), ICON_NONE);
      break;
    case SPREADSHEET_VALUE_TYPE_FLOAT:
      uiItemR(layout, filter_ptr, "operation", 0, nullptr, ICON_NONE);
      uiItemR(layout, filter_ptr, "value_float", 0, IFACE_("Value"), ICON_NONE);
      if (operation == SPREADSHEET_ROW_FILTER_EQUAL) {
        uiItemR(layout, filter_ptr, "threshold", 0, nullptr, ICON_NONE);
      }
      break;
    case SPREADSHEET_VALUE_TYPE_FLOAT2:
      uiItemR(layout, filter_ptr, "operation", 0, nullptr, ICON_NONE);
      uiItemR(layout, filter_ptr, "value_float2", 0, IFACE_("Value"), ICON_NONE);
      if (operation == SPREADSHEET_ROW_FILTER_EQUAL) {
        uiItemR(layout, filter_ptr, "threshold", 0, nullptr, ICON_NONE);
      }
      break;
    case SPREADSHEET_VALUE_TYPE_FLOAT3:
      uiItemR(layout, filter_ptr, "operation", 0, nullptr, ICON_NONE);
      uiItemR(layout, filter_ptr, "value_float3", 0, IFACE_("Value"), ICON_NONE);
      if (operation == SPREADSHEET_ROW_FILTER_EQUAL) {
        uiItemR(layout, filter_ptr, "threshold", 0, nullptr, ICON_NONE);
      }
      break;
    case SPREADSHEET_VALUE_TYPE_BOOL:
      uiItemR(layout, filter_ptr, "value_boolean", 0, IFACE_("Value"), ICON_NONE);
      break;
    case SPREADSHEET_VALUE_TYPE_INSTANCES:
      uiItemR(layout, filter_ptr, "value_string", 0, IFACE_("Value"), ICON_NONE);
      break;
    case SPREADSHEET_VALUE_TYPE_COLOR:
      uiItemR(layout, filter_ptr, "value_color", 0, IFACE_("Value"), ICON_NONE);
      uiItemR(layout, filter_ptr, "threshold", 0, nullptr, ICON_NONE);
      break;
    case SPREADSHEET_VALUE_TYPE_STRING:
      uiItemR(layout, filter_ptr, "value_string", 0, IFACE_("Value"), ICON_NONE);
      break;
    case SPREADSHEET_VALUE_TYPE_UNKNOWN:
      uiItemL(layout, IFACE_("Unknown column type"), ICON_ERROR);
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
    uiLayoutSetActive(layout, false);
  }

  uiItemO(layout, nullptr, ICON_ADD, "SPREADSHEET_OT_add_row_filter_rule");

  const bool panels_match = UI_panel_list_matches_data(region, row_filters, filter_panel_id_fn);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    LISTBASE_FOREACH (SpreadsheetRowFilter *, row_filter, row_filters) {
      char panel_idname[MAX_NAME];
      filter_panel_id_fn(row_filter, panel_idname);

      PointerRNA *filter_ptr = (PointerRNA *)MEM_mallocN(sizeof(PointerRNA), "panel customdata");
      RNA_pointer_create(&screen->id, &RNA_SpreadsheetRowFilter, row_filter, filter_ptr);

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

      PointerRNA *filter_ptr = (PointerRNA *)MEM_mallocN(sizeof(PointerRNA), "panel customdata");
      RNA_pointer_create(&screen->id, &RNA_SpreadsheetRowFilter, row_filter, filter_ptr);
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

static short get_filter_expand_flag(const bContext *UNUSED(C), Panel *panel)
{
  PointerRNA *filter_ptr = UI_panel_custom_data_get(panel);
  SpreadsheetRowFilter *filter = (SpreadsheetRowFilter *)filter_ptr->data;

  return (short)filter->flag & SPREADSHEET_ROW_FILTER_UI_EXPAND;
}

static void set_filter_expand_flag(const bContext *UNUSED(C), Panel *panel, short expand_flag)
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
    PanelType *panel_type = MEM_cnew<PanelType>(__func__);
    strcpy(panel_type->idname, "SPREADSHEET_PT_row_filters");
    strcpy(panel_type->label, N_("Filters"));
    strcpy(panel_type->category, "Filters");
    strcpy(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
    panel_type->flag = PANEL_TYPE_NO_HEADER;
    panel_type->draw = spreadsheet_row_filters_layout;
    BLI_addtail(&region_type.paneltypes, panel_type);
  }

  {
    PanelType *panel_type = MEM_cnew<PanelType>(__func__);
    strcpy(panel_type->idname, "SPREADSHEET_PT_filter");
    strcpy(panel_type->label, "");
    strcpy(panel_type->category, "Filters");
    strcpy(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
    panel_type->flag = PANEL_TYPE_INSTANCED | PANEL_TYPE_HEADER_EXPAND;
    panel_type->draw_header = spreadsheet_filter_panel_draw_header;
    panel_type->draw = spreadsheet_filter_panel_draw;
    panel_type->get_list_data_expand_flag = get_filter_expand_flag;
    panel_type->set_list_data_expand_flag = set_filter_expand_flag;
    panel_type->reorder = filter_reorder;
    BLI_addtail(&region_type.paneltypes, panel_type);
  }
}
