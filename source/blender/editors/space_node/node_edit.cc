/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 */

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_text_types.h"
#include "DNA_world_types.h"

#include "BKE_callbacks.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_workspace.h"

#include "BLI_set.hh"
#include "BLI_string_utf8.h"

#include "BLT_translation.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "ED_image.hh"
#include "ED_node.hh" /* own include */
#include "ED_render.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_viewer_path.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_view2d.hh"

#include "GPU_material.h"

#include "IMB_imbuf_types.h"

#include "NOD_composite.h"
#include "NOD_geometry.hh"
#include "NOD_shader.h"
#include "NOD_socket.hh"
#include "NOD_texture.h"
#include "node_intern.hh" /* own include */

namespace blender::ed::space_node {

#define USE_ESC_COMPO

/* -------------------------------------------------------------------- */
/** \name Composite Job Manager
 * \{ */

enum {
  COM_RECALC_COMPOSITE = 1,
  COM_RECALC_VIEWER = 2,
};

struct CompoJob {
  /* Input parameters. */
  Main *bmain;
  Scene *scene;
  ViewLayer *view_layer;
  bNodeTree *ntree;
  int recalc_flags;
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
};

float node_socket_calculate_height(const bNodeSocket &socket)
{
  float sock_height = NODE_SOCKSIZE * NODE_SOCKSIZE_DRAW_MULIPLIER;
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

static void compo_tag_output_nodes(bNodeTree *nodetree, int recalc_flags)
{
  for (bNode *node : nodetree->all_nodes()) {
    if (node->type == CMP_NODE_COMPOSITE) {
      if (recalc_flags & COM_RECALC_COMPOSITE) {
        node->flag |= NODE_DO_OUTPUT_RECALC;
      }
    }
    else if (ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
      if (recalc_flags & COM_RECALC_VIEWER) {
        node->flag |= NODE_DO_OUTPUT_RECALC;
      }
    }
    else if (node->type == NODE_GROUP) {
      if (node->id) {
        compo_tag_output_nodes((bNodeTree *)node->id, recalc_flags);
      }
    }
  }
}

static int compo_get_recalc_flags(const bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  int recalc_flags = 0;

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    const bScreen *screen = WM_window_get_active_screen(win);

    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->spacetype == SPACE_IMAGE) {
        SpaceImage *sima = (SpaceImage *)area->spacedata.first;
        if (sima->image) {
          if (sima->image->type == IMA_TYPE_R_RESULT) {
            recalc_flags |= COM_RECALC_COMPOSITE;
          }
          else if (sima->image->type == IMA_TYPE_COMPOSITE) {
            recalc_flags |= COM_RECALC_VIEWER;
          }
        }
      }
      else if (area->spacetype == SPACE_NODE) {
        SpaceNode *snode = (SpaceNode *)area->spacedata.first;
        if (snode->flag & SNODE_BACKDRAW) {
          recalc_flags |= COM_RECALC_VIEWER;
        }
      }
    }
  }

  return recalc_flags;
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
      bke::ntreeLocalMerge(cj->bmain, cj->localtree, cj->ntree);
    }

    bke::ntreeFreeTree(cj->localtree);
    MEM_freeN(cj->localtree);
  }
  if (cj->compositor_depsgraph != nullptr) {
    DEG_graph_free(cj->compositor_depsgraph);
  }
  MEM_freeN(cj);
}

/* Only now we copy the nodetree, so adding many jobs while
 * sliding buttons doesn't frustrate. */
static void compo_initjob(void *cjv)
{
  CompoJob *cj = (CompoJob *)cjv;
  Main *bmain = cj->bmain;
  Scene *scene = cj->scene;
  ViewLayer *view_layer = cj->view_layer;

  cj->compositor_depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_RENDER);
  DEG_graph_build_for_compositor_preview(cj->compositor_depsgraph, cj->ntree);

  /* NOTE: Don't update animation to preserve unkeyed changes, this means can not use
   * evaluate_on_framechange. */
  DEG_evaluate_on_refresh(cj->compositor_depsgraph);

  bNodeTree *ntree_eval = (bNodeTree *)DEG_get_evaluated_id(cj->compositor_depsgraph,
                                                            &cj->ntree->id);

  cj->localtree = ntreeLocalize(ntree_eval);

  if (cj->recalc_flags) {
    compo_tag_output_nodes(cj->localtree, cj->recalc_flags);
  }

  cj->re = RE_NewSceneRender(scene);
  RE_system_gpu_context_ensure(cj->re);
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
static void compo_startjob(void *cjv,
                           /* Cannot be const, this function implements wm_jobs_start_callback.
                            * NOLINTNEXTLINE: readability-non-const-parameter. */
                           bool *stop,
                           bool *do_update,
                           float *progress)
{
  CompoJob *cj = (CompoJob *)cjv;
  bNodeTree *ntree = cj->localtree;
  Scene *scene = cj->scene;

  if (scene->use_nodes == false) {
    return;
  }

  cj->stop = stop;
  cj->do_update = do_update;
  cj->progress = progress;

  ntree->runtime->test_break = compo_breakjob;
  ntree->runtime->tbh = cj;
  ntree->runtime->stats_draw = compo_statsdrawjob;
  ntree->runtime->sdh = cj;
  ntree->runtime->progress = compo_progressjob;
  ntree->runtime->prh = cj;
  ntree->runtime->update_draw = compo_redrawjob;
  ntree->runtime->udh = cj;

  BKE_callback_exec_id(cj->bmain, &scene->id, BKE_CB_EVT_COMPOSITE_PRE);

  if ((cj->scene->r.scemode & R_MULTIVIEW) == 0) {
    ntreeCompositExecTree(cj->re, cj->scene, ntree, &cj->scene->r, false, true, "");
  }
  else {
    LISTBASE_FOREACH (SceneRenderView *, srv, &scene->r.views) {
      if (BKE_scene_multiview_is_render_view_active(&scene->r, srv) == false) {
        continue;
      }
      ntreeCompositExecTree(cj->re, cj->scene, ntree, &cj->scene->r, false, true, srv->name);
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
}

static void compo_completejob(void *cjv)
{
  CompoJob *cj = (CompoJob *)cjv;
  Main *bmain = cj->bmain;
  Scene *scene = cj->scene;
  BKE_callback_exec_id(bmain, &scene->id, BKE_CB_EVT_COMPOSITE_POST);
}

/** \} */

}  // namespace blender::ed::space_node

/* -------------------------------------------------------------------- */
/** \name Composite Job C API
 * \{ */

void ED_node_composite_job(const bContext *C, bNodeTree *nodetree, Scene *scene_owner)
{
  using namespace blender::ed::space_node;

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

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
                              "Compositing",
                              WM_JOB_EXCL_RENDER | WM_JOB_PROGRESS,
                              WM_JOB_TYPE_COMPOSITE);
  CompoJob *cj = MEM_cnew<CompoJob>("compo job");

  /* Custom data for preview thread. */
  cj->bmain = bmain;
  cj->scene = scene;
  cj->view_layer = view_layer;
  cj->ntree = nodetree;
  cj->recalc_flags = compo_get_recalc_flags(C);

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

static void send_notifiers_after_tree_change(ID *id, bNodeTree *ntree)
{
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  if (ntree->type == NTREE_SHADER && id != nullptr) {
    if (GS(id->name) == ID_MA) {
      WM_main_add_notifier(NC_MATERIAL | ND_SHADING, id);
    }
    else if (GS(id->name) == ID_LA) {
      WM_main_add_notifier(NC_LAMP | ND_LIGHTING, id);
    }
    else if (GS(id->name) == ID_WO) {
      WM_main_add_notifier(NC_WORLD | ND_WORLD, id);
    }
  }
  else if (ntree->type == NTREE_COMPOSIT) {
    WM_main_add_notifier(NC_SCENE | ND_NODES, id);
  }
  else if (ntree->type == NTREE_TEXTURE) {
    WM_main_add_notifier(NC_TEXTURE | ND_NODES, id);
  }
  else if (ntree->type == NTREE_GEOMETRY) {
    WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, id);
  }
}

