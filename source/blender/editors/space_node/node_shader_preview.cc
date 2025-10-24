/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edrend
 *
 * This file implements shader node previews which rely on a structure owned by each SpaceNode.
 * We take advantage of the RenderResult available as ImBuf images to store a Render for every
 * viewed nested node tree present in a SpaceNode. The computation is initiated at the moment of
 * drawing nodes overlays. One render is started for the current nodetree, having a ViewLayer
 * associated with each previewed node.
 *
 * We separate the previewed nodes in two categories: the shader ones and the non-shader ones.
 * - for non-shader nodes, we use AOVs(Arbitrary Output Variable) which highly speed up the
 * rendering process by rendering every non-shader node at the same time. They are rendered in the
 * first ViewLayer.
 * - for shader nodes, we render them each in a different ViewLayer, by routing the node to the
 * output of the material in the preview scene.
 *
 * At the moment of drawing, we take the Render of the viewed node tree and extract the ImBuf of
 * the wanted viewlayer/pass for each previewed node.
 */

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_string_utf8.h"

#include "DNA_camera_types.h"
#include "DNA_material_types.h"
#include "DNA_world_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BKE_colortools.hh"
#include "BKE_compute_context_cache.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"

#include "DEG_depsgraph.hh"

#include "IMB_imbuf.hh"

#include "WM_api.hh"

#include "ED_node_preview.hh"
#include "ED_render.hh"
#include "ED_screen.hh"

#include "node_intern.hh"

namespace blender::ed::space_node {
/* -------------------------------------------------------------------- */
/** \name Local Structs
 * \{ */
using NodeSocketPair = std::pair<bNode *, bNodeSocket *>;

struct ShaderNodesPreviewJob {
  NestedTreePreviews *tree_previews;
  Scene *scene;
  /* Pointer to the job's stop variable which is used to know when the job is asked for finishing.
   * The idea is that the renderer will read this value frequently and abort the render if it is
   * true. */
  bool *stop;
  /* Pointer to the job's update variable which is set to true to refresh the UI when the renderer
   * is delivering a fresh result. It allows the job to give some UI refresh tags to the WM. */
  bool *do_update;

  Material *mat_copy;
  ePreviewType preview_type;
  bNode *mat_output_copy;
  NodeSocketPair mat_displacement_copy;
  /* TreePath used to locate the nodetree.
   * bNodeTreePath elements have some listbase pointers which should not be used. */
  Vector<bNodeTreePath *> treepath_copy;
  Vector<NodeSocketPair> AOV_nodes;
  Vector<NodeSocketPair> shader_nodes;

  bNode *rendering_node;
  bool rendering_AOVs;

