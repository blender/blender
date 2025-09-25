/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"

#include "node_shader_util.hh"
#include "node_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_bsdf_hair_principled_cc {

/* Color, melanin and absorption coefficient default to approximately same brownish hair. */
static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color")
      .default_value({0.017513f, 0.005763f, 0.002059f, 1.0f})
      .description("The RGB color of the strand. Only used in Direct Coloring");
  b.add_input<decl::Float>("Melanin")
      .default_value(0.8f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Hair pigment. Specify its absolute quantity between 0 and 1");
  b.add_input<decl::Float>("Melanin Redness")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Fraction of pheomelanin in melanin, gives yellowish to reddish color, as opposed to "
          "the brownish to black color of eumelanin");
  b.add_input<decl::Color>("Tint")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Additional color used for dyeing the hair");
  b.add_input<decl::Vector>("Absorption Coefficient")
      .default_value({0.245531f, 0.52f, 1.365f})
      .min(0.0f)
      .max(1000.0f)
      .description(
          "Specifies energy absorption per unit length as light passes through the hair. A higher "
          "value leads to a darker color");
  b.add_input<decl::Float>("Aspect Ratio")
      .default_value(0.85f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "The ratio of the minor axis to the major axis of an elliptical cross-section. "
          "Recommended values are 0.8~1 for Asian hair, 0.65~0.9 for Caucasian hair, 0.5~0.65 for "
          "African hair. The major axis is aligned with the curve normal, which is not supported "
          "in particle hair");
  b.add_input<decl::Float>("Roughness")
      .default_value(0.3f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Hair roughness. A low value leads to a metallic look");
  b.add_input<decl::Float>("Radial Roughness")
      .default_value(0.3f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Coat")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Simulate a shiny coat by reducing the roughness to the given factor only for the first "
          "light bounce (diffuse). Range [0, 1] is equivalent to a reduction of [0%, 100%] of the "
          "original roughness");
  b.add_input<decl::Float>("IOR").default_value(1.55f).min(0.0f).max(1000.0f).description(
      "Index of refraction determines how much the ray is bent. At 1.0 rays pass straight through "
      "like in a transparent material; higher values cause larger deflection in angle. Default "
      "value is 1.55 (the IOR of keratin)");
  b.add_input<decl::Float>("Offset")
      .default_value(2.0f * float(M_PI) / 180.0f)
      .min(-M_PI_2)
      .max(M_PI_2)
      .subtype(PROP_ANGLE)
      .description(
          "The tilt angle of the cuticle scales (the outermost part of the hair). They are always "
          "tilted towards the hair root. The value is usually between 2 and 4 for human hair");
  b.add_input<decl::Float>("Random Color")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Vary the melanin concentration for each strand");
  b.add_input<decl::Float>("Random Roughness")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Vary roughness values for each strand");
  b.add_input<decl::Float>("Random").hide_value();
  b.add_input<decl::Float>("Weight").available(false);
  b.add_input<decl::Float>("Reflection", "R lobe")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Optional factor for modulating the first light bounce off the hair surface. The color "
          "of this component is always white. Keep this 1.0 for physical correctness");
  b.add_input<decl::Float>("Transmission", "TT lobe")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Optional factor for modulating the transmission component. Picks up the color of the "
          "pigment inside the hair. Keep this 1.0 for physical correctness");
  b.add_input<decl::Float>("Secondary Reflection", "TRT lobe")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Optional factor for modulating the component which is transmitted into the hair, "
          "reflected off the backside of the hair and then transmitted out of the hair. This "
          "component is oriented approximately around the incoming direction, and picks up the "
          "color of the pigment inside the hair. Keep this 1.0 for physical correctness");
  b.add_output<decl::Shader>("BSDF");
}

