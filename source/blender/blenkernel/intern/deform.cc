/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_enums.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_span.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_customdata.hh"
#include "BKE_deform.hh" /* own include */
#include "BKE_grease_pencil.hh"
#include "BKE_grease_pencil_vertex_groups.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"

#include "BLO_read_write.hh"

#include "data_transfer_intern.hh"

using blender::Span;
using blender::StringRef;

bDeformGroup *BKE_object_defgroup_new(Object *ob, const StringRef name)
{
  bDeformGroup *defgroup;

  BLI_assert(OB_TYPE_SUPPORT_VGROUP(ob->type));

  defgroup = MEM_callocN<bDeformGroup>(__func__);

  name.copy_utf8_truncated(defgroup->name);

  ListBase *defbase = BKE_object_defgroup_list_mutable(ob);

  BLI_addtail(defbase, defgroup);
  BKE_object_defgroup_unique_name(defgroup, ob);

  if (ob->type == OB_GREASE_PENCIL) {
    blender::bke::greasepencil::validate_drawing_vertex_groups(
        *static_cast<GreasePencil *>(ob->data));
  }

  BKE_object_batch_cache_dirty_tag(ob);

  return defgroup;
}

void BKE_defgroup_copy_list(ListBase *outbase, const ListBase *inbase)
{
  BLI_listbase_clear(outbase);
  LISTBASE_FOREACH (const bDeformGroup *, defgroup, inbase) {
    bDeformGroup *defgroupn = BKE_defgroup_duplicate(defgroup);
    BLI_addtail(outbase, defgroupn);
  }
}

bDeformGroup *BKE_defgroup_duplicate(const bDeformGroup *ingroup)
{
  if (!ingroup) {
    BLI_assert(0);
    return nullptr;
  }

  bDeformGroup *outgroup = MEM_callocN<bDeformGroup>(__func__);

  /* For now, just copy everything over. */
  memcpy(outgroup, ingroup, sizeof(bDeformGroup));

  outgroup->next = outgroup->prev = nullptr;

  return outgroup;
}

void BKE_defvert_copy_subset(MDeformVert *dvert_dst,
                             const MDeformVert *dvert_src,
                             const bool *vgroup_subset,
                             const int vgroup_num)
{
  int defgroup;
  for (defgroup = 0; defgroup < vgroup_num; defgroup++) {
    if (vgroup_subset[defgroup]) {
      BKE_defvert_copy_index(dvert_dst, defgroup, dvert_src, defgroup);
    }
  }
}

void BKE_defvert_mirror_subset(MDeformVert *dvert_dst,
                               const MDeformVert *dvert_src,
                               const bool *vgroup_subset,
                               const int vgroup_num,
                               const int *flip_map,
                               const int flip_map_num)
{
  int defgroup;
  for (defgroup = 0; defgroup < vgroup_num && defgroup < flip_map_num; defgroup++) {
    if (vgroup_subset[defgroup] && (dvert_dst != dvert_src || flip_map[defgroup] != defgroup)) {
      BKE_defvert_copy_index(dvert_dst, flip_map[defgroup], dvert_src, defgroup);
    }
  }
}

void BKE_defvert_copy(MDeformVert *dvert_dst, const MDeformVert *dvert_src)
{
  if (dvert_dst->totweight == dvert_src->totweight) {
    if (dvert_src->totweight) {
      memcpy(dvert_dst->dw, dvert_src->dw, dvert_src->totweight * sizeof(MDeformWeight));
    }
  }
  else {
    if (dvert_dst->dw) {
      MEM_freeN(dvert_dst->dw);
    }

    if (dvert_src->totweight) {
      dvert_dst->dw = static_cast<MDeformWeight *>(MEM_dupallocN(dvert_src->dw));
    }
    else {
      dvert_dst->dw = nullptr;
    }

    dvert_dst->totweight = dvert_src->totweight;
  }
}

void BKE_defvert_copy_index(MDeformVert *dvert_dst,
                            const int defgroup_dst,
                            const MDeformVert *dvert_src,
                            const int defgroup_src)
{
  MDeformWeight *dw_dst;

  const MDeformWeight *dw_src = BKE_defvert_find_index(dvert_src, defgroup_src);

  if (dw_src) {
    /* Source is valid, ensure destination is created. */
    dw_dst = BKE_defvert_ensure_index(dvert_dst, defgroup_dst);
    dw_dst->weight = dw_src->weight;
  }
  else {
    /* Source was nullptr, assign zero (could also remove). */
    dw_dst = BKE_defvert_find_index(dvert_dst, defgroup_dst);

    if (dw_dst) {
      dw_dst->weight = 0.0f;
    }
  }
}

void BKE_defvert_sync(MDeformVert *dvert_dst, const MDeformVert *dvert_src, const bool use_ensure)
{
  if (dvert_src->totweight && dvert_dst->totweight) {
    MDeformWeight *dw_src = dvert_src->dw;
    for (int i = 0; i < dvert_src->totweight; i++, dw_src++) {
      MDeformWeight *dw_dst;
      if (use_ensure) {
        dw_dst = BKE_defvert_ensure_index(dvert_dst, dw_src->def_nr);
      }
      else {
        dw_dst = BKE_defvert_find_index(dvert_dst, dw_src->def_nr);
      }

      if (dw_dst) {
        dw_dst->weight = dw_src->weight;
      }
    }
  }
}

void BKE_defvert_sync_mapped(MDeformVert *dvert_dst,
                             const MDeformVert *dvert_src,
                             const int *flip_map,
                             const int flip_map_num,
                             const bool use_ensure)
{
  if (dvert_src->totweight && dvert_dst->totweight) {
    MDeformWeight *dw_src = dvert_src->dw;
    for (int i = 0; i < dvert_src->totweight; i++, dw_src++) {
      if (dw_src->def_nr < flip_map_num) {
        MDeformWeight *dw_dst;
        if (use_ensure) {
          dw_dst = BKE_defvert_ensure_index(dvert_dst, flip_map[dw_src->def_nr]);
        }
        else {
          dw_dst = BKE_defvert_find_index(dvert_dst, flip_map[dw_src->def_nr]);
        }

        if (dw_dst) {
          dw_dst->weight = dw_src->weight;
        }
      }
    }
  }
}

void BKE_defvert_remap(MDeformVert *dvert, const int *map, const int map_len)
{
  MDeformWeight *dw = dvert->dw;
  for (int i = dvert->totweight; i != 0; i--, dw++) {
    if (dw->def_nr < map_len) {
      BLI_assert(map[dw->def_nr] >= 0);

      dw->def_nr = map[dw->def_nr];
    }
  }
}

void BKE_defvert_normalize_subset(MDeformVert &dvert, blender::Span<bool> subset_flags)
{
  BKE_defvert_normalize_ex(dvert, subset_flags, {}, {});
}

void BKE_defvert_normalize(MDeformVert &dvert)
{
  BKE_defvert_normalize_ex(dvert, {}, {}, {});
}

