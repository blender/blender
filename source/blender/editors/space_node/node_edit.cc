/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 */

#include <algorithm>
#include <optional>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_text_types.h"
#include "DNA_world_types.h"

#include "BKE_callbacks.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_material.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_scene_runtime.hh"
#include "BKE_screen.hh"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_debug.hh"
#include "DEG_depsgraph_query.hh"

#include "NOD_shader_nodes_inline.hh"
#include "RE_engine.h"
#include "RE_pipeline.h"

#include "ED_image.hh"
#include "ED_node.hh" /* own include */
#include "ED_render.hh"
#include "ED_screen.hh"
#include "ED_viewer_path.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_view2d.hh"

#include "GPU_capabilities.hh"
#include "GPU_material.hh"

#include "IMB_imbuf_types.hh"

#include "NOD_composite.hh"
#include "NOD_geometry.hh"
#include "NOD_shader.h"
#include "NOD_socket.hh"
#include "NOD_texture.h"
#include "node_intern.hh" /* own include */

#include "COM_compositor.hh"
#include "COM_context.hh"
#include "COM_profiler.hh"

namespace blender::ed::space_node {

#define USE_ESC_COMPO

/* -------------------------------------------------------------------- */
/** \name Composite Job Manager
 * \{ */

struct CompoJob {
  /* Input parameters. */
  Main *bmain;
  Scene *scene;
  ViewLayer *view_layer;
  bNodeTree *ntree;
  /* Evaluated state/ */
  Depsgraph *compositor_depsgraph;
  bNodeTree *localtree;
  /* Render instance. */
  Render *re;
  /* Job system integration. */
  const bool *stop;
  bool *do_update;
  float *progress;
  bool cancelled;

  compositor::Profiler profiler;
  compositor::OutputTypes needed_outputs;
};

float node_socket_calculate_height(const bNodeSocket &socket)
{
  float sock_height = NODE_SOCKSIZE;
  if (socket.flag & SOCK_MULTI_INPUT) {
    sock_height += max_ii(NODE_MULTI_INPUT_LINK_GAP * 0.5f * socket.runtime->total_inputs,
                          NODE_SOCKSIZE);
  }
  return sock_height;
}

float2 node_link_calculate_multi_input_position(const float2 &socket_position,
                                                const int index,
                                                const int total_inputs)
{
  const float offset = (total_inputs * NODE_MULTI_INPUT_LINK_GAP - NODE_MULTI_INPUT_LINK_GAP) *
                       0.5f;
  return {socket_position.x, socket_position.y - offset + index * NODE_MULTI_INPUT_LINK_GAP};
}

/* Called by compositor, only to check job 'stop' value. */
static bool compo_breakjob(void *cjv)
{
  CompoJob *cj = (CompoJob *)cjv;

  /* Without G.is_break 'ESC' won't quit - which annoys users. */
  return (*(cj->stop)
#ifdef USE_ESC_COMPO
          || G.is_break
#endif
  );
}

/* Called by compositor, #wmJob sends notifier. */
static void compo_statsdrawjob(void *cjv, const char * /*str*/)
{
  CompoJob *cj = (CompoJob *)cjv;

  *(cj->do_update) = true;
}

/* Called by compositor, wmJob sends notifier. */
static void compo_redrawjob(void *cjv)
{
  CompoJob *cj = (CompoJob *)cjv;

  *(cj->do_update) = true;
}

static void compo_freejob(void *cjv)
{
  CompoJob *cj = (CompoJob *)cjv;

  if (cj->localtree) {
    /* Merge back node previews, only for completed jobs. */
    if (!cj->cancelled) {
      bke::node_tree_local_merge(cj->bmain, cj->localtree, cj->ntree);
    }

    bke::node_tree_free_tree(*cj->localtree);
    MEM_freeN(cj->localtree);
  }

  MEM_delete(cj);
}

/* Only now we copy the nodetree, so adding many jobs while
 * sliding buttons doesn't frustrate. */
static void compo_initjob(void *cjv)
{
  CompoJob *cj = (CompoJob *)cjv;
  Main *bmain = cj->bmain;
  Scene *scene = cj->scene;
  ViewLayer *view_layer = cj->view_layer;

  bke::CompositorRuntime &compositor_runtime = scene->runtime->compositor;

  if (!compositor_runtime.preview_depsgraph) {
    compositor_runtime.preview_depsgraph = DEG_graph_new(
        bmain, scene, view_layer, DAG_EVAL_RENDER);
    DEG_debug_name_set(compositor_runtime.preview_depsgraph, "COMPOSITOR");
  }

  /* Update the viewer layer of the compositor since it changed since the depsgraph was created. */
  if (DEG_get_input_view_layer(compositor_runtime.preview_depsgraph) != view_layer) {
    DEG_graph_replace_owners(compositor_runtime.preview_depsgraph, bmain, scene, view_layer);
    DEG_graph_tag_relations_update(compositor_runtime.preview_depsgraph);
  }

  cj->compositor_depsgraph = compositor_runtime.preview_depsgraph;
  DEG_graph_build_for_compositor_preview(cj->compositor_depsgraph, cj->ntree);

  /* NOTE: Don't update animation to preserve unkeyed changes, this means can not use
   * evaluate_on_framechange. */
  DEG_evaluate_on_refresh(cj->compositor_depsgraph);

  bNodeTree *ntree_eval = DEG_get_evaluated(cj->compositor_depsgraph, cj->ntree);

  cj->localtree = bke::node_tree_localize(ntree_eval, nullptr);

  cj->re = RE_NewInteractiveCompositorRender(scene);
  if (scene->r.compositor_device == SCE_COMPOSITOR_DEVICE_GPU) {
    RE_system_gpu_context_ensure(cj->re);
  }
}

/* Called before redraw notifiers, it moves finished previews over. */
static void compo_updatejob(void * /*cjv*/)
{
  WM_main_add_notifier(NC_SCENE | ND_COMPO_RESULT, nullptr);
}

static void compo_progressjob(void *cjv, float progress)
{
  CompoJob *cj = (CompoJob *)cjv;

  *(cj->progress) = progress;
}

/* Only this runs inside thread. */
static void compo_startjob(void *cjv, wmJobWorkerStatus *worker_status)
{
  CompoJob *cj = (CompoJob *)cjv;
  bNodeTree *ntree = cj->localtree;
  Scene *scene = DEG_get_evaluated_scene(cj->compositor_depsgraph);

  cj->stop = &worker_status->stop;
  cj->do_update = &worker_status->do_update;
  cj->progress = &worker_status->progress;

  ntree->runtime->test_break = compo_breakjob;
  ntree->runtime->tbh = cj;
  ntree->runtime->stats_draw = compo_statsdrawjob;
  ntree->runtime->sdh = cj;
  ntree->runtime->progress = compo_progressjob;
  ntree->runtime->prh = cj;
  ntree->runtime->update_draw = compo_redrawjob;
  ntree->runtime->udh = cj;

  BKE_callback_exec_id(cj->bmain, &cj->scene->id, BKE_CB_EVT_COMPOSITE_PRE);

  if ((scene->r.scemode & R_MULTIVIEW) == 0) {
    COM_execute(cj->re, &scene->r, scene, ntree, "", nullptr, &cj->profiler, cj->needed_outputs);
  }
  else {
    LISTBASE_FOREACH (SceneRenderView *, srv, &scene->r.views) {
      if (BKE_scene_multiview_is_render_view_active(&scene->r, srv) == false) {
        continue;
      }
      COM_execute(
          cj->re, &scene->r, scene, ntree, srv->name, nullptr, &cj->profiler, cj->needed_outputs);
    }
  }

  ntree->runtime->test_break = nullptr;
  ntree->runtime->stats_draw = nullptr;
  ntree->runtime->progress = nullptr;
}

static void compo_canceljob(void *cjv)
{
  CompoJob *cj = (CompoJob *)cjv;
  Main *bmain = cj->bmain;
  Scene *scene = cj->scene;
  BKE_callback_exec_id(bmain, &scene->id, BKE_CB_EVT_COMPOSITE_CANCEL);
  cj->cancelled = true;

  scene->runtime->compositor.per_node_execution_time = cj->profiler.get_nodes_evaluation_times();
}

static void compo_completejob(void *cjv)
{
  CompoJob *cj = (CompoJob *)cjv;
  Main *bmain = cj->bmain;
  Scene *scene = cj->scene;
  BKE_callback_exec_id(bmain, &scene->id, BKE_CB_EVT_COMPOSITE_POST);

  scene->runtime->compositor.per_node_execution_time = cj->profiler.get_nodes_evaluation_times();
}

/** \} */

}  // namespace blender::ed::space_node

/* -------------------------------------------------------------------- */
/** \name Composite Job C API
 * \{ */

/* Identify if the compositor can run. Currently, this only checks if the compositor is set to GPU
 * and the render size exceeds what can be allocated as a texture in it. */
