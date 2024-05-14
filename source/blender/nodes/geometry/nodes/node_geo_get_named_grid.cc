/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"

#include "RNA_enum_types.hh"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_get_named_grid_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Volume");
  b.add_input<decl::String>("Name");
  b.add_input<decl::Bool>("Remove").default_value(true);

  b.add_output<decl::Geometry>("Volume");

  const bNode *node = b.node_or_null();
  if (!node) {

    return;
  }

  b.add_output(eNodeSocketDatatype(node->custom1), "Grid");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = SOCK_FLOAT;
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const bNode &node = params.node();
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Volume");
  const std::string grid_name = params.extract_input<std::string>("Name");
  const bool remove_grid = params.extract_input<bool>("Remove");
  const VolumeGridType grid_type = *bke::socket_type_to_grid_type(
      eNodeSocketDatatype(node.custom1));

  if (Volume *volume = geometry_set.get_volume_for_write()) {
    if (const bke::VolumeGridData *grid = BKE_volume_grid_find(volume, grid_name.c_str())) {
      /* Increment user count before removing from volume. */
      grid->add_user();
      if (remove_grid) {
        BKE_volume_grid_remove(volume, grid);
      }

      params.set_output("Grid", bke::GVolumeGrid(grid));
      params.set_output("Volume", geometry_set);
      return;
    }
  }

  params.set_output("Grid", bke::GVolumeGrid(grid_type));
  params.set_output("Volume", geometry_set);
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "Type of grid data",
                    rna_enum_node_socket_data_type_items,
                    NOD_inline_enum_accessors(custom1),
                    SOCK_FLOAT,
                    grid_socket_type_items_filter_fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_GET_NAMED_GRID, "Get Named Grid", NODE_CLASS_GEOMETRY);

  ntype.declare = node_declare;
  ntype.gather_link_search_ops = search_link_ops_for_volume_grid_node;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_get_named_grid_cc
