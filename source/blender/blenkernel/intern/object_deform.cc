/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_armature_types.h"
#include "DNA_cloth_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_mesh.hh"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_object_deform.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Misc helpers
 * \{ */

static Lattice *object_defgroup_lattice_get(ID *id)
{
  Lattice *lt = (Lattice *)id;
  BLI_assert(GS(id->name) == ID_LT);
  return (lt->editlatt) ? lt->editlatt->latt : lt;
}

void BKE_object_defgroup_remap_update_users(Object *ob, const int *map)
{
  ModifierData *md;
  ParticleSystem *psys;
  int a;

  /* these cases don't use names to refer to vertex groups, so when
   * they get removed the numbers get out of sync, this corrects that */

  if (ob->soft) {
    ob->soft->vertgroup = map[ob->soft->vertgroup];
  }

  for (md = static_cast<ModifierData *>(ob->modifiers.first); md; md = md->next) {
    if (md->type == eModifierType_Explode) {
      ExplodeModifierData *emd = (ExplodeModifierData *)md;
      emd->vgroup = map[emd->vgroup];
    }
    else if (md->type == eModifierType_Cloth) {
      ClothModifierData *clmd = (ClothModifierData *)md;
      ClothSimSettings *clsim = clmd->sim_parms;

      if (clsim) {
        clsim->vgroup_mass = map[clsim->vgroup_mass];
        clsim->vgroup_bend = map[clsim->vgroup_bend];
        clsim->vgroup_struct = map[clsim->vgroup_struct];
      }
    }
  }

  for (psys = static_cast<ParticleSystem *>(ob->particlesystem.first); psys; psys = psys->next) {
    for (a = 0; a < PSYS_TOT_VG; a++) {
      psys->vgroup[a] = map[psys->vgroup[a]];
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Group creation
 * \{ */

bDeformGroup *BKE_object_defgroup_add_name(Object *ob, const char *name)
{
  bDeformGroup *defgroup;

  if (!ob || !OB_TYPE_SUPPORT_VGROUP(ob->type)) {
    return nullptr;
  }

  defgroup = BKE_object_defgroup_new(ob, name);
  BKE_object_defgroup_active_index_set(ob, BKE_object_defgroup_count(ob));

  return defgroup;
}

bDeformGroup *BKE_object_defgroup_add(Object *ob)
{
  return BKE_object_defgroup_add_name(ob, DATA_("Group"));
}

MDeformVert *BKE_object_defgroup_data_create(ID *id)
{
  if (GS(id->name) == ID_ME) {
    return BKE_mesh_deform_verts_for_write((Mesh *)id);
  }
  if (GS(id->name) == ID_LT) {
    Lattice *lt = (Lattice *)id;
    lt->dvert = static_cast<MDeformVert *>(MEM_callocN(
        sizeof(MDeformVert) * lt->pntsu * lt->pntsv * lt->pntsw, "lattice deformVert"));
    return lt->dvert;
  }

  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Group clearing
 * \{ */

bool BKE_object_defgroup_clear(Object *ob, bDeformGroup *dg, const bool use_selection)
{
  MDeformVert *dv;
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  const int def_nr = BLI_findindex(defbase, dg);
  bool changed = false;

  if (ob->type == OB_MESH) {
    Mesh *me = static_cast<Mesh *>(ob->data);

    if (me->edit_mesh) {
      BMEditMesh *em = me->edit_mesh;
      const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

      if (cd_dvert_offset != -1) {
        BMVert *eve;
        BMIter iter;

        BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
          dv = static_cast<MDeformVert *>(BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));

          if (dv && dv->dw && (!use_selection || BM_elem_flag_test(eve, BM_ELEM_SELECT))) {
            MDeformWeight *dw = BKE_defvert_find_index(dv, def_nr);
            BKE_defvert_remove_group(dv, dw); /* dw can be nullptr */
            changed = true;
          }
        }
      }
    }
    else {
      if (BKE_mesh_deform_verts(me)) {
        const bool *select_vert = (const bool *)CustomData_get_layer_named(
            &me->vert_data, CD_PROP_BOOL, ".select_vert");
        int i;

        dv = BKE_mesh_deform_verts_for_write(me);

        for (i = 0; i < me->totvert; i++, dv++) {
          if (dv->dw && (!use_selection || (select_vert && select_vert[i]))) {
            MDeformWeight *dw = BKE_defvert_find_index(dv, def_nr);
            BKE_defvert_remove_group(dv, dw); /* dw can be nullptr */
            changed = true;
          }
        }
      }
    }
  }
  else if (ob->type == OB_LATTICE) {
    Lattice *lt = object_defgroup_lattice_get((ID *)(ob->data));

    if (lt->dvert) {
      BPoint *bp;
      int i, tot = lt->pntsu * lt->pntsv * lt->pntsw;

      for (i = 0, bp = lt->def; i < tot; i++, bp++) {
        if (!use_selection || (bp->f1 & SELECT)) {
          MDeformWeight *dw;

          dv = &lt->dvert[i];

          dw = BKE_defvert_find_index(dv, def_nr);
          BKE_defvert_remove_group(dv, dw); /* dw can be nullptr */
          changed = true;
        }
      }
    }
  }

  return changed;
}

bool BKE_object_defgroup_clear_all(Object *ob, const bool use_selection)
{
  bDeformGroup *dg;
  bool changed = false;

  const ListBase *defbase = BKE_object_defgroup_list(ob);

  for (dg = static_cast<bDeformGroup *>(defbase->first); dg; dg = dg->next) {
    if (BKE_object_defgroup_clear(ob, dg, use_selection)) {
      changed = true;
    }
  }

  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Group removal
 * \{ */

static void object_defgroup_remove_update_users(Object *ob, const int idx)
{
  int i, defbase_tot = BKE_object_defgroup_count(ob) + 1;
  int *map = static_cast<int *>(MEM_mallocN(sizeof(int) * defbase_tot, "vgroup del"));

  map[idx] = map[0] = 0;
  for (i = 1; i < idx; i++) {
    map[i] = i;
  }
  for (i = idx + 1; i < defbase_tot; i++) {
    map[i] = i - 1;
  }

  BKE_object_defgroup_remap_update_users(ob, map);
  MEM_freeN(map);
}

static void object_defgroup_remove_common(Object *ob, bDeformGroup *dg, const int def_nr)
{
  object_defgroup_remove_update_users(ob, def_nr + 1);

  /* Remove the group */
  ListBase *defbase = BKE_object_defgroup_list_mutable(ob);

  BLI_freelinkN(defbase, dg);

  /* Update the active deform index if necessary */
  const int active_index = BKE_object_defgroup_active_index_get(ob);
  if (active_index > def_nr) {
    BKE_object_defgroup_active_index_set(ob, active_index - 1);
  }

  /* Remove all deform-verts. */
  if (BLI_listbase_is_empty(defbase)) {
    if (ob->type == OB_MESH) {
      Mesh *me = static_cast<Mesh *>(ob->data);
      CustomData_free_layer_active(&me->vert_data, CD_MDEFORMVERT, me->totvert);
    }
    else if (ob->type == OB_LATTICE) {
      Lattice *lt = object_defgroup_lattice_get((ID *)(ob->data));
      MEM_SAFE_FREE(lt->dvert);
    }
  }
  else if (BKE_object_defgroup_active_index_get(ob) < 1) {
    /* Keep a valid active index if we still have some vgroups. */
    BKE_object_defgroup_active_index_set(ob, 1);
  }
}

static void object_defgroup_remove_object_mode(Object *ob, bDeformGroup *dg)
{
  MDeformVert *dvert_array = nullptr;
  int dvert_tot = 0;
  const ListBase *defbase = BKE_object_defgroup_list(ob);

  const int def_nr = BLI_findindex(defbase, dg);

  BLI_assert(def_nr != -1);

  BKE_object_defgroup_array_get(static_cast<ID *>(ob->data), &dvert_array, &dvert_tot);

  if (dvert_array) {
    int i, j;
    MDeformVert *dv;
    for (i = 0, dv = dvert_array; i < dvert_tot; i++, dv++) {
      MDeformWeight *dw;

      dw = BKE_defvert_find_index(dv, def_nr);
      BKE_defvert_remove_group(dv, dw); /* dw can be nullptr */

      /* inline, make into a function if anything else needs to do this */
      for (j = 0; j < dv->totweight; j++) {
        if (dv->dw[j].def_nr > def_nr) {
          dv->dw[j].def_nr--;
        }
      }
      /* done */
    }
  }

  object_defgroup_remove_common(ob, dg, def_nr);
}

static void object_defgroup_remove_edit_mode(Object *ob, bDeformGroup *dg)
{
  int i;
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  const int def_nr = BLI_findindex(defbase, dg);

  BLI_assert(def_nr != -1);

  /* Make sure that no verts are using this group - if none were removed,
   * we can skip next per-vert update. */
  if (!BKE_object_defgroup_clear(ob, dg, false)) {
    /* Nothing to do. */
  }
  /* Else, make sure that any groups with higher indices are adjusted accordingly */
  else if (ob->type == OB_MESH) {
    Mesh *me = static_cast<Mesh *>(ob->data);
    BMEditMesh *em = me->edit_mesh;
    const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

    BMIter iter;
    BMVert *eve;
    MDeformVert *dvert;

    BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
      dvert = static_cast<MDeformVert *>(BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));

      if (dvert) {
        for (i = 0; i < dvert->totweight; i++) {
          if (dvert->dw[i].def_nr > def_nr) {
            dvert->dw[i].def_nr--;
          }
        }
      }
    }
  }
  else if (ob->type == OB_LATTICE) {
    Lattice *lt = ((Lattice *)(ob->data))->editlatt->latt;
    BPoint *bp;
    MDeformVert *dvert = lt->dvert;
    int a, tot;

    if (dvert) {
      tot = lt->pntsu * lt->pntsv * lt->pntsw;
      for (a = 0, bp = lt->def; a < tot; a++, bp++, dvert++) {
        for (i = 0; i < dvert->totweight; i++) {
          if (dvert->dw[i].def_nr > def_nr) {
            dvert->dw[i].def_nr--;
          }
        }
      }
    }
  }

  object_defgroup_remove_common(ob, dg, def_nr);
}

void BKE_object_defgroup_remove(Object *ob, bDeformGroup *defgroup)
{
  if (ob->type == OB_GPENCIL_LEGACY) {
    BKE_gpencil_vgroup_remove(ob, defgroup);
  }
  else {
    if (BKE_object_is_in_editmode_vgroup(ob)) {
      object_defgroup_remove_edit_mode(ob, defgroup);
    }
    else {
      object_defgroup_remove_object_mode(ob, defgroup);
    }

    BKE_object_batch_cache_dirty_tag(ob);
  }
}

void BKE_object_defgroup_remove_all_ex(Object *ob, bool only_unlocked)
{
  ListBase *defbase = BKE_object_defgroup_list_mutable(ob);

  bDeformGroup *dg = (bDeformGroup *)defbase->first;
  const bool edit_mode = BKE_object_is_in_editmode_vgroup(ob);

  if (dg) {
    while (dg) {
      bDeformGroup *next_dg = dg->next;

      if (!only_unlocked || (dg->flag & DG_LOCK_WEIGHT) == 0) {
        if (edit_mode) {
          object_defgroup_remove_edit_mode(ob, dg);
        }
        else {
          object_defgroup_remove_object_mode(ob, dg);
        }
      }

      dg = next_dg;
    }
  }
  else { /* `defbase` is empty. */
    /* Remove all deform-verts. */
    if (ob->type == OB_MESH) {
      Mesh *me = static_cast<Mesh *>(ob->data);
      CustomData_free_layer_active(&me->vert_data, CD_MDEFORMVERT, me->totvert);
    }
    else if (ob->type == OB_LATTICE) {
      Lattice *lt = object_defgroup_lattice_get((ID *)(ob->data));
      MEM_SAFE_FREE(lt->dvert);
    }
    /* Fix counters/indices */
    BKE_object_defgroup_active_index_set(ob, 0);
  }
}

void BKE_object_defgroup_remove_all(Object *ob)
{
  BKE_object_defgroup_remove_all_ex(ob, false);
}

int *BKE_object_defgroup_index_map_create(Object *ob_src, Object *ob_dst, int *r_map_len)
{
  const ListBase *src_defbase = BKE_object_defgroup_list(ob_src);
  const ListBase *dst_defbase = BKE_object_defgroup_list(ob_dst);

  /* Build src to merged mapping of vgroup indices. */
  if (BLI_listbase_is_empty(src_defbase) || BLI_listbase_is_empty(dst_defbase)) {
    *r_map_len = 0;
    return nullptr;
  }

  bDeformGroup *dg_src;
  *r_map_len = BLI_listbase_count(src_defbase);
  int *vgroup_index_map = static_cast<int *>(
      MEM_malloc_arrayN(*r_map_len, sizeof(*vgroup_index_map), "defgroup index map create"));
  bool is_vgroup_remap_needed = false;
  int i;

  for (dg_src = static_cast<bDeformGroup *>(src_defbase->first), i = 0; dg_src;
       dg_src = dg_src->next, i++)
  {
    vgroup_index_map[i] = BKE_object_defgroup_name_index(ob_dst, dg_src->name);
    is_vgroup_remap_needed = is_vgroup_remap_needed || (vgroup_index_map[i] != i);
  }

  if (!is_vgroup_remap_needed) {
    MEM_freeN(vgroup_index_map);
    vgroup_index_map = nullptr;
    *r_map_len = 0;
  }

  return vgroup_index_map;
}

void BKE_object_defgroup_index_map_apply(MDeformVert *dvert,
                                         int dvert_len,
                                         const int *map,
                                         int map_len)
{
  if (map == nullptr || map_len == 0) {
    return;
  }

  MDeformVert *dv = dvert;
  for (int i = 0; i < dvert_len; i++, dv++) {
    int totweight = dv->totweight;
    for (int j = 0; j < totweight; j++) {
      int def_nr = dv->dw[j].def_nr;
      if (uint(def_nr) < uint(map_len) && map[def_nr] != -1) {
        dv->dw[j].def_nr = map[def_nr];
      }
      else {
        totweight--;
        dv->dw[j] = dv->dw[totweight];
        j--;
      }
    }
    if (totweight != dv->totweight) {
      if (totweight) {
        dv->dw = static_cast<MDeformWeight *>(MEM_reallocN(dv->dw, sizeof(*dv->dw) * totweight));
      }
      else {
        MEM_SAFE_FREE(dv->dw);
      }
      dv->totweight = totweight;
    }
  }
}

bool BKE_object_defgroup_array_get(ID *id, MDeformVert **dvert_arr, int *dvert_tot)
{
  if (id) {
    switch (GS(id->name)) {
      case ID_ME: {
        Mesh *me = (Mesh *)id;
        *dvert_arr = BKE_mesh_deform_verts_for_write(me);
        *dvert_tot = me->totvert;
        return true;
      }
      case ID_LT: {
        Lattice *lt = object_defgroup_lattice_get(id);
        *dvert_arr = lt->dvert;
        *dvert_tot = lt->pntsu * lt->pntsv * lt->pntsw;
        return true;
      }
      default:
        break;
    }
  }

  *dvert_arr = nullptr;
  *dvert_tot = 0;
  return false;
}

/** \} */

/* --- functions for getting vgroup aligned maps --- */

bool *BKE_object_defgroup_lock_flags_get(Object *ob, const int defbase_tot)
{
  bool is_locked = false;
  int i;
  ListBase *defbase = BKE_object_defgroup_list_mutable(ob);
  bool *lock_flags = static_cast<bool *>(MEM_mallocN(defbase_tot * sizeof(bool), "defflags"));
  bDeformGroup *defgroup;

  for (i = 0, defgroup = static_cast<bDeformGroup *>(defbase->first); i < defbase_tot && defgroup;
       defgroup = defgroup->next, i++)
  {
    lock_flags[i] = ((defgroup->flag & DG_LOCK_WEIGHT) != 0);
    is_locked |= lock_flags[i];
  }
  if (is_locked) {
    return lock_flags;
  }

  MEM_freeN(lock_flags);
  return nullptr;
}

bool *BKE_object_defgroup_validmap_get(Object *ob, const int defbase_tot)
{
  bDeformGroup *dg;
  ModifierData *md;
  bool *defgroup_validmap;
  GHash *gh;
  int i, step1 = 1;
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  VirtualModifierData virtual_modifier_data;

  if (BLI_listbase_is_empty(defbase)) {
    return nullptr;
  }

  gh = BLI_ghash_str_new_ex(__func__, defbase_tot);

  /* add all names to a hash table */
  for (dg = static_cast<bDeformGroup *>(defbase->first); dg; dg = dg->next) {
    BLI_ghash_insert(gh, dg->name, nullptr);
  }

  BLI_assert(BLI_ghash_len(gh) == defbase_tot);

  /* now loop through the armature modifiers and identify deform bones */
  for (md = static_cast<ModifierData *>(ob->modifiers.first); md;
       md = !md->next && step1 ? (step1 = 0),
      BKE_modifiers_get_virtual_modifierlist(ob, &virtual_modifier_data) :
       md->next)
  {
    if (!(md->mode & (eModifierMode_Realtime | eModifierMode_Virtual))) {
      continue;
    }

    if (md->type == eModifierType_Armature) {
      ArmatureModifierData *amd = (ArmatureModifierData *)md;

      if (amd->object && amd->object->pose) {
        bPose *pose = amd->object->pose;
        bPoseChannel *chan;

        for (chan = static_cast<bPoseChannel *>(pose->chanbase.first); chan; chan = chan->next) {
          void **val_p;
          if (chan->bone->flag & BONE_NO_DEFORM) {
            continue;
          }

          val_p = BLI_ghash_lookup_p(gh, chan->name);
          if (val_p) {
            *val_p = POINTER_FROM_INT(1);
          }
        }
      }
    }
  }

  defgroup_validmap = static_cast<bool *>(
      MEM_mallocN(sizeof(*defgroup_validmap) * defbase_tot, "wpaint valid map"));

  /* add all names to a hash table */
  for (dg = static_cast<bDeformGroup *>(defbase->first), i = 0; dg; dg = dg->next, i++) {
    defgroup_validmap[i] = (BLI_ghash_lookup(gh, dg->name) != nullptr);
  }

  BLI_assert(i == BLI_ghash_len(gh));

  BLI_ghash_free(gh, nullptr, nullptr);

  return defgroup_validmap;
}

bool *BKE_object_defgroup_selected_get(Object *ob, int defbase_tot, int *r_dg_flags_sel_tot)
{
  bool *dg_selection = static_cast<bool *>(MEM_mallocN(defbase_tot * sizeof(bool), __func__));
  bDeformGroup *defgroup;
  uint i;
  Object *armob = BKE_object_pose_armature_get(ob);
  (*r_dg_flags_sel_tot) = 0;

  const ListBase *defbase = BKE_object_defgroup_list(ob);

  if (armob) {
    bPose *pose = armob->pose;
    for (i = 0, defgroup = static_cast<bDeformGroup *>(defbase->first);
         i < defbase_tot && defgroup;
         defgroup = defgroup->next, i++)
    {
      bPoseChannel *pchan = BKE_pose_channel_find_name(pose, defgroup->name);
      if (pchan && (pchan->bone->flag & BONE_SELECTED)) {
        dg_selection[i] = true;
        (*r_dg_flags_sel_tot) += 1;
      }
      else {
        dg_selection[i] = false;
      }
    }
  }
  else {
    memset(dg_selection, false, sizeof(*dg_selection) * defbase_tot);
  }

  return dg_selection;
}

bool BKE_object_defgroup_check_lock_relative(const bool *lock_flags,
                                             const bool *validmap,
                                             int index)
{
  return validmap && validmap[index] && !(lock_flags && lock_flags[index]);
}

bool BKE_object_defgroup_check_lock_relative_multi(int defbase_tot,
                                                   const bool *lock_flags,
                                                   const bool *selected,
                                                   int sel_tot)
{
  if (lock_flags == nullptr) {
    return true;
  }

  if (selected == nullptr || sel_tot <= 1) {
    return true;
  }

  for (int i = 0; i < defbase_tot; i++) {
    if (selected[i] && lock_flags[i]) {
      return false;
    }
  }

  return true;
}

bool BKE_object_defgroup_active_is_locked(const Object *ob)
{
  Mesh *me = static_cast<Mesh *>(ob->data);
  bDeformGroup *dg = static_cast<bDeformGroup *>(
      BLI_findlink(&me->vertex_group_names, me->vertex_group_active_index - 1));
  return dg->flag & DG_LOCK_WEIGHT;
}

void BKE_object_defgroup_split_locked_validmap(
    int defbase_tot, const bool *locked, const bool *deform, bool *r_locked, bool *r_unlocked)
{
  if (!locked) {
    if (r_unlocked != deform) {
      memcpy(r_unlocked, deform, sizeof(bool) * defbase_tot);
    }
    if (r_locked) {
      memset(r_locked, 0, sizeof(bool) * defbase_tot);
    }
    return;
  }

  for (int i = 0; i < defbase_tot; i++) {
    bool is_locked = locked[i];
    bool is_deform = deform[i];

    r_locked[i] = is_deform && is_locked;
    r_unlocked[i] = is_deform && !is_locked;
  }
}

void BKE_object_defgroup_mirror_selection(Object *ob,
                                          int defbase_tot,
                                          const bool *dg_selection,
                                          bool *dg_flags_sel,
                                          int *r_dg_flags_sel_tot)
{
  const ListBase *defbase = BKE_object_defgroup_list(ob);

  bDeformGroup *defgroup;
  uint i;
  int i_mirr;

  for (i = 0, defgroup = static_cast<bDeformGroup *>(defbase->first); i < defbase_tot && defgroup;
       defgroup = defgroup->next, i++)
  {
    if (dg_selection[i]) {
      char name_flip[MAXBONENAME];

      BLI_string_flip_side_name(name_flip, defgroup->name, false, sizeof(name_flip));
      i_mirr = STREQ(name_flip, defgroup->name) ? i :
                                                  BKE_object_defgroup_name_index(ob, name_flip);

      if ((i_mirr >= 0 && i_mirr < defbase_tot) && (dg_flags_sel[i_mirr] == false)) {
        dg_flags_sel[i_mirr] = true;
        (*r_dg_flags_sel_tot) += 1;
      }
    }
  }
}

bool *BKE_object_defgroup_subset_from_select_type(Object *ob,
                                                  eVGroupSelect subset_type,
                                                  int *r_defgroup_tot,
                                                  int *r_subset_count)
{
  bool *defgroup_validmap = nullptr;

  *r_defgroup_tot = BKE_object_defgroup_count(ob);

  switch (subset_type) {
    case WT_VGROUP_ACTIVE: {
      const int def_nr_active = BKE_object_defgroup_active_index_get(ob) - 1;
      defgroup_validmap = static_cast<bool *>(
          MEM_mallocN(*r_defgroup_tot * sizeof(*defgroup_validmap), __func__));
      memset(defgroup_validmap, false, *r_defgroup_tot * sizeof(*defgroup_validmap));
      if ((def_nr_active >= 0) && (def_nr_active < *r_defgroup_tot)) {
        *r_subset_count = 1;
        defgroup_validmap[def_nr_active] = true;
      }
      else {
        *r_subset_count = 0;
      }
      break;
    }
    case WT_VGROUP_BONE_SELECT: {
      defgroup_validmap = BKE_object_defgroup_selected_get(ob, *r_defgroup_tot, r_subset_count);
      break;
    }
    case WT_VGROUP_BONE_DEFORM: {
      int i;
      defgroup_validmap = BKE_object_defgroup_validmap_get(ob, *r_defgroup_tot);
      *r_subset_count = 0;
      for (i = 0; i < *r_defgroup_tot; i++) {
        if (defgroup_validmap[i] == true) {
          *r_subset_count += 1;
        }
      }
      break;
    }
    case WT_VGROUP_BONE_DEFORM_OFF: {
      int i;
      defgroup_validmap = BKE_object_defgroup_validmap_get(ob, *r_defgroup_tot);
      *r_subset_count = 0;
      for (i = 0; i < *r_defgroup_tot; i++) {
        defgroup_validmap[i] = !defgroup_validmap[i];
        if (defgroup_validmap[i] == true) {
          *r_subset_count += 1;
        }
      }
      break;
    }
    case WT_VGROUP_ALL:
    default: {
      defgroup_validmap = static_cast<bool *>(
          MEM_mallocN(*r_defgroup_tot * sizeof(*defgroup_validmap), __func__));
      memset(defgroup_validmap, true, *r_defgroup_tot * sizeof(*defgroup_validmap));
      *r_subset_count = *r_defgroup_tot;
      break;
    }
  }

  return defgroup_validmap;
}

void BKE_object_defgroup_subset_to_index_array(const bool *defgroup_validmap,
                                               const int defgroup_tot,
                                               int *r_defgroup_subset_map)
{
  int i, j = 0;
  for (i = 0; i < defgroup_tot; i++) {
    if (defgroup_validmap[i]) {
      r_defgroup_subset_map[j++] = i;
    }
  }
}
