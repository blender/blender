/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edphys
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_bvhutils.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_object.hh"
#include "ED_particle.hh"
#include "ED_screen.hh"

#include "particle_edit_utildefines.h"

#include "physics_intern.hh"

static float I[4][4] = {
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
};

/********************** particle system slot operators *********************/

static wmOperatorStatus particle_system_add_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = blender::ed::object::context_object(C);
  Scene *scene = CTX_data_scene(C);

  if (!scene || !ob) {
    return OPERATOR_CANCELLED;
  }

  object_add_particle_system(bmain, scene, ob, nullptr);

  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_POINTCACHE, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_particle_system_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Particle System Slot";
  ot->idname = "OBJECT_OT_particle_system_add";
  ot->description = "Add a particle system";

  /* API callbacks. */
  ot->poll = ED_operator_object_active_local_editable;
  ot->exec = particle_system_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus particle_system_remove_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = blender::ed::object::context_object(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  int mode_orig;

  if (!scene || !ob) {
    return OPERATOR_CANCELLED;
  }

  mode_orig = ob->mode;
  ParticleSystem *psys = psys_get_current(ob);
  object_remove_particle_system(bmain, scene, ob, psys);

  /* possible this isn't the active object
   * object_remove_particle_system() clears the mode on the last psys
   */
  if (mode_orig & OB_MODE_PARTICLE_EDIT) {
    if ((ob->mode & OB_MODE_PARTICLE_EDIT) == 0) {
      BKE_view_layer_synced_ensure(scene, view_layer);
      if (BKE_view_layer_active_object_get(view_layer) == ob) {
        WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, nullptr);
      }
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_POINTCACHE, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_particle_system_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Particle System Slot";
  ot->idname = "OBJECT_OT_particle_system_remove";
  ot->description = "Remove the selected particle system";

  /* API callbacks. */
  ot->poll = ED_operator_object_active_local_editable;
  ot->exec = particle_system_remove_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** new particle settings operator *********************/

static bool psys_poll(bContext *C)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
  return (ptr.data != nullptr);
}

static wmOperatorStatus new_particle_settings_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  ParticleSystem *psys;
  ParticleSettings *part = nullptr;
  Object *ob;
  PointerRNA ptr;

  ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);

  psys = static_cast<ParticleSystem *>(ptr.data);

  /* add or copy particle setting */
  if (psys->part) {
    part = reinterpret_cast<ParticleSettings *>(BKE_id_copy_ex(
        bmain, &psys->part->id, nullptr, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
  }
  else {
    part = BKE_particlesettings_add(bmain, "ParticleSettings");
  }

  ob = (Object *)ptr.owner_id;

  if (psys->part) {
    id_us_min(&psys->part->id);
  }

  psys->part = part;

  psys_check_boid_data(psys);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Particle Settings";
  ot->idname = "PARTICLE_OT_new";
  ot->description = "Add new particle settings";

  /* API callbacks. */
  ot->exec = new_particle_settings_exec;
  ot->poll = psys_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** keyed particle target operators *********************/

static wmOperatorStatus new_particle_target_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
  ParticleSystem *psys = static_cast<ParticleSystem *>(ptr.data);
  Object *ob = (Object *)ptr.owner_id;

  ParticleTarget *pt;

  if (!psys) {
    return OPERATOR_CANCELLED;
  }

  pt = static_cast<ParticleTarget *>(psys->targets.first);
  for (; pt; pt = pt->next) {
    pt->flag &= ~PTARGET_CURRENT;
  }

  pt = MEM_callocN<ParticleTarget>("keyed particle target");

  pt->flag |= PTARGET_CURRENT;
  pt->psys = 1;

  BLI_addtail(&psys->targets, pt);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_new_target(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Particle Target";
  ot->idname = "PARTICLE_OT_new_target";
  ot->description = "Add a new particle target";

  /* API callbacks. */
  ot->exec = new_particle_target_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus remove_particle_target_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
  ParticleSystem *psys = static_cast<ParticleSystem *>(ptr.data);
  Object *ob = (Object *)ptr.owner_id;

  ParticleTarget *pt;

  if (!psys) {
    return OPERATOR_CANCELLED;
  }

  pt = static_cast<ParticleTarget *>(psys->targets.first);
  for (; pt; pt = pt->next) {
    if (pt->flag & PTARGET_CURRENT) {
      BLI_remlink(&psys->targets, pt);
      MEM_freeN(pt);
      break;
    }
  }
  pt = static_cast<ParticleTarget *>(psys->targets.last);

  if (pt) {
    pt->flag |= PTARGET_CURRENT;
  }

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_target_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Particle Target";
  ot->idname = "PARTICLE_OT_target_remove";
  ot->description = "Remove the selected particle target";

  /* API callbacks. */
  ot->exec = remove_particle_target_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************ move up particle target operator *********************/

static wmOperatorStatus target_move_up_exec(bContext *C, wmOperator * /*op*/)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
  ParticleSystem *psys = static_cast<ParticleSystem *>(ptr.data);
  Object *ob = (Object *)ptr.owner_id;
  ParticleTarget *pt;

  if (!psys) {
    return OPERATOR_CANCELLED;
  }

  pt = static_cast<ParticleTarget *>(psys->targets.first);
  for (; pt; pt = pt->next) {
    if (pt->flag & PTARGET_CURRENT && pt->prev) {
      BLI_remlink(&psys->targets, pt);
      BLI_insertlinkbefore(&psys->targets, pt->prev, pt);

      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, ob);
      break;
    }
  }

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_target_move_up(wmOperatorType *ot)
{
  ot->name = "Move Up Target";
  ot->idname = "PARTICLE_OT_target_move_up";
  ot->description = "Move particle target up in the list";

  ot->exec = target_move_up_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************ move down particle target operator *********************/

static wmOperatorStatus target_move_down_exec(bContext *C, wmOperator * /*op*/)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
  ParticleSystem *psys = static_cast<ParticleSystem *>(ptr.data);
  Object *ob = (Object *)ptr.owner_id;
  ParticleTarget *pt;

  if (!psys) {
    return OPERATOR_CANCELLED;
  }
  pt = static_cast<ParticleTarget *>(psys->targets.first);
  for (; pt; pt = pt->next) {
    if (pt->flag & PTARGET_CURRENT && pt->next) {
      BLI_remlink(&psys->targets, pt);
      BLI_insertlinkafter(&psys->targets, pt->next, pt);

      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, ob);
      break;
    }
  }

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_target_move_down(wmOperatorType *ot)
{
  ot->name = "Move Down Target";
  ot->idname = "PARTICLE_OT_target_move_down";
  ot->description = "Move particle target down in the list";

  ot->exec = target_move_down_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************ refresh dupli objects *********************/

static wmOperatorStatus dupliob_refresh_exec(bContext *C, wmOperator * /*op*/)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
  ParticleSystem *psys = static_cast<ParticleSystem *>(ptr.data);

  if (!psys) {
    return OPERATOR_CANCELLED;
  }

  psys_check_group_weights(psys->part);
  DEG_id_tag_update(&psys->part->id, ID_RECALC_GEOMETRY | ID_RECALC_PSYS_REDO);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, nullptr);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_dupliob_refresh(wmOperatorType *ot)
{
  ot->name = "Refresh Instance Objects";
  ot->idname = "PARTICLE_OT_dupliob_refresh";
  ot->description = "Refresh list of instance objects and their weights";

  ot->exec = dupliob_refresh_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************ move up particle dupli-weight operator *********************/

static wmOperatorStatus dupliob_move_up_exec(bContext *C, wmOperator * /*op*/)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
  ParticleSystem *psys = static_cast<ParticleSystem *>(ptr.data);
  ParticleSettings *part;

  if (!psys) {
    return OPERATOR_CANCELLED;
  }

  part = psys->part;
  LISTBASE_FOREACH (ParticleDupliWeight *, dw, &part->instance_weights) {
    if (dw->flag & PART_DUPLIW_CURRENT && dw->prev) {
      BLI_remlink(&part->instance_weights, dw);
      BLI_insertlinkbefore(&part->instance_weights, dw->prev, dw);

      DEG_id_tag_update(&part->id, ID_RECALC_GEOMETRY | ID_RECALC_PSYS_REDO);
      WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, nullptr);
      break;
    }
  }

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_dupliob_move_up(wmOperatorType *ot)
{
  ot->name = "Move Up Instance Object";
  ot->idname = "PARTICLE_OT_dupliob_move_up";
  ot->description = "Move instance object up in the list";

  ot->exec = dupliob_move_up_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** particle dupliweight operators *********************/

static wmOperatorStatus copy_particle_dupliob_exec(bContext *C, wmOperator * /*op*/)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
  ParticleSystem *psys = static_cast<ParticleSystem *>(ptr.data);
  ParticleSettings *part;

  if (!psys) {
    return OPERATOR_CANCELLED;
  }
  part = psys->part;
  LISTBASE_FOREACH (ParticleDupliWeight *, dw, &part->instance_weights) {
    if (dw->flag & PART_DUPLIW_CURRENT) {
      dw->flag &= ~PART_DUPLIW_CURRENT;
      dw = static_cast<ParticleDupliWeight *>(MEM_dupallocN(dw));
      dw->flag |= PART_DUPLIW_CURRENT;
      BLI_addhead(&part->instance_weights, dw);

      DEG_id_tag_update(&part->id, ID_RECALC_GEOMETRY | ID_RECALC_PSYS_REDO);
      WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, nullptr);
      break;
    }
  }

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_dupliob_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Particle Instance Object";
  ot->idname = "PARTICLE_OT_dupliob_copy";
  ot->description = "Duplicate the current instance object";

  /* API callbacks. */
  ot->exec = copy_particle_dupliob_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus remove_particle_dupliob_exec(bContext *C, wmOperator * /*op*/)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
  ParticleSystem *psys = static_cast<ParticleSystem *>(ptr.data);
  ParticleSettings *part;

  if (!psys) {
    return OPERATOR_CANCELLED;
  }

  part = psys->part;
  LISTBASE_FOREACH (ParticleDupliWeight *, dw, &part->instance_weights) {
    if (dw->flag & PART_DUPLIW_CURRENT) {
      BLI_remlink(&part->instance_weights, dw);
      MEM_freeN(dw);
      break;
    }
  }

  ParticleDupliWeight *dw = static_cast<ParticleDupliWeight *>(part->instance_weights.last);
  if (dw) {
    dw->flag |= PART_DUPLIW_CURRENT;
  }

  DEG_id_tag_update(&part->id, ID_RECALC_GEOMETRY | ID_RECALC_PSYS_REDO);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, nullptr);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_dupliob_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Particle Instance Object";
  ot->idname = "PARTICLE_OT_dupliob_remove";
  ot->description = "Remove the selected instance object";

  /* API callbacks. */
  ot->exec = remove_particle_dupliob_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************ move down particle dupliweight operator *********************/

static wmOperatorStatus dupliob_move_down_exec(bContext *C, wmOperator * /*op*/)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
  ParticleSystem *psys = static_cast<ParticleSystem *>(ptr.data);
  ParticleSettings *part;

  if (!psys) {
    return OPERATOR_CANCELLED;
  }

  part = psys->part;
  LISTBASE_FOREACH (ParticleDupliWeight *, dw, &part->instance_weights) {
    if (dw->flag & PART_DUPLIW_CURRENT && dw->next) {
      BLI_remlink(&part->instance_weights, dw);
      BLI_insertlinkafter(&part->instance_weights, dw->next, dw);

      DEG_id_tag_update(&part->id, ID_RECALC_GEOMETRY | ID_RECALC_PSYS_REDO);
      WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, nullptr);
      break;
    }
  }

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_dupliob_move_down(wmOperatorType *ot)
{
  ot->name = "Move Down Instance Object";
  ot->idname = "PARTICLE_OT_dupliob_move_down";
  ot->description = "Move instance object down in the list";

  ot->exec = dupliob_move_down_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************ connect/disconnect hair operators *********************/

static void disconnect_hair(Depsgraph *depsgraph, Scene *scene, Object *ob, ParticleSystem *psys)
{
  Object *object_eval = DEG_get_evaluated(depsgraph, ob);
  ParticleSystem *psys_eval = psys_eval_get(depsgraph, ob, psys);
  ParticleSystemModifierData *psmd_eval = psys_get_modifier(object_eval, psys_eval);
  ParticleEditSettings *pset = PE_settings(scene);
  ParticleData *pa;
  PTCacheEdit *edit;
  PTCacheEditPoint *point;
  PTCacheEditKey *ekey = nullptr;
  HairKey *key;
  int i, k;
  float hairmat[4][4];

  if (!ob || !psys || psys->flag & PSYS_GLOBAL_HAIR) {
    return;
  }

  if (!psys->part || psys->part->type != PART_HAIR) {
    return;
  }

  edit = psys->edit;
  point = edit ? edit->points : nullptr;

  for (i = 0, pa = psys->particles; i < psys->totpart; i++, pa++) {
    if (point) {
      ekey = point->keys;
      point++;
    }

    psys_mat_hair_to_global(ob, psmd_eval->mesh_final, psys->part->from, pa, hairmat);

    for (k = 0, key = pa->hair; k < pa->totkey; k++, key++) {
      mul_m4_v3(hairmat, key->co);

      if (ekey) {
        ekey->flag &= ~PEK_USE_WCO;
        ekey++;
      }
    }
  }

  psys_free_path_cache(psys, psys->edit);

  psys->flag |= PSYS_GLOBAL_HAIR;

  if (ELEM(pset->brushtype, PE_BRUSH_ADD, PE_BRUSH_PUFF)) {
    pset->brushtype = PE_BRUSH_COMB;
  }

  PE_update_object(depsgraph, scene, ob, 0);
}

static wmOperatorStatus disconnect_hair_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = blender::ed::object::context_object(C);
  ParticleSystem *psys = nullptr;
  const bool all = RNA_boolean_get(op->ptr, "all");

  if (!ob) {
    return OPERATOR_CANCELLED;
  }

  if (all) {
    LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
      disconnect_hair(depsgraph, scene, ob, psys);
    }
  }
  else {
    psys = psys_get_current(ob);
    disconnect_hair(depsgraph, scene, ob, psys);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_disconnect_hair(wmOperatorType *ot)
{
  ot->name = "Disconnect Hair";
  ot->description = "Disconnect hair from the emitter mesh";
  ot->idname = "PARTICLE_OT_disconnect_hair";

  ot->exec = disconnect_hair_exec;

  /* flags */
  /* No REGISTER, redo does not work due to missing update, see #47750. */
  ot->flag = OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "all", false, "All Hair", "Disconnect all hair systems from the emitter mesh");
}