static bool is_compositing_possible(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  /* CPU compositor can always run. */
  if (scene->r.compositor_device != SCE_COMPOSITOR_DEVICE_GPU) {
    return true;
  }

  int width, height;
  BKE_render_resolution(&scene->r, false, &width, &height);
  if (!GPU_is_safe_texture_size(width, height)) {
    WM_global_report(RPT_ERROR, "Render size too large for GPU, use CPU compositor instead");
    return false;
  }

  return true;
}

/* Returns the compositor outputs that need to be computed because their result is visible to the
 * user. */
static blender::compositor::OutputTypes get_compositor_needed_outputs(const bContext *C)
{
  blender::compositor::OutputTypes needed_outputs = blender::compositor::OutputTypes::None;

  wmWindowManager *window_manager = CTX_wm_manager(C);
  LISTBASE_FOREACH (wmWindow *, window, &window_manager->windows) {
    bScreen *screen = WM_window_get_active_screen(window);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      SpaceLink *space_link = static_cast<SpaceLink *>(area->spacedata.first);
      if (!space_link || !ELEM(space_link->spacetype, SPACE_NODE, SPACE_IMAGE)) {
        continue;
      }
      if (space_link->spacetype == SPACE_NODE) {
        const SpaceNode *space_node = reinterpret_cast<const SpaceNode *>(space_link);
        if (space_node->flag & SNODE_BACKDRAW) {
          needed_outputs |= blender::compositor::OutputTypes::Viewer;
        }
        if (space_node->overlay.flag & SN_OVERLAY_SHOW_PREVIEWS) {
          needed_outputs |= blender::compositor::OutputTypes::Previews;
        }
      }
      else if (space_link->spacetype == SPACE_IMAGE) {
        const SpaceImage *space_image = reinterpret_cast<const SpaceImage *>(space_link);
        Image *image = ED_space_image(space_image);
        if (!image || image->source != IMA_SRC_VIEWER) {
          continue;
        }
        if (image->type == IMA_TYPE_R_RESULT) {
          needed_outputs |= blender::compositor::OutputTypes::Composite;
        }
        else if (image->type == IMA_TYPE_COMPOSITE) {
          needed_outputs |= blender::compositor::OutputTypes::Viewer;
        }
      }

      /* All outputs are already needed, return early. */
      if (needed_outputs ==
          (blender::compositor::OutputTypes::Composite | blender::compositor::OutputTypes::Viewer |
           blender::compositor::OutputTypes::Previews))
      {
        return needed_outputs;
      }
    }
  }

  return needed_outputs;
}

void ED_node_composite_job(const bContext *C, bNodeTree *nodetree, Scene *scene_owner)
{
  /* None of the outputs are needed except maybe previews, so no need to execute the compositor.
   * Previews are not considered because they are a secondary output that needs another output to
   * be computed with. */
  blender::compositor::OutputTypes needed_outputs = get_compositor_needed_outputs(C);
  if (ELEM(needed_outputs,
           blender::compositor::OutputTypes::None,
           blender::compositor::OutputTypes::Previews))
  {
    return;
  }

  using namespace blender::ed::space_node;

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (!is_compositing_possible(C)) {
    return;
  }

  /* See #32272. */
  if (G.is_rendering) {
    return;
  }

#ifdef USE_ESC_COMPO
  G.is_break = false;
#endif

  BKE_image_backup_render(
      scene, BKE_image_ensure_viewer(bmain, IMA_TYPE_R_RESULT, "Render Result"), false);

  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              scene_owner,
                              "Compositing...",
                              WM_JOB_EXCL_RENDER | WM_JOB_PROGRESS,
                              WM_JOB_TYPE_COMPOSITE);
  CompoJob *cj = MEM_new<CompoJob>("compo job");

  /* Custom data for preview thread. */
  cj->bmain = bmain;
  cj->scene = scene;
  cj->view_layer = view_layer;
  cj->ntree = nodetree;
  cj->needed_outputs = needed_outputs;

  /* Set up job. */
  WM_jobs_customdata_set(wm_job, cj, compo_freejob);
  WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_COMPO_RESULT, NC_SCENE | ND_COMPO_RESULT);
  WM_jobs_callbacks_ex(wm_job,
                       compo_startjob,
                       compo_initjob,
                       compo_updatejob,
                       nullptr,
                       compo_completejob,
                       compo_canceljob);

  WM_jobs_start(CTX_wm_manager(C), wm_job);
}

/** \} */

namespace blender::ed::space_node {

/* -------------------------------------------------------------------- */
/** \name Composite Poll & Utility Functions
 * \{ */

bool composite_node_active(bContext *C)
{
  if (ED_operator_node_active(C)) {
    SpaceNode *snode = CTX_wm_space_node(C);
    if (ED_node_is_compositor(snode)) {
      return true;
    }
  }
  return false;
}

bool composite_node_editable(bContext *C)
{
  if (ED_operator_node_editable(C)) {
    SpaceNode *snode = CTX_wm_space_node(C);
    if (ED_node_is_compositor(snode)) {
      return true;
    }
  }
  return false;
}

/** \} */

}  // namespace blender::ed::space_node

/* -------------------------------------------------------------------- */
/** \name Node Editor Public API Functions
 * \{ */

void ED_node_set_tree_type(SpaceNode *snode, blender::bke::bNodeTreeType *typeinfo)
{
  if (typeinfo) {
    STRNCPY_UTF8(snode->tree_idname, typeinfo->idname.c_str());
  }
  else {
    snode->tree_idname[0] = '\0';
  }

  /* Reset members that store tree type-dependant values. */
  snode->node_tree_sub_type = 0;
  snode->selected_node_group = nullptr;
}

bool ED_node_is_compositor(const SpaceNode *snode)
{
  return snode->tree_idname == ntreeType_Composite->idname;
}

bool ED_node_is_shader(SpaceNode *snode)
{
  return snode->tree_idname == ntreeType_Shader->idname;
}

bool ED_node_is_texture(SpaceNode *snode)
{
  return snode->tree_idname == ntreeType_Texture->idname;
}

bool ED_node_is_geometry(const SpaceNode *snode)
{
  return snode->tree_idname == ntreeType_Geometry->idname;
}

bool ED_node_supports_preview(SpaceNode *snode)
{
  return ED_node_is_compositor(snode) ||
         (USER_EXPERIMENTAL_TEST(&U, use_shader_node_previews) && ED_node_is_shader(snode));
}

void ED_node_shader_default(const bContext *C, Main *bmain, ID *id)
{
  if (GS(id->name) == ID_MA) {
    /* Materials */
    Object *ob = CTX_data_active_object(C);
    Material *ma = (Material *)id;
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
      World *world = (World *)id;
      ntree = world->nodetree;

      shader = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_BACKGROUND);
      output = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_OUTPUT_WORLD);
      blender::bke::node_add_link(*ntree,
                                  *shader,
                                  *blender::bke::node_find_socket(*shader, SOCK_OUT, "Background"),
                                  *output,
                                  *blender::bke::node_find_socket(*output, SOCK_IN, "Surface"));

      bNodeSocket *color_sock = blender::bke::node_find_socket(*shader, SOCK_IN, "Color");
      copy_v3_v3(((bNodeSocketValueRGBA *)color_sock->default_value)->value, &world->horr);
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
    printf("ED_node_shader_default called on wrong ID type.\n");
    return;
  }
}

void ED_node_composit_default(const bContext *C, Scene *sce)
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

  ED_node_composit_default_init(C, sce->compositing_node_group);

  BKE_ntree_update_after_single_tree_change(*bmain, *sce->compositing_node_group);
}

void ED_node_composit_default_init(const bContext *C, bNodeTree *ntree)
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
                              *(bNodeSocket *)in->outputs.first,
                              *reroute,
                              *(bNodeSocket *)reroute->inputs.first);

  blender::bke::node_add_link(*ntree,
                              *reroute,
                              *(bNodeSocket *)reroute->outputs.first,
                              *composite,
                              *(bNodeSocket *)composite->inputs.first);

  blender::bke::node_add_link(*ntree,
                              *reroute,
                              *(bNodeSocket *)reroute->outputs.first,
                              *viewer,
                              *(bNodeSocket *)viewer->inputs.first);

  BKE_ntree_update_after_single_tree_change(*CTX_data_main(C), *ntree);
}