void BKE_defvert_normalize_lock_map(MDeformVert &dvert,
                                    blender::Span<bool> subset_flags,
                                    blender::Span<bool> lock_flags)
{
  BKE_defvert_normalize_ex(dvert, subset_flags, lock_flags, {});
}

void BKE_defvert_normalize_ex(MDeformVert &dvert,
                              blender::Span<bool> subset_flags,
                              blender::Span<bool> lock_flags,
                              blender::Span<bool> soft_lock_flags)
{
  const bool use_subset = !subset_flags.is_empty();
  const bool use_locks = !lock_flags.is_empty();
  const bool use_soft_locks = !soft_lock_flags.is_empty();

  /* Note: confusingly, `totweight` isn't the total weight on the vertex, it's
   * the number of vertex groups assigned to the vertex. It's a DNA field, so
   * I'm leaving it named as-is for now despite it being confusing. */
  if (dvert.totweight == 0) {
    /* No vertex groups assigned: do nothing. */
    return;
  }

  if (dvert.totweight == 1) {
    /* Only one vertex group is assigned to the vertex.
     *
     * TODO: this special case for single-group vertices should be completely
     * unnecessary. The code further below works just as well for one assigned
     * group as for twenty. However, the old version of this function was *not*
     * consistent in its behavior between single-group and multi-group vertices:
     * single-group vertices would set the group weight to 1.0 even if the
     * initial weight was zero, whereas multi-group vertices with all weights
     * set to zero would be left as-is.
     *
     * I (Nathan Vegdahl) decided to leave this special case here just in case
     * any other code depends on this odd behavior. But we should revisit this
     * at some point to check if that's actually the case, and simply remove
     * this special case if nothing is depending on it. */

    MDeformWeight *dw = dvert.dw;

    if (use_subset && !subset_flags[dw->def_nr]) {
      return;
    }

    const bool is_unlocked = lock_flags.is_empty() || !lock_flags[dw->def_nr];
    if (is_unlocked) {
      dw->weight = 1.0f;
    }
    return;
  }

  blender::MutableSpan<MDeformWeight> vertex_weights = blender::MutableSpan(dvert.dw,
                                                                            dvert.totweight);

  /* Collect weights. */
  float total_locked_weight = 0.0f;
  float total_soft_locked_weight = 0.0f;
  float total_regular_weight = 0.0f; /* Neither locked nor soft locked. */
  int soft_locked_group_count = 0;
  for (MDeformWeight &dw : vertex_weights) {
    if (use_subset && !subset_flags[dw.def_nr]) {
      /* Not part of the subset being normalized. */
      continue;
    }

    if (use_locks && lock_flags[dw.def_nr]) {
      /* Locked. */
      total_locked_weight += dw.weight;
    }
    else if (use_soft_locks && soft_lock_flags[dw.def_nr]) {
      total_soft_locked_weight += dw.weight;
      soft_locked_group_count++;
    }
    else {
      total_regular_weight += dw.weight;
    }
  }

  const float available_weight = max_ff(0.0f, 1.0f - total_locked_weight);

  /* Special case: all non-hard-locked vertex groups have zero weight.
   *
   * Note: conceptually this if condition is checking for `== 0.0`, because
   * negative weights shouldn't be possible. We're just being paranoid with
   * the `<=`. */
  if (total_regular_weight <= 0.0f && total_soft_locked_weight <= 0.0f) {
    /* There isn't any "right" thing to do here.
     *
     * What we choose to do is: if there are any soft-locked groups on the
     * vertex, distribute the needed weight equally among them. If there are no
     * soft-locked groups on the vertex, we do nothing. The rationale behind
     * this is that:
     *
     * 1. Zero-weight groups should typically be treated the same as unassigned
     *    groups.
     * 2. But soft-locked groups can be treated specially: since their intended
     *    use case is indicating vertex groups that have just now had their
     *    weights set, we know they were intentionally set. Therefore even when
     *    zero-weight we can consider them assigned.
     *
     * There isn't any deep truth behind this approach, but after discussion
     * with a few people I (Nathan Vegdahl) think in practice this is likely to
     * be the least surprising behavior to users (out of several bad options).
     * In particular, when the user modifies weights with auto-normalize
     * enabled, they expect Blender to ensure normalized weights whenever
     * possible (see issue #141024), and this approach achieves that.
     *
     * However, this approach is very much worth revisiting if it ends up
     * causing other problems. */

    if (soft_locked_group_count == 0) {
      return;
    }

    const float weight = available_weight / soft_locked_group_count;
    for (MDeformWeight &dw : vertex_weights) {
      if (!subset_flags.is_empty() && !subset_flags[dw.def_nr]) {
        /* Not part of the subset being normalized. */
        continue;
      }

      if (!lock_flags.is_empty() && lock_flags[dw.def_nr]) {
        /* Locked. */
        continue;
      }

      if (use_soft_locks && soft_lock_flags[dw.def_nr]) {
        dw.weight = weight;
      }
    }

    return;
  }

  /* Compute scale factors for soft-locked and regular group weights. */
  float soft_locked_scale;
  float regular_scale;
  const bool must_adjust_soft_locked = total_soft_locked_weight >= available_weight ||
                                       total_regular_weight <= 0.0f;
  if (must_adjust_soft_locked) {
    soft_locked_scale = available_weight / total_soft_locked_weight;
    regular_scale = 0.0f;
  }
  else {
    soft_locked_scale = 1.0f;
    regular_scale = (available_weight - total_soft_locked_weight) / total_regular_weight;
  }

  /* Normalize the weights via scaling by the appropriate factors. */
  for (MDeformWeight &dw : vertex_weights) {
    if (use_subset && !subset_flags[dw.def_nr]) {
      /* Not part of the subset being normalized. */
      continue;
    }

    if (use_locks && lock_flags[dw.def_nr]) {
      /* Locked. */
      continue;
    }

    if (use_soft_locks && soft_lock_flags[dw.def_nr]) {
      dw.weight *= soft_locked_scale;
    }
    else {
      dw.weight *= regular_scale;
    }

    /* In case of division errors with very low weights. */
    CLAMP(dw.weight, 0.0f, 1.0f);
  }
}

void BKE_defvert_flip(MDeformVert *dvert, const int *flip_map, const int flip_map_num)
{
  MDeformWeight *dw;
  int i;

  for (dw = dvert->dw, i = 0; i < dvert->totweight; dw++, i++) {
    if (dw->def_nr < flip_map_num) {
      if (flip_map[dw->def_nr] >= 0) {
        dw->def_nr = flip_map[dw->def_nr];
      }
    }
  }
}

void BKE_defvert_flip_merged(MDeformVert *dvert, const int *flip_map, const int flip_map_num)
{
  MDeformWeight *dw, *dw_cpy;
  float weight;
  int i, totweight = dvert->totweight;

  /* copy weights */
  for (dw = dvert->dw, i = 0; i < totweight; dw++, i++) {
    if (dw->def_nr < flip_map_num) {
      if (flip_map[dw->def_nr] >= 0) {
        /* error checkers complain of this but we'll never get nullptr return */
        dw_cpy = BKE_defvert_ensure_index(dvert, flip_map[dw->def_nr]);
        dw = &dvert->dw[i]; /* in case array got realloced */

        /* distribute weights: if only one of the vertex groups was
         * assigned this will halve the weights, otherwise it gets
         * evened out. this keeps it proportional to other groups */
        weight = 0.5f * (dw_cpy->weight + dw->weight);
        dw_cpy->weight = weight;
        dw->weight = weight;
      }
    }
  }
}