/* from/to_world_space : whether from/to particles are in world or hair space
 * from/to_mat : additional transform for from/to particles (e.g. for using object space copying)
 */
static bool remap_hair_emitter(Depsgraph *depsgraph,
                               Scene *scene,
                               Object *ob,
                               ParticleSystem *psys,
                               Object *target_ob,
                               ParticleSystem *target_psys,
                               PTCacheEdit *target_edit,
                               const float from_mat[4][4],
                               const float to_mat[4][4],
                               bool from_global,
                               bool to_global)
{
  Object *object_eval = DEG_get_evaluated(depsgraph, ob);
  ParticleSystem *psys_eval = psys_eval_get(depsgraph, ob, psys);
  ParticleSystemModifierData *target_psmd = psys_get_modifier(object_eval, psys_eval);
  ParticleData *pa, *tpa;
  PTCacheEditPoint *edit_point;
  PTCacheEditKey *ekey;
  blender::bke::BVHTreeFromMesh bvhtree = {nullptr};
  const MFace *mface = nullptr, *mf;
  const blender::int2 *edges = nullptr, *edge;
  Mesh *mesh, *target_mesh;
  int numverts;
  int k;
  float from_ob_imat[4][4], to_ob_imat[4][4];
  float from_imat[4][4], to_imat[4][4];

  if (!target_psmd->mesh_final) {
    return false;
  }
  if (!psys->part || psys->part->type != PART_HAIR) {
    return false;
  }
  if (!target_psys->part || target_psys->part->type != PART_HAIR) {
    return false;
  }

  edit_point = target_edit ? target_edit->points : nullptr;

  invert_m4_m4(from_ob_imat, ob->object_to_world().ptr());
  invert_m4_m4(to_ob_imat, target_ob->object_to_world().ptr());
  invert_m4_m4(from_imat, from_mat);
  invert_m4_m4(to_imat, to_mat);

  const bool use_dm_final_indices = (target_psys->part->use_modifier_stack &&
                                     !target_psmd->mesh_final->runtime->deformed_only);

  if (use_dm_final_indices || !target_psmd->mesh_original) {
    mesh = target_psmd->mesh_final;
  }
  else {
    mesh = target_psmd->mesh_original;
  }
  target_mesh = target_psmd->mesh_final;
  if (mesh == nullptr) {
    return false;
  }
  /* don't modify the original vertices */
  /* we don't want to mess up target_psmd->dm when converting to global coordinates below */
  mesh = (Mesh *)BKE_id_copy_ex(nullptr, &mesh->id, nullptr, LIB_ID_COPY_LOCALIZE);

  /* BMESH_ONLY, deform dm may not have tessface */
  BKE_mesh_tessface_ensure(mesh);

  numverts = mesh->verts_num;
  blender::MutableSpan<blender::float3> positions = mesh->vert_positions_for_write();

  /* convert to global coordinates */
  for (int i = 0; i < numverts; i++) {
    mul_m4_v3(to_mat, positions[i]);
  }

  if (mesh->totface_legacy != 0) {
    mface = static_cast<const MFace *>(CustomData_get_layer(&mesh->fdata_legacy, CD_MFACE));
    bvhtree = mesh->bvh_legacy_faces();
  }
  else if (mesh->edges_num != 0) {
    edges = mesh->edges().data();
    bvhtree = mesh->bvh_edges();
  }
  else {
    BKE_id_free(nullptr, mesh);
    return false;
  }

  int i;
  for (i = 0, tpa = target_psys->particles, pa = psys->particles; i < target_psys->totpart;
       i++, tpa++, pa++)
  {
    float from_co[3];
    BVHTreeNearest nearest;

    if (from_global) {
      mul_v3_m4v3(from_co, from_ob_imat, pa->hair[0].co);
    }
    else {
      mul_v3_m4v3(from_co, from_ob_imat, pa->hair[0].world_co);
    }
    mul_m4_v3(from_mat, from_co);

    nearest.index = -1;
    nearest.dist_sq = FLT_MAX;

    BLI_bvhtree_find_nearest(bvhtree.tree, from_co, &nearest, bvhtree.nearest_callback, &bvhtree);

    if (nearest.index == -1) {
      if (G.debug & G_DEBUG) {
        printf("No nearest point found for hair root!");
      }
      continue;
    }

    if (mface) {
      float v[4][3];

      mf = &mface[nearest.index];

      copy_v3_v3(v[0], positions[mf->v1]);
      copy_v3_v3(v[1], positions[mf->v2]);
      copy_v3_v3(v[2], positions[mf->v3]);
      if (mf->v4) {
        copy_v3_v3(v[3], positions[mf->v4]);
        interp_weights_poly_v3(tpa->fuv, v, 4, nearest.co);
      }
      else {
        interp_weights_poly_v3(tpa->fuv, v, 3, nearest.co);
      }
      tpa->foffset = 0.0f;

      tpa->num = nearest.index;
      if (use_dm_final_indices) {
        tpa->num_dmcache = DMCACHE_ISCHILD;
      }
      else {
        tpa->num_dmcache = psys_particle_dm_face_lookup(
            target_psmd->mesh_final, target_psmd->mesh_original, tpa->num, tpa->fuv, nullptr);
      }
    }
    else {
      edge = &edges[nearest.index];

      tpa->fuv[1] = line_point_factor_v3(nearest.co, positions[edge->x], positions[edge->y]);
      tpa->fuv[0] = 1.0f - tpa->fuv[1];
      tpa->fuv[2] = tpa->fuv[3] = 0.0f;
      tpa->foffset = 0.0f;

      tpa->num = nearest.index;
      tpa->num_dmcache = -1;
    }

    /* translate hair keys */
    {
      HairKey *key, *tkey;
      float hairmat[4][4], imat[4][4];
      float offset[3];

      if (to_global) {
        copy_m4_m4(imat, target_ob->object_to_world().ptr());
      }
      else {
        /* NOTE: using target_dm here, which is in target_ob object space and has full modifiers.
         */
        psys_mat_hair_to_object(target_ob, target_mesh, target_psys->part->from, tpa, hairmat);
        invert_m4_m4(imat, hairmat);
      }
      mul_m4_m4m4(imat, imat, to_imat);

      /* offset in world space */
      sub_v3_v3v3(offset, nearest.co, from_co);

      if (edit_point) {
        for (k = 0, key = pa->hair, tkey = tpa->hair, ekey = edit_point->keys; k < tpa->totkey;
             k++, key++, tkey++, ekey++)
        {
          float co_orig[3];

          if (from_global) {
            mul_v3_m4v3(co_orig, from_ob_imat, key->co);
          }
          else {
            mul_v3_m4v3(co_orig, from_ob_imat, key->world_co);
          }
          mul_m4_v3(from_mat, co_orig);

          add_v3_v3v3(tkey->co, co_orig, offset);

          mul_m4_v3(imat, tkey->co);

          ekey->flag |= PEK_USE_WCO;
        }

        edit_point++;
      }
      else {
        for (k = 0, key = pa->hair, tkey = tpa->hair; k < tpa->totkey; k++, key++, tkey++) {
          float co_orig[3];

          if (from_global) {
            mul_v3_m4v3(co_orig, from_ob_imat, key->co);
          }
          else {
            mul_v3_m4v3(co_orig, from_ob_imat, key->world_co);
          }
          mul_m4_v3(from_mat, co_orig);

          add_v3_v3v3(tkey->co, co_orig, offset);

          mul_m4_v3(imat, tkey->co);
        }
      }
    }
  }

  BKE_id_free(nullptr, mesh);

  psys_free_path_cache(target_psys, target_edit);

  PE_update_object(depsgraph, scene, target_ob, 0);

  return true;
}

