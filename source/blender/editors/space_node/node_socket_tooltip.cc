/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>
#include <sstream>

#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_node_enum.hh"
#include "BKE_node_runtime.hh"
#include "BKE_type_conversions.hh"

#include "BLI_math_euler.hh"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_collection_types.h"
#include "DNA_material_types.h"

#include "NOD_geometry_nodes_log.hh"
#include "NOD_menu_value.hh"
#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"

#include "ED_node.hh"

#include "node_intern.hh"

namespace geo_log = blender::nodes::geo_eval_log;

namespace blender::ed::space_node {

class SocketTooltipBuilder {
 private:
  uiTooltipData &tip_data_;
  const bNodeTree &tree_;
  const bNode &node_;
  const bNodeSocket &socket_;
  uiBut *but_ = nullptr;
  bContext &C_;
  int indentation_ = 0;

  enum class TooltipBlockType {
    Label,
    Description,
    Value,
    Python,
  };

  std::optional<TooltipBlockType> last_block_type_;

 public:
  SocketTooltipBuilder(uiTooltipData &tip_data,
                       const bNodeTree &tree,
                       const bNodeSocket &socket,
                       bContext &C,
                       uiBut *but)
      : tip_data_(tip_data),
        tree_(tree),
        node_(socket.owner_node()),
        socket_(socket),
        but_(but),
        C_(C)
  {
  }

  void build()
  {
    const bool is_extend = StringRef(socket_.idname) == "NodeSocketVirtual";
    if (is_extend) {
      this->build_tooltip_extend_socket();
      return;
    }
    if (node_.is_dangling_reroute()) {
      this->build_tooltip_dangling_reroute();
      return;
    }
    if (this->should_show_label()) {
      this->build_tooltip_label();
    }
    this->build_tooltip_description();
    this->build_tooltip_value();
    this->build_python();

    /* Extra padding at the bottom. */
    this->add_space();
  }

 private:
  void build_tooltip_extend_socket()
  {
    this->add_text_field(TIP_("Connect a link to create a new socket."));
  }

  void build_tooltip_dangling_reroute()
  {
    this->add_text_field(TIP_("Dangling reroute nodes are ignored."), UI_TIP_LC_ALERT);
  }

  bool should_show_label()
  {
    if (this->get_socket_description().value_or("").empty()) {
      /* Show label when the description is empty so that the tooltip is never empty. */
      return true;
    }
    if (socket_.is_output()) {
      return false;
    }
    if (socket_.type == SOCK_MENU) {
      return true;
    }
    if (socket_.runtime->declaration && socket_.runtime->declaration->optional_label) {
      return true;
    }
    return false;
  }

  void build_tooltip_label()
  {
    this->start_block(TooltipBlockType::Label);
    if (node_.is_reroute()) {
      this->add_text_field_header(TIP_("Reroute"));
      return;
    }
    const StringRefNull translated_socket_label = node_socket_get_label(&socket_, nullptr);
    this->add_text_field_header(translated_socket_label);
  }

  void build_tooltip_description()
  {
    std::optional<std::string> description_opt = this->get_socket_description();
    if (!description_opt) {
      return;
    }
    std::string description = std::move(*description_opt);
    if (description.empty()) {
      return;
    }
    if (description[description.size() - 1] != '.') {
      description += '.';
    }
    this->start_block(TooltipBlockType::Description);
    this->add_text_field(std::move(description));
  }

  std::optional<std::string> get_socket_description()
  {
    if (socket_.runtime->declaration == nullptr) {
      if (socket_.description[0]) {
        return socket_.description;
      }
      return std::nullopt;
    }
    const nodes::SocketDeclaration &socket_decl = *socket_.runtime->declaration;
    if (!socket_decl.description.empty()) {
      return TIP_(socket_decl.description);
    }
    if (socket_decl.align_with_previous_socket) {
      const Span<nodes::ItemDeclarationPtr> all_items = node_.runtime->declaration->all_items;
      for (const int i : all_items.index_range()) {
        if (&*all_items[i] != &socket_decl) {
          continue;
        }
        if (i == 0) {
          break;
        }
        const nodes::SocketDeclaration *previous_socket_decl =
            dynamic_cast<const nodes::SocketDeclaration *>(all_items[i - 1].get());
        if (!previous_socket_decl) {
          break;
        }
        if (!previous_socket_decl->description.empty()) {
          return TIP_(previous_socket_decl->description);
        }
      }
    }

    return std::nullopt;
  }