void ED_node_texture_default(const bContext *C, Tex *tex)
{
  if (tex->nodetree) {
    if (G.debug & G_DEBUG) {
      printf("error in texture initialize\n");
    }
    return;
  }

  tex->nodetree = blender::bke::node_tree_add_tree_embedded(
      nullptr, &tex->id, "Texture Nodetree", ntreeType_Texture->idname);

  bNode *out = blender::bke::node_add_static_node(C, *tex->nodetree, TEX_NODE_OUTPUT);
  out->location[0] = 300.0f;
  out->location[1] = 300.0f;

  bNode *in = blender::bke::node_add_static_node(C, *tex->nodetree, TEX_NODE_CHECKER);
  in->location[0] = 10.0f;
  in->location[1] = 300.0f;
  blender::bke::node_set_active(*tex->nodetree, *in);

  bNodeSocket *fromsock = (bNodeSocket *)in->outputs.first;
  bNodeSocket *tosock = (bNodeSocket *)out->inputs.first;
  blender::bke::node_add_link(*tex->nodetree, *in, *fromsock, *out, *tosock);

  BKE_ntree_update_after_single_tree_change(*CTX_data_main(C), *tex->nodetree);
}

namespace blender::ed::space_node {

void snode_set_context(const bContext &C)
{
  /* NOTE: Here we set the active tree(s), even called for each redraw now, so keep it fast :). */

  SpaceNode *snode = CTX_wm_space_node(&C);
  bke::bNodeTreeType *treetype = bke::node_tree_type_find(snode->tree_idname);
  bNodeTree *ntree = snode->nodetree;
  ID *id = snode->id, *from = snode->from;

  /* Check the tree type. */
  if (!treetype || (treetype->poll && !treetype->poll(&C, treetype))) {
    /* Invalid tree type, skip.
     * NOTE: not resetting the node path here, invalid #bNodeTreeType
     * may still be registered at a later point. */
    return;
  }

  if (snode->nodetree && !STREQ(snode->nodetree->idname, snode->tree_idname)) {
    /* Current tree does not match selected type, clear tree path. */
    ntree = nullptr;
    id = nullptr;
    from = nullptr;
  }

  if (!(snode->flag & SNODE_PIN) || ntree == nullptr) {
    if (treetype->get_from_context) {
      /* Reset and update from context. */
      ntree = nullptr;
      id = nullptr;
      from = nullptr;

      treetype->get_from_context(&C, treetype, &ntree, &id, &from);
    }
  }

  if (snode->nodetree != ntree || snode->id != id || snode->from != from ||
      (snode->treepath.last == nullptr && ntree))
  {
    ScrArea *area = CTX_wm_area(&C);
    ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
    ED_node_tree_start(region, snode, ntree, id, from);
  }
}

}  // namespace blender::ed::space_node

void ED_node_set_active(
    Main *bmain, SpaceNode *snode, bNodeTree *ntree, bNode *node, bool *r_active_texture_changed)
{
  if (r_active_texture_changed) {
    *r_active_texture_changed = false;
  }

  blender::bke::node_set_active(*ntree, *node);
  if (node->type_legacy == NODE_GROUP) {
    return;
  }

  const bool was_output = (node->flag & NODE_DO_OUTPUT) != 0;
  bool do_update = false;

  /* Generic node group output: set node as active output. */
  if (node->is_group_output()) {
    for (bNode *node_iter : ntree->all_nodes()) {
      if (node_iter->is_group_output()) {
        node_iter->flag &= ~NODE_DO_OUTPUT;
      }
    }

    node->flag |= NODE_DO_OUTPUT;
    if (!was_output) {
      do_update = true;
      BKE_ntree_update_tag_active_output_changed(ntree);
    }
  }

  /* Tree specific activate calls. */
  if (ntree->type == NTREE_SHADER) {
    if (ELEM(node->type_legacy,
             SH_NODE_OUTPUT_MATERIAL,
             SH_NODE_OUTPUT_WORLD,
             SH_NODE_OUTPUT_LIGHT,
             SH_NODE_OUTPUT_LINESTYLE))
    {
      for (bNode *node_iter : ntree->all_nodes()) {
        if (node_iter->type_legacy == node->type_legacy) {
          node_iter->flag &= ~NODE_DO_OUTPUT;
        }
      }

      node->flag |= NODE_DO_OUTPUT;
      BKE_ntree_update_tag_active_output_changed(ntree);
    }

    BKE_main_ensure_invariants(*bmain, ntree->id);

    if (node->flag & NODE_ACTIVE_TEXTURE) {
      /* If active texture changed, free GLSL materials. */
      LISTBASE_FOREACH (Material *, ma, &bmain->materials) {
        if (ma->nodetree && blender::bke::node_tree_contains_tree(*ma->nodetree, *ntree)) {
          GPU_material_free(&ma->gpumaterial);

          /* Sync to active texpaint slot, otherwise we can end up painting on a different slot
           * than we are looking at. */
          if (ma->texpaintslot) {
            if (node->id != nullptr && GS(node->id->name) == ID_IM) {
              Image *image = (Image *)node->id;
              for (int i = 0; i < ma->tot_slots; i++) {
                if (ma->texpaintslot[i].ima == image) {
                  ma->paint_active_slot = i;
                }
              }
            }
          }
        }
      }

      LISTBASE_FOREACH (World *, wo, &bmain->worlds) {
        if (wo->nodetree && blender::bke::node_tree_contains_tree(*wo->nodetree, *ntree)) {
          GPU_material_free(&wo->gpumaterial);
        }
      }

      /* Sync to Image Editor under the following conditions:
       * - current image is not pinned
       * - current image is not a Render Result or ViewerNode (want to keep looking at these) */
      if (node->id != nullptr && GS(node->id->name) == ID_IM) {
        Image *image = (Image *)node->id;
        ED_space_image_sync(bmain, image, true);
      }

      if (r_active_texture_changed) {
        *r_active_texture_changed = true;
      }
      BKE_main_ensure_invariants(*bmain, ntree->id);
      WM_main_add_notifier(NC_IMAGE, nullptr);
    }

    WM_main_add_notifier(NC_MATERIAL | ND_NODES, node->id);
  }
  else if (ntree->type == NTREE_COMPOSIT) {
    /* Make active viewer, currently only one is supported. */
    if (node->type_legacy == CMP_NODE_VIEWER) {
      for (bNode *node_iter : ntree->all_nodes()) {
        if (node_iter->type_legacy == CMP_NODE_VIEWER) {
          node_iter->flag &= ~NODE_DO_OUTPUT;
        }
      }

      node->flag |= NODE_DO_OUTPUT;
      if (was_output == 0) {
        BKE_ntree_update_tag_active_output_changed(ntree);
        BKE_main_ensure_invariants(*bmain, ntree->id);
      }
    }
    else if (do_update) {
      BKE_main_ensure_invariants(*bmain, ntree->id);
    }
  }
  else if (ntree->type == NTREE_GEOMETRY) {
    if (node->type_legacy == GEO_NODE_VIEWER) {
      if ((node->flag & NODE_DO_OUTPUT) == 0) {
        for (bNode *node_iter : ntree->all_nodes()) {
          if (node_iter->type_legacy == GEO_NODE_VIEWER) {
            node_iter->flag &= ~NODE_DO_OUTPUT;
          }
        }
        node->flag |= NODE_DO_OUTPUT;
      }
      blender::ed::viewer_path::activate_geometry_node(*bmain, *snode, *node);
    }
  }
}

void ED_node_post_apply_transform(bContext * /*C*/, bNodeTree * /*ntree*/)
{
  /* XXX This does not work due to layout functions relying on node->block,
   * which only exists during actual drawing. Can we rely on valid draw_bounds rects?
   */
  /* make sure nodes have correct bounding boxes after transform */
  // node_update_nodetree(C, ntree, 0.0f, 0.0f);
}

/** \} */

