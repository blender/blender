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

#include "UI_interface_layout.hh"
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
  const bNode *node = b.node_or_null();

  auto &first_grid = b.add_input<decl::Float>("Grid 1").hide_value().structure_type(
      StructureType::Grid);

  if (node) {
    static const auto make_available = [](bNode &node) {
      node.custom1 = int16_t(Operation::Difference);
    };
    switch (Operation(node->custom1)) {
      case Operation::Intersect:
      case Operation::Union:
        b.add_input<decl::Float>("Grid", "Grid 2")
            .hide_value()
            .multi_input()
            .make_available(make_available)
            .structure_type(StructureType::Grid);
        break;
      case Operation::Difference:
        b.add_input<decl::Float>("Grid 2")
            .hide_value()
            .multi_input()
            .make_available(make_available)
            .structure_type(StructureType::Grid);
        break;
    }
  }

  b.add_output<decl::Float>("Grid").hide_value().structure_type(StructureType::Grid);

  if (node) {
    switch (Operation(node->custom1)) {
      case Operation::Intersect:
      case Operation::Union:
        first_grid.available(false);
        break;
      case Operation::Difference:
        first_grid.available(true);
        break;
    }
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "operation", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int16_t(Operation::Difference);
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const Operation operation = Operation(params.node().custom1);

  auto grids = params.extract_input<GeoNodesMultiInput<bke::VolumeGrid<float>>>("Grid 2");
  Vector<bke::VolumeGrid<float>> operands;
  switch (operation) {
    case Operation::Intersect:
    case Operation::Union:
      operands.extend(grids.values);
      break;
    case Operation::Difference:
      if (auto grid = params.extract_input<bke::VolumeGrid<float>>("Grid 1")) {
        operands.append(std::move(grid));
      }
      operands.extend(grids.values);
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
  operands.first()->tag_tree_modified();

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
  geo_node_type_base(&ntype, "GeometryNodeSDFGridBoolean", GEO_NODE_SDF_GRID_BOOLEAN);
  ntype.ui_name = "SDF Grid Boolean";
  ntype.ui_description = "Cut, subtract, or join multiple SDF volume grid inputs";
  ntype.enum_name_legacy = "SDF_GRID_BOOLEAN";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_sdf_grid_boolean_cc
