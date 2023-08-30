/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <cmath>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_array.hh"
#include "BLI_blenlib.h"
#include "BLI_compiler_attrs.h"
#include "BLI_hash.h"
#include "BLI_index_range.hh"
#include "BLI_linklist.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_brush.hh"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.hh"
#include "BKE_particle.h"
#include "BKE_pbvh_api.hh"
#include "BKE_pointcache.h"
#include "BKE_scene.h"

#include "BLI_index_range.hh"

#include "DEG_depsgraph.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_undo.hh"
#include "sculpt_intern.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "bmesh.h"
#include "bmesh_idmap.h"
#include "bmesh_log.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>

using blender::IndexRange;
using blender::int2;
using blender::Vector;

BMesh *SCULPT_dyntopo_empty_bmesh()
{
  return BKE_sculptsession_empty_bmesh_create();
}

void SCULPT_pbvh_clear(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  ss->pmap = {};
  ss->vert_to_face_indices = {};
  ss->vert_to_face_offsets = {};
  ss->epmap = {};
  ss->edge_to_face_indices = {};
  ss->edge_to_face_offsets = {};
  ss->vemap = {};
  ss->vert_to_edge_indices = {};
  ss->vert_to_edge_offsets = {};

  /* Clear out any existing DM and PBVH. */
  if (ss->pbvh) {
    BKE_pbvh_free(ss->pbvh);
    ss->pbvh = nullptr;
  }

  BKE_object_free_derived_caches(ob);

  /* Tag to rebuild PBVH in depsgraph. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
}

static void customdata_strip_templayers(CustomData *cdata, int totelem)
{
  for (int i = 0; i < cdata->totlayer; i++) {
    CustomDataLayer *layer = cdata->layers + i;

    if (layer->flag & CD_FLAG_TEMPORARY) {
      CustomData_free_layer(cdata, eCustomDataType(layer->type), totelem, i);
      i--;
    }
  }
}

void SCULPT_dynamic_topology_enable_ex(Main *bmain, Depsgraph *depsgraph, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = BKE_object_get_original_mesh(ob);

  customdata_strip_templayers(&me->vert_data, me->totvert);
  customdata_strip_templayers(&me->face_data, me->faces_num);

  if (!ss->pmap.is_empty()) {
    ss->pmap = {};
    ss->vert_to_face_indices = {};
    ss->vert_to_face_offsets = {};
    ss->epmap = {};
    ss->edge_to_face_indices = {};
    ss->edge_to_face_offsets = {};
    ss->vemap = {};
    ss->vert_to_edge_indices = {};
    ss->vert_to_edge_offsets = {};
  }

  if (!ss->bm || !ss->pbvh || BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
    SCULPT_pbvh_clear(ob);
  }

  PBVH *pbvh = ss->pbvh;

  if (pbvh) {
    BMesh *bm = BKE_pbvh_get_bmesh(pbvh);

    if (!ss->bm) {
      ss->bm = bm;
    }
    else if (ss->bm != bm) {
      printf("%s: bmesh differed!\n", __func__);
      SCULPT_pbvh_clear(ob);
    }
  }

  /* Dynamic topology doesn't ensure selection state is valid, so remove #36280. */
  BKE_mesh_mselect_clear(me);
  bool tag_update = false;

  if (!ss->bm) {
    ss->bm = BKE_sculptsession_empty_bmesh_create();

    BMeshFromMeshParams params = {};
    params.use_shapekey = true;
    params.active_shapekey = ob->shapenr;

    BM_mesh_bm_from_me(ss->bm, me, &params);
    tag_update = true;
  }

#ifndef DYNTOPO_DYNAMIC_TESS
  SCULPT_dynamic_topology_triangulate(ss, ss->bm);
#endif

  if (ss->pbvh) {
    BKE_sculptsession_update_attr_refs(ob);
  }

  /* XXX Delete this block of code? Might be old fake quadrangulation edge hiding. */
  BMIter iter;
  BMEdge *e;
  BM_ITER_MESH (e, &iter, ss->bm, BM_EDGES_OF_MESH) {
    e->head.hflag |= BM_ELEM_DRAW;
  }

  /* Calculate normals. */
  BM_mesh_normals_update(ss->bm);

  /* Enable dynamic topology. */
  me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  BKE_sculpt_ensure_idmap(ob);

  /* Enable logging for undo/redo. */
  if (!ss->bm_log) {
    ss->bm_log = BM_log_create(ss->bm, ss->bm_idmap);
  }

  tag_update |= !ss->pbvh || BKE_pbvh_type(ss->pbvh) != PBVH_BMESH;

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created. */
  if (tag_update) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    BKE_scene_graph_update_tagged(depsgraph, bmain);
  }

  /* ss->pbvh should exist by this point. */

  if (ss->pbvh) {
    BKE_sculpt_ensure_sculpt_layers(ob);
    BKE_sculpt_ensure_origco(ob);
    blender::bke::paint::load_all_original(ob);
  }

  SCULPT_update_all_valence_boundary(ob);

  if (ss->pbvh && SCULPT_has_persistent_base(ss)) {
    SCULPT_ensure_persistent_layers(ss, ob);
  }

  if (!CustomData_has_layer(&ss->bm->vdata, CD_PAINT_MASK)) {
    BM_data_layer_add(ss->bm, &ss->bm->vdata, CD_PAINT_MASK);
    BKE_sculptsession_update_attr_refs(ob);
  }
}

