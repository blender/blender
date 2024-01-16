/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <cmath>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "DNA_modifier_types.h"

#include "BKE_context.hh"
#include "BKE_global.h"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_particle.h"
#include "BKE_pbvh_api.hh"
#include "BKE_pointcache.h"
#include "BKE_scene.h"

#include "BLI_index_range.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_undo.hh"
#include "sculpt_intern.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "bmesh.hh"
#include "bmesh_tools.hh"

void SCULPT_pbvh_clear(Object *ob)
{
  using namespace blender;
  SculptSession *ss = ob->sculpt;

  /* Clear out any existing DM and PBVH. */
  if (ss->pbvh) {
    bke::pbvh::free(ss->pbvh);
    ss->pbvh = nullptr;
  }

  BKE_object_free_derived_caches(ob);

  /* Tag to rebuild PBVH in depsgraph. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
}

namespace blender::ed::sculpt_paint::dyntopo {

void triangulate(BMesh *bm)
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

void enable_ex(Main *bmain, Depsgraph *depsgraph, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);

  SCULPT_pbvh_clear(ob);

  /* Dynamic topology doesn't ensure selection state is valid, so remove #36280. */
  BKE_mesh_mselect_clear(mesh);

  /* Create triangles-only BMesh. */
  BMeshCreateParams create_params{};
  create_params.use_toolflags = false;
  ss->bm = BM_mesh_create(&allocsize, &create_params);

  BMeshFromMeshParams convert_params{};
  convert_params.calc_face_normal = true;
  convert_params.calc_vert_normal = true;
  convert_params.use_shapekey = true;
  convert_params.active_shapekey = ob->shapenr;
  BM_mesh_bm_from_me(ss->bm, mesh, &convert_params);
  triangulate(ss->bm);

  BM_data_layer_add_named(ss->bm, &ss->bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

  /* Make sure the data for existing faces are initialized. */
  if (mesh->faces_num != ss->bm->totface) {
    BM_mesh_normals_update(ss->bm);
  }

  /* Enable dynamic topology. */
  mesh->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

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
    Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob, undo::Node *unode)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  if (ss->attrs.dyntopo_node_id_vertex) {
    BKE_sculpt_attribute_destroy(ob, ss->attrs.dyntopo_node_id_vertex);
  }

  if (ss->attrs.dyntopo_node_id_face) {
    BKE_sculpt_attribute_destroy(ob, ss->attrs.dyntopo_node_id_face);
  }

  SCULPT_pbvh_clear(ob);

  if (unode) {
    /* Free all existing custom data. */
    BKE_mesh_clear_geometry(mesh);

    /* Copy over stored custom data. */
    undo::NodeGeometry *geometry = &unode->geometry_bmesh_enter;
    mesh->verts_num = geometry->totvert;
    mesh->corners_num = geometry->totloop;
    mesh->faces_num = geometry->faces_num;
    mesh->edges_num = geometry->totedge;
    mesh->totface_legacy = 0;
    CustomData_copy(&geometry->vert_data, &mesh->vert_data, CD_MASK_MESH.vmask, geometry->totvert);
    CustomData_copy(&geometry->edge_data, &mesh->edge_data, CD_MASK_MESH.emask, geometry->totedge);
    CustomData_copy(
        &geometry->corner_data, &mesh->corner_data, CD_MASK_MESH.lmask, geometry->totloop);
    CustomData_copy(
        &geometry->face_data, &mesh->face_data, CD_MASK_MESH.pmask, geometry->faces_num);
    implicit_sharing::copy_shared_pointer(geometry->face_offset_indices,
                                          geometry->face_offsets_sharing_info,
                                          &mesh->face_offset_indices,
                                          &mesh->runtime->face_offsets_sharing_info);
  }
  else {
    BKE_sculptsession_bm_to_me(ob, true);

    /* Sync the visibility to vertices manually as `vert_to_face_map` is still not initialized. */
    bool *hide_vert = (bool *)CustomData_get_layer_named_for_write(
        &mesh->vert_data, CD_PROP_BOOL, ".hide_vert", mesh->verts_num);
    if (hide_vert != nullptr) {
      memset(hide_vert, 0, sizeof(bool) * mesh->verts_num);
    }
  }

  /* Clear data. */
  mesh->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

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

void disable(bContext *C, undo::Node *unode)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  SCULPT_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, unode);
}

