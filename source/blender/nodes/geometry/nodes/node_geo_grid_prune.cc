/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_volume_grid.hh"
#include "BKE_volume_grid_process.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

#ifdef WITH_OPENVDB
#  include "openvdb/tools/Prune.h"
#endif

namespace blender::nodes::node_geo_grid_prune_cc {

enum class Mode : int16_t {
  Inactive = 0,
  Threshold = 1,
  SDF = 2,
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);
  b.add_input(data_type, "Grid").hide_value().structure_type(StructureType::Grid);
  b.add_output(data_type, "Grid").structure_type(StructureType::Grid).align_with_previous();
  static EnumPropertyItem mode_items[] = {
      {int(Mode::Inactive),
       "INACTIVE",
       0,
       N_("Inactive"),
       N_("Turn inactive voxels and tiles into inactive background tiles")},
      {int(Mode::Threshold),
       "THRESHOLD",
       0,
       N_("Threshold"),
       N_("Turn regions where all voxels have the same value and active state (within a tolerance "
          "threshold) into inactive background tiles")},
      {int(Mode::SDF),
       "SDF",
       0,
       N_("SDF"),
       N_("Replace inactive tiles with inactive nodes. Faster than tolerance-based pruning, "
          "useful for cases like narrow-band SDF grids with only inside or outside background "
          "values.")},
      {0, nullptr, 0, nullptr, nullptr},
  };
  b.add_input<decl::Menu>("Mode")
      .static_items(mode_items)
      .default_value(MenuValue(Mode::Threshold))
      .structure_type(StructureType::Single)
      .optional_label();
  if (data_type != SOCK_BOOLEAN) {
    auto &threshold = b.add_input(data_type, "Threshold")
                          .structure_type(StructureType::Single)
                          .usage_by_single_menu(int(Mode::Threshold));
    switch (data_type) {
      case SOCK_FLOAT: {
        auto &threshold_typed = static_cast<decl::FloatBuilder &>(threshold);
        threshold_typed.min(0.0f).default_value(0.01f);
        break;
      }
      case SOCK_VECTOR: {
        auto &threshold_typed = static_cast<decl::VectorBuilder &>(threshold);
        threshold_typed.min(0.0f).default_value(float3(0.01f));
        break;
      }
      case SOCK_INT: {
        auto &threshold_typed = static_cast<decl::IntBuilder &>(threshold);
        threshold_typed.min(0).default_value(0);
        break;
      }
      default:
        BLI_assert_unreachable();
    }
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static std::optional<eNodeSocketDatatype> node_type_for_socket_type(const bNodeSocket &socket)
{
  switch (socket.type) {
    case SOCK_FLOAT:
      return SOCK_FLOAT;
    case SOCK_BOOLEAN:
      return SOCK_BOOLEAN;
    case SOCK_INT:
      return SOCK_INT;
    case SOCK_VECTOR:
    case SOCK_RGBA:
      return SOCK_VECTOR;
    default:
      return std::nullopt;
  }
}

static void node_gather_link_search_ops(GatherLinkSearchOpParams &params)
{
  const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(
      params.other_socket());
  if (!data_type) {
    return;
  }
  params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
    bNode &node = params.add_node("GeometryNodeGridPrune");
    node.custom1 = *data_type;
    params.update_and_connect_available_socket(node, "Grid");
  });
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  bke::GVolumeGrid grid = params.extract_input<bke::GVolumeGrid>("Grid");
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }
  bke::VolumeTreeAccessToken tree_token;
  openvdb::GridBase &grid_base = grid.get_for_write().grid_for_write(tree_token);
  switch (params.extract_input<Mode>("Mode")) {
    case Mode::Inactive: {
      bke::volume_grid::prune_inactive(grid_base);
      break;
    }
    case Mode::Threshold: {
      const VolumeGridType grid_type = bke::volume_grid::get_type(grid_base);
      switch (grid_type) {
        case VOLUME_GRID_BOOLEAN: {
          auto &grid = static_cast<openvdb::BoolGrid &>(grid_base);
          openvdb::tools::prune(grid.tree());
          break;
        }
        case VOLUME_GRID_MASK: {
          auto &grid = static_cast<openvdb::MaskGrid &>(grid_base);
          openvdb::tools::prune(grid.tree());
          break;
        }
        case VOLUME_GRID_FLOAT: {
          auto &grid = static_cast<openvdb::FloatGrid &>(grid_base);
          const float threshold = params.extract_input<float>("Threshold");
          openvdb::tools::prune(grid.tree(), threshold);
          break;
        }
        case VOLUME_GRID_INT: {
          auto &grid = static_cast<openvdb::Int32Grid &>(grid_base);
          const int threshold = params.extract_input<int>("Threshold");
          openvdb::tools::prune(grid.tree(), threshold);
          break;
        }
        case VOLUME_GRID_VECTOR_FLOAT: {
          auto &grid = static_cast<openvdb::Vec3fGrid &>(grid_base);
          const float3 threshold = params.extract_input<float3>("Threshold");
          openvdb::tools::prune(grid.tree(),
                                openvdb::Vec3s(threshold.x, threshold.y, threshold.z));
          break;
        }
        case VOLUME_GRID_UNKNOWN:
        case VOLUME_GRID_DOUBLE:
        case VOLUME_GRID_INT64:
        case VOLUME_GRID_VECTOR_DOUBLE:
        case VOLUME_GRID_VECTOR_INT:
        case VOLUME_GRID_POINTS: {
          params.error_message_add(NodeWarningType::Error, "Unsupported grid type");
          break;
        }
      }
      break;
    }
    case Mode::SDF: {
      const VolumeGridType grid_type = bke::volume_grid::get_type(grid_base);
      BKE_volume_grid_type_to_static_type(grid_type, [&](auto type_tag) {
        using GridT = typename decltype(type_tag)::type;
        if constexpr (bke::volume_grid::is_supported_grid_type<GridT>) {
          if constexpr (std::is_scalar_v<typename GridT::ValueType>) {
            GridT &grid = static_cast<GridT &>(grid_base);
            openvdb::tools::pruneLevelSet(grid.tree());
          }
        }
        else {
          BLI_assert_unreachable();
        }
      });
      break;
    }
  }
  params.set_output("Grid", std::move(grid));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = SOCK_FLOAT;
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "Node socket data type",
                    rna_enum_node_socket_data_type_items,
                    NOD_inline_enum_accessors(custom1),
                    SOCK_FLOAT,
                    grid_socket_type_items_filter_fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeGridPrune");
  ntype.ui_name = "Prune Grid";
  ntype.ui_description =
      "Make the storage of a volume grid more efficient by collapsing data into tiles or inner "
      "nodes";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grid_prune_cc
