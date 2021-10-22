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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLT_translation.h"

#include "DNA_defaults.h"

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_anim_visualization.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "BIK_api.h"

#include "BLO_read_write.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.armature"};

/* -------------------------------------------------------------------- */
/** \name Prototypes
 * \{ */

static void copy_bonechildren(Bone *bone_dst,
                              const Bone *bone_src,
                              const Bone *bone_src_act,
                              Bone **r_bone_dst_act,
                              const int flag);

static void copy_bonechildren_custom_handles(Bone *bone_dst, bArmature *arm_dst);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Data-block
 * \{ */

static void armature_init_data(ID *id)
{
  bArmature *armature = (bArmature *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(armature, id));

  MEMCPY_STRUCT_AFTER(armature, DNA_struct_default_get(bArmature), id);
}

/**
 * Only copy internal data of Armature ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_lib_id.h's LIB_ID_COPY_... flags for more).
 */
static void armature_copy_data(Main *UNUSED(bmain), ID *id_dst, const ID *id_src, const int flag)
{
  bArmature *armature_dst = (bArmature *)id_dst;
  const bArmature *armature_src = (const bArmature *)id_src;

  Bone *bone_src, *bone_dst;
  Bone *bone_dst_act = NULL;

  /* We never handle usercount here for own data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

  armature_dst->bonehash = NULL;

  BLI_duplicatelist(&armature_dst->bonebase, &armature_src->bonebase);

  /* Duplicate the childrens' lists */
  bone_dst = armature_dst->bonebase.first;
  for (bone_src = armature_src->bonebase.first; bone_src; bone_src = bone_src->next) {
    bone_dst->parent = NULL;
    copy_bonechildren(bone_dst, bone_src, armature_src->act_bone, &bone_dst_act, flag_subdata);
    bone_dst = bone_dst->next;
  }

  armature_dst->act_bone = bone_dst_act;

  BKE_armature_bone_hash_make(armature_dst);

  /* Fix custom handle references. */
  for (bone_dst = armature_dst->bonebase.first; bone_dst; bone_dst = bone_dst->next) {
    copy_bonechildren_custom_handles(bone_dst, armature_dst);
  }

  armature_dst->edbo = NULL;
  armature_dst->act_edbone = NULL;
}

/** Free (or release) any data used by this armature (does not free the armature itself). */
static void armature_free_data(struct ID *id)
{
  bArmature *armature = (bArmature *)id;

  BKE_armature_bone_hash_free(armature);
  BKE_armature_bonelist_free(&armature->bonebase, false);

  /* free editmode data */
  if (armature->edbo) {
    BKE_armature_editbonelist_free(armature->edbo, false);
    MEM_freeN(armature->edbo);
    armature->edbo = NULL;
  }
}

static void armature_foreach_id_bone(Bone *bone, LibraryForeachIDData *data)
{
  IDP_foreach_property(
      bone->prop, IDP_TYPE_FILTER_ID, BKE_lib_query_idpropertiesForeachIDLink_callback, data);

  LISTBASE_FOREACH (Bone *, curbone, &bone->childbase) {
    armature_foreach_id_bone(curbone, data);
  }
}

static void armature_foreach_id_editbone(EditBone *edit_bone, LibraryForeachIDData *data)
{
  IDP_foreach_property(
      edit_bone->prop, IDP_TYPE_FILTER_ID, BKE_lib_query_idpropertiesForeachIDLink_callback, data);
}

static void armature_foreach_id(ID *id, LibraryForeachIDData *data)
{
  bArmature *arm = (bArmature *)id;
  LISTBASE_FOREACH (Bone *, bone, &arm->bonebase) {
    armature_foreach_id_bone(bone, data);
  }

  if (arm->edbo != NULL) {
    LISTBASE_FOREACH (EditBone *, edit_bone, arm->edbo) {
      armature_foreach_id_editbone(edit_bone, data);
    }
  }
}

static void write_bone(BlendWriter *writer, Bone *bone)
{
  /* PATCH for upward compatibility after 2.37+ armature recode */
  bone->size[0] = bone->size[1] = bone->size[2] = 1.0f;

  /* Write this bone */
  BLO_write_struct(writer, Bone, bone);

  /* Write ID Properties -- and copy this comment EXACTLY for easy finding
   * of library blocks that implement this. */
  if (bone->prop) {
    IDP_BlendWrite(writer, bone->prop);
  }

  /* Write Children */
  LISTBASE_FOREACH (Bone *, cbone, &bone->childbase) {
    write_bone(writer, cbone);
  }
}

static void armature_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bArmature *arm = (bArmature *)id;

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  arm->bonehash = NULL;
  arm->edbo = NULL;
  /* Must always be cleared (armatures don't have their own edit-data). */
  arm->needs_flush_to_id = 0;
  arm->act_edbone = NULL;

  BLO_write_id_struct(writer, bArmature, id_address, &arm->id);
  BKE_id_blend_write(writer, &arm->id);

  if (arm->adt) {
    BKE_animdata_blend_write(writer, arm->adt);
  }

  /* Direct data */
  LISTBASE_FOREACH (Bone *, bone, &arm->bonebase) {
    write_bone(writer, bone);
  }
}

static void direct_link_bones(BlendDataReader *reader, Bone *bone)
{
  BLO_read_data_address(reader, &bone->parent);
  BLO_read_data_address(reader, &bone->prop);
  IDP_BlendDataRead(reader, &bone->prop);

  BLO_read_data_address(reader, &bone->bbone_next);
  BLO_read_data_address(reader, &bone->bbone_prev);

  bone->flag &= ~(BONE_DRAW_ACTIVE | BONE_DRAW_LOCKED_WEIGHT);

  BLO_read_list(reader, &bone->childbase);

  LISTBASE_FOREACH (Bone *, child, &bone->childbase) {
    direct_link_bones(reader, child);
  }
}

static void armature_blend_read_data(BlendDataReader *reader, ID *id)
{
  bArmature *arm = (bArmature *)id;
  BLO_read_list(reader, &arm->bonebase);
  arm->bonehash = NULL;
  arm->edbo = NULL;
  /* Must always be cleared (armatures don't have their own edit-data). */
  arm->needs_flush_to_id = 0;

  BLO_read_data_address(reader, &arm->adt);
  BKE_animdata_blend_read_data(reader, arm->adt);

  LISTBASE_FOREACH (Bone *, bone, &arm->bonebase) {
    direct_link_bones(reader, bone);
  }

  BLO_read_data_address(reader, &arm->act_bone);
  arm->act_edbone = NULL;

  BKE_armature_bone_hash_make(arm);
}

static void lib_link_bones(BlendLibReader *reader, Bone *bone)
{
  IDP_BlendReadLib(reader, bone->prop);

  LISTBASE_FOREACH (Bone *, curbone, &bone->childbase) {
    lib_link_bones(reader, curbone);
  }
}

static void armature_blend_read_lib(BlendLibReader *reader, ID *id)
{
  bArmature *arm = (bArmature *)id;
  LISTBASE_FOREACH (Bone *, curbone, &arm->bonebase) {
    lib_link_bones(reader, curbone);
  }
}

static void expand_bones(BlendExpander *expander, Bone *bone)
{
  IDP_BlendReadExpand(expander, bone->prop);

  LISTBASE_FOREACH (Bone *, curBone, &bone->childbase) {
    expand_bones(expander, curBone);
  }
}

static void armature_blend_read_expand(BlendExpander *expander, ID *id)
{
  bArmature *arm = (bArmature *)id;
  LISTBASE_FOREACH (Bone *, curBone, &arm->bonebase) {
    expand_bones(expander, curBone);
  }
}

