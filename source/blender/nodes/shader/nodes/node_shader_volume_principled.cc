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

namespace blender::nodes::node_shader_volume_principled_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color")).default_value({0.5f, 0.5f, 0.5f, 1.0f});
  b.add_input<decl::String>(N_("Color Attribute"));
  b.add_input<decl::Float>(N_("Density")).default_value(1.0f).min(0.0f).max(1000.0f);
  b.add_input<decl::String>(N_("Density Attribute"));
  b.add_input<decl::Float>(N_("Anisotropy"))
      .default_value(0.0f)
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Color>(N_("Absorption Color")).default_value({0.0f, 0.0f, 0.0f, 1.0f});
  b.add_input<decl::Float>(N_("Emission Strength")).default_value(0.0f).min(0.0f).max(1000.0f);
  b.add_input<decl::Color>(N_("Emission Color")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Blackbody Intensity"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Color>(N_("Blackbody Tint")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Temperature")).default_value(1000.0f).min(0.0f).max(6500.0f);
  b.add_input<decl::String>(N_("Temperature Attribute"));
  b.add_output<decl::Shader>(N_("Volume"));
}

static void node_shader_init_volume_principled(bNodeTree *UNUSED(ntree), bNode *node)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (STREQ(sock->name, "Density Attribute")) {
      strcpy(((bNodeSocketValueString *)sock->default_value)->value, "density");
    }
    else if (STREQ(sock->name, "Temperature Attribute")) {
      strcpy(((bNodeSocketValueString *)sock->default_value)->value, "temperature");
    }
  }
}

static int node_shader_gpu_volume_principled(GPUMaterial *mat,
                                             bNode *node,
                                             bNodeExecData *UNUSED(execdata),
                                             GPUNodeStack *in,
                                             GPUNodeStack *out)
{
  /* Test if blackbody intensity is enabled. */
  bool use_blackbody = (in[8].link || in[8].vec[0] != 0.0f);

  /* Get volume attributes. */
  GPUNodeLink *density = nullptr, *color = nullptr, *temperature = nullptr;

  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (sock->typeinfo->type != SOCK_STRING) {
      continue;
    }

    bNodeSocketValueString *value = (bNodeSocketValueString *)sock->default_value;
    const char *attribute_name = value->value;
    if (attribute_name[0] == '\0') {
      continue;
    }

    if (STREQ(sock->name, "Density Attribute")) {
      density = GPU_volume_grid(mat, attribute_name, GPU_VOLUME_DEFAULT_1);
    }
    else if (STREQ(sock->name, "Color Attribute")) {
      color = GPU_volume_grid(mat, attribute_name, GPU_VOLUME_DEFAULT_1);
    }
    else if (use_blackbody && STREQ(sock->name, "Temperature Attribute")) {
      temperature = GPU_volume_grid(mat, attribute_name, GPU_VOLUME_DEFAULT_0);
    }
  }

  /* Default values if attributes not found. */
  if (!density) {
    static float one = 1.0f;
    density = GPU_constant(&one);
  }
  if (!color) {
    static float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    color = GPU_constant(white);
  }
  if (!temperature) {
    static float one = 1.0f;
    temperature = GPU_constant(&one);
  }

  /* Create blackbody spectrum. */
  const int size = CM_TABLE + 1;
  float *data, layer;
  if (use_blackbody) {
    data = (float *)MEM_mallocN(sizeof(float) * size * 4, "blackbody texture");
    blackbody_temperature_to_rgb_table(data, size, 965.0f, 12000.0f);
  }
  else {
    data = (float *)MEM_callocN(sizeof(float) * size * 4, "blackbody black");
  }
  GPUNodeLink *spectrummap = GPU_color_band(mat, size, data, &layer);

  return GPU_stack_link(mat,
                        node,
                        "node_volume_principled",
                        in,
                        out,
                        density,
                        color,
                        temperature,
                        spectrummap,
                        GPU_constant(&layer));
}

}  // namespace blender::nodes::node_shader_volume_principled_cc

/* node type definition */
void register_node_type_sh_volume_principled()
{
  namespace file_ns = blender::nodes::node_shader_volume_principled_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_VOLUME_PRINCIPLED, "Principled Volume", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);
  node_type_init(&ntype, file_ns::node_shader_init_volume_principled);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_volume_principled);

  nodeRegisterType(&ntype);
}
