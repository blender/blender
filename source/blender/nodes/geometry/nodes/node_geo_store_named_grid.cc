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

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_store_named_grid_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  b.add_input<decl::Geometry>("Volume").description("Volume geometry to add a grid to");
  b.add_output<decl::Geometry>("Volume").align_with_previous();
  b.add_input<decl::String>("Name").optional_label().is_volume_grid_name();

  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }

  b.add_input(*bke::grid_type_to_socket_type(VolumeGridType(node->custom1)), "Grid")
      .hide_value()
      .structure_type(StructureType::Grid);
}

static void search_link_ops(GatherLinkSearchOpParams &params)
{
  if (params.other_socket().type == SOCK_GEOMETRY) {
    params.add_item(IFACE_("Volume"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeStoreNamedGrid");
      params.update_and_connect_available_socket(node, "Volume");
    });
  }
  if (params.in_out() == SOCK_IN) {
    if (params.other_socket().type == SOCK_STRING) {
      params.add_item(IFACE_("Name"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeStoreNamedGrid");
        params.update_and_connect_available_socket(node, "Name");
      });
    }
    if (const std::optional<VolumeGridType> data_type = bke::socket_type_to_grid_type(
            eNodeSocketDatatype(params.other_socket().type)))
    {
      params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeStoreNamedGrid");
        node.custom1 = *data_type;
        params.update_and_connect_available_socket(node, "Grid");
      });
    }
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = VOLUME_GRID_FLOAT;
}

#ifdef WITH_OPENVDB

static void try_store_grid(GeoNodeExecParams params, Volume &volume)
{
  const std::string grid_name = params.extract_input<std::string>("Name");

  bke::GVolumeGrid grid = params.extract_input<bke::GVolumeGrid>("Grid");
  if (!grid) {
    return;
  }

  if (const bke::VolumeGridData *existing_grid = BKE_volume_grid_find(&volume, grid_name)) {
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
    volume = BKE_id_new_nomain<Volume>("Store Named Grid Output");
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
                    rna_enum_volume_grid_data_type_items,
                    NOD_inline_enum_accessors(custom1),
                    VOLUME_GRID_FLOAT,
                    grid_data_type_socket_items_filter_fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeStoreNamedGrid", GEO_NODE_STORE_NAMED_GRID);
  ntype.ui_name = "Store Named Grid";
  ntype.ui_description = "Store grid data in a volume geometry with the specified name";
  ntype.enum_name_legacy = "STORE_NAMED_GRID";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = search_link_ops;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_store_named_grid_cc