static bool connect_hair(Depsgraph *depsgraph, Scene *scene, Object *ob, ParticleSystem *psys)
{
  bool ok;

  if (!psys) {
    return false;
  }

  ok = remap_hair_emitter(depsgraph,
                          scene,
                          ob,
                          psys,
                          ob,
                          psys,
                          psys->edit,
                          ob->object_to_world().ptr(),
                          ob->object_to_world().ptr(),
                          psys->flag & PSYS_GLOBAL_HAIR,
                          false);
  if (ok) {
    psys->flag &= ~PSYS_GLOBAL_HAIR;
  }

  return ok;
}

static wmOperatorStatus connect_hair_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = blender::ed::object::context_object(C);
  ParticleSystem *psys = nullptr;
  const bool all = RNA_boolean_get(op->ptr, "all");
  bool any_connected = false;

  if (!ob) {
    return OPERATOR_CANCELLED;
  }

  if (all) {
    LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
      any_connected |= connect_hair(depsgraph, scene, ob, psys);
    }
  }
  else {
    psys = psys_get_current(ob);
    any_connected |= connect_hair(depsgraph, scene, ob, psys);
  }

  if (!any_connected) {
    BKE_report(op->reports,
               RPT_WARNING,
               "No hair connected (cannot connect hair if particle system modifier is disabled)");
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_connect_hair(wmOperatorType *ot)
{
  ot->name = "Connect Hair";
  ot->description = "Connect hair to the emitter mesh";
  ot->idname = "PARTICLE_OT_connect_hair";

  ot->exec = connect_hair_exec;

  /* flags */
  /* No REGISTER, redo does not work due to missing update, see #47750. */
  ot->flag = OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "all", false, "All Hair", "Connect all hair systems to the emitter mesh");
}

