/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#include "sculpt_dyntopo.hh"

#include <cstdlib>

#include "BLT_translation.hh"

#include "DNA_modifier_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_undo.hh"

#include "sculpt_intern.hh"
#include "sculpt_undo.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "bmesh.hh"
#include "bmesh_tools.hh"

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

void enable_ex(Main &bmain, Depsgraph &depsgraph, Object &ob)
{
  SculptSession &ss = *ob.sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob.data);
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);

  BKE_sculptsession_free_pbvh(ob);

  /* Dynamic topology doesn't ensure selection state is valid, so remove #36280. */
  BKE_mesh_mselect_clear(mesh);

  /* Create triangles-only BMesh. */
  BMeshCreateParams create_params{};
  create_params.use_toolflags = false;
  ss.bm = BM_mesh_create(&allocsize, &create_params);

  BMeshFromMeshParams convert_params{};
  convert_params.calc_face_normal = true;
  convert_params.calc_vert_normal = true;
  convert_params.use_shapekey = true;
  convert_params.active_shapekey = ob.shapenr;
  BM_mesh_bm_from_me(ss.bm, mesh, &convert_params);
  triangulate(ss.bm);

  BM_data_layer_ensure_named(ss.bm, &ss.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

  /* Make sure the data for existing faces are initialized. */
  if (mesh->faces_num != ss.bm->totface) {
    BM_mesh_normals_update(ss.bm);
  }

  /* Enable dynamic topology. */
  mesh->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Enable logging for undo/redo. */
  ss.bm_log = BM_log_create(ss.bm);

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the #bke::pbvh::Tree is re-created. */
  DEG_id_tag_update(&ob.id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_update_tagged(&depsgraph, &bmain);
}

/**
 * Free the sculpt BMesh and BMLog
 *
 * If `unode` is given, the #BMesh's data is copied out to the `unode`
 * before the BMesh is deleted so that it can be restored from.
 */
static void disable(
    Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob, undo::StepData *undo_step)
{
  SculptSession &ss = *ob.sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob.data);

  if (BMesh *bm = ss.bm) {
    BM_data_layer_free_named(bm, &bm->vdata, ".sculpt_dyntopo_node_id_vertex");
    BM_data_layer_free_named(bm, &bm->pdata, ".sculpt_dyntopo_node_id_face");
  }

  BKE_sculptsession_free_pbvh(ob);

  if (undo_step) {
    undo::restore_from_bmesh_enter_geometry(*undo_step, *mesh);
  }
  else {
    BKE_sculptsession_bm_to_me(&ob);
  }

  /* Clear data. */
  mesh->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Typically valid but with global-undo they can be nullptr, see: #36234. */
  if (ss.bm) {
    BM_mesh_free(ss.bm);
    ss.bm = nullptr;
  }
  if (ss.bm_log) {
    BM_log_free(ss.bm_log);
    ss.bm_log = nullptr;
  }

  BKE_particlesystem_reset_all(&ob);
  BKE_ptcache_object_reset(&scene, &ob, PTCACHE_RESET_OUTDATED);

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the #bke::pbvh::Tree is re-created. */
  DEG_id_tag_update(&ob.id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_update_tagged(&depsgraph, &bmain);
}

void disable(bContext *C, undo::StepData *undo_step)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  disable(*bmain, *depsgraph, *scene, *ob, undo_step);
}

void disable_with_undo(Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob)
{
  /* This is an unlikely situation to happen in normal usage, though with application handlers
   * it is possible that a user is attempting to exit the current object mode. See #146398 */
  if (ob.sculpt && ob.sculpt->bm) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != nullptr) : true;
    if (use_undo) {
      undo::push_begin_ex(scene, ob, "Dynamic topology disable");
      undo::push_node(depsgraph, ob, nullptr, undo::Type::DyntopoEnd);
    }
    disable(bmain, depsgraph, scene, ob, nullptr);
    if (use_undo) {
      undo::push_end(ob);
    }
  }
}

static void enable_with_undo(Main &bmain, Depsgraph &depsgraph, const Scene &scene, Object &ob)
{
  SculptSession &ss = *ob.sculpt;
  if (ss.bm == nullptr) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != nullptr) : true;
    if (use_undo) {
      undo::push_begin_ex(scene, ob, "Dynamic topology enable");
    }
    enable_ex(bmain, depsgraph, ob);
    if (use_undo) {
      undo::push_node(depsgraph, ob, nullptr, undo::Type::DyntopoBegin);
      undo::push_end(ob);
    }
  }
}

static wmOperatorStatus sculpt_dynamic_topology_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  Main &bmain = *CTX_data_main(C);
  Depsgraph &depsgraph = *CTX_data_ensure_evaluated_depsgraph(C);
  Scene &scene = *CTX_data_scene(C);
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;

  WM_cursor_wait(true);

  if (ss.bm) {
    disable_with_undo(bmain, depsgraph, scene, ob);
  }
  else {
    enable_with_undo(bmain, depsgraph, scene, ob);
  }

  WM_cursor_wait(false);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);

  return OPERATOR_FINISHED;
}

static bool dyntopo_supports_layer(const CustomDataLayer &layer)
{
  if (layer.type == CD_PROP_FLOAT && STREQ(layer.name, ".sculpt_mask")) {
    return true;
  }
  if (CD_TYPE_AS_MASK(eCustomDataType(layer.type)) & CD_MASK_PROP_ALL) {
    return BM_attribute_stored_in_bmesh_builtin(layer.name);
  }
  return ELEM(layer.type, CD_ORIGINDEX);
}

static bool dyntopo_supports_customdata_layers(const Span<CustomDataLayer> layers)
{
  return std::all_of(layers.begin(), layers.end(), [&](const CustomDataLayer &layer) {
    return dyntopo_supports_layer(layer);
  });
}

WarnFlag check_attribute_warning(Scene &scene, Object &ob)
{
  Mesh *mesh = static_cast<Mesh *>(ob.data);
  SculptSession &ss = *ob.sculpt;

  WarnFlag flag = WarnFlag(0);

  BLI_assert(ss.bm == nullptr);
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
    ModifierData *md = BKE_modifiers_get_virtual_modifierlist(&ob, &virtual_modifier_data);

    /* Exception for shape keys because we can edit those. */
    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
      if (!BKE_modifier_is_enabled(&scene, md, eModifierMode_Realtime)) {
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

static wmOperatorStatus sculpt_dynamic_topology_toggle_invoke(bContext *C,
                                                              wmOperator *op,
                                                              const wmEvent * /*event*/)
{
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;

  if (!ss.bm) {
    Scene &scene = *CTX_data_scene(C);
    const WarnFlag flag = check_attribute_warning(scene, ob);

    if (flag & (VDATA | EDATA | LDATA)) {
      return WM_operator_confirm_ex(
          C,
          op,
          RPT_("Attribute Data Detected"),
          RPT_("Dyntopo will not preserve colors, UVs, or other attributes"),
          IFACE_("Enable"),
          ALERT_ICON_WARNING,
          false);
    }

    if (flag & MODIFIER) {
      return WM_operator_confirm_ex(
          C,
          op,
          RPT_("Generative Modifiers Detected!"),
          RPT_("Keeping the modifiers will increase polycount when returning to object mode"),
          IFACE_("Enable"),
          ALERT_ICON_WARNING,
          false);
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