namespace blender::ed::space_node {

/* -------------------------------------------------------------------- */
/** \name Node Generic
 * \{ */

static bool socket_is_occluded(const float2 &location,
                               const bNode &node_the_socket_belongs_to,
                               const Span<bNode *> sorted_nodes)
{
  for (bNode *node : sorted_nodes) {
    if (node == &node_the_socket_belongs_to) {
      /* Nodes after this one are underneath and can't occlude the socket. */
      return false;
    }

    rctf socket_hitbox;
    const float socket_hitbox_radius = NODE_SOCKSIZE - 0.1f * U.widget_unit;
    BLI_rctf_init_pt_radius(&socket_hitbox, location, socket_hitbox_radius);
    if (BLI_rctf_inside_rctf(&node->runtime->draw_bounds, &socket_hitbox)) {
      return true;
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Size Widget Operator
 * \{ */

struct NodeSizeWidget {
  float mxstart, mystart;
  float oldlocx, oldlocy;
  float oldwidth, oldheight;
  int directions;
  bool precision, snap_to_grid;
};

static void node_resize_init(
    bContext *C, wmOperator *op, const float2 &cursor, const bNode *node, NodeResizeDirection dir)
{
  Scene *scene = CTX_data_scene(C);
  NodeSizeWidget *nsw = MEM_callocN<NodeSizeWidget>(__func__);

  op->customdata = nsw;

  nsw->mxstart = cursor.x;
  nsw->mystart = cursor.y;

  /* store old */
  nsw->oldlocx = node->location[0];
  nsw->oldlocy = node->location[1];
  nsw->oldwidth = node->width;
  nsw->oldheight = node->height;
  nsw->directions = dir;
  nsw->snap_to_grid = scene->toolsettings->snap_flag_node;

  WM_cursor_modal_set(CTX_wm_window(C), node_get_resize_cursor(dir));
  /* add modal handler */
  WM_event_add_modal_handler(C, op);
}

static void node_resize_exit(bContext *C, wmOperator *op, bool cancel)
{
  NodeSizeWidget *nsw = (NodeSizeWidget *)op->customdata;

  WM_cursor_modal_restore(CTX_wm_window(C));

  /* Restore old data on cancel. */
  if (cancel) {
    SpaceNode *snode = CTX_wm_space_node(C);
    bNode *node = bke::node_get_active(*snode->edittree);

    node->location[0] = nsw->oldlocx;
    node->location[1] = nsw->oldlocy;
    node->width = nsw->oldwidth;
    node->height = nsw->oldheight;
  }

  MEM_freeN(nsw);
  op->customdata = nullptr;
}

enum class NodeResizeAction : int {
  Begin = 0,
  Cancel = 1,
  SnapInvertOn = 2,
  SnapInvertOff = 3,
};

wmKeyMap *node_resize_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {int(NodeResizeAction::Begin), "BEGIN", 0, "Resize Node", ""},
      {int(NodeResizeAction::Cancel), "CANCEL", 0, "Cancel", ""},
      {int(NodeResizeAction::SnapInvertOn), "SNAP_INVERT_ON", 0, "Snap Invert", ""},
      {int(NodeResizeAction::SnapInvertOff), "SNAP_INVERT_OFF", 0, "Snap Invert", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Node Resize Modal Map");

  if (keymap && keymap->modal_items) {
    return nullptr;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Node Resize Modal Map", modal_items);

  WM_modalkeymap_assign(keymap, "NODE_OT_resize");

  return keymap;
}

/* Compute the nearest 1D coordinate corresponding to the nearest grid in node editors. */
static float nearest_node_grid_coord(float co)
{
  /* Size and location of nodes are independent of UI scale, so grid size should be independent of
   * UI scale as well. */
  float grid_size = grid_size_get() / UI_SCALE_FAC;
  float rest = fmod(co, grid_size);
  float offset = rest - grid_size / 2 >= 0 ? grid_size : 0;

  return co - rest + offset;
}

static wmOperatorStatus node_resize_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  bNode *node = bke::node_get_active(*snode->edittree);
  NodeSizeWidget *nsw = (NodeSizeWidget *)op->customdata;

  if (event->type == EVT_MODAL_MAP) {
    switch (NodeResizeAction(event->val)) {
      case NodeResizeAction::Begin: {
        return OPERATOR_RUNNING_MODAL;
      }
      case NodeResizeAction::Cancel: {
        node_resize_exit(C, op, true);
        ED_region_tag_redraw(region);
        return OPERATOR_CANCELLED;
      }
      case NodeResizeAction::SnapInvertOn:
      case NodeResizeAction::SnapInvertOff: {
        nsw->snap_to_grid = !nsw->snap_to_grid;
        return OPERATOR_RUNNING_MODAL;
      }
    }
  }

  switch (event->type) {
    case MOUSEMOVE: {
      int2 mval;
      WM_event_drag_start_mval(event, region, mval);
      float mx, my;
      UI_view2d_region_to_view(&region->v2d, mval.x, mval.y, &mx, &my);
      const float dx = (mx - nsw->mxstart) / UI_SCALE_FAC;
      const float dy = (my - nsw->mystart) / UI_SCALE_FAC;

      if (node) {
        float *pwidth = &node->width;
        float *pheight = &node->height;
        float oldwidth = nsw->oldwidth;
        float widthmin = node->typeinfo->minwidth;
        float widthmax = node->typeinfo->maxwidth;

        {
          if (nsw->directions & NODE_RESIZE_RIGHT) {
            *pwidth = oldwidth + dx;

            if (nsw->snap_to_grid) {
              *pwidth = nearest_node_grid_coord(*pwidth);
            }
            CLAMP(*pwidth, widthmin, widthmax);
          }
          if (nsw->directions & NODE_RESIZE_LEFT) {
            float locmax = nsw->oldlocx + oldwidth;
            *pwidth = oldwidth - dx;

            if (nsw->snap_to_grid) {
              *pwidth = nearest_node_grid_coord(*pwidth);
            }
            CLAMP(*pwidth, widthmin, widthmax);
            node->location[0] = locmax - *pwidth;
          }
        }

        /* Height works the other way round. */
        {
          float heightmin = UI_SCALE_FAC * node->typeinfo->minheight;
          float heightmax = UI_SCALE_FAC * node->typeinfo->maxheight;
          if (nsw->directions & NODE_RESIZE_TOP) {
            float locmin = nsw->oldlocy - nsw->oldheight;
            *pheight = nsw->oldheight + dy;

            if (nsw->snap_to_grid) {
              *pheight = nearest_node_grid_coord(*pheight);
            }
            CLAMP(*pheight, heightmin, heightmax);
            node->location[1] = locmin + *pheight;
          }
          if (nsw->directions & NODE_RESIZE_BOTTOM) {
            *pheight = nsw->oldheight - dy;

            if (nsw->snap_to_grid) {
              *pheight = nearest_node_grid_coord(*pheight);
            }
            CLAMP(*pheight, heightmin, heightmax);
          }
        }
      }

      ED_region_tag_redraw(region);

      break;
    }
    case LEFTMOUSE:
    case MIDDLEMOUSE:
    case RIGHTMOUSE: {
      if (event->val == KM_RELEASE) {
        node_resize_exit(C, op, false);
        ED_node_post_apply_transform(C, snode->edittree);

        return OPERATOR_FINISHED;
      }
      break;
    }
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus node_resize_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  const bNode *node = bke::node_get_active(*snode->edittree);

  if (node == nullptr) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  /* Convert mouse coordinates to `v2d` space. */
  float2 cursor;
  int2 mval;
  WM_event_drag_start_mval(event, region, mval);
  UI_view2d_region_to_view(&region->v2d, mval.x, mval.y, &cursor.x, &cursor.y);
  const NodeResizeDirection dir = node_get_resize_direction(*snode, node, cursor.x, cursor.y);
  if (dir == NODE_RESIZE_NONE) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  node_resize_init(C, op, cursor, node, dir);
  return OPERATOR_RUNNING_MODAL;
}

static void node_resize_cancel(bContext *C, wmOperator *op)
{
  node_resize_exit(C, op, true);
}

void NODE_OT_resize(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Resize Node";
  ot->idname = "NODE_OT_resize";
  ot->description = "Resize a node";

  /* API callbacks. */
  ot->invoke = node_resize_invoke;
  ot->modal = node_resize_modal;
  ot->poll = ED_operator_node_active;
  ot->cancel = node_resize_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Hidden Sockets
 * \{ */

bool node_has_hidden_sockets(bNode *node)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (sock->flag & SOCK_HIDDEN) {
      return true;
    }
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    if (sock->flag & SOCK_HIDDEN) {
      return true;
    }
  }
  return false;
}

void node_set_hidden_sockets(bNode *node, int set)
{
  /* The Reroute node is the socket itself, do not hide this. */
  if (node->is_reroute()) {
    return;
  }

  if (set == 0) {
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      sock->flag &= ~SOCK_HIDDEN;
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      sock->flag &= ~SOCK_HIDDEN;
    }
  }
  else {
    /* Hide unused sockets. */
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      if (sock->link == nullptr) {
        sock->flag |= SOCK_HIDDEN;
      }
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      if ((sock->flag & SOCK_IS_LINKED) == 0) {
        sock->flag |= SOCK_HIDDEN;
      }
    }
  }
}

bool node_is_previewable(const SpaceNode &snode, const bNodeTree &ntree, const bNode &node)
{
  if (!(snode.overlay.flag & SN_OVERLAY_SHOW_OVERLAYS) ||
      !(snode.overlay.flag & SN_OVERLAY_SHOW_PREVIEWS))
  {
    return false;
  }
  if (ntree.type == NTREE_SHADER) {
    return USER_EXPERIMENTAL_TEST(&U, use_shader_node_previews) && !node.is_frame() &&
           snode.shaderfrom == SNODE_SHADER_OBJECT;
  }
  return node.typeinfo->flag & NODE_PREVIEW;
}

static bool cursor_isect_multi_input_socket(const float2 &cursor, const bNodeSocket &socket)
{
  const float node_socket_height = node_socket_calculate_height(socket);
  const float2 location = socket.runtime->location;
  /* `.xmax = socket->location[0] + NODE_SOCKSIZE * 5.5f`
   * would be the same behavior as for regular sockets.
   * But keep it smaller because for multi-input socket you
   * sometimes want to drag the link to the other side, if you may
   * accidentally pick the wrong link otherwise. */
  rctf multi_socket_rect;
  BLI_rctf_init(&multi_socket_rect,
                location.x - NODE_SOCKSIZE * 4.0f,
                location.x + NODE_SOCKSIZE * 2.0f,
                location.y - node_socket_height,
                location.y + node_socket_height);
  if (BLI_rctf_isect_pt(&multi_socket_rect, cursor.x, cursor.y)) {
    return true;
  }
  return false;
}

bNodeSocket *node_find_indicated_socket(SpaceNode &snode,
                                        ARegion &region,
                                        const float2 &cursor,
                                        const eNodeSocketInOut in_out)
{
  const float view2d_scale = UI_view2d_scale_get_x(&region.v2d);
  const float max_distance = NODE_SOCKSIZE + std::clamp(20.0f / view2d_scale, 5.0f, 30.0f);
  const float padded_socket_size = NODE_SOCKSIZE + 4;

  bNodeTree &tree = *snode.edittree;
  tree.ensure_topology_cache();

  const Array<bNode *> sorted_nodes = tree_draw_order_calc_nodes_reversed(tree);
  if (sorted_nodes.is_empty()) {
    return nullptr;
  }

  float best_distance = FLT_MAX;
  bNodeSocket *best_socket = nullptr;

  auto update_best_socket = [&](bNodeSocket *socket, const float distance) {
    if (socket_is_occluded(socket->runtime->location, socket->owner_node(), sorted_nodes)) {
      return;
    }
    if (distance < best_distance) {
      best_distance = distance;
      best_socket = socket;
    }
  };

  for (bNode *node : sorted_nodes) {
    const bool node_collapsed = node->flag & NODE_COLLAPSED;
    if (!node->is_reroute() && !node_collapsed &&
        node->runtime->draw_bounds.ymax - cursor.y < NODE_DY)
    {
      /* Don't pick socket when cursor is over node header. This allows the user to always resize
       * by dragging on the left and right side of the header. */
      continue;
    }
    if (in_out & SOCK_IN) {
      for (bNodeSocket *sock : node->input_sockets()) {
        if (!sock->is_icon_visible()) {
          continue;
        }
        const float2 location = sock->runtime->location;
        const float distance = math::distance(location, cursor);
        if (sock->flag & SOCK_MULTI_INPUT && !node_collapsed) {
          if (cursor_isect_multi_input_socket(cursor, *sock)) {
            update_best_socket(sock, distance);
            continue;
          }
        }
        if (distance < max_distance) {
          if (node_collapsed) {
            if ((cursor.x - location.x > NODE_SOCKSIZE) ||
                ((location.x < cursor.x) && (cursor.x - location.x <= padded_socket_size) &&
                 (abs(location.y - cursor.y) > NODE_SOCKSIZE)))
            {
              /* Needed to be able to resize collapsed nodes. */
              continue;
            }
          }
          update_best_socket(sock, distance);
        }
      }
    }
    if (in_out & SOCK_OUT) {
      for (bNodeSocket *sock : node->output_sockets()) {
        if (!sock->is_icon_visible()) {
          continue;
        }
        const float2 location = sock->runtime->location;
        const float distance = math::distance(location, cursor);
        if (distance < max_distance) {
          if (node_collapsed) {
            if ((location.x - cursor.x > NODE_SOCKSIZE) ||
                ((location.x > cursor.x) && (location.x - cursor.x <= padded_socket_size) &&
                 (abs(location.y - cursor.y) > NODE_SOCKSIZE)))
            {
              /* Needed to be able to resize collapsed nodes. */
              continue;
            }
          }
          update_best_socket(sock, distance);
        }
      }
    }
  }

  return best_socket;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Link Dimming
 * \{ */

float node_link_dim_factor(const View2D &v2d, const bNodeLink &link)
{
  if (link.fromsock == nullptr || link.tosock == nullptr) {
    return 1.0f;
  }
  if (link.flag & NODE_LINK_INSERT_TARGET_INVALID) {
    return 0.2f;
  }

  const float2 from = link.fromsock->runtime->location;
  const float2 to = link.tosock->runtime->location;

  const float min_endpoint_distance = std::min(
      std::max(BLI_rctf_length_x(&v2d.cur, from.x), BLI_rctf_length_y(&v2d.cur, from.y)),
      std::max(BLI_rctf_length_x(&v2d.cur, to.x), BLI_rctf_length_y(&v2d.cur, to.y)));

  if (min_endpoint_distance == 0.0f) {
    return 1.0f;
  }
  const float viewport_width = BLI_rctf_size_x(&v2d.cur);
  return std::clamp(1.0f - min_endpoint_distance / viewport_width * 10.0f, 0.05f, 1.0f);
}

bool node_link_is_hidden_or_dimmed(const View2D &v2d, const bNodeLink &link)
{
  return bke::node_link_is_hidden(link) || node_link_dim_factor(v2d, link) < 0.5f;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Duplicate Operator
 * \{ */

static void node_duplicate_reparent_recursive(bNodeTree *ntree,
                                              const Map<bNode *, bNode *> &node_map,
                                              bNode *node)
{
  bNode *parent;

  node->flag |= NODE_TEST;

  /* Find first selected parent. */
  for (parent = node->parent; parent; parent = parent->parent) {
    if (parent->flag & SELECT) {
      if (!(parent->flag & NODE_TEST)) {
        node_duplicate_reparent_recursive(ntree, node_map, parent);
      }
      break;
    }
  }
  /* Reparent node copy to parent copy. */
  if (parent) {
    bke::node_detach_node(*ntree, *node_map.lookup(node));
    bke::node_attach_node(*ntree, *node_map.lookup(node), *node_map.lookup(parent));
  }
}

void remap_node_pairing(bNodeTree &dst_tree, const Map<const bNode *, bNode *> &node_map)
{
  /* We don't have the old tree for looking up output nodes by ID,
   * so we have to build a map first to find copied output nodes in the new tree. */
  Map<int32_t, bNode *> dst_output_node_map;
  for (const auto &item : node_map.items()) {
    if (bke::all_zone_output_node_types().contains(item.key->type_legacy)) {
      dst_output_node_map.add_new(item.key->identifier, item.value);
    }
  }

  for (bNode *dst_node : node_map.values()) {
    if (bke::all_zone_input_node_types().contains(dst_node->type_legacy)) {
      const bke::bNodeZoneType &zone_type = *bke::zone_type_by_node_type(dst_node->type_legacy);
      int &output_node_id = zone_type.get_corresponding_output_id(*dst_node);
      if (const bNode *output_node = dst_output_node_map.lookup_default(output_node_id, nullptr)) {
        output_node_id = output_node->identifier;
      }
      else {
        output_node_id = 0;
        blender::nodes::update_node_declaration_and_sockets(dst_tree, *dst_node);
      }
    }
  }
}

static wmOperatorStatus node_duplicate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  const bool keep_inputs = RNA_boolean_get(op->ptr, "keep_inputs");
  bool linked = RNA_boolean_get(op->ptr, "linked") || ((U.dupflag & USER_DUP_NTREE) == 0);
  const bool dupli_node_tree = !linked;

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  Map<bNode *, bNode *> node_map;
  Map<const bNodeSocket *, bNodeSocket *> socket_map;
  Map<const ID *, ID *> duplicated_node_groups;

  node_select_paired(*ntree);

  for (bNode *node : get_selected_nodes(*ntree)) {
    bNode *new_node = bke::node_copy_with_mapping(
        ntree, *node, LIB_ID_COPY_DEFAULT, std::nullopt, std::nullopt, socket_map);
    node_map.add_new(node, new_node);

    if (node->id && dupli_node_tree && !ID_IS_LINKED(node->id)) {
      ID *new_group = duplicated_node_groups.lookup_or_add_cb(node->id, [&]() {
        ID *new_group = BKE_id_copy(bmain, node->id);
        /* Remove user added by copying. */
        id_us_min(new_group);
        return new_group;
      });
      id_us_plus(new_group);
      id_us_min(new_node->id);
      new_node->id = new_group;
    }
  }

  if (node_map.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  /* Copy links between selected nodes. */
  bNodeLink *lastlink = (bNodeLink *)ntree->links.last;
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    /* This creates new links between copied nodes. If keep_inputs is set, also copies input links
     * from unselected (when fromnode is null)! */
    if (link->tonode && (link->tonode->flag & NODE_SELECT) &&
        (keep_inputs || (link->fromnode && (link->fromnode->flag & NODE_SELECT))))
    {
      bNodeLink *newlink = MEM_callocN<bNodeLink>("bNodeLink");
      newlink->flag = link->flag;
      newlink->tonode = node_map.lookup(link->tonode);
      newlink->tosock = socket_map.lookup(link->tosock);

      if (link->tosock->flag & SOCK_MULTI_INPUT) {
        newlink->multi_input_sort_id = link->multi_input_sort_id;
      }

      if (link->fromnode && (link->fromnode->flag & NODE_SELECT)) {
        newlink->fromnode = node_map.lookup(link->fromnode);
        newlink->fromsock = socket_map.lookup(link->fromsock);
      }
      else {
        /* Input node not copied, this keeps the original input linked. */
        newlink->fromnode = link->fromnode;
        newlink->fromsock = link->fromsock;
      }

      BLI_addtail(&ntree->links, newlink);
    }

    /* Make sure we don't copy new links again. */
    if (link == lastlink) {
      break;
    }
  }

  for (bNode *node : node_map.values()) {
    blender::bke::node_declaration_ensure(*ntree, *node);
  }

  ntree->ensure_topology_cache();
  for (bNode *node : node_map.values()) {
    update_multi_input_indices_for_removed_links(*node);
  }

  /* Clear flags for recursive depth-first iteration. */
  for (bNode *node : ntree->all_nodes()) {
    node->flag &= ~NODE_TEST;
  }
  /* Reparent copied nodes. */
  for (bNode *node : node_map.keys()) {
    if (!(node->flag & NODE_TEST)) {
      node_duplicate_reparent_recursive(ntree, node_map, node);
    }
  }

  {
    /* Use temporary map that has const key, because that's what the function below expects. */
    Map<const bNode *, bNode *> const_node_map;
    for (const auto item : node_map.items()) {
      const_node_map.add(item.key, item.value);
    }
    remap_node_pairing(*ntree, const_node_map);
  }

  /* Deselect old nodes, select the copies instead. */
  for (const auto item : node_map.items()) {
    bNode *src_node = item.key;
    bNode *dst_node = item.value;

    bke::node_set_selected(*src_node, false);
    src_node->flag &= ~(NODE_ACTIVE | NODE_ACTIVE_TEXTURE);
    bke::node_set_selected(*dst_node, true);
  }

  tree_draw_order_update(*snode->edittree);
  BKE_main_ensure_invariants(*bmain, snode->edittree->id);
  return OPERATOR_FINISHED;
}

void NODE_OT_duplicate(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Duplicate Nodes";
  ot->description = "Duplicate selected nodes";
  ot->idname = "NODE_OT_duplicate";

  /* API callbacks. */
  ot->exec = node_duplicate_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "keep_inputs", false, "Keep Inputs", "Keep the input links to duplicated nodes");

  prop = RNA_def_boolean(ot->srna,
                         "linked",
                         true,
                         "Linked",
                         "Duplicate node but not node trees, linking to the original data");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* Goes over all scenes, reads render layers. */
static wmOperatorStatus node_read_viewlayers_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  Scene *curscene = CTX_data_scene(C);
  bNodeTree &edit_tree = *snode->edittree;

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  /* first tag scenes unread */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    scene->id.tag |= ID_TAG_DOIT;
  }

  for (bNode *node : edit_tree.all_nodes()) {
    if ((node->type_legacy == CMP_NODE_R_LAYERS) ||
        (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
         node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_RENDER))
    {
      ID *id = node->id;
      if (id == nullptr) {
        continue;
      }
      if (id->tag & ID_TAG_DOIT) {
        RE_ReadRenderResult(curscene, (Scene *)id);
        ntreeCompositTagRender((Scene *)id);
        id->tag &= ~ID_TAG_DOIT;
      }
    }
  }

  BKE_main_ensure_invariants(*bmain, edit_tree.id);

  return OPERATOR_FINISHED;
}

void NODE_OT_read_viewlayers(wmOperatorType *ot)
{
  ot->name = "Read View Layers";
  ot->idname = "NODE_OT_read_viewlayers";
  ot->description = "Read all render layers of all used scenes";

  ot->exec = node_read_viewlayers_exec;

  ot->poll = composite_node_active;
}

wmOperatorStatus node_render_changed_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *sce = CTX_data_scene(C);

  /* This is actually a test whether scene is used by the compositor or not.
   * All the nodes are using same render result, so there is no need to do
   * anything smart about check how exactly scene is used. */
  bNode *node = nullptr;
  for (bNode *node_iter : sce->compositing_node_group->all_nodes()) {
    if (node_iter->id == (ID *)sce) {
      node = node_iter;
      break;
    }
  }

  if (node) {
    ViewLayer *view_layer = (ViewLayer *)BLI_findlink(&sce->view_layers, node->custom1);

    if (view_layer) {
      PointerRNA op_ptr;

      WM_operator_properties_create(&op_ptr, "RENDER_OT_render");
      RNA_string_set(&op_ptr, "layer", view_layer->name);
      RNA_string_set(&op_ptr, "scene", sce->id.name + 2);

      /* To keep keyframe positions. */
      sce->r.scemode |= R_NO_FRAME_UPDATE;

      WM_operator_name_call(
          C, "RENDER_OT_render", wm::OpCallContext::InvokeDefault, &op_ptr, nullptr);

      WM_operator_properties_free(&op_ptr);

      return OPERATOR_FINISHED;
    }
  }
  return OPERATOR_CANCELLED;
}

void NODE_OT_render_changed(wmOperatorType *ot)
{
  ot->name = "Render Changed Layer";
  ot->idname = "NODE_OT_render_changed";
  ot->description = "Render current scene, when input node's layer has been changed";

  ot->exec = node_render_changed_exec;

  ot->poll = composite_node_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Collapse Operator
 * \{ */

/**
 * Toggles the flag on all selected nodes. If the flag is set on all nodes it is unset.
 * If the flag is not set on all nodes, it is set. If tag_update is true, the nodes will be tagged
 * for a property change update.
 */
static void node_flag_toggle_exec(SpaceNode *snode, int toggle_flag, const bool tag_update = false)
{
  int tot_eq = 0, tot_neq = 0;

  for (bNode *node : snode->edittree->all_nodes()) {
    if (node->flag & SELECT) {

      if (toggle_flag == NODE_PREVIEW && !node_is_previewable(*snode, *snode->edittree, *node)) {
        continue;
      }
      if (toggle_flag == NODE_OPTIONS &&
          !(node->typeinfo->draw_buttons || node->typeinfo->draw_buttons_ex))
      {
        continue;
      }

      if (node->flag & toggle_flag) {
        tot_eq++;
      }
      else {
        tot_neq++;
      }
    }
  }
  for (bNode *node : snode->edittree->all_nodes()) {
    if (node->flag & SELECT) {

      if (toggle_flag == NODE_PREVIEW && !node_is_previewable(*snode, *snode->edittree, *node)) {
        continue;
      }
      if (toggle_flag == NODE_OPTIONS &&
          !(node->typeinfo->draw_buttons || node->typeinfo->draw_buttons_ex))
      {
        continue;
      }

      if ((tot_eq && tot_neq) || tot_eq == 0) {
        node->flag |= toggle_flag;
      }
      else {
        node->flag &= ~toggle_flag;
      }

      if (tag_update) {
        BKE_ntree_update_tag_node_property(snode->edittree, node);
      }
    }
  }
}

static wmOperatorStatus node_collapse_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  /* Sanity checking (poll callback checks this already). */
  if ((snode == nullptr) || (snode->edittree == nullptr)) {
    return OPERATOR_CANCELLED;
  }

  node_flag_toggle_exec(snode, NODE_COLLAPSED);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_collapse_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Collapse";
  ot->description = "Toggle collapsing of selected nodes";
  ot->idname = "NODE_OT_hide_toggle";

  /* callbacks */
  ot->exec = node_collapse_toggle_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus node_preview_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  /* Sanity checking (poll callback checks this already). */
  if ((snode == nullptr) || (snode->edittree == nullptr)) {
    return OPERATOR_CANCELLED;
  }

  node_flag_toggle_exec(snode, NODE_PREVIEW, true);

  WM_event_add_notifier(C, NC_NODE | NA_EDITED, &snode->edittree->id);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, &snode->edittree->id);

  BKE_main_ensure_invariants(*CTX_data_main(C), snode->edittree->id);

  return OPERATOR_FINISHED;
}

static bool node_previewable(bContext *C)
{
  if (ED_operator_node_active(C)) {
    SpaceNode *snode = CTX_wm_space_node(C);
    if (ED_node_supports_preview(snode)) {
      return true;
    }
  }
  return false;
}

void NODE_OT_preview_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Node Preview";
  ot->description = "Toggle preview display for selected nodes";
  ot->idname = "NODE_OT_preview_toggle";

  /* callbacks */
  ot->exec = node_preview_toggle_exec;
  ot->poll = node_previewable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus node_activate_viewer_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  PointerRNA ptr = CTX_data_pointer_get(C, "node");
  Main *bmain = CTX_data_main(C);
  bNodeTree *ntree = nullptr;
  bNode *node = nullptr;

  if (ptr.data) {
    node = static_cast<bNode *>(ptr.data);
    ntree = reinterpret_cast<bNodeTree *>(ptr.owner_id);
  }
  else if (snode && snode->edittree) {
    ntree = snode->edittree;
    node = bke::node_get_active(*ntree);
  }

  if (!node) {
    return OPERATOR_CANCELLED;
  }

  if (node->is_type("CompositorNodeViewer")) {
    for (bNode *other_node : ntree->all_nodes()) {
      if (other_node->type_legacy == node->type_legacy) {
        other_node->flag &= ~NODE_DO_OUTPUT;
      }
      node->flag |= NODE_DO_OUTPUT;

      WM_main_add_notifier(NC_NODE | NA_EDITED, &ntree->id);
      WM_main_add_notifier(NC_SCENE | ND_NODES, &ntree->id);
    }
  }
  else if (node->is_type("GeometryNodeViewer")) {
    /* Geometry nodes viewers don't rely on NODE_DO_OUTPUT flag alone. */
    viewer_path::activate_geometry_node(*bmain, *snode, *node);
  }
  else {
    return OPERATOR_CANCELLED;
  }

  BKE_ntree_update_tag_active_output_changed(snode->edittree);
  BKE_main_ensure_invariants(*bmain, snode->edittree->id);
  return OPERATOR_FINISHED;
}

void NODE_OT_activate_viewer(wmOperatorType *ot)
{
  ot->name = "Activate Viewer Node";
  ot->description = "Activate selected viewer node in compositor and geometry nodes";
  ot->idname = "NODE_OT_activate_viewer";

  ot->exec = node_activate_viewer_exec;
  ot->poll = ED_operator_node_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus test_inline_shader_nodes_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &ntree = *snode.edittree;
  Main &bmain = *CTX_data_main(C);

  bNodeTree *new_tree = bke::node_tree_add_tree(
      &bmain, (StringRef(ntree.id.name) + " Inlined").c_str(), ntree.idname);

  nodes::InlineShaderNodeTreeParams params;
  params.allow_preserving_repeat_zones = false;
  nodes::inline_shader_node_tree(ntree, *new_tree, params);
  bNode *group_node = bke::node_add_node(C, ntree, ntree.typeinfo->group_idname);
  group_node->id = &new_tree->id;
  node_deselect_all(ntree);
  bke::node_set_selected(*group_node, true);
  bke::node_set_active(ntree, *group_node);

  BKE_main_ensure_invariants(bmain);

  return OPERATOR_FINISHED;
}

void NODE_OT_test_inlining_shader_nodes(wmOperatorType *ot)
{
  ot->name = "Test Inlining Shader Nodes";
  ot->description = "Create a new inlined shader node tree as is consumed by renderers";
  ot->idname = "NODE_OT_test_inlining_shader_nodes";

  ot->exec = test_inline_shader_nodes_exec;
  ot->poll = ED_operator_node_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus node_deactivate_viewer_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  WorkSpace &workspace = *CTX_wm_workspace(C);

  bNode *active_viewer = viewer_path::find_geometry_nodes_viewer(workspace.viewer_path, snode);

  for (bNode *node : snode.edittree->all_nodes()) {
    if (node->type_legacy != GEO_NODE_VIEWER) {
      continue;
    }
    if (node == active_viewer) {
      node->flag &= ~NODE_DO_OUTPUT;
      BKE_ntree_update_tag_node_property(snode.edittree, node);
      /* At most, only one viewer is active so break early. */
      break;
    }
  }

  BKE_main_ensure_invariants(*CTX_data_main(C), snode.edittree->id);

  return OPERATOR_FINISHED;
}

void NODE_OT_deactivate_viewer(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Deactivate Viewer Node";
  ot->description = "Deactivate selected viewer node in geometry nodes";
  ot->idname = __func__;

  /* callbacks */
  ot->exec = node_deactivate_viewer_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus node_toggle_viewer_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  WorkSpace *workspace = CTX_wm_workspace(C);
  PointerRNA ptr = CTX_data_pointer_get(C, "node");
  bNode *node = nullptr;
  bNodeTree *ntree = nullptr;
  wmOperatorStatus ret = OPERATOR_FINISHED;

  if (ptr.data) {
    node = static_cast<bNode *>(ptr.data);
    ntree = reinterpret_cast<bNodeTree *>(ptr.owner_id);
  }
  else if (snode && snode->edittree) {
    ntree = snode->edittree;
    node = bke::node_get_active(*ntree);
  }

  if (!node) {
    return OPERATOR_CANCELLED;
  }

  bNode *active_viewer = viewer_path::find_geometry_nodes_viewer(workspace->viewer_path, *snode);
  if (node == active_viewer) {
    ret = WM_operator_name_call(
        C, "NODE_OT_deactivate_viewer", wm::OpCallContext::InvokeDefault, nullptr, nullptr);
  }
  else {
    ret = WM_operator_name_call(
        C, "NODE_OT_activate_viewer", wm::OpCallContext::InvokeDefault, nullptr, nullptr);
  }

  return ret;
}

void NODE_OT_toggle_viewer(wmOperatorType *ot)
{
  ot->name = "Toggle Viewer Node";
  ot->description = "Toggle selected viewer node in compositor and geometry nodes";
  ot->idname = "NODE_OT_toggle_viewer";

  ot->exec = node_toggle_viewer_exec;
  ot->poll = ED_operator_node_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus node_options_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  /* Sanity checking (poll callback checks this already). */
  if ((snode == nullptr) || (snode->edittree == nullptr)) {
    return OPERATOR_CANCELLED;
  }

  node_flag_toggle_exec(snode, NODE_OPTIONS);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_options_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Node Options";
  ot->description = "Toggle option buttons display for selected nodes";
  ot->idname = "NODE_OT_options_toggle";

  /* callbacks */
  ot->exec = node_options_toggle_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus node_socket_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  /* Sanity checking (poll callback checks this already). */
  if ((snode == nullptr) || (snode->edittree == nullptr)) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  /* Toggle for all selected nodes */
  bool hidden = false;
  for (bNode *node : snode->edittree->all_nodes()) {
    if (node->flag & SELECT) {
      if (node_has_hidden_sockets(node)) {
        hidden = true;
        break;
      }
    }
  }

  for (bNode *node : snode->edittree->all_nodes()) {
    if (node->flag & SELECT) {
      node_set_hidden_sockets(node, !hidden);
    }
  }

  BKE_main_ensure_invariants(*CTX_data_main(C), snode->edittree->id);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);
  /* Hack to force update of the button state after drawing, see #112462. */
  WM_event_add_mousemove(CTX_wm_window(C));

  return OPERATOR_FINISHED;
}

void NODE_OT_hide_socket_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Hidden Node Sockets";
  ot->description = "Toggle unused node socket display";
  ot->idname = "NODE_OT_hide_socket_toggle";

  /* callbacks */
  ot->exec = node_socket_toggle_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Mute Operator
 * \{ */

static wmOperatorStatus node_mute_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  for (bNode *node : snode->edittree->all_nodes()) {
    if ((node->flag & SELECT) && !node->typeinfo->no_muting) {
      node->flag ^= NODE_MUTED;
      BKE_ntree_update_tag_node_mute(snode->edittree, node);
    }
  }

  BKE_main_ensure_invariants(*bmain, snode->edittree->id);

  return OPERATOR_FINISHED;
}

void NODE_OT_mute_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Node Mute";
  ot->description = "Toggle muting of selected nodes";
  ot->idname = "NODE_OT_mute_toggle";

  /* callbacks */
  ot->exec = node_mute_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Delete Operator
 * \{ */

static wmOperatorStatus node_delete_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  /* Delete paired nodes as well. */
  node_select_paired(*snode->edittree);

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & SELECT) {
      bke::node_remove_node(bmain, *snode->edittree, *node, true);
    }
  }