  void build_tooltip_value()
  {
    SpaceNode *snode = CTX_wm_space_node(&C_);
    geo_log::ContextualGeoTreeLogs geo_tree_logs;
    if (snode) {
      geo_tree_logs = geo_log::GeoNodesLog::get_contextual_tree_logs(*snode);
    }
    geo_log::GeoTreeLog *geo_tree_log = geo_tree_logs.get_main_tree_log(socket_);
    if (geo_tree_log && this->build_tooltip_value_from_geometry_nodes_log(*geo_tree_log)) {
      return;
    }
    const bool always_show_value = tree_.type == NTREE_GEOMETRY;
    if (node_.is_reroute()) {
      if (always_show_value) {
        this->start_block(TooltipBlockType::Value);
        this->build_tooltip_value_unknown();
      }
      return;
    }
    if (socket_.is_input()) {
      if (this->is_socket_default_value_used()) {
        this->build_tooltip_value_socket_default();
        return;
      }
    }
    if (always_show_value) {
      this->start_block(TooltipBlockType::Value);
      this->build_tooltip_value_unknown();
    }
  }

  void build_tooltip_value_unknown()
  {
    this->add_text_field_mono(TIP_("Value: Unknown (not evaluated)"));
  }

  void build_tooltip_value_socket_default()
  {
    if (socket_.is_multi_input()) {
      this->start_block(TooltipBlockType::Value);
      this->add_text_field_mono(TIP_("Values: None"));
      return;
    }
    const nodes::SocketDeclaration *socket_decl = socket_.runtime->declaration;
    if (socket_decl && socket_decl->input_field_type == nodes::InputSocketFieldType::Implicit) {
      this->start_block(TooltipBlockType::Value);
      build_tooltip_value_implicit_default(socket_decl->default_input_type);
      return;
    }
    if (socket_decl && socket_decl->structure_type == nodes::StructureType::Grid) {
      this->start_block(TooltipBlockType::Value);
      this->build_tooltip_value_and_type_oneline(TIP_("Empty Grid"), TIP_("Volume Grid"));
      return;
    }
    if (socket_.typeinfo->base_cpp_type == nullptr) {
      return;
    }
    const CPPType &cpp_type = *socket_.typeinfo->base_cpp_type;
    BUFFER_FOR_CPP_TYPE_VALUE(cpp_type, socket_value);
    socket_.typeinfo->get_base_cpp_value(socket_.default_value, socket_value);
    BLI_SCOPED_DEFER([&]() { cpp_type.destruct(socket_value); });
    this->start_block(TooltipBlockType::Value);
    this->build_tooltip_value_generic({cpp_type, socket_value});
  }

  [[nodiscard]] bool build_tooltip_value_from_geometry_nodes_log(geo_log::GeoTreeLog &geo_tree_log)
  {
    if (socket_.typeinfo->base_cpp_type == nullptr) {
      return false;
    }
    geo_tree_log.ensure_socket_values();
    if (socket_.is_multi_input()) {
      return this->build_tooltip_last_value_multi_input(geo_tree_log);
    }
    geo_log::ValueLog *value_log = geo_tree_log.find_socket_value_log(socket_);
    if (!value_log) {
      return false;
    }
    this->start_block(TooltipBlockType::Value);
    this->build_tooltip_value_geo_log(*value_log);
    return true;
  }

