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

/** \file
 * \ingroup spnode
 */

#include "MEM_guardedalloc.h"

#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_text_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "ED_node.h" /* own include */
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_select_utils.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "GPU_material.h"

#include "IMB_imbuf_types.h"

#include "NOD_composite.h"
#include "NOD_geometry.h"
#include "NOD_shader.h"
#include "NOD_texture.h"
#include "node_intern.h" /* own include */

#define USE_ESC_COMPO

/* ***************** composite job manager ********************** */

enum {
  COM_RECALC_COMPOSITE = 1,
  COM_RECALC_VIEWER = 2,
};

typedef struct CompoJob {
  /* Input parameters. */
  Main *bmain;
  Scene *scene;
  ViewLayer *view_layer;
  bNodeTree *ntree;
  int recalc_flags;
  /* Evaluated state/ */
  Depsgraph *compositor_depsgraph;
  bNodeTree *localtree;
  /* Jon system integration. */
  const short *stop;
  short *do_update;
  float *progress;
} CompoJob;

float node_socket_calculate_height(const bNodeSocket *socket)
{
  float sock_height = NODE_SOCKSIZE * 2.0f;
  if (socket->flag & SOCK_MULTI_INPUT) {
    sock_height += max_ii(NODE_MULTI_INPUT_LINK_GAP * 0.5f * socket->total_inputs, NODE_SOCKSIZE);
  }
  return sock_height;
}

void node_link_calculate_multi_input_position(const float socket_x,
                                              const float socket_y,
                                              const int index,
                                              const int total_inputs,
                                              float r[2])
{
  float offset = (total_inputs * NODE_MULTI_INPUT_LINK_GAP - NODE_MULTI_INPUT_LINK_GAP) * 0.5;
  r[0] = socket_x - NODE_SOCKSIZE * 0.5f;
  r[1] = socket_y - offset + (index * NODE_MULTI_INPUT_LINK_GAP);
}

static void compo_tag_output_nodes(bNodeTree *nodetree, int recalc_flags)
{
  LISTBASE_FOREACH (bNode *, node, &nodetree->nodes) {
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
        SpaceImage *sima = area->spacedata.first;
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
        SpaceNode *snode = area->spacedata.first;
        if (snode->flag & SNODE_BACKDRAW) {
          recalc_flags |= COM_RECALC_VIEWER;
        }
      }
    }
  }

  return recalc_flags;
}

/* called by compo, only to check job 'stop' value */
static int compo_breakjob(void *cjv)
{
  CompoJob *cj = cjv;

  /* without G.is_break 'ESC' wont quit - which annoys users */
  return (*(cj->stop)
#ifdef USE_ESC_COMPO
          || G.is_break
#endif
  );
}

/* called by compo, wmJob sends notifier */
static void compo_statsdrawjob(void *cjv, const char *UNUSED(str))
{
  CompoJob *cj = cjv;

  *(cj->do_update) = true;
}

/* called by compo, wmJob sends notifier */
static void compo_redrawjob(void *cjv)
{
  CompoJob *cj = cjv;

  *(cj->do_update) = true;
}

static void compo_freejob(void *cjv)
{
  CompoJob *cj = cjv;

  if (cj->localtree) {
    ntreeLocalMerge(cj->bmain, cj->localtree, cj->ntree);
  }
  if (cj->compositor_depsgraph != NULL) {
    DEG_graph_free(cj->compositor_depsgraph);
  }
  MEM_freeN(cj);
}

/* only now we copy the nodetree, so adding many jobs while
 * sliding buttons doesn't frustrate */
static void compo_initjob(void *cjv)
{
  CompoJob *cj = cjv;
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
}

/* called before redraw notifiers, it moves finished previews over */
static void compo_updatejob(void *UNUSED(cjv))
{
  WM_main_add_notifier(NC_SCENE | ND_COMPO_RESULT, NULL);
}

static void compo_progressjob(void *cjv, float progress)
{
  CompoJob *cj = cjv;

  *(cj->progress) = progress;
}

/* only this runs inside thread */
static void compo_startjob(void *cjv,
                           /* Cannot be const, this function implements wm_jobs_start_callback.
                            * NOLINTNEXTLINE: readability-non-const-parameter. */
                           short *stop,
                           short *do_update,
                           float *progress)
{
  CompoJob *cj = cjv;
  bNodeTree *ntree = cj->localtree;
  Scene *scene = cj->scene;

  if (scene->use_nodes == false) {
    return;
  }

  cj->stop = stop;
  cj->do_update = do_update;
  cj->progress = progress;

  ntree->test_break = compo_breakjob;
  ntree->tbh = cj;
  ntree->stats_draw = compo_statsdrawjob;
  ntree->sdh = cj;
  ntree->progress = compo_progressjob;
  ntree->prh = cj;
  ntree->update_draw = compo_redrawjob;
  ntree->udh = cj;

  // XXX BIF_store_spare();
  /* 1 is do_previews */

  if ((cj->scene->r.scemode & R_MULTIVIEW) == 0) {
    ntreeCompositExecTree(cj->scene,
                          ntree,
                          &cj->scene->r,
                          false,
                          true,
                          &scene->view_settings,
                          &scene->display_settings,
                          "");
  }
  else {
    LISTBASE_FOREACH (SceneRenderView *, srv, &scene->r.views) {
      if (BKE_scene_multiview_is_render_view_active(&scene->r, srv) == false) {
        continue;
      }
      ntreeCompositExecTree(cj->scene,
                            ntree,
                            &cj->scene->r,
                            false,
                            true,
                            &scene->view_settings,
                            &scene->display_settings,
                            srv->name);
    }
  }

  ntree->test_break = NULL;
  ntree->stats_draw = NULL;
  ntree->progress = NULL;
}

/**
 * \param scene_owner: is the owner of the job,
 * we don't use it for anything else currently so could also be a void pointer,
 * but for now keep it an 'Scene' for consistency.
 *
 * \note only call from spaces `refresh` callbacks, not direct! - use with care.
 */
void ED_node_composite_job(const bContext *C, struct bNodeTree *nodetree, Scene *scene_owner)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  /* to fix bug: T32272. */
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
  CompoJob *cj = MEM_callocN(sizeof(CompoJob), "compo job");

  /* customdata for preview thread */
  cj->bmain = bmain;
  cj->scene = scene;
  cj->view_layer = view_layer;
  cj->ntree = nodetree;
  cj->recalc_flags = compo_get_recalc_flags(C);

  /* setup job */
  WM_jobs_customdata_set(wm_job, cj, compo_freejob);
  WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_COMPO_RESULT, NC_SCENE | ND_COMPO_RESULT);
  WM_jobs_callbacks(wm_job, compo_startjob, compo_initjob, compo_updatejob, NULL);

  WM_jobs_start(CTX_wm_manager(C), wm_job);
}

/* ***************************************** */

/* operator poll callback */
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

/* operator poll callback */
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

