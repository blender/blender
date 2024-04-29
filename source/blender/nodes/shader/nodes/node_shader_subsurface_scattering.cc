/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BLI_string.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BKE_node_runtime.hh"

namespace blender::nodes::node_shader_subsurface_scattering_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Float>("Scale").default_value(0.05f).min(0.0f).max(1000.0f);
  b.add_input<decl::Vector>("Radius").default_value({1.0f, 0.2f, 0.1f}).min(0.0f).max(100.0f);
  b.add_input<decl::Float>("IOR").default_value(1.4f).min(1.01f).max(3.8f).subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Roughness")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Anisotropy")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Vector>("Normal").hide_value();
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("BSSRDF");
}

static void node_shader_buts_subsurface(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "falloff", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_subsurface_scattering(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_SUBSURFACE_RANDOM_WALK;
  node->custom2 = true;
}

static int node_shader_gpu_subsurface_scattering(GPUMaterial *mat,
                                                 bNode *node,
                                                 bNodeExecData * /*execdata*/,
                                                 GPUNodeStack *in,
                                                 GPUNodeStack *out)
{
  if (!in[6].link) {
    GPU_link(mat, "world_normals_get", &in[6].link);
  }

  bNodeSocket *socket = (bNodeSocket *)BLI_findlink(&node->runtime->original->inputs, 2);
  bNodeSocketValueRGBA *socket_data = (bNodeSocketValueRGBA *)socket->default_value;
  /* For some reason it seems that the socket value is in ARGB format. */
  bool use_subsurf = GPU_material_sss_profile_create(mat, &socket_data->value[1]);

  float use_sss = (use_subsurf) ? 1.0f : 0.0f;

  GPU_material_flag_set(mat, GPU_MATFLAG_DIFFUSE | GPU_MATFLAG_SUBSURFACE);

  return GPU_stack_link(mat, node, "node_subsurface_scattering", in, out, GPU_constant(&use_sss));
}

static void node_shader_update_subsurface_scattering(bNodeTree *ntree, bNode *node)
{
  const int sss_method = node->custom1;

  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (STR_ELEM(sock->name, "IOR", "Anisotropy")) {
      bke::nodeSetSocketAvailability(ntree, sock, sss_method != SHD_SUBSURFACE_BURLEY);
    }
    if (STR_ELEM(sock->name, "Roughness")) {
      bke::nodeSetSocketAvailability(ntree, sock, sss_method == SHD_SUBSURFACE_RANDOM_WALK);
    }
  }
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* NOTE: IOR and Subsurface Method isn't supported for this node in MaterialX. */
  if (to_type_ != NodeItem::Type::BSDF) {
    return empty();
  }

  NodeItem color = get_input_value("Color", NodeItem::Type::Color3);
  NodeItem scale = get_input_value("Scale", NodeItem::Type::Float);
  NodeItem radius = get_input_value("Radius", NodeItem::Type::Vector3);
  NodeItem anisotropy = get_input_value("Anisotropy", NodeItem::Type::Float);
  NodeItem normal = get_input_link("Normal", NodeItem::Type::Vector3);

  return create_node("subsurface_bsdf",
                     NodeItem::Type::BSDF,
                     {{"weight", val(1.0f)},
                      {"color", color},
                      {"radius", radius * scale},
                      {"anisotropy", anisotropy},
                      {"normal", normal}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_subsurface_scattering_cc

/* node type definition */
void register_node_type_sh_subsurface_scattering()
{
  namespace file_ns = blender::nodes::node_shader_subsurface_scattering_cc;

  static bNodeType ntype;

  sh_node_type_base(
      &ntype, SH_NODE_SUBSURFACE_SCATTERING, "Subsurface Scattering", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_shader_nodes_poll;
  ntype.draw_buttons = file_ns::node_shader_buts_subsurface;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.initfunc = file_ns::node_shader_init_subsurface_scattering;
  ntype.gpu_fn = file_ns::node_shader_gpu_subsurface_scattering;
  ntype.updatefunc = file_ns::node_shader_update_subsurface_scattering;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  nodeRegisterType(&ntype);
}
