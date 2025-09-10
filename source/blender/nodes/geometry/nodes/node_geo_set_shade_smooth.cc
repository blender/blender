/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"

#include "RNA_enum_types.hh"

#include "GEO_foreach_geometry.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_shade_smooth_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  b.add_input<decl::Geometry>("Mesh", "Geometry")
      .supported_type(GeometryComponent::Type::Mesh)
      .description("Geometry to set the smoothness of");
  b.add_output<decl::Geometry>("Mesh", "Geometry").propagate_all().align_with_previous();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Bool>("Shade Smooth").default_value(true).field_on_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "domain", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int16_t(AttrDomain::Face);
}

/**
 * When the `sharp_face` attribute doesn't exist, all faces are considered smooth. If all faces
 * are selected and the sharp value is a constant false value, we can remove the attribute instead
 * as an optimization to avoid storing it and propagating it in the future.
 */
static bool try_removing_sharp_attribute(Mesh &mesh,
                                         const StringRef name,
                                         const Field<bool> &selection,
                                         const Field<bool> &sharpness)
{
  if (selection.node().depends_on_input() || sharpness.node().depends_on_input()) {
    return false;
  }
  if (!fn::evaluate_constant_field(selection)) {
    return true;
  }
  if (fn::evaluate_constant_field(sharpness)) {
    return false;
  }
  mesh.attributes_for_write().remove(name);
  return true;
}

static void set_sharp(Mesh &mesh,
                      const AttrDomain domain,
                      const StringRef name,
                      const Field<bool> &selection,
                      const Field<bool> &sharpness)
{
  const int domain_size = mesh.attributes().domain_size(domain);
  if (domain_size == 0) {
    return;
  }
  if (try_removing_sharp_attribute(mesh, name, selection, sharpness)) {
    return;
  }
  bke::try_capture_field_on_geometry(mesh.attributes_for_write(),
                                     bke::MeshFieldContext(mesh, domain),
                                     name,
                                     domain,
                                     selection,
                                     sharpness);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const AttrDomain domain = AttrDomain(params.node().custom1);
  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  const Field<bool> smooth_field = params.extract_input<Field<bool>>("Shade Smooth");

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
      set_sharp(*mesh,
                domain,
                domain == AttrDomain::Face ? "sharp_face" : "sharp_edge",
                selection,
                fn::invert_boolean_field(smooth_field));
    }
  });
  params.set_output("Geometry", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "",
                    rna_enum_attribute_domain_edge_face_items,
                    NOD_inline_enum_accessors(custom1));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSetShadeSmooth", GEO_NODE_SET_SHADE_SMOOTH);
  ntype.ui_name = "Set Shade Smooth";
  ntype.ui_description =
      "Control the smoothness of mesh normals around each face by changing the \"shade smooth\" "
      "attribute";
  ntype.enum_name_legacy = "SET_SHADE_SMOOTH";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_shade_smooth_cc