void snode_dag_update(bContext *C, SpaceNode *snode)
{
  Main *bmain = CTX_data_main(C);

  /* for groups, update all ID's using this */
  if (snode->edittree != snode->nodetree) {
    FOREACH_NODETREE_BEGIN (bmain, tntree, id) {
      if (ntreeHasTree(tntree, snode->edittree)) {
        DEG_id_tag_update(id, 0);
      }
    }
    FOREACH_NODETREE_END;
  }

  DEG_id_tag_update(snode->id, 0);
  DEG_id_tag_update(&snode->nodetree->id, 0);
}

void snode_notify(bContext *C, SpaceNode *snode)
{
  ID *id = snode->id;

  WM_event_add_notifier(C, NC_NODE | NA_EDITED, NULL);

  if (ED_node_is_shader(snode)) {
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
  else if (ED_node_is_compositor(snode)) {
    WM_event_add_notifier(C, NC_SCENE | ND_NODES, id);
  }
  else if (ED_node_is_texture(snode)) {
    WM_event_add_notifier(C, NC_TEXTURE | ND_NODES, id);
  }
  else if (ED_node_is_geometry(snode)) {
    WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, id);
  }
}

void ED_node_set_tree_type(SpaceNode *snode, bNodeTreeType *typeinfo)
{
  if (typeinfo) {
    BLI_strncpy(snode->tree_idname, typeinfo->idname, sizeof(snode->tree_idname));
  }
  else {
    snode->tree_idname[0] = '\0';
  }
}

bool ED_node_is_compositor(struct SpaceNode *snode)
{
  return STREQ(snode->tree_idname, ntreeType_Composite->idname);
}

bool ED_node_is_shader(struct SpaceNode *snode)
{
  return STREQ(snode->tree_idname, ntreeType_Shader->idname);
}

bool ED_node_is_texture(struct SpaceNode *snode)
{
  return STREQ(snode->tree_idname, ntreeType_Texture->idname);
}

bool ED_node_is_geometry(struct SpaceNode *snode)
{
  return STREQ(snode->tree_idname, ntreeType_Geometry->idname);
}

/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
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

    ma->nodetree = ntreeCopyTree(bmain, ma_default->nodetree);
    ntreeUpdateTree(bmain, ma->nodetree);
  }
  else if (ELEM(GS(id->name), ID_WO, ID_LA)) {
    /* Emission */
    bNodeTree *ntree = ntreeAddTree(NULL, "Shader Nodetree", ntreeType_Shader->idname);
    bNode *shader, *output;

    if (GS(id->name) == ID_WO) {
      World *world = (World *)id;
      world->nodetree = ntree;

      shader = nodeAddStaticNode(NULL, ntree, SH_NODE_BACKGROUND);
      output = nodeAddStaticNode(NULL, ntree, SH_NODE_OUTPUT_WORLD);
      nodeAddLink(ntree,
                  shader,
                  nodeFindSocket(shader, SOCK_OUT, "Background"),
                  output,
                  nodeFindSocket(output, SOCK_IN, "Surface"));

      bNodeSocket *color_sock = nodeFindSocket(shader, SOCK_IN, "Color");
      copy_v3_v3(((bNodeSocketValueRGBA *)color_sock->default_value)->value, &world->horr);
    }
    else {
      Light *light = (Light *)id;
      light->nodetree = ntree;

      shader = nodeAddStaticNode(NULL, ntree, SH_NODE_EMISSION);
      output = nodeAddStaticNode(NULL, ntree, SH_NODE_OUTPUT_LIGHT);
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
    ntreeUpdateTree(bmain, ntree);
  }
  else {
    printf("ED_node_shader_default called on wrong ID type.\n");
    return;
  }
}

/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
void ED_node_composit_default(const bContext *C, struct Scene *sce)
{
  /* but lets check it anyway */
  if (sce->nodetree) {
    if (G.debug & G_DEBUG) {
      printf("error in composite initialize\n");
    }
    return;
  }

  sce->nodetree = ntreeAddTree(NULL, "Compositing Nodetree", ntreeType_Composite->idname);

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

  /* links from color to color */
  bNodeSocket *fromsock = in->outputs.first;
  bNodeSocket *tosock = out->inputs.first;
  nodeAddLink(sce->nodetree, in, fromsock, out, tosock);

  ntreeUpdateTree(CTX_data_main(C), sce->nodetree);
}

/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
void ED_node_texture_default(const bContext *C, Tex *tex)
{
  /* but lets check it anyway */
  if (tex->nodetree) {
    if (G.debug & G_DEBUG) {
      printf("error in texture initialize\n");
    }
    return;
  }

  tex->nodetree = ntreeAddTree(NULL, "Texture Nodetree", ntreeType_Texture->idname);

  bNode *out = nodeAddStaticNode(C, tex->nodetree, TEX_NODE_OUTPUT);
  out->locx = 300.0f;
  out->locy = 300.0f;

  bNode *in = nodeAddStaticNode(C, tex->nodetree, TEX_NODE_CHECKER);
  in->locx = 10.0f;
  in->locy = 300.0f;
  nodeSetActive(tex->nodetree, in);

  bNodeSocket *fromsock = in->outputs.first;
  bNodeSocket *tosock = out->inputs.first;
  nodeAddLink(tex->nodetree, in, fromsock, out, tosock);

  ntreeUpdateTree(CTX_data_main(C), tex->nodetree);
}

/* Here we set the active tree(s), even called for each redraw now, so keep it fast :) */
void snode_set_context(const bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTreeType *treetype = ntreeTypeFind(snode->tree_idname);
  bNodeTree *ntree = snode->nodetree;
  ID *id = snode->id, *from = snode->from;

  /* check the tree type */
  if (!treetype || (treetype->poll && !treetype->poll(C, treetype))) {
    /* invalid tree type, skip
     * NB: not resetting the node path here, invalid bNodeTreeType
     * may still be registered at a later point.
     */
    return;
  }

  if (snode->nodetree && !STREQ(snode->nodetree->idname, snode->tree_idname)) {
    /* current tree does not match selected type, clear tree path */
    ntree = NULL;
    id = NULL;
    from = NULL;
  }

  if (!(snode->flag & SNODE_PIN) || ntree == NULL) {
    if (treetype->get_from_context) {
      /* reset and update from context */
      ntree = NULL;
      id = NULL;
      from = NULL;

      treetype->get_from_context(C, treetype, &ntree, &id, &from);
    }
  }

  if (snode->nodetree != ntree || snode->id != id || snode->from != from ||
      (snode->treepath.last == NULL && ntree)) {
    ED_node_tree_start(snode, ntree, id, from);
  }
}

void snode_update(SpaceNode *snode, bNode *node)
{
  /* XXX this only updates nodes in the current node space tree path.
   * The function supposedly should update any potential group node linking to changed tree,
   * this really requires a working depsgraph ...
   */

  /* update all edited group nodes */
  bNodeTreePath *path = snode->treepath.last;
  if (path) {
    bNodeTree *ngroup = path->nodetree;
    for (path = path->prev; path; path = path->prev) {
      nodeUpdateID(path->nodetree, (ID *)ngroup);
      ngroup = path->nodetree;
    }
  }

  if (node) {
    nodeUpdate(snode->edittree, node);
  }
}

