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

#include "BLI_math_vec_types.hh"

#include "BKE_geometry_set.hh"

#include "spreadsheet_column_values.hh"
#include "spreadsheet_layout.hh"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
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
    if (real_index > column.size()) {
      return;
    }

    const fn::GVArray &data = column.data();

    if (data.type().is<int>()) {
      const int value = data.get<int>(real_index);
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
    else if (data.type().is<float>()) {
      const float value = data.get<float>(real_index);
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
    else if (data.type().is<bool>()) {
      const bool value = data.get<bool>(real_index);
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
    else if (data.type().is<float2>()) {
      const float2 value = data.get<float2>(real_index);
      this->draw_float_vector(params, Span(&value.x, 2));
    }
    else if (data.type().is<float3>()) {
      const float3 value = data.get<float3>(real_index);
      this->draw_float_vector(params, Span(&value.x, 3));
    }
    else if (data.type().is<ColorGeometry4f>()) {
      const ColorGeometry4f value = data.get<ColorGeometry4f>(real_index);
      this->draw_float_vector(params, Span(&value.r, 4));
    }
    else if (data.type().is<InstanceReference>()) {
      const InstanceReference value = data.get<InstanceReference>(real_index);
      switch (value.type()) {
        case InstanceReference::Type::Object: {
          const Object &object = value.object();
          uiDefIconTextBut(params.block,
                           UI_BTYPE_LABEL,
                           0,
                           ICON_OBJECT_DATA,
                           object.id.name + 2,
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
          break;
        }
        case InstanceReference::Type::Collection: {
          Collection &collection = value.collection();
          uiDefIconTextBut(params.block,
                           UI_BTYPE_LABEL,
                           0,
                           ICON_OUTLINER_COLLECTION,
                           collection.id.name + 2,
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
          break;
        }
        case InstanceReference::Type::GeometrySet: {
          uiDefIconTextBut(params.block,
                           UI_BTYPE_LABEL,
                           0,
                           ICON_MESH_DATA,
                           "Geometry",
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
          break;
        }
        case InstanceReference::Type::None: {
          break;
        }
      }
    }
    else if (data.type().is<std::string>()) {
      uiDefIconTextBut(params.block,
                       UI_BTYPE_LABEL,
                       0,
                       ICON_NONE,
                       data.get<std::string>(real_index).c_str(),
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
