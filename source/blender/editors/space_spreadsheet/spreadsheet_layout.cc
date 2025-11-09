/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <iomanip>
#include <sstream>

#include <fmt/format.h>

#include "BLF_api.hh"

#include "BLI_color.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"

#include "BKE_instances.hh"

#include "NOD_geometry_nodes_bundle.hh"

#include "spreadsheet_column_values.hh"
#include "spreadsheet_data_source_geometry.hh"
#include "spreadsheet_layout.hh"

#include "DNA_meshdata_types.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

/* Need to do our own padding in some cases because we use low-level ui code to draw the
 * spreadsheet. */
#define CELL_PADDING_X (0.15f * SPREADSHEET_WIDTH_UNIT)

namespace blender::ed::spreadsheet {

static std::string format_matrix_to_grid(const float4x4 &matrix)
{
  auto format_element = [](float value) {
    if (math::abs(value) < 1e-4f) {
      return fmt::format("{:.3}", value);
    }
    return fmt::format("{:.6}", value);
  };

  /* Transpose to be able to print row by row. */
  const float4x4 t_matrix = math::transpose(matrix);
  std::array<std::array<std::string, 4>, 4> formatted_elements;
  std::array<size_t, 4> column_widths = {};
  for (const int row_i : IndexRange(4)) {
    for (const int col_i : IndexRange(4)) {
      formatted_elements[row_i][col_i] = format_element(t_matrix[row_i][col_i]);
      column_widths[col_i] = std::max(column_widths[col_i],
                                      formatted_elements[row_i][col_i].length());
    }
  }

  fmt::memory_buffer buf;
  for (const int row_i : IndexRange(4)) {
    for (const int col_i : IndexRange(4)) {
      fmt::format_to(
          fmt::appender(buf), "{:>{}}", formatted_elements[row_i][col_i], column_widths[col_i]);
      if (col_i < 3) {
        fmt::format_to(fmt::appender(buf), "  ");
      }
    }
    if (row_i < 3) {
      fmt::format_to(fmt::appender(buf), "\n");
    }
  }
  return fmt::to_string(buf);
}

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
                                  ButType::Label,
                                  0,
                                  ICON_NONE,
                                  name,
                                  params.xmin,
                                  params.ymin,
                                  params.width,
                                  params.height,
                                  nullptr,
                                  std::nullopt);
    UI_but_func_tooltip_set(
        but,
        [](bContext * /*C*/, void *arg, blender::StringRef /*tip*/) {
          return *static_cast<std::string *>(arg);
        },
        MEM_new<std::string>(__func__, name),
        [](void *arg) { MEM_delete(static_cast<std::string *>(arg)); });
    /* Center-align column headers. */
    UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
    UI_but_drawflag_disable(but, UI_BUT_TEXT_RIGHT);
  }

  void draw_left_column_cell(int row_index, const CellDrawParams &params) const final
  {
    const int real_index = spreadsheet_layout_.row_indices[row_index];
    std::string index_str = std::to_string(real_index);
    uiBut *but = uiDefIconTextBut(params.block,
                                  ButType::Label,
                                  0,
                                  ICON_NONE,
                                  index_str,
                                  params.xmin,
                                  params.ymin,
                                  params.width,
                                  params.height,
                                  nullptr,
                                  std::nullopt);
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

    const GVArray &data = column.data();
    const CPPType &type = data.type();
    BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);
    data.get_to_uninitialized(real_index, buffer);
    this->draw_content_cell_value(GPointer(type, buffer), params, column);
    type.destruct(buffer);
  }

  void draw_content_cell_value(const GPointer value_ptr,
                               const CellDrawParams &params,
                               const ColumnValues &column) const
  {
    const CPPType &type = *value_ptr.type();
    if (type.is<int>()) {
      this->draw_int(params, *value_ptr.get<int>(), column.display_hint());
      return;
    }
    if (type.is<int64_t>()) {
      this->draw_int(params, *value_ptr.get<int64_t>(), column.display_hint());
      return;
    }
    if (type.is<int8_t>()) {
      const int8_t value = *value_ptr.get<int8_t>();
      const std::string value_str = std::to_string(value);
      uiBut *but = uiDefIconTextBut(params.block,
                                    ButType::Label,
                                    0,
                                    ICON_NONE,
                                    value_str,
                                    params.xmin,
                                    params.ymin,
                                    params.width,
                                    params.height,
                                    nullptr,
                                    std::nullopt);
      /* Right-align Integers. */
      UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
      UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
      return;
    }
    if (type.is<short2>()) {
      const int2 value = int2(*value_ptr.get<short2>());
      this->draw_int_vector(params, Span(&value.x, 2));
      return;
    }
    if (type.is<int2>()) {
      const int2 value = *value_ptr.get<int2>();
      this->draw_int_vector(params, Span(&value.x, 2));
      return;
    }
    if (type.is<int3>()) {
      const int3 value = *value_ptr.get<int3>();
      this->draw_int_vector(params, Span(&value.x, 3));
      return;
    }
    if (type.is<float>()) {
      const float value = *value_ptr.get<float>();
      std::stringstream ss;
      ss << std::fixed << std::setprecision(3) << value;
      const std::string value_str = ss.str();
      uiBut *but = uiDefIconTextBut(params.block,
                                    ButType::Label,
                                    0,
                                    ICON_NONE,
                                    value_str,
                                    params.xmin,
                                    params.ymin,
                                    params.width,
                                    params.height,
                                    nullptr,
                                    std::nullopt);
      UI_but_func_tooltip_set(
          but,
          [](bContext * /*C*/, void *argN, const StringRef /*tip*/) {
            return fmt::format("{:f}", *((float *)argN));
          },
          MEM_dupallocN<float>(__func__, value),
          MEM_freeN);
      /* Right-align Floats. */
      UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
      UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
      return;
    }
    if (type.is<bool>()) {
      const bool value = *value_ptr.get<bool>();
      const int icon = value ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
      uiBut *but = uiDefIconTextBut(params.block,
                                    ButType::Label,
                                    0,
                                    icon,
                                    "",
                                    params.xmin,
                                    params.ymin,
                                    params.width,
                                    params.height,
                                    nullptr,
                                    std::nullopt);
      UI_but_drawflag_disable(but, UI_BUT_ICON_LEFT);
      return;
    }
    if (type.is<float2>()) {
      const float2 value = *value_ptr.get<float2>();
      this->draw_float_vector(params, Span(&value.x, 2));
      return;
    }
    if (type.is<float3>()) {
      const float3 value = *value_ptr.get<float3>();
      this->draw_float_vector(params, Span(&value.x, 3));
      return;
    }
    if (type.is<ColorGeometry4f>()) {
      const ColorGeometry4f value = *value_ptr.get<ColorGeometry4f>();
      this->draw_float_vector(params, Span(&value.r, 4));
      return;
    }
    if (type.is<ColorGeometry4b>()) {
      const ColorGeometry4b value = *value_ptr.get<ColorGeometry4b>();
      this->draw_byte_color(params, value);
      return;
    }
    if (type.is<math::Quaternion>()) {
      const float4 value = float4(*value_ptr.get<math::Quaternion>());
      this->draw_float_vector(params, Span(&value.x, 4));
      return;
    }
    if (type.is<float4x4>()) {
      this->draw_float4x4(params, *value_ptr.get<float4x4>());
      return;
    }
    if (type.is<bke::InstanceReference>()) {
      const bke::InstanceReference value = *value_ptr.get<bke::InstanceReference>();
      const StringRefNull name = value.name().is_empty() ? IFACE_("(Geometry)") : value.name();
      const int icon = get_instance_reference_icon(value);
      uiDefIconTextBut(params.block,
                       ButType::Label,
                       0,
                       icon,
                       name.c_str(),
                       params.xmin,
                       params.ymin,
                       params.width,
                       params.height,
                       nullptr,
                       std::nullopt);
      return;
    }
    if (type.is<std::string>()) {
      uiDefIconTextBut(params.block,
                       ButType::Label,
                       0,
                       ICON_NONE,
                       *value_ptr.get<std::string>(),
                       params.xmin + CELL_PADDING_X,
                       params.ymin,
                       params.width - 2.0f * CELL_PADDING_X,
                       params.height,
                       nullptr,
                       std::nullopt);
      return;
    }
    if (type.is<MStringProperty>()) {
      MStringProperty *prop = MEM_callocN<MStringProperty>(__func__);
      *prop = *value_ptr.get<MStringProperty>();
      uiBut *but = uiDefIconTextBut(params.block,
                                    ButType::Label,
                                    0,
                                    ICON_NONE,
                                    StringRef(prop->s, prop->s_len),
                                    params.xmin + CELL_PADDING_X,
                                    params.ymin,
                                    params.width - 2.0f * CELL_PADDING_X,
                                    params.height,
                                    nullptr,
                                    std::nullopt);

      UI_but_func_tooltip_set(
          but,
          [](bContext * /*C*/, void *argN, const StringRef /*tip*/) {
            const MStringProperty &prop = *static_cast<MStringProperty *>(argN);
            return std::string(StringRef(prop.s, prop.s_len));
          },
          prop,
          MEM_freeN);
      return;
    }
    if (type.is<nodes::BundleItemValue>()) {
      const nodes::BundleItemValue &value = *value_ptr.get<nodes::BundleItemValue>();
      if (const nodes::BundleItemSocketValue *socket_value =
              std::get_if<nodes::BundleItemSocketValue>(&value.value))
      {
        const bke::SocketValueVariant &value_variant = socket_value->value;
        if (value_variant.is_single()) {
          const GPointer single_value_ptr = value_variant.get_single_ptr();
          this->draw_content_cell_value(single_value_ptr, params, column);
          return;
        }
      }
      this->draw_undrawable(params);
      return;
    }
    this->draw_undrawable(params);
  }

  void draw_float_vector(const CellDrawParams &params, const Span<float> values) const
  {
    BLI_assert(!values.is_empty());
    const float segment_width = float(params.width) / values.size();
    for (const int i : values.index_range()) {
      std::stringstream ss;
      const float value = values[i];
      ss << " " << std::fixed << std::setprecision(3) << value;
      const std::string value_str = ss.str();
      uiBut *but = uiDefIconTextBut(params.block,
                                    ButType::Label,
                                    0,
                                    ICON_NONE,
                                    value_str,
                                    params.xmin + i * segment_width,
                                    params.ymin,
                                    segment_width,
                                    params.height,
                                    nullptr,
                                    std::nullopt);

      UI_but_func_tooltip_set(
          but,
          [](bContext * /*C*/, void *argN, const StringRef /*tip*/) {
            return fmt::format("{:f}", *((float *)argN));
          },
          MEM_dupallocN<float>(__func__, value),
          MEM_freeN);
      /* Right-align Floats. */
      UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
      UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
    }
  }

  void draw_int(const CellDrawParams &params,
                const int64_t value,
                const ColumnValueDisplayHint display_hint) const
  {
    std::string value_str;
    switch (display_hint) {
      case ColumnValueDisplayHint::Bytes: {
        char dst[BLI_STR_FORMAT_INT64_BYTE_UNIT_SIZE];
        BLI_str_format_byte_unit(dst, value, true);
        value_str = dst;
        break;
      }
      default: {
        char dst[BLI_STR_FORMAT_INT64_GROUPED_SIZE];
        BLI_str_format_int64_grouped(dst, value);
        value_str = dst;
        break;
      }
    }
    uiBut *but = uiDefIconTextBut(params.block,
                                  ButType::Label,
                                  0,
                                  ICON_NONE,
                                  value_str,
                                  params.xmin,
                                  params.ymin,
                                  params.width,
                                  params.height,
                                  nullptr,
                                  std::nullopt);
    switch (display_hint) {
      case ColumnValueDisplayHint::Bytes: {
        UI_but_func_tooltip_set(
            but,
            [](bContext * /*C*/, void *argN, const StringRef /*tip*/) {
              char dst[BLI_STR_FORMAT_INT64_GROUPED_SIZE];
              BLI_str_format_int64_grouped(dst, *(int64_t *)argN);
              return fmt::format("{} {}", dst, TIP_("bytes"));
            },
            MEM_dupallocN<int64_t>(__func__, value),
            MEM_freeN);
        break;
      }
      default: {
        UI_but_func_tooltip_set(
            but,
            [](bContext * /*C*/, void *argN, const StringRef /*tip*/) {
              return fmt::format("{}", *(int64_t *)argN);
            },
            MEM_dupallocN<int64_t>(__func__, value),
            MEM_freeN);
        break;
      }
    }
    /* Right-align Integers. */
    UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
    UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
  }

  void draw_int_vector(const CellDrawParams &params, const Span<int> values) const
  {
    BLI_assert(!values.is_empty());
    const float segment_width = float(params.width) / values.size();
    for (const int i : values.index_range()) {
      std::stringstream ss;
      const int value = values[i];
      ss << " " << value;
      const std::string value_str = ss.str();
      uiBut *but = uiDefIconTextBut(params.block,
                                    ButType::Label,
                                    0,
                                    ICON_NONE,
                                    value_str,
                                    params.xmin + i * segment_width,
                                    params.ymin,
                                    segment_width,
                                    params.height,
                                    nullptr,
                                    std::nullopt);
      UI_but_func_tooltip_set(
          but,
          [](bContext * /*C*/, void *argN, const StringRef /*tip*/) {
            return fmt::format("{}", *((int *)argN));
          },
          MEM_dupallocN<int>(__func__, value),
          MEM_freeN);
      /* Right-align Floats. */
      UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
      UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
    }
  }

  void draw_byte_color(const CellDrawParams &params, const ColorGeometry4b color) const
  {
    const ColorGeometry4f float_color = color::decode(color);
    Span<float> values(&float_color.r, 4);
    const float segment_width = float(params.width) / values.size();
    for (const int i : values.index_range()) {
      std::stringstream ss;
      const float value = values[i];
      ss << " " << std::fixed << std::setprecision(3) << value;
      const std::string value_str = ss.str();
      uiBut *but = uiDefIconTextBut(params.block,
                                    ButType::Label,
                                    0,
                                    ICON_NONE,
                                    value_str,
                                    params.xmin + i * segment_width,
                                    params.ymin,
                                    segment_width,
                                    params.height,
                                    nullptr,
                                    std::nullopt);
      /* Right-align Floats. */
      UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
      UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);

      /* Tooltip showing raw byte values. Encode values in pointer to avoid memory allocation. */
      UI_but_func_tooltip_set(
          but,
          [](bContext * /*C*/, void *argN, const StringRef /*tip*/) {
            const uint32_t uint_color = POINTER_AS_UINT(argN);
            ColorGeometry4b color = *(ColorGeometry4b *)&uint_color;
            return fmt::format(fmt::runtime(TIP_("Byte Color (sRGB encoded):\n{}  {}  {}  {}")),
                               color.r,
                               color.g,
                               color.b,
                               color.a);
          },
          POINTER_FROM_UINT(*(uint32_t *)&color),
          nullptr);
    }
  }

  void draw_float4x4(const CellDrawParams &params, const float4x4 &value) const
  {
    uiBut *but = this->draw_undrawable(params);
    /* Center alignment. */
    UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
    UI_but_func_tooltip_custom_set(
        but,
        [](bContext & /*C*/, uiTooltipData &tip, uiBut * /*but*/, void *argN) {
          const float4x4 matrix = *static_cast<const float4x4 *>(argN);
          UI_tooltip_text_field_add(
              tip, format_matrix_to_grid(matrix), {}, UI_TIP_STYLE_MONO, UI_TIP_LC_VALUE);
        },
        MEM_dupallocN<float4x4>(__func__, value),
        MEM_freeN);
  }

  uiBut *draw_undrawable(const CellDrawParams &params) const
  {
    uiBut *but = uiDefIconTextBut(params.block,
                                  ButType::Label,
                                  0,
                                  ICON_NONE,
                                  "...",
                                  params.xmin,
                                  params.ymin,
                                  params.width,
                                  params.height,
                                  nullptr,
                                  std::nullopt);
    /* Center alignment. */
    UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
    return but;
  }

  int column_width(int column_index) const final
  {
    return spreadsheet_layout_.columns[column_index].width;
  }
};