void ED_node_set_active(Main *bmain, bNodeTree *ntree, bNode *node, bool *r_active_texture_changed)
{
  const bool was_active_texture = (node->flag & NODE_ACTIVE_TEXTURE) != 0;
  if (r_active_texture_changed) {
    *r_active_texture_changed = false;
  }

  nodeSetActive(ntree, node);

  if (node->type != NODE_GROUP) {
    const bool was_output = (node->flag & NODE_DO_OUTPUT) != 0;
    bool do_update = false;

    /* generic node group output: set node as active output */
    if (node->type == NODE_GROUP_OUTPUT) {
      LISTBASE_FOREACH (bNode *, node_iter, &ntree->nodes) {
        if (node_iter->type == NODE_GROUP_OUTPUT) {
          node_iter->flag &= ~NODE_DO_OUTPUT;
        }
      }

      node->flag |= NODE_DO_OUTPUT;
      if (!was_output) {
        do_update = 1;
      }
    }

    /* tree specific activate calls */
    if (ntree->type == NTREE_SHADER) {
      /* when we select a material, active texture is cleared, for buttons */
      if (node->id && ELEM(GS(node->id->name), ID_MA, ID_LA, ID_WO)) {
        nodeClearActiveID(ntree, ID_TE);
      }

      if (ELEM(node->type,
               SH_NODE_OUTPUT_MATERIAL,
               SH_NODE_OUTPUT_WORLD,
               SH_NODE_OUTPUT_LIGHT,
               SH_NODE_OUTPUT_LINESTYLE)) {
        LISTBASE_FOREACH (bNode *, node_iter, &ntree->nodes) {
          if (node_iter->type == node->type) {
            node_iter->flag &= ~NODE_DO_OUTPUT;
          }
        }

        node->flag |= NODE_DO_OUTPUT;
        if (was_output == 0) {
          ED_node_tag_update_nodetree(bmain, ntree, node);
        }
      }
      else if (do_update) {
        ED_node_tag_update_nodetree(bmain, ntree, node);
      }

      /* if active texture changed, free glsl materials */
      if ((node->flag & NODE_ACTIVE_TEXTURE) && !was_active_texture) {
        LISTBASE_FOREACH (Material *, ma, &bmain->materials) {
          if (ma->nodetree && ma->use_nodes && ntreeHasTree(ma->nodetree, ntree)) {
            GPU_material_free(&ma->gpumaterial);
          }
        }

        LISTBASE_FOREACH (World *, wo, &bmain->worlds) {
          if (wo->nodetree && wo->use_nodes && ntreeHasTree(wo->nodetree, ntree)) {
            GPU_material_free(&wo->gpumaterial);
          }
        }

        if (r_active_texture_changed) {
          *r_active_texture_changed = true;
        }
        ED_node_tag_update_nodetree(bmain, ntree, node);
        WM_main_add_notifier(NC_IMAGE, NULL);
      }

      WM_main_add_notifier(NC_MATERIAL | ND_NODES, node->id);
    }
    else if (ntree->type == NTREE_COMPOSIT) {
      /* make active viewer, currently only 1 supported... */
      if (ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
        LISTBASE_FOREACH (bNode *, node_iter, &ntree->nodes) {
          if (ELEM(node_iter->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
            node_iter->flag &= ~NODE_DO_OUTPUT;
          }
        }

        node->flag |= NODE_DO_OUTPUT;
        if (was_output == 0) {
          ED_node_tag_update_nodetree(bmain, ntree, node);
        }

        /* addnode() doesn't link this yet... */
        node->id = (ID *)BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
      }
      else if (node->type == CMP_NODE_COMPOSITE) {
        if (was_output == 0) {
          LISTBASE_FOREACH (bNode *, node_iter, &ntree->nodes) {
            if (node_iter->type == CMP_NODE_COMPOSITE) {
              node_iter->flag &= ~NODE_DO_OUTPUT;
            }
          }

          node->flag |= NODE_DO_OUTPUT;
          ED_node_tag_update_nodetree(bmain, ntree, node);
        }
      }
      else if (do_update) {
        ED_node_tag_update_nodetree(bmain, ntree, node);
      }
    }
    else if (ntree->type == NTREE_TEXTURE) {
      /* XXX */
#if 0
      if (node->id) {
        BIF_preview_changed(-1);
        allqueue(REDRAWBUTSSHADING, 1);
        allqueue(REDRAWIPO, 0);
      }
#endif
    }
  }
}

void ED_node_post_apply_transform(bContext *UNUSED(C), bNodeTree *UNUSED(ntree))
{
  /* XXX This does not work due to layout functions relying on node->block,
   * which only exists during actual drawing. Can we rely on valid totr rects?
   */
  /* make sure nodes have correct bounding boxes after transform */
  // node_update_nodetree(C, ntree, 0.0f, 0.0f);
}

/* ***************** generic operator functions for nodes ***************** */

#if 0 /* UNUSED */

static bool edit_node_poll(bContext *C)
{
  return ED_operator_node_active(C);
}

static void edit_node_properties(wmOperatorType *ot)
{
  /* XXX could node be a context pointer? */
  RNA_def_string(ot->srna, "node", NULL, MAX_NAME, "Node", "");
  RNA_def_int(ot->srna, "socket", 0, 0, MAX_SOCKET, "Socket", "", 0, MAX_SOCKET);
  RNA_def_enum(ot->srna, "in_out", rna_enum_node_socket_in_out_items, SOCK_IN, "Socket Side", "");
}

static int edit_node_invoke_properties(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set(op->ptr, "node")) {
    bNode *node = CTX_data_pointer_get_type(C, "node", &RNA_Node).data;
    if (!node) {
      return 0;
    }
    else {
      RNA_string_set(op->ptr, "node", node->name);
    }
  }

  if (!RNA_struct_property_is_set(op->ptr, "in_out")) {
    RNA_enum_set(op->ptr, "in_out", SOCK_IN);
  }

  if (!RNA_struct_property_is_set(op->ptr, "socket")) {
    RNA_int_set(op->ptr, "socket", 0);
  }

  return 1;
}

static void edit_node_properties_get(
    wmOperator *op, bNodeTree *ntree, bNode **r_node, bNodeSocket **r_sock, int *r_in_out)
{
  bNode *node;
  bNodeSocket *sock = NULL;
  char nodename[MAX_NAME];
  int sockindex;
  int in_out;

  RNA_string_get(op->ptr, "node", nodename);
  node = nodeFindNodebyName(ntree, nodename);

  in_out = RNA_enum_get(op->ptr, "in_out");

  sockindex = RNA_int_get(op->ptr, "socket");
  switch (in_out) {
    case SOCK_IN:
      sock = BLI_findlink(&node->inputs, sockindex);
      break;
    case SOCK_OUT:
      sock = BLI_findlink(&node->outputs, sockindex);
      break;
  }

  if (r_node) {
    *r_node = node;
  }
  if (r_sock) {
    *r_sock = sock;
  }
  if (r_in_out) {
    *r_in_out = in_out;
  }
}
#endif