void SCULPT_update_all_valence_boundary(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  /* Do bmesh seperately to avoid needing the PBVH, which we might not
   * have inside the undo code.
   */

  if (ss->bm) {
    SCULPT_vertex_random_access_ensure(ss);
    BMIter iter;
    BMVert *v;

    int cd_flag = CustomData_get_offset_named(
        &ss->bm->vdata, CD_PROP_INT8, SCULPT_ATTRIBUTE_NAME(flags));
    int cd_boundary = CustomData_get_offset_named(
        &ss->bm->vdata, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(boundary_flags));
    int cd_valence = CustomData_get_offset_named(
        &ss->bm->vdata, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(valence));

    BLI_assert(cd_flag != -1 && cd_boundary != -1 && cd_valence != -1);

    BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
      *BM_ELEM_CD_PTR<uint8_t *>(v, cd_flag) = SCULPTFLAG_NEED_TRIANGULATE;
      BM_ELEM_CD_SET_INT(v, cd_valence, BM_vert_edge_count(v));
      *BM_ELEM_CD_PTR<int *>(v, cd_boundary) |= SCULPT_BOUNDARY_NEEDS_UPDATE |
                                                SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;

      /* Update boundary if we have a pbvh. */
      if (ss->pbvh) {
        PBVHVertRef vertex = {reinterpret_cast<intptr_t>(v)};
        SCULPT_vertex_is_boundary(ss, vertex, SCULPT_BOUNDARY_ALL);
      }
    }

    return;
  }
  if (!ss->pbvh) {
    return;
  }

  int verts_count = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < verts_count; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    blender::bke::paint::vertex_attr_set<int>(
        vertex, ss->attrs.flags, SCULPTFLAG_NEED_VALENCE | SCULPTFLAG_NEED_TRIANGULATE);
    BKE_sculpt_boundary_flag_update(ss, vertex);
    SCULPT_vertex_valence_get(ss, vertex);
    SCULPT_vertex_is_boundary(ss, vertex, SCULPT_BOUNDARY_ALL);
  }
}

/* Free the sculpt BMesh and BMLog
 *
 * If 'unode' is given, the BMesh's data is copied out to the unode
 * before the BMesh is deleted so that it can be restored from. */
static void SCULPT_dynamic_topology_disable_ex(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob, SculptUndoNode * /*unode*/)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = static_cast<Mesh *>(ob->data);

  /* Destroy temporary layers. */
  BKE_sculpt_attribute_destroy_temporary_all(ob);

  if (ss->attrs.dyntopo_node_id_vertex) {
    BKE_sculpt_attribute_destroy(ob, ss->attrs.dyntopo_node_id_vertex);
  }

  if (ss->attrs.dyntopo_node_id_face) {
    BKE_sculpt_attribute_destroy(ob, ss->attrs.dyntopo_node_id_face);
  }

  BKE_sculptsession_update_attr_refs(ob);
  BKE_sculptsession_bm_to_me(ob, true);
  SCULPT_pbvh_clear(ob);

  /* Sync the visibility to vertices manually as the pmap is still not initialized. */
  bool *hide_vert = (bool *)CustomData_get_layer_named_for_write(
      &me->vert_data, CD_PROP_BOOL, ".hide_vert", me->totvert);
  if (hide_vert != nullptr) {
    memset(static_cast<void *>(hide_vert), 0, sizeof(bool) * me->totvert);
  }

  /* Clear data. */
  me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

  if (ss->bm_idmap) {
    BM_idmap_destroy(ss->bm_idmap);
    ss->bm_idmap = nullptr;
  }

  if (ss->bm_log) {
    BM_log_free(ss->bm_log);
    ss->bm_log = nullptr;
  }

  /* Typically valid but with global-undo they can be nullptr, see: T36234. */
  if (ss->bm) {
    BM_mesh_free(ss->bm);
    ss->bm = nullptr;
  }

  SCULPT_pbvh_clear(ob);

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
  if (ss->bm != nullptr) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != nullptr) : true;
    if (use_undo) {
      SCULPT_undo_push_begin_ex(ob, "Dynamic topology disable");
      SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_DYNTOPO_END);
    }
    SCULPT_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, nullptr);
    if (use_undo) {
      SCULPT_undo_push_end(ob);
    }

    ss->active_vertex.i = ss->active_face.i = PBVH_REF_NONE;
  }
}