  bool build_tooltip_last_value_multi_input(geo_log::GeoTreeLog &geo_tree_log)
  {
    const Span<const bNodeLink *> connected_links = socket_.directly_linked_links();

    Vector<std::pair<int, geo_log::ValueLog *>> value_logs;
    bool all_value_logs_missing = true;
    for (const int i : connected_links.index_range()) {
      const bNodeLink &link = *connected_links[i];
      if (!link.is_used()) {
        continue;
      }
      if (!(link.flag & NODE_LINK_VALID)) {
        continue;
      }
      const bNodeSocket &from_socket = *link.fromsock;
      geo_log::ValueLog *value_log = geo_tree_log.find_socket_value_log(from_socket);
      value_logs.append({i, value_log});
      if (value_log) {
        all_value_logs_missing = false;
      }
    }
    if (all_value_logs_missing) {
      return false;
    }

    this->start_block(TooltipBlockType::Value);
    for (const auto &[i, value_log] : value_logs) {
      const int connection_number = i + 1;
      if (i > 0) {
        this->add_space();
      }
      this->add_text_field_mono(fmt::format("{}:", connection_number));
      this->add_space();
      indentation_++;
      BLI_SCOPED_DEFER([&]() { indentation_--; });
      if (value_log) {
        this->build_tooltip_value_geo_log(*value_log);
      }
      else {
        this->build_tooltip_value_unknown();
      }
    }

    return true;
  }

  void build_tooltip_value_geo_log(geo_log::ValueLog &value_log)
  {
    if (const auto *generic_value_log = dynamic_cast<const geo_log::GenericValueLog *>(&value_log))
    {
      this->build_tooltip_value_generic(generic_value_log->value);
    }
    else if (const auto *string_value_log = dynamic_cast<const geo_log::StringLog *>(&value_log)) {
      this->build_tooltip_value_string_log(*string_value_log);
    }
    else if (const auto *field_value_log = dynamic_cast<const geo_log::FieldInfoLog *>(&value_log))
    {
      this->build_tooltip_value_field_log(*field_value_log);
    }
    else if (const auto *geometry_log = dynamic_cast<const geo_log::GeometryInfoLog *>(&value_log))
    {
      this->build_tooltip_value_geometry_log(*geometry_log);
    }
    else if (const auto *grid_log = dynamic_cast<const geo_log::GridInfoLog *>(&value_log)) {
      build_tooltip_value_grid_log(*grid_log);
    }
    else if (const auto *bundle_log = dynamic_cast<const geo_log::BundleValueLog *>(&value_log)) {
      this->build_tooltip_value_bundle_log(*bundle_log);
    }
    else if (const auto *closure_log = dynamic_cast<const geo_log::ClosureValueLog *>(&value_log))
    {
      this->build_tooltip_value_closure_log(*closure_log);
    }
    else if (const auto *list_log = dynamic_cast<const geo_log::ListInfoLog *>(&value_log)) {
      this->build_tooltip_value_list_log(*list_log);
    }
  }

  void build_tooltip_value_and_type_oneline(const StringRef value, const StringRef type)
  {
    this->add_text_field_mono(fmt::format("{}: {}", TIP_("Value"), value));
    this->add_space();
    this->add_text_field_mono(fmt::format("{}: {}", TIP_("Type"), type));
  }

  template<typename T> [[nodiscard]] bool build_tooltip_value_data_block(const GPointer &value)
  {
    const CPPType &type = *value.type();
    if (!type.is<T *>()) {
      return false;
    }
    const T *data = *value.get<T *>();
    std::string value_str;
    if (data) {
      value_str = BKE_id_name(id_cast<const ID &>(*data));
    }
    else {
      value_str = TIP_("None");
    }
    const ID_Type id_type = T::id_type;
    const char *id_type_name = BKE_idtype_idcode_to_name(id_type);

    this->build_tooltip_value_and_type_oneline(value_str, TIP_(id_type_name));
    return true;
  }