/* ************************** Node generic ************** */

/* is rct in visible part of node? */
static bNode *visible_node(SpaceNode *snode, const rctf *rct)
{
  LISTBASE_FOREACH_BACKWARD (bNode *, node, &snode->edittree->nodes) {
    if (BLI_rctf_isect(&node->totr, rct, NULL)) {
      return node;
    }
  }
  return NULL;
}

/* ********************** size widget operator ******************** */

typedef struct NodeSizeWidget {
  float mxstart, mystart;
  float oldlocx, oldlocy;
  float oldoffsetx, oldoffsety;
  float oldwidth, oldheight;
  int directions;
} NodeSizeWidget;

static void node_resize_init(
    bContext *C, wmOperator *op, const wmEvent *UNUSED(event), bNode *node, int dir)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  NodeSizeWidget *nsw = MEM_callocN(sizeof(NodeSizeWidget), "size widget op data");

  op->customdata = nsw;
  nsw->mxstart = snode->runtime->cursor[0] * UI_DPI_FAC;
  nsw->mystart = snode->runtime->cursor[1] * UI_DPI_FAC;

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
    NodeSizeWidget *nsw = op->customdata;

    node->locx = nsw->oldlocx;
    node->locy = nsw->oldlocy;
    node->offsetx = nsw->oldoffsetx;
    node->offsety = nsw->oldoffsety;
    node->width = nsw->oldwidth;
    node->height = nsw->oldheight;
  }

  MEM_freeN(op->customdata);
  op->customdata = NULL;
}

static int node_resize_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  bNode *node = nodeGetActive(snode->edittree);
  NodeSizeWidget *nsw = op->customdata;

  switch (event->type) {
    case MOUSEMOVE: {
      float mx, my;
      UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &mx, &my);
      float dx = (mx - nsw->mxstart) / UI_DPI_FAC;
      float dy = (my - nsw->mystart) / UI_DPI_FAC;

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

        /* height works the other way round ... */
        {
          float heightmin = UI_DPI_FAC * node->typeinfo->minheight;
          float heightmax = UI_DPI_FAC * node->typeinfo->maxheight;
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

        /* XXX make callback? */
        if (node->type == NODE_FRAME) {
          /* keep the offset symmetric around center point */
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
  }

  return OPERATOR_RUNNING_MODAL;
}

static int node_resize_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  bNode *node = nodeGetActive(snode->edittree);
  int dir;

  if (node) {
    float cursor[2];

    /* convert mouse coordinates to v2d space */
    UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &cursor[0], &cursor[1]);
    dir = node->typeinfo->resize_area_func(node, cursor[0], cursor[1]);
    if (dir != 0) {
      node_resize_init(C, op, event, node, dir);
      return OPERATOR_RUNNING_MODAL;
    }
  }
  return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
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

/* ********************** hidden sockets ******************** */

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

void node_set_hidden_sockets(SpaceNode *snode, bNode *node, int set)
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
    /* hide unused sockets */
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      if (sock->link == NULL) {
        sock->flag |= SOCK_HIDDEN;
      }
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      if (nodeCountSocketLinks(snode->edittree, sock) == 0) {
        sock->flag |= SOCK_HIDDEN;
      }
    }
  }
}

/* checks snode->mouse position, and returns found node/socket */
static bool cursor_isect_multi_input_socket(const float cursor[2], const bNodeSocket *socket)
{
  const float node_socket_height = node_socket_calculate_height(socket);
  const rctf multi_socket_rect = {
      .xmin = socket->locx - NODE_SOCKSIZE * 4,
      .xmax = socket->locx + NODE_SOCKSIZE,
      .ymin = socket->locy - node_socket_height * 0.5 - NODE_SOCKSIZE * 2.0f,
      .ymax = socket->locy + node_socket_height * 0.5 + NODE_SOCKSIZE * 2.0f,
  };
  if (BLI_rctf_isect_pt(&multi_socket_rect, cursor[0], cursor[1])) {
    return true;
  }
  return false;
}

/* type is SOCK_IN and/or SOCK_OUT */
int node_find_indicated_socket(
    SpaceNode *snode, bNode **nodep, bNodeSocket **sockp, float cursor[2], int in_out)
{
  rctf rect;

  *nodep = NULL;
  *sockp = NULL;

  /* check if we click in a socket */
  LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
    BLI_rctf_init_pt_radius(&rect, cursor, NODE_SOCKSIZE + 4);

    if (!(node->flag & NODE_HIDDEN)) {
      /* extra padding inside and out - allow dragging on the text areas too */
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
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
        if (!nodeSocketIsHidden(sock)) {
          if (sock->flag & SOCK_MULTI_INPUT && !(node->flag & NODE_HIDDEN)) {
            if (cursor_isect_multi_input_socket(cursor, sock)) {
              if (node == visible_node(snode, &rect)) {
                *nodep = node;
                *sockp = sock;
                return 1;
              }
            }
          }
          else if (BLI_rctf_isect_pt(&rect, sock->locx, sock->locy)) {
            if (node == visible_node(snode, &rect)) {
              *nodep = node;
              *sockp = sock;
              return 1;
            }
          }
        }
      }
    }
    if (in_out & SOCK_OUT) {
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
        if (!nodeSocketIsHidden(sock)) {
          if (BLI_rctf_isect_pt(&rect, sock->locx, sock->locy)) {
            if (node == visible_node(snode, &rect)) {
              *nodep = node;
              *sockp = sock;
              return 1;
            }
          }
        }
      }
    }
  }

  return 0;
}

/* ****************** Duplicate *********************** */

static void node_duplicate_reparent_recursive(bNode *node)
{
  bNode *parent;

  node->flag |= NODE_TEST;

  /* find first selected parent */
  for (parent = node->parent; parent; parent = parent->parent) {
    if (parent->flag & SELECT) {
      if (!(parent->flag & NODE_TEST)) {
        node_duplicate_reparent_recursive(parent);
      }
      break;
    }
  }
  /* reparent node copy to parent copy */
  if (parent) {
    nodeDetachNode(node->new_node);
    nodeAttachNode(node->new_node, parent->new_node);
  }
}

