/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLT_translation.hh"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "DNA_armature_types.h"
#include "DNA_cloth_types.h"
#include "DNA_curve_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_grease_pencil_vertex_groups.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
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
  /* these cases don't use names to refer to vertex groups, so when
   * they get removed the numbers get out of sync, this corrects that */

  if (ob->soft) {
    ob->soft->vertgroup = map[ob->soft->vertgroup];
  }

  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (md->type == eModifierType_Explode) {
      ExplodeModifierData *emd = (ExplodeModifierData *)md;
      emd->vgroup = map[emd->vgroup];
    }
    else if (md->type == eModifierType_Cloth) {
      ClothModifierData *clmd = (ClothModifierData *)md;
      ClothSimSettings *clsim = clmd->sim_parms;
      ClothCollSettings *clcoll = clmd->coll_parms;

      if (clsim) {
        clsim->vgroup_mass = map[clsim->vgroup_mass];
        clsim->vgroup_shrink = map[clsim->vgroup_shrink];
        clsim->vgroup_struct = map[clsim->vgroup_struct];
        clsim->vgroup_shear = map[clsim->vgroup_shear];
        clsim->vgroup_bend = map[clsim->vgroup_bend];
        clsim->vgroup_intern = map[clsim->vgroup_intern];
        clsim->vgroup_pressure = map[clsim->vgroup_pressure];
        clcoll->vgroup_selfcol = map[clcoll->vgroup_selfcol];
        clcoll->vgroup_objcol = map[clcoll->vgroup_objcol];
      }
    }
  }

  LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
    for (int a = 0; a < PSYS_TOT_VG; a++) {
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
    return ((Mesh *)id)->deform_verts_for_write().data();
  }
  if (GS(id->name) == ID_LT) {
    Lattice *lt = (Lattice *)id;
    lt->dvert = MEM_calloc_arrayN<MDeformVert>(
        size_t(lt->pntsu) * size_t(lt->pntsv) * size_t(lt->pntsw), "lattice deformVert");
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
  using namespace blender;
  using namespace blender::bke;
  MDeformVert *dv;
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  const int def_nr = BLI_findindex(defbase, dg);
  bool changed = false;

  if (ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);

    if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
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
      if (mesh->deform_verts().data()) {
        const AttributeAccessor attributes = mesh->attributes();
        const VArray select_vert = *attributes.lookup_or_default<bool>(
            ".select_vert", AttrDomain::Point, false);
        int i;

        dv = mesh->deform_verts_for_write().data();

        for (i = 0; i < mesh->verts_num; i++, dv++) {
          if (dv->dw && (!use_selection || select_vert[i])) {
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
  bool changed = false;

  const ListBase *defbase = BKE_object_defgroup_list(ob);

  LISTBASE_FOREACH (bDeformGroup *, dg, defbase) {
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
  int *map = MEM_malloc_arrayN<int>(size_t(defbase_tot), "vgroup del");

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
      Mesh *mesh = static_cast<Mesh *>(ob->data);
      CustomData_free_layer_active(&mesh->vert_data, CD_MDEFORMVERT);
    }
    else if (ob->type == OB_LATTICE) {
      Lattice *lt = object_defgroup_lattice_get((ID *)(ob->data));
      MEM_SAFE_FREE(lt->dvert);
    }
    else if (ob->type == OB_GREASE_PENCIL) {
      GreasePencil *grease_pencil = static_cast<GreasePencil *>(ob->data);
      blender::bke::greasepencil::clear_vertex_groups(*grease_pencil);
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
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    BMEditMesh *em = mesh->runtime->edit_mesh.get();
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
  if (BKE_object_is_in_editmode_vgroup(ob)) {
    object_defgroup_remove_edit_mode(ob, defgroup);
  }
  else {
    object_defgroup_remove_object_mode(ob, defgroup);
  }

  if (ob->type == OB_GREASE_PENCIL) {
    blender::bke::greasepencil::validate_drawing_vertex_groups(
        *static_cast<GreasePencil *>(ob->data));
  }

  BKE_object_batch_cache_dirty_tag(ob);
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
      Mesh *mesh = static_cast<Mesh *>(ob->data);
      CustomData_free_layer_active(&mesh->vert_data, CD_MDEFORMVERT);
    }
    else if (ob->type == OB_LATTICE) {
      Lattice *lt = object_defgroup_lattice_get((ID *)(ob->data));
      MEM_SAFE_FREE(lt->dvert);
    }
    else if (ob->type == OB_GREASE_PENCIL) {
      GreasePencil *grease_pencil = static_cast<GreasePencil *>(ob->data);
      blender::bke::greasepencil::clear_vertex_groups(*grease_pencil);
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
  int *vgroup_index_map = MEM_malloc_arrayN<int>(size_t(*r_map_len), "defgroup index map create");
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
        Mesh *mesh = (Mesh *)id;
        *dvert_arr = mesh->deform_verts_for_write().data();
        *dvert_tot = mesh->verts_num;
        return true;
      }
      case ID_LT: {
        Lattice *lt = object_defgroup_lattice_get(id);
        *dvert_arr = lt->dvert;
        *dvert_tot = lt->pntsu * lt->pntsv * lt->pntsw;
        return true;
      }
      case ID_GP:
        /* Should not be used with grease pencil objects. */
        dvert_arr = nullptr;
        return false;
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
  bool *lock_flags = MEM_malloc_arrayN<bool>(size_t(defbase_tot), "defflags");
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
  LISTBASE_FOREACH (bDeformGroup *, dg, defbase) {
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

    if (ELEM(md->type, eModifierType_Armature, eModifierType_GreasePencilArmature)) {
      Object *object = (md->type == eModifierType_Armature) ?
                           ((ArmatureModifierData *)md)->object :
                           ((GreasePencilArmatureModifierData *)md)->object;
      if (object && object->pose) {
        bPose *pose = object->pose;

        LISTBASE_FOREACH (bPoseChannel *, chan, &pose->chanbase) {
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

  defgroup_validmap = MEM_malloc_arrayN<bool>(size_t(defbase_tot), "wpaint valid map");

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
  bool *dg_selection = MEM_malloc_arrayN<bool>(size_t(defbase_tot), __func__);
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
      if (pchan && (pchan->flag & POSE_SELECTED)) {
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
  bDeformGroup *dg;
  switch (ob->type) {
    case OB_GREASE_PENCIL: {
      GreasePencil *grease_pencil = static_cast<GreasePencil *>(ob->data);
      dg = static_cast<bDeformGroup *>(BLI_findlink(&grease_pencil->vertex_group_names,
                                                    grease_pencil->vertex_group_active_index - 1));
      break;
    }
    default: {
      Mesh *mesh = static_cast<Mesh *>(ob->data);
      dg = static_cast<bDeformGroup *>(
          BLI_findlink(&mesh->vertex_group_names, mesh->vertex_group_active_index - 1));
      break;
    }
  }
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
      defgroup_validmap = MEM_malloc_arrayN<bool>(size_t(*r_defgroup_tot), __func__);
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
      defgroup_validmap = MEM_malloc_arrayN<bool>(size_t(*r_defgroup_tot), __func__);
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