  ED_node_set_active_viewer_key(snode);
  BKE_main_ensure_invariants(*bmain, snode->edittree->id);

  return OPERATOR_FINISHED;
}

void NODE_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete";
  ot->description = "Remove selected nodes";
  ot->idname = "NODE_OT_delete";

  /* API callbacks. */
  ot->exec = node_delete_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Delete with Reconnect Operator
 * \{ */

static wmOperatorStatus node_delete_reconnect_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  /* Delete paired nodes as well. */
  node_select_paired(*snode->edittree);

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & SELECT) {
      blender::bke::node_internal_relink(*snode->edittree, *node);
      bke::node_remove_node(bmain, *snode->edittree, *node, true);

      /* Since this node might have been animated, and that animation data been
       * deleted, a notifier call is necessary to redraw any animation editor. */
      WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
    }
  }

  BKE_main_ensure_invariants(*bmain, snode->edittree->id);

  return OPERATOR_FINISHED;
}

void NODE_OT_delete_reconnect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete with Reconnect";
  ot->description = "Remove nodes and reconnect nodes as if deletion was muted";
  ot->idname = "NODE_OT_delete_reconnect";

  /* API callbacks. */
  ot->exec = node_delete_reconnect_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Copy Node Color Operator
 * \{ */

static wmOperatorStatus node_copy_color_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &ntree = *snode.edittree;

  bNode *active_node = bke::node_get_active(ntree);
  if (!active_node) {
    return OPERATOR_CANCELLED;
  }

  for (bNode *node : ntree.all_nodes()) {
    if (node->flag & NODE_SELECT && node != active_node) {
      if (active_node->flag & NODE_CUSTOM_COLOR) {
        node->flag |= NODE_CUSTOM_COLOR;
        copy_v3_v3(node->color, active_node->color);
      }
      else {
        node->flag &= ~NODE_CUSTOM_COLOR;
      }
    }
  }

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_node_copy_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Color";
  ot->description = "Copy color to all selected nodes";
  ot->idname = "NODE_OT_node_copy_color";

  /* API callbacks. */
  ot->exec = node_copy_color_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Shader Script Update
 * \{ */

static bool node_shader_script_update_poll(bContext *C)
{
  RenderEngineType *type = CTX_data_engine_type(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  /* Test if we have a render engine that supports shaders scripts. */
  if (!(type && type->update_script_node)) {
    return false;
  }

  /* See if we have a shader script node in context. */
  bNode *node = (bNode *)CTX_data_pointer_get_type(C, "node", &RNA_ShaderNodeScript).data;

  if (!node && snode && snode->edittree) {
    node = bke::node_get_active(*snode->edittree);
  }

  if (node && node->type_legacy == SH_NODE_SCRIPT) {
    NodeShaderScript *nss = (NodeShaderScript *)node->storage;

    if (node->id || nss->filepath[0]) {
      return ED_operator_node_editable(C);
    }
  }

  return false;
}

static wmOperatorStatus node_shader_script_update_exec(bContext *C, wmOperator *op)
{
  RenderEngineType *type = CTX_data_engine_type(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  PointerRNA nodeptr = CTX_data_pointer_get_type(C, "node", &RNA_ShaderNodeScript);

  /* setup render engine */
  RenderEngine *engine = RE_engine_create(type);
  engine->reports = op->reports;

  bNodeTree *ntree_base = nullptr;
  bNode *node = nullptr;
  if (nodeptr.data) {
    ntree_base = (bNodeTree *)nodeptr.owner_id;
    node = (bNode *)nodeptr.data;
  }
  else if (snode && snode->edittree) {
    ntree_base = snode->edittree;
    node = bke::node_get_active(*snode->edittree);
  }

  /* Update node. */
  type->update_script_node(engine, ntree_base, node);

  RE_engine_free(engine);

  return OPERATOR_FINISHED;
}

void NODE_OT_shader_script_update(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Script Node Update";
  ot->description = "Update shader script node with new sockets and options from the script";
  ot->idname = "NODE_OT_shader_script_update";

  /* API callbacks. */
  ot->exec = node_shader_script_update_exec;
  ot->poll = node_shader_script_update_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Viewer Border
 * \{ */

static void viewer_border_corner_to_backdrop(SpaceNode *snode,
                                             ARegion *region,
                                             int x,
                                             int y,
                                             int backdrop_width,
                                             int backdrop_height,
                                             float *fx,
                                             float *fy)
{
  float bufx = backdrop_width * snode->zoom;
  float bufy = backdrop_height * snode->zoom;

  *fx = (bufx > 0.0f ? (float(x) - 0.5f * region->winx - snode->xof) / bufx + 0.5f : 0.0f);
  *fy = (bufy > 0.0f ? (float(y) - 0.5f * region->winy - snode->yof) / bufy + 0.5f : 0.0f);
}

static wmOperatorStatus viewer_border_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  void *lock;

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (ibuf) {
    ARegion *region = CTX_wm_region(C);
    SpaceNode *snode = CTX_wm_space_node(C);
    bNodeTree *btree = snode->nodetree;
    rcti rect;
    rctf rectf;

    /* Get border from operator. */
    WM_operator_properties_border_to_rcti(op, &rect);

    /* Convert border to unified space within backdrop image. */
    viewer_border_corner_to_backdrop(
        snode, region, rect.xmin, rect.ymin, ibuf->x, ibuf->y, &rectf.xmin, &rectf.ymin);

    viewer_border_corner_to_backdrop(
        snode, region, rect.xmax, rect.ymax, ibuf->x, ibuf->y, &rectf.xmax, &rectf.ymax);

    /* Clamp coordinates. */
    rectf.xmin = max_ff(rectf.xmin, 0.0f);
    rectf.ymin = max_ff(rectf.ymin, 0.0f);
    rectf.xmax = min_ff(rectf.xmax, 1.0f);
    rectf.ymax = min_ff(rectf.ymax, 1.0f);

    if (rectf.xmin < rectf.xmax && rectf.ymin < rectf.ymax) {
      btree->viewer_border = rectf;

      if (rectf.xmin == 0.0f && rectf.ymin == 0.0f && rectf.xmax == 1.0f && rectf.ymax == 1.0f) {
        btree->flag &= ~NTREE_VIEWER_BORDER;
      }
      else {
        btree->flag |= NTREE_VIEWER_BORDER;
      }

      BKE_main_ensure_invariants(*bmain, btree->id);
      WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);
    }
    else {
      btree->flag &= ~NTREE_VIEWER_BORDER;
    }
  }

  BKE_image_release_ibuf(ima, ibuf, lock);

  return OPERATOR_FINISHED;
}

void NODE_OT_viewer_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Viewer Region";
  ot->description = "Set the boundaries for viewer operations";
  ot->idname = "NODE_OT_viewer_border";

  /* API callbacks. */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = viewer_border_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;
  ot->poll = composite_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_box(ot);
}

static wmOperatorStatus clear_viewer_border_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *btree = snode->nodetree;

  btree->flag &= ~NTREE_VIEWER_BORDER;
  BKE_main_ensure_invariants(*CTX_data_main(C), btree->id);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_clear_viewer_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Viewer Region";
  ot->description = "Clear the boundaries for viewer operations";
  ot->idname = "NODE_OT_clear_viewer_border";

  /* API callbacks. */
  ot->exec = clear_viewer_border_exec;
  ot->poll = composite_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cryptomatte Add Socket
 * \{ */

static wmOperatorStatus node_cryptomatte_add_socket_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  PointerRNA ptr = CTX_data_pointer_get(C, "node");
  bNodeTree *ntree = nullptr;
  bNode *node = nullptr;

  if (ptr.data) {
    node = (bNode *)ptr.data;
    ntree = (bNodeTree *)ptr.owner_id;
  }
  else if (snode && snode->edittree) {
    ntree = snode->edittree;
    node = bke::node_get_active(*snode->edittree);
  }

  if (!node || node->type_legacy != CMP_NODE_CRYPTOMATTE_LEGACY) {
    return OPERATOR_CANCELLED;
  }

  ntreeCompositCryptomatteAddSocket(ntree, node);

  BKE_main_ensure_invariants(*CTX_data_main(C), ntree->id);

  return OPERATOR_FINISHED;
}

void NODE_OT_cryptomatte_layer_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Cryptomatte Socket";
  ot->description = "Add a new input layer to a Cryptomatte node";
  ot->idname = "NODE_OT_cryptomatte_layer_add";

  /* callbacks */
  ot->exec = node_cryptomatte_add_socket_exec;
  ot->poll = composite_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cryptomatte Remove Socket
 * \{ */

static wmOperatorStatus node_cryptomatte_remove_socket_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  PointerRNA ptr = CTX_data_pointer_get(C, "node");
  bNodeTree *ntree = nullptr;
  bNode *node = nullptr;

  if (ptr.data) {
    node = (bNode *)ptr.data;
    ntree = (bNodeTree *)ptr.owner_id;
  }
  else if (snode && snode->edittree) {
    ntree = snode->edittree;
    node = bke::node_get_active(*snode->edittree);
  }

  if (!node || node->type_legacy != CMP_NODE_CRYPTOMATTE_LEGACY) {
    return OPERATOR_CANCELLED;
  }

  if (!ntreeCompositCryptomatteRemoveSocket(ntree, node)) {
    return OPERATOR_CANCELLED;
  }

  BKE_main_ensure_invariants(*CTX_data_main(C), ntree->id);

  return OPERATOR_FINISHED;
}

void NODE_OT_cryptomatte_layer_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Cryptomatte Socket";
  ot->description = "Remove layer from a Cryptomatte node";
  ot->idname = "NODE_OT_cryptomatte_layer_remove";

  /* callbacks */
  ot->exec = node_cryptomatte_remove_socket_exec;
  ot->poll = composite_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender::ed::space_node
