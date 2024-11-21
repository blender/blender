/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BLI_string.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BKE_node_runtime.hh"

namespace blender::nodes::node_shader_volume_scatter_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.8f, 0.8f, 0.8f, 1.0f});
#define SOCK_COLOR_ID 0
  b.add_input<decl::Float>("Density").default_value(1.0f).min(0.0f).max(1000.0f);
#define SOCK_DENSITY_ID 1
  b.add_input<decl::Float>("Anisotropy")
      .default_value(0.0f)
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Directionality of the scattering. Zero is isotropic, negative is backward, "
          "positive is forward");
  b.add_input<decl::Float>("IOR")
      .default_value(1.33f)
      .min(1.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR)
      .description("Index Of Refraction of the scattering particles");
  b.add_input<decl::Float>("Backscatter")
      .default_value(0.1f)
      .min(0.0f)
      .max(0.5f)
      .subtype(PROP_FACTOR)
      .description("Fraction of light that is scattered backwards");
  b.add_input<decl::Float>("Alpha").default_value(0.5f).min(0.0f).max(500.0f);
  b.add_input<decl::Float>("Diameter")
      .default_value(20.0f)
      .min(5.0f)
      .max(50.0f)
      .description("Diameter of the water droplets, in micrometers");
  b.add_input<decl::Float>("Weight").available(false);
  b.add_output<decl::Shader>("Volume").translation_context(BLT_I18NCONTEXT_ID_ID);
}

static void node_shader_buts_scatter(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "phase", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_scatter(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_PHASE_HENYEY_GREENSTEIN;
}

static void node_shader_update_scatter(bNodeTree *ntree, bNode *node)
{
  const int phase_function = node->custom1;

  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (STR_ELEM(sock->name, "IOR", "Backscatter")) {
      bke::node_set_socket_availability(ntree, sock, phase_function == SHD_PHASE_FOURNIER_FORAND);
    }
    else if (STR_ELEM(sock->name, "Anisotropy")) {
      bke::node_set_socket_availability(
          ntree, sock, ELEM(phase_function, SHD_PHASE_HENYEY_GREENSTEIN, SHD_PHASE_DRAINE));
    }
    else if (STR_ELEM(sock->name, "Alpha")) {
      bke::node_set_socket_availability(ntree, sock, phase_function == SHD_PHASE_DRAINE);
    }
    else if (STR_ELEM(sock->name, "Diameter")) {
      bke::node_set_socket_availability(ntree, sock, phase_function == SHD_PHASE_MIE);
    }
  }
}

static int node_shader_gpu_volume_scatter(GPUMaterial *mat,
                                          bNode *node,
                                          bNodeExecData * /*execdata*/,
                                          GPUNodeStack *in,
                                          GPUNodeStack *out)
{
  if (node_socket_not_zero(in[SOCK_DENSITY_ID]) && node_socket_not_black(in[SOCK_COLOR_ID])) {
    /* Consider there is absorption phenomenon when there is scattering since
     * `extinction = scattering + absorption`. */
    GPU_material_flag_set(mat, GPU_MATFLAG_VOLUME_SCATTER | GPU_MATFLAG_VOLUME_ABSORPTION);
  }
  return GPU_stack_link(mat, node, "node_volume_scatter", in, out);
}

#undef SOCK_COLOR_ID
#undef SOCK_DENSITY_ID

}  // namespace blender::nodes::node_shader_volume_scatter_cc

/* node type definition */
void register_node_type_sh_volume_scatter()
{
  namespace file_ns = blender::nodes::node_shader_volume_scatter_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_VOLUME_SCATTER, "Volume Scatter", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_shader_nodes_poll;
  ntype.draw_buttons = file_ns::node_shader_buts_scatter;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.initfunc = file_ns::node_shader_init_scatter;
  ntype.gpu_fn = file_ns::node_shader_gpu_volume_scatter;
  ntype.updatefunc = file_ns::node_shader_update_scatter;

  blender::bke::node_register_type(&ntype);
}