bool BKE_id_supports_vertex_groups(const ID *id)
{
  if (id == nullptr) {
    return false;
  }
  return ELEM(GS(id->name), ID_ME, ID_LT, ID_GD_LEGACY, ID_GP);
}

bool BKE_object_supports_vertex_groups(const Object *ob)
{
  const ID *id = static_cast<const ID *>(ob->data);

  return BKE_id_supports_vertex_groups(id);
}

const ListBase *BKE_id_defgroup_list_get(const ID *id)
{
  switch (GS(id->name)) {
    case ID_ME: {
      const Mesh *mesh = (const Mesh *)id;
      return &mesh->vertex_group_names;
    }
    case ID_LT: {
      const Lattice *lt = (const Lattice *)id;
      return &lt->vertex_group_names;
    }
    case ID_GD_LEGACY: {
      const bGPdata *gpd = (const bGPdata *)id;
      return &gpd->vertex_group_names;
    }
    case ID_GP: {
      const GreasePencil *grease_pencil = (const GreasePencil *)id;
      return &grease_pencil->vertex_group_names;
    }
    default: {
      BLI_assert_unreachable();
    }
  }
  return nullptr;
}

static const int *object_defgroup_active_index_get_p(const Object *ob)
{
  BLI_assert(BKE_object_supports_vertex_groups(ob));
  switch (ob->type) {
    case OB_MESH: {
      const Mesh *mesh = (const Mesh *)ob->data;
      return &mesh->vertex_group_active_index;
    }
    case OB_LATTICE: {
      const Lattice *lattice = (const Lattice *)ob->data;
      return &lattice->vertex_group_active_index;
    }
    case OB_GPENCIL_LEGACY: {
      const bGPdata *gpd = (const bGPdata *)ob->data;
      return &gpd->vertex_group_active_index;
    }
    case OB_GREASE_PENCIL: {
      const GreasePencil *grease_pencil = (const GreasePencil *)ob->data;
      return &grease_pencil->vertex_group_active_index;
    }
  }
  return nullptr;
}

ListBase *BKE_id_defgroup_list_get_mutable(ID *id)
{
  /* Cast away const just for the accessor. */
  return (ListBase *)BKE_id_defgroup_list_get(id);
}

bDeformGroup *BKE_object_defgroup_find_name(const Object *ob, const StringRef name)
{
  if (name.is_empty()) {
    return nullptr;
  }
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  LISTBASE_FOREACH (bDeformGroup *, group, defbase) {
    if (name == group->name) {
      return group;
    }
  }
  return nullptr;
}

int BKE_defgroup_name_index(const ListBase *defbase, const StringRef name)
{
  int index;
  if (!BKE_defgroup_listbase_name_find(defbase, name, &index, nullptr)) {
    return -1;
  }
  return index;
}

int BKE_id_defgroup_name_index(const ID *id, const StringRef name)
{
  return BKE_defgroup_name_index(BKE_id_defgroup_list_get(id), name);
}

bool BKE_defgroup_listbase_name_find(const ListBase *defbase,
                                     const StringRef name,
                                     int *r_index,
                                     bDeformGroup **r_group)
{
  if (name.is_empty()) {
    return false;
  }
  int index;
  LISTBASE_FOREACH_INDEX (bDeformGroup *, group, defbase, index) {
    if (name == group->name) {
      if (r_index != nullptr) {
        *r_index = index;
      }
      if (r_group != nullptr) {
        *r_group = group;
      }
      return true;
    }
  }
  return false;
}

bool BKE_id_defgroup_name_find(const ID *id,
                               const StringRef name,
                               int *r_index,
                               bDeformGroup **r_group)
{
  return BKE_defgroup_listbase_name_find(BKE_id_defgroup_list_get(id), name, r_index, r_group);
}

const ListBase *BKE_object_defgroup_list(const Object *ob)
{
  BLI_assert(BKE_object_supports_vertex_groups(ob));
  return BKE_id_defgroup_list_get((const ID *)ob->data);
}

int BKE_object_defgroup_name_index(const Object *ob, const StringRef name)
{
  return BKE_id_defgroup_name_index((ID *)ob->data, name);
}

ListBase *BKE_object_defgroup_list_mutable(Object *ob)
{
  BLI_assert(BKE_object_supports_vertex_groups(ob));
  return BKE_id_defgroup_list_get_mutable((ID *)ob->data);
}

int BKE_object_defgroup_count(const Object *ob)
{
  return BLI_listbase_count(BKE_object_defgroup_list(ob));
}

int BKE_object_defgroup_active_index_get(const Object *ob)
{
  return *object_defgroup_active_index_get_p(ob);
}

void BKE_object_defgroup_active_index_set(Object *ob, const int new_index)
{
  /* Cast away const just for the accessor. */
  int *index = (int *)object_defgroup_active_index_get_p(ob);
  *index = new_index;
}

static int *object_defgroup_unlocked_flip_map_ex(const Object *ob,
                                                 const bool use_default,
                                                 const bool use_only_unlocked,
                                                 int *r_flip_map_num)
{
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  const int defbase_num = BLI_listbase_count(defbase);
  *r_flip_map_num = defbase_num;

  if (defbase_num == 0) {
    return nullptr;
  }

  bDeformGroup *dg;
  char name_flip[sizeof(dg->name)];
  int i, flip_num;
  int *map = MEM_malloc_arrayN<int>(size_t(defbase_num), __func__);

  for (i = 0; i < defbase_num; i++) {
    map[i] = -1;
  }

  for (dg = static_cast<bDeformGroup *>(defbase->first), i = 0; dg; dg = dg->next, i++) {
    if (map[i] == -1) { /* may be calculated previously */

      /* in case no valid value is found, use this */
      if (use_default) {
        map[i] = i;
      }

      if (use_only_unlocked && (dg->flag & DG_LOCK_WEIGHT)) {
        continue;
      }

      BLI_string_flip_side_name(name_flip, dg->name, false, sizeof(name_flip));

      if (!STREQ(name_flip, dg->name)) {
        flip_num = BKE_object_defgroup_name_index(ob, name_flip);
        if (flip_num != -1) {
          map[i] = flip_num;
          map[flip_num] = i; /* save an extra lookup */
        }
      }
    }
  }
  return map;
}

int *BKE_object_defgroup_flip_map(const Object *ob, const bool use_default, int *r_flip_map_num)
{
  return object_defgroup_unlocked_flip_map_ex(ob, use_default, false, r_flip_map_num);
}

int *BKE_object_defgroup_flip_map_unlocked(const Object *ob,
                                           const bool use_default,
                                           int *r_flip_map_num)
{
  return object_defgroup_unlocked_flip_map_ex(ob, use_default, true, r_flip_map_num);
}