/************************ particle system copy operator *********************/

enum eCopyParticlesSpace {
  PAR_COPY_SPACE_OBJECT = 0,
  PAR_COPY_SPACE_WORLD = 1,
};

static void copy_particle_edit(Depsgraph *depsgraph,
                               Scene *scene,
                               Object *ob,
                               ParticleSystem *psys,
                               ParticleSystem *psys_from)
{
  PTCacheEdit *edit_from = psys_from->edit, *edit;
  ParticleData *pa;
  KEY_K;
  POINT_P;

  if (!edit_from) {
    return;
  }

  edit = static_cast<PTCacheEdit *>(MEM_dupallocN(edit_from));
  edit->psys = psys;
  psys->edit = edit;

  edit->pathcache = nullptr;
  edit->mirror_cache = nullptr;
  BLI_listbase_clear(&edit->pathcachebufs);

  edit->emitter_field = nullptr;
  edit->emitter_cosnos = nullptr;

  edit->points = static_cast<PTCacheEditPoint *>(MEM_dupallocN(edit_from->points));
  pa = psys->particles;
  LOOP_POINTS {
    HairKey *hkey = pa->hair;

    point->keys = static_cast<PTCacheEditKey *>(MEM_dupallocN(point->keys));
    LOOP_KEYS {
      key->co = hkey->co;
      key->time = &hkey->time;
      key->flag = hkey->editflag;
      if (!(psys->flag & PSYS_GLOBAL_HAIR)) {
        key->flag |= PEK_USE_WCO;
        hkey->editflag |= PEK_USE_WCO;
      }

      hkey++;
    }

    pa++;
  }
  update_world_cos(ob, edit);

  recalc_lengths(edit);
  recalc_emitter_field(depsgraph, ob, psys);
  PE_update_object(depsgraph, scene, ob, true);
}

