/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "DNA_grease_pencil_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"

#include "GEO_mesh_copy_selection.hh"
#include "GEO_separate_geometry.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_delete_geometry_cc {

NODE_STORAGE_FUNCS(NodeGeometryDeleteGeometry)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Bool>("Selection")
      .default_value(true)
      .hide_value()
      .field_on_all()
      .description("The parts of the geometry to be deleted");
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  const bNode *node = static_cast<bNode *>(ptr->data);
  const NodeGeometryDeleteGeometry &storage = node_storage(*node);
  const AttrDomain domain = AttrDomain(storage.domain);

  uiItemR(layout, ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
  /* Only show the mode when it is relevant. */
  if (ELEM(domain, AttrDomain::Point, AttrDomain::Edge, AttrDomain::Face)) {
    uiItemR(layout, ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryDeleteGeometry *data = MEM_cnew<NodeGeometryDeleteGeometry>(__func__);
  data->domain = int(AttrDomain::Point);
  data->mode = GEO_NODE_DELETE_GEOMETRY_MODE_ALL;

  node->storage = data;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  /* The node's input is a selection of elements that should be deleted, but the code is
   * implemented as a separation operation that copies the selected elements to a new geometry.
   * Invert the selection to avoid the need to keep track of both cases in the code. */
  const Field<bool> selection = fn::invert_boolean_field(
      params.extract_input<Field<bool>>("Selection"));

  const NodeGeometryDeleteGeometry &storage = node_storage(params.node());
  const AttrDomain domain = AttrDomain(storage.domain);
  const GeometryNodeDeleteGeometryMode mode = (GeometryNodeDeleteGeometryMode)storage.mode;

  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info(
      "Geometry");

  if (domain == AttrDomain::Instance) {
    bool is_error;
    geometry::separate_geometry(geometry_set, domain, mode, selection, propagation_info, is_error);
  }
  else {
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      bool is_error;
      /* Invert here because we want to keep the things not in the selection. */
      geometry::separate_geometry(
          geometry_set, domain, mode, selection, propagation_info, is_error);
    });
  }

  params.set_output("Geometry", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem mode_items[] = {
      {GEO_NODE_DELETE_GEOMETRY_MODE_ALL, "ALL", 0, "All", ""},
      {GEO_NODE_DELETE_GEOMETRY_MODE_EDGE_FACE, "EDGE_FACE", 0, "Only Edges & Faces", ""},
      {GEO_NODE_DELETE_GEOMETRY_MODE_ONLY_FACE, "ONLY_FACE", 0, "Only Faces", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "mode",
                    "Mode",
                    "Which parts of the mesh component to delete",
                    mode_items,
                    NOD_storage_enum_accessors(mode),
                    GEO_NODE_DELETE_GEOMETRY_MODE_ALL);

  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "Which domain to delete in",
                    rna_enum_attribute_domain_without_corner_items,
                    NOD_storage_enum_accessors(domain),
                    int(AttrDomain::Point),
                    enums::domain_experimental_grease_pencil_version3_fn);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_DELETE_GEOMETRY, "Delete Geometry", NODE_CLASS_GEOMETRY);

  node_type_storage(&ntype,
                    "NodeGeometryDeleteGeometry",
                    node_free_standard_storage,
                    node_copy_standard_storage);

  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_delete_geometry_cc