static int node_duplicate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  const bool keep_inputs = RNA_boolean_get(op->ptr, "keep_inputs");
  bool do_tag_update = false;

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  bNode *lastnode = ntree->nodes.last;
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->flag & SELECT) {
      BKE_node_copy_store_new_pointers(ntree, node, LIB_ID_COPY_DEFAULT);

      /* To ensure redraws or re-renders happen. */
      ED_node_tag_update_id(snode->id);
    }

    /* make sure we don't copy new nodes again! */
    if (node == lastnode) {
      break;
    }
  }

  /* copy links between selected nodes
   * NB: this depends on correct node->new_node and sock->new_sock pointers from above copy!
   */
  bNodeLink *lastlink = ntree->links.last;
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    /* This creates new links between copied nodes.
     * If keep_inputs is set, also copies input links from unselected (when fromnode==NULL)!
     */
    if (link->tonode && (link->tonode->flag & NODE_SELECT) &&
        (keep_inputs || (link->fromnode && (link->fromnode->flag & NODE_SELECT)))) {
      bNodeLink *newlink = MEM_callocN(sizeof(bNodeLink), "bNodeLink");
      newlink->flag = link->flag;
      newlink->tonode = link->tonode->new_node;
      newlink->tosock = link->tosock->new_sock;
      if (link->fromnode && (link->fromnode->flag & NODE_SELECT)) {
        newlink->fromnode = link->fromnode->new_node;
        newlink->fromsock = link->fromsock->new_sock;
      }
      else {
        /* input node not copied, this keeps the original input linked */
        newlink->fromnode = link->fromnode;
        newlink->fromsock = link->fromsock;
      }

      BLI_addtail(&ntree->links, newlink);
    }

    /* make sure we don't copy new links again! */
    if (link == lastlink) {
      break;
    }
  }

  /* clear flags for recursive depth-first iteration */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->flag &= ~NODE_TEST;
  }
  /* reparent copied nodes */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if ((node->flag & SELECT) && !(node->flag & NODE_TEST)) {
      node_duplicate_reparent_recursive(node);
    }

    /* only has to check old nodes */
    if (node == lastnode) {
      break;
    }
  }

  /* deselect old nodes, select the copies instead */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->flag & SELECT) {
      /* has been set during copy above */
      bNode *newnode = node->new_node;

      nodeSetSelected(node, false);
      node->flag &= ~(NODE_ACTIVE | NODE_ACTIVE_TEXTURE);
      nodeSetSelected(newnode, true);

      do_tag_update |= (do_tag_update || node_connected_to_output(bmain, ntree, newnode));
    }

    /* make sure we don't copy new nodes again! */
    if (node == lastnode) {
      break;
    }
  }

  ntreeUpdateTree(CTX_data_main(C), snode->edittree);

  snode_notify(C, snode);
  if (do_tag_update) {
    snode_dag_update(C, snode);
  }

  return OPERATOR_FINISHED;
}

void NODE_OT_duplicate(wmOperatorType *ot)
{
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
      ot->srna, "keep_inputs", 0, "Keep Inputs", "Keep the input links to duplicated nodes");
}

bool ED_node_select_check(ListBase *lb)
{
  LISTBASE_FOREACH (bNode *, node, lb) {
    if (node->flag & NODE_SELECT) {
      return true;
    }
  }

  return false;
}

void ED_node_select_all(ListBase *lb, int action)
{
  if (action == SEL_TOGGLE) {
    if (ED_node_select_check(lb)) {
      action = SEL_DESELECT;
    }
    else {
      action = SEL_SELECT;
    }
  }

  LISTBASE_FOREACH (bNode *, node, lb) {
    switch (action) {
      case SEL_SELECT:
        nodeSetSelected(node, true);
        break;
      case SEL_DESELECT:
        nodeSetSelected(node, false);
        break;
      case SEL_INVERT:
        nodeSetSelected(node, !(node->flag & SELECT));
        break;
    }
  }
}

/* ******************************** */
/* XXX some code needing updating to operators. */

/* goes over all scenes, reads render layers */
static int node_read_viewlayers_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  Scene *curscene = CTX_data_scene(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  /* first tag scenes unread */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    scene->id.tag |= LIB_TAG_DOIT;
  }

  LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
    if ((node->type == CMP_NODE_R_LAYERS) ||
        (node->type == CMP_NODE_CRYPTOMATTE && node->custom1 == CMP_CRYPTOMATTE_SRC_RENDER)) {
      ID *id = node->id;
      if (id == NULL) {
        continue;
      }
      if (id->tag & LIB_TAG_DOIT) {
        RE_ReadRenderResult(curscene, (Scene *)id);
        ntreeCompositTagRender((Scene *)id);
        id->tag &= ~LIB_TAG_DOIT;
      }
    }
  }

  snode_notify(C, snode);
  snode_dag_update(C, snode);

  return OPERATOR_FINISHED;
}

void NODE_OT_read_viewlayers(wmOperatorType *ot)
{

  ot->name = "Read View Layers";
  ot->idname = "NODE_OT_read_viewlayers";
  ot->description = "Read all render layers of all used scenes";

  ot->exec = node_read_viewlayers_exec;

  ot->poll = composite_node_active;

  /* flags */
  ot->flag = 0;
}

int node_render_changed_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *sce = CTX_data_scene(C);

  /* This is actually a test whether scene is used by the compositor or not.
   * All the nodes are using same render result, so there is no need to do
   * anything smart about check how exactly scene is used. */
  bNode *node = NULL;
  LISTBASE_FOREACH (bNode *, node_iter, &sce->nodetree->nodes) {
    if (node_iter->id == (ID *)sce) {
      node = node_iter;
      break;
    }
  }

  if (node) {
    ViewLayer *view_layer = BLI_findlink(&sce->view_layers, node->custom1);

    if (view_layer) {
      PointerRNA op_ptr;

      WM_operator_properties_create(&op_ptr, "RENDER_OT_render");
      RNA_string_set(&op_ptr, "layer", view_layer->name);
      RNA_string_set(&op_ptr, "scene", sce->id.name + 2);

      /* to keep keypositions */
      sce->r.scemode |= R_NO_FRAME_UPDATE;

      WM_operator_name_call(C, "RENDER_OT_render", WM_OP_INVOKE_DEFAULT, &op_ptr);

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

/* ****************** Hide operator *********************** */

static void node_flag_toggle_exec(SpaceNode *snode, int toggle_flag)
{
  int tot_eq = 0, tot_neq = 0;

  /* Toggles the flag on all selected nodes.
   * If the flag is set on all nodes it is unset.
   * If the flag is not set on all nodes, it is set.
   */

  LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & SELECT) {

      if (toggle_flag == NODE_PREVIEW && (node->typeinfo->flag & NODE_PREVIEW) == 0) {
        continue;
      }
      if (toggle_flag == NODE_OPTIONS &&
          !(node->typeinfo->draw_buttons || node->typeinfo->draw_buttons_ex)) {
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
  LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & SELECT) {

      if (toggle_flag == NODE_PREVIEW && (node->typeinfo->flag & NODE_PREVIEW) == 0) {
        continue;
      }
      if (toggle_flag == NODE_OPTIONS &&
          !(node->typeinfo->draw_buttons || node->typeinfo->draw_buttons_ex)) {
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

static int node_hide_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);

  /* sanity checking (poll callback checks this already) */
  if ((snode == NULL) || (snode->edittree == NULL)) {
    return OPERATOR_CANCELLED;
  }

  node_flag_toggle_exec(snode, NODE_HIDDEN);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

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

static int node_preview_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);

  /* sanity checking (poll callback checks this already) */
  if ((snode == NULL) || (snode->edittree == NULL)) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  node_flag_toggle_exec(snode, NODE_PREVIEW);

  snode_notify(C, snode);

  return OPERATOR_FINISHED;
}

void NODE_OT_preview_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Node Preview";
  ot->description = "Toggle preview display for selected nodes";
  ot->idname = "NODE_OT_preview_toggle";

  /* callbacks */
  ot->exec = node_preview_toggle_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int node_options_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);

  /* sanity checking (poll callback checks this already) */
  if ((snode == NULL) || (snode->edittree == NULL)) {
    return OPERATOR_CANCELLED;
  }

  node_flag_toggle_exec(snode, NODE_OPTIONS);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

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

static int node_socket_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);

  /* sanity checking (poll callback checks this already) */
  if ((snode == NULL) || (snode->edittree == NULL)) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  /* Toggle for all selected nodes */
  bool hidden = false;
  LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & SELECT) {
      if (node_has_hidden_sockets(node)) {
        hidden = true;
        break;
      }
    }
  }

  LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & SELECT) {
      node_set_hidden_sockets(snode, node, !hidden);
    }
  }

  ntreeUpdateTree(CTX_data_main(C), snode->edittree);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

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