static void node_shader_buts_principled_hair(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "model", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  layout->prop(ptr, "parametrization", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

/* Initialize custom properties. */
static void node_shader_init_hair_principled(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderHairPrincipled *data = MEM_callocN<NodeShaderHairPrincipled>(__func__);

  data->model = SHD_PRINCIPLED_HAIR_CHIANG;
  data->parametrization = SHD_PRINCIPLED_HAIR_REFLECTANCE;

  node->storage = data;
}

/* Triggers (in)visibility of some sockets when changing the parametrization or the model. */
static void node_shader_update_hair_principled(bNodeTree *ntree, bNode *node)
{
  NodeShaderHairPrincipled *data = static_cast<NodeShaderHairPrincipled *>(node->storage);

  int parametrization = data->parametrization;
  int model = data->model;

  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (STREQ(sock->name, "Color")) {
      bke::node_set_socket_availability(
          *ntree, *sock, parametrization == SHD_PRINCIPLED_HAIR_REFLECTANCE);
    }
    else if (STREQ(sock->name, "Melanin")) {
      bke::node_set_socket_availability(
          *ntree, *sock, parametrization == SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
    }
    else if (STREQ(sock->name, "Melanin Redness")) {
      bke::node_set_socket_availability(
          *ntree, *sock, parametrization == SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
    }
    else if (STREQ(sock->name, "Tint")) {
      bke::node_set_socket_availability(
          *ntree, *sock, parametrization == SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
    }
    else if (STREQ(sock->name, "Absorption Coefficient")) {
      bke::node_set_socket_availability(
          *ntree, *sock, parametrization == SHD_PRINCIPLED_HAIR_DIRECT_ABSORPTION);
    }
    else if (STREQ(sock->name, "Random Color")) {
      bke::node_set_socket_availability(
          *ntree, *sock, parametrization == SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
    }
    else if (STREQ(sock->name, "Radial Roughness")) {
      bke::node_set_socket_availability(*ntree, *sock, model == SHD_PRINCIPLED_HAIR_CHIANG);
    }
    else if (STREQ(sock->name, "Coat")) {
      bke::node_set_socket_availability(*ntree, *sock, model == SHD_PRINCIPLED_HAIR_CHIANG);
    }
    else if (STREQ(sock->name, "Aspect Ratio")) {
      bke::node_set_socket_availability(*ntree, *sock, model == SHD_PRINCIPLED_HAIR_HUANG);
    }
    else if (STR_ELEM(sock->name, "Reflection", "Transmission", "Secondary Reflection")) {
      bke::node_set_socket_availability(*ntree, *sock, model == SHD_PRINCIPLED_HAIR_HUANG);
    }
  }
}

static int node_shader_gpu_hair_principled(GPUMaterial *mat,
                                           bNode *node,
                                           bNodeExecData * /*execdata*/,
                                           GPUNodeStack *in,
                                           GPUNodeStack *out)
{
  GPU_material_flag_set(mat, GPU_MATFLAG_DIFFUSE);

  return GPU_stack_link(mat, node, "node_bsdf_hair_principled", in, out);
}

}  // namespace blender::nodes::node_shader_bsdf_hair_principled_cc

/* node type definition */
void register_node_type_sh_bsdf_hair_principled()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_hair_principled_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeBsdfHairPrincipled", SH_NODE_BSDF_HAIR_PRINCIPLED);
  ntype.ui_name = "Principled Hair BSDF";
  ntype.ui_description = "Physically-based, easy-to-use shader for rendering hair and fur";
  ntype.enum_name_legacy = "BSDF_HAIR_PRINCIPLED";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_cycles_shader_nodes_poll;
  ntype.draw_buttons = file_ns::node_shader_buts_principled_hair;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Large);
  ntype.initfunc = file_ns::node_shader_init_hair_principled;
  ntype.updatefunc = file_ns::node_shader_update_hair_principled;
  ntype.gpu_fn = file_ns::node_shader_gpu_hair_principled;
  blender::bke::node_type_storage(
      ntype, "NodeShaderHairPrincipled", node_free_standard_storage, node_copy_standard_storage);

  blender::bke::node_register_type(ntype);
}