/** \} */

}  // namespace blender::ed::space_node

/* -------------------------------------------------------------------- */
/** \name Node Editor Public API Functions
 * \{ */

void ED_node_tree_propagate_change(const bContext *C, Main *bmain, bNodeTree *root_ntree)
{
  if (C != nullptr) {
    SpaceNode *snode = CTX_wm_space_node(C);
    if (snode != nullptr && root_ntree != nullptr) {
      blender::ed::space_node::send_notifiers_after_tree_change(snode->id, root_ntree);
    }
  }

  NodeTreeUpdateExtraParams params = {nullptr};
  params.tree_changed_fn = [](ID *id, bNodeTree *ntree, void * /*user_data*/) {
    blender::ed::space_node::send_notifiers_after_tree_change(id, ntree);
    DEG_id_tag_update(&ntree->id, ID_RECALC_COPY_ON_WRITE);
  };
  params.tree_output_changed_fn = [](ID * /*id*/, bNodeTree *ntree, void * /*user_data*/) {
    DEG_id_tag_update(&ntree->id, ID_RECALC_NTREE_OUTPUT);
  };

  BKE_ntree_update_main_tree(bmain, root_ntree, &params);
}

void ED_node_set_tree_type(SpaceNode *snode, bNodeTreeType *typeinfo)
{
  if (typeinfo) {
    STRNCPY(snode->tree_idname, typeinfo->idname);
  }
  else {
    snode->tree_idname[0] = '\0';
  }
}

bool ED_node_is_compositor(SpaceNode *snode)
{
  return STREQ(snode->tree_idname, ntreeType_Composite->idname);
}

bool ED_node_is_shader(SpaceNode *snode)
{
  return STREQ(snode->tree_idname, ntreeType_Shader->idname);
}

bool ED_node_is_texture(SpaceNode *snode)
{
  return STREQ(snode->tree_idname, ntreeType_Texture->idname);
}

bool ED_node_is_geometry(SpaceNode *snode)
{
  return STREQ(snode->tree_idname, ntreeType_Geometry->idname);
}

bool ED_node_supports_preview(SpaceNode *snode)
{
  return ED_node_is_compositor(snode) ||
         (U.experimental.use_shader_node_previews && ED_node_is_shader(snode));
}

void ED_node_shader_default(const bContext *C, ID *id)
{
  Main *bmain = CTX_data_main(C);

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

    ma->nodetree = blender::bke::ntreeCopyTree(bmain, ma_default->nodetree);
    ma->nodetree->owner_id = &ma->id;
    for (bNode *node_iter : ma->nodetree->all_nodes()) {
      STRNCPY_UTF8(node_iter->name, DATA_(node_iter->name));
      nodeUniqueName(ma->nodetree, node_iter);
    }

    BKE_ntree_update_main_tree(bmain, ma->nodetree, nullptr);
  }
  else if (ELEM(GS(id->name), ID_WO, ID_LA)) {
    /* Emission */
    bNodeTree *ntree = blender::bke::ntreeAddTreeEmbedded(
        nullptr, id, "Shader Nodetree", ntreeType_Shader->idname);
    bNode *shader, *output;

    if (GS(id->name) == ID_WO) {
      World *world = (World *)id;

      shader = nodeAddStaticNode(nullptr, ntree, SH_NODE_BACKGROUND);
      output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_WORLD);
      nodeAddLink(ntree,
                  shader,
                  nodeFindSocket(shader, SOCK_OUT, "Background"),
                  output,
                  nodeFindSocket(output, SOCK_IN, "Surface"));

      bNodeSocket *color_sock = nodeFindSocket(shader, SOCK_IN, "Color");
      copy_v3_v3(((bNodeSocketValueRGBA *)color_sock->default_value)->value, &world->horr);
    }
    else {
      shader = nodeAddStaticNode(nullptr, ntree, SH_NODE_EMISSION);
      output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_LIGHT);
      nodeAddLink(ntree,
                  shader,
                  nodeFindSocket(shader, SOCK_OUT, "Emission"),
                  output,
                  nodeFindSocket(output, SOCK_IN, "Surface"));
    }

    shader->locx = 10.0f;
    shader->locy = 300.0f;
    output->locx = 300.0f;
    output->locy = 300.0f;
    nodeSetActive(ntree, output);
    BKE_ntree_update_main_tree(bmain, ntree, nullptr);
  }
  else {
    printf("ED_node_shader_default called on wrong ID type.\n");
    return;
  }
}

void ED_node_composit_default(const bContext *C, Scene *sce)
{
  /* but lets check it anyway */
  if (sce->nodetree) {
    if (G.debug & G_DEBUG) {
      printf("error in composite initialize\n");
    }
    return;
  }

  sce->nodetree = blender::bke::ntreeAddTreeEmbedded(
      nullptr, &sce->id, "Compositing Nodetree", ntreeType_Composite->idname);

  sce->nodetree->chunksize = 256;
  sce->nodetree->edit_quality = NTREE_QUALITY_HIGH;
  sce->nodetree->render_quality = NTREE_QUALITY_HIGH;

  bNode *out = nodeAddStaticNode(C, sce->nodetree, CMP_NODE_COMPOSITE);
  out->locx = 300.0f;
  out->locy = 400.0f;

  bNode *in = nodeAddStaticNode(C, sce->nodetree, CMP_NODE_R_LAYERS);
  in->locx = 10.0f;
  in->locy = 400.0f;
  nodeSetActive(sce->nodetree, in);

  /* Links from color to color. */
  bNodeSocket *fromsock = (bNodeSocket *)in->outputs.first;
  bNodeSocket *tosock = (bNodeSocket *)out->inputs.first;
  nodeAddLink(sce->nodetree, in, fromsock, out, tosock);

  BKE_ntree_update_main_tree(CTX_data_main(C), sce->nodetree, nullptr);
}

void ED_node_texture_default(const bContext *C, Tex *tex)
{
  if (tex->nodetree) {
    if (G.debug & G_DEBUG) {
      printf("error in texture initialize\n");
    }
    return;
  }

  tex->nodetree = blender::bke::ntreeAddTreeEmbedded(
      nullptr, &tex->id, "Texture Nodetree", ntreeType_Texture->idname);

  bNode *out = nodeAddStaticNode(C, tex->nodetree, TEX_NODE_OUTPUT);
  out->locx = 300.0f;
  out->locy = 300.0f;

  bNode *in = nodeAddStaticNode(C, tex->nodetree, TEX_NODE_CHECKER);
  in->locx = 10.0f;
  in->locy = 300.0f;
  nodeSetActive(tex->nodetree, in);

  bNodeSocket *fromsock = (bNodeSocket *)in->outputs.first;
  bNodeSocket *tosock = (bNodeSocket *)out->inputs.first;
  nodeAddLink(tex->nodetree, in, fromsock, out, tosock);

  BKE_ntree_update_main_tree(CTX_data_main(C), tex->nodetree, nullptr);
}