/* ****************** Mute operator *********************** */

static int node_mute_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  bool do_tag_update = false;

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
    /* Only allow muting of nodes having a mute func! */
    if ((node->flag & SELECT) && node->typeinfo->update_internal_links) {
      node->flag ^= NODE_MUTED;
      snode_update(snode, node);
      do_tag_update |= (do_tag_update || node_connected_to_output(bmain, snode->edittree, node));
    }
  }

  do_tag_update |= ED_node_is_geometry(snode);

  snode_notify(C, snode);
  if (do_tag_update) {
    snode_dag_update(C, snode);
  }

  return OPERATOR_FINISHED;
}

void NODE_OT_mute_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Node Mute";
  ot->description = "Toggle muting of the nodes";
  ot->idname = "NODE_OT_mute_toggle";

  /* callbacks */
  ot->exec = node_mute_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** Delete operator ******************* */

static int node_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  bool do_tag_update = false;

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & SELECT) {
      do_tag_update |= (do_tag_update || node_connected_to_output(bmain, snode->edittree, node));
      nodeRemoveNode(bmain, snode->edittree, node, true);
    }
  }

  do_tag_update |= ED_node_is_geometry(snode);

  ntreeUpdateTree(CTX_data_main(C), snode->edittree);

  snode_notify(C, snode);
  if (do_tag_update) {
    snode_dag_update(C, snode);
  }

  return OPERATOR_FINISHED;
}

void NODE_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete";
  ot->description = "Delete selected nodes";
  ot->idname = "NODE_OT_delete";

  /* api callbacks */
  ot->exec = node_delete_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** Switch View ******************* */

static bool node_switch_view_poll(bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  if (snode && snode->edittree) {
    return true;
  }

  return false;
}

static int node_switch_view_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & SELECT) {
      /* call the update function from the Switch View node */
      node->update = NODE_UPDATE_OPERATOR;
    }
  }

  ntreeUpdateTree(CTX_data_main(C), snode->edittree);

  snode_notify(C, snode);
  snode_dag_update(C, snode);

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

/* ****************** Delete with reconnect ******************* */
static int node_delete_reconnect_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & SELECT) {
      nodeInternalRelink(snode->edittree, node);
      nodeRemoveNode(bmain, snode->edittree, node, true);
    }
  }

  ntreeUpdateTree(CTX_data_main(C), snode->edittree);

  snode_notify(C, snode);
  snode_dag_update(C, snode);

  return OPERATOR_FINISHED;
}

void NODE_OT_delete_reconnect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete with Reconnect";
  ot->description = "Delete nodes; will reconnect nodes as if deletion was muted";
  ot->idname = "NODE_OT_delete_reconnect";

  /* api callbacks */
  ot->exec = node_delete_reconnect_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** File Output Add Socket  ******************* */

static int node_output_file_add_socket_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  PointerRNA ptr = CTX_data_pointer_get(C, "node");
  bNodeTree *ntree = NULL;
  bNode *node = NULL;
  char file_path[MAX_NAME];

  if (ptr.data) {
    node = ptr.data;
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

  snode_notify(C, snode);

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

/* ****************** Multi File Output Remove Socket  ******************* */

static int node_output_file_remove_active_socket_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);
  PointerRNA ptr = CTX_data_pointer_get(C, "node");
  bNodeTree *ntree = NULL;
  bNode *node = NULL;

  if (ptr.data) {
    node = ptr.data;
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

  snode_notify(C, snode);

  return OPERATOR_FINISHED;
}

void NODE_OT_output_file_remove_active_socket(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove File Node Socket";
  ot->description = "Remove active input from a file output node";
  ot->idname = "NODE_OT_output_file_remove_active_socket";

  /* callbacks */
  ot->exec = node_output_file_remove_active_socket_exec;
  ot->poll = composite_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** Multi File Output Move Socket  ******************* */

static int node_output_file_move_active_socket_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  PointerRNA ptr = CTX_data_pointer_get(C, "node");
  bNode *node = NULL;

  if (ptr.data) {
    node = ptr.data;
  }
  else if (snode && snode->edittree) {
    node = nodeGetActive(snode->edittree);
  }

  if (!node || node->type != CMP_NODE_OUTPUT_FILE) {
    return OPERATOR_CANCELLED;
  }

  NodeImageMultiFile *nimf = node->storage;

  bNodeSocket *sock = BLI_findlink(&node->inputs, nimf->active_input);
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

  snode_notify(C, snode);

  return OPERATOR_FINISHED;
}

void NODE_OT_output_file_move_active_socket(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {1, "UP", 0, "Up", ""}, {2, "DOWN", 0, "Down", ""}, {0, NULL, 0, NULL, NULL}};

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

/* ****************** Copy Node Color ******************* */

static int node_copy_color_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;

  if (!ntree) {
    return OPERATOR_CANCELLED;
  }
  bNode *node = nodeGetActive(ntree);
  if (!node) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (bNode *, node_iter, &ntree->nodes) {
    if (node_iter->flag & NODE_SELECT && node_iter != node) {
      if (node->flag & NODE_CUSTOM_COLOR) {
        node_iter->flag |= NODE_CUSTOM_COLOR;
        copy_v3_v3(node_iter->color, node->color);
      }
      else {
        node_iter->flag &= ~NODE_CUSTOM_COLOR;
      }
    }
  }

  ED_node_sort(ntree);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

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

/* ****************** Copy to clipboard ******************* */