  void build_tooltip_value_enum(const nodes::MenuValue menu_item)
  {
    const auto *storage = socket_.default_value_typed<bNodeSocketValueMenu>();
    if (!storage->enum_items || storage->has_conflict()) {
      this->build_tooltip_value_and_type_oneline(TIP_("Unknown"), TIP_("Menu"));
      return;
    }
    const bke::RuntimeNodeEnumItem *enum_item = storage->enum_items->find_item_by_identifier(
        menu_item.value);
    if (!enum_item) {
      return;
    }
    if (!enum_item->description.empty()) {
      this->add_text_field(TIP_(enum_item->description), UI_TIP_LC_VALUE);
      this->add_space();
    }
    this->build_tooltip_value_and_type_oneline(TIP_(enum_item->name), TIP_("Menu"));
  }

  void build_tooltip_value_int(const int value)
  {
    std::string value_str = fmt::format("{}", value);
    this->build_tooltip_value_and_type_oneline(value_str, TIP_("Integer"));
  }

  void build_tooltip_value_float(const float value)
  {
    std::string value_str;
    /* Above that threshold floats can't represent fractions anymore. */
    if (std::abs(value) > (1 << 24)) {
      /* Use higher precision to display correct integer value instead of one that is rounded to
       * fewer significant digits. */
      value_str = fmt::format("{:.10}", value);
    }
    else {
      value_str = fmt::format("{}", value);
    }
    this->build_tooltip_value_and_type_oneline(value_str, TIP_("Float"));
  }

  void build_tooltip_value_float3(const float3 &value)
  {
    const std::string value_str = fmt::format("{} {} {}", value.x, value.y, value.z);
    this->build_tooltip_value_and_type_oneline(value_str, TIP_("3D Float Vector"));
  }

  void build_tooltip_value_color(const ColorGeometry4f &value)
  {
    const std::string value_str = fmt::format(
        "{} {} {} {} ({})", value.r, value.g, value.b, value.a, TIP_("Linear"));
    this->build_tooltip_value_and_type_oneline(value_str, TIP_("Float Color"));
    this->add_space();

    bool is_gamma = false;
    const ColorManagedDisplay *display = nullptr;
    if (but_) {
      is_gamma = UI_but_is_color_gamma(*but_);
      display = UI_but_cm_display_get(*but_);
    }
    UI_tooltip_color_field_add(tip_data_, float4(value), true, is_gamma, display, UI_TIP_LC_VALUE);
  }

  void build_tooltip_value_quaternion(const math::Quaternion &value)
  {
    const math::EulerXYZ euler = math::to_euler(value);
    const std::string value_str = fmt::format("{}" BLI_STR_UTF8_DEGREE_SIGN
                                              " {}" BLI_STR_UTF8_DEGREE_SIGN
                                              " {}" BLI_STR_UTF8_DEGREE_SIGN,
                                              euler.x().degree(),
                                              euler.y().degree(),
                                              euler.z().degree());
    this->build_tooltip_value_and_type_oneline(value_str, TIP_("Rotation"));
  }

  void build_tooltip_value_bool(const bool value)
  {
    std::string value_str = value ? TIP_("True") : TIP_("False");
    this->build_tooltip_value_and_type_oneline(value_str, TIP_("Boolean"));
  }

  void build_tooltip_value_float4x4(const float4x4 &value)
  {
    /* Transpose to be able to print row by row. */
    const float4x4 value_transposed = math::transpose(value);

    std::stringstream ss;
    for (const int row_i : IndexRange(4)) {
      const float4 row = value_transposed[row_i];
      ss << fmt::format("{:7.3} {:7.3} {:7.3} {:7.3}\n", row[0], row[1], row[2], row[3]);
    }

    this->add_text_field_mono(fmt::format("{}:", TIP_("Value")));
    this->add_space();
    this->add_text_field_mono(ss.str());
    this->add_space();
    this->add_text_field_mono(fmt::format("{}: {}", TIP_("Type"), TIP_("4x4 Float Matrix")));
  }

  void build_tooltip_value_generic(const GPointer &value)

