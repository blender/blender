/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_world_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_material.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_tree_update.hh"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "NOD_composite.hh"
#include "NOD_defaults.hh"
#include "NOD_shader.h"

namespace blender::nodes {

void node_tree_shader_default(const bContext *C, Main *bmain, ID *id)
{
  if (GS(id->name) == ID_MA) {
    /* Materials */
    Object *ob = CTX_data_active_object(C);
    Material *ma = reinterpret_cast<Material *>(id);
    Material *ma_default;

    if (ob && ob->type == OB_VOLUME) {
      ma_default = BKE_material_default_volume();
    }
    else {
      ma_default = BKE_material_default_surface();
    }

    ma->nodetree = blender::bke::node_tree_copy_tree(bmain, *ma_default->nodetree);
    ma->nodetree->owner_id = &ma->id;
    for (bNode *node_iter : ma->nodetree->all_nodes()) {
      STRNCPY_UTF8(node_iter->name, DATA_(node_iter->name));
      blender::bke::node_unique_name(*ma->nodetree, *node_iter);
    }

    BKE_ntree_update_after_single_tree_change(*bmain, *ma->nodetree);
  }
  else if (ELEM(GS(id->name), ID_WO, ID_LA)) {
    /* Emission */
    bNode *shader, *output;
    bNodeTree *ntree = blender::bke::node_tree_add_tree_embedded(
        nullptr, id, "Shader Nodetree", ntreeType_Shader->idname);

    if (GS(id->name) == ID_WO) {
      World *world = reinterpret_cast<World *>(id);
      ntree = world->nodetree;

      shader = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_BACKGROUND);
      output = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_OUTPUT_WORLD);
      blender::bke::node_add_link(*ntree,
                                  *shader,
                                  *blender::bke::node_find_socket(*shader, SOCK_OUT, "Background"),
                                  *output,
                                  *blender::bke::node_find_socket(*output, SOCK_IN, "Surface"));

      bNodeSocket *color_sock = blender::bke::node_find_socket(*shader, SOCK_IN, "Color");
      copy_v3_v3((reinterpret_cast<bNodeSocketValueRGBA *>(color_sock->default_value))->value,
                 &world->horr);
    }
    else {
      shader = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_EMISSION);
      output = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_OUTPUT_LIGHT);
      blender::bke::node_add_link(*ntree,
                                  *shader,
                                  *blender::bke::node_find_socket(*shader, SOCK_OUT, "Emission"),
                                  *output,
                                  *blender::bke::node_find_socket(*output, SOCK_IN, "Surface"));
    }

    shader->location[0] = -200.0f;
    shader->location[1] = 100.0f;
    output->location[0] = 200.0f;
    output->location[1] = 100.0f;
    blender::bke::node_set_active(*ntree, *output);
    BKE_ntree_update_after_single_tree_change(*bmain, *ntree);
  }
  else {
    printf("node_tree_shader_default() called on wrong ID type.\n");
    return;
  }
}

void node_tree_composit_default(const bContext *C, Scene *sce)
{
  Main *bmain = CTX_data_main(C);

  /* but lets check it anyway */
  if (sce->compositing_node_group) {
    if (G.debug & G_DEBUG) {
      printf("error in composite initialize\n");
    }
    return;
  }

  sce->compositing_node_group = blender::bke::node_tree_add_tree(
      bmain, DATA_("Compositor Nodes"), ntreeType_Composite->idname);

  node_tree_composit_default_init(C, sce->compositing_node_group);

  BKE_ntree_update_after_single_tree_change(*bmain, *sce->compositing_node_group);
}

void node_tree_composit_default_init(const bContext *C, bNodeTree *ntree)
{
  BLI_assert(ntree != nullptr && ntree->type == NTREE_COMPOSIT);
  BLI_assert(BLI_listbase_count(&ntree->nodes) == 0);

  ntree->tree_interface.add_socket(
      DATA_("Image"), "", "NodeSocketColor", NODE_INTERFACE_SOCKET_INPUT, nullptr);
  ntree->tree_interface.add_socket(
      DATA_("Image"), "", "NodeSocketColor", NODE_INTERFACE_SOCKET_OUTPUT, nullptr);

  bNode *composite = blender::bke::node_add_node(C, *ntree, "NodeGroupOutput");
  composite->location[0] = 200.0f;
  composite->location[1] = 0.0f;

  bNode *in = blender::bke::node_add_static_node(C, *ntree, CMP_NODE_R_LAYERS);
  in->location[0] = -150.0f - in->width;
  in->location[1] = 0.0f;
  blender::bke::node_set_active(*ntree, *in);
  in->flag &= ~NODE_PREVIEW;

  bNode *reroute = blender::bke::node_add_static_node(C, *ntree, NODE_REROUTE);
  reroute->location[0] = 100.0f;
  reroute->location[1] = -35.0f;

  bNode *viewer = blender::bke::node_add_static_node(C, *ntree, CMP_NODE_VIEWER);
  viewer->location[0] = 200.0f;
  viewer->location[1] = -80.0f;

  /* Viewer and Composite nodes are linked to Render Layer's output image socket through a reroute
   * node. */
  blender::bke::node_add_link(*ntree,
                              *in,
                              *reinterpret_cast<bNodeSocket *>(in->outputs.first),
                              *reroute,
                              *reinterpret_cast<bNodeSocket *>(reroute->inputs.first));

  blender::bke::node_add_link(*ntree,
                              *reroute,
                              *reinterpret_cast<bNodeSocket *>(reroute->outputs.first),
                              *composite,
                              *reinterpret_cast<bNodeSocket *>(composite->inputs.first));

  blender::bke::node_add_link(*ntree,
                              *reroute,
                              *reinterpret_cast<bNodeSocket *>(reroute->outputs.first),
                              *viewer,
                              *reinterpret_cast<bNodeSocket *>(viewer->inputs.first));

  BKE_ntree_update_after_single_tree_change(*CTX_data_main(C), *ntree);
}

}  // namespace blender::nodes
