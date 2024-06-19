/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_numbers.hh"

#include "BKE_texture.h"

#include "node_shader_util.hh"
#include "node_util.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_tex_gabor_cc {

NODE_STORAGE_FUNCS(NodeTexGabor)

static void sh_node_tex_gabor_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector")
      .implicit_field(implicit_field_inputs::position)
      .description(
          "The coordinates at which Gabor noise will be evaluated. The Z component is ignored in "
          "the 2D case");
  b.add_input<decl::Float>("Scale").default_value(5.0f).description(
      "The scale of the Gabor noise");
  b.add_input<decl::Float>("Frequency")
      .default_value(2.0f)
      .min(0.0f)
      .description(
          "The rate at which the Gabor noise changes across space. This is different from the "
          "Scale input in that it only scales perpendicular to the Gabor noise direction");
  b.add_input<decl::Float>("Anisotropy")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "The directionality of Gabor noise. 1 means the noise is completely directional, while "
          "0 means the noise is omnidirectional");
  b.add_input<decl::Float>("Orientation", "Orientation 2D")
      .default_value(math::numbers::pi / 4)
      .subtype(PROP_ANGLE)
      .description("The direction of the anisotropic Gabor noise");
  b.add_input<decl::Vector>("Orientation", "Orientation 3D")
      .default_value({math::numbers::sqrt2, math::numbers::sqrt2, 0.0f})
      .subtype(PROP_DIRECTION)
      .description("The direction of the anisotropic Gabor noise");
  b.add_output<decl::Float>("Value").description(
      "The Gabor noise value with both random intensity and phase. This is equal to sine the "
      "phase multiplied by the intensity");
  b.add_output<decl::Float>("Phase").description(
      "The phase of the Gabor noise, which has no random intensity");
  b.add_output<decl::Float>("Intensity")
      .description("The intensity of the Gabor noise, which has no random phase");
}

static void node_shader_buts_tex_gabor(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "gabor_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_tex_gabor(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexGabor *storage = MEM_cnew<NodeTexGabor>(__func__);
  BKE_texture_mapping_default(&storage->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&storage->base.color_mapping);

  storage->type = SHD_GABOR_TYPE_2D;

  node->storage = storage;
}

static void node_shader_update_tex_gabor(bNodeTree *ntree, bNode *node)
{
  const NodeTexGabor &storage = node_storage(*node);

  bNodeSocket *orientation_2d_socket = bke::nodeFindSocket(node, SOCK_IN, "Orientation 2D");
  bke::nodeSetSocketAvailability(ntree, orientation_2d_socket, storage.type == SHD_GABOR_TYPE_2D);

  bNodeSocket *orientation_3d_socket = bke::nodeFindSocket(node, SOCK_IN, "Orientation 3D");
  bke::nodeSetSocketAvailability(ntree, orientation_3d_socket, storage.type == SHD_GABOR_TYPE_3D);
}

static int node_shader_gpu_tex_gabor(GPUMaterial *material,
                                     bNode *node,
                                     bNodeExecData * /* execdata */,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(material, node, &in[0].link);
  node_shader_gpu_tex_mapping(material, node, in, out);

  const float type = float(node_storage(*node).type);
  return GPU_stack_link(material, node, "node_tex_gabor", in, out, GPU_constant(&type));
}

}  // namespace blender::nodes::node_shader_tex_gabor_cc

void register_node_type_sh_tex_gabor()
{
  namespace file_ns = blender::nodes::node_shader_tex_gabor_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_GABOR, "Gabor Texture", NODE_CLASS_TEXTURE);
  ntype.declare = file_ns::sh_node_tex_gabor_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_gabor;
  ntype.initfunc = file_ns::node_shader_init_tex_gabor;
  node_type_storage(
      &ntype, "NodeTexGabor", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_gabor;
  ntype.updatefunc = file_ns::node_shader_update_tex_gabor;

  nodeRegisterType(&ntype);
}