  {
    const CPPType &value_type = *value.type();
    if (this->build_tooltip_value_data_block<Object>(value)) {
      return;
    }
    if (this->build_tooltip_value_data_block<Material>(value)) {
      return;
    }
    if (this->build_tooltip_value_data_block<Tex>(value)) {
      return;
    }
    if (this->build_tooltip_value_data_block<Image>(value)) {
      return;
    }
    if (this->build_tooltip_value_data_block<Collection>(value)) {
      return;
    }

    if (socket_.type == SOCK_MENU) {
      if (!value_type.is<nodes::MenuValue>()) {
        this->build_tooltip_value_unknown();
        return;
      }
      const nodes::MenuValue menu_item = *value.get<nodes::MenuValue>();
      this->build_tooltip_value_enum(menu_item);
      return;
    }

    const CPPType &socket_base_cpp_type = *socket_.typeinfo->base_cpp_type;
    const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
    if (value_type != socket_base_cpp_type) {
      if (!conversions.is_convertible(value_type, socket_base_cpp_type)) {
        this->build_tooltip_value_unknown();
        return;
      }
    }
    BUFFER_FOR_CPP_TYPE_VALUE(socket_base_cpp_type, socket_value);
    conversions.convert_to_uninitialized(
        value_type, socket_base_cpp_type, value.get(), socket_value);
    BLI_SCOPED_DEFER([&]() { socket_base_cpp_type.destruct(socket_value); });

    if (socket_base_cpp_type.is<int>()) {
      this->build_tooltip_value_int(*static_cast<int *>(socket_value));
      return;
    }
    if (socket_base_cpp_type.is<float>()) {
      this->build_tooltip_value_float(*static_cast<float *>(socket_value));
      return;
    }
    if (socket_base_cpp_type.is<float3>()) {
      this->build_tooltip_value_float3(*static_cast<float3 *>(socket_value));
      return;
    }
    if (socket_base_cpp_type.is<ColorGeometry4f>()) {
      this->build_tooltip_value_color(*static_cast<ColorGeometry4f *>(socket_value));
      return;
    }
    if (socket_base_cpp_type.is<math::Quaternion>()) {
      this->build_tooltip_value_quaternion(*static_cast<math::Quaternion *>(socket_value));
      return;
    }
    if (socket_base_cpp_type.is<bool>()) {
      this->build_tooltip_value_bool(*static_cast<bool *>(socket_value));
      return;
    }
    if (socket_base_cpp_type.is<float4x4>()) {
      this->build_tooltip_value_float4x4(*static_cast<float4x4 *>(socket_value));
      return;
    }
    this->build_tooltip_value_unknown();
  }

  void build_tooltip_value_string_log(const geo_log::StringLog &value_log)
  {
    std::string value_str = value_log.value;
    if (value_log.truncated) {
      value_str += "...";
    }
    this->build_tooltip_value_and_type_oneline(value_str, TIP_("String"));
  }

  const char *get_field_type_name(const CPPType &base_type)
  {
    if (base_type.is<int>()) {
      return TIP_("Integer Field");
    }
    if (base_type.is<float>()) {
      return TIP_("Float Field");
    }
    if (base_type.is<blender::float3>()) {
      return TIP_("3D Float Vector Field");
    }
    if (base_type.is<bool>()) {
      return TIP_("Boolean Field");
    }
    if (base_type.is<std::string>()) {
      return TIP_("String Field");
    }
    if (base_type.is<blender::ColorGeometry4f>()) {
      return TIP_("Color Field");
    }
    if (base_type.is<math::Quaternion>()) {
      return TIP_("Rotation Field");
    }
    if (base_type.is<blender::float4x4>()) {
      return TIP_("Matrix Field");
    }
    BLI_assert_unreachable();
    return TIP_("Field");
  }

  void build_tooltip_value_field_log(const geo_log::FieldInfoLog &value_log)
  {
    const CPPType &socket_base_cpp_type = *socket_.typeinfo->base_cpp_type;
    const Span<std::string> input_tooltips = value_log.input_tooltips;

    if (input_tooltips.is_empty()) {
      /* Should have been logged as constant value. */
      BLI_assert_unreachable();
      return;
    }

    this->add_text_field_mono(TIP_("Field depending on:"));

    for (const std::string &input_tooltip : input_tooltips) {
      this->add_space();
      this->add_text_field_mono(fmt::format(" \u2022 {}", input_tooltip));
    }

    this->add_space();
    std::string type_str = this->get_field_type_name(socket_base_cpp_type);
    this->add_text_field_mono(fmt::format("{}: {}", TIP_("Type"), type_str));
  }

