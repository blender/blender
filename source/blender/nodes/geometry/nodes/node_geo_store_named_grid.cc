/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BKE_lib_id.hh"
#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"

#include "RNA_enum_types.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_store_named_grid_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Volume");
  b.add_input<decl::String>("Name");
  b.add_output<decl::Geometry>("Volume");

  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }

  b.add_input(eCustomDataType(node->custom1), "Grid").hide_value();
}

static void search_link_ops(GatherLinkSearchOpParams &params)
{
  if (U.experimental.use_new_volume_nodes) {
    nodes::search_link_ops_for_basic_node(params);
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
}

#ifdef WITH_OPENVDB

static void try_store_grid(GeoNodeExecParams params, Volume &volume)
{
  const std::string grid_name = params.extract_input<std::string>("Name");

  bke::GVolumeGrid grid = params.extract_input<bke::GVolumeGrid>("Grid");
  if (!grid) {
    return;
  }

  if (const bke::VolumeGridData *existing_grid = BKE_volume_grid_find(&volume, grid_name.data())) {
    BKE_volume_grid_remove(&volume, existing_grid);
  }
  grid.get_for_write().set_name(grid_name);
  grid->add_user();
  BKE_volume_grid_add(&volume, grid.get());
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Volume");
  Volume *volume = geometry_set.get_volume_for_write();
  if (!volume) {
    volume = static_cast<Volume *>(BKE_id_new_nomain(ID_VO, "Store Named Grid Output"));
    geometry_set.replace_volume(volume);
  }

  try_store_grid(params, *volume);

  params.set_output("Volume", geometry_set);
}

#else /* WITH_OPENVDB */

static void node_geo_exec(GeoNodeExecParams params)
{
  node_geo_exec_with_missing_openvdb(params);
}

#endif /* WITH_OPENVDB */

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "Type of grid data",
                    rna_enum_attribute_type_items,
                    NOD_inline_enum_accessors(custom1),
                    CD_PROP_FLOAT,
                    grid_custom_data_type_items_filter_fn);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_STORE_NAMED_GRID, "Store Named Grid", NODE_CLASS_GEOMETRY);

  ntype.declare = node_declare;
  ntype.gather_link_search_ops = search_link_ops;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_store_named_grid_cc
