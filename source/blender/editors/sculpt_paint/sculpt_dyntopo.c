/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_undo.h"
#include "sculpt_intern.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>

void SCULPT_dynamic_topology_triangulate(BMesh *bm)
{
  if (bm->totloop != bm->totface * 3) {
    BM_mesh_triangulate(
        bm, MOD_TRIANGULATE_QUAD_BEAUTY, MOD_TRIANGULATE_NGON_EARCLIP, 4, false, NULL, NULL, NULL);
  }
}

void SCULPT_pbvh_clear(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  /* Clear out any existing DM and PBVH. */
  if (ss->pbvh) {
    BKE_pbvh_free(ss->pbvh);
    ss->pbvh = NULL;
  }

  MEM_SAFE_FREE(ss->pmap);

  MEM_SAFE_FREE(ss->pmap_mem);

  BKE_object_free_derived_caches(ob);

  /* Tag to rebuild PBVH in depsgraph. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
}

void SCULPT_dynamic_topology_enable_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = ob->data;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(me);

  SCULPT_pbvh_clear(ob);

  ss->bm_smooth_shading = (scene->toolsettings->sculpt->flags & SCULPT_DYNTOPO_SMOOTH_SHADING) !=
                          0;

  /* Dynamic topology doesn't ensure selection state is valid, so remove T36280. */
  BKE_mesh_mselect_clear(me);

  /* Create triangles-only BMesh. */
  ss->bm = BM_mesh_create(&allocsize,
                          &((struct BMeshCreateParams){
                              .use_toolflags = false,
                          }));

  BM_mesh_bm_from_me(ss->bm,
                     me,
                     (&(struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                         .calc_vert_normal = true,
                         .use_shapekey = true,
                         .active_shapekey = ob->shapenr,
                     }));
  SCULPT_dynamic_topology_triangulate(ss->bm);

  BM_data_layer_add(ss->bm, &ss->bm->vdata, CD_PAINT_MASK);

  /* Make sure the data for existing faces are initialized. */
  if (me->totpoly != ss->bm->totface) {
    BM_mesh_normals_update(ss->bm);
  }

  /* Enable dynamic topology. */
  me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Enable logging for undo/redo. */
  ss->bm_log = BM_log_create(ss->bm);

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_update_tagged(depsgraph, bmain);
}

/* Free the sculpt BMesh and BMLog
 *
 * If 'unode' is given, the BMesh's data is copied out to the unode
 * before the BMesh is deleted so that it can be restored from. */
static void SCULPT_dynamic_topology_disable_ex(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = ob->data;

  if (ss->attrs.dyntopo_node_id_vertex) {
    BKE_sculpt_attribute_destroy(ob, ss->attrs.dyntopo_node_id_vertex);
  }

  if (ss->attrs.dyntopo_node_id_face) {
    BKE_sculpt_attribute_destroy(ob, ss->attrs.dyntopo_node_id_face);
  }

  SCULPT_pbvh_clear(ob);

  if (unode) {
    /* Free all existing custom data. */
    CustomData_free(&me->vdata, me->totvert);
    CustomData_free(&me->edata, me->totedge);
    CustomData_free(&me->fdata, me->totface);
    CustomData_free(&me->ldata, me->totloop);
    CustomData_free(&me->pdata, me->totpoly);

    /* Copy over stored custom data. */
    SculptUndoNodeGeometry *geometry = &unode->geometry_bmesh_enter;
    me->totvert = geometry->totvert;
    me->totloop = geometry->totloop;
    me->totpoly = geometry->totpoly;
    me->totedge = geometry->totedge;
    me->totface = 0;
    CustomData_copy(
        &geometry->vdata, &me->vdata, CD_MASK_MESH.vmask, CD_DUPLICATE, geometry->totvert);
    CustomData_copy(
        &geometry->edata, &me->edata, CD_MASK_MESH.emask, CD_DUPLICATE, geometry->totedge);
    CustomData_copy(
        &geometry->ldata, &me->ldata, CD_MASK_MESH.lmask, CD_DUPLICATE, geometry->totloop);
    CustomData_copy(
        &geometry->pdata, &me->pdata, CD_MASK_MESH.pmask, CD_DUPLICATE, geometry->totpoly);
  }
  else {
    BKE_sculptsession_bm_to_me(ob, true);

    /* Reset Face Sets as they are no longer valid. */
    CustomData_free_layer_named(&me->pdata, ".sculpt_face_set", me->totpoly);
    me->face_sets_color_default = 1;

    /* Sync the visibility to vertices manually as the pmap is still not initialized. */
    bool *hide_vert = (bool *)CustomData_get_layer_named(&me->vdata, CD_PROP_BOOL, ".hide_vert");
    if (hide_vert != NULL) {
      memset(hide_vert, 0, sizeof(bool) * me->totvert);
    }
  }

  /* Clear data. */
  me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Typically valid but with global-undo they can be NULL, see: T36234. */
  if (ss->bm) {
    BM_mesh_free(ss->bm);
    ss->bm = NULL;
  }
  if (ss->bm_log) {
    BM_log_free(ss->bm_log);
    ss->bm_log = NULL;
  }

  BKE_particlesystem_reset_all(ob);
  BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_OUTDATED);

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_update_tagged(depsgraph, bmain);
}

