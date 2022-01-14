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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_vector_transform_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>(N_("Vector"))
      .default_value({0.5f, 0.5f, 0.5f})
      .min(-10000.0f)
      .max(10000.0f);
  b.add_output<decl::Vector>(N_("Vector"));
}

static void node_shader_buts_vect_transform(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout,
          ptr,
          "vector_type",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
          nullptr,
          ICON_NONE);
  uiItemR(layout, ptr, "convert_from", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "convert_to", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_vect_transform(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeShaderVectTransform *vect = MEM_cnew<NodeShaderVectTransform>("NodeShaderVectTransform");

  /* Convert World into Object Space per default */
  vect->convert_to = 1;

  node->storage = vect;
}

static GPUNodeLink *get_gpulink_matrix_from_to(short from, short to)
{
  switch (from) {
    case SHD_VECT_TRANSFORM_SPACE_OBJECT:
      switch (to) {
        case SHD_VECT_TRANSFORM_SPACE_OBJECT:
          return nullptr;
        case SHD_VECT_TRANSFORM_SPACE_WORLD:
          return GPU_builtin(GPU_OBJECT_MATRIX);
        case SHD_VECT_TRANSFORM_SPACE_CAMERA:
          return GPU_builtin(GPU_LOC_TO_VIEW_MATRIX);
      }
      break;
    case SHD_VECT_TRANSFORM_SPACE_WORLD:
      switch (to) {
        case SHD_VECT_TRANSFORM_SPACE_WORLD:
          return nullptr;
        case SHD_VECT_TRANSFORM_SPACE_CAMERA:
          return GPU_builtin(GPU_VIEW_MATRIX);
        case SHD_VECT_TRANSFORM_SPACE_OBJECT:
          return GPU_builtin(GPU_INVERSE_OBJECT_MATRIX);
      }
      break;
    case SHD_VECT_TRANSFORM_SPACE_CAMERA:
      switch (to) {
        case SHD_VECT_TRANSFORM_SPACE_CAMERA:
          return nullptr;
        case SHD_VECT_TRANSFORM_SPACE_WORLD:
          return GPU_builtin(GPU_INVERSE_VIEW_MATRIX);
        case SHD_VECT_TRANSFORM_SPACE_OBJECT:
          return GPU_builtin(GPU_INVERSE_LOC_TO_VIEW_MATRIX);
      }
      break;
  }
  return nullptr;
}
static int gpu_shader_vect_transform(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  struct GPUNodeLink *inputlink;
  struct GPUNodeLink *fromto;

  const char *vtransform = "direction_transform_m4v3";
  const char *ptransform = "point_transform_m4v3";
  const char *func_name = nullptr;

  NodeShaderVectTransform *nodeprop = (NodeShaderVectTransform *)node->storage;

  if (in[0].hasinput) {
    inputlink = in[0].link;
  }
  else {
    inputlink = GPU_constant(in[0].vec);
  }

  fromto = get_gpulink_matrix_from_to(nodeprop->convert_from, nodeprop->convert_to);

  func_name = (nodeprop->type == SHD_VECT_TRANSFORM_TYPE_POINT) ? ptransform : vtransform;
  if (fromto) {
    /* For cycles we have inverted Z */
    /* TODO: pass here the correct matrices */
    if (nodeprop->convert_from == SHD_VECT_TRANSFORM_SPACE_CAMERA &&
        nodeprop->convert_to != SHD_VECT_TRANSFORM_SPACE_CAMERA) {
      GPU_link(mat, "invert_z", inputlink, &inputlink);
    }
    GPU_link(mat, func_name, inputlink, fromto, &out[0].link);
    if (nodeprop->convert_to == SHD_VECT_TRANSFORM_SPACE_CAMERA &&
        nodeprop->convert_from != SHD_VECT_TRANSFORM_SPACE_CAMERA) {
      GPU_link(mat, "invert_z", out[0].link, &out[0].link);
    }
  }
  else {
    GPU_link(mat, "set_rgb", inputlink, &out[0].link);
  }

  if (nodeprop->type == SHD_VECT_TRANSFORM_TYPE_NORMAL) {
    GPU_link(mat, "vector_normalize", out[0].link, &out[0].link);
  }

  return true;
}

}  // namespace blender::nodes::node_shader_vector_transform_cc

void register_node_type_sh_vect_transform()
{
  namespace file_ns = blender::nodes::node_shader_vector_transform_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_VECT_TRANSFORM, "Vector Transform", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_vect_transform;
  node_type_init(&ntype, file_ns::node_shader_init_vect_transform);
  node_type_storage(
      &ntype, "NodeShaderVectTransform", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, file_ns::gpu_shader_vect_transform);

  nodeRegisterType(&ntype);
}