static void sculpt_dynamic_topology_enable_with_undo(Main *bmain, Depsgraph *depsgraph, Object *ob)
{
  SculptSession *ss = ob->sculpt;

  if (ss->bm == nullptr) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != nullptr) : true;
    if (use_undo) {
      SCULPT_undo_push_begin_ex(ob, "Dynamic topology enable");
    }
    SCULPT_dynamic_topology_enable_ex(bmain, depsgraph, ob);
    if (use_undo) {
      SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_DYNTOPO_BEGIN);
      SCULPT_undo_push_end(ob);
    }

    ss->active_vertex.i = ss->active_face.i = 0;
  }
}

static int sculpt_dynamic_topology_toggle_exec(bContext *C, wmOperator * /*op*/)
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
    sculpt_dynamic_topology_enable_with_undo(bmain, depsgraph, ob);
  }

  WM_cursor_wait(false);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);

  return OPERATOR_FINISHED;
}

static int dyntopo_error_popup(bContext *C, wmOperatorType * /*ot*/, enum eDynTopoWarnFlag flag)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Error!"), ICON_ERROR);
  uiLayout *layout = UI_popup_menu_layout(pup);

  if (flag & DYNTOPO_ERROR_MULTIRES) {
    const char *msg_error = TIP_("Multires modifier detected; cannot enable dyntopo.");
    const char *msg = TIP_("Dyntopo and multires cannot be mixed.");

    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static int dyntopo_warning_popup(bContext *C, wmOperatorType *ot, enum eDynTopoWarnFlag flag)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Warning!"), ICON_ERROR);
  uiLayout *layout = UI_popup_menu_layout(pup);

  if (flag & (DYNTOPO_WARN_EDATA)) {
    const char *msg_error = TIP_("Edge Data Detected!");
    const char *msg = TIP_("Dyntopo will not preserve custom edge attributes");
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

  uiItemFullO_ptr(
      layout, ot, IFACE_("OK"), ICON_NONE, nullptr, WM_OP_EXEC_DEFAULT, UI_ITEM_NONE, nullptr);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

enum eDynTopoWarnFlag SCULPT_dynamic_topology_check(Scene *scene, Object *ob)
{
  SculptSession *ss = ob->sculpt;

  enum eDynTopoWarnFlag flag = eDynTopoWarnFlag(0);

  BLI_assert(ss->bm == nullptr);
  UNUSED_VARS_NDEBUG(ss);

  {
    VirtualModifierData virtual_modifier_data;
    ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtual_modifier_data);

    /* Exception for shape keys because we can edit those. */
    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
      if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        continue;
      }

      if (md->type == eModifierType_Multires) {
        flag |= DYNTOPO_ERROR_MULTIRES;
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
                                                 const wmEvent * /*event*/)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  if (!ss->bm) {
    Scene *scene = CTX_data_scene(C);
    enum eDynTopoWarnFlag flag = SCULPT_dynamic_topology_check(scene, ob);

    if (flag & DYNTOPO_ERROR_MULTIRES) {
      return dyntopo_error_popup(C, op->type, flag);
    }
    else if (flag) {
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
  ot->description =
      "Dynamic mode; note that you must now check the DynTopo"
      "option to enable dynamic remesher (which updates topology will sculpting)"
      "this is on by default.";

  /* API callbacks. */
  ot->invoke = sculpt_dynamic_topology_toggle_invoke;
  ot->exec = sculpt_dynamic_topology_toggle_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