int *BKE_object_defgroup_flip_map_single(const Object *ob,
                                         const bool use_default,
                                         const int defgroup,
                                         int *r_flip_map_num)
{
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  const int defbase_num = BLI_listbase_count(defbase);
  *r_flip_map_num = defbase_num;

  if (defbase_num == 0) {
    return nullptr;
  }

  char name_flip[sizeof(bDeformGroup::name)];
  int i, flip_num, *map = MEM_malloc_arrayN<int>(size_t(defbase_num), __func__);

  for (i = 0; i < defbase_num; i++) {
    map[i] = use_default ? i : -1;
  }

  bDeformGroup *dg = static_cast<bDeformGroup *>(BLI_findlink(defbase, defgroup));

  BLI_string_flip_side_name(name_flip, dg->name, false, sizeof(name_flip));
  if (!STREQ(name_flip, dg->name)) {
    flip_num = BKE_object_defgroup_name_index(ob, name_flip);

    if (flip_num != -1) {
      map[defgroup] = flip_num;
      map[flip_num] = defgroup;
    }
  }

  return map;
}

int BKE_object_defgroup_flip_index(const Object *ob, int index, const bool use_default)
{
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  bDeformGroup *dg = static_cast<bDeformGroup *>(BLI_findlink(defbase, index));
  int flip_index = -1;

  if (dg) {
    char name_flip[sizeof(dg->name)];
    BLI_string_flip_side_name(name_flip, dg->name, false, sizeof(name_flip));

    if (!STREQ(name_flip, dg->name)) {
      flip_index = BKE_object_defgroup_name_index(ob, name_flip);
    }
  }

  return (flip_index == -1 && use_default) ? index : flip_index;
}

struct DeformGroupUniqueNameData {
  Object *ob;
  bDeformGroup *dg;
};

static bool defgroup_find_name_dupe(const StringRef name, bDeformGroup *dg, Object *ob)
{
  const ListBase *defbase = BKE_object_defgroup_list(ob);

  LISTBASE_FOREACH (bDeformGroup *, curdef, defbase) {
    if (dg != curdef) {
      if (curdef->name == name) {
        return true;
      }
    }
  }

  return false;
}

void BKE_object_defgroup_unique_name(bDeformGroup *dg, Object *ob)
{
  BLI_uniquename_cb(
      [&](const blender::StringRef name) { return defgroup_find_name_dupe(name, dg, ob); },
      DATA_("Group"),
      '.',
      dg->name,
      sizeof(dg->name));
}

void BKE_object_defgroup_set_name(bDeformGroup *dg, Object *ob, const char *new_name)
{
  std::string old_name = dg->name;
  STRNCPY_UTF8(dg->name, new_name);
  BKE_object_defgroup_unique_name(dg, ob);

  if (ob->type == OB_GREASE_PENCIL) {
    /* Update vgroup names stored in CurvesGeometry */
    BKE_grease_pencil_vgroup_name_update(ob, old_name.c_str(), dg->name);
  }
}

float BKE_defvert_find_weight(const MDeformVert *dvert, const int defgroup)
{
  MDeformWeight *dw = BKE_defvert_find_index(dvert, defgroup);
  return dw ? dw->weight : 0.0f;
}

float BKE_defvert_array_find_weight_safe(const MDeformVert *dvert,
                                         const int index,
                                         const int defgroup,
                                         const bool invert)
{
  /* Invalid defgroup index means the vgroup selected is invalid,
   * does not exist, in that case it is OK to return 1.0
   * (i.e. maximum weight, as if no vgroup was selected).
   * But in case of valid defgroup and nullptr dvert data pointer, it means that vgroup **is**
   * valid, and just totally empty, so we shall return '0.0' (or '1.0' if inverted) value then! */
  if (defgroup == -1) {
    return 1.0f;
  }
  if (dvert == nullptr) {
    return invert ? 1.0 : 0.0f;
  }

  float weight = BKE_defvert_find_weight(dvert + index, defgroup);

  if (invert) {
    weight = 1.0f - weight;
  }

  return weight;
}

MDeformWeight *BKE_defvert_find_index(const MDeformVert *dvert, const int defgroup)
{
  if (dvert && defgroup >= 0) {
    MDeformWeight *dw = dvert->dw;
    uint i;

    for (i = dvert->totweight; i != 0; i--, dw++) {
      if (dw->def_nr == defgroup) {
        return dw;
      }
    }
  }
  else {
    BLI_assert(0);
  }

  return nullptr;
}

MDeformWeight *BKE_defvert_ensure_index(MDeformVert *dvert, const int defgroup)
{
  MDeformWeight *dw_new;

  /* do this check always, this function is used to check for it */
  if (!dvert || defgroup < 0) {
    BLI_assert(0);
    return nullptr;
  }

  dw_new = BKE_defvert_find_index(dvert, defgroup);
  if (dw_new) {
    return dw_new;
  }

  dw_new = MEM_malloc_arrayN<MDeformWeight>(size_t(dvert->totweight + 1), __func__);
  if (dvert->dw) {
    memcpy(dw_new, dvert->dw, sizeof(MDeformWeight) * dvert->totweight);
    MEM_freeN(dvert->dw);
  }
  dvert->dw = dw_new;
  dw_new += dvert->totweight;
  dw_new->weight = 0.0f;
  dw_new->def_nr = defgroup;
  /* Group index */

  dvert->totweight++;

  return dw_new;
}

void BKE_defvert_add_index_notest(MDeformVert *dvert, const int defgroup, const float weight)
{
  /* TODO: merge with #BKE_defvert_ensure_index! */

  MDeformWeight *dw_new;

  /* do this check always, this function is used to check for it */
  if (!dvert || defgroup < 0) {
    BLI_assert(0);
    return;
  }

  dw_new = MEM_calloc_arrayN<MDeformWeight>(size_t(dvert->totweight + 1), __func__);
  if (dvert->dw) {
    memcpy(dw_new, dvert->dw, sizeof(MDeformWeight) * dvert->totweight);
    MEM_freeN(dvert->dw);
  }
  dvert->dw = dw_new;
  dw_new += dvert->totweight;
  dw_new->weight = weight;
  dw_new->def_nr = defgroup;
  dvert->totweight++;
}

void BKE_defvert_remove_group(MDeformVert *dvert, MDeformWeight *dw)
{
  if (UNLIKELY(!dvert || !dw)) {
    return;
  }
  /* Ensure `dw` is part of `dvert` (security check). */
  if (UNLIKELY(uintptr_t(dw - dvert->dw) >= uintptr_t(dvert->totweight))) {
    /* Assert as an invalid `dw` (while supported) isn't likely to do what the caller expected. */
    BLI_assert_unreachable();
    return;
  }

  const int i = dw - dvert->dw;
  dvert->totweight--;
  /* If there are still other deform weights attached to this vert then remove
   * this deform weight, and reshuffle the others. */
  if (dvert->totweight) {
    BLI_assert(dvert->dw != nullptr);

    if (i != dvert->totweight) {
      dvert->dw[i] = dvert->dw[dvert->totweight];
    }

    dvert->dw = static_cast<MDeformWeight *>(
        MEM_reallocN(dvert->dw, sizeof(MDeformWeight) * dvert->totweight));
  }
  else {
    /* If there are no other deform weights left then just remove this one. */
    MEM_freeN(dvert->dw);
    dvert->dw = nullptr;
  }
}