static int node_clipboard_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  /* clear current clipboard */
  BKE_node_clipboard_clear();
  BKE_node_clipboard_init(ntree);

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->flag & SELECT) {
      /* No ID refcounting, this node is virtual,
       * detached from any actual Blender data currently. */
      bNode *new_node = BKE_node_copy_store_new_pointers(
          NULL, node, LIB_ID_CREATE_NO_USER_REFCOUNT | LIB_ID_CREATE_NO_MAIN);
      BKE_node_clipboard_add_node(new_node);
    }
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->flag & SELECT) {
      bNode *new_node = node->new_node;

      /* ensure valid pointers */
      if (new_node->parent) {
        /* parent pointer must be redirected to new node or detached if parent is
         * not copied */
        if (new_node->parent->flag & NODE_SELECT) {
          new_node->parent = new_node->parent->new_node;
        }
        else {
          nodeDetachNode(new_node);
        }
      }
    }
  }

  /* copy links between selected nodes
   * NB: this depends on correct node->new_node and sock->new_sock pointers from above copy!
   */
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    /* This creates new links between copied nodes. */
    if (link->tonode && (link->tonode->flag & NODE_SELECT) && link->fromnode &&
        (link->fromnode->flag & NODE_SELECT)) {
      bNodeLink *newlink = MEM_callocN(sizeof(bNodeLink), "bNodeLink");
      newlink->flag = link->flag;
      newlink->tonode = link->tonode->new_node;
      newlink->tosock = link->tosock->new_sock;
      newlink->fromnode = link->fromnode->new_node;
      newlink->fromsock = link->fromsock->new_sock;

      BKE_node_clipboard_add_link(newlink);
    }
  }

  return OPERATOR_FINISHED;
}

void NODE_OT_clipboard_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy to Clipboard";
  ot->description = "Copies selected nodes to the clipboard";
  ot->idname = "NODE_OT_clipboard_copy";

  /* api callbacks */
  ot->exec = node_clipboard_copy_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** Paste from clipboard ******************* */

static int node_clipboard_paste_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;

  /* validate pointers in the clipboard */
  bool is_clipboard_valid = BKE_node_clipboard_validate();
  const ListBase *clipboard_nodes_lb = BKE_node_clipboard_get_nodes();
  const ListBase *clipboard_links_lb = BKE_node_clipboard_get_links();

  if (BLI_listbase_is_empty(clipboard_nodes_lb)) {
    BKE_report(op->reports, RPT_ERROR, "Clipboard is empty");
    return OPERATOR_CANCELLED;
  }

  if (BKE_node_clipboard_get_type() != ntree->type) {
    BKE_report(op->reports, RPT_ERROR, "Clipboard nodes are an incompatible type");
    return OPERATOR_CANCELLED;
  }

  /* only warn */
  if (is_clipboard_valid == false) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Some nodes references could not be restored, will be left empty");
  }

  /* make sure all clipboard nodes would be valid in the target tree */
  bool all_nodes_valid = true;
  LISTBASE_FOREACH (bNode *, node, clipboard_nodes_lb) {
    if (!node->typeinfo->poll_instance || !node->typeinfo->poll_instance(node, ntree)) {
      all_nodes_valid = false;
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Cannot add node %s into node tree %s",
                  node->name,
                  ntree->id.name + 2);
    }
  }
  if (!all_nodes_valid) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  /* deselect old nodes */
  node_deselect_all(snode);

  /* calculate "barycenter" for placing on mouse cursor */
  float center[2] = {0.0f, 0.0f};
  int num_nodes = 0;
  LISTBASE_FOREACH_INDEX (bNode *, node, clipboard_nodes_lb, num_nodes) {
    center[0] += BLI_rctf_cent_x(&node->totr);
    center[1] += BLI_rctf_cent_y(&node->totr);
  }
  mul_v2_fl(center, 1.0 / num_nodes);

  /* copy nodes from clipboard */
  LISTBASE_FOREACH (bNode *, node, clipboard_nodes_lb) {
    bNode *new_node = BKE_node_copy_store_new_pointers(ntree, node, LIB_ID_COPY_DEFAULT);

    /* pasted nodes are selected */
    nodeSetSelected(new_node, true);
  }

  /* reparent copied nodes */
  LISTBASE_FOREACH (bNode *, node, clipboard_nodes_lb) {
    bNode *new_node = node->new_node;
    if (new_node->parent) {
      new_node->parent = new_node->parent->new_node;
    }
  }

  LISTBASE_FOREACH (bNodeLink *, link, clipboard_links_lb) {
    nodeAddLink(ntree,
                link->fromnode->new_node,
                link->fromsock->new_sock,
                link->tonode->new_node,
                link->tosock->new_sock);
  }

  Main *bmain = CTX_data_main(C);
  ntreeUpdateTree(bmain, snode->edittree);

  snode_notify(C, snode);
  snode_dag_update(C, snode);
  /* Pasting nodes can create arbitrary new relations, because nodes can reference IDs. */
  DEG_relations_tag_update(bmain);

  return OPERATOR_FINISHED;
}

void NODE_OT_clipboard_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste from Clipboard";
  ot->description = "Pastes nodes from the clipboard to the active node tree";
  ot->idname = "NODE_OT_clipboard_paste";

  /* api callbacks */
  ot->exec = node_clipboard_paste_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** Add interface socket operator *********************/

static bNodeSocket *ntree_get_active_interface_socket(ListBase *lb)
{
  LISTBASE_FOREACH (bNodeSocket *, socket, lb) {
    if (socket->flag & SELECT) {
      return socket;
    }
  }
  return NULL;
}

static int ntree_socket_add_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;

  PointerRNA ntree_ptr;
  RNA_id_pointer_create((ID *)ntree, &ntree_ptr);

  const eNodeSocketInOut in_out = RNA_enum_get(op->ptr, "in_out");
  ListBase *sockets = (in_out == SOCK_IN) ? &ntree->inputs : &ntree->outputs;

  const char *default_name = (in_out == SOCK_IN) ? "Input" : "Output";
  bNodeSocket *active_sock = ntree_get_active_interface_socket(sockets);

  bNodeSocket *sock;
  if (active_sock) {
    /* insert a copy of the active socket right after it */
    sock = ntreeInsertSocketInterface(
        ntree, in_out, active_sock->idname, active_sock->next, active_sock->name);
    /* XXX this only works for actual sockets, not interface templates! */
    /*nodeSocketCopyValue(sock, &ntree_ptr, active_sock, &ntree_ptr);*/
  }
  else {
    /* XXX TODO define default socket type for a tree! */
    sock = ntreeAddSocketInterface(ntree, in_out, "NodeSocketFloat", default_name);
  }

  /* Deactivate sockets. */
  LISTBASE_FOREACH (bNodeSocket *, socket_iter, sockets) {
    socket_iter->flag &= ~SELECT;
  }
  /* make the new socket active */
  sock->flag |= SELECT;

  ntreeUpdateTree(CTX_data_main(C), ntree);

  snode_notify(C, snode);
  snode_dag_update(C, snode);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

  return OPERATOR_FINISHED;
}

void NODE_OT_tree_socket_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Node Tree Interface Socket";
  ot->description = "Add an input or output socket to the current node tree";
  ot->idname = "NODE_OT_tree_socket_add";

  /* api callbacks */
  ot->exec = ntree_socket_add_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "in_out", rna_enum_node_socket_in_out_items, SOCK_IN, "Socket Type", "");
}

/********************** Remove interface socket operator *********************/