namespace blender::ed::space_node {

/**
 * Here we set the active tree(s), even called for each redraw now, so keep it fast :)
 */
void snode_set_context(const bContext &C)
{
  SpaceNode *snode = CTX_wm_space_node(&C);
  bNodeTreeType *treetype = ntreeTypeFind(snode->tree_idname);
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
    ED_node_tree_start(snode, ntree, id, from);
  }
}

}  // namespace blender::ed::space_node

void ED_node_set_active(
    Main *bmain, SpaceNode *snode, bNodeTree *ntree, bNode *node, bool *r_active_texture_changed)
{
  const bool was_active_texture = (node->flag & NODE_ACTIVE_TEXTURE) != 0;
  if (r_active_texture_changed) {
    *r_active_texture_changed = false;
  }

  nodeSetActive(ntree, node);
  if (node->type == NODE_GROUP) {
    return;
  }

  const bool was_output = (node->flag & NODE_DO_OUTPUT) != 0;
  bool do_update = false;

  /* Generic node group output: set node as active output. */
  if (node->type == NODE_GROUP_OUTPUT) {
    for (bNode *node_iter : ntree->all_nodes()) {
      if (node_iter->type == NODE_GROUP_OUTPUT) {
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
    if (ELEM(node->type,
             SH_NODE_OUTPUT_MATERIAL,
             SH_NODE_OUTPUT_WORLD,
             SH_NODE_OUTPUT_LIGHT,
             SH_NODE_OUTPUT_LINESTYLE))
    {
      for (bNode *node_iter : ntree->all_nodes()) {
        if (node_iter->type == node->type) {
          node_iter->flag &= ~NODE_DO_OUTPUT;
        }
      }

      node->flag |= NODE_DO_OUTPUT;
      BKE_ntree_update_tag_active_output_changed(ntree);
    }

    ED_node_tree_propagate_change(nullptr, bmain, ntree);

    if ((node->flag & NODE_ACTIVE_TEXTURE) && !was_active_texture) {
      /* If active texture changed, free GLSL materials. */
      LISTBASE_FOREACH (Material *, ma, &bmain->materials) {
        if (ma->nodetree && ma->use_nodes && ntreeContainsTree(ma->nodetree, ntree)) {
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
        if (wo->nodetree && wo->use_nodes && ntreeContainsTree(wo->nodetree, ntree)) {
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
      ED_node_tree_propagate_change(nullptr, bmain, ntree);
      WM_main_add_notifier(NC_IMAGE, nullptr);
    }

    WM_main_add_notifier(NC_MATERIAL | ND_NODES, node->id);
  }
  else if (ntree->type == NTREE_COMPOSIT) {
    /* Make active viewer, currently only one is supported. */
    if (ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
      for (bNode *node_iter : ntree->all_nodes()) {
        if (ELEM(node_iter->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
          node_iter->flag &= ~NODE_DO_OUTPUT;
        }
      }

      node->flag |= NODE_DO_OUTPUT;
      if (was_output == 0) {
        BKE_ntree_update_tag_active_output_changed(ntree);
        ED_node_tree_propagate_change(nullptr, bmain, ntree);
      }

      /* Adding a node doesn't link this yet. */
      node->id = (ID *)BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
    }
    else if (node->type == CMP_NODE_COMPOSITE) {
      if (was_output == 0) {
        for (bNode *node_iter : ntree->all_nodes()) {
          if (node_iter->type == CMP_NODE_COMPOSITE) {
            node_iter->flag &= ~NODE_DO_OUTPUT;
          }
        }

        node->flag |= NODE_DO_OUTPUT;
        BKE_ntree_update_tag_active_output_changed(ntree);
        ED_node_tree_propagate_change(nullptr, bmain, ntree);
      }
    }
    else if (do_update) {
      ED_node_tree_propagate_change(nullptr, bmain, ntree);
    }
  }
  else if (ntree->type == NTREE_GEOMETRY) {
    if (node->type == GEO_NODE_VIEWER) {
      if ((node->flag & NODE_DO_OUTPUT) == 0) {
        for (bNode *node_iter : ntree->all_nodes()) {
          if (node_iter->type == GEO_NODE_VIEWER) {
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
   * which only exists during actual drawing. Can we rely on valid totr rects?
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
                               const SpaceNode &snode)
{
  LISTBASE_FOREACH_BACKWARD (bNode *, node, &snode.edittree->nodes) {
    if (node == &node_the_socket_belongs_to) {
      /* Nodes after this one are underneath and can't occlude the socket. */
      return false;
    }

    rctf socket_hitbox;
    const float socket_hitbox_radius = NODE_SOCKSIZE - 0.1f * U.widget_unit;
    BLI_rctf_init_pt_radius(&socket_hitbox, location, socket_hitbox_radius);
    if (BLI_rctf_inside_rctf(&node->runtime->totr, &socket_hitbox)) {
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
  float oldoffsetx, oldoffsety;
  float oldwidth, oldheight;
  int directions;
};

static void node_resize_init(
    bContext *C, wmOperator *op, const float2 &cursor, const bNode *node, NodeResizeDirection dir)
{
  NodeSizeWidget *nsw = MEM_cnew<NodeSizeWidget>(__func__);

  op->customdata = nsw;

  nsw->mxstart = cursor.x;
  nsw->mystart = cursor.y;

  /* store old */
  nsw->oldlocx = node->locx;
  nsw->oldlocy = node->locy;
  nsw->oldoffsetx = node->offsetx;
  nsw->oldoffsety = node->offsety;
  nsw->oldwidth = node->width;
  nsw->oldheight = node->height;
  nsw->directions = dir;

  WM_cursor_modal_set(CTX_wm_window(C), node_get_resize_cursor(dir));
  /* add modal handler */
  WM_event_add_modal_handler(C, op);
}

static void node_resize_exit(bContext *C, wmOperator *op, bool cancel)
{
  WM_cursor_modal_restore(CTX_wm_window(C));

  /* Restore old data on cancel. */
  if (cancel) {
    SpaceNode *snode = CTX_wm_space_node(C);
    bNode *node = nodeGetActive(snode->edittree);
    NodeSizeWidget *nsw = (NodeSizeWidget *)op->customdata;

    node->locx = nsw->oldlocx;
    node->locy = nsw->oldlocy;
    node->offsetx = nsw->oldoffsetx;
    node->offsety = nsw->oldoffsety;
    node->width = nsw->oldwidth;
    node->height = nsw->oldheight;
  }

  MEM_freeN(op->customdata);
  op->customdata = nullptr;
}

static int node_resize_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  bNode *node = nodeGetActive(snode->edittree);
  NodeSizeWidget *nsw = (NodeSizeWidget *)op->customdata;

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
        float oldwidth = nsw->oldwidth;
        float widthmin = node->typeinfo->minwidth;
        float widthmax = node->typeinfo->maxwidth;

        {
          if (nsw->directions & NODE_RESIZE_RIGHT) {
            *pwidth = oldwidth + dx;
            CLAMP(*pwidth, widthmin, widthmax);
          }
          if (nsw->directions & NODE_RESIZE_LEFT) {
            float locmax = nsw->oldlocx + oldwidth;

            node->locx = nsw->oldlocx + dx;
            CLAMP(node->locx, locmax - widthmax, locmax - widthmin);
            *pwidth = locmax - node->locx;
          }
        }

        /* Height works the other way round. */
        {
          float heightmin = UI_SCALE_FAC * node->typeinfo->minheight;
          float heightmax = UI_SCALE_FAC * node->typeinfo->maxheight;
          if (nsw->directions & NODE_RESIZE_TOP) {
            float locmin = nsw->oldlocy - nsw->oldheight;

            node->locy = nsw->oldlocy + dy;
            CLAMP(node->locy, locmin + heightmin, locmin + heightmax);
            node->height = node->locy - locmin;
          }
          if (nsw->directions & NODE_RESIZE_BOTTOM) {
            node->height = nsw->oldheight - dy;
            CLAMP(node->height, heightmin, heightmax);
          }
        }

        if (node->type == NODE_FRAME) {
          /* Keep the offset symmetric around center point. */
          if (nsw->directions & NODE_RESIZE_LEFT) {
            node->locx = nsw->oldlocx + 0.5f * dx;
            node->offsetx = nsw->oldoffsetx + 0.5f * dx;
          }
          if (nsw->directions & NODE_RESIZE_RIGHT) {
            node->locx = nsw->oldlocx + 0.5f * dx;
            node->offsetx = nsw->oldoffsetx - 0.5f * dx;
          }
          if (nsw->directions & NODE_RESIZE_TOP) {
            node->locy = nsw->oldlocy + 0.5f * dy;
            node->offsety = nsw->oldoffsety + 0.5f * dy;
          }
          if (nsw->directions & NODE_RESIZE_BOTTOM) {
            node->locy = nsw->oldlocy + 0.5f * dy;
            node->offsety = nsw->oldoffsety - 0.5f * dy;
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
      if (event->val == KM_PRESS) {
        node_resize_exit(C, op, true);
        ED_region_tag_redraw(region);

        return OPERATOR_CANCELLED;
      }
      break;
    }
    case EVT_ESCKEY:
      node_resize_exit(C, op, true);
      ED_region_tag_redraw(region);
      return OPERATOR_CANCELLED;
  }

  return OPERATOR_RUNNING_MODAL;
}

static int node_resize_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  const bNode *node = nodeGetActive(snode->edittree);

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

  /* api callbacks */
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
    return U.experimental.use_shader_node_previews && !(node.is_frame());
  }
  return node.typeinfo->flag & NODE_PREVIEW;
}

static bool cursor_isect_multi_input_socket(const float2 &cursor, const bNodeSocket &socket)
{
  const float node_socket_height = node_socket_calculate_height(socket);
  const float2 location = socket.runtime->location;
  /* `.xmax = socket->locx + NODE_SOCKSIZE * 5.5f`
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
                                        const float2 &cursor,
                                        const eNodeSocketInOut in_out)
{
  rctf rect;
  const float size_sock_padded = NODE_SOCKSIZE + 4;

  bNodeTree &node_tree = *snode.edittree;
  node_tree.ensure_topology_cache();
  const Span<bNode *> nodes = node_tree.all_nodes();
  if (nodes.is_empty()) {
    return nullptr;
  }

  for (int i = nodes.index_range().last(); i >= 0; i--) {
    bNode &node = *nodes[i];

    BLI_rctf_init_pt_radius(&rect, cursor, size_sock_padded);
    if (!(node.flag & NODE_HIDDEN)) {
      /* Extra padding inside and out - allow dragging on the text areas too. */
      if (in_out == SOCK_IN) {
        rect.xmax += NODE_SOCKSIZE;
        rect.xmin -= NODE_SOCKSIZE * 4;
      }
      else if (in_out == SOCK_OUT) {
        rect.xmax += NODE_SOCKSIZE * 4;
        rect.xmin -= NODE_SOCKSIZE;
      }
    }

    if (in_out & SOCK_IN) {
      for (bNodeSocket *sock : node.input_sockets()) {
        if (sock->is_visible()) {
          const float2 location = sock->runtime->location;
          if (sock->flag & SOCK_MULTI_INPUT && !(node.flag & NODE_HIDDEN)) {
            if (cursor_isect_multi_input_socket(cursor, *sock)) {
              if (!socket_is_occluded(location, node, snode)) {
                return sock;
              }
            }
          }
          else if (BLI_rctf_isect_pt(&rect, location.x, location.y)) {
            if (!socket_is_occluded(location, node, snode)) {
              return sock;
            }
          }
        }
      }
    }
    if (in_out & SOCK_OUT) {
      for (bNodeSocket *sock : node.output_sockets()) {
        if (sock->is_visible()) {
          const float2 location = sock->runtime->location;
          if (BLI_rctf_isect_pt(&rect, location.x, location.y)) {
            if (!socket_is_occluded(location, node, snode)) {
              return sock;
            }
          }
        }
      }
    }
  }

  return nullptr;
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
  return nodeLinkIsHidden(&link) || node_link_dim_factor(v2d, link) < 0.5f;
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
    nodeDetachNode(ntree, node_map.lookup(node));
    nodeAttachNode(ntree, node_map.lookup(node), node_map.lookup(parent));
  }
}

void remap_node_pairing(bNodeTree &dst_tree, const Map<const bNode *, bNode *> &node_map)
{
  /* We don't have the old tree for looking up output nodes by ID,
   * so we have to build a map first to find copied output nodes in the new tree. */
  Map<int32_t, bNode *> dst_output_node_map;
  for (const auto &item : node_map.items()) {
    if (ELEM(item.key->type, GEO_NODE_SIMULATION_OUTPUT, GEO_NODE_REPEAT_OUTPUT)) {
      dst_output_node_map.add_new(item.key->identifier, item.value);
    }
  }

  for (bNode *dst_node : node_map.values()) {
    switch (dst_node->type) {
      case GEO_NODE_SIMULATION_INPUT: {
        NodeGeometrySimulationInput *data = static_cast<NodeGeometrySimulationInput *>(
            dst_node->storage);
        if (const bNode *output_node = dst_output_node_map.lookup_default(data->output_node_id,
                                                                          nullptr)) {
          data->output_node_id = output_node->identifier;
        }
        else {
          data->output_node_id = 0;
          blender::nodes::update_node_declaration_and_sockets(dst_tree, *dst_node);
        }
        break;
      }
      case GEO_NODE_REPEAT_INPUT: {
        NodeGeometryRepeatInput *data = static_cast<NodeGeometryRepeatInput *>(dst_node->storage);
        if (const bNode *output_node = dst_output_node_map.lookup_default(data->output_node_id,
                                                                          nullptr)) {
          data->output_node_id = output_node->identifier;
        }
        else {
          data->output_node_id = 0;
          blender::nodes::update_node_declaration_and_sockets(dst_tree, *dst_node);
        }
        break;
      }
    }
  }
}

static int node_duplicate_exec(bContext *C, wmOperator *op)
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

  for (bNode *node : get_selected_nodes(*ntree)) {
    bNode *new_node = bke::node_copy_with_mapping(
        ntree, *node, LIB_ID_COPY_DEFAULT, true, socket_map);
    node_map.add_new(node, new_node);

    if (node->id && dupli_node_tree) {
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
      bNodeLink *newlink = MEM_cnew<bNodeLink>("bNodeLink");
      newlink->flag = link->flag;
      newlink->tonode = node_map.lookup(link->tonode);
      newlink->tosock = socket_map.lookup(link->tosock);

      if (link->tosock->flag & SOCK_MULTI_INPUT) {
        newlink->multi_input_socket_index = link->multi_input_socket_index;
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
    blender::bke::nodeDeclarationEnsure(ntree, node);
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

    nodeSetSelected(src_node, false);
    src_node->flag &= ~(NODE_ACTIVE | NODE_ACTIVE_TEXTURE);
    nodeSetSelected(dst_node, true);
  }

  ED_node_tree_propagate_change(C, bmain, snode->edittree);
  return OPERATOR_FINISHED;
}

void NODE_OT_duplicate(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Duplicate Nodes";
  ot->description = "Duplicate selected nodes";
  ot->idname = "NODE_OT_duplicate";

  /* api callbacks */
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
static int node_read_viewlayers_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  Scene *curscene = CTX_data_scene(C);
  bNodeTree &edit_tree = *snode->edittree;

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  /* first tag scenes unread */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    scene->id.tag |= LIB_TAG_DOIT;
  }

  for (bNode *node : edit_tree.all_nodes()) {
    if ((node->type == CMP_NODE_R_LAYERS) ||
        (node->type == CMP_NODE_CRYPTOMATTE && node->custom1 == CMP_CRYPTOMATTE_SRC_RENDER))
    {
      ID *id = node->id;
      if (id == nullptr) {
        continue;
      }
      if (id->tag & LIB_TAG_DOIT) {
        RE_ReadRenderResult(curscene, (Scene *)id);
        ntreeCompositTagRender((Scene *)id);
        id->tag &= ~LIB_TAG_DOIT;
      }
    }
  }

  ED_node_tree_propagate_change(C, bmain, &edit_tree);

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

int node_render_changed_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *sce = CTX_data_scene(C);

  /* This is actually a test whether scene is used by the compositor or not.
   * All the nodes are using same render result, so there is no need to do
   * anything smart about check how exactly scene is used. */
  bNode *node = nullptr;
  for (bNode *node_iter : sce->nodetree->all_nodes()) {
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

      WM_operator_name_call(C, "RENDER_OT_render", WM_OP_INVOKE_DEFAULT, &op_ptr, nullptr);

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
/** \name Node Hide Operator
 * \{ */

/**
 * Toggles the flag on all selected nodes. If the flag is set on all nodes it is unset.
 * If the flag is not set on all nodes, it is set.
 */
static void node_flag_toggle_exec(SpaceNode *snode, int toggle_flag)
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
    }
  }
}

static int node_hide_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  /* Sanity checking (poll callback checks this already). */
  if ((snode == nullptr) || (snode->edittree == nullptr)) {
    return OPERATOR_CANCELLED;
  }

  node_flag_toggle_exec(snode, NODE_HIDDEN);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_hide_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide";
  ot->description = "Toggle hiding of selected nodes";
  ot->idname = "NODE_OT_hide_toggle";

  /* callbacks */
  ot->exec = node_hide_toggle_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int node_preview_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  /* Sanity checking (poll callback checks this already). */
  if ((snode == nullptr) || (snode->edittree == nullptr)) {
    return OPERATOR_CANCELLED;
  }

  node_flag_toggle_exec(snode, NODE_PREVIEW);

  ED_node_tree_propagate_change(C, CTX_data_main(C), snode->edittree);

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

static int node_deactivate_viewer_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  WorkSpace &workspace = *CTX_wm_workspace(C);

  bNode *active_viewer = viewer_path::find_geometry_nodes_viewer(workspace.viewer_path, snode);

  for (bNode *node : snode.edittree->all_nodes()) {
    if (node->type != GEO_NODE_VIEWER) {
      continue;
    }
    if (!(node->flag & SELECT)) {
      continue;
    }
    if (node == active_viewer) {
      node->flag &= ~NODE_DO_OUTPUT;
      BKE_ntree_update_tag_node_property(snode.edittree, node);
    }
  }

  ED_node_tree_propagate_change(C, CTX_data_main(C), snode.edittree);

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

static int node_options_toggle_exec(bContext *C, wmOperator * /*op*/)
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

static int node_socket_toggle_exec(bContext *C, wmOperator * /*op*/)
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

  ED_node_tree_propagate_change(C, CTX_data_main(C), snode->edittree);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

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

static int node_mute_exec(bContext *C, wmOperator * /*op*/)
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

  ED_node_tree_propagate_change(C, bmain, snode->edittree);

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

static int node_delete_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  /* Delete paired nodes as well. */
  node_select_paired(*snode->edittree);

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & SELECT) {
      nodeRemoveNode(bmain, snode->edittree, node, true);
    }
  }

  ED_node_tree_propagate_change(C, bmain, snode->edittree);

  return OPERATOR_FINISHED;
}

void NODE_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete";
  ot->description = "Remove selected nodes";
  ot->idname = "NODE_OT_delete";

  /* api callbacks */
  ot->exec = node_delete_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Switch View
 * \{ */

static bool node_switch_view_poll(bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  if (snode && snode->edittree) {
    return true;
  }

  return false;
}

static int node_switch_view_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & SELECT) {
      /* Call the update function from the Switch View node. */
      node->runtime->update = NODE_UPDATE_OPERATOR;
    }
  }

  ED_node_tree_propagate_change(C, CTX_data_main(C), snode->edittree);

  return OPERATOR_FINISHED;
}

void NODE_OT_switch_view_update(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Update Views";
  ot->description = "Update views of selected node";
  ot->idname = "NODE_OT_switch_view_update";

  /* api callbacks */
  ot->exec = node_switch_view_exec;
  ot->poll = node_switch_view_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Delete with Reconnect Operator
 * \{ */

static int node_delete_reconnect_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  /* Delete paired nodes as well. */
  node_select_paired(*snode->edittree);

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & SELECT) {
      blender::bke::nodeInternalRelink(snode->edittree, node);
      nodeRemoveNode(bmain, snode->edittree, node, true);
    }
  }

  ED_node_tree_propagate_change(C, bmain, snode->edittree);

  return OPERATOR_FINISHED;
}