  std::string count_to_string(const int count)
  {
    char str[BLI_STR_FORMAT_INT32_GROUPED_SIZE];
    BLI_str_format_int_grouped(str, count);
    return std::string(str);
  }

  void build_tooltip_value_geometry_log(const geo_log::GeometryInfoLog &geometry_log)
  {
    Span<bke::GeometryComponent::Type> component_types = geometry_log.component_types;
    if (component_types.is_empty()) {
      this->build_tooltip_value_and_type_oneline(TIP_("None"), TIP_("Geometry Set"));
      return;
    }
    this->add_text_field_mono(TIP_("Geometry components:"));
    for (const bke::GeometryComponent::Type type : component_types) {
      std::string component_str;
      switch (type) {
        case bke::GeometryComponent::Type::Mesh: {
          const geo_log::GeometryInfoLog::MeshInfo &info = *geometry_log.mesh_info;
          component_str = fmt::format(fmt::runtime(TIP_("Mesh: {} vertices, {} edges, {} faces")),
                                      this->count_to_string(info.verts_num),
                                      this->count_to_string(info.edges_num),
                                      this->count_to_string(info.faces_num));
          break;
        }
        case bke::GeometryComponent::Type::PointCloud: {
          const geo_log::GeometryInfoLog::PointCloudInfo &info = *geometry_log.pointcloud_info;
          component_str = fmt::format(fmt::runtime(TIP_("Point Cloud: {} points")),
                                      this->count_to_string(info.points_num));
          break;
        }
        case bke::GeometryComponent::Type::Instance: {
          const geo_log::GeometryInfoLog::InstancesInfo &info = *geometry_log.instances_info;
          component_str = fmt::format(fmt::runtime(TIP_("Instances: {}")),
                                      this->count_to_string(info.instances_num));
          break;
        }
        case bke::GeometryComponent::Type::Volume: {
          const geo_log::GeometryInfoLog::VolumeInfo &info = *geometry_log.volume_info;
          component_str = fmt::format(fmt::runtime(TIP_("Volume: {} grids")),
                                      this->count_to_string(info.grids.size()));
          break;
        }
        case bke::GeometryComponent::Type::Curve: {
          const geo_log::GeometryInfoLog::CurveInfo &info = *geometry_log.curve_info;
          component_str = fmt::format(fmt::runtime(TIP_("Curve: {} points, {} splines")),
                                      this->count_to_string(info.points_num),
                                      this->count_to_string(info.splines_num));
          break;
        }
        case bke::GeometryComponent::Type::GreasePencil: {
          const geo_log::GeometryInfoLog::GreasePencilInfo &info =
              *geometry_log.grease_pencil_info;
          component_str = fmt::format(fmt::runtime(TIP_("Grease Pencil: {} layers")),
                                      this->count_to_string(info.layers_num));
          break;
        }
        case bke::GeometryComponent::Type::Edit: {
          if (geometry_log.edit_data_info.has_value()) {
            const geo_log::GeometryInfoLog::EditDataInfo &info = *geometry_log.edit_data_info;
            component_str = fmt::format(
                fmt::runtime(TIP_("Edit: {}, {}, {}")),
                info.has_deformed_positions ? TIP_("positions") : TIP_("no positions"),
                info.has_deform_matrices ? TIP_("matrices") : TIP_("no matrices"),
                info.gizmo_transforms_num > 0 ? TIP_("gizmos") : TIP_("no gizmos"));
          }
          break;
        }
      }
      if (!component_str.empty()) {
        this->add_space();
        this->add_text_field_mono(fmt::format(" \u2022 {}", component_str));
      }
    }
    this->add_space();
    this->add_text_field_mono(TIP_("Type: Geometry Set"));
  }