static void remove_particle_systems_from_object(Object *ob_to)
{
  ModifierData *md, *md_next;

  if (ob_to->type != OB_MESH) {
    return;
  }
  if (!ob_to->data || !ID_IS_EDITABLE(ob_to->data) || ID_IS_OVERRIDE_LIBRARY(ob_to->data)) {
    return;
  }

  for (md = static_cast<ModifierData *>(ob_to->modifiers.first); md; md = md_next) {
    md_next = md->next;

    /* remove all particle system modifiers as well,
     * these need to sync to the particle system list
     */
    if (ELEM(md->type,
             eModifierType_ParticleSystem,
             eModifierType_DynamicPaint,
             eModifierType_Fluid))
    {
      BLI_remlink(&ob_to->modifiers, md);
      BKE_modifier_free(md);
    }
  }

  BKE_object_free_particlesystems(ob_to);
}

/* single_psys_from is optional, if nullptr all psys of ob_from are copied */
static bool copy_particle_systems_to_object(const bContext *C,
                                            Scene *scene,
                                            Object *ob_from,
                                            ParticleSystem *single_psys_from,
                                            Object *ob_to,
                                            int space,
                                            bool duplicate_settings)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ModifierData *md;
  ParticleSystem *psys_start = nullptr, *psys, *psys_from;
  ParticleSystem **tmp_psys;
  CustomData_MeshMasks cdmask = {0};
  int i, totpsys;

  if (ob_to->type != OB_MESH) {
    return false;
  }
  if (!ob_to->data || !BKE_id_is_editable(bmain, static_cast<const ID *>(ob_to->data))) {
    return false;
  }

