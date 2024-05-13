/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/Composite.h>
#endif

#include "BKE_volume_grid.hh"

#include "GEO_volume_grid_resample.hh"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_sdf_grid_boolean_cc {

enum class Operation {
  Intersect = 0,
  Union = 1,
  Difference = 2,
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Grid 1").hide_value();
  b.add_input<decl::Float>("Grid 2").hide_value().multi_input().make_available(
      [](bNode &node) { node.custom1 = int16_t(Operation::Difference); });
  b.add_output<decl::Float>("Grid").hide_value();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *grid_1_socket = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *grid_2_socket = grid_1_socket->next;
  switch (Operation(node->custom1)) {
    case Operation::Intersect:
    case Operation::Union:
      bke::nodeSetSocketAvailability(ntree, grid_1_socket, false);
      node_sock_label(grid_2_socket, "Grid");
      break;
    case Operation::Difference:
      bke::nodeSetSocketAvailability(ntree, grid_1_socket, true);
      node_sock_label(grid_2_socket, "Grid 2");
      break;
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int16_t(Operation::Difference);
}

#ifdef WITH_OPENVDB
static void get_float_grids(MutableSpan<SocketValueVariant> values,
                            Vector<bke::VolumeGrid<float>> &grids)
{
  for (SocketValueVariant &input : values) {
    if (auto grid = input.extract<bke::VolumeGrid<float>>()) {
      grids.append(std::move(grid));
    }
  }
}
#endif

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const Operation operation = Operation(params.node().custom1);

  Vector<SocketValueVariant> inputs = params.extract_input<Vector<SocketValueVariant>>("Grid 2");
  Vector<bke::VolumeGrid<float>> operands;
  switch (operation) {
    case Operation::Intersect:
    case Operation::Union:
      get_float_grids(inputs, operands);
      break;
    case Operation::Difference:
      if (auto grid = params.extract_input<bke::VolumeGrid<float>>("Grid 1")) {
        operands.append(std::move(grid));
      }
      get_float_grids(inputs, operands);
      break;
  }

  if (operands.is_empty()) {
    params.set_default_remaining_outputs();
    return;
  }

  bke::VolumeTreeAccessToken result_token;
  openvdb::FloatGrid &result_grid = operands.first().grid_for_write(result_token);
  const openvdb::math::Transform &transform = result_grid.transform();

  for (bke::VolumeGrid<float> &volume_grid : operands.as_mutable_span().drop_front(1)) {
    bke::VolumeTreeAccessToken tree_token;
    std::shared_ptr<openvdb::FloatGrid> resampled_storage;
    openvdb::FloatGrid &grid = geometry::resample_sdf_grid_if_necessary(
        volume_grid, tree_token, transform, resampled_storage);

    try {
      switch (operation) {
        case Operation::Intersect:
          openvdb::tools::csgIntersection(result_grid, grid);
          break;
        case Operation::Union:
          openvdb::tools::csgUnion(result_grid, grid);
          break;
        case Operation::Difference:
          openvdb::tools::csgDifference(result_grid, grid);
          break;
      }
    }
    catch (const openvdb::ValueError & /*ex*/) {
      /* May happen if a grid is empty. */
      params.set_default_remaining_outputs();
      return;
    }
  }

  params.set_output("Grid", std::move(operands.first()));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem operation_items[] = {
      {int(Operation::Intersect),
       "INTERSECT",
       0,
       "Intersect",
       "Keep the part of the grids that is common between all operands"},
      {int(Operation::Union), "UNION", 0, "Union", "Combine grids in an additive way"},
      {int(Operation::Difference),
       "DIFFERENCE",
       0,
       "Difference",
       "Combine grids in a subtractive way"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "operation",
                    "Operation",
                    "",
                    operation_items,
                    NOD_inline_enum_accessors(custom1),
                    int(Operation::Difference));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_SDF_GRID_BOOLEAN, "SDF Grid Boolean", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  ntype.updatefunc = node_update;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.gather_link_search_ops = search_link_ops_for_volume_grid_node;
  blender::bke::nodeRegisterType(&ntype);
  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_sdf_grid_boolean_cc