void NODE_OT_delete_reconnect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete with Reconnect";
  ot->description = "Remove nodes and reconnect nodes as if deletion was muted";
  ot->idname = "NODE_OT_delete_reconnect";

  /* api callbacks */
  ot->exec = node_delete_reconnect_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node File Output Add Socket Operator
 * \{ */

static int node_output_file_add_socket_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  PointerRNA ptr = CTX_data_pointer_get(C, "node");
  bNodeTree *ntree = nullptr;
  bNode *node = nullptr;
  char file_path[MAX_NAME];

  if (ptr.data) {
    node = (bNode *)ptr.data;
    ntree = (bNodeTree *)ptr.owner_id;
  }
  else if (snode && snode->edittree) {
    ntree = snode->edittree;
    node = nodeGetActive(snode->edittree);
  }

  if (!node || node->type != CMP_NODE_OUTPUT_FILE) {
    return OPERATOR_CANCELLED;
  }

  RNA_string_get(op->ptr, "file_path", file_path);
  ntreeCompositOutputFileAddSocket(ntree, node, file_path, &scene->r.im_format);

  ED_node_tree_propagate_change(C, CTX_data_main(C), snode->edittree);

  return OPERATOR_FINISHED;
}

void NODE_OT_output_file_add_socket(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add File Node Socket";
  ot->description = "Add a new input to a file output node";
  ot->idname = "NODE_OT_output_file_add_socket";

  /* callbacks */
  ot->exec = node_output_file_add_socket_exec;
  ot->poll = composite_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_string(
      ot->srna, "file_path", "Image", MAX_NAME, "File Path", "Subpath of the output file");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Multi File Output Remove Socket Operator
 * \{ */

static int node_output_file_remove_active_socket_exec(bContext *C, wmOperator * /*op*/)
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
    node = nodeGetActive(snode->edittree);
  }

  if (!node || node->type != CMP_NODE_OUTPUT_FILE) {
    return OPERATOR_CANCELLED;
  }

  if (!ntreeCompositOutputFileRemoveActiveSocket(ntree, node)) {
    return OPERATOR_CANCELLED;
  }

  ED_node_tree_propagate_change(C, CTX_data_main(C), ntree);

  return OPERATOR_FINISHED;
}

