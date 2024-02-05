/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "IMB_colormanagement.hh"

namespace blender::nodes::node_shader_volume_principled_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.5f, 0.5f, 0.5f, 1.0f});
#define SOCK_COLOR_ID 0
  b.add_input<decl::String>("Color Attribute");
#define SOCK_COLOR_ATTR_ID 1
  b.add_input<decl::Float>("Density").default_value(1.0f).min(0.0f).max(1000.0f);
#define SOCK_DENSITY_ID 2
  b.add_input<decl::String>("Density Attribute").default_value("density");
#define SOCK_DENSITY_ATTR_ID 3
  b.add_input<decl::Float>("Anisotropy")
      .default_value(0.0f)
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
#define SOCK_ANISOTROPY_ID 4
  b.add_input<decl::Color>("Absorption Color").default_value({0.0f, 0.0f, 0.0f, 1.0f});
#define SOCK_ABSORPTION_COLOR_ID 5
  b.add_input<decl::Float>("Emission Strength").default_value(0.0f).min(0.0f).max(1000.0f);
#define SOCK_EMISSION_ID 6
  b.add_input<decl::Color>("Emission Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
#define SOCK_EMISSION_COLOR_ID 7
  b.add_input<decl::Float>("Blackbody Intensity")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
#define SOCK_BLACKBODY_INTENSITY_ID 8
  b.add_input<decl::Color>("Blackbody Tint").default_value({1.0f, 1.0f, 1.0f, 1.0f});
#define SOCK_BLACKBODY_TINT_ID 8
  b.add_input<decl::Float>("Temperature").default_value(1000.0f).min(0.0f).max(6500.0f);
  b.add_input<decl::String>("Temperature Attribute").default_value("temperature");
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("Volume").translation_context(BLT_I18NCONTEXT_ID_ID);
}

static void attribute_post_process(GPUMaterial *mat,
                                   const char *attribute_name,
                                   GPUNodeLink **attribute_link)
{
  if (STREQ(attribute_name, "color")) {
    GPU_link(mat, "node_attribute_color", *attribute_link, attribute_link);
  }
  else if (STREQ(attribute_name, "temperature")) {
    GPU_link(mat, "node_attribute_temperature", *attribute_link, attribute_link);
  }
}

static int node_shader_gpu_volume_principled(GPUMaterial *mat,
                                             bNode *node,
                                             bNodeExecData * /*execdata*/,
                                             GPUNodeStack *in,
                                             GPUNodeStack *out)
{
  /* Test if blackbody intensity is enabled. */
  bool use_blackbody = node_socket_not_zero(in[SOCK_BLACKBODY_INTENSITY_ID]);

  if (node_socket_not_zero(in[SOCK_DENSITY_ID]) && node_socket_not_black(in[SOCK_COLOR_ID])) {
    /* Consider there is absorption phenomenon when there is scattering since
     * `extinction = scattering + absorption`. */
    GPU_material_flag_set(mat, GPU_MATFLAG_VOLUME_SCATTER | GPU_MATFLAG_VOLUME_ABSORPTION);
  }
  if (node_socket_not_zero(in[SOCK_DENSITY_ID]) &&
      node_socket_not_white(in[SOCK_ABSORPTION_COLOR_ID]))
  {
    GPU_material_flag_set(mat, GPU_MATFLAG_VOLUME_ABSORPTION);
  }

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
      density = GPU_attribute_with_default(mat, CD_AUTO_FROM_NAME, attribute_name, GPU_DEFAULT_1);
      attribute_post_process(mat, attribute_name, &density);
    }
    else if (STREQ(sock->name, "Color Attribute")) {
      color = GPU_attribute_with_default(mat, CD_AUTO_FROM_NAME, attribute_name, GPU_DEFAULT_1);
      attribute_post_process(mat, attribute_name, &color);
    }
    else if (use_blackbody && STREQ(sock->name, "Temperature Attribute")) {
      temperature = GPU_attribute(mat, CD_AUTO_FROM_NAME, attribute_name);
      attribute_post_process(mat, attribute_name, &temperature);
    }
  }

  /* Default values if attributes not found. */
  static float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  if (!density) {
    density = GPU_constant(white);
  }
  if (!color) {
    color = GPU_constant(white);
  }
  if (!temperature) {
    temperature = GPU_constant(white);
  }

  /* Create blackbody spectrum. */
  const int size = CM_TABLE + 1;
  float *data, layer;
  if (use_blackbody) {
    data = (float *)MEM_mallocN(sizeof(float) * size * 4, "blackbody texture");
    IMB_colormanagement_blackbody_temperature_to_rgb_table(data, size, 800.0f, 12000.0f);
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

#undef SOCK_COLOR_ID
#undef SOCK_COLOR_ATTR_ID
#undef SOCK_DENSITY_ID
#undef SOCK_DENSITY_ATTR_ID
#undef SOCK_ANISOTROPY_ID
#undef SOCK_ABSORPTION_COLOR_ID
#undef SOCK_EMISSION_ID
#undef SOCK_EMISSION_COLOR_ID
#undef SOCK_BLACKBODY_INTENSITY_ID
#undef SOCK_BLACKBODY_TINT_ID

}  // namespace blender::nodes::node_shader_volume_principled_cc

/* node type definition */
void register_node_type_sh_volume_principled()
{
  namespace file_ns = blender::nodes::node_shader_volume_principled_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_VOLUME_PRINCIPLED, "Principled Volume", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::LARGE);
  ntype.gpu_fn = file_ns::node_shader_gpu_volume_principled;

  nodeRegisterType(&ntype);
}