void BKE_defvert_clear(MDeformVert *dvert)
{
  MEM_SAFE_FREE(dvert->dw);

  dvert->totweight = 0;
}

int BKE_defvert_find_shared(const MDeformVert *dvert_a, const MDeformVert *dvert_b)
{
  if (dvert_a->totweight && dvert_b->totweight) {
    MDeformWeight *dw = dvert_a->dw;
    uint i;

    for (i = dvert_a->totweight; i != 0; i--, dw++) {
      if (dw->weight > 0.0f && BKE_defvert_find_weight(dvert_b, dw->def_nr) > 0.0f) {
        return dw->def_nr;
      }
    }
  }

  return -1;
}

bool BKE_defvert_is_weight_zero(const MDeformVert *dvert, const int defgroup_tot)
{
  MDeformWeight *dw = dvert->dw;
  for (int i = dvert->totweight; i != 0; i--, dw++) {
    if (dw->weight != 0.0f) {
      /* check the group is in-range, happens on rare situations */
      if (LIKELY(dw->def_nr < defgroup_tot)) {
        return false;
      }
    }
  }
  return true;
}

float BKE_defvert_total_selected_weight(const MDeformVert *dv,
                                        int defbase_num,
                                        const bool *defbase_sel)
{
  float total = 0.0f;
  const MDeformWeight *dw = dv->dw;

  if (defbase_sel == nullptr) {
    return total;
  }

  for (int i = dv->totweight; i != 0; i--, dw++) {
    if (dw->def_nr < defbase_num) {
      if (defbase_sel[dw->def_nr]) {
        total += dw->weight;
      }
    }
  }

  return total;
}

float BKE_defvert_multipaint_collective_weight(const MDeformVert *dv,
                                               const int defbase_num,
                                               const bool *defbase_sel,
                                               const int defbase_sel_num,
                                               const bool is_normalized)
{
  float total = BKE_defvert_total_selected_weight(dv, defbase_num, defbase_sel);

  /* in multipaint, get the average if auto normalize is inactive
   * get the sum if it is active */
  if (!is_normalized) {
    total /= defbase_sel_num;
  }

  return total;
}

float BKE_defvert_calc_lock_relative_weight(float weight,
                                            float locked_weight,
                                            float unlocked_weight)
{
  /* First try normalizing unlocked weights. */
  if (unlocked_weight > 0.0f) {
    return weight / unlocked_weight;
  }

  /* If no unlocked weight exists, take locked into account. */
  if (locked_weight <= 0.0f) {
    return weight;
  }

  /* handle division by zero */
  if (locked_weight >= 1.0f - VERTEX_WEIGHT_LOCK_EPSILON) {
    if (weight != 0.0f) {
      return 1.0f;
    }

    /* resolve 0/0 to 0 */
    return 0.0f;
  }

  /* non-degenerate division */
  return weight / (1.0f - locked_weight);
}

float BKE_defvert_lock_relative_weight(const float weight,
                                       const MDeformVert *dv,
                                       const int defbase_num,
                                       const bool *defbase_locked,
                                       const bool *defbase_unlocked)
{
  float unlocked = BKE_defvert_total_selected_weight(dv, defbase_num, defbase_unlocked);

  if (unlocked > 0.0f) {
    return weight / unlocked;
  }

  float locked = BKE_defvert_total_selected_weight(dv, defbase_num, defbase_locked);

  return BKE_defvert_calc_lock_relative_weight(weight, locked, unlocked);
}

/* -------------------------------------------------------------------- */
/** \name Defvert Array functions
 * \{ */

void BKE_defvert_array_copy(MDeformVert *dst, const MDeformVert *src, int totvert)
{
  /* Assumes dst is already set up */

  if (!src || !dst) {
    return;
  }

  memcpy(dst, src, totvert * sizeof(MDeformVert));

  for (int i = 0; i < totvert; i++) {
    if (src[i].dw) {
      dst[i].dw = MEM_malloc_arrayN<MDeformWeight>(size_t(src[i].totweight), __func__);
      memcpy(dst[i].dw, src[i].dw, sizeof(MDeformWeight) * src[i].totweight);
    }
  }
}

void BKE_defvert_array_free_elems(MDeformVert *dvert, int totvert)
{
  /* Instead of freeing the verts directly,
   * call this function to delete any special
   * vert data */

  if (!dvert) {
    return;
  }

  /* Free any special data from the verts */
  for (int i = 0; i < totvert; i++) {
    if (dvert[i].dw) {
      MEM_freeN(dvert[i].dw);
    }
  }
}

void BKE_defvert_array_free(MDeformVert *dvert, int totvert)
{
  /* Instead of freeing the verts directly,
   * call this function to delete any special
   * vert data */
  if (!dvert) {
    return;
  }

  /* Free any special data from the verts */
  BKE_defvert_array_free_elems(dvert, totvert);

  MEM_freeN(dvert);
}

void BKE_defvert_extract_vgroup_to_vertweights(const MDeformVert *dvert,
                                               const int defgroup,
                                               const int verts_num,
                                               const bool invert_vgroup,
                                               float *r_weights)
{
  if (dvert && defgroup != -1) {
    int i = verts_num;

    while (i--) {
      const float w = BKE_defvert_find_weight(&dvert[i], defgroup);
      r_weights[i] = invert_vgroup ? (1.0f - w) : w;
    }
  }
  else {
    copy_vn_fl(r_weights, verts_num, invert_vgroup ? 1.0f : 0.0f);
  }
}

void BKE_defvert_extract_vgroup_to_edgeweights(const MDeformVert *dvert,
                                               const int defgroup,
                                               const int verts_num,
                                               blender::Span<blender::int2> edges,
                                               const bool invert_vgroup,
                                               float *r_weights)
{
  if (UNLIKELY(!dvert || defgroup == -1)) {
    copy_vn_fl(r_weights, edges.size(), 0.0f);
    return;
  }

  int i = edges.size();
  float *tmp_weights = MEM_malloc_arrayN<float>(size_t(verts_num), __func__);

  BKE_defvert_extract_vgroup_to_vertweights(
      dvert, defgroup, verts_num, invert_vgroup, tmp_weights);

  while (i--) {
    const blender::int2 &edge = edges[i];

    r_weights[i] = (tmp_weights[edge[0]] + tmp_weights[edge[1]]) * 0.5f;
  }

  MEM_freeN(tmp_weights);
}