/* For remapping we need a valid DM.
 * Because the modifiers are appended at the end it's safe to use
 * the final DM of the object without particles.
 * However, when evaluating the DM all the particle modifiers must be valid,
 * i.e. have the psys assigned already.
 *
 * To break this hen/egg problem we create all psys separately first
 * (to collect required customdata masks),
 * then create the DM, then add them to the object and make the psys modifiers.
 */
#define PSYS_FROM_FIRST \
  static_cast<ParticleSystem *>( \
      (single_psys_from ? single_psys_from : ob_from->particlesystem.first))
#define PSYS_FROM_NEXT(cur) (single_psys_from ? nullptr : (cur)->next)
  totpsys = single_psys_from ? 1 : BLI_listbase_count(&ob_from->particlesystem);

  tmp_psys = MEM_malloc_arrayN<ParticleSystem *>(totpsys, "temporary particle system array");

  for (psys_from = PSYS_FROM_FIRST, i = 0; psys_from; psys_from = PSYS_FROM_NEXT(psys_from), i++) {
    psys = BKE_object_copy_particlesystem(psys_from, 0);
    tmp_psys[i] = psys;

    if (psys_start == nullptr) {
      psys_start = psys;
    }

    psys_emitter_customdata_mask(psys, &cdmask);
  }
  /* to iterate source and target psys in sync,
   * we need to know where the newly added psys start
   */
  psys_start = totpsys > 0 ? tmp_psys[0] : nullptr;

  /* now append psys to the object and make modifiers */
  for (i = 0, psys_from = PSYS_FROM_FIRST; i < totpsys; ++i, psys_from = PSYS_FROM_NEXT(psys_from))
  {
    ParticleSystemModifierData *psmd;

    psys = tmp_psys[i];

    /* append to the object */
    BLI_addtail(&ob_to->particlesystem, psys);
    psys_unique_name(ob_to, psys, psys->name);

    /* add a particle system modifier for each system */
    md = BKE_modifier_new(eModifierType_ParticleSystem);
    psmd = (ParticleSystemModifierData *)md;
    /* push on top of the stack, no use trying to reproduce old stack order */
    BLI_addtail(&ob_to->modifiers, md);
    BKE_modifiers_persistent_uid_init(*ob_to, *md);

    SNPRINTF_UTF8(md->name, "ParticleSystem %i", i);
    BKE_modifier_unique_name(&ob_to->modifiers, (ModifierData *)psmd);

    psmd->psys = psys;

    if (psys_from->edit) {
      copy_particle_edit(depsgraph, scene, ob_to, psys, psys_from);
    }

    if (duplicate_settings) {
      id_us_min(&psys->part->id);
      psys->part = (ParticleSettings *)BKE_id_copy(bmain, &psys->part->id);
    }
  }
  MEM_freeN(tmp_psys);

  /* NOTE: do this after creating DM copies for all the particle system modifiers,
   * the remapping otherwise makes final_dm invalid!
   */
  for (psys = psys_start, psys_from = PSYS_FROM_FIRST, i = 0; psys;
       psys = psys->next, psys_from = PSYS_FROM_NEXT(psys_from), i++)
  {
    const float (*from_mat)[4], (*to_mat)[4];

    switch (space) {
      case PAR_COPY_SPACE_OBJECT:
        from_mat = I;
        to_mat = I;
        break;
      case PAR_COPY_SPACE_WORLD:
        from_mat = ob_from->object_to_world().ptr();
        to_mat = ob_to->object_to_world().ptr();
        break;
      default:
        /* should not happen */
        from_mat = to_mat = nullptr;
        BLI_assert(false);
        break;
    }
    if (ob_from != ob_to) {
      remap_hair_emitter(depsgraph,
                         scene,
                         ob_from,
                         psys_from,
                         ob_to,
                         psys,
                         psys->edit,
                         from_mat,
                         to_mat,
                         psys_from->flag & PSYS_GLOBAL_HAIR,
                         psys->flag & PSYS_GLOBAL_HAIR);
    }

    /* tag for recalc */
    //      psys->recalc |= ID_RECALC_PSYS_RESET;
  }