void disable_with_undo(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->bm != nullptr) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != nullptr) : true;
    if (use_undo) {
      undo::push_begin_ex(ob, "Dynamic topology disable");
      undo::push_node(ob, nullptr, undo::Type::DyntopoEnd);
    }
    SCULPT_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, nullptr);
    if (use_undo) {
      undo::push_end(ob);
    }
  }
}

static void sculpt_dynamic_topology_enable_with_undo(Main *bmain, Depsgraph *depsgraph, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->bm == nullptr) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != nullptr) : true;
    if (use_undo) {
      undo::push_begin_ex(ob, "Dynamic topology enable");
    }
    enable_ex(bmain, depsgraph, ob);
    if (use_undo) {
      undo::push_node(ob, nullptr, undo::Type::DyntopoBegin);
      undo::push_end(ob);
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
    disable_with_undo(bmain, depsgraph, scene, ob);
  }
  else {
    sculpt_dynamic_topology_enable_with_undo(bmain, depsgraph, ob);
  }

  WM_cursor_wait(false);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);

  return OPERATOR_FINISHED;
}

static int dyntopo_warning_popup(bContext *C, wmOperatorType *ot, enum WarnFlag flag)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Warning!"), ICON_ERROR);
  uiLayout *layout = UI_popup_menu_layout(pup);

  if (flag & (VDATA | EDATA | LDATA)) {
    const char *msg_error = RPT_("Attribute Data Detected");
    const char *msg = RPT_("Dyntopo will not preserve colors, UVs, or other attributes");
    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  if (flag & MODIFIER) {
    const char *msg_error = RPT_("Generative Modifiers Detected!");
    const char *msg = RPT_(
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

static bool dyntopo_supports_layer(const CustomDataLayer &layer)
{
  if (CD_TYPE_AS_MASK(layer.type) & CD_MASK_PROP_ALL) {
    /* Some data is stored as generic attributes on #Mesh but in flags or fields on #BMesh. */
    return BM_attribute_stored_in_bmesh_builtin(layer.name);
  }
  /* Some layers just encode #Mesh topology or are handled as special cases for dyntopo. */
  return ELEM(layer.type, CD_ORIGINDEX);
}

static bool dyntopo_supports_customdata_layers(const Span<CustomDataLayer> layers)
{
  return std::all_of(layers.begin(), layers.end(), [&](const CustomDataLayer &layer) {
    return dyntopo_supports_layer(layer);
  });
}

enum WarnFlag check_attribute_warning(Scene *scene, Object *ob)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  SculptSession *ss = ob->sculpt;

  WarnFlag flag = WarnFlag(0);

  BLI_assert(ss->bm == nullptr);
  UNUSED_VARS_NDEBUG(ss);

  if (!dyntopo_supports_customdata_layers({mesh->vert_data.layers, mesh->vert_data.totlayer})) {
    flag |= VDATA;
  }
  if (!dyntopo_supports_customdata_layers({mesh->edge_data.layers, mesh->edge_data.totlayer})) {
    flag |= EDATA;
  }
  if (!dyntopo_supports_customdata_layers({mesh->face_data.layers, mesh->face_data.totlayer})) {
    flag |= LDATA;
  }
  if (!dyntopo_supports_customdata_layers({mesh->corner_data.layers, mesh->corner_data.totlayer}))
  {
    flag |= LDATA;
  }

  {
    VirtualModifierData virtual_modifier_data;
    ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtual_modifier_data);

    /* Exception for shape keys because we can edit those. */
    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
      if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        continue;
      }

      if (mti->type == ModifierTypeType::Constructive) {
        flag |= MODIFIER;
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
    const WarnFlag flag = check_attribute_warning(scene, ob);

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

}  // namespace blender::ed::sculpt_paint::dyntopo
