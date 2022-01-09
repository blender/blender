/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

#include "node_shader_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_bsdf_principled_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Base Color")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Float>(N_("Subsurface"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Vector>(N_("Subsurface Radius"))
      .default_value({1.0f, 0.2f, 0.1f})
      .min(0.0f)
      .max(100.0f)
      .compact();
  b.add_input<decl::Color>(N_("Subsurface Color")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Float>(N_("Subsurface IOR"))
      .default_value(1.4f)
      .min(1.01f)
      .max(3.8f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Subsurface Anisotropy"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Metallic"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Specular"))
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Specular Tint"))
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Roughness"))
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Anisotropic"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Anisotropic Rotation"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Sheen"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Sheen Tint"))
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Clearcoat"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Clearcoat Roughness"))
      .default_value(0.03f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("IOR")).default_value(1.45f).min(0.0f).max(1000.0f);
  b.add_input<decl::Float>(N_("Transmission"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Transmission Roughness"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Color>(N_("Emission")).default_value({0.0f, 0.0f, 0.0f, 1.0f});
  b.add_input<decl::Float>(N_("Emission Strength")).default_value(1.0).min(0.0f).max(1000000.0f);
  b.add_input<decl::Float>(N_("Alpha"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Vector>(N_("Normal")).hide_value();
  b.add_input<decl::Vector>(N_("Clearcoat Normal")).hide_value();
  b.add_input<decl::Vector>(N_("Tangent")).hide_value();
  b.add_output<decl::Shader>(N_("BSDF"));
}

static void node_shader_buts_principled(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribution", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "subsurface_method", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_principled(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = SHD_GLOSSY_GGX;
  node->custom2 = SHD_SUBSURFACE_RANDOM_WALK;
}

#define socket_not_zero(sock) (in[sock].link || (clamp_f(in[sock].vec[0], 0.0f, 1.0f) > 1e-5f))
#define socket_not_one(sock) \
  (in[sock].link || (clamp_f(in[sock].vec[0], 0.0f, 1.0f) < 1.0f - 1e-5f))

static int node_shader_gpu_bsdf_principled(GPUMaterial *mat,
                                           bNode *node,
                                           bNodeExecData *UNUSED(execdata),
                                           GPUNodeStack *in,
                                           GPUNodeStack *out)
{
  GPUNodeLink *sss_scale;

  /* Normals */
  if (!in[22].link) {
    GPU_link(mat, "world_normals_get", &in[22].link);
  }

  /* Clearcoat Normals */
  if (!in[23].link) {
    GPU_link(mat, "world_normals_get", &in[23].link);
  }

#if 0 /* Not used at the moment. */
  /* Tangents */
  if (!in[24].link) {
    GPUNodeLink *orco = GPU_attribute(CD_ORCO, "");
    GPU_link(mat, "tangent_orco_z", orco, &in[24].link);
    GPU_link(mat,
             "node_tangent",
             GPU_builtin(GPU_WORLD_NORMAL),
             in[24].link,
             GPU_builtin(GPU_OBJECT_MATRIX),
             &in[24].link);
  }
#endif

  bool use_diffuse = socket_not_one(6) && socket_not_one(17);
  bool use_subsurf = socket_not_zero(1) && use_diffuse && node->sss_id > 0;
  bool use_refract = socket_not_one(6) && socket_not_zero(17);
  bool use_clear = socket_not_zero(14);

  /* SSS Profile */
  if (use_subsurf) {
    bNodeSocket *socket = (bNodeSocket *)BLI_findlink(&node->original->inputs, 2);
    bNodeSocketValueRGBA *socket_data = (bNodeSocketValueRGBA *)socket->default_value;
    /* For some reason it seems that the socket value is in ARGB format. */
    GPU_material_sss_profile_create(mat, &socket_data->value[1]);
  }

  if (in[2].link) {
    sss_scale = in[2].link;
  }
  else {
    GPU_link(mat, "set_rgb_one", &sss_scale);
  }

  uint flag = GPU_MATFLAG_GLOSSY;
  if (use_diffuse) {
    flag |= GPU_MATFLAG_DIFFUSE;
  }
  if (use_refract) {
    flag |= GPU_MATFLAG_REFRACT;
  }
  if (use_subsurf) {
    flag |= GPU_MATFLAG_SSS;
  }

  float f_use_diffuse = use_diffuse ? 1.0f : 0.0f;
  float f_use_clearcoat = use_clear ? 1.0f : 0.0f;
  float f_use_refraction = use_refract ? 1.0f : 0.0f;
  float use_multi_scatter = (node->custom1 == SHD_GLOSSY_MULTI_GGX) ? 1.0f : 0.0f;

  GPU_material_flag_set(mat, (eGPUMatFlag)flag);

  return GPU_stack_link(mat,
                        node,
                        "node_bsdf_principled",
                        in,
                        out,
                        GPU_constant(&f_use_diffuse),
                        GPU_constant(&f_use_clearcoat),
                        GPU_constant(&f_use_refraction),
                        GPU_constant(&use_multi_scatter),
                        GPU_constant(&node->ssr_id),
                        GPU_constant(&node->sss_id),
                        sss_scale);
}

static void node_shader_update_principled(bNodeTree *ntree, bNode *node)
{
  const int distribution = node->custom1;
  const int sss_method = node->custom2;

  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (STREQ(sock->name, "Transmission Roughness")) {
      nodeSetSocketAvailability(ntree, sock, distribution == SHD_GLOSSY_GGX);
    }

    if (STR_ELEM(sock->name, "Subsurface IOR", "Subsurface Anisotropy")) {
      nodeSetSocketAvailability(ntree, sock, sss_method != SHD_SUBSURFACE_BURLEY);
    }
  }
}

}  // namespace blender::nodes::node_shader_bsdf_principled_cc

/* node type definition */
void register_node_type_sh_bsdf_principled()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_principled_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_PRINCIPLED, "Principled BSDF", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_principled;
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);
  node_type_init(&ntype, file_ns::node_shader_init_principled);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_bsdf_principled);
  node_type_update(&ntype, file_ns::node_shader_update_principled);

  nodeRegisterType(&ntype);
}