template<typename T>
static float estimate_max_column_width(const float min_width,
                                       const int fontid,
                                       const std::optional<int64_t> max_sample_size,
                                       const VArray<T> &data,
                                       FunctionRef<std::string(const T &)> to_string)
{
  if (const std::optional<T> value = data.get_if_single()) {
    const std::string str = to_string(*value);
    return std::max(min_width, BLF_width(fontid, str.c_str(), str.size()));
  }
  const int sample_size = max_sample_size.value_or(data.size());
  float width = min_width;
  for (const int i : data.index_range().take_front(sample_size)) {
    const std::string str = to_string(data[i]);
    const float value_width = BLF_width(fontid, str.c_str(), str.size());
    width = std::max(width, value_width);
  }
  return width;
}

float ColumnValues::fit_column_values_width_px(const std::optional<int64_t> &max_sample_size) const
{
  const int fontid = BLF_default();
  BLF_size(fontid, UI_DEFAULT_TEXT_POINTS * UI_SCALE_FAC);

  auto get_min_width = [&](const float min_width) {
    return max_sample_size.has_value() ? min_width : 0.0f;
  };

  const eSpreadsheetColumnValueType column_type = this->type();
  switch (column_type) {
    case SPREADSHEET_VALUE_TYPE_BOOL: {
      return 2.0f * SPREADSHEET_WIDTH_UNIT;
    }
    case SPREADSHEET_VALUE_TYPE_FLOAT4X4: {
      return 2.0f * SPREADSHEET_WIDTH_UNIT;
    }
    case SPREADSHEET_VALUE_TYPE_INT8: {
      return estimate_max_column_width<int8_t>(
          get_min_width(3 * SPREADSHEET_WIDTH_UNIT),
          fontid,
          max_sample_size,
          data_.typed<int8_t>(),
          [](const int value) { return fmt::format("{}", value); });
    }
    case SPREADSHEET_VALUE_TYPE_INT32: {
      return estimate_max_column_width<int>(
          get_min_width(3 * SPREADSHEET_WIDTH_UNIT),
          fontid,
          max_sample_size,
          data_.typed<int>(),
          [](const int value) { return fmt::format("{}", value); });
    }
    case SPREADSHEET_VALUE_TYPE_INT64: {
      return estimate_max_column_width<int64_t>(get_min_width(3 * SPREADSHEET_WIDTH_UNIT),
                                                fontid,
                                                max_sample_size,
                                                data_.typed<int64_t>(),
                                                [](const int64_t value) {
                                                  char dst[BLI_STR_FORMAT_INT64_GROUPED_SIZE];
                                                  BLI_str_format_int64_grouped(dst, value);
                                                  return std::string(dst);
                                                });
    }
    case SPREADSHEET_VALUE_TYPE_FLOAT: {
      return estimate_max_column_width<float>(
          get_min_width(3 * SPREADSHEET_WIDTH_UNIT),
          fontid,
          max_sample_size,
          data_.typed<float>(),
          [](const float value) { return fmt::format("{:.3f}", value); });
    }
    case SPREADSHEET_VALUE_TYPE_INT32_2D: {
      if (data_.type().is<short2>()) {
        return estimate_max_column_width<short2>(
            get_min_width(6 * SPREADSHEET_WIDTH_UNIT),
            fontid,
            max_sample_size,
            data_.typed<short2>(),
            [](const short2 value) { return fmt::format("{}  {}", value.x, value.y); });
      }
      return estimate_max_column_width<int2>(
          get_min_width(6 * SPREADSHEET_WIDTH_UNIT),
          fontid,
          max_sample_size,
          data_.typed<int2>(),
          [](const int2 value) { return fmt::format("{}  {}", value.x, value.y); });
    }
    case SPREADSHEET_VALUE_TYPE_INT32_3D: {
      return estimate_max_column_width<int3>(
          get_min_width(9 * SPREADSHEET_WIDTH_UNIT),
          fontid,
          max_sample_size,
          data_.typed<int3>(),
          [](const int3 value) { return fmt::format("{}  {}  {}", value.x, value.y, value.z); });
    }
    case SPREADSHEET_VALUE_TYPE_FLOAT2: {
      return estimate_max_column_width<float2>(
          get_min_width(6 * SPREADSHEET_WIDTH_UNIT),
          fontid,
          max_sample_size,
          data_.typed<float2>(),
          [](const float2 value) { return fmt::format("{:.3f}  {:.3f}", value.x, value.y); });
    }
    case SPREADSHEET_VALUE_TYPE_FLOAT3: {
      return estimate_max_column_width<float3>(
          get_min_width(9 * SPREADSHEET_WIDTH_UNIT),
          fontid,
          max_sample_size,
          data_.typed<float3>(),
          [](const float3 value) {
            return fmt::format("{:.3f}  {:.3f}  {:.3f}", value.x, value.y, value.z);
          });
    }
    case SPREADSHEET_VALUE_TYPE_COLOR: {
      return estimate_max_column_width<ColorGeometry4f>(
          get_min_width(12 * SPREADSHEET_WIDTH_UNIT),
          fontid,
          max_sample_size,
          data_.typed<ColorGeometry4f>(),
          [](const ColorGeometry4f value) {
            return fmt::format(
                "{:.3f}  {:.3f}  {:.3f}  {:.3f}", value.r, value.g, value.b, value.a);
          });
    }
    case SPREADSHEET_VALUE_TYPE_BYTE_COLOR: {
      return estimate_max_column_width<ColorGeometry4b>(
          get_min_width(12 * SPREADSHEET_WIDTH_UNIT),
          fontid,
          max_sample_size,
          data_.typed<ColorGeometry4b>(),
          [](const ColorGeometry4b value) {
            return fmt::format("{}  {}  {}  {}", value.r, value.g, value.b, value.a);
          });
    }
    case SPREADSHEET_VALUE_TYPE_QUATERNION: {
      return estimate_max_column_width<math::Quaternion>(
          get_min_width(12 * SPREADSHEET_WIDTH_UNIT),
          fontid,
          max_sample_size,
          data_.typed<math::Quaternion>(),
          [](const math::Quaternion value) {
            return fmt::format(
                "{:.3f}  {:.3f}  {:.3f}  {:.3f}", value.x, value.y, value.z, value.w);
          });
    }
    case SPREADSHEET_VALUE_TYPE_INSTANCES: {
      return UI_ICON_SIZE + 0.5f * UI_UNIT_X +
             estimate_max_column_width<bke::InstanceReference>(
                 get_min_width(8 * SPREADSHEET_WIDTH_UNIT),
                 fontid,
                 max_sample_size,
                 data_.typed<bke::InstanceReference>(),
                 [](const bke::InstanceReference &value) {
                   const StringRef name = value.name().is_empty() ? IFACE_("(Geometry)") :
                                                                    value.name();
                   return name;
                 });
    }
    case SPREADSHEET_VALUE_TYPE_STRING: {
      if (data_.type().is<std::string>()) {
        return estimate_max_column_width<std::string>(get_min_width(SPREADSHEET_WIDTH_UNIT),
                                                      fontid,
                                                      max_sample_size,
                                                      data_.typed<std::string>(),
                                                      [](const StringRef value) { return value; });
      }
      if (data_.type().is<MStringProperty>()) {
        return estimate_max_column_width<MStringProperty>(
            get_min_width(SPREADSHEET_WIDTH_UNIT),
            fontid,
            max_sample_size,
            data_.typed<MStringProperty>(),
            [](const MStringProperty &value) { return StringRef(value.s, value.s_len); });
      }
      break;
    }
    case SPREADSHEET_VALUE_TYPE_BUNDLE_ITEM: {
      return 12 * SPREADSHEET_WIDTH_UNIT;
    }
    case SPREADSHEET_VALUE_TYPE_UNKNOWN: {
      break;
    }
  }
  return 2.0f * SPREADSHEET_WIDTH_UNIT;
}

float ColumnValues::fit_column_width_px(const std::optional<int64_t> &max_sample_size) const
{
  const float padding_px = 0.5 * SPREADSHEET_WIDTH_UNIT;
  const float min_width_px = SPREADSHEET_WIDTH_UNIT;

  const float data_width_px = this->fit_column_values_width_px(max_sample_size);

  const int fontid = BLF_default();
  BLF_size(fontid, UI_DEFAULT_TEXT_POINTS * UI_SCALE_FAC);
  const float name_width_px = BLF_width(fontid, name_.data(), name_.size());

  const float width_px = std::max(min_width_px,
                                  padding_px + std::max(data_width_px, name_width_px));
  return width_px;
}

std::unique_ptr<SpreadsheetDrawer> spreadsheet_drawer_from_layout(
    const SpreadsheetLayout &spreadsheet_layout)
{
  return std::make_unique<SpreadsheetLayoutDrawer>(spreadsheet_layout);
}

}  // namespace blender::ed::spreadsheet
