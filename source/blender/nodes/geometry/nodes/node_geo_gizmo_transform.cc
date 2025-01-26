/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "RNA_enum_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_gizmo_transform_cc {

NODE_STORAGE_FUNCS(NodeGeometryTransformGizmo)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Matrix>("Value").multi_input();
  b.add_input<decl::Vector>("Position").subtype(PROP_TRANSLATION);
  b.add_input<decl::Rotation>("Rotation")
      .description(
          "Local rotation of the gizmo. Only used if the local transforms are used in the 3D "
          "view");
  b.add_output<decl::Geometry>("Transform");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryTransformGizmo *storage = MEM_cnew<NodeGeometryTransformGizmo>(__func__);
  storage->flag = (GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_X |
                   GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_Y |
                   GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_Z |
                   GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_X |
                   GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_Y |
                   GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_Z | GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_X |
                   GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_Y | GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_Z);
  node->storage = storage;
}

static void node_layout_ex(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  {
    uiLayout *row = uiLayoutColumnWithHeading(layout, true, IFACE_("Translation"));
    uiItemR(row, ptr, "use_translation_x", UI_ITEM_NONE, IFACE_("X"), ICON_NONE);
    uiItemR(row, ptr, "use_translation_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
    uiItemR(row, ptr, "use_translation_z", UI_ITEM_NONE, IFACE_("Z"), ICON_NONE);
  }
  {
    uiLayout *row = uiLayoutColumnWithHeading(layout, true, IFACE_("Rotation"));
    uiItemR(row, ptr, "use_rotation_x", UI_ITEM_NONE, IFACE_("X"), ICON_NONE);
    uiItemR(row, ptr, "use_rotation_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
    uiItemR(row, ptr, "use_rotation_z", UI_ITEM_NONE, IFACE_("Z"), ICON_NONE);
  }
  {
    uiLayout *row = uiLayoutColumnWithHeading(layout, true, IFACE_("Scale"));
    uiItemR(row, ptr, "use_scale_x", UI_ITEM_NONE, IFACE_("X"), ICON_NONE);
    uiItemR(row, ptr, "use_scale_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
    uiItemR(row, ptr, "use_scale_z", UI_ITEM_NONE, IFACE_("Z"), ICON_NONE);
  }
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeGizmoTransform", GEO_NODE_GIZMO_TRANSFORM);
  ntype.ui_name = "Transform Gizmo";
  ntype.ui_description = "Show a transform gizmo in the viewport";
  ntype.enum_name_legacy = "GIZMO_TRANSFORM";
  ntype.nclass = NODE_CLASS_INTERFACE;
  bke::node_type_storage(&ntype,
                         "NodeGeometryTransformGizmo",
                         node_free_standard_storage,
                         node_copy_standard_storage);
  ntype.declare = node_declare;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.initfunc = node_init;
  bke::node_register_type(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_gizmo_transform_cc