  void build_tooltip_value_grid_log(const geo_log::GridInfoLog &grid_log)
  {
    std::string value_str;
    if (grid_log.is_empty) {
      value_str = TIP_("None");
    }
    else {
      value_str = TIP_("Volume Grid");
    }
    this->build_tooltip_value_and_type_oneline(value_str, TIP_("Volume Grid"));
  }

  void build_tooltip_value_bundle_log(const geo_log::BundleValueLog &bundle_log)
  {
    if (bundle_log.items.is_empty()) {
      this->add_text_field_mono(TIP_("Values: None"));
    }
    else {
      this->add_text_field_mono(TIP_("Values:"));
      Vector<geo_log::BundleValueLog::Item> sorted_items = bundle_log.items;
      std::sort(sorted_items.begin(), sorted_items.end(), [](const auto &a, const auto &b) {
        return BLI_strcasecmp_natural(a.key.c_str(), b.key.c_str()) < 0;
      });
      for (const geo_log::BundleValueLog::Item &item : sorted_items) {
        this->add_space();
        std::string type_name;
        if (const bke::bNodeSocketType *const *socket_type =
                std::get_if<const bke::bNodeSocketType *>(&item.type))
        {
          type_name = TIP_((*socket_type)->label);
        }
        else if (const StringRefNull *internal_type_name = std::get_if<StringRefNull>(&item.type))
        {
          type_name = *internal_type_name;
        }
        this->add_text_field_mono(
            fmt::format(fmt::runtime("\u2022 \"{}\" ({})\n"), item.key, type_name));
      }
    }
    this->add_space();
    this->add_text_field_mono(TIP_("Type: Bundle"));
  }

  void build_tooltip_value_closure_log(const geo_log::ClosureValueLog &closure_log)
  {
    if (closure_log.inputs.is_empty() && closure_log.outputs.is_empty()) {
      this->add_text_field_mono(TIP_("Value: None"));
    }
    else {
      if (!closure_log.inputs.is_empty()) {
        this->add_text_field_mono(TIP_("Inputs:"));
        for (const geo_log::ClosureValueLog::Item &item : closure_log.inputs) {
          this->add_space();
          const std::string type_name = TIP_(item.type->label);
          this->add_text_field_mono(
              fmt::format(fmt::runtime("\u2022 \"{}\" ({})\n"), item.key, type_name));
        }
      }
      if (!closure_log.outputs.is_empty()) {
        this->add_space();
        this->add_text_field_mono(TIP_("Outputs:"));
        for (const geo_log::ClosureValueLog::Item &item : closure_log.outputs) {
          this->add_space();
          const std::string type_name = TIP_(item.type->label);
          this->add_text_field_mono(
              fmt::format(fmt::runtime("\u2022 \"{}\" ({})\n"), item.key, type_name));
        }
      }
    }
    this->add_space();
    this->add_text_field_mono(TIP_("Type: Closure"));
  }

  void build_tooltip_value_list_log(const geo_log::ListInfoLog &list_log)
  {
    this->add_text_field_mono(fmt::format("{}: {}", TIP_("Length"), list_log.size));
    this->add_space();
    this->add_text_field_mono(TIP_("Type: List"));
  }