#undef PSYS_FROM_FIRST
#undef PSYS_FROM_NEXT

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&ob_to->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_PARTICLE | NA_EDITED, ob_to);
  return true;
}

static bool copy_particle_systems_poll(bContext *C)
{
  Object *ob;
  if (!ED_operator_object_active_local_editable(C)) {
    return false;
  }

  ob = blender::ed::object::context_active_object(C);
  if (BLI_listbase_is_empty(&ob->particlesystem)) {
    return false;
  }

  return true;
}

static wmOperatorStatus copy_particle_systems_exec(bContext *C, wmOperator *op)
{
  const int space = RNA_enum_get(op->ptr, "space");
  const bool remove_target_particles = RNA_boolean_get(op->ptr, "remove_target_particles");
  const bool use_active = RNA_boolean_get(op->ptr, "use_active");
  Scene *scene = CTX_data_scene(C);
  Object *ob_from = blender::ed::object::context_active_object(C);

  ParticleSystem *psys_from = nullptr;
  if (use_active) {
    psys_from = static_cast<ParticleSystem *>(
        CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem).data);
    if (psys_from == nullptr) {
      /* Particle System context pointer is only valid in the Properties Editor. */
      psys_from = psys_get_current(ob_from);
    }
  }

  int changed_tot = 0;
  int fail = 0;

  CTX_DATA_BEGIN (C, Object *, ob_to, selected_editable_objects) {
    if (ob_from != ob_to) {
      bool changed = false;
      if (remove_target_particles) {
        remove_particle_systems_from_object(ob_to);
        changed = true;
      }
      if (copy_particle_systems_to_object(C, scene, ob_from, psys_from, ob_to, space, false)) {
        changed = true;
      }
      else {
        fail++;
      }

      if (changed) {
        changed_tot++;
      }
    }
  }
  CTX_DATA_END;

  if (changed_tot > 0) {
    DEG_relations_tag_update(CTX_data_main(C));
  }

  if ((changed_tot == 0 && fail == 0) || fail) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Copy particle systems to selected: %d done, %d failed",
                changed_tot,
                fail);
  }

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_copy_particle_systems(wmOperatorType *ot)
{
  static const EnumPropertyItem space_items[] = {
      {PAR_COPY_SPACE_OBJECT, "OBJECT", 0, "Object", "Copy inside each object's local space"},
      {PAR_COPY_SPACE_WORLD, "WORLD", 0, "World", "Copy in world space"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  ot->name = "Copy Particle Systems";
  ot->description = "Copy particle systems from the active object to selected objects";
  ot->idname = "PARTICLE_OT_copy_particle_systems";

  ot->poll = copy_particle_systems_poll;
  ot->exec = copy_particle_systems_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "space",
               space_items,
               PAR_COPY_SPACE_OBJECT,
               "Space",
               "Space transform for copying from one object to another");
  RNA_def_boolean(ot->srna,
                  "remove_target_particles",
                  true,
                  "Remove Target Particles",
                  "Remove particle systems on the target objects");
  RNA_def_boolean(ot->srna,
                  "use_active",
                  false,
                  "Use Active",
                  "Use the active particle system from the context");
}

static bool duplicate_particle_systems_poll(bContext *C)
{
  if (!ED_operator_object_active_local_editable(C)) {
    return false;
  }
  Object *ob = blender::ed::object::context_active_object(C);
  if (BLI_listbase_is_empty(&ob->particlesystem)) {
    return false;
  }
  if (ob->mode != OB_MODE_OBJECT) {
    CTX_wm_operator_poll_msg_set(C, "Object must be in object mode");
    return false;
  }
  return true;
}

static wmOperatorStatus duplicate_particle_systems_exec(bContext *C, wmOperator *op)
{
  const bool duplicate_settings = RNA_boolean_get(op->ptr, "use_duplicate_settings");
  Scene *scene = CTX_data_scene(C);
  Object *ob = blender::ed::object::context_active_object(C);
  /* Context pointer is only valid in the Properties Editor. */
  ParticleSystem *psys = static_cast<ParticleSystem *>(
      CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem).data);
  if (psys == nullptr) {
    psys = psys_get_current(ob);
  }

  copy_particle_systems_to_object(
      C, scene, ob, psys, ob, PAR_COPY_SPACE_OBJECT, duplicate_settings);
  return OPERATOR_FINISHED;
}