void NODE_OT_output_file_remove_active_socket(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove File Node Socket";
  ot->description = "Remove the active input from a file output node";
  ot->idname = "NODE_OT_output_file_remove_active_socket";

  /* callbacks */
  ot->exec = node_output_file_remove_active_socket_exec;
  ot->poll = composite_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Multi File Output Move Socket Node
 * \{ */

static int node_output_file_move_active_socket_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  PointerRNA ptr = CTX_data_pointer_get(C, "node");
  bNode *node = nullptr;

  if (ptr.data) {
    node = (bNode *)ptr.data;
  }
  else if (snode && snode->edittree) {
    node = nodeGetActive(snode->edittree);
  }

  if (!node || node->type != CMP_NODE_OUTPUT_FILE) {
    return OPERATOR_CANCELLED;
  }

  NodeImageMultiFile *nimf = (NodeImageMultiFile *)node->storage;

  bNodeSocket *sock = (bNodeSocket *)BLI_findlink(&node->inputs, nimf->active_input);
  if (!sock) {
    return OPERATOR_CANCELLED;
  }

  int direction = RNA_enum_get(op->ptr, "direction");

  if (direction == 1) {
    bNodeSocket *before = sock->prev;
    if (!before) {
      return OPERATOR_CANCELLED;
    }
    BLI_remlink(&node->inputs, sock);
    BLI_insertlinkbefore(&node->inputs, before, sock);
    nimf->active_input--;
  }
  else {
    bNodeSocket *after = sock->next;
    if (!after) {
      return OPERATOR_CANCELLED;
    }
    BLI_remlink(&node->inputs, sock);
    BLI_insertlinkafter(&node->inputs, after, sock);
    nimf->active_input++;
  }

  BKE_ntree_update_tag_node_property(snode->edittree, node);
  ED_node_tree_propagate_change(C, CTX_data_main(C), snode->edittree);

  return OPERATOR_FINISHED;
}

void NODE_OT_output_file_move_active_socket(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {1, "UP", 0, "Up", ""}, {2, "DOWN", 0, "Down", ""}, {0, nullptr, 0, nullptr, nullptr}};

  /* identifiers */
  ot->name = "Move File Node Socket";
  ot->description = "Move the active input of a file output node up or down the list";
  ot->idname = "NODE_OT_output_file_move_active_socket";

  /* callbacks */
  ot->exec = node_output_file_move_active_socket_exec;
  ot->poll = composite_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "direction", direction_items, 2, "Direction", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Copy Node Color Operator
 * \{ */

static int node_copy_color_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &ntree = *snode.edittree;

  bNode *active_node = nodeGetActive(&ntree);
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

  /* api callbacks */
  ot->exec = node_copy_color_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node-Tree Add Interface Socket Operator
 * \{ */

static bNodeSocket *ntree_get_active_interface_socket(const ListBase *lb)
{
  LISTBASE_FOREACH (bNodeSocket *, socket, lb) {
    if (socket->flag & SELECT) {
      return socket;
    }
  }
  return nullptr;
}

static int ntree_socket_add_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;

  PointerRNA ntree_ptr;
  RNA_id_pointer_create((ID *)ntree, &ntree_ptr);

  const eNodeSocketInOut in_out = (eNodeSocketInOut)RNA_enum_get(op->ptr, "in_out");
  ListBase *sockets = (in_out == SOCK_IN) ? &ntree->inputs : &ntree->outputs;

  const char *default_name = (in_out == SOCK_IN) ? DATA_("Input") : DATA_("Output");
  bNodeSocket *active_sock = ntree_get_active_interface_socket(sockets);

  bNodeSocket *sock;
  if (active_sock) {
    /* Insert a copy of the active socket right after it. */
    sock = blender::bke::ntreeInsertSocketInterface(
        ntree, in_out, active_sock->idname, active_sock->next, active_sock->name);
    /* XXX this only works for actual sockets, not interface templates! */
    // nodeSocketCopyValue(sock, &ntree_ptr, active_sock, &ntree_ptr);
  }
  else {
    /* XXX TODO: define default socket type for a tree! */
    sock = ntreeAddSocketInterface(ntree, in_out, "NodeSocketFloat", default_name);
  }

  /* Deactivate sockets. */
  LISTBASE_FOREACH (bNodeSocket *, socket_iter, sockets) {
    socket_iter->flag &= ~SELECT;
  }
  /* Make the new socket selected. */
  sock->flag |= SELECT;

  ED_node_tree_propagate_change(C, CTX_data_main(C), snode->edittree);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_tree_socket_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Node Tree Interface Socket";
  ot->description = "Add an input or output to the active node tree";
  ot->idname = "NODE_OT_tree_socket_add";

  /* api callbacks */
  ot->exec = ntree_socket_add_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "in_out", rna_enum_node_socket_in_out_items, SOCK_IN, "Socket Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node-Tree Remove Interface Socket Operator
 * \{ */

static int ntree_socket_remove_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  const eNodeSocketInOut in_out = (eNodeSocketInOut)RNA_enum_get(op->ptr, "in_out");

  bNodeSocket *iosock = ntree_get_active_interface_socket(in_out == SOCK_IN ? &ntree->inputs :
                                                                              &ntree->outputs);
  if (iosock == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Preferably next socket becomes active, otherwise try previous socket. */
  bNodeSocket *active_sock = (iosock->next ? iosock->next : iosock->prev);
  ntreeRemoveSocketInterface(ntree, iosock);

  /* Set active socket. */
  if (active_sock) {
    active_sock->flag |= SELECT;
  }

  ED_node_tree_propagate_change(C, CTX_data_main(C), ntree);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_tree_socket_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Node Tree Interface Socket";
  ot->description = "Remove an input or output from the active node tree";
  ot->idname = "NODE_OT_tree_socket_remove";

  /* api callbacks */
  ot->exec = ntree_socket_remove_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  RNA_def_enum(ot->srna, "in_out", rna_enum_node_socket_in_out_items, SOCK_IN, "Socket Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node-Tree Change Interface Socket Type Operator
 * \{ */

static int ntree_socket_change_type_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  const eNodeSocketInOut in_out = (eNodeSocketInOut)RNA_enum_get(op->ptr, "in_out");
  const bNodeSocketType *socket_type = rna_node_socket_type_from_enum(
      RNA_enum_get(op->ptr, "socket_type"));
  ListBase *sockets = (in_out == SOCK_IN) ? &ntree->inputs : &ntree->outputs;

  Main *main = CTX_data_main(C);

  bNodeSocket *iosock = ntree_get_active_interface_socket(sockets);
  if (iosock == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* The type remains the same, so we don't need to change anything. */
  if (iosock->typeinfo == socket_type) {
    return OPERATOR_FINISHED;
  }

  blender::bke::nodeModifySocketType(ntree, nullptr, iosock, socket_type->idname);

  /* Need the extra update here because the loop above does not check for valid links in the node
   * group we're currently editing. */
  BKE_ntree_update_tag_interface(ntree);

  /* Deactivate sockets. */
  LISTBASE_FOREACH (bNodeSocket *, socket_iter, sockets) {
    socket_iter->flag &= ~SELECT;
  }
  /* Make the new socket active. */
  iosock->flag |= SELECT;

  ED_node_tree_propagate_change(C, main, ntree);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

static bool socket_change_poll_type(void *userdata, bNodeSocketType *socket_type)
{
  /* Check if the node tree supports the socket type. */
  bNodeTreeType *ntreetype = (bNodeTreeType *)userdata;
  if (ntreetype->valid_socket_type && !ntreetype->valid_socket_type(ntreetype, socket_type)) {
    return false;
  }

  /* Only use basic socket types for this enum. */
  if (socket_type->subtype != PROP_NONE) {
    return false;
  }

  return true;
}

static const EnumPropertyItem *socket_change_type_itemf(bContext *C,
                                                        PointerRNA * /*ptr*/,
                                                        PropertyRNA * /*prop*/,
                                                        bool *r_free)
{
  if (!C) {
    return DummyRNA_NULL_items;
  }

  SpaceNode *snode = CTX_wm_space_node(C);
  if (!snode || !snode->edittree) {
    return DummyRNA_NULL_items;
  }

  return rna_node_socket_type_itemf(snode->edittree->typeinfo, socket_change_poll_type, r_free);
}

void NODE_OT_tree_socket_change_type(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Change Node Tree Interface Socket Type";
  ot->description = "Change the type of an input or output of the active node tree";
  ot->idname = "NODE_OT_tree_socket_change_type";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = ntree_socket_change_type_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "in_out", rna_enum_node_socket_in_out_items, SOCK_IN, "Socket Type", "");
  prop = RNA_def_enum(ot->srna, "socket_type", DummyRNA_DEFAULT_items, 0, "Socket Type", "");
  RNA_def_enum_funcs(prop, socket_change_type_itemf);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node-Tree Change Interface Socket Subtype Operator
 * \{ */

static int ntree_socket_change_subtype_exec(bContext *C, wmOperator *op)
{
  Main *main = CTX_data_main(C);
  const int socket_subtype = RNA_enum_get(op->ptr, "socket_subtype");

  PointerRNA io_socket_ptr = CTX_data_pointer_get_type(
      C, "interface_socket", &RNA_NodeSocketInterface);
  bNodeSocket *io_socket = static_cast<bNodeSocket *>(io_socket_ptr.data);
  if (!io_socket) {
    return OPERATOR_CANCELLED;
  }

  bNodeTree &node_tree = *reinterpret_cast<bNodeTree *>(io_socket_ptr.owner_id);

  ListBase *sockets;
  if (node_tree.interface_inputs().contains(io_socket)) {
    sockets = &node_tree.inputs;
  }
  else if (node_tree.interface_outputs().contains(io_socket)) {
    sockets = &node_tree.outputs;
  }
  else {
    /* The interface socket should be in the inputs or outputs. */
    BLI_assert_unreachable();
    return OPERATOR_CANCELLED;
  }

  nodeModifySocketTypeStatic(&node_tree, nullptr, io_socket, io_socket->type, socket_subtype);

  /* Deactivate sockets. */
  LISTBASE_FOREACH (bNodeSocket *, socket_iter, sockets) {
    socket_iter->flag &= ~SELECT;
  }
  /* Make the new socket active. */
  io_socket->flag |= SELECT;

  BKE_ntree_update_tag_interface(&node_tree);
  ED_node_tree_propagate_change(C, main, &node_tree);

  return OPERATOR_FINISHED;
}

static Set<int> socket_type_get_subtypes(const eNodeSocketDatatype type)
{
  switch (type) {
    case SOCK_FLOAT:
      return {PROP_PERCENTAGE,
              PROP_FACTOR,
              PROP_ANGLE,
              PROP_TIME,
              PROP_TIME_ABSOLUTE,
              PROP_DISTANCE,
              PROP_NONE};
    case SOCK_INT:
      return {PROP_PERCENTAGE, PROP_FACTOR, PROP_NONE};
    case SOCK_VECTOR:
      return {PROP_TRANSLATION,
              /* Direction doesn't seem to work. */
              // PROP_DIRECTION,
              PROP_VELOCITY,
              PROP_ACCELERATION,
              PROP_EULER,
              PROP_XYZ,
              PROP_NONE};
    default:
      return {};
  }
}

static const EnumPropertyItem *socket_change_subtype_itemf(bContext *C,
                                                           PointerRNA * /*ptr*/,
                                                           PropertyRNA * /*prop*/,
                                                           bool *r_free)
{
  if (!C) {
    return DummyRNA_NULL_items;
  }
  SpaceNode *snode = CTX_wm_space_node(C);
  if (!snode || !snode->edittree) {
    return DummyRNA_NULL_items;
  }

  PointerRNA active_socket_ptr = CTX_data_pointer_get_type(
      C, "interface_socket", &RNA_NodeSocketInterface);
  const bNodeSocket *active_socket = static_cast<const bNodeSocket *>(active_socket_ptr.data);
  if (!active_socket) {
    return DummyRNA_NULL_items;
  }

  const Set<int> subtypes = socket_type_get_subtypes(eNodeSocketDatatype(active_socket->type));
  if (subtypes.is_empty()) {
    return DummyRNA_NULL_items;
  }

  EnumPropertyItem *items = nullptr;
  int items_count = 0;
  for (const EnumPropertyItem *item = rna_enum_property_subtype_items; item->name != nullptr;
       item++) {
    if (subtypes.contains(item->value)) {
      RNA_enum_item_add(&items, &items_count, item);
    }
  }

  if (items_count == 0) {
    return DummyRNA_NULL_items;
  }

  RNA_enum_item_end(&items, &items_count);
  *r_free = true;
  return items;
}

static bool ntree_socket_change_subtype_poll(bContext *C)
{
  if (!ED_operator_node_editable(C)) {
    return false;
  }
  PointerRNA io_socket_ptr = CTX_data_pointer_get_type(
      C, "interface_socket", &RNA_NodeSocketInterface);
  const bNodeSocket *io_socket = static_cast<const bNodeSocket *>(io_socket_ptr.data);
  if (!io_socket) {
    return false;
  }
  return !socket_type_get_subtypes(eNodeSocketDatatype(io_socket->type)).is_empty();
}

void NODE_OT_tree_socket_change_subtype(wmOperatorType *ot)
{
  ot->name = "Change Node Tree Socket Subtype";
  ot->description = "Change the subtype of a socket of the active node tree";
  ot->idname = "NODE_OT_tree_socket_change_subtype";

  ot->invoke = WM_menu_invoke;
  ot->exec = ntree_socket_change_subtype_exec;
  ot->poll = ntree_socket_change_subtype_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(
      ot->srna, "socket_subtype", DummyRNA_DEFAULT_items, 0, "Socket Subtype", "");
  RNA_def_enum_funcs(ot->prop, socket_change_subtype_itemf);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node-Tree Move Interface Socket Operator
 * \{ */

static const EnumPropertyItem move_direction_items[] = {
    {1, "UP", 0, "Up", ""},
    {2, "DOWN", 0, "Down", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static int ntree_socket_move_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  int direction = RNA_enum_get(op->ptr, "direction");

  const eNodeSocketInOut in_out = (eNodeSocketInOut)RNA_enum_get(op->ptr, "in_out");
  ListBase *sockets = in_out == SOCK_IN ? &ntree->inputs : &ntree->outputs;

  bNodeSocket *iosock = ntree_get_active_interface_socket(sockets);

  if (iosock == nullptr) {
    return OPERATOR_CANCELLED;
  }

  switch (direction) {
    case 1: { /* up */
      bNodeSocket *before = iosock->prev;
      BLI_remlink(sockets, iosock);
      if (before) {
        BLI_insertlinkbefore(sockets, before, iosock);
      }
      else {
        BLI_addhead(sockets, iosock);
      }
      break;
    }
    case 2: { /* down */
      bNodeSocket *after = iosock->next;
      BLI_remlink(sockets, iosock);
      if (after) {
        BLI_insertlinkafter(sockets, after, iosock);
      }
      else {
        BLI_addtail(sockets, iosock);
      }
      break;
    }
  }

  BKE_ntree_update_tag_interface(ntree);
  ED_node_tree_propagate_change(C, CTX_data_main(C), ntree);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_tree_socket_move(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move Node Tree Socket";
  ot->description = "Move a socket up or down in the active node tree's interface";
  ot->idname = "NODE_OT_tree_socket_move";

  /* api callbacks */
  ot->exec = ntree_socket_move_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "direction", move_direction_items, 1, "Direction", "");
  RNA_def_enum(ot->srna, "in_out", rna_enum_node_socket_in_out_items, SOCK_IN, "Socket Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Shader Script Update
 * \{ */

static bool node_shader_script_update_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  const RenderEngineType *type = RE_engines_find(scene->r.engine);
  SpaceNode *snode = CTX_wm_space_node(C);

  /* Test if we have a render engine that supports shaders scripts. */
  if (!(type && type->update_script_node)) {
    return false;
  }

  /* See if we have a shader script node in context. */
  bNode *node = (bNode *)CTX_data_pointer_get_type(C, "node", &RNA_ShaderNodeScript).data;

  if (!node && snode && snode->edittree) {
    node = nodeGetActive(snode->edittree);
  }

  if (node && node->type == SH_NODE_SCRIPT) {
    NodeShaderScript *nss = (NodeShaderScript *)node->storage;

    if (node->id || nss->filepath[0]) {
      return ED_operator_node_editable(C);
    }
  }

  /* See if we have a text datablock in context. */
  Text *text = (Text *)CTX_data_pointer_get_type(C, "edit_text", &RNA_Text).data;
  if (text) {
    return true;
  }

  /* We don't check if text datablock is actually in use, too slow for poll. */

  return false;
}

/* recursively check for script nodes in groups using this text and update */
static bool node_shader_script_update_text_recursive(RenderEngine *engine,
                                                     RenderEngineType *type,
                                                     bNodeTree *ntree,
                                                     Text *text,
                                                     VectorSet<bNodeTree *> &done_trees)
{
  bool found = false;

  done_trees.add_new(ntree);

  /* Update each script that is using this text datablock. */
  for (bNode *node : ntree->all_nodes()) {
    if (node->type == NODE_GROUP) {
      bNodeTree *ngroup = (bNodeTree *)node->id;
      if (ngroup && !done_trees.contains(ngroup)) {
        found |= node_shader_script_update_text_recursive(engine, type, ngroup, text, done_trees);
      }
    }
    else if (node->type == SH_NODE_SCRIPT && node->id == &text->id) {
      type->update_script_node(engine, ntree, node);
      found = true;
    }
  }

  return found;
}

static int node_shader_script_update_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  PointerRNA nodeptr = CTX_data_pointer_get_type(C, "node", &RNA_ShaderNodeScript);
  bool found = false;

  /* setup render engine */
  RenderEngineType *type = RE_engines_find(scene->r.engine);
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
    node = nodeGetActive(snode->edittree);
  }

  if (node) {
    /* Update single node. */
    type->update_script_node(engine, ntree_base, node);

    found = true;
  }
  else {
    /* Update all nodes using text datablock. */
    Text *text = (Text *)CTX_data_pointer_get_type(C, "edit_text", &RNA_Text).data;

    if (text) {

      VectorSet<bNodeTree *> done_trees;

      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_SHADER) {
          if (!done_trees.contains(ntree)) {
            found |= node_shader_script_update_text_recursive(
                engine, type, ntree, text, done_trees);
          }
        }
      }
      FOREACH_NODETREE_END;

      if (!found) {
        BKE_report(op->reports, RPT_INFO, "Text not used by any node, no update done");
      }
    }
  }

  RE_engine_free(engine);

  return (found) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void NODE_OT_shader_script_update(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Script Node Update";
  ot->description = "Update shader script node with new sockets and options from the script";
  ot->idname = "NODE_OT_shader_script_update";

  /* api callbacks */
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

static int viewer_border_exec(bContext *C, wmOperator *op)
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

      ED_node_tree_propagate_change(C, bmain, btree);
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

  /* api callbacks */
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

static int clear_viewer_border_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *btree = snode->nodetree;

  btree->flag &= ~NTREE_VIEWER_BORDER;
  ED_node_tree_propagate_change(C, CTX_data_main(C), btree);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_clear_viewer_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Viewer Region";
  ot->description = "Clear the boundaries for viewer operations";
  ot->idname = "NODE_OT_clear_viewer_border";

  /* api callbacks */
  ot->exec = clear_viewer_border_exec;
  ot->poll = composite_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cryptomatte Add Socket
 * \{ */

static int node_cryptomatte_add_socket_exec(bContext *C, wmOperator * /*op*/)
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
    node = nodeGetActive(snode->edittree);
  }

  if (!node || node->type != CMP_NODE_CRYPTOMATTE_LEGACY) {
    return OPERATOR_CANCELLED;
  }

  ntreeCompositCryptomatteAddSocket(ntree, node);

  ED_node_tree_propagate_change(C, CTX_data_main(C), ntree);

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

static int node_cryptomatte_remove_socket_exec(bContext *C, wmOperator * /*op*/)
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
    node = nodeGetActive(snode->edittree);
  }

  if (!node || node->type != CMP_NODE_CRYPTOMATTE_LEGACY) {
    return OPERATOR_CANCELLED;
  }

  if (!ntreeCompositCryptomatteRemoveSocket(ntree, node)) {
    return OPERATOR_CANCELLED;
  }

  ED_node_tree_propagate_change(C, CTX_data_main(C), ntree);

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