static int ntree_socket_remove_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  const eNodeSocketInOut in_out = RNA_enum_get(op->ptr, "in_out");

  bNodeSocket *iosock = ntree_get_active_interface_socket(in_out == SOCK_IN ? &ntree->inputs :
                                                                              &ntree->outputs);
  if (iosock == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* preferably next socket becomes active, otherwise try previous socket */
  bNodeSocket *active_sock = (iosock->next ? iosock->next : iosock->prev);
  ntreeRemoveSocketInterface(ntree, iosock);

  /* set active socket */
  if (active_sock) {
    active_sock->flag |= SELECT;
  }

  ntreeUpdateTree(CTX_data_main(C), ntree);

  snode_notify(C, snode);
  snode_dag_update(C, snode);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

  return OPERATOR_FINISHED;
}

void NODE_OT_tree_socket_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Node Tree Interface Socket";
  ot->description = "Remove an input or output socket to the current node tree";
  ot->idname = "NODE_OT_tree_socket_remove";

  /* api callbacks */
  ot->exec = ntree_socket_remove_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  RNA_def_enum(ot->srna, "in_out", rna_enum_node_socket_in_out_items, SOCK_IN, "Socket Type", "");
}

/********************** Move interface socket operator *********************/

static const EnumPropertyItem move_direction_items[] = {
    {1, "UP", 0, "Up", ""},
    {2, "DOWN", 0, "Down", ""},
    {0, NULL, 0, NULL, NULL},
};

static int ntree_socket_move_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  int direction = RNA_enum_get(op->ptr, "direction");

  const eNodeSocketInOut in_out = RNA_enum_get(op->ptr, "in_out");
  ListBase *sockets = in_out == SOCK_IN ? &ntree->inputs : &ntree->outputs;

  bNodeSocket *iosock = ntree_get_active_interface_socket(sockets);

  if (iosock == NULL) {
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

  ntree->update |= NTREE_UPDATE_GROUP;
  ntreeUpdateTree(CTX_data_main(C), ntree);

  snode_notify(C, snode);
  snode_dag_update(C, snode);

  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

  return OPERATOR_FINISHED;
}

void NODE_OT_tree_socket_move(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move Node Tree Socket";
  ot->description = "Move a socket up or down in the current node tree's sockets stack";
  ot->idname = "NODE_OT_tree_socket_move";

  /* api callbacks */
  ot->exec = ntree_socket_move_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "direction", move_direction_items, 1, "Direction", "");
  RNA_def_enum(ot->srna, "in_out", rna_enum_node_socket_in_out_items, SOCK_IN, "Socket Type", "");
}

/* ********************** Shader Script Update ******************/

static bool node_shader_script_update_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  const RenderEngineType *type = RE_engines_find(scene->r.engine);
  SpaceNode *snode = CTX_wm_space_node(C);

  /* test if we have a render engine that supports shaders scripts */
  if (!(type && type->update_script_node)) {
    return 0;
  }

  /* see if we have a shader script node in context */
  bNode *node = CTX_data_pointer_get_type(C, "node", &RNA_ShaderNodeScript).data;

  if (!node && snode && snode->edittree) {
    node = nodeGetActive(snode->edittree);
  }

  if (node && node->type == SH_NODE_SCRIPT) {
    NodeShaderScript *nss = node->storage;

    if (node->id || nss->filepath[0]) {
      return ED_operator_node_editable(C);
    }
  }

  /* see if we have a text datablock in context */
  Text *text = CTX_data_pointer_get_type(C, "edit_text", &RNA_Text).data;
  if (text) {
    return 1;
  }

  /* we don't check if text datablock is actually in use, too slow for poll */

  return 0;
}

/* recursively check for script nodes in groups using this text and update */
static bool node_shader_script_update_text_recursive(RenderEngine *engine,
                                                     RenderEngineType *type,
                                                     bNodeTree *ntree,
                                                     Text *text)
{
  bool found = false;

  ntree->done = true;

  /* update each script that is using this text datablock */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == NODE_GROUP) {
      bNodeTree *ngroup = (bNodeTree *)node->id;
      if (ngroup && !ngroup->done) {
        found |= node_shader_script_update_text_recursive(engine, type, ngroup, text);
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

  /* get node */
  bNodeTree *ntree_base = NULL;
  bNode *node = NULL;
  if (nodeptr.data) {
    ntree_base = (bNodeTree *)nodeptr.owner_id;
    node = nodeptr.data;
  }
  else if (snode && snode->edittree) {
    ntree_base = snode->edittree;
    node = nodeGetActive(snode->edittree);
  }

  if (node) {
    /* update single node */
    type->update_script_node(engine, ntree_base, node);

    found = true;
  }
  else {
    /* update all nodes using text datablock */
    Text *text = CTX_data_pointer_get_type(C, "edit_text", &RNA_Text).data;

    if (text) {
      /* clear flags for recursion check */
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_SHADER) {
          ntree->done = false;
        }
      }
      FOREACH_NODETREE_END;

      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_SHADER) {
          if (!ntree->done) {
            found |= node_shader_script_update_text_recursive(engine, type, ntree, text);
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

/* ********************** Viewer border ******************/

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

  *fx = (bufx > 0.0f ? ((float)x - 0.5f * region->winx - snode->xof) / bufx + 0.5f : 0.0f);
  *fy = (bufy > 0.0f ? ((float)y - 0.5f * region->winy - snode->yof) / bufy + 0.5f : 0.0f);
}

static int viewer_border_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  void *lock;

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock);

  if (ibuf) {
    ARegion *region = CTX_wm_region(C);
    SpaceNode *snode = CTX_wm_space_node(C);
    bNodeTree *btree = snode->nodetree;
    rcti rect;
    rctf rectf;

    /* get border from operator */
    WM_operator_properties_border_to_rcti(op, &rect);

    /* convert border to unified space within backdrop image */
    viewer_border_corner_to_backdrop(
        snode, region, rect.xmin, rect.ymin, ibuf->x, ibuf->y, &rectf.xmin, &rectf.ymin);

    viewer_border_corner_to_backdrop(
        snode, region, rect.xmax, rect.ymax, ibuf->x, ibuf->y, &rectf.xmax, &rectf.ymax);

    /* clamp coordinates */
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

      snode_notify(C, snode);
      WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);
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

static int clear_viewer_border_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *btree = snode->nodetree;

  btree->flag &= ~NTREE_VIEWER_BORDER;
  snode_notify(C, snode);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

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

/* ****************** Cryptomatte Add Socket  ******************* */

static int node_cryptomatte_add_socket_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);
  PointerRNA ptr = CTX_data_pointer_get(C, "node");
  bNodeTree *ntree = NULL;
  bNode *node = NULL;

  if (ptr.data) {
    node = ptr.data;
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

  snode_notify(C, snode);

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

/* ****************** Cryptomatte Remove Socket  ******************* */

static int node_cryptomatte_remove_socket_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);
  PointerRNA ptr = CTX_data_pointer_get(C, "node");
  bNodeTree *ntree = NULL;
  bNode *node = NULL;

  if (ptr.data) {
    node = ptr.data;
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

  snode_notify(C, snode);

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
