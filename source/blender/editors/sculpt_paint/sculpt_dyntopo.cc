/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup edsculpt
 */

#include <cmath>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh.hh"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"

#include "BLI_index_range.hh"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_undo.h"
#include "sculpt_intern.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"
#include "bmesh_tools.h"

using blender::IndexRange;

void SCULPT_dynamic_topology_triangulate(BMesh *bm)
{
  if (bm->totloop != bm->totface * 3) {
    BM_mesh_triangulate(bm,
                        MOD_TRIANGULATE_QUAD_BEAUTY,
                        MOD_TRIANGULATE_NGON_EARCLIP,
                        4,
                        false,
                        nullptr,
                        nullptr,
                        nullptr);
  }
}

void SCULPT_pbvh_clear(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  /* Clear out any existing DM and PBVH. */
  if (ss->pbvh) {
    BKE_pbvh_free(ss->pbvh);
    ss->pbvh = nullptr;
  }

  BKE_object_free_derived_caches(ob);

  /* Tag to rebuild PBVH in depsgraph. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
}

void SCULPT_dynamic_topology_enable_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = static_cast<Mesh *>(ob->data);
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(me);

  SCULPT_pbvh_clear(ob);

  ss->bm_smooth_shading = (scene->toolsettings->sculpt->flags & SCULPT_DYNTOPO_SMOOTH_SHADING) !=
                          0;

  /* Dynamic topology doesn't ensure selection state is valid, so remove #36280. */
  BKE_mesh_mselect_clear(me);

  /* Create triangles-only BMesh. */
  BMeshCreateParams create_params{};
  create_params.use_toolflags = false;
  ss->bm = BM_mesh_create(&allocsize, &create_params);

  BMeshFromMeshParams convert_params{};
  convert_params.calc_face_normal = true;
  convert_params.calc_vert_normal = true;
  convert_params.use_shapekey = true;
  convert_params.active_shapekey = ob->shapenr;
  BM_mesh_bm_from_me(ss->bm, me, &convert_params);
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
  Mesh *me = static_cast<Mesh *>(ob->data);

  if (ss->attrs.dyntopo_node_id_vertex) {
    BKE_sculpt_attribute_destroy(ob, ss->attrs.dyntopo_node_id_vertex);
  }

  if (ss->attrs.dyntopo_node_id_face) {
    BKE_sculpt_attribute_destroy(ob, ss->attrs.dyntopo_node_id_face);
  }

  SCULPT_pbvh_clear(ob);

  if (unode) {
    /* Free all existing custom data. */
    BKE_mesh_clear_geometry(me);

    /* Copy over stored custom data. */
    SculptUndoNodeGeometry *geometry = &unode->geometry_bmesh_enter;
    me->totvert = geometry->totvert;
    me->totloop = geometry->totloop;
    me->totpoly = geometry->totpoly;
    me->totedge = geometry->totedge;
    me->totface = 0;
    CustomData_copy(&geometry->vdata, &me->vdata, CD_MASK_MESH.vmask, geometry->totvert);
    CustomData_copy(&geometry->edata, &me->edata, CD_MASK_MESH.emask, geometry->totedge);
    CustomData_copy(&geometry->ldata, &me->ldata, CD_MASK_MESH.lmask, geometry->totloop);
    CustomData_copy(&geometry->pdata, &me->pdata, CD_MASK_MESH.pmask, geometry->totpoly);
    blender::implicit_sharing::copy_shared_pointer(geometry->poly_offset_indices,
                                                   geometry->poly_offsets_sharing_info,
                                                   &me->poly_offset_indices,
                                                   &me->runtime->poly_offsets_sharing_info);
  }
  else {
    BKE_sculptsession_bm_to_me(ob, true);

    /* Reset Face Sets as they are no longer valid. */
    CustomData_free_layer_named(&me->pdata, ".sculpt_face_set", me->totpoly);
    me->face_sets_color_default = 1;

    /* Sync the visibility to vertices manually as the pmap is still not initialized. */
    bool *hide_vert = (bool *)CustomData_get_layer_named_for_write(
        &me->vdata, CD_PROP_BOOL, ".hide_vert", me->totvert);
    if (hide_vert != nullptr) {
      memset(hide_vert, 0, sizeof(bool) * me->totvert);
    }
  }

  /* Clear data. */
  me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Typically valid but with global-undo they can be nullptr, see: #36234. */
  if (ss->bm) {
    BM_mesh_free(ss->bm);
    ss->bm = nullptr;
  }
  if (ss->bm_log) {
    BM_log_free(ss->bm_log);
    ss->bm_log = nullptr;
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
  }
}