void PARTICLE_OT_duplicate_particle_system(wmOperatorType *ot)
{
  ot->name = "Duplicate Particle System";
  ot->description = "Duplicate particle system within the active object";
  ot->idname = "PARTICLE_OT_duplicate_particle_system";

  ot->poll = duplicate_particle_systems_poll;
  ot->exec = duplicate_particle_systems_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "use_duplicate_settings",
                  false,
                  "Duplicate Settings",
                  "Duplicate settings as well, so the new particle system uses its own settings");
}

static bool remove_all_particle_systems_poll(bContext *C)
{
  if (!ED_operator_object_active_local_editable(C)) {
    return false;
  }
  const Object *ob = blender::ed::object::context_active_object(C);
  if (BLI_listbase_is_empty(&ob->particlesystem)) {
    return false;
  }
  if (ob->mode != OB_MODE_OBJECT) {
    CTX_wm_operator_poll_msg_set(C, "Object must be in object mode");
    return false;
  }
  return true;
}
static wmOperatorStatus particle_system_remove_all_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = blender::ed::object::context_object(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (!scene || !ob) {
    return OPERATOR_CANCELLED;
  }

  const eObjectMode mode_orig = eObjectMode(ob->mode);
  LISTBASE_FOREACH_MUTABLE (ParticleSystem *, psys, &ob->particlesystem) {
    object_remove_particle_system(bmain, scene, ob, psys);
  }

  /* possible this isn't the active object
   * object_remove_particle_system() clears the mode on the last psys
   */
  if (mode_orig & OB_MODE_PARTICLE_EDIT) {
    if ((ob->mode & OB_MODE_PARTICLE_EDIT) == 0) {
      BKE_view_layer_synced_ensure(scene, view_layer);
      if (BKE_view_layer_active_object_get(view_layer) == ob) {
        WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, nullptr);
      }
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_POINTCACHE, ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_particle_system_remove_all(wmOperatorType *ot)
{
  ot->name = "Remove All Particle Systems";
  ot->description = "Remove all particle system within the active object";
  ot->idname = "PARTICLE_OT_particle_system_remove_all";

  ot->poll = remove_all_particle_systems_poll;
  ot->exec = particle_system_remove_all_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