IDTypeInfo IDType_ID_AR = {
    .id_code = ID_AR,
    .id_filter = FILTER_ID_AR,
    .main_listbase_index = INDEX_ID_AR,
    .struct_size = sizeof(bArmature),
    .name = "Armature",
    .name_plural = "armatures",
    .translation_context = BLT_I18NCONTEXT_ID_ARMATURE,
    .flags = IDTYPE_FLAGS_APPEND_IS_REUSABLE,

    .init_data = armature_init_data,
    .copy_data = armature_copy_data,
    .free_data = armature_free_data,
    .make_local = NULL,
    .foreach_id = armature_foreach_id,
    .foreach_cache = NULL,
    .owner_get = NULL,

    .blend_write = armature_blend_write,
    .blend_read_data = armature_blend_read_data,
    .blend_read_lib = armature_blend_read_lib,
    .blend_read_expand = armature_blend_read_expand,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Data-Level Functions
 * \{ */

bArmature *BKE_armature_add(Main *bmain, const char *name)
{
  bArmature *arm;

  arm = BKE_id_new(bmain, ID_AR, name);
  return arm;
}

bArmature *BKE_armature_from_object(Object *ob)
{
  if (ob->type == OB_ARMATURE) {
    return (bArmature *)ob->data;
  }
  return NULL;
}

int BKE_armature_bonelist_count(const ListBase *lb)
{
  int i = 0;
  LISTBASE_FOREACH (Bone *, bone, lb) {
    i += 1 + BKE_armature_bonelist_count(&bone->childbase);
  }

  return i;
}

void BKE_armature_bonelist_free(ListBase *lb, const bool do_id_user)
{
  Bone *bone;

  for (bone = lb->first; bone; bone = bone->next) {
    if (bone->prop) {
      IDP_FreeProperty_ex(bone->prop, do_id_user);
    }
    BKE_armature_bonelist_free(&bone->childbase, do_id_user);
  }

  BLI_freelistN(lb);
}

void BKE_armature_editbonelist_free(ListBase *lb, const bool do_id_user)
{
  LISTBASE_FOREACH_MUTABLE (EditBone *, edit_bone, lb) {
    if (edit_bone->prop) {
      IDP_FreeProperty_ex(edit_bone->prop, do_id_user);
    }
    BLI_remlink_safe(lb, edit_bone);
    MEM_freeN(edit_bone);
  }
}

static void copy_bonechildren(Bone *bone_dst,
                              const Bone *bone_src,
                              const Bone *bone_src_act,
                              Bone **r_bone_dst_act,
                              const int flag)
{
  Bone *bone_src_child, *bone_dst_child;

  if (bone_src == bone_src_act) {
    *r_bone_dst_act = bone_dst;
  }

  if (bone_src->prop) {
    bone_dst->prop = IDP_CopyProperty_ex(bone_src->prop, flag);
  }

  /* Copy this bone's list */
  BLI_duplicatelist(&bone_dst->childbase, &bone_src->childbase);

  /* For each child in the list, update its children */
  for (bone_src_child = bone_src->childbase.first, bone_dst_child = bone_dst->childbase.first;
       bone_src_child;
       bone_src_child = bone_src_child->next, bone_dst_child = bone_dst_child->next) {
    bone_dst_child->parent = bone_dst;
    copy_bonechildren(bone_dst_child, bone_src_child, bone_src_act, r_bone_dst_act, flag);
  }
}

static void copy_bonechildren_custom_handles(Bone *bone_dst, bArmature *arm_dst)
{
  Bone *bone_dst_child;

  if (bone_dst->bbone_prev) {
    bone_dst->bbone_prev = BKE_armature_find_bone_name(arm_dst, bone_dst->bbone_prev->name);
  }
  if (bone_dst->bbone_next) {
    bone_dst->bbone_next = BKE_armature_find_bone_name(arm_dst, bone_dst->bbone_next->name);
  }

  for (bone_dst_child = bone_dst->childbase.first; bone_dst_child;
       bone_dst_child = bone_dst_child->next) {
    copy_bonechildren_custom_handles(bone_dst_child, arm_dst);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Transform Copy
 * \{ */

static void copy_bone_transform(Bone *bone_dst, const Bone *bone_src)
{
  bone_dst->roll = bone_src->roll;

  copy_v3_v3(bone_dst->head, bone_src->head);
  copy_v3_v3(bone_dst->tail, bone_src->tail);

  copy_m3_m3(bone_dst->bone_mat, bone_src->bone_mat);

  copy_v3_v3(bone_dst->arm_head, bone_src->arm_head);
  copy_v3_v3(bone_dst->arm_tail, bone_src->arm_tail);

  copy_m4_m4(bone_dst->arm_mat, bone_src->arm_mat);

  bone_dst->arm_roll = bone_src->arm_roll;
}

void BKE_armature_copy_bone_transforms(bArmature *armature_dst, const bArmature *armature_src)
{
  Bone *bone_dst = armature_dst->bonebase.first;
  const Bone *bone_src = armature_src->bonebase.first;
  while (bone_dst != NULL) {
    BLI_assert(bone_src != NULL);
    copy_bone_transform(bone_dst, bone_src);
    bone_dst = bone_dst->next;
    bone_src = bone_src->next;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Transform by 4x4 Matrix
 *
 * \see #ED_armature_edit_transform for the edit-mode version of this function.
 * \{ */

/** Helper for #ED_armature_transform */
static void armature_transform_recurse(ListBase *bonebase,
                                       const float mat[4][4],
                                       const bool do_props,
                                       /* Cached from 'mat'. */
                                       const float mat3[3][3],
                                       const float scale,
                                       /* Child bones. */
                                       const Bone *bone_parent,
                                       const float arm_mat_parent_inv[4][4])
{
  LISTBASE_FOREACH (Bone *, bone, bonebase) {

    /* Store the initial bone roll in a matrix, this is needed even for child bones
     * so any change in head/tail doesn't cause the roll to change.
     *
     * Logic here is different to edit-mode because
     * this is calculated in relative to the parent. */
    float roll_mat3_pre[3][3];
    {
      float delta[3];
      sub_v3_v3v3(delta, bone->tail, bone->head);
      vec_roll_to_mat3(delta, bone->roll, roll_mat3_pre);
      if (bone->parent == NULL) {
        mul_m3_m3m3(roll_mat3_pre, mat3, roll_mat3_pre);
      }
    }
    /* Optional, use this for predictable results since the roll is re-calculated below anyway. */
    bone->roll = 0.0f;

    mul_m4_v3(mat, bone->arm_head);
    mul_m4_v3(mat, bone->arm_tail);

    /* Get the new head and tail */
    if (bone_parent) {
      sub_v3_v3v3(bone->head, bone->arm_head, bone_parent->arm_tail);
      sub_v3_v3v3(bone->tail, bone->arm_tail, bone_parent->arm_tail);

      mul_mat3_m4_v3(arm_mat_parent_inv, bone->head);
      mul_mat3_m4_v3(arm_mat_parent_inv, bone->tail);
    }
    else {
      copy_v3_v3(bone->head, bone->arm_head);
      copy_v3_v3(bone->tail, bone->arm_tail);
    }

    /* Now the head/tail have been updated, set the roll back, matching 'roll_mat3_pre'. */
    {
      float roll_mat3_post[3][3], delta_mat3[3][3];
      float delta[3];
      sub_v3_v3v3(delta, bone->tail, bone->head);
      vec_roll_to_mat3(delta, 0.0f, roll_mat3_post);
      invert_m3(roll_mat3_post);
      mul_m3_m3m3(delta_mat3, roll_mat3_post, roll_mat3_pre);
      bone->roll = atan2f(delta_mat3[2][0], delta_mat3[2][2]);
    }

    BKE_armature_where_is_bone(bone, bone_parent, false);

    {
      float arm_mat3[3][3];
      copy_m3_m4(arm_mat3, bone->arm_mat);
      mat3_to_vec_roll(arm_mat3, NULL, &bone->arm_roll);
    }

    if (do_props) {
      bone->rad_head *= scale;
      bone->rad_tail *= scale;
      bone->dist *= scale;

      /* we could be smarter and scale by the matrix along the x & z axis */
      bone->xwidth *= scale;
      bone->zwidth *= scale;
    }

    if (!BLI_listbase_is_empty(&bone->childbase)) {
      float arm_mat_inv[4][4];
      invert_m4_m4(arm_mat_inv, bone->arm_mat);
      armature_transform_recurse(&bone->childbase, mat, do_props, mat3, scale, bone, arm_mat_inv);
    }
  }
}

void BKE_armature_transform(bArmature *arm, const float mat[4][4], const bool do_props)
{
  /* Store the scale of the matrix here to use on envelopes. */
  float scale = mat4_to_scale(mat);
  float mat3[3][3];

  copy_m3_m4(mat3, mat);
  normalize_m3(mat3);

  armature_transform_recurse(&arm->bonebase, mat, do_props, mat3, scale, NULL, NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Bone Find by Name
 *
 * Using fast #GHash lookups when available.
 * \{ */

static Bone *get_named_bone_bonechildren(ListBase *lb, const char *name)
{
  Bone *curBone, *rbone;

  for (curBone = lb->first; curBone; curBone = curBone->next) {
    if (STREQ(curBone->name, name)) {
      return curBone;
    }

    rbone = get_named_bone_bonechildren(&curBone->childbase, name);
    if (rbone) {
      return rbone;
    }
  }

  return NULL;
}

/**
 * Walk the list until the bone is found (slow!),
 * use #BKE_armature_bone_from_name_map for multiple lookups.
 */
Bone *BKE_armature_find_bone_name(bArmature *arm, const char *name)
{
  if (!arm) {
    return NULL;
  }

  if (arm->bonehash) {
    return BLI_ghash_lookup(arm->bonehash, name);
  }

  return get_named_bone_bonechildren(&arm->bonebase, name);
}

static void armature_bone_from_name_insert_recursive(GHash *bone_hash, ListBase *lb)
{
  LISTBASE_FOREACH (Bone *, bone, lb) {
    BLI_ghash_insert(bone_hash, bone->name, bone);
    armature_bone_from_name_insert_recursive(bone_hash, &bone->childbase);
  }
}

/**
 * Create a (name -> bone) map.
 *
 * \note typically #bPose.chanhash us used via #BKE_pose_channel_find_name
 * this is for the cases we can't use pose channels.
 */
static GHash *armature_bone_from_name_map(bArmature *arm)
{
  const int bones_count = BKE_armature_bonelist_count(&arm->bonebase);
  GHash *bone_hash = BLI_ghash_str_new_ex(__func__, bones_count);
  armature_bone_from_name_insert_recursive(bone_hash, &arm->bonebase);
  return bone_hash;
}

void BKE_armature_bone_hash_make(bArmature *arm)
{
  if (!arm->bonehash) {
    arm->bonehash = armature_bone_from_name_map(arm);
  }
}

void BKE_armature_bone_hash_free(bArmature *arm)
{
  if (arm->bonehash) {
    BLI_ghash_free(arm->bonehash, NULL, NULL);
    arm->bonehash = NULL;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Bone Flags
 * \{ */

bool BKE_armature_bone_flag_test_recursive(const Bone *bone, int flag)
{
  if (bone->flag & flag) {
    return true;
  }
  if (bone->parent) {
    return BKE_armature_bone_flag_test_recursive(bone->parent, flag);
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Layer Refresh Used
 * \{ */

static void armature_refresh_layer_used_recursive(bArmature *arm, ListBase *bones)
{
  LISTBASE_FOREACH (Bone *, bone, bones) {
    arm->layer_used |= bone->layer;
    armature_refresh_layer_used_recursive(arm, &bone->childbase);
  }
}

void BKE_armature_refresh_layer_used(struct Depsgraph *depsgraph, struct bArmature *arm)
{
  if (arm->edbo != NULL) {
    /* Don't perform this update when the armature is in edit mode. In that case it should be
     * handled by ED_armature_edit_refresh_layer_used(). */
    return;
  }

  arm->layer_used = 0;
  armature_refresh_layer_used_recursive(arm, &arm->bonebase);

  if (depsgraph == NULL || DEG_is_active(depsgraph)) {
    bArmature *arm_orig = (bArmature *)DEG_get_original_id(&arm->id);
    arm_orig->layer_used = arm->layer_used;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Layer Refresh Used
 * \{ */

/* Finds the best possible extension to the name on a particular axis. (For renaming, check for
 * unique names afterwards) strip_number: removes number extensions  (TODO: not used)
 * axis: the axis to name on
 * head/tail: the head/tail co-ordinate of the bone on the specified axis */
bool bone_autoside_name(
    char name[MAXBONENAME], int UNUSED(strip_number), short axis, float head, float tail)
{
  unsigned int len;
  char basename[MAXBONENAME] = "";
  char extension[5] = "";

  len = strlen(name);
  if (len == 0) {
    return false;
  }
  BLI_strncpy(basename, name, sizeof(basename));

  /* Figure out extension to append:
   * - The extension to append is based upon the axis that we are working on.
   * - If head happens to be on 0, then we must consider the tail position as well to decide
   *   which side the bone is on
   *   -> If tail is 0, then its bone is considered to be on axis, so no extension should be added
   *   -> Otherwise, extension is added from perspective of object based on which side tail goes to
   * - If head is non-zero, extension is added from perspective of object based on side head is on
   */
  if (axis == 2) {
    /* z-axis - vertical (top/bottom) */
    if (IS_EQF(head, 0.0f)) {
      if (tail < 0) {
        strcpy(extension, "Bot");
      }
      else if (tail > 0) {
        strcpy(extension, "Top");
      }
    }
    else {
      if (head < 0) {
        strcpy(extension, "Bot");
      }
      else {
        strcpy(extension, "Top");
      }
    }
  }
  else if (axis == 1) {
    /* y-axis - depth (front/back) */
    if (IS_EQF(head, 0.0f)) {
      if (tail < 0) {
        strcpy(extension, "Fr");
      }
      else if (tail > 0) {
        strcpy(extension, "Bk");
      }
    }
    else {
      if (head < 0) {
        strcpy(extension, "Fr");
      }
      else {
        strcpy(extension, "Bk");
      }
    }
  }
  else {
    /* x-axis - horizontal (left/right) */
    if (IS_EQF(head, 0.0f)) {
      if (tail < 0) {
        strcpy(extension, "R");
      }
      else if (tail > 0) {
        strcpy(extension, "L");
      }
    }
    else {
      if (head < 0) {
        strcpy(extension, "R");
        /* XXX Shouldn't this be simple else, as for z and y axes? */
      }
      else if (head > 0) {
        strcpy(extension, "L");
      }
    }
  }

  /* Simple name truncation
   * - truncate if there is an extension and it wouldn't be able to fit
   * - otherwise, just append to end
   */
  if (extension[0]) {
    bool changed = true;

    while (changed) { /* remove extensions */
      changed = false;
      if (len > 2 && basename[len - 2] == '.') {
        if (ELEM(basename[len - 1], 'L', 'R')) { /* L R */
          basename[len - 2] = '\0';
          len -= 2;
          changed = true;
        }
      }
      else if (len > 3 && basename[len - 3] == '.') {
        if ((basename[len - 2] == 'F' && basename[len - 1] == 'r') || /* Fr */
            (basename[len - 2] == 'B' && basename[len - 1] == 'k'))   /* Bk */
        {
          basename[len - 3] = '\0';
          len -= 3;
          changed = true;
        }
      }
      else if (len > 4 && basename[len - 4] == '.') {
        if ((basename[len - 3] == 'T' && basename[len - 2] == 'o' &&
             basename[len - 1] == 'p') || /* Top */
            (basename[len - 3] == 'B' && basename[len - 2] == 'o' &&
             basename[len - 1] == 't')) /* Bot */
        {
          basename[len - 4] = '\0';
          len -= 4;
          changed = true;
        }
      }
    }

    if ((MAXBONENAME - len) < strlen(extension) + 1) { /* add 1 for the '.' */
      strncpy(name, basename, len - strlen(extension));
    }

    BLI_snprintf(name, MAXBONENAME, "%s.%s", basename, extension);

    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature B-Bone Support
 * \{ */

/* Compute a set of bezier parameter values that produce approximately equally spaced points. */
static void equalize_cubic_bezier(const float control[4][3],
                                  int temp_segments,
                                  int final_segments,
                                  const float *segment_scales,
                                  float *r_t_points)
{
  float(*coords)[3] = BLI_array_alloca(coords, temp_segments + 1);
  float *pdist = BLI_array_alloca(pdist, temp_segments + 1);

  /* Compute the first pass of bezier point coordinates. */
  for (int i = 0; i < 3; i++) {
    BKE_curve_forward_diff_bezier(control[0][i],
                                  control[1][i],
                                  control[2][i],
                                  control[3][i],
                                  &coords[0][i],
                                  temp_segments,
                                  sizeof(*coords));
  }

  /* Calculate the length of the polyline at each point. */
  pdist[0] = 0.0f;

  for (int i = 0; i < temp_segments; i++) {
    pdist[i + 1] = pdist[i] + len_v3v3(coords[i], coords[i + 1]);
  }

  /* Go over distances and calculate new parameter values. */
  float dist_step = pdist[temp_segments];
  float dist = 0, sum = 0;

  for (int i = 0; i < final_segments; i++) {
    sum += segment_scales[i];
  }

  dist_step /= sum;

  r_t_points[0] = 0.0f;

  for (int i = 1, nr = 1; i <= final_segments; i++) {
    dist += segment_scales[i - 1] * dist_step;

    /* We're looking for location (distance) 'dist' in the array. */
    while ((nr < temp_segments) && (dist >= pdist[nr])) {
      nr++;
    }

    float fac = (pdist[nr] - dist) / (pdist[nr] - pdist[nr - 1]);

    r_t_points[i] = (nr - fac) / temp_segments;
  }

  r_t_points[final_segments] = 1.0f;
}

/* Evaluate bezier position and tangent at a specific parameter value
 * using the De Casteljau algorithm. */
static void evaluate_cubic_bezier(const float control[4][3],
                                  float t,
                                  float r_pos[3],
                                  float r_tangent[3])
{
  float layer1[3][3];
  interp_v3_v3v3(layer1[0], control[0], control[1], t);
  interp_v3_v3v3(layer1[1], control[1], control[2], t);
  interp_v3_v3v3(layer1[2], control[2], control[3], t);

  float layer2[2][3];
  interp_v3_v3v3(layer2[0], layer1[0], layer1[1], t);
  interp_v3_v3v3(layer2[1], layer1[1], layer1[2], t);

  sub_v3_v3v3(r_tangent, layer2[1], layer2[0]);
  madd_v3_v3v3fl(r_pos, layer2[0], r_tangent, t);
}

/* Get "next" and "prev" bones - these are used for handle calculations. */
void BKE_pchan_bbone_handles_get(bPoseChannel *pchan, bPoseChannel **r_prev, bPoseChannel **r_next)
{
  if (pchan->bone->bbone_prev_type == BBONE_HANDLE_AUTO) {
    /* Use connected parent. */
    if (pchan->bone->flag & BONE_CONNECTED) {
      *r_prev = pchan->parent;
    }
    else {
      *r_prev = NULL;
    }
  }
  else {
    /* Use the provided bone as prev - leave blank to eliminate this effect altogether. */
    *r_prev = pchan->bbone_prev;
  }

  if (pchan->bone->bbone_next_type == BBONE_HANDLE_AUTO) {
    /* Use connected child. */
    *r_next = pchan->child;
  }
  else {
    /* Use the provided bone as next - leave blank to eliminate this effect altogether. */
    *r_next = pchan->bbone_next;
  }
}

/* Compute B-Bone spline parameters for the given channel. */
void BKE_pchan_bbone_spline_params_get(struct bPoseChannel *pchan,
                                       const bool rest,
                                       struct BBoneSplineParameters *param)
{
  bPoseChannel *next, *prev;
  Bone *bone = pchan->bone;
  float imat[4][4], posemat[4][4], tmpmat[4][4];
  float delta[3];

  memset(param, 0, sizeof(*param));

  param->segments = bone->segments;
  param->length = bone->length;

  if (!rest) {
    float scale[3];

    /* Check if we need to take non-uniform bone scaling into account. */
    mat4_to_size(scale, pchan->pose_mat);

    if (fabsf(scale[0] - scale[1]) > 1e-6f || fabsf(scale[1] - scale[2]) > 1e-6f) {
      param->do_scale = true;
      copy_v3_v3(param->scale, scale);
    }
  }

  BKE_pchan_bbone_handles_get(pchan, &prev, &next);

  /* Find the handle points, since this is inside bone space, the
   * first point = (0, 0, 0)
   * last point =  (0, length, 0) */
  if (rest) {
    invert_m4_m4(imat, pchan->bone->arm_mat);
  }
  else if (param->do_scale) {
    copy_m4_m4(posemat, pchan->pose_mat);
    normalize_m4(posemat);
    invert_m4_m4(imat, posemat);
  }
  else {
    invert_m4_m4(imat, pchan->pose_mat);
  }

  float prev_scale[3], next_scale[3];

  copy_v3_fl(prev_scale, 1.0f);
  copy_v3_fl(next_scale, 1.0f);

  if (prev) {
    float h1[3];
    bool done = false;

    param->use_prev = true;

    /* Transform previous point inside this bone space. */
    if (bone->bbone_prev_type == BBONE_HANDLE_RELATIVE) {
      /* Use delta movement (from restpose), and apply this relative to the current bone's head. */
      if (rest) {
        /* In restpose, arm_head == pose_head */
        zero_v3(param->prev_h);
        done = true;
      }
      else {
        sub_v3_v3v3(delta, prev->pose_head, prev->bone->arm_head);
        sub_v3_v3v3(h1, pchan->pose_head, delta);
      }
    }
    else if (bone->bbone_prev_type == BBONE_HANDLE_TANGENT) {
      /* Use bone direction by offsetting so that its tail meets current bone's head */
      if (rest) {
        sub_v3_v3v3(delta, prev->bone->arm_tail, prev->bone->arm_head);
        sub_v3_v3v3(h1, bone->arm_head, delta);
      }
      else {
        sub_v3_v3v3(delta, prev->pose_tail, prev->pose_head);
        sub_v3_v3v3(h1, pchan->pose_head, delta);
      }
    }
    else {
      /* Apply special handling for smoothly joining B-Bone chains */
      param->prev_bbone = (prev->bone->segments > 1);

      /* Use bone head as absolute position. */
      copy_v3_v3(h1, rest ? prev->bone->arm_head : prev->pose_head);
    }

    if (!done) {
      mul_v3_m4v3(param->prev_h, imat, h1);
    }

    if (!param->prev_bbone) {
      /* Find the previous roll to interpolate. */
      mul_m4_m4m4(param->prev_mat, imat, rest ? prev->bone->arm_mat : prev->pose_mat);

      /* Retrieve the local scale of the bone if necessary. */
      if ((bone->bbone_prev_flag & BBONE_HANDLE_SCALE_ANY) && !rest) {
        BKE_armature_mat_pose_to_bone(prev, prev->pose_mat, tmpmat);
        mat4_to_size(prev_scale, tmpmat);
      }
    }
  }

  if (next) {
    float h2[3];
    bool done = false;

    param->use_next = true;

    /* Transform next point inside this bone space. */
    if (bone->bbone_next_type == BBONE_HANDLE_RELATIVE) {
      /* Use delta movement (from restpose), and apply this relative to the current bone's tail. */
      if (rest) {
        /* In restpose, arm_head == pose_head */
        copy_v3_fl3(param->next_h, 0.0f, param->length, 0.0);
        done = true;
      }
      else {
        sub_v3_v3v3(delta, next->pose_head, next->bone->arm_head);
        add_v3_v3v3(h2, pchan->pose_tail, delta);
      }
    }
    else if (bone->bbone_next_type == BBONE_HANDLE_TANGENT) {
      /* Use bone direction by offsetting so that its head meets current bone's tail */
      if (rest) {
        sub_v3_v3v3(delta, next->bone->arm_tail, next->bone->arm_head);
        add_v3_v3v3(h2, bone->arm_tail, delta);
      }
      else {
        sub_v3_v3v3(delta, next->pose_tail, next->pose_head);
        add_v3_v3v3(h2, pchan->pose_tail, delta);
      }
    }
    else {
      /* Apply special handling for smoothly joining B-Bone chains */
      param->next_bbone = (next->bone->segments > 1);

      /* Use bone tail as absolute position. */
      copy_v3_v3(h2, rest ? next->bone->arm_tail : next->pose_tail);
    }

    if (!done) {
      mul_v3_m4v3(param->next_h, imat, h2);
    }

    /* Find the next roll to interpolate as well. */
    mul_m4_m4m4(param->next_mat, imat, rest ? next->bone->arm_mat : next->pose_mat);

    /* Retrieve the local scale of the bone if necessary. */
    if ((bone->bbone_next_flag & BBONE_HANDLE_SCALE_ANY) && !rest) {
      BKE_armature_mat_pose_to_bone(next, next->pose_mat, tmpmat);
      mat4_to_size(next_scale, tmpmat);
    }
  }

  /* Add effects from bbone properties over the top
   * - These properties allow users to hand-animate the
   *   bone curve/shape, without having to resort to using
   *   extra bones
   * - The "bone" level offsets are for defining the restpose
   *   shape of the bone (e.g. for curved eyebrows for example).
   *   -> In the viewport, it's needed to define what the rest pose
   *      looks like
   *   -> For "rest == 0", we also still need to have it present
   *      so that we can "cancel out" this restpose when it comes
   *      time to deform some geometry, it won't cause double transforms.
   * - The "pchan" level offsets are the ones that animators actually
   *   end up animating
   */
  {
    param->ease1 = bone->ease1 + (!rest ? pchan->ease1 : 0.0f);
    param->ease2 = bone->ease2 + (!rest ? pchan->ease2 : 0.0f);

    param->roll1 = bone->roll1 + (!rest ? pchan->roll1 : 0.0f);
    param->roll2 = bone->roll2 + (!rest ? pchan->roll2 : 0.0f);

    if (bone->bbone_flag & BBONE_ADD_PARENT_END_ROLL) {
      if (prev) {
        if (prev->bone) {
          param->roll1 += prev->bone->roll2;
        }

        if (!rest) {
          param->roll1 += prev->roll2;
        }
      }
    }

    copy_v3_v3(param->scale_in, bone->scale_in);
    copy_v3_v3(param->scale_out, bone->scale_out);

    if (!rest) {
      mul_v3_v3(param->scale_in, pchan->scale_in);
      mul_v3_v3(param->scale_out, pchan->scale_out);
    }

    /* Extra curve x / z */
    param->curve_in_x = bone->curve_in_x + (!rest ? pchan->curve_in_x : 0.0f);
    param->curve_in_z = bone->curve_in_z + (!rest ? pchan->curve_in_z : 0.0f);

    param->curve_out_x = bone->curve_out_x + (!rest ? pchan->curve_out_x : 0.0f);
    param->curve_out_z = bone->curve_out_z + (!rest ? pchan->curve_out_z : 0.0f);

    if (bone->bbone_flag & BBONE_SCALE_EASING) {
      param->ease1 *= param->scale_in[1];
      param->curve_in_x *= param->scale_in[1];
      param->curve_in_z *= param->scale_in[1];

      param->ease2 *= param->scale_out[1];
      param->curve_out_x *= param->scale_out[1];
      param->curve_out_z *= param->scale_out[1];
    }

    /* Custom handle scale. */
    if (bone->bbone_prev_flag & BBONE_HANDLE_SCALE_X) {
      param->scale_in[0] *= prev_scale[0];
    }
    if (bone->bbone_prev_flag & BBONE_HANDLE_SCALE_Y) {
      param->scale_in[1] *= prev_scale[1];
    }
    if (bone->bbone_prev_flag & BBONE_HANDLE_SCALE_Z) {
      param->scale_in[2] *= prev_scale[2];
    }
    if (bone->bbone_prev_flag & BBONE_HANDLE_SCALE_EASE) {
      param->ease1 *= prev_scale[1];
      param->curve_in_x *= prev_scale[1];
      param->curve_in_z *= prev_scale[1];
    }

    if (bone->bbone_next_flag & BBONE_HANDLE_SCALE_X) {
      param->scale_out[0] *= next_scale[0];
    }
    if (bone->bbone_next_flag & BBONE_HANDLE_SCALE_Y) {
      param->scale_out[1] *= next_scale[1];
    }
    if (bone->bbone_next_flag & BBONE_HANDLE_SCALE_Z) {
      param->scale_out[2] *= next_scale[2];
    }
    if (bone->bbone_next_flag & BBONE_HANDLE_SCALE_EASE) {
      param->ease2 *= next_scale[1];
      param->curve_out_x *= next_scale[1];
      param->curve_out_z *= next_scale[1];
    }
  }
}

/* Fills the array with the desired amount of bone->segments elements.
 * This calculation is done within unit bone space. */
void BKE_pchan_bbone_spline_setup(bPoseChannel *pchan,
                                  const bool rest,
                                  const bool for_deform,
                                  Mat4 *result_array)
{
  BBoneSplineParameters param;

  BKE_pchan_bbone_spline_params_get(pchan, rest, &param);

  pchan->bone->segments = BKE_pchan_bbone_spline_compute(&param, for_deform, result_array);
}

/* Computes the bezier handle vectors and rolls coming from custom handles. */
void BKE_pchan_bbone_handles_compute(const BBoneSplineParameters *param,
                                     float h1[3],
                                     float *r_roll1,
                                     float h2[3],
                                     float *r_roll2,
                                     bool ease,
                                     bool offsets)
{
  float mat3[3][3];
  float length = param->length;
  float epsilon = 1e-5 * length;

  if (param->do_scale) {
    length *= param->scale[1];
  }

  *r_roll1 = *r_roll2 = 0.0f;

  if (param->use_prev) {
    copy_v3_v3(h1, param->prev_h);

    if (param->prev_bbone) {
      /* If previous bone is B-bone too, use average handle direction. */
      h1[1] -= length;
    }

    if (normalize_v3(h1) < epsilon) {
      copy_v3_fl3(h1, 0.0f, -1.0f, 0.0f);
    }

    negate_v3(h1);

    if (!param->prev_bbone) {
      /* Find the previous roll to interpolate. */
      copy_m3_m4(mat3, param->prev_mat);
      mat3_vec_to_roll(mat3, h1, r_roll1);
    }
  }
  else {
    h1[0] = 0.0f;
    h1[1] = 1.0;
    h1[2] = 0.0f;
  }

  if (param->use_next) {
    copy_v3_v3(h2, param->next_h);

    /* If next bone is B-bone too, use average handle direction. */
    if (param->next_bbone) {
      /* pass */
    }
    else {
      h2[1] -= length;
    }

    if (normalize_v3(h2) < epsilon) {
      copy_v3_fl3(h2, 0.0f, 1.0f, 0.0f);
    }

    /* Find the next roll to interpolate as well. */
    copy_m3_m4(mat3, param->next_mat);
    mat3_vec_to_roll(mat3, h2, r_roll2);
  }
  else {
    h2[0] = 0.0f;
    h2[1] = 1.0f;
    h2[2] = 0.0f;
  }

  if (ease) {
    const float circle_factor = length * (cubic_tangent_factor_circle_v3(h1, h2) / 0.75f);

    const float hlength1 = param->ease1 * circle_factor;
    const float hlength2 = param->ease2 * circle_factor;

    /* and only now negate h2 */
    mul_v3_fl(h1, hlength1);
    mul_v3_fl(h2, -hlength2);
  }

  /* Add effects from bbone properties over the top
   * - These properties allow users to hand-animate the
   *   bone curve/shape, without having to resort to using
   *   extra bones
   * - The "bone" level offsets are for defining the rest-pose
   *   shape of the bone (e.g. for curved eyebrows for example).
   *   -> In the viewport, it's needed to define what the rest pose
   *      looks like
   *   -> For "rest == 0", we also still need to have it present
   *      so that we can "cancel out" this rest-pose when it comes
   *      time to deform some geometry, it won't cause double transforms.
   * - The "pchan" level offsets are the ones that animators actually
   *   end up animating
   */
  if (offsets) {
    /* Add extra rolls. */
    *r_roll1 += param->roll1;
    *r_roll2 += param->roll2;

    /* Extra curve x / y */
    /* NOTE:
     * Scale correction factors here are to compensate for some random floating-point glitches
     * when scaling up the bone or its parent by a factor of approximately 8.15/6, which results
     * in the bone length getting scaled up too (from 1 to 8), causing the curve to flatten out.
     */
    const float xscale_correction = (param->do_scale) ? param->scale[0] : 1.0f;
    const float zscale_correction = (param->do_scale) ? param->scale[2] : 1.0f;

    h1[0] += param->curve_in_x * xscale_correction;
    h1[2] += param->curve_in_z * zscale_correction;

    h2[0] += param->curve_out_x * xscale_correction;
    h2[2] += param->curve_out_z * zscale_correction;
  }
}

static void make_bbone_spline_matrix(BBoneSplineParameters *param,
                                     const float scalemats[2][4][4],
                                     const float pos[3],
                                     const float axis[3],
                                     float roll,
                                     float scalex,
                                     float scalez,
                                     float result[4][4])
{
  float mat3[3][3];

  vec_roll_to_mat3(axis, roll, mat3);

  copy_m4_m3(result, mat3);
  copy_v3_v3(result[3], pos);

  if (param->do_scale) {
    /* Correct for scaling when this matrix is used in scaled space. */
    mul_m4_series(result, scalemats[0], result, scalemats[1]);
  }

  /* BBone scale... */
  mul_v3_fl(result[0], scalex);
  mul_v3_fl(result[2], scalez);
}

/* Fade from first to second derivative when the handle is very short. */
static void ease_handle_axis(const float deriv1[3], const float deriv2[3], float r_axis[3])
{
  const float gap = 0.1f;

  copy_v3_v3(r_axis, deriv1);

  float len1 = len_squared_v3(deriv1), len2 = len_squared_v3(deriv2);
  float ratio = len1 / len2;

  if (ratio < gap * gap) {
    madd_v3_v3fl(r_axis, deriv2, gap - sqrtf(ratio));
  }
}

/* Fills the array with the desired amount of bone->segments elements.
 * This calculation is done within unit bone space. */
int BKE_pchan_bbone_spline_compute(BBoneSplineParameters *param,
                                   const bool for_deform,
                                   Mat4 *result_array)
{
  float scalemats[2][4][4];
  float bezt_controls[4][3];
  float h1[3], roll1, h2[3], roll2, prev[3], cur[3], axis[3];
  float length = param->length;

  if (param->do_scale) {
    size_to_mat4(scalemats[1], param->scale);
    invert_m4_m4(scalemats[0], scalemats[1]);

    length *= param->scale[1];
  }

  BKE_pchan_bbone_handles_compute(param, h1, &roll1, h2, &roll2, true, true);

  /* Make curve. */
  CLAMP_MAX(param->segments, MAX_BBONE_SUBDIV);

  copy_v3_fl3(bezt_controls[3], 0.0f, length, 0.0f);
  add_v3_v3v3(bezt_controls[2], bezt_controls[3], h2);
  copy_v3_v3(bezt_controls[1], h1);
  zero_v3(bezt_controls[0]);

  /* Compute lengthwise segment scale. */
  float segment_scales[MAX_BBONE_SUBDIV];

  CLAMP_MIN(param->scale_in[1], 0.0001f);
  CLAMP_MIN(param->scale_out[1], 0.0001f);

  const float log_scale_in_len = logf(param->scale_in[1]);
  const float log_scale_out_len = logf(param->scale_out[1]);

  for (int i = 0; i < param->segments; i++) {
    const float fac = ((float)i) / (param->segments - 1);
    segment_scales[i] = expf(interpf(log_scale_out_len, log_scale_in_len, fac));
  }

  /* Compute segment vertex offsets along the curve length. */
  float bezt_points[MAX_BBONE_SUBDIV + 1];

  equalize_cubic_bezier(
      bezt_controls, MAX_BBONE_SUBDIV, param->segments, segment_scales, bezt_points);

  /* Deformation uses N+1 matrices computed at points between the segments. */
  if (for_deform) {
    /* Bezier derivatives. */
    float bezt_deriv1[3][3], bezt_deriv2[2][3];

    for (int i = 0; i < 3; i++) {
      sub_v3_v3v3(bezt_deriv1[i], bezt_controls[i + 1], bezt_controls[i]);
    }
    for (int i = 0; i < 2; i++) {
      sub_v3_v3v3(bezt_deriv2[i], bezt_deriv1[i + 1], bezt_deriv1[i]);
    }

    /* End points require special handling to fix zero length handles. */
    ease_handle_axis(bezt_deriv1[0], bezt_deriv2[0], axis);
    make_bbone_spline_matrix(param,
                             scalemats,
                             bezt_controls[0],
                             axis,
                             roll1,
                             param->scale_in[0],
                             param->scale_in[2],
                             result_array[0].mat);

    for (int a = 1; a < param->segments; a++) {
      evaluate_cubic_bezier(bezt_controls, bezt_points[a], cur, axis);

      float fac = ((float)a) / param->segments;
      float roll = interpf(roll2, roll1, fac);
      float scalex = interpf(param->scale_out[0], param->scale_in[0], fac);
      float scalez = interpf(param->scale_out[2], param->scale_in[2], fac);

      make_bbone_spline_matrix(
          param, scalemats, cur, axis, roll, scalex, scalez, result_array[a].mat);
    }

    negate_v3(bezt_deriv2[1]);
    ease_handle_axis(bezt_deriv1[2], bezt_deriv2[1], axis);
    make_bbone_spline_matrix(param,
                             scalemats,
                             bezt_controls[3],
                             axis,
                             roll2,
                             param->scale_out[0],
                             param->scale_out[2],
                             result_array[param->segments].mat);
  }
  /* Other code (e.g. display) uses matrices for the segments themselves. */
  else {
    zero_v3(prev);

    for (int a = 0; a < param->segments; a++) {
      evaluate_cubic_bezier(bezt_controls, bezt_points[a + 1], cur, axis);

      sub_v3_v3v3(axis, cur, prev);

      float fac = (a + 0.5f) / param->segments;
      float roll = interpf(roll2, roll1, fac);
      float scalex = interpf(param->scale_out[0], param->scale_in[0], fac);
      float scalez = interpf(param->scale_out[2], param->scale_in[2], fac);

      make_bbone_spline_matrix(
          param, scalemats, prev, axis, roll, scalex, scalez, result_array[a].mat);
      copy_v3_v3(prev, cur);
    }
  }

  return param->segments;
}

static void allocate_bbone_cache(bPoseChannel *pchan, int segments)
{
  bPoseChannel_Runtime *runtime = &pchan->runtime;

  if (runtime->bbone_segments != segments) {
    BKE_pose_channel_free_bbone_cache(runtime);

    runtime->bbone_segments = segments;
    runtime->bbone_rest_mats = MEM_malloc_arrayN(
        1 + (uint)segments, sizeof(Mat4), "bPoseChannel_Runtime::bbone_rest_mats");
    runtime->bbone_pose_mats = MEM_malloc_arrayN(
        1 + (uint)segments, sizeof(Mat4), "bPoseChannel_Runtime::bbone_pose_mats");
    runtime->bbone_deform_mats = MEM_malloc_arrayN(
        2 + (uint)segments, sizeof(Mat4), "bPoseChannel_Runtime::bbone_deform_mats");
    runtime->bbone_dual_quats = MEM_malloc_arrayN(
        1 + (uint)segments, sizeof(DualQuat), "bPoseChannel_Runtime::bbone_dual_quats");
  }
}

/** Compute and cache the B-Bone shape in the channel runtime struct. */
void BKE_pchan_bbone_segments_cache_compute(bPoseChannel *pchan)
{
  bPoseChannel_Runtime *runtime = &pchan->runtime;
  Bone *bone = pchan->bone;
  int segments = bone->segments;

  BLI_assert(segments > 1);

  /* Allocate the cache if needed. */
  allocate_bbone_cache(pchan, segments);

  /* Compute the shape. */
  Mat4 *b_bone = runtime->bbone_pose_mats;
  Mat4 *b_bone_rest = runtime->bbone_rest_mats;
  Mat4 *b_bone_mats = runtime->bbone_deform_mats;
  DualQuat *b_bone_dual_quats = runtime->bbone_dual_quats;
  int a;

  BKE_pchan_bbone_spline_setup(pchan, false, true, b_bone);
  BKE_pchan_bbone_spline_setup(pchan, true, true, b_bone_rest);

  /* Compute deform matrices. */
  /* first matrix is the inverse arm_mat, to bring points in local bone space
   * for finding out which segment it belongs to */
  invert_m4_m4(b_bone_mats[0].mat, bone->arm_mat);

  /* then we make the b_bone_mats:
   * - first transform to local bone space
   * - translate over the curve to the bbone mat space
   * - transform with b_bone matrix
   * - transform back into global space */

  for (a = 0; a <= bone->segments; a++) {
    float tmat[4][4];

    invert_m4_m4(tmat, b_bone_rest[a].mat);
    mul_m4_series(b_bone_mats[a + 1].mat,
                  pchan->chan_mat,
                  bone->arm_mat,
                  b_bone[a].mat,
                  tmat,
                  b_bone_mats[0].mat);

    /* Compute the orthonormal object space rest matrix of the segment. */
    mul_m4_m4m4(tmat, bone->arm_mat, b_bone_rest[a].mat);
    normalize_m4(tmat);

    mat4_to_dquat(&b_bone_dual_quats[a], tmat, b_bone_mats[a + 1].mat);
  }
}

/** Copy cached B-Bone segments from one channel to another */
void BKE_pchan_bbone_segments_cache_copy(bPoseChannel *pchan, bPoseChannel *pchan_from)
{
  bPoseChannel_Runtime *runtime = &pchan->runtime;
  bPoseChannel_Runtime *runtime_from = &pchan_from->runtime;
  int segments = runtime_from->bbone_segments;

  if (segments <= 1) {
    BKE_pose_channel_free_bbone_cache(&pchan->runtime);
  }
  else {
    allocate_bbone_cache(pchan, segments);

    memcpy(runtime->bbone_rest_mats, runtime_from->bbone_rest_mats, sizeof(Mat4) * (1 + segments));
    memcpy(runtime->bbone_pose_mats, runtime_from->bbone_pose_mats, sizeof(Mat4) * (1 + segments));
    memcpy(runtime->bbone_deform_mats,
           runtime_from->bbone_deform_mats,
           sizeof(Mat4) * (2 + segments));
    memcpy(runtime->bbone_dual_quats,
           runtime_from->bbone_dual_quats,
           sizeof(DualQuat) * (1 + segments));
  }
}

/**
 * Calculate index and blend factor for the two B-Bone segment nodes
 * affecting the point at 0 <= pos <= 1.
 */
void BKE_pchan_bbone_deform_segment_index(const bPoseChannel *pchan,
                                          float pos,
                                          int *r_index,
                                          float *r_blend_next)
{
  int segments = pchan->bone->segments;

  CLAMP(pos, 0.0f, 1.0f);

  /* Calculate the indices of the 2 affecting b_bone segments.
   * Integer part is the first segment's index.
   * Integer part plus 1 is the second segment's index.
   * Fractional part is the blend factor. */
  float pre_blend = pos * (float)segments;

  int index = (int)floorf(pre_blend);
  CLAMP(index, 0, segments - 1);

  float blend = pre_blend - index;
  CLAMP(blend, 0.0f, 1.0f);

  *r_index = index;
  *r_blend_next = blend;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bone Space to Space Conversion API
 * \{ */

/* Convert World-Space Matrix to Pose-Space Matrix */
void BKE_armature_mat_world_to_pose(Object *ob, const float inmat[4][4], float outmat[4][4])
{
  float obmat[4][4];

  /* prevent crashes */
  if (ob == NULL) {
    return;
  }

  /* Get inverse of (armature) object's matrix. */
  invert_m4_m4(obmat, ob->obmat);

  /* multiply given matrix by object's-inverse to find pose-space matrix */
  mul_m4_m4m4(outmat, inmat, obmat);
}

/* Convert World-Space Location to Pose-Space Location
 * NOTE: this cannot be used to convert to pose-space location of the supplied
 *       pose-channel into its local space (i.e. 'visual'-keyframing) */
void BKE_armature_loc_world_to_pose(Object *ob, const float inloc[3], float outloc[3])
{
  float xLocMat[4][4];
  float nLocMat[4][4];

  /* build matrix for location */
  unit_m4(xLocMat);
  copy_v3_v3(xLocMat[3], inloc);

  /* get bone-space cursor matrix and extract location */
  BKE_armature_mat_world_to_pose(ob, xLocMat, nLocMat);
  copy_v3_v3(outloc, nLocMat[3]);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bone Matrix Calculation API
 * \{ */

/* Simple helper, computes the offset bone matrix.
 *     offs_bone = yoffs(b-1) + root(b) + bonemat(b). */
void BKE_bone_offset_matrix_get(const Bone *bone, float offs_bone[4][4])
{
  BLI_assert(bone->parent != NULL);

  /* Bone transform itself. */
  copy_m4_m3(offs_bone, bone->bone_mat);

  /* The bone's root offset (is in the parent's coordinate system). */
  copy_v3_v3(offs_bone[3], bone->head);

  /* Get the length translation of parent (length along y axis). */
  offs_bone[3][1] += bone->parent->length;
}

/* Construct the matrices (rot/scale and loc)
 * to apply the PoseChannels into the armature (object) space.
 * I.e. (roughly) the "pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b)" in the
 *     pose_mat(b)= pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) * chan_mat(b)
 * ...function.
 *
 * This allows to get the transformations of a bone in its object space,
 * *before* constraints (and IK) get applied (used by pose evaluation code).
 * And reverse: to find pchan transformations needed to place a bone at a given loc/rot/scale
 * in object space (used by interactive transform, and snapping code).
 *
 * Note that, with the HINGE/NO_SCALE/NO_LOCAL_LOCATION options, the location matrix
 * will differ from the rotation/scale matrix...
 *
 * NOTE: This cannot be used to convert to pose-space transforms of the supplied
 *       pose-channel into its local space (i.e. 'visual'-keyframing).
 *       (note: I don't understand that, so I keep it :p --mont29).
 */
void BKE_bone_parent_transform_calc_from_pchan(const bPoseChannel *pchan,
                                               BoneParentTransform *r_bpt)
{
  const Bone *bone, *parbone;
  const bPoseChannel *parchan;

  /* set up variables for quicker access below */
  bone = pchan->bone;
  parbone = bone->parent;
  parchan = pchan->parent;

  if (parchan) {
    float offs_bone[4][4];
    /* yoffs(b-1) + root(b) + bonemat(b). */
    BKE_bone_offset_matrix_get(bone, offs_bone);

    BKE_bone_parent_transform_calc_from_matrices(bone->flag,
                                                 bone->inherit_scale_mode,
                                                 offs_bone,
                                                 parbone->arm_mat,
                                                 parchan->pose_mat,
                                                 r_bpt);
  }
  else {
    BKE_bone_parent_transform_calc_from_matrices(
        bone->flag, bone->inherit_scale_mode, bone->arm_mat, NULL, NULL, r_bpt);
  }
}

/* Compute the parent transform using data decoupled from specific data structures.
 *
 * bone_flag: Bone->flag containing settings
 * offs_bone: delta from parent to current arm_mat (or just arm_mat if no parent)
 * parent_arm_mat, parent_pose_mat: arm_mat and pose_mat of parent, or NULL
 * r_bpt: OUTPUT parent transform */
void BKE_bone_parent_transform_calc_from_matrices(int bone_flag,
                                                  int inherit_scale_mode,
                                                  const float offs_bone[4][4],
                                                  const float parent_arm_mat[4][4],
                                                  const float parent_pose_mat[4][4],
                                                  BoneParentTransform *r_bpt)
{
  copy_v3_fl(r_bpt->post_scale, 1.0f);

  if (parent_pose_mat) {
    const bool use_rotation = (bone_flag & BONE_HINGE) == 0;
    const bool full_transform = use_rotation && inherit_scale_mode == BONE_INHERIT_SCALE_FULL;

    /* Compose the rotscale matrix for this bone. */
    if (full_transform) {
      /* Parent pose rotation and scale. */
      mul_m4_m4m4(r_bpt->rotscale_mat, parent_pose_mat, offs_bone);
    }
    else {
      float tmat[4][4], tscale[3];

      /* If using parent pose rotation: */
      if (use_rotation) {
        copy_m4_m4(tmat, parent_pose_mat);

        /* Normalize the matrix when needed. */
        switch (inherit_scale_mode) {
          case BONE_INHERIT_SCALE_FULL:
          case BONE_INHERIT_SCALE_FIX_SHEAR:
            /* Keep scale and shear. */
            break;

          case BONE_INHERIT_SCALE_NONE:
          case BONE_INHERIT_SCALE_AVERAGE:
            /* Remove scale and shear from parent. */
            orthogonalize_m4_stable(tmat, 1, true);
            break;

          case BONE_INHERIT_SCALE_ALIGNED:
            /* Remove shear and extract scale. */
            orthogonalize_m4_stable(tmat, 1, false);
            normalize_m4_ex(tmat, r_bpt->post_scale);
            break;

          case BONE_INHERIT_SCALE_NONE_LEGACY:
            /* Remove only scale - bad legacy way. */
            normalize_m4(tmat);
            break;

          default:
            BLI_assert_unreachable();
        }
      }
      /* If removing parent pose rotation: */
      else {
        copy_m4_m4(tmat, parent_arm_mat);

        /* Copy the parent scale when needed. */
        switch (inherit_scale_mode) {
          case BONE_INHERIT_SCALE_FULL:
            /* Ignore effects of shear. */
            mat4_to_size(tscale, parent_pose_mat);
            rescale_m4(tmat, tscale);
            break;

          case BONE_INHERIT_SCALE_FIX_SHEAR:
            /* Take the effects of parent shear into account to get exact volume. */
            mat4_to_size_fix_shear(tscale, parent_pose_mat);
            rescale_m4(tmat, tscale);
            break;

          case BONE_INHERIT_SCALE_ALIGNED:
            mat4_to_size_fix_shear(r_bpt->post_scale, parent_pose_mat);
            break;

          case BONE_INHERIT_SCALE_NONE:
          case BONE_INHERIT_SCALE_AVERAGE:
          case BONE_INHERIT_SCALE_NONE_LEGACY:
            /* Keep unscaled. */
            break;

          default:
            BLI_assert_unreachable();
        }
      }

      /* Apply the average parent scale when needed. */
      if (inherit_scale_mode == BONE_INHERIT_SCALE_AVERAGE) {
        mul_mat3_m4_fl(tmat, cbrtf(fabsf(mat4_to_volume_scale(parent_pose_mat))));
      }

      mul_m4_m4m4(r_bpt->rotscale_mat, tmat, offs_bone);

      /* Remove remaining shear when needed, preserving volume. */
      if (inherit_scale_mode == BONE_INHERIT_SCALE_FIX_SHEAR) {
        orthogonalize_m4_stable(r_bpt->rotscale_mat, 1, false);
      }
    }

    /* Compose the loc matrix for this bone. */
    /* NOTE: That version does not modify bone's loc when HINGE/NO_SCALE options are set. */

    /* In this case, use the object's space *orientation*. */
    if (bone_flag & BONE_NO_LOCAL_LOCATION) {
      /* XXX I'm sure that code can be simplified! */
      float bone_loc[4][4], bone_rotscale[3][3], tmat4[4][4], tmat3[3][3];
      unit_m4(bone_loc);
      unit_m4(r_bpt->loc_mat);
      unit_m4(tmat4);

      mul_v3_m4v3(bone_loc[3], parent_pose_mat, offs_bone[3]);

      unit_m3(bone_rotscale);
      copy_m3_m4(tmat3, parent_pose_mat);
      mul_m3_m3m3(bone_rotscale, tmat3, bone_rotscale);

      copy_m4_m3(tmat4, bone_rotscale);
      mul_m4_m4m4(r_bpt->loc_mat, bone_loc, tmat4);
    }
    /* Those flags do not affect position, use plain parent transform space! */
    else if (!full_transform) {
      mul_m4_m4m4(r_bpt->loc_mat, parent_pose_mat, offs_bone);
    }
    /* Else (i.e. default, usual case),
     * just use the same matrix for rotation/scaling, and location. */
    else {
      copy_m4_m4(r_bpt->loc_mat, r_bpt->rotscale_mat);
    }
  }
  /* Root bones. */
  else {
    /* Rotation/scaling. */
    copy_m4_m4(r_bpt->rotscale_mat, offs_bone);
    /* Translation. */
    if (bone_flag & BONE_NO_LOCAL_LOCATION) {
      /* Translation of arm_mat, without the rotation. */
      unit_m4(r_bpt->loc_mat);
      copy_v3_v3(r_bpt->loc_mat[3], offs_bone[3]);
    }
    else {
      copy_m4_m4(r_bpt->loc_mat, r_bpt->rotscale_mat);
    }
  }
}

void BKE_bone_parent_transform_clear(struct BoneParentTransform *bpt)
{
  unit_m4(bpt->rotscale_mat);
  unit_m4(bpt->loc_mat);
  copy_v3_fl(bpt->post_scale, 1.0f);
}

void BKE_bone_parent_transform_invert(struct BoneParentTransform *bpt)
{
  invert_m4(bpt->rotscale_mat);
  invert_m4(bpt->loc_mat);
  invert_v3_safe(bpt->post_scale);
}

void BKE_bone_parent_transform_combine(const struct BoneParentTransform *in1,
                                       const struct BoneParentTransform *in2,
                                       struct BoneParentTransform *result)
{
  mul_m4_m4m4(result->rotscale_mat, in1->rotscale_mat, in2->rotscale_mat);
  mul_m4_m4m4(result->loc_mat, in1->loc_mat, in2->loc_mat);
  mul_v3_v3v3(result->post_scale, in1->post_scale, in2->post_scale);
}

void BKE_bone_parent_transform_apply(const struct BoneParentTransform *bpt,
                                     const float inmat[4][4],
                                     float outmat[4][4])
{
  /* in case inmat == outmat */
  float tmploc[3];
  copy_v3_v3(tmploc, inmat[3]);

  mul_m4_m4m4(outmat, bpt->rotscale_mat, inmat);
  mul_v3_m4v3(outmat[3], bpt->loc_mat, tmploc);
  rescale_m4(outmat, bpt->post_scale);
}

/* Convert Pose-Space Matrix to Bone-Space Matrix.
 * NOTE: this cannot be used to convert to pose-space transforms of the supplied
 *       pose-channel into its local space (i.e. 'visual'-keyframing) */
void BKE_armature_mat_pose_to_bone(bPoseChannel *pchan,
                                   const float inmat[4][4],
                                   float outmat[4][4])
{
  BoneParentTransform bpt;

  BKE_bone_parent_transform_calc_from_pchan(pchan, &bpt);
  BKE_bone_parent_transform_invert(&bpt);
  BKE_bone_parent_transform_apply(&bpt, inmat, outmat);
}

/* Convert Bone-Space Matrix to Pose-Space Matrix. */
void BKE_armature_mat_bone_to_pose(bPoseChannel *pchan,
                                   const float inmat[4][4],
                                   float outmat[4][4])
{
  BoneParentTransform bpt;

  BKE_bone_parent_transform_calc_from_pchan(pchan, &bpt);
  BKE_bone_parent_transform_apply(&bpt, inmat, outmat);
}

/* Convert Pose-Space Location to Bone-Space Location
 * NOTE: this cannot be used to convert to pose-space location of the supplied
 *       pose-channel into its local space (i.e. 'visual'-keyframing) */
void BKE_armature_loc_pose_to_bone(bPoseChannel *pchan, const float inloc[3], float outloc[3])
{
  float xLocMat[4][4];
  float nLocMat[4][4];

  /* build matrix for location */
  unit_m4(xLocMat);
  copy_v3_v3(xLocMat[3], inloc);

  /* get bone-space cursor matrix and extract location */
  BKE_armature_mat_pose_to_bone(pchan, xLocMat, nLocMat);
  copy_v3_v3(outloc, nLocMat[3]);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bone Matrix Read/Write API
 *
 * High level functions for transforming bones and reading the transform values.
 * \{ */

void BKE_armature_mat_pose_to_bone_ex(struct Depsgraph *depsgraph,
                                      Object *ob,
                                      bPoseChannel *pchan,
                                      const float inmat[4][4],
                                      float outmat[4][4])
{
  bPoseChannel work_pchan = *pchan;

  /* recalculate pose matrix with only parent transformations,
   * bone loc/sca/rot is ignored, scene and frame are not used. */
  BKE_pose_where_is_bone(depsgraph, NULL, ob, &work_pchan, 0.0f, false);

  /* Find the matrix, need to remove the bone transforms first so this is calculated
   * as a matrix to set rather than a difference on top of what's already there. */
  unit_m4(outmat);
  BKE_pchan_apply_mat4(&work_pchan, outmat, false);

  BKE_armature_mat_pose_to_bone(&work_pchan, inmat, outmat);
}

/**
 * Same as #BKE_object_mat3_to_rot().
 */
void BKE_pchan_mat3_to_rot(bPoseChannel *pchan, const float mat[3][3], bool use_compat)
{
  BLI_ASSERT_UNIT_M3(mat);

  switch (pchan->rotmode) {
    case ROT_MODE_QUAT:
      mat3_normalized_to_quat(pchan->quat, mat);
      break;
    case ROT_MODE_AXISANGLE:
      mat3_normalized_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, mat);
      break;
    default: /* euler */
      if (use_compat) {
        mat3_normalized_to_compatible_eulO(pchan->eul, pchan->eul, pchan->rotmode, mat);
      }
      else {
        mat3_normalized_to_eulO(pchan->eul, pchan->rotmode, mat);
      }
      break;
  }
}

/**
 * Same as #BKE_object_rot_to_mat3().
 */
void BKE_pchan_rot_to_mat3(const bPoseChannel *pchan, float r_mat[3][3])
{
  /* rotations may either be quats, eulers (with various rotation orders), or axis-angle */
  if (pchan->rotmode > 0) {
    /* Euler rotations (will cause gimbal lock,
     * but this can be alleviated a bit with rotation orders) */
    eulO_to_mat3(r_mat, pchan->eul, pchan->rotmode);
  }
  else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
    /* axis-angle - not really that great for 3D-changing orientations */
    axis_angle_to_mat3(r_mat, pchan->rotAxis, pchan->rotAngle);
  }
  else {
    /* quats are normalized before use to eliminate scaling issues */
    float quat[4];

    /* NOTE: we now don't normalize the stored values anymore,
     * since this was kind of evil in some cases but if this proves to be too problematic,
     * switch back to the old system of operating directly on the stored copy. */
    normalize_qt_qt(quat, pchan->quat);
    quat_to_mat3(r_mat, quat);
  }
}

/**
 * Apply a 4x4 matrix to the pose bone,
 * similar to #BKE_object_apply_mat4().
 */
void BKE_pchan_apply_mat4(bPoseChannel *pchan, const float mat[4][4], bool use_compat)
{
  float rot[3][3];
  mat4_to_loc_rot_size(pchan->loc, rot, pchan->size, mat);
  BKE_pchan_mat3_to_rot(pchan, rot, use_compat);
}

/**
 * Remove rest-position effects from pose-transform for obtaining
 * 'visual' transformation of pose-channel.
 * (used by the Visual-Keyframing stuff).
 */
void BKE_armature_mat_pose_to_delta(float delta_mat[4][4],
                                    float pose_mat[4][4],
                                    float arm_mat[4][4])
{
  float imat[4][4];

  invert_m4_m4(imat, arm_mat);
  mul_m4_m4m4(delta_mat, imat, pose_mat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rotation Mode Conversions
 *
 * Used for Objects and Pose Channels, since both can have multiple rotation representations.
 * \{ */

/**
 * Called from RNA when rotation mode changes
 * - the result should be that the rotations given in the provided pointers have had conversions
 *   applied (as appropriate), such that the rotation of the element hasn't 'visually' changed.
 */
void BKE_rotMode_change_values(
    float quat[4], float eul[3], float axis[3], float *angle, short oldMode, short newMode)
{
  /* check if any change - if so, need to convert data */
  if (newMode > 0) { /* to euler */
    if (oldMode == ROT_MODE_AXISANGLE) {
      /* axis-angle to euler */
      axis_angle_to_eulO(eul, newMode, axis, *angle);
    }
    else if (oldMode == ROT_MODE_QUAT) {
      /* quat to euler */
      normalize_qt(quat);
      quat_to_eulO(eul, newMode, quat);
    }
    /* else { no conversion needed } */
  }
  else if (newMode == ROT_MODE_QUAT) { /* to quat */
    if (oldMode == ROT_MODE_AXISANGLE) {
      /* axis angle to quat */
      axis_angle_to_quat(quat, axis, *angle);
    }
    else if (oldMode > 0) {
      /* euler to quat */
      eulO_to_quat(quat, eul, oldMode);
    }
    /* else { no conversion needed } */
  }
  else if (newMode == ROT_MODE_AXISANGLE) { /* to axis-angle */
    if (oldMode > 0) {
      /* euler to axis angle */
      eulO_to_axis_angle(axis, angle, eul, oldMode);
    }
    else if (oldMode == ROT_MODE_QUAT) {
      /* quat to axis angle */
      normalize_qt(quat);
      quat_to_axis_angle(axis, angle, quat);
    }

    /* When converting to axis-angle,
     * we need a special exception for the case when there is no axis. */
    if (IS_EQF(axis[0], axis[1]) && IS_EQF(axis[1], axis[2])) {
      /* for now, rotate around y-axis then (so that it simply becomes the roll) */
      axis[1] = 1.0f;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bone Vector, Roll Conversion
 *
 * Used for Objects and Pose Channels, since both can have multiple rotation representations.
 *
 * How it Works
 * ============
 *
 * This is the bone transformation trick; they're hierarchical so each bone(b)
 * is in the coord system of bone(b-1):
 *
 * arm_mat(b)= arm_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b)
 *
 * -> yoffs is just the y axis translation in parent's coord system
 * -> d_root is the translation of the bone root, also in parent's coord system
 *
 * pose_mat(b)= pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) * chan_mat(b)
 *
 * we then - in init deform - store the deform in chan_mat, such that:
 *
 * pose_mat(b)= arm_mat(b) * chan_mat(b)
 *
 * \{ */

/* Computes vector and roll based on a rotation.
 * "mat" must contain only a rotation, and no scaling. */
void mat3_to_vec_roll(const float mat[3][3], float r_vec[3], float *r_roll)
{
  if (r_vec) {
    copy_v3_v3(r_vec, mat[1]);
  }

  if (r_roll) {
    mat3_vec_to_roll(mat, mat[1], r_roll);
  }
}

/* Computes roll around the vector that best approximates the matrix.
 * If vec is the Y vector from purely rotational mat, result should be exact. */
void mat3_vec_to_roll(const float mat[3][3], const float vec[3], float *r_roll)
{
  float vecmat[3][3], vecmatinv[3][3], rollmat[3][3], q[4];

  /* Compute the orientation relative to the vector with zero roll. */
  vec_roll_to_mat3(vec, 0.0f, vecmat);
  invert_m3_m3(vecmatinv, vecmat);
  mul_m3_m3m3(rollmat, vecmatinv, mat);

  /* Extract the twist angle as the roll value. */
  mat3_to_quat(q, rollmat);

  *r_roll = quat_split_swing_and_twist(q, 1, NULL, NULL);
}

/* Calculates the rest matrix of a bone based on its vector and a roll around that vector. */
/**
 * Given `v = (v.x, v.y, v.z)` our (normalized) bone vector, we want the rotation matrix M
 * from the Y axis (so that `M * (0, 1, 0) = v`).
 * - The rotation axis a lays on XZ plane, and it is orthonormal to v,
 *   hence to the projection of v onto XZ plane.
 * - `a = (v.z, 0, -v.x)`
 *
 * We know a is eigenvector of M (so M * a = a).
 * Finally, we have w, such that M * w = (0, 1, 0)
 * (i.e. the vector that will be aligned with Y axis once transformed).
 * We know w is symmetric to v by the Y axis.
 * - `w = (-v.x, v.y, -v.z)`
 *
 * Solving this, we get (x, y and z being the components of v):
 * <pre>
 *     ┌ (x^2 * y + z^2) / (x^2 + z^2),   x,   x * z * (y - 1) / (x^2 + z^2) ┐
 * M = │  x * (y^2 - 1)  / (x^2 + z^2),   y,    z * (y^2 - 1)  / (x^2 + z^2) │
 *     └ x * z * (y - 1) / (x^2 + z^2),   z,   (x^2 + z^2 * y) / (x^2 + z^2) ┘
 * </pre>
 *
 * This is stable as long as v (the bone) is not too much aligned with +/-Y
 * (i.e. x and z components are not too close to 0).
 *
 * Since v is normalized, we have `x^2 + y^2 + z^2 = 1`,
 * hence `x^2 + z^2 = 1 - y^2 = (1 - y)(1 + y)`.
 *
 * This allows to simplifies M like this:
 * <pre>
 *     ┌ 1 - x^2 / (1 + y),   x,     -x * z / (1 + y) ┐
 * M = │                -x,   y,                   -z │
 *     └  -x * z / (1 + y),   z,    1 - z^2 / (1 + y) ┘
 * </pre>
 *
 * Written this way, we see the case v = +Y is no more a singularity.
 * The only one
 * remaining is the bone being aligned with -Y.
 *
 * Let's handle
 * the asymptotic behavior when bone vector is reaching the limit of y = -1.
 * Each of the four corner elements can vary from -1 to 1,
 * depending on the axis a chosen for doing the rotation.
 * And the "rotation" here is in fact established by mirroring XZ plane by that given axis,
 * then inversing the Y-axis.
 * For sufficiently small x and z, and with y approaching -1,
 * all elements but the four corner ones of M will degenerate.
 * So let's now focus on these corner elements.
 *
 * We rewrite M so that it only contains its four corner elements,
 * and combine the `1 / (1 + y)` factor:
 * <pre>
 *                    ┌ 1 + y - x^2,        -x * z ┐
 * M* = 1 / (1 + y) * │                            │
 *                    └      -x * z,   1 + y - z^2 ┘
 * </pre>
 *
 * When y is close to -1, computing 1 / (1 + y) will cause severe numerical instability,
 * so we use a different approach based on x and z as inputs.
 * We know `y^2 = 1 - (x^2 + z^2)`, and `y < 0`, hence `y = -sqrt(1 - (x^2 + z^2))`.
 *
 * Since x and z are both close to 0, we apply the binomial expansion to the second order:
 * `y = -sqrt(1 - (x^2 + z^2)) = -1 + (x^2 + z^2) / 2 + (x^2 + z^2)^2 / 8`, which allows
 * eliminating the problematic `1` constant.
 *
 * A first order expansion allows simplifying to this, but second order is more precise:
 * <pre>
 *                        ┌  z^2 - x^2,  -2 * x * z ┐
 * M* = 1 / (x^2 + z^2) * │                         │
 *                        └ -2 * x * z,   x^2 - z^2 ┘
 * </pre>
 *
 * P.S. In the end, this basically is a heavily optimized version of Damped Track +Y.
 */
void vec_roll_to_mat3_normalized(const float nor[3], const float roll, float r_mat[3][3])
{
  const float SAFE_THRESHOLD = 6.1e-3f;     /* Theta above this value has good enough precision. */
  const float CRITICAL_THRESHOLD = 2.5e-4f; /* True singularity if XZ distance is below this. */
  const float THRESHOLD_SQUARED = CRITICAL_THRESHOLD * CRITICAL_THRESHOLD;

  const float x = nor[0];
  const float y = nor[1];
  const float z = nor[2];

  float theta = 1.0f + y;                /* remapping Y from [-1,+1] to [0,2]. */
  const float theta_alt = x * x + z * z; /* squared distance from origin in x,z plane. */
  float rMatrix[3][3], bMatrix[3][3];

  BLI_ASSERT_UNIT_V3(nor);

  /* Determine if the input is far enough from the true singularity of this type of
   * transformation at (0,-1,0), where roll becomes 0/0 undefined without a limit.
   *
   * When theta is close to zero (nor is aligned close to negative Y Axis),
   * we have to check we do have non-null X/Z components as well.
   * Also, due to float precision errors, nor can be (0.0, -0.99999994, 0.0) which results
   * in theta being close to zero. This will cause problems when theta is used as divisor.
   */
  if (theta > SAFE_THRESHOLD || theta_alt > THRESHOLD_SQUARED) {
    /* nor is *not* aligned to negative Y-axis (0,-1,0). */

    bMatrix[0][1] = -x;
    bMatrix[1][0] = x;
    bMatrix[1][1] = y;
    bMatrix[1][2] = z;
    bMatrix[2][1] = -z;

    if (theta <= SAFE_THRESHOLD) {
      /* When nor is close to negative Y axis (0,-1,0) the theta precision is very bad,
       * so recompute it from x and z instead, using the series expansion for sqrt. */
      theta = theta_alt * 0.5f + theta_alt * theta_alt * 0.125f;
    }

    bMatrix[0][0] = 1 - x * x / theta;
    bMatrix[2][2] = 1 - z * z / theta;
    bMatrix[2][0] = bMatrix[0][2] = -x * z / theta;
  }
  else {
    /* nor is very close to negative Y axis (0,-1,0): use simple symmetry by Z axis. */
    unit_m3(bMatrix);
    bMatrix[0][0] = bMatrix[1][1] = -1.0;
  }

  /* Make Roll matrix */
  axis_angle_normalized_to_mat3(rMatrix, nor, roll);

  /* Combine and output result */
  mul_m3_m3m3(r_mat, rMatrix, bMatrix);
}

void vec_roll_to_mat3(const float vec[3], const float roll, float r_mat[3][3])
{
  float nor[3];

  normalize_v3_v3(nor, vec);
  vec_roll_to_mat3_normalized(nor, roll, r_mat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Bone Matrix Calculation (Recursive)
 * \{ */

/**
 * Recursive part, calculates rest-position of entire tree of children.
 * \note Used when exiting edit-mode too.
 */
void BKE_armature_where_is_bone(Bone *bone, const Bone *bone_parent, const bool use_recursion)
{
  float vec[3];

  /* Bone Space */
  sub_v3_v3v3(vec, bone->tail, bone->head);
  bone->length = len_v3(vec);
  vec_roll_to_mat3(vec, bone->roll, bone->bone_mat);

  /* this is called on old file reading too... */
  if (bone->xwidth == 0.0f) {
    bone->xwidth = 0.1f;
    bone->zwidth = 0.1f;
    bone->segments = 1;
  }

  if (bone_parent) {
    float offs_bone[4][4];
    /* yoffs(b-1) + root(b) + bonemat(b) */
    BKE_bone_offset_matrix_get(bone, offs_bone);

    /* Compose the matrix for this bone. */
    mul_m4_m4m4(bone->arm_mat, bone_parent->arm_mat, offs_bone);
  }
  else {
    copy_m4_m3(bone->arm_mat, bone->bone_mat);
    copy_v3_v3(bone->arm_mat[3], bone->head);
  }

  /* and the kiddies */
  if (use_recursion) {
    bone_parent = bone;
    for (bone = bone->childbase.first; bone; bone = bone->next) {
      BKE_armature_where_is_bone(bone, bone_parent, use_recursion);
    }
  }
}

/* updates vectors and matrices on rest-position level, only needed
 * after editing armature itself, now only on reading file */
void BKE_armature_where_is(bArmature *arm)
{
  Bone *bone;

  /* hierarchical from root to children */
  for (bone = arm->bonebase.first; bone; bone = bone->next) {
    BKE_armature_where_is_bone(bone, NULL, true);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose Rebuild
 * \{ */

/* if bone layer is protected, copy the data from from->pose
 * when used with linked libraries this copies from the linked pose into the local pose */
static void pose_proxy_sync(Object *ob, Object *from, int layer_protected)
{
  bPose *pose = ob->pose, *frompose = from->pose;
  bPoseChannel *pchan, *pchanp;
  bConstraint *con;
  int error = 0;

  if (frompose == NULL) {
    return;
  }

  /* in some cases when rigs change, we can't synchronize
   * to avoid crashing check for possible errors here */
  for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
    if (pchan->bone->layer & layer_protected) {
      if (BKE_pose_channel_find_name(frompose, pchan->name) == NULL) {
        CLOG_ERROR(&LOG,
                   "failed to sync proxy armature because '%s' is missing pose channel '%s'",
                   from->id.name,
                   pchan->name);
        error = 1;
      }
    }
  }

  if (error) {
    return;
  }

  /* clear all transformation values from library */
  BKE_pose_rest(frompose, false);

  /* copy over all of the proxy's bone groups */
  /* TODO: for later
   * - implement 'local' bone groups as for constraints
   * NOTE: this isn't trivial, as bones reference groups by index not by pointer,
   *       so syncing things correctly needs careful attention */
  BLI_freelistN(&pose->agroups);
  BLI_duplicatelist(&pose->agroups, &frompose->agroups);
  pose->active_group = frompose->active_group;

  for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
    pchanp = BKE_pose_channel_find_name(frompose, pchan->name);

    if (UNLIKELY(pchanp == NULL)) {
      /* happens for proxies that become invalid because of a missing link
       * for regular cases it shouldn't happen at all */
    }
    else if (pchan->bone->layer & layer_protected) {
      ListBase proxylocal_constraints = {NULL, NULL};
      bPoseChannel pchanw;

      /* copy posechannel to temp, but restore important pointers */
      pchanw = *pchanp;
      pchanw.bone = pchan->bone;
      pchanw.prev = pchan->prev;
      pchanw.next = pchan->next;
      pchanw.parent = pchan->parent;
      pchanw.child = pchan->child;
      pchanw.custom_tx = pchan->custom_tx;
      pchanw.bbone_prev = pchan->bbone_prev;
      pchanw.bbone_next = pchan->bbone_next;

      pchanw.mpath = pchan->mpath;
      pchan->mpath = NULL;

      /* Reset runtime data, we don't want to share that with the proxy. */
      BKE_pose_channel_runtime_reset_on_copy(&pchanw.runtime);

      /* this is freed so copy a copy, else undo crashes */
      if (pchanw.prop) {
        pchanw.prop = IDP_CopyProperty(pchanw.prop);

        /* use the values from the existing props */
        if (pchan->prop) {
          IDP_SyncGroupValues(pchanw.prop, pchan->prop);
        }
      }

      /* Constraints - proxy constraints are flushed... local ones are added after
       * 1: extract constraints not from proxy (CONSTRAINT_PROXY_LOCAL) from pchan's constraints.
       * 2: copy proxy-pchan's constraints on-to new.
       * 3: add extracted local constraints back on top.
       *
       * Note for BKE_constraints_copy:
       * When copying constraints, disable 'do_extern' otherwise
       * we get the libs direct linked in this blend.
       */
      BKE_constraints_proxylocal_extract(&proxylocal_constraints, &pchan->constraints);
      BKE_constraints_copy(&pchanw.constraints, &pchanp->constraints, false);
      BLI_movelisttolist(&pchanw.constraints, &proxylocal_constraints);

      /* constraints - set target ob pointer to own object */
      for (con = pchanw.constraints.first; con; con = con->next) {
        const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
        ListBase targets = {NULL, NULL};
        bConstraintTarget *ct;

        if (cti && cti->get_constraint_targets) {
          cti->get_constraint_targets(con, &targets);

          for (ct = targets.first; ct; ct = ct->next) {
            if (ct->tar == from) {
              ct->tar = ob;
            }
          }

          if (cti->flush_constraint_targets) {
            cti->flush_constraint_targets(con, &targets, 0);
          }
        }
      }

      /* free stuff from current channel */
      BKE_pose_channel_free(pchan);

      /* copy data in temp back over to the cleaned-out (but still allocated) original channel */
      *pchan = pchanw;
      if (pchan->custom) {
        id_us_plus(&pchan->custom->id);
      }
    }
    else {
      /* always copy custom shape */
      pchan->custom = pchanp->custom;
      if (pchan->custom) {
        id_us_plus(&pchan->custom->id);
      }
      if (pchanp->custom_tx) {
        pchan->custom_tx = BKE_pose_channel_find_name(pose, pchanp->custom_tx->name);
      }

      /* ID-Property Syncing */
      {
        IDProperty *prop_orig = pchan->prop;
        if (pchanp->prop) {
          pchan->prop = IDP_CopyProperty(pchanp->prop);
          if (prop_orig) {
            /* copy existing values across when types match */
            IDP_SyncGroupValues(pchan->prop, prop_orig);
          }
        }
        else {
          pchan->prop = NULL;
        }
        if (prop_orig) {
          IDP_FreeProperty(prop_orig);
        }
      }
    }
  }
}

/**
 * \param r_last_visited_bone_p: The last bone handled by the last call to this function.
 */
static int rebuild_pose_bone(
    bPose *pose, Bone *bone, bPoseChannel *parchan, int counter, Bone **r_last_visited_bone_p)
{
  bPoseChannel *pchan = BKE_pose_channel_ensure(pose, bone->name); /* verify checks and/or adds */

  pchan->bone = bone;
  pchan->parent = parchan;

  /* We ensure the current pchan is immediately after the one we just generated/updated in the
   * previous call to `rebuild_pose_bone`.
   *
   * It may be either the parent, the previous sibling, or the last
   * (grand-(grand-(...)))-child (as processed by the recursive, depth-first nature of this
   * function) of the previous sibling.
   *
   * NOTE: In most cases there is nothing to do here, but pose list may get out of order when some
   * bones are added, removed or moved in the armature data. */
  bPoseChannel *pchan_prev = pchan->prev;
  const Bone *last_visited_bone = *r_last_visited_bone_p;
  if ((pchan_prev == NULL && last_visited_bone != NULL) ||
      (pchan_prev != NULL && pchan_prev->bone != last_visited_bone)) {
    pchan_prev = last_visited_bone != NULL ?
                     BKE_pose_channel_find_name(pose, last_visited_bone->name) :
                     NULL;
    BLI_remlink(&pose->chanbase, pchan);
    BLI_insertlinkafter(&pose->chanbase, pchan_prev, pchan);
  }

  *r_last_visited_bone_p = pchan->bone;
  counter++;

  for (bone = bone->childbase.first; bone; bone = bone->next) {
    counter = rebuild_pose_bone(pose, bone, pchan, counter, r_last_visited_bone_p);
    /* for quick detecting of next bone in chain, only b-bone uses it now */
    if (bone->flag & BONE_CONNECTED) {
      pchan->child = BKE_pose_channel_find_name(pose, bone->name);
    }
  }

  return counter;
}

/**
 * Clear pointers of object's pose
 * (needed in remap case, since we cannot always wait for a complete pose rebuild).
 */
void BKE_pose_clear_pointers(bPose *pose)
{
  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    pchan->bone = NULL;
    pchan->child = NULL;
  }
}

void BKE_pose_remap_bone_pointers(bArmature *armature, bPose *pose)
{
  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    pchan->bone = BKE_armature_find_bone_name(armature, pchan->name);
  }
}

/** Find the matching pose channel using the bone name, if not NULL. */
static bPoseChannel *pose_channel_find_bone(bPose *pose, Bone *bone)
{
  return (bone != NULL) ? BKE_pose_channel_find_name(pose, bone->name) : NULL;
}

/** Update the links for the B-Bone handles from Bone data. */
void BKE_pchan_rebuild_bbone_handles(bPose *pose, bPoseChannel *pchan)
{
  pchan->bbone_prev = pose_channel_find_bone(pose, pchan->bone->bbone_prev);
  pchan->bbone_next = pose_channel_find_bone(pose, pchan->bone->bbone_next);
}

void BKE_pose_channels_clear_with_null_bone(bPose *pose, const bool do_id_user)
{
  LISTBASE_FOREACH_MUTABLE (bPoseChannel *, pchan, &pose->chanbase) {
    if (pchan->bone == NULL) {
      BKE_pose_channel_free_ex(pchan, do_id_user);
      BKE_pose_channels_hash_free(pose);
      BLI_freelinkN(&pose->chanbase, pchan);
    }
  }
}

/**
 * Only after leave editmode, duplicating, validating older files, library syncing.
 *
 * \note pose->flag is set for it.
 *
 * \param bmain: May be NULL, only used to tag depsgraph as being dirty...
 */
void BKE_pose_rebuild(Main *bmain, Object *ob, bArmature *arm, const bool do_id_user)
{
  Bone *bone;
  bPose *pose;
  bPoseChannel *pchan;
  int counter = 0;

  /* only done here */
  if (ob->pose == NULL) {
    /* create new pose */
    ob->pose = MEM_callocN(sizeof(bPose), "new pose");

    /* set default settings for animviz */
    animviz_settings_init(&ob->pose->avs);
  }
  pose = ob->pose;

  /* clear */
  BKE_pose_clear_pointers(pose);

  /* first step, check if all channels are there */
  Bone *prev_bone = NULL;
  for (bone = arm->bonebase.first; bone; bone = bone->next) {
    counter = rebuild_pose_bone(pose, bone, NULL, counter, &prev_bone);
  }

  /* and a check for garbage */
  BKE_pose_channels_clear_with_null_bone(pose, do_id_user);

  BKE_pose_channels_hash_ensure(pose);

  for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
    /* Find the custom B-Bone handles. */
    BKE_pchan_rebuild_bbone_handles(pose, pchan);
    /* Re-validate that we are still using a valid pchan form custom transform. */
    /* Note that we could store pointers of freed pchan in a GSet to speed this up, however this is
     * supposed to be a rarely used feature, so for now assuming that always building that GSet
     * would be less optimal. */
    if (pchan->custom_tx != NULL && BLI_findindex(&pose->chanbase, pchan->custom_tx) == -1) {
      pchan->custom_tx = NULL;
    }
  }

  // printf("rebuild pose %s, %d bones\n", ob->id.name, counter);

  /* synchronize protected layers with proxy */
  /* HACK! To preserve 2.7x behavior that you always can pose even locked bones,
   * do not do any restoration if this is a COW temp copy! */
  /* Switched back to just NO_MAIN tag, for some reasons (c)
   * using COW tag was working this morning, but not anymore... */
  if (ob->proxy != NULL && (ob->id.tag & LIB_TAG_NO_MAIN) == 0) {
    BKE_object_copy_proxy_drivers(ob, ob->proxy);
    pose_proxy_sync(ob, ob->proxy, arm->layer_protected);
  }

  BKE_pose_update_constraint_flags(pose); /* for IK detection for example */

  pose->flag &= ~POSE_RECALC;
  pose->flag |= POSE_WAS_REBUILT;

  /* Rebuilding poses forces us to also rebuild the dependency graph,
   * since there is one node per pose/bone. */
  if (bmain != NULL) {
    DEG_relations_tag_update(bmain);
  }
}

/**
 * Ensures object's pose is rebuilt if needed.
 *
 * \param bmain: May be NULL, only used to tag depsgraph as being dirty...
 */
void BKE_pose_ensure(Main *bmain, Object *ob, bArmature *arm, const bool do_id_user)
{
  BLI_assert(!ELEM(NULL, arm, ob));
  if (ob->type == OB_ARMATURE && ((ob->pose == NULL) || (ob->pose->flag & POSE_RECALC))) {
    BLI_assert(GS(arm->id.name) == ID_AR);
    BKE_pose_rebuild(bmain, ob, arm, do_id_user);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose Solver
 * \{ */

/**
 * Convert the loc/rot/size to \a r_chanmat (typically #bPoseChannel.chan_mat).
 */
void BKE_pchan_to_mat4(const bPoseChannel *pchan, float r_chanmat[4][4])
{
  float smat[3][3];
  float rmat[3][3];
  float tmat[3][3];

  /* get scaling matrix */
  size_to_mat3(smat, pchan->size);

  /* get rotation matrix */
  BKE_pchan_rot_to_mat3(pchan, rmat);

  /* calculate matrix of bone (as 3x3 matrix, but then copy the 4x4) */
  mul_m3_m3m3(tmat, rmat, smat);
  copy_m4_m3(r_chanmat, tmat);

  /* prevent action channels breaking chains */
  /* need to check for bone here, CONSTRAINT_TYPE_ACTION uses this call */
  if ((pchan->bone == NULL) || !(pchan->bone->flag & BONE_CONNECTED)) {
    copy_v3_v3(r_chanmat[3], pchan->loc);
  }
}

/* loc/rot/size to mat4 */
/* used in constraint.c too */
void BKE_pchan_calc_mat(bPoseChannel *pchan)
{
  /* this is just a wrapper around the copy of this function which calculates the matrix
   * and stores the result in any given channel
   */
  BKE_pchan_to_mat4(pchan, pchan->chan_mat);
}

/* calculate tail of posechannel */
void BKE_pose_where_is_bone_tail(bPoseChannel *pchan)
{
  float vec[3];

  copy_v3_v3(vec, pchan->pose_mat[1]);
  mul_v3_fl(vec, pchan->bone->length);
  add_v3_v3v3(pchan->pose_tail, pchan->pose_head, vec);
}

/* The main armature solver, does all constraints excluding IK */
/* pchan is validated, as having bone and parent pointer
 * 'do_extra': when zero skips loc/size/rot, constraints and strip modifiers.
 */
void BKE_pose_where_is_bone(struct Depsgraph *depsgraph,
                            Scene *scene,
                            Object *ob,
                            bPoseChannel *pchan,
                            float ctime,
                            bool do_extra)
{
  /* This gives a chan_mat with actions (F-curve) results. */
  if (do_extra) {
    BKE_pchan_calc_mat(pchan);
  }
  else {
    unit_m4(pchan->chan_mat);
  }

  /* Construct the posemat based on PoseChannels, that we do before applying constraints. */
  /* pose_mat(b) = pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) * chan_mat(b) */
  BKE_armature_mat_bone_to_pose(pchan, pchan->chan_mat, pchan->pose_mat);

  /* Only rootbones get the cyclic offset (unless user doesn't want that). */
  /* XXX That could be a problem for snapping and other "reverse transform" features... */
  if (!pchan->parent) {
    if ((pchan->bone->flag & BONE_NO_CYCLICOFFSET) == 0) {
      add_v3_v3(pchan->pose_mat[3], ob->pose->cyclic_offset);
    }
  }

  if (do_extra) {
    /* Do constraints */
    if (pchan->constraints.first) {
      bConstraintOb *cob;
      float vec[3];

      /* make a copy of location of PoseChannel for later */
      copy_v3_v3(vec, pchan->pose_mat[3]);

      /* prepare PoseChannel for Constraint solving
       * - makes a copy of matrix, and creates temporary struct to use
       */
      cob = BKE_constraints_make_evalob(depsgraph, scene, ob, pchan, CONSTRAINT_OBTYPE_BONE);

      /* Solve PoseChannel's Constraints */

      /* ctime doesn't alter objects. */
      BKE_constraints_solve(depsgraph, &pchan->constraints, cob, ctime);

      /* cleanup after Constraint Solving
       * - applies matrix back to pchan, and frees temporary struct used
       */
      BKE_constraints_clear_evalob(cob);

      /* prevent constraints breaking a chain */
      if (pchan->bone->flag & BONE_CONNECTED) {
        copy_v3_v3(pchan->pose_mat[3], vec);
      }
    }
  }

  /* calculate head */
  copy_v3_v3(pchan->pose_head, pchan->pose_mat[3]);
  /* calculate tail */
  BKE_pose_where_is_bone_tail(pchan);
}

/* This only reads anim data from channels, and writes to channels */
/* This is the only function adding poses */
void BKE_pose_where_is(struct Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  bArmature *arm;
  Bone *bone;
  bPoseChannel *pchan;
  float imat[4][4];
  float ctime;

  if (ob->type != OB_ARMATURE) {
    return;
  }
  arm = ob->data;

  if (ELEM(NULL, arm, scene)) {
    return;
  }
  /* WARNING! passing NULL bmain here means we won't tag depsgraph's as dirty -
   * hopefully this is OK. */
  BKE_pose_ensure(NULL, ob, arm, true);

  ctime = BKE_scene_ctime_get(scene); /* not accurate... */

  /* In edit-mode or rest-position we read the data from the bones. */
  if (arm->edbo || (arm->flag & ARM_RESTPOS)) {
    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      bone = pchan->bone;
      if (bone) {
        copy_m4_m4(pchan->pose_mat, bone->arm_mat);
        copy_v3_v3(pchan->pose_head, bone->arm_head);
        copy_v3_v3(pchan->pose_tail, bone->arm_tail);
      }
    }
  }
  else {
    invert_m4_m4(ob->imat, ob->obmat); /* imat is needed */

    /* 1. clear flags */
    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      pchan->flag &= ~(POSE_DONE | POSE_CHAIN | POSE_IKTREE | POSE_IKSPLINE);
    }

    /* 2a. construct the IK tree (standard IK) */
    BIK_init_tree(depsgraph, scene, ob, ctime);

    /* 2b. construct the Spline IK trees
     * - this is not integrated as an IK plugin, since it should be able
     *   to function in conjunction with standard IK
     */
    BKE_pose_splineik_init_tree(scene, ob, ctime);

    /* 3. the main loop, channels are already hierarchical sorted from root to children */
    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      /* 4a. if we find an IK root, we handle it separated */
      if (pchan->flag & POSE_IKTREE) {
        BIK_execute_tree(depsgraph, scene, ob, pchan, ctime);
      }
      /* 4b. if we find a Spline IK root, we handle it separated too */
      else if (pchan->flag & POSE_IKSPLINE) {
        BKE_splineik_execute_tree(depsgraph, scene, ob, pchan, ctime);
      }
      /* 5. otherwise just call the normal solver */
      else if (!(pchan->flag & POSE_DONE)) {
        BKE_pose_where_is_bone(depsgraph, scene, ob, pchan, ctime, 1);
      }
    }
    /* 6. release the IK tree */
    BIK_release_tree(scene, ob, ctime);
  }

  /* calculating deform matrices */
  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    if (pchan->bone) {
      invert_m4_m4(imat, pchan->bone->arm_mat);
      mul_m4_m4m4(pchan->chan_mat, pchan->pose_mat, imat);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calculate Bounding Box (Armature & Pose)
 * \{ */

static int minmax_armature(Object *ob, float r_min[3], float r_max[3])
{
  bPoseChannel *pchan;

  /* For now, we assume BKE_pose_where_is has already been called
   * (hence we have valid data in pachan). */
  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    minmax_v3v3_v3(r_min, r_max, pchan->pose_head);
    minmax_v3v3_v3(r_min, r_max, pchan->pose_tail);
  }

  return (BLI_listbase_is_empty(&ob->pose->chanbase) == false);
}

static void boundbox_armature(Object *ob)
{
  BoundBox *bb;
  float min[3], max[3];

  if (ob->runtime.bb == NULL) {
    ob->runtime.bb = MEM_callocN(sizeof(BoundBox), "Armature boundbox");
  }
  bb = ob->runtime.bb;

  INIT_MINMAX(min, max);
  if (!minmax_armature(ob, min, max)) {
    min[0] = min[1] = min[2] = -1.0f;
    max[0] = max[1] = max[2] = 1.0f;
  }

  BKE_boundbox_init_from_minmax(bb, min, max);

  bb->flag &= ~BOUNDBOX_DIRTY;
}

BoundBox *BKE_armature_boundbox_get(Object *ob)
{
  boundbox_armature(ob);

  return ob->runtime.bb;
}

bool BKE_pose_minmax(Object *ob, float r_min[3], float r_max[3], bool use_hidden, bool use_select)
{
  bool changed = false;

  if (ob->pose) {
    bArmature *arm = ob->data;
    bPoseChannel *pchan;

    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      /* XXX pchan->bone may be NULL for duplicated bones, see duplicateEditBoneObjects() comment
       *     (editarmature.c:2592)... Skip in this case too! */
      if (pchan->bone && (!((use_hidden == false) && (PBONE_VISIBLE(arm, pchan->bone) == false)) &&
                          !((use_select == true) && ((pchan->bone->flag & BONE_SELECTED) == 0)))) {
        bPoseChannel *pchan_tx = (pchan->custom && pchan->custom_tx) ? pchan->custom_tx : pchan;
        BoundBox *bb_custom = ((pchan->custom) && !(arm->flag & ARM_NO_CUSTOM)) ?
                                  BKE_object_boundbox_get(pchan->custom) :
                                  NULL;
        if (bb_custom) {
          float mat[4][4], smat[4][4], rmat[4][4], tmp[4][4];
          scale_m4_fl(smat, PCHAN_CUSTOM_BONE_LENGTH(pchan));
          rescale_m4(smat, pchan->custom_scale_xyz);
          eulO_to_mat4(rmat, pchan->custom_rotation_euler, ROT_MODE_XYZ);
          copy_m4_m4(tmp, pchan_tx->pose_mat);
          translate_m4(tmp,
                       pchan->custom_translation[0],
                       pchan->custom_translation[1],
                       pchan->custom_translation[2]);
          mul_m4_series(mat, ob->obmat, tmp, rmat, smat);
          BKE_boundbox_minmax(bb_custom, mat, r_min, r_max);
        }
        else {
          float vec[3];
          mul_v3_m4v3(vec, ob->obmat, pchan_tx->pose_head);
          minmax_v3v3_v3(r_min, r_max, vec);
          mul_v3_m4v3(vec, ob->obmat, pchan_tx->pose_tail);
          minmax_v3v3_v3(r_min, r_max, vec);
        }

        changed = true;
      }
    }
  }

  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Graph Evaluation
 * \{ */

bPoseChannel *BKE_armature_ik_solver_find_root(bPoseChannel *pchan, bKinematicConstraint *data)
{
  bPoseChannel *rootchan = pchan;
  if (!(data->flag & CONSTRAINT_IK_TIP)) {
    /* Exclude tip from chain. */
    rootchan = rootchan->parent;
  }
  if (rootchan != NULL) {
    int segcount = 0;
    while (rootchan->parent) {
      /* Continue up chain, until we reach target number of items. */
      segcount++;
      if (segcount == data->rootbone) {
        break;
      }
      rootchan = rootchan->parent;
    }
  }
  return rootchan;
}

bPoseChannel *BKE_armature_splineik_solver_find_root(bPoseChannel *pchan,
                                                     bSplineIKConstraint *data)
{
  bPoseChannel *rootchan = pchan;
  int segcount = 0;
  BLI_assert(rootchan != NULL);
  while (rootchan->parent) {
    /* Continue up chain, until we reach target number of items. */
    segcount++;
    if (segcount == data->chainlen) {
      break;
    }
    rootchan = rootchan->parent;
  }
  return rootchan;
}

/** \} */
