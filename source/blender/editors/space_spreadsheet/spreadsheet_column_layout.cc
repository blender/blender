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

#include "spreadsheet_column_layout.hh"

#include "DNA_userdef_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLF_api.h"

namespace blender::ed::spreadsheet {

class ColumnLayoutDrawer : public SpreadsheetDrawer {
 private:
  const SpreadsheetColumnLayout &column_layout_;
  Vector<int> column_widths_;

 public:
  ColumnLayoutDrawer(const SpreadsheetColumnLayout &column_layout) : column_layout_(column_layout)
  {
    tot_columns = column_layout.columns.size();
    tot_rows = column_layout.row_indices.size();

    const int fontid = UI_style_get()->widget.uifont_id;
    /* Use a consistent font size for the width calculation. */
    BLF_size(fontid, 11 * U.pixelsize, U.dpi);

    /* The width of the index column depends on the maximum row index. */
    left_column_width = std::to_string(std::max(0, column_layout_.tot_rows - 1)).size() *
                            BLF_width(fontid, "0", 1) +
                        UI_UNIT_X * 0.75;

    /* The column widths depend on the column name widths. */
    const int minimum_column_width = 3 * UI_UNIT_X;
    const int header_name_padding = UI_UNIT_X;
    for (const SpreadsheetColumn *column : column_layout_.columns) {
      StringRefNull name = column->name();
      const int name_width = BLF_width(fontid, name.data(), name.size());
      const int width = std::max(name_width + header_name_padding, minimum_column_width);
      column_widths_.append(width);
    }
  }

  void draw_top_row_cell(int column_index, const CellDrawParams &params) const final
  {
    const StringRefNull name = column_layout_.columns[column_index]->name();
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
    const int real_index = column_layout_.row_indices[row_index];
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
    const int real_index = column_layout_.row_indices[row_index];
    const SpreadsheetColumn &column = *column_layout_.columns[column_index];
    CellValue cell_value;
    column.get_value(real_index, cell_value);

    if (std::holds_alternative<int>(cell_value.value)) {
      const int value = *std::get_if<int>(&cell_value.value);
      const std::string value_str = std::to_string(value);
      uiDefIconTextBut(params.block,
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
    }
    else if (std::holds_alternative<float>(cell_value.value)) {
      const float value = *std::get_if<float>(&cell_value.value);
      std::stringstream ss;
      ss << std::fixed << std::setprecision(3) << value;
      const std::string value_str = ss.str();
      uiDefIconTextBut(params.block,
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
    }
    else if (std::holds_alternative<bool>(cell_value.value)) {
      const bool value = *std::get_if<bool>(&cell_value.value);
      const int icon = value ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
      uiDefIconTextBut(params.block,
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
    }
  }

  int column_width(int column_index) const final
  {
    return column_widths_[column_index];
  }
};

std::unique_ptr<SpreadsheetDrawer> spreadsheet_drawer_from_column_layout(
    const SpreadsheetColumnLayout &column_layout)
{
  return std::make_unique<ColumnLayoutDrawer>(column_layout);
}

}  // namespace blender::ed::spreadsheet
