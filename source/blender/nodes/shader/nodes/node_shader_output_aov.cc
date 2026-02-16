/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BLI_hash.h"

#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender {

namespace nodes::node_shader_output_aov_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.0f, 0.0f, 0.0f, 1.0f});
  b.add_input<decl::Float>("Value").default_value(0.0f).min(0.0f).max(1.0f);
}

static BIFIconID aov_icon(const ViewLayer *view_layer, PointerRNA *ptr)
{
  std::string aov_name = RNA_string_get(ptr, "aov_name");
  if (aov_name.empty()) {
    return ICON_RECORD_OFF;
  }

  const ViewLayerAOV *aov = static_cast<const ViewLayerAOV *>(
      BLI_findstring(&view_layer->aovs, aov_name.c_str(), offsetof(ViewLayerAOV, name)));

  if (aov) {
    switch (aov->type) {
      case AOV_TYPE_COLOR:
        return ICON_NODE_SOCKET_RGBA;
      case AOV_TYPE_VALUE:
        return ICON_NODE_SOCKET_FLOAT;
    }
  }

  return ICON_RECORD_OFF;
}

static void node_shader_buts_output_aov(ui::Layout &layout, bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (scene && view_layer) {
    PointerRNA view_layer_rna_ptr = RNA_pointer_create_id_subdata(
        scene->id, RNA_ViewLayer, view_layer);
    layout.prop_search(
        ptr, "aov_name", &view_layer_rna_ptr, "aovs", "", aov_icon(view_layer, ptr));
  }
  else {
    layout.prop(ptr, "aov_name", ui::ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  }
}

static void node_shader_init_output_aov(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderOutputAOV *aov = MEM_new<NodeShaderOutputAOV>("NodeShaderOutputAOV");
  node->storage = aov;
}

static int node_shader_gpu_output_aov(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  GPUNodeLink *outlink;
  NodeShaderOutputAOV *aov = static_cast<NodeShaderOutputAOV *>(node->storage);
  uint hash = BLI_hash_string(aov->name);
  /* WORKAROUND: We don't support int/uint constants for now. So make sure the aliasing works.
   * We cast back to uint in GLSL. */
  BLI_STATIC_ASSERT(sizeof(float) == sizeof(uint),
                    "GPUCodegen: AOV hash needs float and uint to be the same size.");
  GPUNodeLink *hash_link = GPU_constant(reinterpret_cast<float *>(&hash));

  GPU_material_flag_set(mat, GPU_MATFLAG_AOV | GPU_MATFLAG_OBJECT_INFO);
  GPU_stack_link(mat, node, "node_output_aov", in, out, hash_link, &outlink);
  GPU_material_add_output_link_aov(mat, outlink, hash);
  return true;
}

}  // namespace nodes::node_shader_output_aov_cc

/* node type definition */
void register_node_type_sh_output_aov()
{
  namespace file_ns = nodes::node_shader_output_aov_cc;

  static bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeOutputAOV", SH_NODE_OUTPUT_AOV);
  ntype.ui_name = "AOV Output";
  ntype.ui_description =
      "Arbitrary Output Variables.\nProvide custom render passes for arbitrary shader node "
      "outputs";
  ntype.enum_name_legacy = "OUTPUT_AOV";
  ntype.nclass = NODE_CLASS_OUTPUT;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_output_aov;
  ntype.initfunc = file_ns::node_shader_init_output_aov;
  bke::node_type_storage(
      ntype, "NodeShaderOutputAOV", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_output_aov;

  ntype.no_muting = true;

  bke::node_register_type(ntype);
}

}  // namespace blender