  void build_tooltip_value_implicit_default(const NodeDefaultInputType &type)
  {
    switch (type) {
      case NODE_DEFAULT_INPUT_VALUE: {
        /* Should be handled elsewhere. */
        BLI_assert_unreachable();
        break;
      }
      case NODE_DEFAULT_INPUT_INDEX_FIELD: {
        this->build_tooltip_value_and_type_oneline(TIP_("Index Field"),
                                                   this->get_field_type_name(CPPType::get<int>()));
        break;
      }
      case NODE_DEFAULT_INPUT_ID_INDEX_FIELD: {
        this->build_tooltip_value_and_type_oneline(TIP_("ID or Index Field"),
                                                   this->get_field_type_name(CPPType::get<int>()));
        break;
      }
      case NODE_DEFAULT_INPUT_NORMAL_FIELD: {
        this->build_tooltip_value_and_type_oneline(
            TIP_("Normal Field"), this->get_field_type_name(CPPType::get<float3>()));
        break;
      }
      case NODE_DEFAULT_INPUT_POSITION_FIELD: {
        this->build_tooltip_value_and_type_oneline(
            TIP_("Position Field"), this->get_field_type_name(CPPType::get<float3>()));
        break;
      }
      case NODE_DEFAULT_INPUT_INSTANCE_TRANSFORM_FIELD: {
        this->build_tooltip_value_and_type_oneline(
            TIP_("Instance Transform Field"), this->get_field_type_name(CPPType::get<float4x4>()));
        break;
      }
      case NODE_DEFAULT_INPUT_HANDLE_LEFT_FIELD: {
        this->build_tooltip_value_and_type_oneline(
            TIP_("Left Handle Field"), this->get_field_type_name(CPPType::get<float3>()));
        break;
      }
      case NODE_DEFAULT_INPUT_HANDLE_RIGHT_FIELD:
        this->build_tooltip_value_and_type_oneline(
            TIP_("Right Handle Field"), this->get_field_type_name(CPPType::get<float3>()));
        break;
    }
  }

  bool is_socket_default_value_used()
  {
    BLI_assert(socket_.is_input());
    for (const bNodeLink *link : socket_.directly_linked_links()) {
      if (!link->is_used()) {
        continue;
      }
      const bNodeSocket &from_socket = *link->fromsock;
      const bNode &from_node = from_socket.owner_node();
      if (from_node.is_dangling_reroute()) {
        continue;
      }
      return false;
    }
    return true;
  }

  StringRef get_structure_type_tooltip(const nodes::StructureType &structure_type)
  {
    switch (structure_type) {
      case nodes::StructureType::Single: {
        return TIP_("Single Value");
      }
      case nodes::StructureType::Dynamic: {
        return TIP_("Dynamic");
      }
      case nodes::StructureType::Field: {
        return TIP_("Field");
      }
      case nodes::StructureType::Grid: {
        return TIP_("Volume Grid");
      }
      case nodes::StructureType::List: {
        return TIP_("List");
      }
    }
    BLI_assert_unreachable();
    return "Unknown";
  }

  void build_python()
  {
    if (!(U.flag & USER_TOOLTIPS_PYTHON)) {
      return;
    }
    if (!but_) {
      return;
    }
    UI_tooltip_uibut_python_add(tip_data_, C_, *but_, nullptr);
  }

  void start_block(const TooltipBlockType new_block_type)
  {
    if (last_block_type_.has_value()) {
      this->add_space(2);
    }
    last_block_type_ = new_block_type;
  }

  void add_text_field_header(std::string text)
  {
    UI_tooltip_text_field_add(
        tip_data_, this->indent(text), {}, UI_TIP_STYLE_HEADER, UI_TIP_LC_MAIN);
  }

  void add_text_field(std::string text, const uiTooltipColorID color_id = UI_TIP_LC_NORMAL)
  {
    UI_tooltip_text_field_add(tip_data_, this->indent(text), {}, UI_TIP_STYLE_NORMAL, color_id);
  }

  void add_text_field_mono(std::string text, const uiTooltipColorID color_id = UI_TIP_LC_VALUE)
  {
    UI_tooltip_text_field_add(tip_data_, this->indent(text), {}, UI_TIP_STYLE_MONO, color_id);
  }

  void add_space(const int amount = 1)
  {
    for ([[maybe_unused]] const int i : IndexRange(amount)) {
      UI_tooltip_text_field_add(tip_data_, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
    }
  }

  std::string indent(std::string text)
  {
    if (indentation_ == 0) {
      return text;
    }
    return fmt::format("{: <{}}{}", "", indentation_, text);
  }
};

void build_socket_tooltip(uiTooltipData &tip_data,
                          bContext &C,
                          uiBut *but,
                          const bNodeTree &tree,
                          const bNodeSocket &socket)
{
  SocketTooltipBuilder builder(tip_data, tree, socket, C, but);
  builder.build();
}

}  // namespace blender::ed::space_node
