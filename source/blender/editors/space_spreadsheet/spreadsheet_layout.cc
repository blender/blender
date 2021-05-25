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

#include <iomanip>
#include <sstream>

#include "spreadsheet_layout.hh"

#include "DNA_userdef_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLF_api.h"

namespace blender::ed::spreadsheet {

class SpreadsheetLayoutDrawer : public SpreadsheetDrawer {
 private:
  const SpreadsheetLayout &spreadsheet_layout_;

 public:
  SpreadsheetLayoutDrawer(const SpreadsheetLayout &spreadsheet_layout)
      : spreadsheet_layout_(spreadsheet_layout)
  {
    tot_columns = spreadsheet_layout.columns.size();
    tot_rows = spreadsheet_layout.row_indices.size();
    left_column_width = spreadsheet_layout.index_column_width;
  }

  void draw_top_row_cell(int column_index, const CellDrawParams &params) const final
  {
    const StringRefNull name = spreadsheet_layout_.columns[column_index].values->name();
    uiBut *but = uiDefIconTextBut(params.block,
                                  UI_BTYPE_LABEL,
                                  0,
                                  ICON_NONE,
                                  name.c_str(),
                                  params.xmin,
                                  params.ymin,
                                  params.width,
                                  params.height,
                                  nullptr,
                                  0,
                                  0,
                                  0,
                                  0,
                                  nullptr);
    /* Center-align column headers. */
    UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
    UI_but_drawflag_disable(but, UI_BUT_TEXT_RIGHT);
  }

  void draw_left_column_cell(int row_index, const CellDrawParams &params) const final
  {
    const int real_index = spreadsheet_layout_.row_indices[row_index];
    std::string index_str = std::to_string(real_index);
    uiBut *but = uiDefIconTextBut(params.block,
                                  UI_BTYPE_LABEL,
                                  0,
                                  ICON_NONE,
                                  index_str.c_str(),
                                  params.xmin,
                                  params.ymin,
                                  params.width,
                                  params.height,
                                  nullptr,
                                  0,
                                  0,
                                  0,
                                  0,
                                  nullptr);
    /* Right-align indices. */
    UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
    UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  }

  void draw_content_cell(int row_index, int column_index, const CellDrawParams &params) const final
  {
    const int real_index = spreadsheet_layout_.row_indices[row_index];
    const ColumnValues &column = *spreadsheet_layout_.columns[column_index].values;
    CellValue cell_value;
    column.get_value(real_index, cell_value);

    if (cell_value.value_int.has_value()) {
      const int value = *cell_value.value_int;
      const std::string value_str = std::to_string(value);
      uiBut *but = uiDefIconTextBut(params.block,
                                    UI_BTYPE_LABEL,
                                    0,
                                    ICON_NONE,
                                    value_str.c_str(),
                                    params.xmin,
                                    params.ymin,
                                    params.width,
                                    params.height,
                                    nullptr,
                                    0,
                                    0,
                                    0,
                                    0,
                                    nullptr);
      /* Right-align Integers. */
      UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
      UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
    }
    else if (cell_value.value_float.has_value()) {
      const float value = *cell_value.value_float;
      std::stringstream ss;
      ss << std::fixed << std::setprecision(3) << value;
      const std::string value_str = ss.str();
      uiBut *but = uiDefIconTextBut(params.block,
                                    UI_BTYPE_LABEL,
                                    0,
                                    ICON_NONE,
                                    value_str.c_str(),
                                    params.xmin,
                                    params.ymin,
                                    params.width,
                                    params.height,
                                    nullptr,
                                    0,
                                    0,
                                    0,
                                    0,
                                    nullptr);
      /* Right-align Floats. */
      UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
      UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
    }
    else if (cell_value.value_bool.has_value()) {
      const bool value = *cell_value.value_bool;
      const int icon = value ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
      uiBut *but = uiDefIconTextBut(params.block,
                                    UI_BTYPE_LABEL,
                                    0,
                                    icon,
                                    "",
                                    params.xmin,
                                    params.ymin,
                                    params.width,
                                    params.height,
                                    nullptr,
                                    0,
                                    0,
                                    0,
                                    0,
                                    nullptr);
      UI_but_drawflag_disable(but, UI_BUT_ICON_LEFT);
    }
    else if (cell_value.value_float2.has_value()) {
      const float2 value = *cell_value.value_float2;
      this->draw_float_vector(params, Span(&value.x, 2));
    }
    else if (cell_value.value_float3.has_value()) {
      const float3 value = *cell_value.value_float3;
      this->draw_float_vector(params, Span(&value.x, 3));
    }
    else if (cell_value.value_color.has_value()) {
      const Color4f value = *cell_value.value_color;
      this->draw_float_vector(params, Span(&value.r, 4));
    }
    else if (cell_value.value_object.has_value()) {
      const ObjectCellValue value = *cell_value.value_object;
      uiDefIconTextBut(params.block,
                       UI_BTYPE_LABEL,
                       0,
                       ICON_OBJECT_DATA,
                       reinterpret_cast<const ID *const>(value.object)->name + 2,
                       params.xmin,
                       params.ymin,
                       params.width,
                       params.height,
                       nullptr,
                       0,
                       0,
                       0,
                       0,
                       nullptr);
    }
    else if (cell_value.value_collection.has_value()) {
      const CollectionCellValue value = *cell_value.value_collection;
      uiDefIconTextBut(params.block,
                       UI_BTYPE_LABEL,
                       0,
                       ICON_OUTLINER_COLLECTION,
                       reinterpret_cast<const ID *const>(value.collection)->name + 2,
                       params.xmin,
                       params.ymin,
                       params.width,
                       params.height,
                       nullptr,
                       0,
                       0,
                       0,
                       0,
                       nullptr);
    }
  }

  void draw_float_vector(const CellDrawParams &params, const Span<float> values) const
  {
    BLI_assert(!values.is_empty());
    const float segment_width = (float)params.width / values.size();
    for (const int i : values.index_range()) {
      std::stringstream ss;
      const float value = values[i];
      ss << std::fixed << std::setprecision(3) << value;
      const std::string value_str = ss.str();
      uiBut *but = uiDefIconTextBut(params.block,
                                    UI_BTYPE_LABEL,
                                    0,
                                    ICON_NONE,
                                    value_str.c_str(),
                                    params.xmin + i * segment_width,
                                    params.ymin,
                                    segment_width,
                                    params.height,
                                    nullptr,
                                    0,
                                    0,
                                    0,
                                    0,
                                    nullptr);
      /* Right-align Floats. */
      UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
      UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
    }
  }

  int column_width(int column_index) const final
  {
    return spreadsheet_layout_.columns[column_index].width;
  }
};

std::unique_ptr<SpreadsheetDrawer> spreadsheet_drawer_from_layout(
    const SpreadsheetLayout &spreadsheet_layout)
{
  return std::make_unique<SpreadsheetLayoutDrawer>(spreadsheet_layout);
}

}  // namespace blender::ed::spreadsheet
