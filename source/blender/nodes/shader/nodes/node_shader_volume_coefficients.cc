/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"

#include "node_shader_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BKE_node_runtime.hh"

namespace blender::nodes::node_shader_volume_coefficients_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();

  b.add_output<decl::Shader>("Volume").translation_context(BLT_I18NCONTEXT_ID_ID);

  b.add_input<decl::Float>("Weight").available(false);
#define SOCK_WEIGHT_ID 0

  PanelDeclarationBuilder &abs = b.add_panel("Absorption").default_closed(false);
  abs.add_input<decl::Vector>("Absorption Coefficients")
      .default_value({1.0f, 1.0f, 1.0f})
      .min(0.0f)
      .max(1000.0f)
      .description(
          "Probability density per color channel that light is absorbed per unit distance "
          "traveled in the medium");
#define SOCK_ABSORPTION_COEFFICIENTS_ID 1
  PanelDeclarationBuilder &sca = b.add_panel("Scatter").default_closed(false);
  sca.add_layout([](uiLayout *layout, bContext * /*C*/, PointerRNA *ptr) {
    layout->prop(ptr, "phase", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  });
  sca.add_input<decl::Vector>("Scatter Coefficients")
      .default_value({1.0f, 1.0f, 1.0f})
      .min(0.0f)
      .max(1000.0f)
      .description(
          "Probability density per color channel of an out-scattering event occurring per unit "
          "distance");
#define SOCK_SCATTER_COEFFICIENTS_ID 2
  sca.add_input<decl::Float>("Anisotropy")
      .default_value(0.0f)
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Directionality of the scattering. Zero is isotropic, negative is backward, "
          "positive is forward");
#define SOCK_SCATTER_ANISOTROPY_ID 3
  sca.add_input<decl::Float>("IOR")
      .default_value(1.33f)
      .min(1.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .description("Index Of Refraction of the scattering particles");
#define SOCK_SCATTER_IOR_ID 4
  sca.add_input<decl::Float>("Backscatter")
      .default_value(0.1f)
      .min(0.0f)
      .max(0.5f)
      .subtype(PROP_FACTOR)
      .description("Fraction of light that is scattered backwards");
#define SOCK_SCATTER_BACKSCATTER_ID 5
  sca.add_input<decl::Float>("Alpha").default_value(0.5f).min(0.0f).max(500.0f);
#define SOCK_SCATTER_ALPHA_ID 6
  sca.add_input<decl::Float>("Diameter")
      .default_value(20.0f)
      .min(0.0f)
      .max(50.0f)
      .description("Diameter of the water droplets, in micrometers");
#define SOCK_SCATTER_DIAMETER_ID 7
  PanelDeclarationBuilder &emi = b.add_panel("Emission").default_closed(false);
  emi.add_input<decl::Vector>("Emission Coefficients")
      .default_value({0.0f, 0.0f, 0.0f})
      .min(0.0f)
      .max(1000.0f)
      .description("Emitted radiance per color channel that is added to a ray per unit distance");
#define SOCK_EMISSION_COEFFICIENTS_ID 8
}

static void node_shader_init_coefficients(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_PHASE_HENYEY_GREENSTEIN;
}

static void node_shader_update_coefficients(bNodeTree *ntree, bNode *node)
{
  const int phase_function = node->custom1;

  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (STR_ELEM(sock->name, "IOR", "Backscatter")) {
      bke::node_set_socket_availability(
          *ntree, *sock, phase_function == SHD_PHASE_FOURNIER_FORAND);
    }
    else if (STR_ELEM(sock->name, "Anisotropy")) {
      bke::node_set_socket_availability(
          *ntree, *sock, ELEM(phase_function, SHD_PHASE_HENYEY_GREENSTEIN, SHD_PHASE_DRAINE));
    }
    else if (STR_ELEM(sock->name, "Alpha")) {
      bke::node_set_socket_availability(*ntree, *sock, phase_function == SHD_PHASE_DRAINE);
    }
    else if (STR_ELEM(sock->name, "Diameter")) {
      bke::node_set_socket_availability(*ntree, *sock, phase_function == SHD_PHASE_MIE);
    }
  }
}

static int node_shader_gpu_volume_coefficients(GPUMaterial *mat,
                                               bNode *node,
                                               bNodeExecData * /*execdata*/,
                                               GPUNodeStack *in,
                                               GPUNodeStack *out)
{
  if (node_socket_not_black(in[SOCK_SCATTER_COEFFICIENTS_ID])) {
    GPU_material_flag_set(mat, GPU_MATFLAG_VOLUME_SCATTER | GPU_MATFLAG_VOLUME_ABSORPTION);
  }
  if (node_socket_not_black(in[SOCK_ABSORPTION_COEFFICIENTS_ID])) {
    GPU_material_flag_set(mat, GPU_MATFLAG_VOLUME_ABSORPTION);
  }
  return GPU_stack_link(mat, node, "node_volume_coefficients", in, out);
}

#undef SOCK_WEIGHT_ID
#undef SOCK_ABSORPTION_COEFFICIENTS_ID
#undef SOCK_SCATTER_COEFFICIENTS_ID
#undef SOCK_SCATTER_ANISOTROPY_ID
#undef SOCK_SCATTER_IOR_ID
#undef SOCK_SCATTER_BACKSCATTER_ID
#undef SOCK_SCATTER_ALPHA_ID
#undef SOCK_SCATTER_DIAMETER_ID
#undef SOCK_EMISSION_COEFFICIENTS_ID

}  // namespace blender::nodes::node_shader_volume_coefficients_cc

/* node type definition */
void register_node_type_sh_volume_coefficients()
{
  namespace file_ns = blender::nodes::node_shader_volume_coefficients_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeVolumeCoefficients", SH_NODE_VOLUME_COEFFICIENTS);
  ntype.ui_name = "Volume Coefficients";
  ntype.ui_description =
      "Model all three physical processes in a volume, represented by their coefficients";
  ntype.enum_name_legacy = "VOLUME_COEFFICIENTS";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_shader_nodes_poll;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Large);
  ntype.initfunc = file_ns::node_shader_init_coefficients;
  ntype.gpu_fn = file_ns::node_shader_gpu_volume_coefficients;
  ntype.updatefunc = file_ns::node_shader_update_coefficients;

  blender::bke::node_register_type(ntype);
}