void BKE_defvert_extract_vgroup_to_loopweights(const MDeformVert *dvert,
                                               const int defgroup,
                                               const int verts_num,
                                               const Span<int> corner_verts,
                                               const bool invert_vgroup,
                                               float *r_weights)
{
  if (UNLIKELY(!dvert || defgroup == -1)) {
    copy_vn_fl(r_weights, corner_verts.size(), 0.0f);
    return;
  }

  int i = corner_verts.size();
  float *tmp_weights = MEM_malloc_arrayN<float>(size_t(verts_num), __func__);

  BKE_defvert_extract_vgroup_to_vertweights(
      dvert, defgroup, verts_num, invert_vgroup, tmp_weights);

  while (i--) {
    r_weights[i] = tmp_weights[corner_verts[i]];
  }

  MEM_freeN(tmp_weights);
}

void BKE_defvert_extract_vgroup_to_faceweights(const MDeformVert *dvert,
                                               const int defgroup,
                                               const int verts_num,
                                               const Span<int> corner_verts,
                                               const blender::OffsetIndices<int> faces,
                                               const bool invert_vgroup,
                                               float *r_weights)
{
  if (UNLIKELY(!dvert || defgroup == -1)) {
    copy_vn_fl(r_weights, faces.size(), 0.0f);
    return;
  }

  int i = faces.size();
  float *tmp_weights = MEM_malloc_arrayN<float>(size_t(verts_num), __func__);

  BKE_defvert_extract_vgroup_to_vertweights(
      dvert, defgroup, verts_num, invert_vgroup, tmp_weights);

  while (i--) {
    const blender::IndexRange face = faces[i];
    const int *corner_vert = &corner_verts[face.start()];
    int j = face.size();
    float w = 0.0f;

    for (; j--; corner_vert++) {
      w += tmp_weights[*corner_vert];
    }
    r_weights[i] = w / float(face.size());
  }

  MEM_freeN(tmp_weights);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data Transfer
 * \{ */

static void vgroups_datatransfer_interp(const CustomDataTransferLayerMap *laymap,
                                        void *dest,
                                        const void **sources,
                                        const float *weights,
                                        const int count,
                                        const float mix_factor)
{
  MDeformVert **data_src = (MDeformVert **)sources;
  MDeformVert *data_dst = (MDeformVert *)dest;
  const int idx_src = laymap->data_src_n;
  const int idx_dst = laymap->data_dst_n;

  const int mix_mode = laymap->mix_mode;

  int i, j;

  MDeformWeight *dw_dst = BKE_defvert_find_index(data_dst, idx_dst);
  float weight_src = 0.0f, weight_dst = 0.0f;

  bool has_dw_sources = false;
  if (sources) {
    for (i = count; i--;) {
      for (j = data_src[i]->totweight; j--;) {
        const MDeformWeight *dw_src = &data_src[i]->dw[j];
        if (dw_src->def_nr == idx_src) {
          weight_src += dw_src->weight * weights[i];
          has_dw_sources = true;
          break;
        }
      }
    }
  }

  if (dw_dst) {
    weight_dst = dw_dst->weight;
  }
  else if (mix_mode == CDT_MIX_REPLACE_ABOVE_THRESHOLD) {
    return; /* Do not affect destination. */
  }

  weight_src = data_transfer_interp_float_do(mix_mode, weight_dst, weight_src, mix_factor);

  CLAMP(weight_src, 0.0f, 1.0f);

  /* Do not create a destination MDeformWeight data if we had no sources at all. */
  if (!has_dw_sources) {
    BLI_assert(weight_src == 0.0f);
    if (dw_dst) {
      dw_dst->weight = weight_src;
    }
  }
  else if (!dw_dst) {
    BKE_defvert_add_index_notest(data_dst, idx_dst, weight_src);
  }
  else {
    dw_dst->weight = weight_src;
  }
}

static bool data_transfer_layersmapping_vgroups_multisrc_to_dst(
    blender::Vector<CustomDataTransferLayerMap> *r_map,
    const int mix_mode,
    const float mix_factor,
    const float *mix_weights,
    const bool use_create,
    const bool use_delete,
    Object *ob_dst,
    const Mesh &mesh_src,
    Mesh &mesh_dst,
    const bool /*use_dupref_dst*/,
    const int tolayers,
    const bool *use_layers_src,
    const int num_layers_src)
{
  int idx_src;
  int idx_dst;
  const ListBase *src_list = &mesh_src.vertex_group_names;
  ListBase *dst_defbase = &mesh_dst.vertex_group_names;

  const int tot_dst = BLI_listbase_count(dst_defbase);

  const size_t elem_size = sizeof(MDeformVert);

  switch (tolayers) {
    case DT_LAYERS_INDEX_DST:
      idx_dst = tot_dst;

      /* Find last source actually used! */
      idx_src = num_layers_src;
      while (idx_src-- && !use_layers_src[idx_src]) {
        /* pass */
      }
      idx_src++;

      if (idx_dst < idx_src) {
        if (use_create) {
          /* Create as much vgroups as necessary! */
          for (; idx_dst < idx_src; idx_dst++) {
            BKE_object_defgroup_add(ob_dst);
          }
        }
        else {
          /* Otherwise, just try to map what we can with existing dst vgroups. */
          idx_src = idx_dst;
        }
      }
      else if (use_delete && idx_dst > idx_src) {
        while (idx_dst-- > idx_src) {
          BKE_object_defgroup_remove(ob_dst, static_cast<bDeformGroup *>(dst_defbase->last));
        }
      }
      if (r_map) {
        while (idx_src--) {
          if (!use_layers_src[idx_src]) {
            continue;
          }
          data_transfer_layersmapping_add_item(r_map,
                                               CD_FAKE_MDEFORMVERT,
                                               mix_mode,
                                               mix_factor,
                                               mix_weights,
                                               mesh_src.deform_verts().data(),
                                               mesh_dst.deform_verts_for_write().data(),
                                               idx_src,
                                               idx_src,
                                               elem_size,
                                               0,
                                               0,
                                               vgroups_datatransfer_interp,
                                               nullptr);
        }
      }
      break;
    case DT_LAYERS_NAME_DST: {
      bDeformGroup *dg_src, *dg_dst;

      if (use_delete) {
        /* Remove all unused dst vgroups first, simpler in this case. */
        for (dg_dst = static_cast<bDeformGroup *>(dst_defbase->first); dg_dst;) {
          bDeformGroup *dg_dst_next = dg_dst->next;

          if (BKE_id_defgroup_name_index(&mesh_src.id, dg_dst->name) == -1) {
            BKE_object_defgroup_remove(ob_dst, dg_dst);
          }
          dg_dst = dg_dst_next;
        }
      }

      for (idx_src = 0, dg_src = static_cast<bDeformGroup *>(src_list->first);
           idx_src < num_layers_src;
           idx_src++, dg_src = dg_src->next)
      {
        if (!use_layers_src[idx_src]) {
          continue;
        }

        idx_dst = BKE_object_defgroup_name_index(ob_dst, dg_src->name);
        if (idx_dst == -1) {
          if (use_create) {
            BKE_object_defgroup_add_name(ob_dst, dg_src->name);
            idx_dst = BKE_object_defgroup_active_index_get(ob_dst) - 1;
          }
          else {
            /* If we are not allowed to create missing dst vgroups, just skip matching src one. */
            continue;
          }
        }
        if (r_map) {
          data_transfer_layersmapping_add_item(r_map,
                                               CD_FAKE_MDEFORMVERT,
                                               mix_mode,
                                               mix_factor,
                                               mix_weights,
                                               mesh_src.deform_verts().data(),
                                               mesh_dst.deform_verts_for_write().data(),
                                               idx_src,
                                               idx_dst,
                                               elem_size,
                                               0,
                                               0,
                                               vgroups_datatransfer_interp,
                                               nullptr);
        }
      }
      break;
    }
    default:
      return false;
  }

  return true;
}

bool data_transfer_layersmapping_vgroups(blender::Vector<CustomDataTransferLayerMap> *r_map,
                                         const int mix_mode,
                                         const float mix_factor,
                                         const float *mix_weights,
                                         const bool use_create,
                                         const bool use_delete,
                                         Object *ob_src,
                                         Object *ob_dst,
                                         const Mesh &mesh_src,
                                         Mesh &mesh_dst,
                                         const bool use_dupref_dst,
                                         const int fromlayers,
                                         const int tolayers)
{
  int idx_src, idx_dst;

  const size_t elem_size = sizeof(MDeformVert);

  /* NOTE:
   * We may have to handle data layout itself while having nullptr data itself,
   * and even have to support nullptr data_src in transfer data code
   * (we always create a data_dst, though).
   */

  const ListBase *src_defbase = BKE_object_defgroup_list(ob_src);
  if (BLI_listbase_is_empty(src_defbase)) {
    if (use_delete) {
      BKE_object_defgroup_remove_all(ob_dst);
    }
    return true;
  }

  if (fromlayers == DT_LAYERS_ACTIVE_SRC || fromlayers >= 0) {
    /* NOTE: use_delete has not much meaning in this case, ignored. */

    if (fromlayers >= 0) {
      idx_src = fromlayers;
      if (idx_src >= BLI_listbase_count(src_defbase)) {
        /* This can happen when vgroups are removed from source object...
         * Remapping would be really tricky here, we'd need to go over all objects in
         * Main every time we delete a vgroup... for now, simpler and safer to abort. */
        return false;
      }
    }
    else if ((idx_src = BKE_object_defgroup_active_index_get(ob_src) - 1) == -1) {
      return false;
    }

    if (tolayers >= 0) {
      /* NOTE: in this case we assume layer exists! */
      idx_dst = tolayers;
      const ListBase *dst_defbase = BKE_object_defgroup_list(ob_dst);
      BLI_assert(idx_dst < BLI_listbase_count(dst_defbase));
      UNUSED_VARS_NDEBUG(dst_defbase);
    }
    else if (tolayers == DT_LAYERS_ACTIVE_DST) {
      idx_dst = BKE_object_defgroup_active_index_get(ob_dst) - 1;
      if (idx_dst == -1) {
        bDeformGroup *dg_src;
        if (!use_create) {
          return true;
        }
        dg_src = static_cast<bDeformGroup *>(BLI_findlink(src_defbase, idx_src));
        BKE_object_defgroup_add_name(ob_dst, dg_src->name);
        idx_dst = BKE_object_defgroup_active_index_get(ob_dst) - 1;
      }
    }
    else if (tolayers == DT_LAYERS_INDEX_DST) {
      int num = BLI_listbase_count(src_defbase);
      idx_dst = idx_src;
      if (num <= idx_dst) {
        if (!use_create) {
          return true;
        }
        /* Create as much vgroups as necessary! */
        for (; num <= idx_dst; num++) {
          BKE_object_defgroup_add(ob_dst);
        }
      }
    }
    else if (tolayers == DT_LAYERS_NAME_DST) {
      bDeformGroup *dg_src = static_cast<bDeformGroup *>(BLI_findlink(src_defbase, idx_src));
      idx_dst = BKE_object_defgroup_name_index(ob_dst, dg_src->name);
      if (idx_dst == -1) {
        if (!use_create) {
          return true;
        }
        BKE_object_defgroup_add_name(ob_dst, dg_src->name);
        idx_dst = BKE_object_defgroup_active_index_get(ob_dst) - 1;
      }
    }
    else {
      return false;
    }

    if (r_map) {
      data_transfer_layersmapping_add_item(r_map,
                                           CD_FAKE_MDEFORMVERT,
                                           mix_mode,
                                           mix_factor,
                                           mix_weights,
                                           mesh_src.deform_verts().data(),
                                           mesh_dst.deform_verts_for_write().data(),
                                           idx_src,
                                           idx_dst,
                                           elem_size,
                                           0,
                                           0,
                                           vgroups_datatransfer_interp,
                                           nullptr);
    }
  }
  else {
    int num_src, num_sel_unused;
    bool *use_layers_src = nullptr;
    bool ret = false;

    switch (fromlayers) {
      case DT_LAYERS_ALL_SRC:
        use_layers_src = BKE_object_defgroup_subset_from_select_type(
            ob_src, WT_VGROUP_ALL, &num_src, &num_sel_unused);
        break;
      case DT_LAYERS_VGROUP_SRC_BONE_SELECT:
        use_layers_src = BKE_object_defgroup_subset_from_select_type(
            ob_src, WT_VGROUP_BONE_SELECT, &num_src, &num_sel_unused);
        break;
      case DT_LAYERS_VGROUP_SRC_BONE_DEFORM:
        use_layers_src = BKE_object_defgroup_subset_from_select_type(
            ob_src, WT_VGROUP_BONE_DEFORM, &num_src, &num_sel_unused);
        break;
    }

    if (use_layers_src) {
      ret = data_transfer_layersmapping_vgroups_multisrc_to_dst(r_map,
                                                                mix_mode,
                                                                mix_factor,
                                                                mix_weights,
                                                                use_create,
                                                                use_delete,
                                                                ob_dst,
                                                                mesh_src,
                                                                mesh_dst,
                                                                use_dupref_dst,
                                                                tolayers,
                                                                use_layers_src,
                                                                num_src);
    }

    MEM_SAFE_FREE(use_layers_src);
    return ret;
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Various utils & helpers.
 * \{ */

void BKE_defvert_weight_to_rgb(float r_rgb[3], const float weight)
{
  const float blend = ((weight / 2.0f) + 0.5f);

  if (weight <= 0.25f) { /* blue->cyan */
    r_rgb[0] = 0.0f;
    r_rgb[1] = blend * weight * 4.0f;
    r_rgb[2] = blend;
  }
  else if (weight <= 0.50f) { /* cyan->green */
    r_rgb[0] = 0.0f;
    r_rgb[1] = blend;
    r_rgb[2] = blend * (1.0f - ((weight - 0.25f) * 4.0f));
  }
  else if (weight <= 0.75f) { /* green->yellow */
    r_rgb[0] = blend * ((weight - 0.50f) * 4.0f);
    r_rgb[1] = blend;
    r_rgb[2] = 0.0f;
  }
  else if (weight <= 1.0f) { /* yellow->red */
    r_rgb[0] = blend;
    r_rgb[1] = blend * (1.0f - ((weight - 0.75f) * 4.0f));
    r_rgb[2] = 0.0f;
  }
  else {
    /* exceptional value, unclamped or nan,
     * avoid uninitialized memory use */
    r_rgb[0] = 1.0f;
    r_rgb[1] = 0.0f;
    r_rgb[2] = 1.0f;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name .blend file I/O
 * \{ */

void BKE_defbase_blend_write(BlendWriter *writer, const ListBase *defbase)
{
  LISTBASE_FOREACH (bDeformGroup *, defgroup, defbase) {
    BLO_write_struct(writer, bDeformGroup, defgroup);
  }
}

void BKE_defvert_blend_write(BlendWriter *writer, int count, const MDeformVert *dvlist)
{
  if (dvlist == nullptr) {
    return;
  }

  /* Write the dvert list */
  BLO_write_struct_array(writer, MDeformVert, count, dvlist);

  /* Write deformation data for each dvert */
  for (int i = 0; i < count; i++) {
    if (dvlist[i].dw) {
      BLO_write_struct_array(writer, MDeformWeight, dvlist[i].totweight, dvlist[i].dw);
    }
  }
}

void BKE_defvert_blend_read(BlendDataReader *reader, int count, MDeformVert *mdverts)
{
  if (mdverts == nullptr) {
    return;
  }

  for (int i = count; i > 0; i--, mdverts++) {
    /* Convert to vertex group allocation system. */
    MDeformWeight *dw = mdverts->dw;
    BLO_read_struct_array(reader, MDeformWeight, mdverts->totweight, &dw);
    if (dw) {
      void *dw_tmp = MEM_malloc_arrayN<MDeformWeight>(size_t(mdverts->totweight), __func__);
      const size_t dw_len = sizeof(MDeformWeight) * mdverts->totweight;
      memcpy(dw_tmp, dw, dw_len);
      mdverts->dw = static_cast<MDeformWeight *>(dw_tmp);
      MEM_freeN(dw);
    }
    else {
      mdverts->dw = nullptr;
      mdverts->totweight = 0;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Virtual array implementation for vertex groups.
 * \{ */

namespace blender::bke {

class VArrayImpl_For_VertexWeights final : public VMutableArrayImpl<float> {
 private:
  MDeformVert *dverts_;
  const int dvert_index_;

 public:
  VArrayImpl_For_VertexWeights(MutableSpan<MDeformVert> dverts, const int dvert_index)
      : VMutableArrayImpl<float>(dverts.size()), dverts_(dverts.data()), dvert_index_(dvert_index)
  {
  }

  VArrayImpl_For_VertexWeights(Span<MDeformVert> dverts, const int dvert_index)
      : VMutableArrayImpl<float>(dverts.size()),
        dverts_(const_cast<MDeformVert *>(dverts.data())),
        dvert_index_(dvert_index)
  {
  }

  float get(const int64_t index) const override
  {
    if (dverts_ == nullptr) {
      return 0.0f;
    }
    if (const MDeformWeight *weight = this->find_weight_at_index(index)) {
      return weight->weight;
    }
    return 0.0f;
  }

  void set(const int64_t index, const float value) override
  {
    MDeformVert &dvert = dverts_[index];
    if (value == 0.0f) {
      if (MDeformWeight *weight = this->find_weight_at_index(index)) {
        weight->weight = 0.0f;
      }
    }
    else {
      MDeformWeight *weight = BKE_defvert_ensure_index(&dvert, dvert_index_);
      weight->weight = value;
    }
  }

  void set_all(Span<float> src) override
  {
    threading::parallel_for(src.index_range(), 4096, [&](const IndexRange range) {
      for (const int64_t i : range) {
        this->set(i, src[i]);
      }
    });
  }

  void materialize(const IndexMask &mask,
                   float *dst,
                   const bool /*dst_is_uninitialized*/) const override
  {
    if (dverts_ == nullptr) {
      mask.foreach_index([&](const int i) { dst[i] = 0.0f; });
    }
    threading::parallel_for(mask.index_range(), 4096, [&](const IndexRange range) {
      mask.slice(range).foreach_index_optimized<int64_t>([&](const int64_t index) {
        if (const MDeformWeight *weight = this->find_weight_at_index(index)) {
          dst[index] = weight->weight;
        }
        else {
          dst[index] = 0.0f;
        }
      });
    });
  }

 private:
  MDeformWeight *find_weight_at_index(const int64_t index)
  {
    for (MDeformWeight &weight : MutableSpan(dverts_[index].dw, dverts_[index].totweight)) {
      if (weight.def_nr == dvert_index_) {
        return &weight;
      }
    }
    return nullptr;
  }
  const MDeformWeight *find_weight_at_index(const int64_t index) const
  {
    for (const MDeformWeight &weight : Span(dverts_[index].dw, dverts_[index].totweight)) {
      if (weight.def_nr == dvert_index_) {
        return &weight;
      }
    }
    return nullptr;
  }
};

VArray<float> varray_for_deform_verts(Span<MDeformVert> dverts, const int defgroup_index)
{
  return VArray<float>::from<VArrayImpl_For_VertexWeights>(dverts, defgroup_index);
}
VMutableArray<float> varray_for_mutable_deform_verts(MutableSpan<MDeformVert> dverts,
                                                     const int defgroup_index)
{
  return VMutableArray<float>::from<VArrayImpl_For_VertexWeights>(dverts, defgroup_index);
}

void remove_defgroup_index(MutableSpan<MDeformVert> dverts, const int defgroup_index)
{
  threading::parallel_for(dverts.index_range(), 1024, [&](IndexRange range) {
    for (MDeformVert &dvert : dverts.slice(range)) {
      MDeformWeight *weight = BKE_defvert_find_index(&dvert, defgroup_index);
      BKE_defvert_remove_group(&dvert, weight);
      for (MDeformWeight &weight : MutableSpan(dvert.dw, dvert.totweight)) {
        if (weight.def_nr > defgroup_index) {
          weight.def_nr--;
        }
      }
    }
  });
}

void gather_deform_verts(const Span<MDeformVert> src,
                         const Span<int> indices,
                         MutableSpan<MDeformVert> dst)
{
  threading::parallel_for(indices.index_range(), 512, [&](const IndexRange range) {
    for (const int dst_i : range) {
      const int src_i = indices[dst_i];
      dst[dst_i].dw = static_cast<MDeformWeight *>(MEM_dupallocN(src[src_i].dw));
      dst[dst_i].totweight = src[src_i].totweight;
      dst[dst_i].flag = src[src_i].flag;
    }
  });
}
void gather_deform_verts(const Span<MDeformVert> src,
                         const IndexMask &indices,
                         MutableSpan<MDeformVert> dst)
{
  indices.foreach_index(GrainSize(512), [&](const int64_t src_i, const int64_t dst_i) {
    dst[dst_i].dw = static_cast<MDeformWeight *>(MEM_dupallocN(src[src_i].dw));
    dst[dst_i].totweight = src[src_i].totweight;
    dst[dst_i].flag = src[src_i].flag;
  });
}

}  // namespace blender::bke

/** \} */