  Main *bmain;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compute Context functions
 * \{ */

static void ensure_nodetree_previews(const bContext &C,
                                     NestedTreePreviews &tree_previews,
                                     Material &material,
                                     ListBase &treepath);

static std::optional<ComputeContextHash> get_compute_context_hash_for_node_editor(
    const SpaceNode &snode)
{
  Vector<const bNodeTreePath *> treepath;
  LISTBASE_FOREACH (const bNodeTreePath *, item, &snode.treepath) {
    treepath.append(item);
  }

  if (treepath.is_empty()) {
    return std::nullopt;
  }
  if (treepath.size() == 1) {
    /* Top group. */
    ComputeContextHash hash;
    hash.v1 = hash.v2 = 0;
    return hash;
  }
  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *compute_context = nullptr;
  for (const int i : treepath.index_range().drop_back(1)) {
    /* The tree path contains the name of the node but not its ID. */
    bNodeTree *tree = treepath[i]->nodetree;
    const bNode *node = bke::node_find_node_by_name(*tree, treepath[i + 1]->node_name);
    if (node == nullptr) {
      /* The current tree path is invalid, probably because some parent group node has been
       * deleted. */
      return std::nullopt;
    }
    compute_context = &compute_context_cache.for_group_node(
        compute_context, node->identifier, tree);
  }
  return compute_context->hash();
}

NestedTreePreviews *get_nested_previews(const bContext &C, SpaceNode &snode)
{
  if (snode.id == nullptr || GS(snode.id->name) != ID_MA) {
    return nullptr;
  }
  NestedTreePreviews *tree_previews = nullptr;
  if (auto hash = get_compute_context_hash_for_node_editor(snode)) {
    tree_previews = snode.runtime->tree_previews_per_context
                        .lookup_or_add_cb(*hash,
                                          [&]() {
                                            return std::make_unique<NestedTreePreviews>(
                                                U.node_preview_res);
                                          })
                        .get();
    Material *ma = reinterpret_cast<Material *>(snode.id);
    ensure_nodetree_previews(C, *tree_previews, *ma, snode.treepath);
  }
  return tree_previews;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preview scene
 * \{ */

static Material *duplicate_material(const Material &mat)
{
  Material *ma_copy = reinterpret_cast<Material *>(
      BKE_id_copy_ex(nullptr,
                     &mat.id,
                     nullptr,
                     LIB_ID_CREATE_LOCAL | LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA));
  return ma_copy;
}

static Scene *preview_prepare_scene(const Main *bmain,
                                    const Scene *scene_orig,
                                    Main *pr_main,
                                    Material *mat_copy,
                                    ePreviewType preview_type)
{
  Scene *scene_preview;

  memcpy(pr_main->filepath, BKE_main_blendfile_path(bmain), sizeof(pr_main->filepath));

  if (pr_main == nullptr) {
    return nullptr;
  }
  scene_preview = static_cast<Scene *>(pr_main->scenes.first);
  if (scene_preview == nullptr) {
    return nullptr;
  }

  ViewLayer *view_layer = static_cast<ViewLayer *>(scene_preview->view_layers.first);

  /* Only enable the combined render-pass. */
  view_layer->passflag = SCE_PASS_COMBINED;
  view_layer->eevee.render_passes = 0;

  /* This flag tells render to not execute depsgraph or F-Curves etc. */
  scene_preview->r.scemode |= R_BUTS_PREVIEW;
  scene_preview->r.mode |= R_PERSISTENT_DATA;
  STRNCPY_UTF8(scene_preview->r.engine, scene_orig->r.engine);

  scene_preview->r.color_mgt_flag = scene_orig->r.color_mgt_flag;
  BKE_color_managed_display_settings_copy(&scene_preview->display_settings,
                                          &scene_orig->display_settings);

  BKE_color_managed_view_settings_free(&scene_preview->view_settings);
  BKE_color_managed_view_settings_copy(&scene_preview->view_settings, &scene_orig->view_settings);

  scene_preview->r.alphamode = R_ADDSKY;

  scene_preview->r.cfra = scene_orig->r.cfra;

  /* Setup the world. */
  scene_preview->world = ED_preview_prepare_world_simple(pr_main);
  ED_preview_world_simple_set_rgb(scene_preview->world, float4{0.05f, 0.05f, 0.05f, 0.05f});

  BLI_addtail(&pr_main->materials, mat_copy);

  ED_preview_set_visibility(pr_main, scene_preview, view_layer, preview_type, PR_BUTS_RENDER);

  BKE_view_layer_synced_ensure(scene_preview, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (base->object->id.name[2] == 'p') {
      if (OB_TYPE_SUPPORT_MATERIAL(base->object->type)) {
        /* Don't use BKE_object_material_assign, it changed mat->id.us, which shows in the UI. */
        Material ***matar = BKE_object_material_array_p(base->object);
        int actcol = max_ii(base->object->actcol - 1, 0);

        if (matar && actcol < base->object->totcol) {
          (*matar)[actcol] = mat_copy;
        }
      }
      else if (base->object->type == OB_LAMP) {
        base->flag |= BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT;
      }
    }
  }

  return scene_preview;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preview rendering
 * \{ */

/**
 * Follows some rules to determine the previewed socket and node associated.
 * We first seek for an output socket of the node, if none if found, the node is an output node,
 * and thus seek for an input socket.
 */
static bNodeSocket *node_find_preview_socket(bNodeTree &ntree, bNode &node)
{
  bNodeSocket *socket = get_main_socket(ntree, node, SOCK_OUT);
  if (socket == nullptr) {
    socket = get_main_socket(ntree, node, SOCK_IN);
    if (socket != nullptr && socket->link == nullptr) {
      if (!ELEM(socket->type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA)) {
        /* We can not preview a socket with no link and no manual value. */
        return nullptr;
      }
    }
  }
  return socket;
}

static bool socket_use_aov(const bNodeSocket *socket)
{
  return socket == nullptr || socket->type != SOCK_SHADER;
}

static bool node_use_aov(bNodeTree &ntree, const bNode *node)
{
  bNode *node_preview = const_cast<bNode *>(node);
  bNodeSocket *socket_preview = node_find_preview_socket(ntree, *node_preview);
  return socket_use_aov(socket_preview);
}

static ImBuf *get_image_from_viewlayer_and_pass(RenderResult &rr,
                                                const char *layer_name,
                                                const char *pass_name)
{
  RenderLayer *rl;
  if (layer_name) {
    rl = RE_GetRenderLayer(&rr, layer_name);
  }
  else {
    rl = static_cast<RenderLayer *>(rr.layers.first);
  }
  if (rl == nullptr) {
    return nullptr;
  }
  RenderPass *rp;
  if (pass_name) {
    rp = RE_pass_find_by_name(rl, pass_name, nullptr);
  }
  else {
    rp = static_cast<RenderPass *>(rl->passes.first);
  }
  ImBuf *ibuf = rp ? rp->ibuf : nullptr;
  return ibuf;
}

ImBuf *node_preview_acquire_ibuf(bNodeTree &ntree,
                                 NestedTreePreviews &tree_previews,
                                 const bNode &node)
{
  if (tree_previews.previews_render == nullptr) {
    return nullptr;
  }

  RenderResult *rr = RE_AcquireResultRead(tree_previews.previews_render);
  ImBuf *&image_cached = tree_previews.previews_map.lookup_or_add(node.identifier, nullptr);
  if (rr == nullptr) {
    return image_cached;
  }
  if (image_cached == nullptr) {
    if (tree_previews.rendering == false) {
      ntree.runtime->previews_refresh_state++;
    }
    else {
      /* When the render process is started, the user must see that the preview area is open. */
      ImBuf *image_latest = nullptr;
      if (node_use_aov(ntree, &node)) {
        image_latest = get_image_from_viewlayer_and_pass(*rr, nullptr, node.name);
      }
      else {
        image_latest = get_image_from_viewlayer_and_pass(*rr, node.name, nullptr);
      }
      if (image_latest) {
        IMB_refImBuf(image_latest);
        image_cached = image_latest;
      }
    }
  }
  return image_cached;
}

void node_release_preview_ibuf(NestedTreePreviews &tree_previews)
{
  if (tree_previews.previews_render == nullptr) {
    return;
  }
  RE_ReleaseResult(tree_previews.previews_render);
}

/**
 * Get a link to the node outside the nested node-groups by creating a new output socket for each
 * nested node-group. To do so we cover all nested node-trees starting from the farthest, and
 * update the `nested_node_iter` pointer to the current node-group instance used for linking.
 * We stop before getting to the main node-tree because the output type is different.
 */
static void connect_nested_node_to_node(const Span<bNodeTreePath *> treepath,
                                        bNode &nested_node,
                                        bNodeSocket &nested_socket,
                                        bNode &final_node,
                                        bNodeSocket &final_socket,
                                        const char *route_name)
{
  bNode *nested_node_iter = &nested_node;
  bNodeSocket *nested_socket_iter = &nested_socket;
  for (int i = treepath.size() - 1; i > 0; --i) {
    bNodeTreePath *path = treepath[i];
    bNodeTreePath *path_prev = treepath[i - 1];
    bNodeTree *nested_nt = path->nodetree;
    bNode *output_node = nullptr;
    for (bNode *iter_node : nested_nt->all_nodes()) {
      if (iter_node->is_group_output() && iter_node->flag & NODE_DO_OUTPUT) {
        output_node = iter_node;
        break;
      }
    }
    if (output_node == nullptr) {
      output_node = bke::node_add_static_node(nullptr, *nested_nt, NODE_GROUP_OUTPUT);
      output_node->flag |= NODE_DO_OUTPUT;
    }

    nested_nt->tree_interface.add_socket(
        route_name, "", nested_socket_iter->idname, NODE_INTERFACE_SOCKET_OUTPUT, nullptr);
    BKE_ntree_update_after_single_tree_change(*G.pr_main, *nested_nt);
    bNodeSocket *out_socket = blender::bke::node_find_enabled_input_socket(*output_node,
                                                                           route_name);

    bke::node_add_link(
        *nested_nt, *nested_node_iter, *nested_socket_iter, *output_node, *out_socket);
    BKE_ntree_update_after_single_tree_change(*G.pr_main, *nested_nt);

    /* Change the `nested_node` pointer to the nested node-group instance node. The tree path
     * contains the name of the instance node but not its ID. */
    nested_node_iter = bke::node_find_node_by_name(*path_prev->nodetree, path->node_name);

    /* Update the sockets of the node because we added a new interface. */
    BKE_ntree_update_tag_node_property(path_prev->nodetree, nested_node_iter);
    BKE_ntree_update_after_single_tree_change(*G.pr_main, *path_prev->nodetree);

    /* Now use the newly created socket of the node-group as previewing socket of the node-group
     * instance node. */
    nested_socket_iter = blender::bke::node_find_enabled_output_socket(*nested_node_iter,
                                                                       route_name);
  }

  bke::node_add_link(*treepath.first()->nodetree,
                     *nested_node_iter,
                     *nested_socket_iter,
                     final_node,
                     final_socket);
}

/* Connect the node to the output of the first nodetree from `treepath`. Last element of `treepath`
 * should be the path to the node's nodetree */
static void connect_node_to_surface_output(const Span<bNodeTreePath *> treepath,
                                           NodeSocketPair nodesocket,
                                           bNode &output_node)
{
  bNodeSocket *out_surface_socket = nullptr;
  bNodeTree *main_nt = treepath.first()->nodetree;
  bNode *node_preview = nodesocket.first;
  bNodeSocket *socket_preview = nodesocket.second;
  if (socket_preview == nullptr) {
    return;
  }
  if (socket_preview->in_out == SOCK_IN) {
    BLI_assert(socket_preview->link != nullptr);
    node_preview = socket_preview->link->fromnode;
    socket_preview = socket_preview->link->fromsock;
  }
  /* Ensure output is usable. */
  out_surface_socket = bke::node_find_socket(output_node, SOCK_IN, "Surface");
  if (out_surface_socket->link) {
    /* Make sure no node is already wired to the output before wiring. */
    bke::node_remove_link(main_nt, *out_surface_socket->link);
  }

  connect_nested_node_to_node(treepath,
                              *node_preview,
                              *socket_preview,
                              output_node,
                              *out_surface_socket,
                              nodesocket.first->name);
  BKE_ntree_update_after_single_tree_change(*G.pr_main, *main_nt);
}

/* Connect the nodes to some aov nodes located in the first nodetree from `treepath`. Last element
 * of `treepath` should be the path to the nodes nodetree. */
static void connect_nodes_to_aovs(const Span<bNodeTreePath *> treepath,
                                  const Span<NodeSocketPair> nodesocket_span)
{
  if (nodesocket_span.is_empty()) {
    return;
  }
  bNodeTree *main_nt = treepath.first()->nodetree;
  bNodeTree *active_nt = treepath.last()->nodetree;
  for (NodeSocketPair nodesocket : nodesocket_span) {
    bNode *node_preview = nodesocket.first;
    bNodeSocket *socket_preview = nodesocket.second;

    bNode *aov_node = bke::node_add_static_node(nullptr, *main_nt, SH_NODE_OUTPUT_AOV);
    STRNCPY_UTF8(reinterpret_cast<NodeShaderOutputAOV *>(aov_node->storage)->name,
                 nodesocket.first->name);
    if (socket_preview == nullptr) {
      continue;
    }
    bNodeSocket *aov_socket = bke::node_find_socket(*aov_node, SOCK_IN, "Color");
    if (socket_preview->in_out == SOCK_IN) {
      if (socket_preview->link == nullptr) {
        /* Copy the custom value of the socket directly to the AOV node.
         * If the socket does not support custom values, it will just render black. */
        float vec[4] = {0., 0., 0., 1.};
        PointerRNA ptr;
        switch (socket_preview->type) {
          case SOCK_FLOAT:
            ptr = RNA_pointer_create_discrete((ID *)active_nt, &RNA_NodeSocket, socket_preview);
            vec[0] = RNA_float_get(&ptr, "default_value");
            vec[1] = vec[0];
            vec[2] = vec[0];
            break;
          case SOCK_VECTOR:
          case SOCK_RGBA:
            ptr = RNA_pointer_create_discrete((ID *)active_nt, &RNA_NodeSocket, socket_preview);
            RNA_float_get_array(&ptr, "default_value", vec);
            break;
        }
        ptr = RNA_pointer_create_discrete((ID *)active_nt, &RNA_NodeSocket, aov_socket);
        RNA_float_set_array(&ptr, "default_value", vec);
        continue;
      }
      node_preview = socket_preview->link->fromnode;
      socket_preview = socket_preview->link->fromsock;
    }
    connect_nested_node_to_node(
        treepath, *node_preview, *socket_preview, *aov_node, *aov_socket, nodesocket.first->name);
  }
  BKE_ntree_update_after_single_tree_change(*G.pr_main, *main_nt);
}

/* Called by renderer, checks job stops. */
static bool nodetree_previews_break(void *spv)
{
  ShaderNodesPreviewJob *job_data = static_cast<ShaderNodesPreviewJob *>(spv);

  return *(job_data->stop);
}

static bool prepare_viewlayer_update(void *pvl_data, ViewLayer *vl, Depsgraph *depsgraph)
{
  NodeSocketPair nodesocket = {nullptr, nullptr};
  ShaderNodesPreviewJob *job_data = static_cast<ShaderNodesPreviewJob *>(pvl_data);
  for (NodeSocketPair nodesocket_iter : job_data->shader_nodes) {
    if (STREQ(vl->name, nodesocket_iter.first->name)) {
      nodesocket = nodesocket_iter;
      job_data->rendering_node = nodesocket_iter.first;
      job_data->rendering_AOVs = false;
      break;
    }
  }
  if (nodesocket.first == nullptr) {
    job_data->rendering_node = nullptr;
    job_data->rendering_AOVs = true;
    /* The AOV layer is the default `ViewLayer` of the scene(which should be the first one). */
    return job_data->AOV_nodes.size() > 0 && !vl->prev;
  }

  bNodeSocket *displacement_socket = bke::node_find_socket(
      *job_data->mat_output_copy, SOCK_IN, "Displacement");
  if (job_data->mat_displacement_copy.first != nullptr && displacement_socket->link == nullptr) {
    bke::node_add_link(*job_data->treepath_copy.first()->nodetree,
                       *job_data->mat_displacement_copy.first,
                       *job_data->mat_displacement_copy.second,
                       *job_data->mat_output_copy,
                       *displacement_socket);
  }
  connect_node_to_surface_output(job_data->treepath_copy, nodesocket, *job_data->mat_output_copy);

  if (depsgraph != nullptr) {
    /* Used to refresh the dependency graph so that the material can be updated. */
    for (bNodeTreePath *path_iter : job_data->treepath_copy) {
      DEG_graph_id_tag_update(
          G.pr_main, depsgraph, &path_iter->nodetree->id, ID_RECALC_NTREE_OUTPUT);
    }
  }
  return true;
}

/* Called by renderer, refresh the UI. */
static void all_nodes_preview_update(void *npv, RenderResult *rr, rcti * /*rect*/)
{
  ShaderNodesPreviewJob *job_data = static_cast<ShaderNodesPreviewJob *>(npv);
  *job_data->do_update = true;
  if (bNode *node = job_data->rendering_node) {
    ImBuf *&image_cached = job_data->tree_previews->previews_map.lookup_or_add(node->identifier,
                                                                               nullptr);
    ImBuf *image_latest = get_image_from_viewlayer_and_pass(*rr, node->name, nullptr);
    if (image_latest == nullptr) {
      return;
    }
    if (image_cached != image_latest) {
      if (image_cached != nullptr) {
        IMB_freeImBuf(image_cached);
      }
      IMB_refImBuf(image_latest);
      image_cached = image_latest;
    }
  }
  if (job_data->rendering_AOVs) {
    for (NodeSocketPair nodesocket_iter : job_data->AOV_nodes) {
      ImBuf *&image_cached = job_data->tree_previews->previews_map.lookup_or_add(
          nodesocket_iter.first->identifier, nullptr);
      ImBuf *image_latest = get_image_from_viewlayer_and_pass(
          *rr, nullptr, nodesocket_iter.first->name);
      if (image_latest == nullptr) {
        continue;
      }
      if (image_cached != image_latest) {
        if (image_cached != nullptr) {
          IMB_freeImBuf(image_cached);
        }
        IMB_refImBuf(image_latest);
        image_cached = image_latest;
      }
    }
  }
}

static void preview_render(ShaderNodesPreviewJob &job_data)
{
  /* Get the stuff from the builtin preview dbase. */
  Scene *scene = preview_prepare_scene(
      job_data.bmain, job_data.scene, G.pr_main, job_data.mat_copy, job_data.preview_type);
  if (scene == nullptr) {
    return;
  }
  Span<bNodeTreePath *> treepath = job_data.treepath_copy;

  /* AOV nodes are rendered in the first RenderLayer so we route them now. */
  connect_nodes_to_aovs(treepath, job_data.AOV_nodes);

  /* Create the AOV passes for the viewlayer. */
  ViewLayer *AOV_layer = static_cast<ViewLayer *>(scene->view_layers.first);
  for (NodeSocketPair nodesocket_iter : job_data.shader_nodes) {
    ViewLayer *vl = BKE_view_layer_add(
        scene, nodesocket_iter.first->name, AOV_layer, VIEWLAYER_ADD_COPY);
    STRNCPY_UTF8(vl->name, nodesocket_iter.first->name);
  }
  for (NodeSocketPair nodesocket_iter : job_data.AOV_nodes) {
    ViewLayerAOV *aov = BKE_view_layer_add_aov(AOV_layer);
    STRNCPY_UTF8(aov->name, nodesocket_iter.first->name);
  }
  scene->r.xsch = job_data.tree_previews->preview_size;
  scene->r.ysch = job_data.tree_previews->preview_size;
  scene->r.size = 100;

  if (job_data.tree_previews->previews_render == nullptr) {
    job_data.tree_previews->previews_render = RE_NewRender(&job_data.tree_previews);
  }
  Render *re = job_data.tree_previews->previews_render;

  /* `sce->r` gets copied in RE_InitState. */
  scene->r.scemode &= ~(R_MATNODE_PREVIEW | R_TEXNODE_PREVIEW);
  scene->r.scemode &= ~R_NO_IMAGE_LOAD;

  scene->display.render_aa = SCE_DISPLAY_AA_SAMPLES_8;

  RE_display_update_cb(re, &job_data, all_nodes_preview_update);
  RE_test_break_cb(re, &job_data, nodetree_previews_break);
  RE_prepare_viewlayer_cb(re, &job_data, prepare_viewlayer_update);

  /* Lens adjust. */
  float oldlens = reinterpret_cast<Camera *>(scene->camera->data)->lens;

  RE_ClearResult(re);
  RE_PreviewRender(re, G.pr_main, scene);

  reinterpret_cast<Camera *>(scene->camera->data)->lens = oldlens;

  /* Free the aov layers and the layers generated for each node. */
  BLI_freelistN(&AOV_layer->aovs);
  ViewLayer *vl = AOV_layer->next;
  while (vl) {
    ViewLayer *vl_rem = vl;
    vl = vl->next;
    BLI_remlink(&scene->view_layers, vl_rem);
    BKE_view_layer_free(vl_rem);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preview job management
 * \{ */

static void update_needed_flag(NestedTreePreviews &tree_previews,
                               const bNodeTree &nt,
                               ePreviewType preview_type)
{
  if (tree_previews.rendering) {
    if (nt.runtime->previews_refresh_state != tree_previews.rendering_previews_refresh_state) {
      tree_previews.restart_needed = true;
      return;
    }
    if (preview_type != tree_previews.rendering_preview_type) {
      tree_previews.restart_needed = true;
      return;
    }
  }
  else {
    if (nt.runtime->previews_refresh_state != tree_previews.cached_previews_refresh_state) {
      tree_previews.restart_needed = true;
      return;
    }
    if (preview_type != tree_previews.cached_preview_type) {
      tree_previews.restart_needed = true;
      return;
    }
  }
  if (tree_previews.preview_size != U.node_preview_res) {
    tree_previews.restart_needed = true;
    return;
  }
}

static void shader_preview_startjob(void *customdata, wmJobWorkerStatus *worker_status)
{
  ShaderNodesPreviewJob *job_data = static_cast<ShaderNodesPreviewJob *>(customdata);

  job_data->stop = &worker_status->stop;
  job_data->do_update = &worker_status->do_update;
  worker_status->do_update = true;
  bool size_changed = job_data->tree_previews->preview_size != U.node_preview_res;
  if (size_changed) {
    job_data->tree_previews->preview_size = U.node_preview_res;
  }

  for (bNode *node_iter : job_data->mat_copy->nodetree->all_nodes()) {
    if (node_iter->flag & NODE_DO_OUTPUT) {
      node_iter->flag &= ~NODE_DO_OUTPUT;
      bNodeSocket *disp_socket = bke::node_find_socket(*node_iter, SOCK_IN, "Displacement");
      if (disp_socket != nullptr && disp_socket->link != nullptr) {
        job_data->mat_displacement_copy = std::make_pair(disp_socket->link->fromnode,
                                                         disp_socket->link->fromsock);
      }
      break;
    }
  }

  /* Add a new output node used only for the previews. This is useful to keep the previously
   * connected links (for previewing the output nodes for example). */
  job_data->mat_output_copy = bke::node_add_static_node(
      nullptr, *job_data->mat_copy->nodetree, SH_NODE_OUTPUT_MATERIAL);
  job_data->mat_output_copy->flag |= NODE_DO_OUTPUT;

  bNodeTree *active_nodetree = job_data->treepath_copy.last()->nodetree;
  active_nodetree->ensure_topology_cache();
  for (bNode *node : active_nodetree->all_nodes()) {
    if (!(node->flag & NODE_PREVIEW)) {
      /* Clear the cached preview for this node to be sure that the preview is re-rendered if
       * needed. */
      if (ImBuf **ibuf = job_data->tree_previews->previews_map.lookup_ptr(node->identifier)) {
        IMB_freeImBuf(*ibuf);
        *ibuf = nullptr;
      }
      continue;
    }
    bNodeSocket *preview_socket = node_find_preview_socket(*active_nodetree, *node);
    if (socket_use_aov(preview_socket)) {
      job_data->AOV_nodes.append({node, preview_socket});
    }
    else {
      job_data->shader_nodes.append({node, preview_socket});
    }
  }

  if (job_data->tree_previews->preview_size > 0) {
    preview_render(*job_data);
  }
}

static void shader_preview_free(void *customdata)
{
  ShaderNodesPreviewJob *job_data = static_cast<ShaderNodesPreviewJob *>(customdata);
  for (bNodeTreePath *path : job_data->treepath_copy) {
    MEM_freeN(path);
  }
  job_data->treepath_copy.clear();
  job_data->tree_previews->rendering = false;
  job_data->tree_previews->cached_previews_refresh_state =
      job_data->tree_previews->rendering_previews_refresh_state;
  job_data->tree_previews->cached_preview_type = job_data->preview_type;
  if (job_data->mat_copy != nullptr) {
    BLI_remlink(&G.pr_main->materials, job_data->mat_copy);
    BKE_id_free(G.pr_main, &job_data->mat_copy->id);
    job_data->mat_copy = nullptr;
  }
  MEM_delete(job_data);
}

static void ensure_nodetree_previews(const bContext &C,
                                     NestedTreePreviews &tree_previews,
                                     Material &material,
                                     ListBase &treepath)
{
  Scene *scene = CTX_data_scene(&C);
  if (!ED_check_engine_supports_preview(scene)) {
    return;
  }

  bNodeTree *displayed_nodetree = static_cast<bNodeTreePath *>(treepath.last)->nodetree;
  ePreviewType preview_type = MA_FLAT;
  if (CTX_wm_space_node(&C)->overlay.preview_shape == SN_OVERLAY_PREVIEW_3D) {
    preview_type = (ePreviewType)material.pr_type;
  }
  update_needed_flag(tree_previews, *displayed_nodetree, preview_type);
  if (!(tree_previews.restart_needed)) {
    return;
  }
  if (tree_previews.rendering) {
    WM_jobs_stop_type(CTX_wm_manager(&C), CTX_wm_space_node(&C), WM_JOB_TYPE_RENDER_PREVIEW);
    return;
  }
  tree_previews.rendering = true;
  tree_previews.restart_needed = false;
  tree_previews.rendering_previews_refresh_state =
      displayed_nodetree->runtime->previews_refresh_state;
  tree_previews.rendering_preview_type = preview_type;

  ED_preview_ensure_dbase(false);

  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(&C),
                              CTX_wm_window(&C),
                              CTX_wm_space_node(&C),
                              "Generating shader previews...",
                              WM_JOB_EXCL_RENDER,
                              WM_JOB_TYPE_RENDER_PREVIEW);
  ShaderNodesPreviewJob *job_data = MEM_new<ShaderNodesPreviewJob>(__func__);

  job_data->scene = scene;
  job_data->tree_previews = &tree_previews;
  job_data->bmain = CTX_data_main(&C);
  job_data->mat_copy = duplicate_material(material);
  job_data->rendering_node = nullptr;
  job_data->rendering_AOVs = false;
  job_data->preview_type = preview_type;

  /* Update the treepath copied to fit the structure of the nodetree copied. */
  bNodeTreePath *root_path = MEM_callocN<bNodeTreePath>(__func__);
  root_path->nodetree = job_data->mat_copy->nodetree;
  job_data->treepath_copy.append(root_path);
  for (bNodeTreePath *original_path = static_cast<bNodeTreePath *>(treepath.first)->next;
       original_path;
       original_path = original_path->next)
  {
    bNode *parent = bke::node_find_node_by_name(*job_data->treepath_copy.last()->nodetree,
                                                original_path->node_name);
    if (parent == nullptr) {
      /* In some cases (e.g. muted nodes), there may not be an equivalent node in the copied
       * nodetree. In that case, just skip the node. */
      continue;
    }
    bNodeTreePath *new_path = MEM_callocN<bNodeTreePath>(__func__);
    memcpy(new_path, original_path, sizeof(bNodeTreePath));
    new_path->nodetree = reinterpret_cast<bNodeTree *>(parent->id);
    job_data->treepath_copy.append(new_path);
  }

  WM_jobs_customdata_set(wm_job, job_data, shader_preview_free);
  WM_jobs_timer(wm_job, 0.2, NC_NODE, NC_NODE);
  WM_jobs_callbacks(wm_job, shader_preview_startjob, nullptr, nullptr, nullptr);

  WM_jobs_start(CTX_wm_manager(&C), wm_job);
}

void free_previews(wmWindowManager &wm, SpaceNode &snode)
{
  /* This should not be called from the drawing pass, because it will result in a deadlock. */
  WM_jobs_kill_type(&wm, &snode, WM_JOB_TYPE_RENDER_PREVIEW);
  snode.runtime->tree_previews_per_context.clear();
}

/** \} */

}  // namespace blender::ed::space_node