static void sculpt_dynamic_topology_enable_with_undo(Main *bmain,
                                                     Depsgraph *depsgraph,
                                                     Scene *scene,
                                                     Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->bm == nullptr) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != nullptr) : true;
    if (use_undo) {
      SCULPT_undo_push_begin_ex(ob, "Dynamic topology enable");
    }
    SCULPT_dynamic_topology_enable_ex(bmain, depsgraph, scene, ob);
    if (use_undo) {
      SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_DYNTOPO_BEGIN);
      SCULPT_undo_push_end(ob);
    }
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
    sculpt_dynamic_topology_enable_with_undo(bmain, depsgraph, scene, ob);
  }

  WM_cursor_wait(false);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);

  return OPERATOR_FINISHED;
}

static int dyntopo_warning_popup(bContext *C, wmOperatorType *ot, enum eDynTopoWarnFlag flag)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Warning!"), ICON_ERROR);
  uiLayout *layout = UI_popup_menu_layout(pup);

  if (flag & (DYNTOPO_WARN_VDATA | DYNTOPO_WARN_EDATA | DYNTOPO_WARN_LDATA)) {
    const char *msg_error = TIP_("Attribute Data Detected");
    const char *msg = TIP_("Dyntopo will not preserve colors, UVs, or other attributes");
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

  uiItemFullO_ptr(layout, ot, IFACE_("OK"), ICON_NONE, nullptr, WM_OP_EXEC_DEFAULT, 0, nullptr);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static bool dyntopo_supports_layer(const CustomDataLayer &layer, const int elem_num)
{
  if (CD_TYPE_AS_MASK(layer.type) & CD_MASK_PROP_ALL) {
    if (STREQ(layer.name, ".sculpt_face_set")) {
      /* Check if only one face set exists. */
      const blender::Span<int> face_sets(static_cast<const int *>(layer.data), elem_num);
      for (const int i : face_sets.index_range()) {
        if (face_sets[i] != face_sets.first()) {
          return false;
        }
      }
      return true;
    }
    /* Some data is stored as generic attributes on #Mesh but in flags or fields on #BMesh. */
    return BM_attribute_stored_in_bmesh_builtin(layer.name);
  }
  /* Some layers just encode #Mesh topology or are handled as special cases for dyntopo. */
  return ELEM(layer.type, CD_PAINT_MASK, CD_ORIGINDEX);
}

static bool dyntopo_supports_customdata_layers(const blender::Span<CustomDataLayer> layers,
                                               const int elem_num)
{
  return std::all_of(layers.begin(), layers.end(), [&](const CustomDataLayer &layer) {
    return dyntopo_supports_layer(layer, elem_num);
  });
}

enum eDynTopoWarnFlag SCULPT_dynamic_topology_check(Scene *scene, Object *ob)
{
  Mesh *me = static_cast<Mesh *>(ob->data);
  SculptSession *ss = ob->sculpt;

  enum eDynTopoWarnFlag flag = eDynTopoWarnFlag(0);

  BLI_assert(ss->bm == nullptr);
  UNUSED_VARS_NDEBUG(ss);

  if (!dyntopo_supports_customdata_layers({me->vdata.layers, me->vdata.totlayer}, me->totvert)) {
    flag |= DYNTOPO_WARN_VDATA;
  }
  if (!dyntopo_supports_customdata_layers({me->edata.layers, me->edata.totlayer}, me->totedge)) {
    flag |= DYNTOPO_WARN_EDATA;
  }
  if (!dyntopo_supports_customdata_layers({me->pdata.layers, me->pdata.totlayer}, me->totpoly)) {
    flag |= DYNTOPO_WARN_LDATA;
  }
  if (!dyntopo_supports_customdata_layers({me->ldata.layers, me->ldata.totlayer}, me->totloop)) {
    flag |= DYNTOPO_WARN_LDATA;
  }

  {
    VirtualModifierData virtualModifierData;
    ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);

    /* Exception for shape keys because we can edit those. */
    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
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
                                                 const wmEvent * /*event*/)
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
