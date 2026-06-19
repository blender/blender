/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_volume_grid.hh"
#include "BKE_volume_openvdb.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "node_geometry_util.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/Prune.h>
#endif

namespace blender::nodes::node_geo_grid_topology_boolean {

enum class Operation {
  Intersect = 0,
  Union = 1,
  Difference = 2,
};

static const EnumPropertyItem operation_items[] = {
    {int(Operation::Intersect),
     "INTERSECT",
     0,
     "Intersect",
     "Keep voxels and tiles that are active in all grids"},
    {int(Operation::Union),
     "UNION",
     0,
     "Union",
     "Add voxels or tiles that are active in any grid"},
    {int(Operation::Difference),
     "DIFFERENCE",
     0,
     "Difference",
     "Keep active voxels and tiles of the primary grid that are not active in secondary grids"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);

  b.add_default_layout();

  b.add_input<decl::Menu>("Operation"_ustr)
      .default_value(Operation::Intersect)
      .static_items(operation_items)
      .optional_label();

  b.add_input(data_type, "Grid 1"_ustr).hide_value().structure_type(StructureType::Grid);
  b.add_output(data_type, "Grid"_ustr)
      .hide_value()
      .structure_type(StructureType::Grid)
      .align_with_previous();

  b.add_input(data_type, "Grid 2"_ustr)
      .hide_value()
      .multi_input()
      .structure_type(StructureType::Grid);
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

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(params.node().custom1);
  const Operation operation = params.extract_input<Operation>("Operation"_ustr);
  Vector<bke::GVolumeGrid> operands;
  if (auto grid = params.extract_input<bke::GVolumeGrid>("Grid 1"_ustr)) {
    operands.append(std::move(grid));
  }
  const auto grids = params.extract_input<GeoNodesMultiInput<bke::GVolumeGrid>>("Grid 2"_ustr);
  for (const bke::GVolumeGrid &grid : grids.values) {
    if (grid) {
      operands.append(grid);
    }
  }

  if (operands.is_empty()) {
    params.set_default_remaining_outputs();
    return;
  }

  const std::optional<VolumeGridType> grid_type = bke::socket_type_to_grid_type(data_type);
  if (!grid_type) {
    params.set_default_remaining_outputs();
    return;
  }

  BKE_volume_grid_type_to_static_type(
      *grid_type, [&]<std::derived_from<openvdb::GridBase> GridType>() {
        if constexpr (std::is_same_v<GridType, openvdb::BoolGrid> ||
                      std::is_same_v<GridType, openvdb::FloatGrid> ||
                      std::is_same_v<GridType, openvdb::Int32Grid> ||
                      std::is_same_v<GridType, openvdb::Vec3fGrid>)
        {
          bke::VolumeTreeAccessToken result_token;
          GridType &result_grid = static_cast<GridType &>(
              operands.first().get_for_write().grid_for_write(result_token));
          const openvdb::math::Transform &transform = result_grid.transform();

          for (bke::GVolumeGrid &volume_grid : operands.as_mutable_span().drop_front(1)) {
            bke::VolumeTreeAccessToken operand_token;
            const GridType &operand_grid = static_cast<const GridType &>(
                volume_grid.get().grid(operand_token));

            if (operand_grid.transform() != transform) {
              params.error_message_add(NodeWarningType::Warning,
                                       TIP_("Mismatched grid operand transforms"));
            }

            try {
              switch (operation) {
                case Operation::Intersect:
                  result_grid.tree().topologyIntersection(operand_grid.tree());
                  openvdb::tools::pruneInactive(result_grid.tree());
                  break;
                case Operation::Union:
                  result_grid.tree().topologyUnion(operand_grid.tree());
                  break;
                case Operation::Difference:
                  result_grid.tree().topologyDifference(operand_grid.tree());
                  openvdb::tools::pruneInactive(result_grid.tree());
                  break;
              }
            }
            catch (const openvdb::ValueError & /*ex*/) {
              /* May happen if a grid is empty. */
              params.set_default_remaining_outputs();
              return;
            }
          }
          operands.first()->tag_tree_modified();
        }
      });

  params.set_output("Grid"_ustr, std::move(operands.first()));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGridTopologyBoolean"_ustr);
  ntype.ui_name = "Grid Topology Boolean";
  ntype.ui_description = "Combine the topology of multiple grids";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  ntype.default_width = bke::NodeWidth::_180;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grid_topology_boolean