void SCULPT_dynamic_topology_disable(bContext *C, SculptUndoNode *unode)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  SCULPT_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, unode);
}

void sculpt_dynamic_topology_disable_with_undo(Main *bmain,
                                               Depsgraph *depsgraph,
                                               Scene *scene,
                                               Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->bm != NULL) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != NULL) : true;
    if (use_undo) {
      SCULPT_undo_push_begin_ex(ob, "Dynamic topology disable");
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_END);
    }
    SCULPT_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, NULL);
    if (use_undo) {
      SCULPT_undo_push_end(ob);
    }
  }
}

static void sculpt_dynamic_topology_enable_with_undo(Main *bmain,
                                                     Depsgraph *depsgraph,
                                                     Scene *scene,
                                                     Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->bm == NULL) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != NULL) : true;
    if (use_undo) {
      SCULPT_undo_push_begin_ex(ob, "Dynamic topology enable");
    }
    SCULPT_dynamic_topology_enable_ex(bmain, depsgraph, scene, ob);
    if (use_undo) {
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_BEGIN);
      SCULPT_undo_push_end(ob);
    }
  }
}

static int sculpt_dynamic_topology_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  WM_cursor_wait(true);

  if (ss->bm) {
    sculpt_dynamic_topology_disable_with_undo(bmain, depsgraph, scene, ob);
  }
  else {
    sculpt_dynamic_topology_enable_with_undo(bmain, depsgraph, scene, ob);
  }

  WM_cursor_wait(false);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);

  return OPERATOR_FINISHED;
}

static int dyntopo_warning_popup(bContext *C, wmOperatorType *ot, enum eDynTopoWarnFlag flag)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Warning!"), ICON_ERROR);
  uiLayout *layout = UI_popup_menu_layout(pup);

  if (flag & (DYNTOPO_WARN_VDATA | DYNTOPO_WARN_EDATA | DYNTOPO_WARN_LDATA)) {
    const char *msg_error = TIP_("Vertex Data Detected!");
    const char *msg = TIP_("Dyntopo will not preserve vertex colors, UVs, or other customdata");
    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  if (flag & DYNTOPO_WARN_MODIFIER) {
    const char *msg_error = TIP_("Generative Modifiers Detected!");
    const char *msg = TIP_(
        "Keeping the modifiers will increase polycount when returning to object mode");

    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  uiItemFullO_ptr(layout, ot, IFACE_("OK"), ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, 0, NULL);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

enum eDynTopoWarnFlag SCULPT_dynamic_topology_check(Scene *scene, Object *ob)
{
  Mesh *me = ob->data;
  SculptSession *ss = ob->sculpt;

  enum eDynTopoWarnFlag flag = 0;

  BLI_assert(ss->bm == NULL);
  UNUSED_VARS_NDEBUG(ss);

  for (int i = 0; i < CD_NUMTYPES; i++) {
    if (!ELEM(i, CD_MEDGE, CD_MFACE, CD_MLOOP, CD_MPOLY, CD_PAINT_MASK, CD_ORIGINDEX)) {
      if (CustomData_has_layer(&me->vdata, i)) {
        flag |= DYNTOPO_WARN_VDATA;
      }
      if (CustomData_has_layer(&me->edata, i)) {
        flag |= DYNTOPO_WARN_EDATA;
      }
      if (CustomData_has_layer(&me->ldata, i)) {
        flag |= DYNTOPO_WARN_LDATA;
      }
    }
  }

  {
    VirtualModifierData virtualModifierData;
    ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);

    /* Exception for shape keys because we can edit those. */
    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);
      if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        continue;
      }

      if (mti->type == eModifierTypeType_Constructive) {
        flag |= DYNTOPO_WARN_MODIFIER;
        break;
      }
    }
  }

  return flag;
}

static int sculpt_dynamic_topology_toggle_invoke(bContext *C,
                                                 wmOperator *op,
                                                 const wmEvent *UNUSED(event))
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  if (!ss->bm) {
    Scene *scene = CTX_data_scene(C);
    enum eDynTopoWarnFlag flag = SCULPT_dynamic_topology_check(scene, ob);

    if (flag) {
      /* The mesh has customdata that will be lost, let the user confirm this is OK. */
      return dyntopo_warning_popup(C, op->type, flag);
    }
  }

  return sculpt_dynamic_topology_toggle_exec(C, op);
}

void SCULPT_OT_dynamic_topology_toggle(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Dynamic Topology Toggle";
  ot->idname = "SCULPT_OT_dynamic_topology_toggle";
  ot->description = "Dynamic topology alters the mesh topology while sculpting";

  /* API callbacks. */
  ot->invoke = sculpt_dynamic_topology_toggle_invoke;
  ot->exec = sculpt_dynamic_topology_toggle_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
