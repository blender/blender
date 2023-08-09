/* SPDX-FileCopyrightText: 2009 Blender Foundation, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_nla.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_texture.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_path.h"
#include "RNA_prototypes.h"

#include "BLO_read_write.h"

#include "nla_private.h"

#include "atomic_ops.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.anim_sys"};

/* *********************************** */
/* KeyingSet API */

/* Finding Tools --------------------------- */

KS_Path *BKE_keyingset_find_path(KeyingSet *ks,
                                 ID *id,
                                 const char group_name[],
                                 const char rna_path[],
                                 int array_index,
                                 int /*group_mode*/)
{
  /* sanity checks */
  if (ELEM(nullptr, ks, rna_path, id)) {
    return nullptr;
  }

  /* loop over paths in the current KeyingSet, finding the first one where all settings match
   * (i.e. the first one where none of the checks fail and equal 0)
   */
  LISTBASE_FOREACH (KS_Path *, ksp, &ks->paths) {
    short eq_id = 1, eq_path = 1, eq_index = 1, eq_group = 1;

    /* id */
    if (id != ksp->id) {
      eq_id = 0;
    }

    /* path */
    if ((ksp->rna_path == nullptr) || !STREQ(rna_path, ksp->rna_path)) {
      eq_path = 0;
    }

    /* index - need to compare whole-array setting too... */
    if (ksp->array_index != array_index) {
      eq_index = 0;
    }

    /* group */
    if (group_name) {
      /* FIXME: these checks need to be coded... for now, it's not too important though */
    }

    /* if all aspects are ok, return */
    if (eq_id && eq_path && eq_index && eq_group) {
      return ksp;
    }
  }

  /* none found */
  return nullptr;
}

/* Defining Tools --------------------------- */

KeyingSet *BKE_keyingset_add(
    ListBase *list, const char idname[], const char name[], short flag, short keyingflag)
{
  KeyingSet *ks;

  /* allocate new KeyingSet */
  ks = static_cast<KeyingSet *>(MEM_callocN(sizeof(KeyingSet), "KeyingSet"));

  STRNCPY_UTF8(ks->idname, (idname) ? idname : (name) ? name : DATA_("KeyingSet"));
  STRNCPY_UTF8(ks->name, (name) ? name : (idname) ? idname : DATA_("Keying Set"));

  ks->flag = flag;
  ks->keyingflag = keyingflag;
  /* NOTE: assume that if one is set one way, the other should be too, so that it'll work */
  ks->keyingoverride = keyingflag;

  /* add KeyingSet to list */
  BLI_addtail(list, ks);

  /* Make sure KeyingSet has a unique idname */
  BLI_uniquename(
      list, ks, DATA_("KeyingSet"), '.', offsetof(KeyingSet, idname), sizeof(ks->idname));

  /* Make sure KeyingSet has a unique label (this helps with identification) */
  BLI_uniquename(list, ks, DATA_("Keying Set"), '.', offsetof(KeyingSet, name), sizeof(ks->name));

  /* return new KeyingSet for further editing */
  return ks;
}

KS_Path *BKE_keyingset_add_path(KeyingSet *ks,
                                ID *id,
                                const char group_name[],
                                const char rna_path[],
                                int array_index,
                                short flag,
                                short groupmode)
{
  KS_Path *ksp;

  /* sanity checks */
  if (ELEM(nullptr, ks, rna_path)) {
    CLOG_ERROR(&LOG, "no Keying Set and/or RNA Path to add path with");
    return nullptr;
  }

  /* ID is required for all types of KeyingSets */
  if (id == nullptr) {
    CLOG_ERROR(&LOG, "No ID provided for Keying Set Path");
    return nullptr;
  }

  /* don't add if there is already a matching KS_Path in the KeyingSet */
  if (BKE_keyingset_find_path(ks, id, group_name, rna_path, array_index, groupmode)) {
    if (G.debug & G_DEBUG) {
      CLOG_ERROR(&LOG, "destination already exists in Keying Set");
    }
    return nullptr;
  }

  /* allocate a new KeyingSet Path */
  ksp = static_cast<KS_Path *>(MEM_callocN(sizeof(KS_Path), "KeyingSet Path"));

  /* just store absolute info */
  ksp->id = id;
  if (group_name) {
    STRNCPY(ksp->group, group_name);
  }
  else {
    ksp->group[0] = '\0';
  }

  /* store additional info for relative paths (just in case user makes the set relative) */
  if (id) {
    ksp->idtype = GS(id->name);
  }

  /* just copy path info */
  /* TODO: should array index be checked too? */
  ksp->rna_path = BLI_strdup(rna_path);
  ksp->array_index = array_index;

  /* store flags */
  ksp->flag = flag;
  ksp->groupmode = groupmode;

  /* add KeyingSet path to KeyingSet */
  BLI_addtail(&ks->paths, ksp);

  /* return this path */
  return ksp;
}

void BKE_keyingset_free_path(KeyingSet *ks, KS_Path *ksp)
{
  /* sanity check */
  if (ELEM(nullptr, ks, ksp)) {
    return;
  }

  /* free RNA-path info */
  if (ksp->rna_path) {
    MEM_freeN(ksp->rna_path);
  }

  /* free path itself */
  BLI_freelinkN(&ks->paths, ksp);
}

void BKE_keyingsets_copy(ListBase *newlist, const ListBase *list)
{
  BLI_duplicatelist(newlist, list);

  LISTBASE_FOREACH (KeyingSet *, ksn, newlist) {
    BLI_duplicatelist(&ksn->paths, &ksn->paths);

    LISTBASE_FOREACH (KS_Path *, kspn, &ksn->paths) {
      kspn->rna_path = static_cast<char *>(MEM_dupallocN(kspn->rna_path));
    }
  }
}

void BKE_keyingsets_foreach_id(LibraryForeachIDData *data, const ListBase *keyingsets)
{
  LISTBASE_FOREACH (KeyingSet *, ksn, keyingsets) {
    LISTBASE_FOREACH (KS_Path *, kspn, &ksn->paths) {
      BKE_LIB_FOREACHID_PROCESS_ID(data, kspn->id, IDWALK_CB_NOP);
    }
  }
}

/* Freeing Tools --------------------------- */

void BKE_keyingset_free(KeyingSet *ks)
{
  KS_Path *ksp, *kspn;

  /* sanity check */
  if (ks == nullptr) {
    return;
  }

  /* free each path as we go to avoid looping twice */
  for (ksp = static_cast<KS_Path *>(ks->paths.first); ksp; ksp = kspn) {
    kspn = ksp->next;
    BKE_keyingset_free_path(ks, ksp);
  }
}

void BKE_keyingsets_free(ListBase *list)
{
  KeyingSet *ks, *ksn;

  /* sanity check */
  if (list == nullptr) {
    return;
  }

  /* loop over KeyingSets freeing them
   * - BKE_keyingset_free() doesn't free the set itself, but it frees its sub-data
   */
  for (ks = static_cast<KeyingSet *>(list->first); ks; ks = ksn) {
    ksn = ks->next;
    BKE_keyingset_free(ks);
    BLI_freelinkN(list, ks);
  }
}

void BKE_keyingsets_blend_write(BlendWriter *writer, ListBase *list)
{
  LISTBASE_FOREACH (KeyingSet *, ks, list) {
    /* KeyingSet */
    BLO_write_struct(writer, KeyingSet, ks);

    /* Paths */
    LISTBASE_FOREACH (KS_Path *, ksp, &ks->paths) {
      /* Path */
      BLO_write_struct(writer, KS_Path, ksp);

      if (ksp->rna_path) {
        BLO_write_string(writer, ksp->rna_path);
      }
    }
  }
}

void BKE_keyingsets_blend_read_data(BlendDataReader *reader, ListBase *list)
{
  LISTBASE_FOREACH (KeyingSet *, ks, list) {
    /* paths */
    BLO_read_list(reader, &ks->paths);

    LISTBASE_FOREACH (KS_Path *, ksp, &ks->paths) {
      /* rna path */
      BLO_read_data_address(reader, &ksp->rna_path);
    }
  }
}

void BKE_keyingsets_blend_read_lib(BlendLibReader *reader, ID *id, ListBase *list)
{
  LISTBASE_FOREACH (KeyingSet *, ks, list) {
    LISTBASE_FOREACH (KS_Path *, ksp, &ks->paths) {
      BLO_read_id_address(reader, id, &ksp->id);
    }
  }
}

void BKE_keyingsets_blend_read_expand(BlendExpander *expander, ListBase *list)
{
  LISTBASE_FOREACH (KeyingSet *, ks, list) {
    LISTBASE_FOREACH (KS_Path *, ksp, &ks->paths) {
      BLO_expand(expander, ksp->id);
    }
  }
}

/* ***************************************** */
/* Evaluation Data-Setting Backend */

static bool is_fcurve_evaluatable(FCurve *fcu)
{
  if (fcu->flag & (FCURVE_MUTED | FCURVE_DISABLED)) {
    return false;
  }
  if (fcu->grp != nullptr && (fcu->grp->flag & AGRP_MUTED)) {
    return false;
  }
  if (BKE_fcurve_is_empty(fcu)) {
    return false;
  }
  return true;
}

bool BKE_animsys_rna_path_resolve(
    PointerRNA *ptr, /* typically 'fcu->rna_path', 'fcu->array_index' */
    const char *rna_path,
    const int array_index,
    PathResolvedRNA *r_result)
{
  if (rna_path == nullptr) {
    return false;
  }

  const char *path = rna_path;
  if (!RNA_path_resolve_property(ptr, path, &r_result->ptr, &r_result->prop)) {
    /* failed to get path */
    /* XXX don't tag as failed yet though, as there are some legit situations (Action Constraint)
     * where some channels will not exist, but shouldn't lock up Action */
    if (G.debug & G_DEBUG) {
      CLOG_WARN(&LOG,
                "Animato: Invalid path. ID = '%s',  '%s[%d]'",
                (ptr->owner_id) ? (ptr->owner_id->name + 2) : "<No ID>",
                path,
                array_index);
    }
    return false;
  }

  if (ptr->owner_id != nullptr && !RNA_property_animateable(&r_result->ptr, r_result->prop)) {
    return false;
  }

  int array_len = RNA_property_array_length(&r_result->ptr, r_result->prop);
  if (array_len && array_index >= array_len) {
    if (G.debug & G_DEBUG) {
      CLOG_WARN(&LOG,
                "Animato: Invalid array index. ID = '%s',  '%s[%d]', array length is %d",
                (ptr->owner_id) ? (ptr->owner_id->name + 2) : "<No ID>",
                path,
                array_index,
                array_len - 1);
    }
    return false;
  }

  r_result->prop_index = array_len ? array_index : -1;
  return true;
}

/* less than 1.0 evaluates to false, use epsilon to avoid float error */
#define ANIMSYS_FLOAT_AS_BOOL(value) ((value) > (1.0f - FLT_EPSILON))

bool BKE_animsys_read_from_rna_path(PathResolvedRNA *anim_rna, float *r_value)
{
  PropertyRNA *prop = anim_rna->prop;
  PointerRNA *ptr = &anim_rna->ptr;
  int array_index = anim_rna->prop_index;
  float orig_value;

  /* caller must ensure this is animatable */
  BLI_assert(RNA_property_animateable(ptr, prop) || ptr->owner_id == nullptr);

  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN: {
      if (array_index != -1) {
        const int orig_value_coerce = RNA_property_boolean_get_index(ptr, prop, array_index);
        orig_value = float(orig_value_coerce);
      }
      else {
        const int orig_value_coerce = RNA_property_boolean_get(ptr, prop);
        orig_value = float(orig_value_coerce);
      }
      break;
    }
    case PROP_INT: {
      if (array_index != -1) {
        const int orig_value_coerce = RNA_property_int_get_index(ptr, prop, array_index);
        orig_value = float(orig_value_coerce);
      }
      else {
        const int orig_value_coerce = RNA_property_int_get(ptr, prop);
        orig_value = float(orig_value_coerce);
      }
      break;
    }
    case PROP_FLOAT: {
      if (array_index != -1) {
        const float orig_value_coerce = RNA_property_float_get_index(ptr, prop, array_index);
        orig_value = float(orig_value_coerce);
      }
      else {
        const float orig_value_coerce = RNA_property_float_get(ptr, prop);
        orig_value = float(orig_value_coerce);
      }
      break;
    }
    case PROP_ENUM: {
      const int orig_value_coerce = RNA_property_enum_get(ptr, prop);
      orig_value = float(orig_value_coerce);
      break;
    }
    default: /* nothing can be done here... so it is unsuccessful? */
      return false;
  }

  if (r_value != nullptr) {
    *r_value = orig_value;
  }

  /* successful */
  return true;
}

bool BKE_animsys_write_to_rna_path(PathResolvedRNA *anim_rna, const float value)
{
  PropertyRNA *prop = anim_rna->prop;
  PointerRNA *ptr = &anim_rna->ptr;
  int array_index = anim_rna->prop_index;

  /* caller must ensure this is animatable */
  BLI_assert(RNA_property_animateable(ptr, prop) || ptr->owner_id == nullptr);

  /* Check whether value is new. Otherwise we skip all the updates. */
  float old_value;
  if (!BKE_animsys_read_from_rna_path(anim_rna, &old_value)) {
    return false;
  }
  if (old_value == value) {
    return true;
  }

  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN: {
      const int value_coerce = ANIMSYS_FLOAT_AS_BOOL(value);
      if (array_index != -1) {
        RNA_property_boolean_set_index(ptr, prop, array_index, value_coerce);
      }
      else {
        RNA_property_boolean_set(ptr, prop, value_coerce);
      }
      break;
    }
    case PROP_INT: {
      int value_coerce = int(value);
      RNA_property_int_clamp(ptr, prop, &value_coerce);
      if (array_index != -1) {
        RNA_property_int_set_index(ptr, prop, array_index, value_coerce);
      }
      else {
        RNA_property_int_set(ptr, prop, value_coerce);
      }
      break;
    }
    case PROP_FLOAT: {
      float value_coerce = value;
      RNA_property_float_clamp(ptr, prop, &value_coerce);
      if (array_index != -1) {
        RNA_property_float_set_index(ptr, prop, array_index, value_coerce);
      }
      else {
        RNA_property_float_set(ptr, prop, value_coerce);
      }
      break;
    }
    case PROP_ENUM: {
      const int value_coerce = int(value);
      RNA_property_enum_set(ptr, prop, value_coerce);
      break;
    }
    default: /* nothing can be done here... so it is unsuccessful? */
      return false;
  }

  /* successful */
  return true;
}

static bool animsys_construct_orig_pointer_rna(const PointerRNA *ptr, PointerRNA *ptr_orig)
{
  *ptr_orig = *ptr;
  /* NOTE: nlastrip_evaluate_controls() creates PointerRNA with ID of nullptr. Technically, this is
   * not a valid pointer, but there are exceptions in various places of this file which handles
   * such pointers.
   * We do special trickery here as well, to quickly go from evaluated to original NlaStrip. */
  if (ptr->owner_id == nullptr) {
    if (ptr->type != &RNA_NlaStrip) {
      return false;
    }
    NlaStrip *strip = ((NlaStrip *)ptr_orig->data);
    if (strip->orig_strip == nullptr) {
      return false;
    }
    ptr_orig->data = strip->orig_strip;
  }
  else {
    ptr_orig->owner_id = ptr_orig->owner_id->orig_id;
    ptr_orig->data = ptr_orig->owner_id;
  }
  return true;
}

static void animsys_write_orig_anim_rna(PointerRNA *ptr,
                                        const char *rna_path,
                                        int array_index,
                                        float value)
{
  PointerRNA ptr_orig;
  if (!animsys_construct_orig_pointer_rna(ptr, &ptr_orig)) {
    return;
  }
  PathResolvedRNA orig_anim_rna;
  /* TODO(sergey): Should be possible to cache resolved path in dependency graph somehow. */
  if (BKE_animsys_rna_path_resolve(&ptr_orig, rna_path, array_index, &orig_anim_rna)) {
    BKE_animsys_write_to_rna_path(&orig_anim_rna, value);
  }
}

/**
 * Evaluate all the F-Curves in the given list
 * This performs a set of standard checks. If extra checks are required,
 * separate code should be used.
 */
static void animsys_evaluate_fcurves(PointerRNA *ptr,
                                     ListBase *list,
                                     const AnimationEvalContext *anim_eval_context,
                                     bool flush_to_original)
{
  /* Calculate then execute each curve. */
  LISTBASE_FOREACH (FCurve *, fcu, list) {

    if (!is_fcurve_evaluatable(fcu)) {
      continue;
    }

    PathResolvedRNA anim_rna;
    if (BKE_animsys_rna_path_resolve(ptr, fcu->rna_path, fcu->array_index, &anim_rna)) {
      const float curval = calculate_fcurve(&anim_rna, fcu, anim_eval_context);
      BKE_animsys_write_to_rna_path(&anim_rna, curval);
      if (flush_to_original) {
        animsys_write_orig_anim_rna(ptr, fcu->rna_path, fcu->array_index, curval);
      }
    }
  }
}

/**
 * This function assumes that the quaternion keys are sequential. They do not
 * have to be in array_index order. If the quaternion is only partially keyed,
 * the result is normalized. If it is fully keyed, the result is returned as-is.
 *
 * \return the number of FCurves used to construct this quaternion. This is so
 * that the caller knows how many FCurves can be skipped while iterating over
 * them. */
static int animsys_quaternion_evaluate_fcurves(PathResolvedRNA quat_rna,
                                               FCurve *first_fcurve,
                                               const AnimationEvalContext *anim_eval_context,
                                               float r_quaternion[4])
{
  FCurve *quat_curve_fcu = first_fcurve;

  /* Initialize r_quaternion to the unit quaternion so that half-keyed quaternions at least have
   * *some* value in there. */
  r_quaternion[0] = 1.0f;
  r_quaternion[1] = 0.0f;
  r_quaternion[2] = 0.0f;
  r_quaternion[3] = 0.0f;

  int fcurve_offset = 0;
  for (; fcurve_offset < 4 && quat_curve_fcu;
       ++fcurve_offset, quat_curve_fcu = quat_curve_fcu->next) {
    if (!STREQ(quat_curve_fcu->rna_path, first_fcurve->rna_path)) {
      /* This should never happen when the quaternion is fully keyed. Some
       * people do use half-keyed quaternions, though, so better to check. */
      break;
    }

    const int array_index = quat_curve_fcu->array_index;
    quat_rna.prop_index = array_index;
    r_quaternion[array_index] = calculate_fcurve(&quat_rna, quat_curve_fcu, anim_eval_context);
  }

  if (fcurve_offset < 4) {
    /* This quaternion was incompletely keyed, so the result is a mixture of the unit quaternion
     * and values from FCurves. This means that it's almost certainly no longer of unit length. */
    normalize_qt(r_quaternion);
  }

  return fcurve_offset;
}

/**
 * This function assumes that the quaternion keys are sequential. They do not
 * have to be in array_index order.
 *
 * \return the number of FCurves used to construct the quaternion, counting from
 * `first_fcurve`. This is so that the caller knows how many FCurves can be
 * skipped while iterating over them. */
static int animsys_blend_fcurves_quaternion(PathResolvedRNA *anim_rna,
                                            FCurve *first_fcurve,
                                            const AnimationEvalContext *anim_eval_context,
                                            const float blend_factor)
{
  float current_quat[4];
  RNA_property_float_get_array(&anim_rna->ptr, anim_rna->prop, current_quat);

  float target_quat[4];
  const int num_fcurves_read = animsys_quaternion_evaluate_fcurves(
      *anim_rna, first_fcurve, anim_eval_context, target_quat);

  float blended_quat[4];
  interp_qt_qtqt(blended_quat, current_quat, target_quat, blend_factor);

  RNA_property_float_set_array(&anim_rna->ptr, anim_rna->prop, blended_quat);

  return num_fcurves_read;
}

/* LERP between current value (blend_factor=0.0) and the value from the FCurve (blend_factor=1.0)
 */
static void animsys_blend_in_fcurves(PointerRNA *ptr,
                                     ListBase *fcurves,
                                     const AnimationEvalContext *anim_eval_context,
                                     const float blend_factor)
{
  char *channel_to_skip = nullptr;
  int num_channels_to_skip = 0;
  LISTBASE_FOREACH (FCurve *, fcu, fcurves) {

    if (num_channels_to_skip) {
      /* For skipping already-handled rotation channels. Rotation channels are handled per group,
       * and not per individual channel. */
      BLI_assert(channel_to_skip != nullptr);
      if (STREQ(channel_to_skip, fcu->rna_path)) {
        /* This is indeed the channel we want to skip. */
        num_channels_to_skip--;
        continue;
      }
    }

    if (!is_fcurve_evaluatable(fcu)) {
      continue;
    }

    PathResolvedRNA anim_rna;
    if (!BKE_animsys_rna_path_resolve(ptr, fcu->rna_path, fcu->array_index, &anim_rna)) {
      continue;
    }

    if (STREQ(RNA_property_identifier(anim_rna.prop), "rotation_quaternion")) {
      const int num_fcurves_read = animsys_blend_fcurves_quaternion(
          &anim_rna, fcu, anim_eval_context, blend_factor);

      /* Skip the next three channels, because those have already been handled here. */
      MEM_SAFE_FREE(channel_to_skip);
      channel_to_skip = BLI_strdup(fcu->rna_path);
      num_channels_to_skip = num_fcurves_read - 1;
      continue;
    }
    /* TODO(Sybren): do something similar as above for Euler and Axis/Angle representations. */

    const float fcurve_value = calculate_fcurve(&anim_rna, fcu, anim_eval_context);

    float current_value;
    float value_to_write;
    if (BKE_animsys_read_from_rna_path(&anim_rna, &current_value)) {
      value_to_write = (1 - blend_factor) * current_value + blend_factor * fcurve_value;

      switch (RNA_property_type(anim_rna.prop)) {
        case PROP_BOOLEAN: /* Without this, anything less than 1.0 is converted to 'False' by
                            * ANIMSYS_FLOAT_AS_BOOL(). This is probably not desirable for blends,
                            * where anything
                            * above a 50% blend should act more like the FCurve than like the
                            * current value. */
        case PROP_INT:
        case PROP_ENUM:
          value_to_write = roundf(value_to_write);
          break;
          /* All other types are just handled as float, and value_to_write is already correct. */
        default:
          break;
      }
    }
    else {
      /* Unable to read the current value for blending, so just apply the FCurve value instead. */
      value_to_write = fcurve_value;
    }

    BKE_animsys_write_to_rna_path(&anim_rna, value_to_write);
  }

  MEM_SAFE_FREE(channel_to_skip);
}

/* ***************************************** */
/* Driver Evaluation */

AnimationEvalContext BKE_animsys_eval_context_construct(Depsgraph *depsgraph, float eval_time)
{
  AnimationEvalContext ctx{};
  ctx.depsgraph = depsgraph;
  ctx.eval_time = eval_time;
  return ctx;
}

AnimationEvalContext BKE_animsys_eval_context_construct_at(
    const AnimationEvalContext *anim_eval_context, float eval_time)
{
  return BKE_animsys_eval_context_construct(anim_eval_context->depsgraph, eval_time);
}

/* Evaluate Drivers */
static void animsys_evaluate_drivers(PointerRNA *ptr,
                                     AnimData *adt,
                                     const AnimationEvalContext *anim_eval_context)
{
  /* drivers are stored as F-Curves, but we cannot use the standard code, as we need to check if
   * the depsgraph requested that this driver be evaluated...
   */
  LISTBASE_FOREACH (FCurve *, fcu, &adt->drivers) {
    ChannelDriver *driver = fcu->driver;
    bool ok = false;

    /* check if this driver's curve should be skipped */
    if ((fcu->flag & (FCURVE_MUTED | FCURVE_DISABLED)) == 0) {
      /* check if driver itself is tagged for recalculation */
      /* XXX driver recalc flag is not set yet by depsgraph! */
      if ((driver) && !(driver->flag & DRIVER_FLAG_INVALID)) {
        /* evaluate this using values set already in other places
         * NOTE: for 'layering' option later on, we should check if we should remove old value
         * before adding new to only be done when drivers only changed. */
        PathResolvedRNA anim_rna;
        if (BKE_animsys_rna_path_resolve(ptr, fcu->rna_path, fcu->array_index, &anim_rna)) {
          const float curval = calculate_fcurve(&anim_rna, fcu, anim_eval_context);
          ok = BKE_animsys_write_to_rna_path(&anim_rna, curval);
        }

        /* set error-flag if evaluation failed */
        if (ok == 0) {
          driver->flag |= DRIVER_FLAG_INVALID;
        }
      }
    }
  }
}

/* ***************************************** */
/* Actions Evaluation */

/* strictly not necessary for actual "evaluation", but it is a useful safety check
 * to reduce the amount of times that users end up having to "revive" wrongly-assigned
 * actions
 */
static void action_idcode_patch_check(ID *id, bAction *act)
{
  int idcode = 0;

  /* just in case */
  if (ELEM(nullptr, id, act)) {
    return;
  }

  idcode = GS(id->name);

  /* the actual checks... hopefully not too much of a performance hit in the long run... */
  if (act->idroot == 0) {
    /* use the current root if not set already
     * (i.e. newly created actions and actions from 2.50-2.57 builds).
     * - this has problems if there are 2 users, and the first one encountered is the invalid one
     *   in which case, the user will need to manually fix this (?)
     */
    act->idroot = idcode;
  }
  else if (act->idroot != idcode) {
    /* only report this error if debug mode is enabled (to save performance everywhere else) */
    if (G.debug & G_DEBUG) {
      printf(
          "AnimSys Safety Check Failed: Action '%s' is not meant to be used from ID-Blocks of "
          "type %d such as '%s'\n",
          act->id.name + 2,
          idcode,
          id->name);
    }
  }
}

/* ----------------------------------------- */

void animsys_evaluate_action_group(PointerRNA *ptr,
                                   bAction *act,
                                   bActionGroup *agrp,
                                   const AnimationEvalContext *anim_eval_context)
{
  FCurve *fcu;

  /* check if mapper is appropriate for use here (we set to nullptr if it's inappropriate) */
  if (ELEM(nullptr, act, agrp)) {
    return;
  }

  action_idcode_patch_check(ptr->owner_id, act);

  /* if group is muted, don't evaluated any of the F-Curve */
  if (agrp->flag & AGRP_MUTED) {
    return;
  }

  /* calculate then execute each curve */
  for (fcu = static_cast<FCurve *>(agrp->channels.first); (fcu) && (fcu->grp == agrp);
       fcu = fcu->next)
  {
    /* check if this curve should be skipped */
    if ((fcu->flag & (FCURVE_MUTED | FCURVE_DISABLED)) == 0 && !BKE_fcurve_is_empty(fcu)) {
      PathResolvedRNA anim_rna;
      if (BKE_animsys_rna_path_resolve(ptr, fcu->rna_path, fcu->array_index, &anim_rna)) {
        const float curval = calculate_fcurve(&anim_rna, fcu, anim_eval_context);
        BKE_animsys_write_to_rna_path(&anim_rna, curval);
      }
    }
  }
}

void animsys_evaluate_action(PointerRNA *ptr,
                             bAction *act,
                             const AnimationEvalContext *anim_eval_context,
                             const bool flush_to_original)
{
  /* check if mapper is appropriate for use here (we set to nullptr if it's inappropriate) */
  if (act == nullptr) {
    return;
  }

  action_idcode_patch_check(ptr->owner_id, act);

  /* calculate then execute each curve */
  animsys_evaluate_fcurves(ptr, &act->curves, anim_eval_context, flush_to_original);
}

void animsys_blend_in_action(PointerRNA *ptr,
                             bAction *act,
                             const AnimationEvalContext *anim_eval_context,
                             const float blend_factor)
{
  action_idcode_patch_check(ptr->owner_id, act);
  animsys_blend_in_fcurves(ptr, &act->curves, anim_eval_context, blend_factor);
}

/* ***************************************** */
/* NLA System - Evaluation */

/* calculate influence of strip based for given frame based on blendin/out values */
static float nlastrip_get_influence(NlaStrip *strip, float cframe)
{
  /* sanity checks - normalize the blendin/out values? */
  strip->blendin = fabsf(strip->blendin);
  strip->blendout = fabsf(strip->blendout);

  /* result depends on where frame is in respect to blendin/out values */
  if (IS_EQF(strip->blendin, 0.0f) == false && (cframe <= (strip->start + strip->blendin))) {
    /* there is some blend-in */
    return fabsf(cframe - strip->start) / (strip->blendin);
  }
  if (IS_EQF(strip->blendout, 0.0f) == false && (cframe >= (strip->end - strip->blendout))) {
    /* there is some blend-out */
    return fabsf(strip->end - cframe) / (strip->blendout);
  }

  /* in the middle of the strip, we should be full strength */
  return 1.0f;
}

/* evaluate the evaluation time and influence for the strip, storing the results in the strip */
static void nlastrip_evaluate_controls(NlaStrip *strip,
                                       const AnimationEvalContext *anim_eval_context,
                                       const bool flush_to_original)
{
  /* now strip's evaluate F-Curves for these settings (if applicable) */
  if (strip->fcurves.first) {
    PointerRNA strip_ptr;

    /* create RNA-pointer needed to set values */
    RNA_pointer_create(nullptr, &RNA_NlaStrip, strip, &strip_ptr);

    /* execute these settings as per normal */
    animsys_evaluate_fcurves(&strip_ptr, &strip->fcurves, anim_eval_context, flush_to_original);
  }

  /* analytically generate values for influence and time (if applicable)
   * - we do this after the F-Curves have been evaluated to override the effects of those
   *   in case the override has been turned off.
   */
  if ((strip->flag & NLASTRIP_FLAG_USR_INFLUENCE) == 0) {
    strip->influence = nlastrip_get_influence(strip, anim_eval_context->eval_time);
  }

  /* Bypass evaluation time computation if time mapping is disabled. */
  if ((strip->flag & NLASTRIP_FLAG_NO_TIME_MAP) != 0) {
    strip->strip_time = anim_eval_context->eval_time;
    return;
  }

  if ((strip->flag & NLASTRIP_FLAG_USR_TIME) == 0) {
    strip->strip_time = nlastrip_get_frame(
        strip, anim_eval_context->eval_time, NLATIME_CONVERT_EVAL);
  }

  /* if user can control the evaluation time (using F-Curves), consider the option which allows
   * this time to be clamped to lie within extents of the action-clip, so that a steady changing
   * rate of progress through several cycles of the clip can be achieved easily.
   */
  /* NOTE: if we add any more of these special cases, we better group them up nicely... */
  if ((strip->flag & NLASTRIP_FLAG_USR_TIME) && (strip->flag & NLASTRIP_FLAG_USR_TIME_CYCLIC)) {
    strip->strip_time = fmod(strip->strip_time - strip->actstart, strip->actend - strip->actstart);
  }
}

NlaEvalStrip *nlastrips_ctime_get_strip(ListBase *list,
                                        ListBase *strips,
                                        short index,
                                        const AnimationEvalContext *anim_eval_context,
                                        const bool flush_to_original)
{
  NlaStrip *estrip = nullptr;
  NlaEvalStrip *nes;
  short side = 0;
  float ctime = anim_eval_context->eval_time;

  /* loop over strips, checking if they fall within the range */
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    /* Check if current time occurs within this strip. */
    if (IN_RANGE_INCL(ctime, strip->start, strip->end) ||
        (strip->flag & NLASTRIP_FLAG_NO_TIME_MAP)) {
      /* this strip is active, so try to use it */
      estrip = strip;
      side = NES_TIME_WITHIN;
      break;
    }

    /* if time occurred before current strip... */
    if (ctime < strip->start) {
      if (strip == strips->first) {
        /* before first strip - only try to use it if it extends backwards in time too */
        if (strip->extendmode == NLASTRIP_EXTEND_HOLD) {
          estrip = strip;
        }

        /* side is 'before' regardless of whether there's a useful strip */
        side = NES_TIME_BEFORE;
      }
      else {
        /* before next strip - previous strip has ended, but next hasn't begun,
         * so blending mode depends on whether strip is being held or not...
         * - only occurs when no transition strip added, otherwise the transition would have
         *   been picked up above...
         */
        strip = strip->prev;

        if (strip->extendmode != NLASTRIP_EXTEND_NOTHING) {
          estrip = strip;
        }
        side = NES_TIME_AFTER;
      }
      break;
    }

    /* if time occurred after current strip... */
    if (ctime > strip->end) {
      /* only if this is the last strip should we do anything, and only if that is being held */
      if (strip == strips->last) {
        if (strip->extendmode != NLASTRIP_EXTEND_NOTHING) {
          estrip = strip;
        }

        side = NES_TIME_AFTER;
        break;
      }

      /* otherwise, skip... as the 'before' case will catch it more elegantly! */
    }
  }

  /* check if a valid strip was found
   * - must not be muted (i.e. will have contribution
   */
  if ((estrip == nullptr) || (estrip->flag & NLASTRIP_FLAG_MUTED)) {
    return nullptr;
  }

  /* if ctime was not within the boundaries of the strip, clamp! */
  switch (side) {
    case NES_TIME_BEFORE: /* extend first frame only */
      ctime = estrip->start;
      break;
    case NES_TIME_AFTER: /* extend last frame only */
      ctime = estrip->end;
      break;
  }

  /* evaluate strip's evaluation controls
   * - skip if no influence (i.e. same effect as muting the strip)
   * - negative influence is not supported yet... how would that be defined?
   */
  /* TODO: this sounds a bit hacky having a few isolated F-Curves
   * stuck on some data it operates on... */
  AnimationEvalContext clamped_eval_context = BKE_animsys_eval_context_construct_at(
      anim_eval_context, ctime);
  nlastrip_evaluate_controls(estrip, &clamped_eval_context, flush_to_original);
  if (estrip->influence <= 0.0f) {
    return nullptr;
  }

  /* check if strip has valid data to evaluate,
   * and/or perform any additional type-specific actions
   */
  switch (estrip->type) {
    case NLASTRIP_TYPE_CLIP: /* clip must have some action to evaluate */
      if (estrip->act == nullptr) {
        return nullptr;
      }
      break;
      /* There must be strips to transition from and to (i.e. `prev` and `next` required). */
    case NLASTRIP_TYPE_TRANSITION:
      if (ELEM(nullptr, estrip->prev, estrip->next)) {
        return nullptr;
      }

      /* evaluate controls for the relevant extents of the bordering strips... */
      AnimationEvalContext start_eval_context = BKE_animsys_eval_context_construct_at(
          anim_eval_context, estrip->start);
      AnimationEvalContext end_eval_context = BKE_animsys_eval_context_construct_at(
          anim_eval_context, estrip->end);
      nlastrip_evaluate_controls(estrip->prev, &start_eval_context, flush_to_original);
      nlastrip_evaluate_controls(estrip->next, &end_eval_context, flush_to_original);
      break;
  }

  /* add to list of strips we need to evaluate */
  nes = static_cast<NlaEvalStrip *>(MEM_callocN(sizeof(NlaEvalStrip), "NlaEvalStrip"));

  nes->strip = estrip;
  nes->strip_mode = side;
  nes->track_index = index;
  nes->strip_time = estrip->strip_time;

  if (list) {
    BLI_addtail(list, nes);
  }

  return nes;
}

static NlaEvalStrip *nlastrips_ctime_get_strip_single(
    ListBase *dst_list,
    NlaStrip *single_strip,
    const AnimationEvalContext *anim_eval_context,
    const bool flush_to_original)
{
  ListBase single_tracks_list;
  single_tracks_list.first = single_tracks_list.last = single_strip;

  return nlastrips_ctime_get_strip(
      dst_list, &single_tracks_list, -1, anim_eval_context, flush_to_original);
}

/* ---------------------- */

/* Initialize a valid mask, allocating memory if necessary. */
static void nlavalidmask_init(NlaValidMask *mask, int bits)
{
  if (BLI_BITMAP_SIZE(bits) > sizeof(mask->buffer)) {
    mask->ptr = BLI_BITMAP_NEW(bits, "NlaValidMask");
  }
  else {
    mask->ptr = mask->buffer;
  }
}

/* Free allocated memory for the mask. */
static void nlavalidmask_free(NlaValidMask *mask)
{
  if (mask->ptr != mask->buffer) {
    MEM_freeN(mask->ptr);
  }
}

/* ---------------------- */

/* Hashing functions for NlaEvalChannelKey. */
static uint nlaevalchan_keyhash(const void *ptr)
{
  const NlaEvalChannelKey *key = static_cast<const NlaEvalChannelKey *>(ptr);
  uint hash = BLI_ghashutil_ptrhash(key->ptr.data);
  return hash ^ BLI_ghashutil_ptrhash(key->prop);
}

static bool nlaevalchan_keycmp(const void *a, const void *b)
{
  const NlaEvalChannelKey *A = static_cast<const NlaEvalChannelKey *>(a);
  const NlaEvalChannelKey *B = static_cast<const NlaEvalChannelKey *>(b);

  return ((A->ptr.data != B->ptr.data) || (A->prop != B->prop));
}

/* ---------------------- */

/* Allocate a new blending value snapshot for the channel. */
static NlaEvalChannelSnapshot *nlaevalchan_snapshot_new(NlaEvalChannel *nec)
{
  int length = nec->base_snapshot.length;

  size_t byte_size = sizeof(NlaEvalChannelSnapshot) + sizeof(float) * length;
  NlaEvalChannelSnapshot *nec_snapshot = static_cast<NlaEvalChannelSnapshot *>(
      MEM_callocN(byte_size, "NlaEvalChannelSnapshot"));

  nec_snapshot->channel = nec;
  nec_snapshot->length = length;
  nlavalidmask_init(&nec_snapshot->blend_domain, length);
  nlavalidmask_init(&nec_snapshot->remap_domain, length);

  return nec_snapshot;
}

/* Free a channel's blending value snapshot. */
static void nlaevalchan_snapshot_free(NlaEvalChannelSnapshot *nec_snapshot)
{
  BLI_assert(!nec_snapshot->is_base);

  nlavalidmask_free(&nec_snapshot->blend_domain);
  nlavalidmask_free(&nec_snapshot->remap_domain);
  MEM_freeN(nec_snapshot);
}

/* Copy all data in the snapshot. */
static void nlaevalchan_snapshot_copy(NlaEvalChannelSnapshot *dst,
                                      const NlaEvalChannelSnapshot *src)
{
  BLI_assert(dst->channel == src->channel);

  memcpy(dst->values, src->values, sizeof(float) * dst->length);
}

/* ---------------------- */

/* Initialize a blending state snapshot structure. */
static void nlaeval_snapshot_init(NlaEvalSnapshot *snapshot,
                                  NlaEvalData *nlaeval,
                                  NlaEvalSnapshot *base)
{
  snapshot->base = base;
  snapshot->size = MAX2(16, nlaeval->num_channels);
  snapshot->channels = static_cast<NlaEvalChannelSnapshot **>(
      MEM_callocN(sizeof(*snapshot->channels) * snapshot->size, "NlaEvalSnapshot::channels"));
}

/* Retrieve the individual channel snapshot. */
static NlaEvalChannelSnapshot *nlaeval_snapshot_get(NlaEvalSnapshot *snapshot, int index)
{
  return (index < snapshot->size) ? snapshot->channels[index] : nullptr;
}

/* Ensure at least this number of slots exists. */
static void nlaeval_snapshot_ensure_size(NlaEvalSnapshot *snapshot, int size)
{
  if (size > snapshot->size) {
    snapshot->size *= 2;
    CLAMP_MIN(snapshot->size, size);
    CLAMP_MIN(snapshot->size, 16);

    size_t byte_size = sizeof(*snapshot->channels) * snapshot->size;
    snapshot->channels = static_cast<NlaEvalChannelSnapshot **>(
        MEM_recallocN_id(snapshot->channels, byte_size, "NlaEvalSnapshot::channels"));
  }
}

/* Retrieve the address of a slot in the blending state snapshot for this channel (may realloc). */
static NlaEvalChannelSnapshot **nlaeval_snapshot_ensure_slot(NlaEvalSnapshot *snapshot,
                                                             NlaEvalChannel *nec)
{
  nlaeval_snapshot_ensure_size(snapshot, nec->owner->num_channels);
  return &snapshot->channels[nec->index];
}

/* Retrieve the blending snapshot for the specified channel, with fallback to base. */
static NlaEvalChannelSnapshot *nlaeval_snapshot_find_channel(NlaEvalSnapshot *snapshot,
                                                             NlaEvalChannel *nec)
{
  while (snapshot != nullptr) {
    NlaEvalChannelSnapshot *nec_snapshot = nlaeval_snapshot_get(snapshot, nec->index);
    if (nec_snapshot != nullptr) {
      return nec_snapshot;
    }
    snapshot = snapshot->base;
  }

  return &nec->base_snapshot;
}

/* Retrieve or create the channel value snapshot, copying from the other snapshot
 * (or default values) */
static NlaEvalChannelSnapshot *nlaeval_snapshot_ensure_channel(NlaEvalSnapshot *snapshot,
                                                               NlaEvalChannel *nec)
{
  NlaEvalChannelSnapshot **slot = nlaeval_snapshot_ensure_slot(snapshot, nec);

  if (*slot == nullptr) {
    NlaEvalChannelSnapshot *base_snapshot, *nec_snapshot;

    nec_snapshot = nlaevalchan_snapshot_new(nec);
    base_snapshot = nlaeval_snapshot_find_channel(snapshot->base, nec);

    nlaevalchan_snapshot_copy(nec_snapshot, base_snapshot);

    *slot = nec_snapshot;
  }

  return *slot;
}

/* Free all memory owned by this blending snapshot structure. */
static void nlaeval_snapshot_free_data(NlaEvalSnapshot *snapshot)
{
  if (snapshot->channels != nullptr) {
    for (int i = 0; i < snapshot->size; i++) {
      NlaEvalChannelSnapshot *nec_snapshot = snapshot->channels[i];
      if (nec_snapshot != nullptr) {
        nlaevalchan_snapshot_free(nec_snapshot);
      }
    }

    MEM_freeN(snapshot->channels);
  }

  snapshot->base = nullptr;
  snapshot->size = 0;
  snapshot->channels = nullptr;
}

/* ---------------------- */

/* Free memory owned by this evaluation channel. */
static void nlaevalchan_free_data(NlaEvalChannel *nec)
{
  nlavalidmask_free(&nec->domain);
}

/* Initialize a full NLA evaluation state structure. */
static void nlaeval_init(NlaEvalData *nlaeval)
{
  memset(nlaeval, 0, sizeof(*nlaeval));

  nlaeval->path_hash = BLI_ghash_str_new("NlaEvalData::path_hash");
  nlaeval->key_hash = BLI_ghash_new(
      nlaevalchan_keyhash, nlaevalchan_keycmp, "NlaEvalData::key_hash");
}

static void nlaeval_free(NlaEvalData *nlaeval)
{
  /* Delete base snapshot - its channels are part of NlaEvalChannel and shouldn't be freed. */
  MEM_SAFE_FREE(nlaeval->base_snapshot.channels);

  /* Delete result snapshot. */
  nlaeval_snapshot_free_data(&nlaeval->eval_snapshot);

  /* Delete channels. */
  LISTBASE_FOREACH (NlaEvalChannel *, nec, &nlaeval->channels) {
    nlaevalchan_free_data(nec);
  }

  BLI_freelistN(&nlaeval->channels);
  BLI_ghash_free(nlaeval->path_hash, nullptr, nullptr);
  BLI_ghash_free(nlaeval->key_hash, nullptr, nullptr);
}

/* ---------------------- */

static int nlaevalchan_validate_index(const NlaEvalChannel *nec, int index)
{
  if (nec->is_array) {
    if (index >= 0 && index < nec->base_snapshot.length) {
      return index;
    }

    return -1;
  }
  return 0;
}

static bool nlaevalchan_validate_index_ex(const NlaEvalChannel *nec, const int array_index)
{
  /** Although array_index comes from fcurve, that doesn't necessarily mean the property has that
   * many elements. */
  const int index = nlaevalchan_validate_index(nec, array_index);

  if (index < 0) {
    if (G.debug & G_DEBUG) {
      ID *id = nec->key.ptr.owner_id;
      CLOG_WARN(&LOG,
                "Animation: Invalid array index. ID = '%s',  '%s[%d]', array length is %d",
                id ? (id->name + 2) : "<No ID>",
                nec->rna_path,
                array_index,
                nec->base_snapshot.length);
    }

    return false;
  }
  return true;
}

/* Initialize default values for NlaEvalChannel from the property data. */
static void nlaevalchan_get_default_values(NlaEvalChannel *nec, float *r_values)
{
  PointerRNA *ptr = &nec->key.ptr;
  PropertyRNA *prop = nec->key.prop;
  int length = nec->base_snapshot.length;

  /* Use unit quaternion for quaternion properties. */
  if (nec->mix_mode == NEC_MIX_QUATERNION) {
    unit_qt(r_values);
    return;
  }
  /* Use all zero for Axis-Angle properties. */
  if (nec->mix_mode == NEC_MIX_AXIS_ANGLE) {
    zero_v4(r_values);
    return;
  }

  /* NOTE: while this doesn't work for all RNA properties as default values aren't in fact
   * set properly for most of them, at least the common ones (which also happen to get used
   * in NLA strips a lot, e.g. scale) are set correctly.
   */
  if (RNA_property_array_check(prop)) {
    BLI_assert(length == RNA_property_array_length(ptr, prop));
    bool *tmp_bool;
    int *tmp_int;

    switch (RNA_property_type(prop)) {
      case PROP_BOOLEAN:
        tmp_bool = static_cast<bool *>(MEM_malloc_arrayN(length, sizeof(*tmp_bool), __func__));
        RNA_property_boolean_get_default_array(ptr, prop, tmp_bool);
        for (int i = 0; i < length; i++) {
          r_values[i] = float(tmp_bool[i]);
        }
        MEM_freeN(tmp_bool);
        break;
      case PROP_INT:
        tmp_int = static_cast<int *>(MEM_malloc_arrayN(length, sizeof(*tmp_int), __func__));
        RNA_property_int_get_default_array(ptr, prop, tmp_int);
        for (int i = 0; i < length; i++) {
          r_values[i] = float(tmp_int[i]);
        }
        MEM_freeN(tmp_int);
        break;
      case PROP_FLOAT:
        RNA_property_float_get_default_array(ptr, prop, r_values);
        break;
      default:
        memset(r_values, 0, sizeof(float) * length);
    }
  }
  else {
    BLI_assert(length == 1);

    switch (RNA_property_type(prop)) {
      case PROP_BOOLEAN:
        *r_values = float(RNA_property_boolean_get_default(ptr, prop));
        break;
      case PROP_INT:
        *r_values = float(RNA_property_int_get_default(ptr, prop));
        break;
      case PROP_FLOAT:
        *r_values = RNA_property_float_get_default(ptr, prop);
        break;
      case PROP_ENUM:
        *r_values = float(RNA_property_enum_get_default(ptr, prop));
        break;
      default:
        *r_values = 0.0f;
    }
  }

  /* Ensure multiplicative properties aren't reset to 0. */
  if (nec->mix_mode == NEC_MIX_MULTIPLY) {
    for (int i = 0; i < length; i++) {
      if (r_values[i] == 0.0f) {
        r_values[i] = 1.0f;
      }
    }
  }
}

static char nlaevalchan_detect_mix_mode(NlaEvalChannelKey *key, int length)
{
  PropertySubType subtype = RNA_property_subtype(key->prop);

  if (subtype == PROP_QUATERNION && length == 4) {
    return NEC_MIX_QUATERNION;
  }
  if (subtype == PROP_AXISANGLE && length == 4) {
    return NEC_MIX_AXIS_ANGLE;
  }
  if (RNA_property_flag(key->prop) & PROP_PROPORTIONAL) {
    return NEC_MIX_MULTIPLY;
  }
  return NEC_MIX_ADD;
}

/* Verify that an appropriate NlaEvalChannel for this property exists. */
static NlaEvalChannel *nlaevalchan_verify_key(NlaEvalData *nlaeval,
                                              const char *path,
                                              NlaEvalChannelKey *key)
{
  /* Look it up in the key hash. */
  NlaEvalChannel **p_key_nec;
  NlaEvalChannelKey **p_key;
  bool found_key = BLI_ghash_ensure_p_ex(
      nlaeval->key_hash, key, (void ***)&p_key, (void ***)&p_key_nec);

  if (found_key) {
    return *p_key_nec;
  }

  /* Create the channel. */
  bool is_array = RNA_property_array_check(key->prop);
  int length = is_array ? RNA_property_array_length(&key->ptr, key->prop) : 1;

  NlaEvalChannel *nec = static_cast<NlaEvalChannel *>(
      MEM_callocN(sizeof(NlaEvalChannel) + sizeof(float) * length, "NlaEvalChannel"));

  /* Initialize the channel. */
  nec->rna_path = path;
  nec->key = *key;

  nec->owner = nlaeval;
  nec->index = nlaeval->num_channels++;
  nec->is_array = is_array;

  nec->mix_mode = nlaevalchan_detect_mix_mode(key, length);

  nlavalidmask_init(&nec->domain, length);

  nec->base_snapshot.channel = nec;
  nec->base_snapshot.length = length;
  nec->base_snapshot.is_base = true;

  nlaevalchan_get_default_values(nec, nec->base_snapshot.values);

  /* Store channel in data structures. */
  BLI_addtail(&nlaeval->channels, nec);

  *nlaeval_snapshot_ensure_slot(&nlaeval->base_snapshot, nec) = &nec->base_snapshot;

  *p_key_nec = nec;
  *p_key = &nec->key;

  return nec;
}

/* Verify that an appropriate NlaEvalChannel for this path exists. */
static NlaEvalChannel *nlaevalchan_verify(PointerRNA *ptr, NlaEvalData *nlaeval, const char *path)
{
  if (path == nullptr) {
    return nullptr;
  }

  /* Lookup the path in the path based hash. */
  NlaEvalChannel **p_path_nec;
  bool found_path = BLI_ghash_ensure_p(nlaeval->path_hash, (void *)path, (void ***)&p_path_nec);

  if (found_path) {
    return *p_path_nec;
  }

  /* Cache nullptr result for now. */
  *p_path_nec = nullptr;

  /* Resolve the property and look it up in the key hash. */
  NlaEvalChannelKey key;

  if (!RNA_path_resolve_property(ptr, path, &key.ptr, &key.prop)) {
    /* Report failure to resolve the path. */
    if (G.debug & G_DEBUG) {
      CLOG_WARN(&LOG,
                "Animato: Invalid path. ID = '%s',  '%s'",
                (ptr->owner_id) ? (ptr->owner_id->name + 2) : "<No ID>",
                path);
    }

    return nullptr;
  }

  /* Check that the property can be animated. */
  if (ptr->owner_id != nullptr && !RNA_property_animateable(&key.ptr, key.prop)) {
    return nullptr;
  }

  NlaEvalChannel *nec = nlaevalchan_verify_key(nlaeval, path, &key);

  if (nec->rna_path == nullptr) {
    nec->rna_path = path;
  }

  return *p_path_nec = nec;
}

/* ---------------------- */

/** \returns true if a solution exists and the output was written to. */
static bool nla_blend_get_inverted_lower_value(const int blendmode,
                                               const float strip_value,
                                               const float blended_value,
                                               const float influence,
                                               float *r_lower_value)
{
  if (IS_EQF(influence, 0.0f)) {
    *r_lower_value = blended_value;
    return true;
  }

  switch (blendmode) {
    case NLASTRIP_MODE_ADD: /* Simply subtract the scaled value on to the stack. */
      *r_lower_value = blended_value - (strip_value * influence);
      return true;

    case NLASTRIP_MODE_SUBTRACT: /* Simply add the scaled value from the stack. */
      *r_lower_value = blended_value + (strip_value * influence);
      return true;

    case NLASTRIP_MODE_MULTIPLY: {
      /* Check for division by zero. */
      const float denominator = (influence * strip_value + (1.0f - influence));
      if (IS_EQF(denominator, 0.0f)) {
        /* For 0/0, any r_lower_value is a solution. We'll just choose 1.
         *
         * Any r_lower_value is a solution. In this case, ideally we would insert redundant
         * keyframes, those that don't change the fcurve's shape. Otherwise, we're likely messing
         * up interpolation for the animator, requiring further cleanup on their part.
         */
        if (IS_EQF(blended_value, 0.0f)) {
          /* When denominator==0:
           *
           *          denominator = (inf * strip_value + (1.0f - inf))
           *                    0 =  inf * strip_value + (1-inf)
           *   -inf * strip_value =  1 - inf
           *         -strip_value = (1 - inf) / inf
           *          strip_value = (inf - 1) / inf
           *          strip_value = 1 - (1/inf)
           *
           * For blending, nla_blend_value(), this results in:
           *
           *        blended_value =  inf * (lower_value * strip_value)   + (1 - inf) * lower_value;
           *                      =  inf * (lower_value * (1 - (1/inf))) + ...
           *                      =  inf * (1 - (1/inf)) * lower_value   + ...
           *                      =  (inf - (inf/inf))   * lower_value   + ...
           *                      = -(inf - 1) * lower_value             + (1 - inf) * lower_value;
           *        blended_value = 0
           *
           * Effectively, blended_value will equal 0 no matter what lower_value is. Put another
           * way, when (blended_value==0 and denominator==0), then lower_value can be any value and
           * blending will give us back blended_value=0. We have infinite solutions for this case.
           */
          *r_lower_value = 1;
          return true;
        }
        /* No solution for division by zero. */
        return false;
      }
      /* Math:
       *     blended_value = inf * (lower_value * strip_value) + (1 - inf) * lower_value
       *                   = lower_value * (inf * strip_value + (1-inf))
       *       lower_value = blended_value / (inf * strip_value + (1-inf))
       *       lower_value = blended_value / denominator
       */
      *r_lower_value = blended_value / denominator;
      return true;
    }
    case NLASTRIP_MODE_COMBINE:
      BLI_assert_msg(0, "Use nla_combine_get_inverted_lower_value()");
      return false;

    case NLASTRIP_MODE_REPLACE:

      /* No solution if lower strip has 0 influence. */
      if (IS_EQF(influence, 1.0f)) {
        return false;
      }

      /* Math:
       *
       *  blended_value = lower_value * (1.0f - inf) + (strip_value * inf)
       *  blended_value - (strip_value * inf) = lower_value * (1.0f - inf)
       *  blended_value - (strip_value * inf) / (1.0f - inf) = lower_value
       *
       *  lower_value = blended_value - (strip_value * inf) / (1.0f - inf)
       */
      *r_lower_value = (blended_value - (strip_value * influence)) / (1.0f - influence);
      return true;
  }

  BLI_assert_msg(0, "invalid blend mode");
  return false;
}

/** \returns true if solution exists and output written to. */
static bool nla_combine_get_inverted_lower_value(const int mix_mode,
                                                 float base_value,
                                                 const float strip_value,
                                                 const float blended_value,
                                                 const float influence,
                                                 float *r_lower_value)
{
  if (IS_EQF(influence, 0.0f)) {
    *r_lower_value = blended_value;
    return true;
  }

  /* Perform blending. */
  switch (mix_mode) {
    case NEC_MIX_ADD:
    case NEC_MIX_AXIS_ANGLE:
      *r_lower_value = blended_value - (strip_value - base_value) * influence;
      return true;
    case NEC_MIX_MULTIPLY: /* Division by zero. */
      if (IS_EQF(strip_value, 0.0f)) {
        /* Resolve 0/0 to 1.
         *
         * Any r_lower_value is a solution. In this case, ideally we would insert redundant
         * keyframes, those that don't change the fcurve's shape. Otherwise, we're likely messing
         * up interpolation for the animator, requiring further cleanup on their part.
         */
        if (IS_EQF(blended_value, 0.0f)) {
          /* For blending, nla_combine_value(), when strip_value==0:
           *
           *        blended_value = lower_value * powf(strip_value / base_value, infl);
           *        blended_value = lower_value * powf(0, infl);
           *        blended_value = lower_value * 0;
           *        blended_value = 0;
           *
           * Effectively, blended_value will equal 0 no matter what lower_value is. Put another
           * way, when (blended_value==0 and strip_value==0), then lower_value can be any value and
           * blending will give us back blended_value=0. We have infinite solutions for this case.
           */
          *r_lower_value = 1.0f;
          return true;
        }
        /* No solution. */
        return false;
      }

      if (IS_EQF(base_value, 0.0f)) {
        base_value = 1.0f;
      }

      *r_lower_value = blended_value / powf(strip_value / base_value, influence);
      return true;

    case NEC_MIX_QUATERNION:
      BLI_assert_msg(0, "Use nla_combine_quaternion_get_inverted_lower_values()");
      return false;
  }

  BLI_assert_msg(0, "Mixmode not implemented");
  return false;
}

static void nla_combine_quaternion_get_inverted_lower_values(const float strip_values[4],
                                                             const float blended_values[4],
                                                             const float influence,
                                                             float r_lower_value[4])
{
  if (IS_EQF(influence, 0.0f)) {
    normalize_qt_qt(r_lower_value, blended_values);
    return;
  }

  /* blended_value = lower_values @ strip_values^infl
   * blended_value @ inv(strip_values^inf) = lower_values
   *
   * Returns: lower_values = blended_value @ inv(strip_values^inf) */
  float tmp_strip_values[4], tmp_blended[4];

  normalize_qt_qt(tmp_strip_values, strip_values);
  normalize_qt_qt(tmp_blended, blended_values);

  pow_qt_fl_normalized(tmp_strip_values, influence);
  invert_qt_normalized(tmp_strip_values);

  mul_qt_qtqt(r_lower_value, tmp_blended, tmp_strip_values);
}

/* Blend the lower nla stack value and upper strip value of a channel according to mode and
 * influence. */
static float nla_blend_value(const int blendmode,
                             const float lower_value,
                             const float strip_value,
                             const float influence)
{
  /* Optimization: no need to try applying if there is no influence. */
  if (IS_EQF(influence, 0.0f)) {
    return lower_value;
  }

  /* Perform blending. */
  switch (blendmode) {
    case NLASTRIP_MODE_ADD: /* Simply add the scaled value on to the stack. */
      return lower_value + (strip_value * influence);

    case NLASTRIP_MODE_SUBTRACT: /* Simply subtract the scaled value from the stack. */
      return lower_value - (strip_value * influence);

    case NLASTRIP_MODE_MULTIPLY: /* Multiply the scaled value with the stack. */
      return influence * (lower_value * strip_value) + (1 - influence) * lower_value;

    case NLASTRIP_MODE_COMBINE:
      BLI_assert_msg(0, "combine mode");
      ATTR_FALLTHROUGH;

    default: /* TODO: Do we really want to blend by default? it seems more uses might prefer add...
              */
      /* Do linear interpolation. The influence of the accumulated data (elsewhere, that is called
       * dstweight) is 1 - influence, since the strip's influence is srcweight.
       */
      return lower_value * (1.0f - influence) + (strip_value * influence);
  }
}

/* Blend the lower nla stack value and upper strip value of a channel according to mode and
 * influence. */
static float nla_combine_value(const int mix_mode,
                               float base_value,
                               const float lower_value,
                               const float strip_value,
                               const float influence)
{
  /* Optimization: No need to try applying if there is no influence. */
  if (IS_EQF(influence, 0.0f)) {
    return lower_value;
  }

  /* Perform blending */
  switch (mix_mode) {
    case NEC_MIX_ADD:
    case NEC_MIX_AXIS_ANGLE:
      return lower_value + (strip_value - base_value) * influence;

    case NEC_MIX_MULTIPLY:
      if (IS_EQF(base_value, 0.0f)) {
        base_value = 1.0f;
      }
      return lower_value * powf(strip_value / base_value, influence);

    default:
      BLI_assert_msg(0, "invalid mix mode");
      return lower_value;
  }
}

/** \returns true if solution exists and output is written to. */
static bool nla_blend_get_inverted_strip_value(const int blendmode,
                                               const float lower_value,
                                               const float blended_value,
                                               const float influence,
                                               float *r_strip_value)
{
  /** No solution if strip had 0 influence. */
  if (IS_EQF(influence, 0.0f)) {
    return false;
  }

  switch (blendmode) {
    case NLASTRIP_MODE_ADD:
      *r_strip_value = (blended_value - lower_value) / influence;
      return true;

    case NLASTRIP_MODE_SUBTRACT:
      *r_strip_value = (lower_value - blended_value) / influence;
      return true;

    case NLASTRIP_MODE_MULTIPLY:
      if (IS_EQF(lower_value, 0.0f)) {
        /* Resolve 0/0 to 1. */
        if (IS_EQF(blended_value, 0.0f)) {
          *r_strip_value = 1.0f;
          return true;
        }
        /* Division by zero. */
        return false;
      }

      /** Math:
       *
       *  blended_value = inf * (lower_value * strip_value) + (1 - inf) * lower_value
       *  blended_value - (1 - inf) * lower_value = inf * (lower_value * strip_value)
       *  (blended_value - (1 - inf) * lower_value) / (inf * lower_value) =  strip_value
       *  (blended_value - lower_value + inf * lower_value) / (inf * lower_value) =  strip_value
       *  ((blended_value - lower_value) / (inf * lower_value)) + 1 =  strip_value
       *
       *  strip_value = ((blended_value - lower_value) / (inf * lower_value)) + 1
       */
      *r_strip_value = ((blended_value - lower_value) / (influence * lower_value)) + 1.0f;
      return true;

    case NLASTRIP_MODE_COMBINE:
      BLI_assert_msg(0, "combine mode");
      ATTR_FALLTHROUGH;

    default:

      /** Math:
       *
       *  blended_value = lower_value * (1.0f - inf) + (strip_value * inf)
       *  blended_value - lower_value * (1.0f - inf) = (strip_value * inf)
       *  (blended_value - lower_value * (1.0f - inf)) / inf = strip_value
       *
       *  strip_value = (blended_value - lower_value * (1.0f - inf)) / inf
       */
      *r_strip_value = (blended_value - lower_value * (1.0f - influence)) / influence;
      return true;
  }
}

/** \returns true if solution exists and output is written to. */
static bool nla_combine_get_inverted_strip_value(const int mix_mode,
                                                 float base_value,
                                                 const float lower_value,
                                                 const float blended_value,
                                                 const float influence,
                                                 float *r_strip_value)
{
  /* No solution if strip had no influence. */
  if (IS_EQF(influence, 0.0f)) {
    return false;
  }

  switch (mix_mode) {
    case NEC_MIX_ADD:
    case NEC_MIX_AXIS_ANGLE:
      *r_strip_value = base_value + (blended_value - lower_value) / influence;
      return true;

    case NEC_MIX_MULTIPLY:
      if (IS_EQF(base_value, 0.0f)) {
        base_value = 1.0f;
      }
      /* Division by zero. */
      if (IS_EQF(lower_value, 0.0f)) {
        /* Resolve 0/0 to 1. */
        if (IS_EQF(blended_value, 0.0f)) {
          *r_strip_value = base_value;
          return true;
        }
        /* Division by zero. */
        return false;
      }

      *r_strip_value = base_value * powf(blended_value / lower_value, 1.0f / influence);
      return true;

    default:
      BLI_assert_msg(0, "invalid mix mode");
      return false;
  }
}

/**
 * Accumulate quaternion channels for Combine mode according to influence.
 * \returns `blended_value = lower_values @ strip_values^infl`
 */
static void nla_combine_quaternion(const float lower_values[4],
                                   const float strip_values[4],
                                   const float influence,
                                   float r_blended_value[4])
{
  float tmp_lower[4], tmp_strip_values[4];

  normalize_qt_qt(tmp_lower, lower_values);
  normalize_qt_qt(tmp_strip_values, strip_values);

  pow_qt_fl_normalized(tmp_strip_values, influence);
  mul_qt_qtqt(r_blended_value, tmp_lower, tmp_strip_values);
}

/** \returns true if solution exists and output written to. */
static bool nla_combine_quaternion_get_inverted_strip_values(const float lower_values[4],
                                                             const float blended_values[4],
                                                             const float influence,
                                                             float r_strip_values[4])
{
  /* blended_value = lower_values @ r_strip_values^infl
   * inv(lower_values) @ blended_value = r_strip_values^infl
   * (inv(lower_values) @ blended_value) ^ (1/inf) = r_strip_values
   *
   * Returns: r_strip_values = (inv(lower_values) @ blended_value) ^ (1/inf) */
  if (IS_EQF(influence, 0.0f)) {
    return false;
  }
  float tmp_lower[4], tmp_blended[4];

  normalize_qt_qt(tmp_lower, lower_values);
  normalize_qt_qt(tmp_blended, blended_values);
  invert_qt_normalized(tmp_lower);

  mul_qt_qtqt(r_strip_values, tmp_lower, tmp_blended);
  pow_qt_fl_normalized(r_strip_values, 1.0f / influence);

  return true;
}

/* ---------------------- */

/* Assert necs and necs->channel is nonNull. */
static void nlaevalchan_assert_nonNull(NlaEvalChannelSnapshot *necs)
{
  UNUSED_VARS_NDEBUG(necs);
  BLI_assert(necs != nullptr && necs->channel != nullptr);
}

/* Assert that the channels given can be blended or combined together. */
static void nlaevalchan_assert_blendOrcombine_compatible(NlaEvalChannelSnapshot *lower_necs,
                                                         NlaEvalChannelSnapshot *upper_necs,
                                                         NlaEvalChannelSnapshot *blended_necs)
{
  UNUSED_VARS_NDEBUG(lower_necs, upper_necs, blended_necs);
  BLI_assert(!ELEM(nullptr, lower_necs, blended_necs));
  BLI_assert(upper_necs == nullptr || lower_necs->length == upper_necs->length);
  BLI_assert(lower_necs->length == blended_necs->length);
}

/**
 * Check each remap domain of blended values individually in case animator had a non-combine NLA
 * strip with a subset of quaternion channels and remapping through any of them failed and thus
 * potentially has undefined values.
 *
 * \returns true if case occurred and handled. Returns false if case didn't occur.
 */
static bool nlaevalchan_combine_quaternion_handle_undefined_blend_values(
    NlaEvalChannelSnapshot *blended_necs, NlaEvalChannelSnapshot *upper_or_lower_necs)
{
  for (int j = 0; j < 4; j++) {
    if (!BLI_BITMAP_TEST_BOOL(blended_necs->remap_domain.ptr, j)) {
      BLI_bitmap_set_all(upper_or_lower_necs->remap_domain.ptr, false, 4);
      return true;
    }
  }

  return false;
}

/* Assert that the channels given can be blended or combined together as a quaternion. */
static void nlaevalchan_assert_blendOrcombine_compatible_quaternion(
    NlaEvalChannelSnapshot *lower_necs,
    NlaEvalChannelSnapshot *upper_necs,
    NlaEvalChannelSnapshot *blended_necs)
{
  nlaevalchan_assert_blendOrcombine_compatible(lower_necs, upper_necs, blended_necs);
  BLI_assert(lower_necs->length == 4);
}

static void nlaevalchan_copy_values(NlaEvalChannelSnapshot *dst, NlaEvalChannelSnapshot *src)
{
  memcpy(dst->values, src->values, src->length * sizeof(float));
}

/**
 * Copies from lower necs to blended necs if upper necs is nullptr or has zero influence.
 * \return true if copied.
 */
static bool nlaevalchan_blendOrcombine_try_copy_from_lower(NlaEvalChannelSnapshot *lower_necs,
                                                           NlaEvalChannelSnapshot *upper_necs,
                                                           const float upper_influence,
                                                           NlaEvalChannelSnapshot *r_blended_necs)
{
  const bool has_influence = !IS_EQF(upper_influence, 0.0f);
  if (upper_necs != nullptr && has_influence) {
    return false;
  }

  nlaevalchan_copy_values(r_blended_necs, lower_necs);
  return true;
}

/**
 * Copies to lower necs from blended necs if upper necs is nullptr or has zero influence. If
 * successful, copies blended_necs remap domains to lower_necs.
 *
 * Does not check upper value blend domains.
 *
 * \return true if copied.
 */
static bool nlaevalchan_blendOrcombine_try_copy_to_lower(NlaEvalChannelSnapshot *blended_necs,
                                                         NlaEvalChannelSnapshot *upper_necs,
                                                         const float upper_influence,
                                                         NlaEvalChannelSnapshot *r_lower_necs)
{
  const bool has_influence = !IS_EQF(upper_influence, 0.0f);
  if (upper_necs != nullptr && has_influence) {
    return false;
  }

  nlaevalchan_copy_values(r_lower_necs, blended_necs);

  /* Must copy remap domain to handle case where some blended values are out of domain. */
  BLI_bitmap_copy_all(
      r_lower_necs->remap_domain.ptr, blended_necs->remap_domain.ptr, r_lower_necs->length);

  return true;
}

/**
 * Based on blendmode, blend lower necs with upper necs into blended necs.
 *
 * Each upper value's blend domain determines whether to blend or to copy directly
 * from lower.
 */
static void nlaevalchan_blend_value(NlaEvalChannelSnapshot *lower_necs,
                                    NlaEvalChannelSnapshot *upper_necs,
                                    const int upper_blendmode,
                                    const float upper_influence,
                                    NlaEvalChannelSnapshot *r_blended_necs)
{
  nlaevalchan_assert_blendOrcombine_compatible(lower_necs, upper_necs, r_blended_necs);
  if (nlaevalchan_blendOrcombine_try_copy_from_lower(
          lower_necs, upper_necs, upper_influence, r_blended_necs))
  {
    return;
  }

  const int length = lower_necs->length;
  for (int j = 0; j < length; j++) {
    if (!BLI_BITMAP_TEST_BOOL(upper_necs->blend_domain.ptr, j)) {
      r_blended_necs->values[j] = lower_necs->values[j];
      continue;
    }

    r_blended_necs->values[j] = nla_blend_value(
        upper_blendmode, lower_necs->values[j], upper_necs->values[j], upper_influence);
  }
}

/**
 * Based on mix-mode, provided by one the necs,
 * combines lower necs with upper necs into blended necs.
 *
 * Each upper value's blend domain determines whether to blend or to copy directly from lower.
 */
static void nlaevalchan_combine_value(NlaEvalChannelSnapshot *lower_necs,
                                      NlaEvalChannelSnapshot *upper_necs,
                                      const float upper_influence,
                                      NlaEvalChannelSnapshot *r_blended_necs)
{
  nlaevalchan_assert_blendOrcombine_compatible(lower_necs, upper_necs, r_blended_necs);
  if (nlaevalchan_blendOrcombine_try_copy_from_lower(
          lower_necs, upper_necs, upper_influence, r_blended_necs))
  {
    return;
  }

  /* Assumes every base is the same. */
  float *base_values = lower_necs->channel->base_snapshot.values;
  const int length = lower_necs->length;
  const char mix_mode = lower_necs->channel->mix_mode;

  for (int j = 0; j < length; j++) {
    if (!BLI_BITMAP_TEST_BOOL(upper_necs->blend_domain.ptr, j)) {
      r_blended_necs->values[j] = lower_necs->values[j];
      continue;
    }

    r_blended_necs->values[j] = nla_combine_value(
        mix_mode, base_values[j], lower_necs->values[j], upper_necs->values[j], upper_influence);
  }
}

/**
 * Quaternion combines lower necs with upper necs into blended necs.
 *
 * Each upper value's blend domain determines whether to blend or to copy directly
 * from lower.
 */
static void nlaevalchan_combine_quaternion(NlaEvalChannelSnapshot *lower_necs,
                                           NlaEvalChannelSnapshot *upper_necs,
                                           const float upper_influence,
                                           NlaEvalChannelSnapshot *r_blended_necs)
{
  nlaevalchan_assert_blendOrcombine_compatible_quaternion(lower_necs, upper_necs, r_blended_necs);
  if (nlaevalchan_blendOrcombine_try_copy_from_lower(
          lower_necs, upper_necs, upper_influence, r_blended_necs))
  {
    return;
  }

  /** No need to check per index. We limit to all or nothing combining for quaternions. */
  if (!BLI_BITMAP_TEST_BOOL(upper_necs->blend_domain.ptr, 0)) {
    nlaevalchan_copy_values(r_blended_necs, lower_necs);
    return;
  }

  nla_combine_quaternion(
      lower_necs->values, upper_necs->values, upper_influence, r_blended_necs->values);
}

/**
 * Based on blend-mode and mix-mode, blend lower necs with upper necs into blended necs.
 *
 * Each upper value's blend domain determines whether to blend or to copy directly
 * from lower.
 *
 * \param lower_necs: Never nullptr.
 * \param upper_necs: Can be nullptr.
 * \param upper_blendmode: Enum value in eNlaStrip_Blend_Mode.
 * \param upper_influence: Value in range [0, 1].
 * \param upper_necs: Never nullptr.
 */
static void nlaevalchan_blendOrcombine(NlaEvalChannelSnapshot *lower_necs,
                                       NlaEvalChannelSnapshot *upper_necs,
                                       const int upper_blendmode,
                                       const float upper_influence,
                                       NlaEvalChannelSnapshot *r_blended_necs)
{
  nlaevalchan_assert_nonNull(r_blended_necs);

  switch (upper_blendmode) {
    case NLASTRIP_MODE_COMBINE: {
      switch (r_blended_necs->channel->mix_mode) {
        case NEC_MIX_QUATERNION: {
          nlaevalchan_combine_quaternion(lower_necs, upper_necs, upper_influence, r_blended_necs);
          return;
        }
        case NEC_MIX_ADD:
        case NEC_MIX_AXIS_ANGLE:
        case NEC_MIX_MULTIPLY: {
          nlaevalchan_combine_value(lower_necs, upper_necs, upper_influence, r_blended_necs);
          return;
        }
        default:
          BLI_assert_msg(0, "Mix mode should've been handled");
      }
      return;
    }
    case NLASTRIP_MODE_ADD:
    case NLASTRIP_MODE_SUBTRACT:
    case NLASTRIP_MODE_MULTIPLY:
    case NLASTRIP_MODE_REPLACE: {
      nlaevalchan_blend_value(
          lower_necs, upper_necs, upper_blendmode, upper_influence, r_blended_necs);
      return;
    }
    default:
      BLI_assert_msg(0, "Blend mode should've been handled");
  }
}

/**
 * Based on blend-mode, solve for the upper values such that when lower blended with upper then we
 * get blended values as a result.
 *
 * Only processes blended values in the remap domain. Successfully remapped upper values are placed
 * in the remap domain so caller knows which values are usable.
 */
static void nlaevalchan_blend_value_get_inverted_upper_evalchan(
    NlaEvalChannelSnapshot *lower_necs,
    NlaEvalChannelSnapshot *blended_necs,
    const int upper_blendmode,
    const float upper_influence,
    NlaEvalChannelSnapshot *r_upper_necs)
{
  nlaevalchan_assert_nonNull(r_upper_necs);
  nlaevalchan_assert_blendOrcombine_compatible(lower_necs, r_upper_necs, blended_necs);

  const int length = lower_necs->length;
  for (int j = 0; j < length; j++) {
    if (!BLI_BITMAP_TEST_BOOL(blended_necs->remap_domain.ptr, j)) {
      BLI_BITMAP_DISABLE(r_upper_necs->remap_domain.ptr, j);
      continue;
    }

    const bool success = nla_blend_get_inverted_strip_value(upper_blendmode,
                                                            lower_necs->values[j],
                                                            blended_necs->values[j],
                                                            upper_influence,
                                                            &r_upper_necs->values[j]);
    BLI_BITMAP_SET(r_upper_necs->remap_domain.ptr, j, success);
  }
}

/**
 * Based on mix-mode, solve for the upper values such that when lower combined with upper then we
 * get blended values as a result.
 *
 * Only processes blended values in the remap domain. Successfully remapped upper values are placed
 * in the remap domain so caller knows which values are usable.
 */
static void nlaevalchan_combine_value_get_inverted_upper_evalchan(
    NlaEvalChannelSnapshot *lower_necs,
    NlaEvalChannelSnapshot *blended_necs,
    const float upper_influence,
    NlaEvalChannelSnapshot *r_upper_necs)
{
  nlaevalchan_assert_nonNull(r_upper_necs);
  nlaevalchan_assert_blendOrcombine_compatible(lower_necs, r_upper_necs, blended_necs);

  /* Assumes every channel's base is the same. */
  float *base_values = lower_necs->channel->base_snapshot.values;
  const int length = lower_necs->length;
  const char mix_mode = lower_necs->channel->mix_mode;

  for (int j = 0; j < length; j++) {
    if (!BLI_BITMAP_TEST_BOOL(blended_necs->remap_domain.ptr, j)) {
      BLI_BITMAP_DISABLE(r_upper_necs->remap_domain.ptr, j);
      continue;
    }

    const bool success = nla_combine_get_inverted_strip_value(mix_mode,
                                                              base_values[j],
                                                              lower_necs->values[j],
                                                              blended_necs->values[j],
                                                              upper_influence,
                                                              &r_upper_necs->values[j]);

    BLI_BITMAP_SET(r_upper_necs->remap_domain.ptr, j, success);
  }
}

/**
 * Solve for the upper values such that when lower quaternion combined with upper then we get
 * blended values as a result.
 *
 * All blended values must be in the remap domain. If successfully remapped, then all upper values
 * are placed in the remap domain so caller knows the result is usable.
 */
static void nlaevalchan_combine_quaternion_get_inverted_upper_evalchan(
    NlaEvalChannelSnapshot *lower_necs,
    NlaEvalChannelSnapshot *blended_necs,
    const float upper_influence,
    NlaEvalChannelSnapshot *r_upper_necs)
{
  nlaevalchan_assert_nonNull(r_upper_necs);
  nlaevalchan_assert_blendOrcombine_compatible_quaternion(lower_necs, r_upper_necs, blended_necs);

  if (nlaevalchan_combine_quaternion_handle_undefined_blend_values(blended_necs, r_upper_necs)) {
    return;
  }

  const bool success = nla_combine_quaternion_get_inverted_strip_values(
      lower_necs->values, blended_necs->values, upper_influence, r_upper_necs->values);

  BLI_bitmap_set_all(r_upper_necs->remap_domain.ptr, success, 4);
}

/**
 * Based on blend-mode and mix mode, solve for the upper values such that when lower blended or
 * combined with upper then we get blended values as a result.
 *
 * Only processes blended values in the remap domain. Successfully remapped upper values are placed
 * in the remap domain so caller knows which values are usable.
 *
 * \param lower_necs: Never nullptr.
 * \param blended_necs: Never nullptr.
 * \param upper_blendmode: Enum value in eNlaStrip_Blend_Mode.
 * \param upper_influence: Value in range [0, 1].
 * \param r_upper_necs: Never nullptr.
 */
static void nlaevalchan_blendOrcombine_get_inverted_upper_evalchan(
    NlaEvalChannelSnapshot *lower_necs,
    NlaEvalChannelSnapshot *blended_necs,
    const int upper_blendmode,
    const float upper_influence,
    NlaEvalChannelSnapshot *r_upper_necs)
{
  nlaevalchan_assert_nonNull(r_upper_necs);

  if (IS_EQF(upper_influence, 0.0f)) {
    BLI_bitmap_set_all(r_upper_necs->remap_domain.ptr, false, r_upper_necs->length);
    return;
  }

  switch (upper_blendmode) {
    case NLASTRIP_MODE_COMBINE: {
      switch (r_upper_necs->channel->mix_mode) {
        case NEC_MIX_QUATERNION: {
          nlaevalchan_combine_quaternion_get_inverted_upper_evalchan(
              lower_necs, blended_necs, upper_influence, r_upper_necs);
          return;
        }
        case NEC_MIX_ADD:
        case NEC_MIX_AXIS_ANGLE:
        case NEC_MIX_MULTIPLY: {
          nlaevalchan_combine_value_get_inverted_upper_evalchan(
              lower_necs, blended_necs, upper_influence, r_upper_necs);
          return;
        }
        default:
          BLI_assert_msg(0, "Mix mode should've been handled");
      }
      return;
    }
    case NLASTRIP_MODE_ADD:
    case NLASTRIP_MODE_SUBTRACT:
    case NLASTRIP_MODE_MULTIPLY:
    case NLASTRIP_MODE_REPLACE: {
      nlaevalchan_blend_value_get_inverted_upper_evalchan(
          lower_necs, blended_necs, upper_blendmode, upper_influence, r_upper_necs);
      return;
    }
    default:
      BLI_assert_msg(0, "Blend mode should've been handled");
  }
}

static void nlaevalchan_blend_value_get_inverted_lower_evalchan(
    NlaEvalChannelSnapshot *blended_necs,
    NlaEvalChannelSnapshot *upper_necs,
    const int upper_blendmode,
    const float upper_influence,
    NlaEvalChannelSnapshot *r_lower_necs)
{
  nlaevalchan_assert_blendOrcombine_compatible(r_lower_necs, upper_necs, blended_necs);

  if (nlaevalchan_blendOrcombine_try_copy_to_lower(
          blended_necs, upper_necs, upper_influence, r_lower_necs))
  {
    return;
  }

  const int length = r_lower_necs->length;

  for (int j = 0; j < length; j++) {
    if (!BLI_BITMAP_TEST_BOOL(blended_necs->remap_domain.ptr, j)) {
      BLI_BITMAP_DISABLE(r_lower_necs->remap_domain.ptr, j);
      continue;
    }

    /* If upper value was not blended, then the blended value was directly copied from the lower
     * value. */
    if (!BLI_BITMAP_TEST_BOOL(upper_necs->blend_domain.ptr, j)) {
      r_lower_necs->values[j] = blended_necs->values[j];
      BLI_BITMAP_ENABLE(r_lower_necs->remap_domain.ptr, j);
      continue;
    }

    const bool success = nla_blend_get_inverted_lower_value(upper_blendmode,
                                                            upper_necs->values[j],
                                                            blended_necs->values[j],
                                                            upper_influence,
                                                            &r_lower_necs->values[j]);

    BLI_BITMAP_SET(r_lower_necs->remap_domain.ptr, j, success);
  }
}

static void nlaevalchan_combine_value_get_inverted_lower_evalchan(
    NlaEvalChannelSnapshot *blended_necs,
    NlaEvalChannelSnapshot *upper_necs,
    const float upper_influence,
    NlaEvalChannelSnapshot *r_lower_necs)
{
  nlaevalchan_assert_blendOrcombine_compatible(r_lower_necs, upper_necs, blended_necs);

  if (nlaevalchan_blendOrcombine_try_copy_to_lower(
          blended_necs, upper_necs, upper_influence, r_lower_necs))
  {
    return;
  }

  float *base_values = r_lower_necs->channel->base_snapshot.values;
  const int mix_mode = r_lower_necs->channel->mix_mode;
  const int length = r_lower_necs->length;

  for (int j = 0; j < length; j++) {
    if (!BLI_BITMAP_TEST_BOOL(blended_necs->remap_domain.ptr, j)) {
      BLI_BITMAP_DISABLE(r_lower_necs->remap_domain.ptr, j);
      continue;
    }

    /* If upper value was not blended, then the blended value was directly copied from the lower
     * value. */
    if (!BLI_BITMAP_TEST_BOOL(upper_necs->blend_domain.ptr, j)) {
      r_lower_necs->values[j] = blended_necs->values[j];
      BLI_BITMAP_ENABLE(r_lower_necs->remap_domain.ptr, j);
      continue;
    }

    const bool success = nla_combine_get_inverted_lower_value(mix_mode,
                                                              base_values[j],
                                                              upper_necs->values[j],
                                                              blended_necs->values[j],
                                                              upper_influence,
                                                              &r_lower_necs->values[j]);

    BLI_BITMAP_SET(r_lower_necs->remap_domain.ptr, j, success);
  }
}

static void nlaevalchan_combine_quaternion_get_inverted_lower_evalchan(
    NlaEvalChannelSnapshot *blended_necs,
    NlaEvalChannelSnapshot *upper_necs,
    const float upper_influence,
    NlaEvalChannelSnapshot *r_lower_necs)
{
  nlaevalchan_assert_blendOrcombine_compatible_quaternion(r_lower_necs, upper_necs, blended_necs);

  if (nlaevalchan_combine_quaternion_handle_undefined_blend_values(blended_necs, r_lower_necs)) {
    return;
  }

  if (nlaevalchan_blendOrcombine_try_copy_to_lower(
          blended_necs, upper_necs, upper_influence, r_lower_necs))
  {
    return;
  }

  /* If upper value was not blended, then the blended value was directly copied from the lower
   * value. */
  if (!BLI_BITMAP_TEST_BOOL(upper_necs->blend_domain.ptr, 0)) {
    memcpy(r_lower_necs->values, blended_necs->values, 4 * sizeof(float));
    BLI_bitmap_set_all(r_lower_necs->remap_domain.ptr, true, 4);
    return;
  }

  nla_combine_quaternion_get_inverted_lower_values(
      upper_necs->values, blended_necs->values, upper_influence, r_lower_necs->values);

  BLI_bitmap_set_all(r_lower_necs->remap_domain.ptr, true, 4);
}

/**
 * Based on blendmode and mix mode, solve for the lower values such that when lower blended or
 * combined with upper then we get blended values as a result.
 *
 * Only processes blended values in the remap domain. Successfully remapped lower values are placed
 * in the remap domain so caller knows which values are usable.
 *
 * \param blended_necs: Never nullptr.
 * \param upper_necs: Can be nullptr.
 * \param upper_blendmode: Enum value in eNlaStrip_Blend_Mode.
 * \param upper_influence: Value in range [0, 1].
 * \param r_lower_necs: Never nullptr.
 */
static void nlaevalchan_blendOrCombine_get_inverted_lower_evalchan(
    NlaEvalChannelSnapshot *blended_necs,
    NlaEvalChannelSnapshot *upper_necs,
    const int upper_blendmode,
    const float upper_influence,
    NlaEvalChannelSnapshot *r_lower_necs)

{
  nlaevalchan_assert_nonNull(r_lower_necs);

  switch (upper_blendmode) {
    case NLASTRIP_MODE_COMBINE: {
      switch (r_lower_necs->channel->mix_mode) {
        case NEC_MIX_QUATERNION: {
          nlaevalchan_combine_quaternion_get_inverted_lower_evalchan(
              blended_necs, upper_necs, upper_influence, r_lower_necs);
          return;
        }
        case NEC_MIX_ADD:
        case NEC_MIX_AXIS_ANGLE:
        case NEC_MIX_MULTIPLY: {
          nlaevalchan_combine_value_get_inverted_lower_evalchan(
              blended_necs, upper_necs, upper_influence, r_lower_necs);
          return;
        }
      }
      BLI_assert_msg(0, "Mix mode should've been handled");
      return;
    }
    case NLASTRIP_MODE_ADD:
    case NLASTRIP_MODE_SUBTRACT:
    case NLASTRIP_MODE_MULTIPLY:
    case NLASTRIP_MODE_REPLACE: {
      nlaevalchan_blend_value_get_inverted_lower_evalchan(
          blended_necs, upper_necs, upper_blendmode, upper_influence, r_lower_necs);
      return;
    }
  }

  BLI_assert_msg(0, "Blend mode should've been handled");
}

/* ---------------------- */
/* F-Modifier stack joining/separation utilities -
 * should we generalize these for BLI_listbase.h interface? */

/* Temporarily join two lists of modifiers together, storing the result in a third list */
static void nlaeval_fmodifiers_join_stacks(ListBase *result, ListBase *list1, ListBase *list2)
{
  FModifier *fcm1, *fcm2;

  /* if list1 is invalid... */
  if (ELEM(nullptr, list1, list1->first)) {
    if (list2 && list2->first) {
      result->first = list2->first;
      result->last = list2->last;
    }
  }
  /* if list 2 is invalid... */
  else if (ELEM(nullptr, list2, list2->first)) {
    result->first = list1->first;
    result->last = list1->last;
  }
  else {
    /* list1 should be added first, and list2 second,
     * with the endpoints of these being the endpoints for result
     * - the original lists must be left unchanged though, as we need that fact for restoring.
     */
    result->first = list1->first;
    result->last = list2->last;

    fcm1 = static_cast<FModifier *>(list1->last);
    fcm2 = static_cast<FModifier *>(list2->first);

    fcm1->next = fcm2;
    fcm2->prev = fcm1;
  }
}

/* Split two temporary lists of modifiers */
static void nlaeval_fmodifiers_split_stacks(ListBase *list1, ListBase *list2)
{
  FModifier *fcm1, *fcm2;

  /* if list1/2 is invalid... just skip */
  if (ELEM(nullptr, list1, list2)) {
    return;
  }
  if (ELEM(nullptr, list1->first, list2->first)) {
    return;
  }

  /* get endpoints */
  fcm1 = static_cast<FModifier *>(list1->last);
  fcm2 = static_cast<FModifier *>(list2->first);

  /* clear their links */
  fcm1->next = nullptr;
  fcm2->prev = nullptr;
}

/* ---------------------- */

/** Fills \a r_snapshot with the \a action's evaluated fcurve values with modifiers applied. */
static void nlasnapshot_from_action(PointerRNA *ptr,
                                    NlaEvalData *channels,
                                    ListBase *modifiers,
                                    bAction *action,
                                    const float evaltime,
                                    NlaEvalSnapshot *r_snapshot)
{
  action_idcode_patch_check(ptr->owner_id, action);

  /* Evaluate modifiers which modify time to evaluate the base curves at. */
  FModifiersStackStorage storage;
  storage.modifier_count = BLI_listbase_count(modifiers);
  storage.size_per_modifier = evaluate_fmodifiers_storage_size_per_modifier(modifiers);
  storage.buffer = alloca(storage.modifier_count * storage.size_per_modifier);

  const float modified_evaltime = evaluate_time_fmodifiers(
      &storage, modifiers, nullptr, 0.0f, evaltime);

  LISTBASE_FOREACH (FCurve *, fcu, &action->curves) {
    if (!is_fcurve_evaluatable(fcu)) {
      continue;
    }

    NlaEvalChannel *nec = nlaevalchan_verify(ptr, channels, fcu->rna_path);

    /* Invalid path or property cannot be animated. */
    if (nec == nullptr) {
      continue;
    }

    if (!nlaevalchan_validate_index_ex(nec, fcu->array_index)) {
      continue;
    }

    NlaEvalChannelSnapshot *necs = nlaeval_snapshot_ensure_channel(r_snapshot, nec);

    float value = evaluate_fcurve(fcu, modified_evaltime);
    evaluate_value_fmodifiers(&storage, modifiers, fcu, &value, evaltime);
    necs->values[fcu->array_index] = value;

    if (nec->mix_mode == NEC_MIX_QUATERNION) {
      BLI_bitmap_set_all(necs->blend_domain.ptr, true, 4);
    }
    else {
      BLI_BITMAP_ENABLE(necs->blend_domain.ptr, fcu->array_index);
    }
  }
}

/* evaluate action-clip strip */
static void nlastrip_evaluate_actionclip(const int evaluation_mode,
                                         PointerRNA *ptr,
                                         NlaEvalData *channels,
                                         ListBase *modifiers,
                                         NlaEvalStrip *nes,
                                         NlaEvalSnapshot *snapshot)
{

  NlaStrip *strip = nes->strip;

  /* sanity checks for action */
  if (strip == nullptr) {
    return;
  }

  if (strip->act == nullptr) {
    CLOG_ERROR(&LOG, "NLA-Strip Eval Error: Strip '%s' has no Action", strip->name);
    return;
  }

  ListBase tmp_modifiers = {nullptr, nullptr};

  /* join this strip's modifiers to the parent's modifiers (own modifiers first) */
  nlaeval_fmodifiers_join_stacks(&tmp_modifiers, &strip->modifiers, modifiers);

  switch (evaluation_mode) {
    case STRIP_EVAL_BLEND: {

      NlaEvalSnapshot strip_snapshot;
      nlaeval_snapshot_init(&strip_snapshot, channels, nullptr);

      nlasnapshot_from_action(
          ptr, channels, &tmp_modifiers, strip->act, strip->strip_time, &strip_snapshot);
      nlasnapshot_blend(
          channels, snapshot, &strip_snapshot, strip->blendmode, strip->influence, snapshot);

      nlaeval_snapshot_free_data(&strip_snapshot);

      break;
    }
    case STRIP_EVAL_BLEND_GET_INVERTED_LOWER_SNAPSHOT: {

      NlaEvalSnapshot strip_snapshot;
      nlaeval_snapshot_init(&strip_snapshot, channels, nullptr);

      nlasnapshot_from_action(
          ptr, channels, &tmp_modifiers, strip->act, strip->strip_time, &strip_snapshot);
      nlasnapshot_blend_get_inverted_lower_snapshot(
          channels, snapshot, &strip_snapshot, strip->blendmode, strip->influence, snapshot);

      nlaeval_snapshot_free_data(&strip_snapshot);

      break;
    }
    case STRIP_EVAL_NOBLEND: {
      nlasnapshot_from_action(
          ptr, channels, &tmp_modifiers, strip->act, strip->strip_time, snapshot);
      break;
    }
  }

  /* unlink this strip's modifiers from the parent's modifiers again */
  nlaeval_fmodifiers_split_stacks(&strip->modifiers, modifiers);
}

/* evaluate transition strip */
static void nlastrip_evaluate_transition(const int evaluation_mode,
                                         PointerRNA *ptr,
                                         NlaEvalData *channels,
                                         ListBase *modifiers,
                                         NlaEvalStrip *nes,
                                         NlaEvalSnapshot *snapshot,
                                         const AnimationEvalContext *anim_eval_context,
                                         const bool flush_to_original)
{
  ListBase tmp_modifiers = {nullptr, nullptr};
  NlaEvalSnapshot snapshot1, snapshot2;
  NlaEvalStrip tmp_nes;
  NlaStrip *s1, *s2;

  /* join this strip's modifiers to the parent's modifiers (own modifiers first) */
  nlaeval_fmodifiers_join_stacks(&tmp_modifiers, &nes->strip->modifiers, modifiers);

  /* get the two strips to operate on
   * - we use the endpoints of the strips directly flanking our strip
   *   using these as the endpoints of the transition (destination and source)
   * - these should have already been determined to be valid...
   * - if this strip is being played in reverse, we need to swap these endpoints
   *   otherwise they will be interpolated wrong
   */
  if (nes->strip->flag & NLASTRIP_FLAG_REVERSE) {
    s1 = nes->strip->next;
    s2 = nes->strip->prev;
  }
  else {
    s1 = nes->strip->prev;
    s2 = nes->strip->next;
  }

  switch (evaluation_mode) {
    case STRIP_EVAL_BLEND: {

      /* prepare template for 'evaluation strip'
       * - based on the transition strip's evaluation strip data
       * - strip_mode is NES_TIME_TRANSITION_* based on which endpoint
       * - strip_time is the 'normalized' (i.e. in-strip) time for evaluation,
       *   which doubles up as an additional weighting factor for the strip influences
       *   which allows us to appear to be 'interpolating' between the two extremes
       */
      tmp_nes = *nes;

      /* evaluate these strips into a temp-buffer (tmp_channels) */
      /* FIXME: modifier evaluation here needs some work... */
      /* first strip */
      tmp_nes.strip_mode = NES_TIME_TRANSITION_START;
      tmp_nes.strip = s1;
      tmp_nes.strip_time = s1->strip_time;
      nlaeval_snapshot_init(&snapshot1, channels, snapshot);
      nlasnapshot_blend_strip(ptr,
                              channels,
                              &tmp_modifiers,
                              &tmp_nes,
                              &snapshot1,
                              anim_eval_context,
                              flush_to_original);

      /* second strip */
      tmp_nes.strip_mode = NES_TIME_TRANSITION_END;
      tmp_nes.strip = s2;
      tmp_nes.strip_time = s2->strip_time;
      nlaeval_snapshot_init(&snapshot2, channels, snapshot);
      nlasnapshot_blend_strip(ptr,
                              channels,
                              &tmp_modifiers,
                              &tmp_nes,
                              &snapshot2,
                              anim_eval_context,
                              flush_to_original);

      /**
       * Replace \a snapshot2 nullptr channels with base or default values so all channels blend.
       */
      nlasnapshot_ensure_channels(channels, &snapshot2);
      /** Mark all \a snapshot2 channel's values to blend. */
      nlasnapshot_enable_all_blend_domain(&snapshot2);
      nlasnapshot_blend(
          channels, &snapshot1, &snapshot2, NLASTRIP_MODE_REPLACE, nes->strip_time, snapshot);

      nlaeval_snapshot_free_data(&snapshot1);
      nlaeval_snapshot_free_data(&snapshot2);

      break;
    }
    case STRIP_EVAL_BLEND_GET_INVERTED_LOWER_SNAPSHOT: {
      /* No support for remapping values through a transition. Mark all channel values affected by
       * transition as non-remappable. */
      tmp_nes = *nes;

      /* Process first strip. */
      tmp_nes.strip = s1;
      tmp_nes.strip_time = s1->strip_time;
      nlaeval_snapshot_init(&snapshot1, channels, snapshot);
      nlasnapshot_blend_strip_no_blend(
          ptr, channels, &tmp_modifiers, &tmp_nes, &snapshot1, anim_eval_context);

      /* Remove channel values affected by transition from the remap domain. */
      LISTBASE_FOREACH (NlaEvalChannel *, nec, &channels->channels) {
        NlaEvalChannelSnapshot *necs = nlaeval_snapshot_get(&snapshot1, nec->index);
        if (necs == nullptr) {
          continue;
        }
        NlaEvalChannelSnapshot *output_necs = nlaeval_snapshot_ensure_channel(snapshot, nec);
        for (int i = 0; i < necs->length; i++) {
          if (BLI_BITMAP_TEST_BOOL(necs->blend_domain.ptr, i)) {
            BLI_BITMAP_DISABLE(output_necs->remap_domain.ptr, i);
          }
        }
      }

      nlaeval_snapshot_free_data(&snapshot1);

      /* Process second strip. */
      tmp_nes.strip = s2;
      tmp_nes.strip_time = s2->strip_time;
      nlaeval_snapshot_init(&snapshot2, channels, snapshot);
      nlasnapshot_blend_strip_no_blend(
          ptr, channels, &tmp_modifiers, &tmp_nes, &snapshot2, anim_eval_context);

      /* Remove channel values affected by transition from the remap domain. */
      LISTBASE_FOREACH (NlaEvalChannel *, nec, &channels->channels) {
        NlaEvalChannelSnapshot *necs = nlaeval_snapshot_get(&snapshot2, nec->index);
        if (necs == nullptr) {
          continue;
        }
        NlaEvalChannelSnapshot *output_necs = nlaeval_snapshot_ensure_channel(snapshot, nec);
        for (int i = 0; i < necs->length; i++) {
          if (BLI_BITMAP_TEST_BOOL(necs->blend_domain.ptr, i)) {
            BLI_BITMAP_DISABLE(output_necs->remap_domain.ptr, i);
          }
        }
      }

      nlaeval_snapshot_free_data(&snapshot2);

      break;
    }
    case STRIP_EVAL_NOBLEND: {
      BLI_assert( !"This case shouldn't occur. Transitions assumed to not reference other "
"transitions. ");
      break;
    }
  }

  /* unlink this strip's modifiers from the parent's modifiers again */
  nlaeval_fmodifiers_split_stacks(&nes->strip->modifiers, modifiers);
}

/* evaluate meta-strip */
static void nlastrip_evaluate_meta(const int evaluation_mode,
                                   PointerRNA *ptr,
                                   NlaEvalData *channels,
                                   ListBase *modifiers,
                                   NlaEvalStrip *nes,
                                   NlaEvalSnapshot *snapshot,
                                   const AnimationEvalContext *anim_eval_context,
                                   const bool flush_to_original)
{
  ListBase tmp_modifiers = {nullptr, nullptr};
  NlaStrip *strip = nes->strip;
  NlaEvalStrip *tmp_nes;
  float evaltime;

  /* meta-strip was calculated normally to have some time to be evaluated at
   * and here we 'look inside' the meta strip, treating it as a decorated window to
   * its child strips, which get evaluated as if they were some tracks on a strip
   * (but with some extra modifiers to apply).
   *
   * NOTE: keep this in sync with animsys_evaluate_nla()
   */

  /* join this strip's modifiers to the parent's modifiers (own modifiers first) */
  nlaeval_fmodifiers_join_stacks(&tmp_modifiers, &strip->modifiers, modifiers);

  /* find the child-strip to evaluate */
  evaltime = (nes->strip_time * (strip->end - strip->start)) + strip->start;
  AnimationEvalContext child_context = BKE_animsys_eval_context_construct_at(anim_eval_context,
                                                                             evaltime);
  tmp_nes = nlastrips_ctime_get_strip(
      nullptr, &strip->strips, -1, &child_context, flush_to_original);

  /* Assert currently supported modes. If new mode added, then assertion marks potentially missed
   * area.
   *
   * NOTE: In the future if support is ever added to metastrips to support nested tracks, then
   * STRIP_EVAL_BLEND and STRIP_EVAL_BLEND_GET_INVERTED_LOWER_SNAPSHOT cases are no longer
   * equivalent. The output of nlastrips_ctime_get_strip() may return a list of strips. The only
   * case difference should be the evaluation order.
   */
  BLI_assert(ELEM(evaluation_mode,
                  STRIP_EVAL_BLEND,
                  STRIP_EVAL_BLEND_GET_INVERTED_LOWER_SNAPSHOT,
                  STRIP_EVAL_NOBLEND));

  /* directly evaluate child strip into accumulation buffer...
   * - there's no need to use a temporary buffer (as it causes issues [#40082])
   */
  if (tmp_nes) {
    nlastrip_evaluate(evaluation_mode,
                      ptr,
                      channels,
                      &tmp_modifiers,
                      tmp_nes,
                      snapshot,
                      &child_context,
                      flush_to_original);

    /* free temp eval-strip */
    MEM_freeN(tmp_nes);
  }

  /* unlink this strip's modifiers from the parent's modifiers again */
  nlaeval_fmodifiers_split_stacks(&strip->modifiers, modifiers);
}

void nlastrip_evaluate(const int evaluation_mode,
                       PointerRNA *ptr,
                       NlaEvalData *channels,
                       ListBase *modifiers,
                       NlaEvalStrip *nes,
                       NlaEvalSnapshot *snapshot,
                       const AnimationEvalContext *anim_eval_context,
                       const bool flush_to_original)
{
  NlaStrip *strip = nes->strip;

  /* To prevent potential infinite recursion problems
   * (i.e. transition strip, beside meta strip containing a transition
   * several levels deep inside it),
   * we tag the current strip as being evaluated, and clear this when we leave.
   */
  /* TODO: be careful with this flag, since some edit tools may be running and have
   * set this while animation playback was running. */
  if (strip->flag & NLASTRIP_FLAG_EDIT_TOUCHED) {
    return;
  }
  strip->flag |= NLASTRIP_FLAG_EDIT_TOUCHED;

  /* actions to take depend on the type of strip */
  switch (strip->type) {
    case NLASTRIP_TYPE_CLIP: /* action-clip */
      nlastrip_evaluate_actionclip(evaluation_mode, ptr, channels, modifiers, nes, snapshot);
      break;
    case NLASTRIP_TYPE_TRANSITION: /* transition */
      nlastrip_evaluate_transition(evaluation_mode,
                                   ptr,
                                   channels,
                                   modifiers,
                                   nes,
                                   snapshot,
                                   anim_eval_context,
                                   flush_to_original);
      break;
    case NLASTRIP_TYPE_META: /* meta */
      nlastrip_evaluate_meta(evaluation_mode,
                             ptr,
                             channels,
                             modifiers,
                             nes,
                             snapshot,
                             anim_eval_context,
                             flush_to_original);
      break;

    default: /* do nothing */
      break;
  }

  /* clear temp recursion safe-check */
  strip->flag &= ~NLASTRIP_FLAG_EDIT_TOUCHED;
}

void nlasnapshot_blend_strip(PointerRNA *ptr,
                             NlaEvalData *channels,
                             ListBase *modifiers,
                             NlaEvalStrip *nes,
                             NlaEvalSnapshot *snapshot,
                             const AnimationEvalContext *anim_eval_context,
                             const bool flush_to_original)
{
  nlastrip_evaluate(STRIP_EVAL_BLEND,
                    ptr,
                    channels,
                    modifiers,
                    nes,
                    snapshot,
                    anim_eval_context,
                    flush_to_original);
}

void nlasnapshot_blend_strip_get_inverted_lower_snapshot(
    PointerRNA *ptr,
    NlaEvalData *channels,
    ListBase *modifiers,
    NlaEvalStrip *nes,
    NlaEvalSnapshot *snapshot,
    const AnimationEvalContext *anim_eval_context)
{
  nlastrip_evaluate(STRIP_EVAL_BLEND_GET_INVERTED_LOWER_SNAPSHOT,
                    ptr,
                    channels,
                    modifiers,
                    nes,
                    snapshot,
                    anim_eval_context,
                    false);
}

void nlasnapshot_blend_strip_no_blend(PointerRNA *ptr,
                                      NlaEvalData *channels,
                                      ListBase *modifiers,
                                      NlaEvalStrip *nes,
                                      NlaEvalSnapshot *snapshot,
                                      const AnimationEvalContext *anim_eval_context)
{
  nlastrip_evaluate(
      STRIP_EVAL_NOBLEND, ptr, channels, modifiers, nes, snapshot, anim_eval_context, false);
}

void nladata_flush_channels(PointerRNA *ptr,
                            NlaEvalData *channels,
                            NlaEvalSnapshot *snapshot,
                            const bool flush_to_original)
{
  /* sanity checks */
  if (channels == nullptr) {
    return;
  }

  /* for each channel with accumulated values, write its value on the property it affects */
  LISTBASE_FOREACH (NlaEvalChannel *, nec, &channels->channels) {
    /**
     * The bitmask is set for all channels touched by NLA due to the domain() function.
     * Channels touched by current set of evaluated strips will have a snapshot channel directly
     * from the evaluation snapshot.
     *
     * This function falls back to the default value if the snapshot channel doesn't exist.
     * Thus channels, touched by NLA but not by the current set of evaluated strips, will be
     * reset to default. If channel not touched by NLA then it's value is unchanged.
     */
    NlaEvalChannelSnapshot *nec_snapshot = nlaeval_snapshot_find_channel(snapshot, nec);

    PathResolvedRNA rna = {nec->key.ptr, nec->key.prop, -1};

    for (int i = 0; i < nec_snapshot->length; i++) {
      if (BLI_BITMAP_TEST(nec->domain.ptr, i)) {
        float value = nec_snapshot->values[i];
        if (nec->is_array) {
          rna.prop_index = i;
        }
        BKE_animsys_write_to_rna_path(&rna, value);
        if (flush_to_original) {
          animsys_write_orig_anim_rna(ptr, nec->rna_path, rna.prop_index, value);
        }
      }
    }
  }
}

/* ---------------------- */

static void nla_eval_domain_action(PointerRNA *ptr,
                                   NlaEvalData *channels,
                                   bAction *act,
                                   GSet *touched_actions)
{
  if (!BLI_gset_add(touched_actions, act)) {
    return;
  }

  LISTBASE_FOREACH (FCurve *, fcu, &act->curves) {
    /* check if this curve should be skipped */
    if (!is_fcurve_evaluatable(fcu)) {
      continue;
    }

    NlaEvalChannel *nec = nlaevalchan_verify(ptr, channels, fcu->rna_path);

    if (nec != nullptr) {
      /* For quaternion properties, enable all sub-channels. */
      if (nec->mix_mode == NEC_MIX_QUATERNION) {
        BLI_bitmap_set_all(nec->domain.ptr, true, 4);
        continue;
      }

      int idx = nlaevalchan_validate_index(nec, fcu->array_index);

      if (idx >= 0) {
        BLI_BITMAP_ENABLE(nec->domain.ptr, idx);
      }
    }
  }
}

static void nla_eval_domain_strips(PointerRNA *ptr,
                                   NlaEvalData *channels,
                                   ListBase *strips,
                                   GSet *touched_actions)
{
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    /* Check strip's action. */
    if (strip->act) {
      nla_eval_domain_action(ptr, channels, strip->act, touched_actions);
    }

    /* Check sub-strips (if meta-strips). */
    nla_eval_domain_strips(ptr, channels, &strip->strips, touched_actions);
  }
}

/**
 * Ensure that all channels touched by any of the actions in enabled tracks exist.
 * This is necessary to ensure that evaluation result depends only on current frame.
 */
static void animsys_evaluate_nla_domain(PointerRNA *ptr, NlaEvalData *channels, AnimData *adt)
{
  GSet *touched_actions = BLI_gset_ptr_new(__func__);

  /* Include domain of Action Track. */
  if ((adt->flag & ADT_NLA_EDIT_ON) == 0) {
    if (adt->action) {
      nla_eval_domain_action(ptr, channels, adt->action, touched_actions);
    }
  }
  else if (adt->tmpact && (adt->flag & ADT_NLA_EVAL_UPPER_TRACKS)) {
    nla_eval_domain_action(ptr, channels, adt->tmpact, touched_actions);
  }

  /* NLA Data - Animation Data for Strips */
  LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
    /* solo and muting are mutually exclusive... */
    if (adt->flag & ADT_NLA_SOLO_TRACK) {
      /* skip if there is a solo track, but this isn't it */
      if ((nlt->flag & NLATRACK_SOLO) == 0) {
        continue;
      }
      /* else - mute doesn't matter */
    }
    else {
      /* no solo tracks - skip track if muted */
      if (nlt->flag & NLATRACK_MUTED) {
        continue;
      }
    }

    nla_eval_domain_strips(ptr, channels, &nlt->strips, touched_actions);
  }

  BLI_gset_free(touched_actions, nullptr);
}

/* ---------------------- */

/**
 * Tweaked strip is evaluated differently from other strips. Adjacent strips are ignored
 * and includes a workaround for when user is not editing in place.
 */
static void animsys_create_tweak_strip(const AnimData *adt,
                                       const bool keyframing_to_strip,
                                       NlaStrip *r_tweak_strip)

{
  /* Copy active strip so we can modify how it evaluates without affecting user data. */
  memcpy(r_tweak_strip, adt->actstrip, sizeof(NlaStrip));
  r_tweak_strip->next = r_tweak_strip->prev = nullptr;

  /* If tweaked strip is syncing action length, then evaluate using action length. */
  if (r_tweak_strip->flag & NLASTRIP_FLAG_SYNC_LENGTH) {
    BKE_nlastrip_recalculate_bounds_sync_action(r_tweak_strip);
  }

  /* Strips with a user-defined time curve don't get properly remapped for editing
   * at the moment, so mapping them just for display may be confusing. */
  const bool is_inplace_tweak = !(adt->flag & ADT_NLA_EDIT_NOMAP) &&
                                !(adt->actstrip->flag & NLASTRIP_FLAG_USR_TIME);

  if (!is_inplace_tweak) {
    /* Use Hold due to no proper remapping yet (the note above). */
    r_tweak_strip->extendmode = NLASTRIP_EXTEND_HOLD;

    /* Disable range. */
    r_tweak_strip->flag |= NLASTRIP_FLAG_NO_TIME_MAP;
  }

  /** Controls whether able to keyframe outside range of tweaked strip. */
  if (keyframing_to_strip) {
    r_tweak_strip->extendmode = (is_inplace_tweak &&
                                 !(r_tweak_strip->flag & NLASTRIP_FLAG_SYNC_LENGTH)) ?
                                    NLASTRIP_EXTEND_NOTHING :
                                    NLASTRIP_EXTEND_HOLD;
  }
}

/** Action track and strip are associated with the non-pushed action. */
static void animsys_create_action_track_strip(const AnimData *adt,
                                              const bool keyframing_to_strip,
                                              NlaStrip *r_action_strip)
{
  memset(r_action_strip, 0, sizeof(NlaStrip));

  bAction *action = adt->action;

  if (adt->flag & ADT_NLA_EDIT_ON) {
    action = adt->tmpact;
  }

  /* Set settings of dummy NLA strip from AnimData settings. */
  r_action_strip->act = action;

  /* Action range is calculated taking F-Modifiers into account
   * (which making new strips doesn't do due to the troublesome nature of that). */
  BKE_action_frame_range_calc(
      r_action_strip->act, true, &r_action_strip->actstart, &r_action_strip->actend);
  r_action_strip->start = r_action_strip->actstart;
  r_action_strip->end = IS_EQF(r_action_strip->actstart, r_action_strip->actend) ?
                            (r_action_strip->actstart + 1.0f) :
                            (r_action_strip->actend);

  r_action_strip->blendmode = adt->act_blendmode;
  r_action_strip->extendmode = adt->act_extendmode;
  r_action_strip->influence = adt->act_influence;

  /* NOTE: must set this, or else the default setting overrides,
   * and this setting doesn't work. */
  r_action_strip->flag |= NLASTRIP_FLAG_USR_INFLUENCE;

  /* Unless `extendmode` is Nothing (might be useful for flattening NLA evaluation), disable range.
   * Extend-mode Nothing and Hold will behave as normal. Hold Forward will behave just like Hold.
   */
  if (r_action_strip->extendmode != NLASTRIP_EXTEND_NOTHING) {
    r_action_strip->flag |= NLASTRIP_FLAG_NO_TIME_MAP;
  }

  const bool tweaking = (adt->flag & ADT_NLA_EDIT_ON) != 0;
  const bool soloing = (adt->flag & ADT_NLA_SOLO_TRACK) != 0;
  const bool eval_upper = !tweaking || (adt->flag & ADT_NLA_EVAL_UPPER_TRACKS) != 0;
  const bool actionstrip_evaluated = r_action_strip->act && !soloing && eval_upper;
  if (!actionstrip_evaluated) {
    r_action_strip->flag |= NLASTRIP_FLAG_MUTED;
  }

  /** If we're keyframing, then we must allow keyframing outside fcurve bounds. */
  if (keyframing_to_strip) {
    r_action_strip->extendmode = NLASTRIP_EXTEND_HOLD;
  }
}

static bool is_nlatrack_evaluatable(const AnimData *adt, const NlaTrack *nlt)
{
  /* Skip disabled tracks unless it contains the tweaked strip. */
  const bool contains_tweak_strip = (adt->flag & ADT_NLA_EDIT_ON) && adt->act_track &&
                                    (nlt->index == adt->act_track->index);
  if ((nlt->flag & NLATRACK_DISABLED) && !contains_tweak_strip) {
    return false;
  }

  /* Solo and muting are mutually exclusive. */
  if (adt->flag & ADT_NLA_SOLO_TRACK) {
    /* Skip if there is a solo track, but this isn't it. */
    if ((nlt->flag & NLATRACK_SOLO) == 0) {
      return false;
    }
  }
  else {
    /* Skip track if muted. */
    if (nlt->flag & NLATRACK_MUTED) {
      return false;
    }
  }

  return true;
}

/**
 * Check for special case of non-pushed action being evaluated with no NLA influence (off and no
 * strips evaluated) nor NLA interference (ensure NLA not soloing).
 */
static bool is_action_track_evaluated_without_nla(const AnimData *adt,
                                                  const bool any_strip_evaluated)
{
  if (adt->action == nullptr) {
    return false;
  }

  if (any_strip_evaluated) {
    return false;
  }

  /** NLA settings interference. */
  if ((adt->flag & (ADT_NLA_SOLO_TRACK | ADT_NLA_EDIT_ON)) != 0) {
    return false;
  }

  /** Allow action track to evaluate as if there isn't any NLA data. */
  return true;
}

/**
 * XXX(Wayde Moss): #BKE_nlatrack_find_tweaked() exists within `nla.cc`, but it doesn't appear to
 * work as expected. From #animsys_evaluate_nla_for_flush(), it returns nullptr in tweak mode. I'm
 * not sure why. Preferably, it would be as simple as checking for `(adt->act_Track == nlt)` but
 * that doesn't work either, neither does comparing indices.
 *
 * This function is a temporary work around. The first disabled track is always the tweaked track.
 */
static NlaTrack *nlatrack_find_tweaked(const AnimData *adt)
{
  if (adt == nullptr) {
    return nullptr;
  }

  /* Since the track itself gets disabled, we want the first disabled. */
  LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
    if (nlt->flag & NLATRACK_DISABLED) {
      return nlt;
    }
  }

  return nullptr;
}

/**
 * NLA Evaluation function - values are calculated and stored in temporary "NlaEvalChannels"
 * \param[out] echannels: Evaluation channels with calculated values
 */
static bool animsys_evaluate_nla_for_flush(NlaEvalData *echannels,
                                           PointerRNA *ptr,
                                           const AnimData *adt,
                                           const AnimationEvalContext *anim_eval_context,
                                           const bool flush_to_original)
{
  NlaTrack *nlt;
  short track_index = 0;
  bool has_strips = false;
  ListBase estrips = {nullptr, nullptr};
  NlaEvalStrip *nes;

  NlaStrip tweak_strip;

  NlaTrack *tweaked_track = nlatrack_find_tweaked(adt);

  /* Get the stack of strips to evaluate at current time (influence calculated here). */
  for (nlt = static_cast<NlaTrack *>(adt->nla_tracks.first); nlt; nlt = nlt->next, track_index++) {

    if (!is_nlatrack_evaluatable(adt, nlt)) {
      continue;
    }

    if (nlt->strips.first) {
      has_strips = true;
    }

    /** Append strip to evaluate for this track. */
    if (nlt == tweaked_track) {
      /** Tweaked strip is evaluated differently. */
      animsys_create_tweak_strip(adt, false, &tweak_strip);
      nes = nlastrips_ctime_get_strip_single(
          &estrips, &tweak_strip, anim_eval_context, flush_to_original);
    }
    else {
      nes = nlastrips_ctime_get_strip(
          &estrips, &nlt->strips, track_index, anim_eval_context, flush_to_original);
    }
    if (nes) {
      nes->track = nlt;
    }
  }

  if (is_action_track_evaluated_without_nla(adt, has_strips)) {
    BLI_freelistN(&estrips);
    return false;
  }

  NlaStrip action_strip = {nullptr};
  animsys_create_action_track_strip(adt, false, &action_strip);
  nlastrips_ctime_get_strip_single(&estrips, &action_strip, anim_eval_context, flush_to_original);

  /* Per strip, evaluate and accumulate on top of existing channels. */
  LISTBASE_FOREACH (NlaEvalStrip *, nes, &estrips) {
    nlasnapshot_blend_strip(ptr,
                            echannels,
                            nullptr,
                            nes,
                            &echannels->eval_snapshot,
                            anim_eval_context,
                            flush_to_original);
  }

  /* Free temporary evaluation data that's not used elsewhere. */
  BLI_freelistN(&estrips);
  return true;
}

/** Lower blended values are calculated and accumulated into r_context->lower_eval_data. */
static void animsys_evaluate_nla_for_keyframing(PointerRNA *ptr,
                                                const AnimData *adt,
                                                const AnimationEvalContext *anim_eval_context,
                                                NlaKeyframingContext *r_context)
{
  if (!r_context) {
    return;
  }

  /* Early out. If NLA track is soloing and tweaked action isn't it, then don't allow keyframe
   * insertion. */
  if (adt->flag & ADT_NLA_SOLO_TRACK) {
    if (!(adt->act_track && (adt->act_track->flag & NLATRACK_SOLO))) {
      r_context->eval_strip = nullptr;
      return;
    }
  }

  NlaTrack *nlt;
  short track_index = 0;
  bool has_strips = false;

  ListBase *upper_estrips = &r_context->upper_estrips;
  ListBase lower_estrips = {nullptr, nullptr};
  NlaEvalStrip *nes;

  NlaTrack *tweaked_track = nlatrack_find_tweaked(adt);

  /* Get the lower stack of strips to evaluate at current time (influence calculated here). */
  for (nlt = static_cast<NlaTrack *>(adt->nla_tracks.first); nlt; nlt = nlt->next, track_index++) {

    if (!is_nlatrack_evaluatable(adt, nlt)) {
      continue;
    }

    /* Tweaked strip effect should not be stored in any snapshot. */
    if (nlt == tweaked_track) {
      break;
    }

    if (nlt->strips.first) {
      has_strips = true;
    }

    /* Get strip to evaluate for this channel. */
    nes = nlastrips_ctime_get_strip(
        &lower_estrips, &nlt->strips, track_index, anim_eval_context, false);
    if (nes) {
      nes->track = nlt;
    }
  }

  /* Get the upper stack of strips to evaluate at current time (influence calculated here).
   * Var nlt exists only if tweak strip exists. */
  if (nlt) {

    /* Skip tweaked strip. */
    nlt = nlt->next;
    track_index++;

    for (; nlt; nlt = nlt->next, track_index++) {

      if (!is_nlatrack_evaluatable(adt, nlt)) {
        continue;
      }

      if (nlt->strips.first) {
        has_strips = true;
      }

      /* Get strip to evaluate for this channel. */
      nes = nlastrips_ctime_get_strip(
          upper_estrips, &nlt->strips, track_index, anim_eval_context, false);
    }
  }

  /* NOTE: Although we early out, we can still keyframe to the non-pushed action since the
   * keyframe remap function detects (r_context->strip.act == nullptr) and will keyframe without
   * remapping. */
  if (is_action_track_evaluated_without_nla(adt, has_strips)) {
    BLI_freelistN(&lower_estrips);
    return;
  }

  /* Write r_context->eval_strip. */
  if (adt->flag & ADT_NLA_EDIT_ON) {
    /* Append action_track_strip to upper estrips. */
    NlaStrip *action_strip = &r_context->action_track_strip;
    animsys_create_action_track_strip(adt, false, action_strip);
    nlastrips_ctime_get_strip_single(upper_estrips, action_strip, anim_eval_context, false);

    NlaStrip *tweak_strip = &r_context->strip;
    animsys_create_tweak_strip(adt, true, tweak_strip);
    r_context->eval_strip = nlastrips_ctime_get_strip_single(
        nullptr, tweak_strip, anim_eval_context, false);
  }
  else {

    NlaStrip *action_strip = &r_context->strip;
    animsys_create_action_track_strip(adt, true, action_strip);
    r_context->eval_strip = nlastrips_ctime_get_strip_single(
        nullptr, action_strip, anim_eval_context, false);
  }

  /* If nullptr, then keyframing will fail. No need to do any more processing. */
  if (!r_context->eval_strip) {
    BLI_freelistN(&lower_estrips);
    return;
  }

  /* If tweak strip is full REPLACE, then lower strips not needed. */
  if (r_context->strip.blendmode == NLASTRIP_MODE_REPLACE &&
      IS_EQF(r_context->strip.influence, 1.0f))
  {
    BLI_freelistN(&lower_estrips);
    return;
  }

  /* For each strip, evaluate then accumulate on top of existing channels. */
  LISTBASE_FOREACH (NlaEvalStrip *, nes, &lower_estrips) {
    nlasnapshot_blend_strip(ptr,
                            &r_context->lower_eval_data,
                            nullptr,
                            nes,
                            &r_context->lower_eval_data.eval_snapshot,
                            anim_eval_context,
                            false);
  }

  /* Free temporary evaluation data that's not used elsewhere. */
  BLI_freelistN(&lower_estrips);
}

/* NLA Evaluation function (mostly for use through do_animdata)
 * - All channels that will be affected are not cleared anymore. Instead, we just evaluate into
 *   some temp channels, where values can be accumulated in one go.
 */
static void animsys_calculate_nla(PointerRNA *ptr,
                                  AnimData *adt,
                                  const AnimationEvalContext *anim_eval_context,
                                  const bool flush_to_original)
{
  NlaEvalData echannels;

  nlaeval_init(&echannels);

  /* evaluate the NLA stack, obtaining a set of values to flush */
  if (animsys_evaluate_nla_for_flush(&echannels, ptr, adt, anim_eval_context, flush_to_original)) {
    /* reset any channels touched by currently inactive actions to default value */
    animsys_evaluate_nla_domain(ptr, &echannels, adt);

    /* flush effects of accumulating channels in NLA to the actual data they affect */
    nladata_flush_channels(ptr, &echannels, &echannels.eval_snapshot, flush_to_original);
  }
  else {
    /* special case - evaluate as if there isn't any NLA data */
    /* TODO: this is really just a stop-gap measure... */
    if (G.debug & G_DEBUG) {
      CLOG_WARN(&LOG, "NLA Eval: Stopgap for active action on NLA Stack - no strips case");
    }

    animsys_evaluate_action(ptr, adt->action, anim_eval_context, flush_to_original);
  }

  /* free temp data */
  nlaeval_free(&echannels);
}

/* ---------------------- */

void nlasnapshot_enable_all_blend_domain(NlaEvalSnapshot *snapshot)
{
  for (int i = 0; i < snapshot->size; i++) {
    NlaEvalChannelSnapshot *necs = nlaeval_snapshot_get(snapshot, i);
    if (necs == nullptr) {
      continue;
    }

    BLI_bitmap_set_all(necs->blend_domain.ptr, true, necs->length);
  }
}

void nlasnapshot_ensure_channels(NlaEvalData *eval_data, NlaEvalSnapshot *snapshot)
{
  LISTBASE_FOREACH (NlaEvalChannel *, nec, &eval_data->channels) {
    nlaeval_snapshot_ensure_channel(snapshot, nec);
  }
}

void nlasnapshot_blend(NlaEvalData *eval_data,
                       NlaEvalSnapshot *lower_snapshot,
                       NlaEvalSnapshot *upper_snapshot,
                       const short upper_blendmode,
                       const float upper_influence,
                       NlaEvalSnapshot *r_blended_snapshot)
{
  nlaeval_snapshot_ensure_size(r_blended_snapshot, eval_data->num_channels);

  LISTBASE_FOREACH (NlaEvalChannel *, nec, &eval_data->channels) {
    NlaEvalChannelSnapshot *upper_necs = nlaeval_snapshot_get(upper_snapshot, nec->index);
    NlaEvalChannelSnapshot *lower_necs = nlaeval_snapshot_get(lower_snapshot, nec->index);
    if (upper_necs == nullptr && lower_necs == nullptr) {
      continue;
    }

    /** Blend with lower_snapshot's base or default. */
    if (lower_necs == nullptr) {
      lower_necs = nlaeval_snapshot_find_channel(lower_snapshot->base, nec);
    }

    NlaEvalChannelSnapshot *result_necs = nlaeval_snapshot_ensure_channel(r_blended_snapshot, nec);
    nlaevalchan_blendOrcombine(
        lower_necs, upper_necs, upper_blendmode, upper_influence, result_necs);
  }
}

void nlasnapshot_blend_get_inverted_upper_snapshot(NlaEvalData *eval_data,
                                                   NlaEvalSnapshot *lower_snapshot,
                                                   NlaEvalSnapshot *blended_snapshot,
                                                   const short upper_blendmode,
                                                   const float upper_influence,
                                                   NlaEvalSnapshot *r_upper_snapshot)
{
  nlaeval_snapshot_ensure_size(r_upper_snapshot, eval_data->num_channels);

  LISTBASE_FOREACH (NlaEvalChannel *, nec, &eval_data->channels) {
    NlaEvalChannelSnapshot *blended_necs = nlaeval_snapshot_get(blended_snapshot, nec->index);
    if (blended_necs == nullptr) {
      /** We assume the caller only wants a subset of channels to be inverted, those that exist
       * within \a blended_snapshot. */
      continue;
    }

    NlaEvalChannelSnapshot *lower_necs = nlaeval_snapshot_get(lower_snapshot, nec->index);
    if (lower_necs == nullptr) {
      lower_necs = nlaeval_snapshot_find_channel(lower_snapshot->base, nec);
    }

    NlaEvalChannelSnapshot *result_necs = nlaeval_snapshot_ensure_channel(r_upper_snapshot, nec);
    nlaevalchan_blendOrcombine_get_inverted_upper_evalchan(
        lower_necs, blended_necs, upper_blendmode, upper_influence, result_necs);
  }
}

void nlasnapshot_blend_get_inverted_lower_snapshot(NlaEvalData *eval_data,
                                                   NlaEvalSnapshot *blended_snapshot,
                                                   NlaEvalSnapshot *upper_snapshot,
                                                   const short upper_blendmode,
                                                   const float upper_influence,
                                                   NlaEvalSnapshot *r_lower_snapshot)
{
  nlaeval_snapshot_ensure_size(r_lower_snapshot, eval_data->num_channels);

  LISTBASE_FOREACH (NlaEvalChannel *, nec, &eval_data->channels) {
    NlaEvalChannelSnapshot *blended_necs = nlaeval_snapshot_get(blended_snapshot, nec->index);
    if (blended_necs == nullptr) {
      /* We assume the caller only wants a subset of channels to be inverted, those that exist
       * within \a blended_snapshot. */
      continue;
    }

    NlaEvalChannelSnapshot *upper_necs = nlaeval_snapshot_get(upper_snapshot, nec->index);
    NlaEvalChannelSnapshot *result_necs = nlaeval_snapshot_ensure_channel(r_lower_snapshot, nec);

    nlaevalchan_blendOrCombine_get_inverted_lower_evalchan(
        blended_necs, upper_necs, upper_blendmode, upper_influence, result_necs);
  }
}

/* ---------------------- */

NlaKeyframingContext *BKE_animsys_get_nla_keyframing_context(
    ListBase *cache, PointerRNA *ptr, AnimData *adt, const AnimationEvalContext *anim_eval_context)
{
  /* No remapping needed if NLA is off or no action. */
  if ((adt == nullptr) || (adt->action == nullptr) || (adt->nla_tracks.first == nullptr) ||
      (adt->flag & ADT_NLA_EVAL_OFF))
  {
    return nullptr;
  }

  /* No remapping if editing an ordinary Replace action with full influence and upper tracks not
   * evaluated. */
  if (!(adt->flag & ADT_NLA_EDIT_ON) &&
      (adt->act_blendmode == NLASTRIP_MODE_REPLACE && adt->act_influence == 1.0f) &&
      (adt->flag & ADT_NLA_EVAL_UPPER_TRACKS) == 0)
  {
    return nullptr;
  }

  /* Try to find a cached context. */
  NlaKeyframingContext *ctx = static_cast<NlaKeyframingContext *>(
      BLI_findptr(cache, adt, offsetof(NlaKeyframingContext, adt)));

  if (ctx == nullptr) {
    /* Allocate and evaluate a new context. */
    ctx = static_cast<NlaKeyframingContext *>(MEM_callocN(sizeof(*ctx), "NlaKeyframingContext"));
    ctx->adt = adt;

    nlaeval_init(&ctx->lower_eval_data);
    animsys_evaluate_nla_for_keyframing(ptr, adt, anim_eval_context, ctx);

    BLI_assert(ELEM(ctx->strip.act, nullptr, adt->action));
    BLI_addtail(cache, ctx);
  }

  return ctx;
}

void BKE_animsys_nla_remap_keyframe_values(NlaKeyframingContext *context,
                                           PointerRNA *prop_ptr,
                                           PropertyRNA *prop,
                                           float *values,
                                           int count,
                                           int index,
                                           const AnimationEvalContext *anim_eval_context,
                                           bool *r_force_all,
                                           BLI_bitmap *r_successful_remaps)
{
  BLI_bitmap_set_all(r_successful_remaps, false, count);

  if (r_force_all != nullptr) {
    *r_force_all = false;
  }

  BLI_bitmap *remap_domain = BLI_BITMAP_NEW(count, __func__);
  for (int i = 0; i < count; i++) {
    if (!ELEM(index, i, -1)) {
      continue;
    }

    BLI_BITMAP_ENABLE(remap_domain, i);
  }

  /* No context means no correction. */
  if (context == nullptr || context->strip.act == nullptr) {
    BLI_bitmap_copy_all(r_successful_remaps, remap_domain, count);
    MEM_freeN(remap_domain);
    return;
  }

  /* If the strip is not evaluated, it is the same as zero influence. */
  if (context->eval_strip == nullptr) {
    MEM_freeN(remap_domain);
    return;
  }

  /* Full influence Replace strips also require no correction if there are no upper tracks
   * evaluating. */
  int blend_mode = context->strip.blendmode;
  float influence = context->strip.influence;

  if (blend_mode == NLASTRIP_MODE_REPLACE && influence == 1.0f &&
      BLI_listbase_is_empty(&context->upper_estrips))
  {
    BLI_bitmap_copy_all(r_successful_remaps, remap_domain, count);
    MEM_freeN(remap_domain);
    return;
  }

  /* Zero influence is division by zero. */
  if (influence <= 0.0f) {
    MEM_freeN(remap_domain);
    return;
  }

  /** Create \a blended_snapshot and fill with input \a values. */
  NlaEvalData *eval_data = &context->lower_eval_data;
  NlaEvalSnapshot blended_snapshot;
  nlaeval_snapshot_init(&blended_snapshot, eval_data, nullptr);

  NlaEvalChannelKey key{};
  key.ptr = *prop_ptr;
  key.prop = prop;

  NlaEvalChannel *nec = nlaevalchan_verify_key(eval_data, nullptr, &key);
  BLI_assert(nec);
  if (nec->base_snapshot.length != count) {
    BLI_assert_msg(0, "invalid value count");
    nlaeval_snapshot_free_data(&blended_snapshot);
    MEM_freeN(remap_domain);
    return;
  }

  NlaEvalChannelSnapshot *blended_necs = nlaeval_snapshot_ensure_channel(&blended_snapshot, nec);
  memcpy(blended_necs->values, values, sizeof(float) * count);

  /* Force all channels to be remapped for quaternions in a Combine or Replace strip, otherwise it
   * will always fail. See nlaevalchan_combine_quaternion_handle_undefined_blend_values().
   */
  const bool can_force_all = r_force_all != nullptr;
  if (blended_necs->channel->mix_mode == NEC_MIX_QUATERNION &&
      ELEM(blend_mode, NLASTRIP_MODE_COMBINE, NLASTRIP_MODE_REPLACE) && can_force_all)
  {

    *r_force_all = true;
    index = -1;
    BLI_bitmap_set_all(remap_domain, true, 4);
  }

  BLI_bitmap_copy_all(blended_necs->remap_domain.ptr, remap_domain, count);

  /* Need to send id_ptr instead of prop_ptr so fcurve RNA paths resolve properly. */
  PointerRNA id_ptr;
  RNA_id_pointer_create(prop_ptr->owner_id, &id_ptr);

  /* Per iteration, remove effect of upper strip which gives output of nla stack below it. */
  LISTBASE_FOREACH_BACKWARD (NlaEvalStrip *, nes, &context->upper_estrips) {
    /* This will disable blended_necs->remap_domain bits if an upper strip is not invertible
     * (full replace, multiply zero, or transition). Then there is no remap solution. */
    nlasnapshot_blend_strip_get_inverted_lower_snapshot(
        &id_ptr, eval_data, nullptr, nes, &blended_snapshot, anim_eval_context);
  }

  /** Remove lower NLA stack effects. */
  nlasnapshot_blend_get_inverted_upper_snapshot(eval_data,
                                                &context->lower_eval_data.eval_snapshot,
                                                &blended_snapshot,
                                                blend_mode,
                                                influence,
                                                &blended_snapshot);

  /* Write results into \a values for successfully remapped values. */
  for (int i = 0; i < count; i++) {
    if (!BLI_BITMAP_TEST_BOOL(blended_necs->remap_domain.ptr, i)) {
      continue;
    }
    values[i] = blended_necs->values[i];
  }

  BLI_bitmap_copy_all(r_successful_remaps, blended_necs->remap_domain.ptr, blended_necs->length);

  nlaeval_snapshot_free_data(&blended_snapshot);
  MEM_freeN(remap_domain);
}

void BKE_animsys_free_nla_keyframing_context_cache(ListBase *cache)
{
  LISTBASE_FOREACH (NlaKeyframingContext *, ctx, cache) {
    MEM_SAFE_FREE(ctx->eval_strip);
    BLI_freelistN(&ctx->upper_estrips);
    nlaeval_free(&ctx->lower_eval_data);
  }

  BLI_freelistN(cache);
}

/* ***************************************** */
/* Overrides System - Public API */

/* Evaluate Overrides */
static void animsys_evaluate_overrides(PointerRNA *ptr, AnimData *adt)
{
  /* for each override, simply execute... */
  LISTBASE_FOREACH (AnimOverride *, aor, &adt->overrides) {
    PathResolvedRNA anim_rna;
    if (BKE_animsys_rna_path_resolve(ptr, aor->rna_path, aor->array_index, &anim_rna)) {
      BKE_animsys_write_to_rna_path(&anim_rna, aor->value);
    }
  }
}

/* ***************************************** */
/* Evaluation System - Public API */

/* Overview of how this system works:
 * 1) Depsgraph sorts data as necessary, so that data is in an order that means
 *     that all dependencies are resolved before dependents.
 * 2) All normal animation is evaluated, so that drivers have some basis values to
 *    work with
 *    a.  NLA stacks are done first, as the Active Actions act as 'tweaking' tracks
 *        which modify the effects of the NLA-stacks
 *    b.  Active Action is evaluated as per normal, on top of the results of the NLA tracks
 *
 * --------------< often in a separate phase... >------------------
 *
 * 3) Drivers/expressions are evaluated on top of this, in an order where dependencies are
 *    resolved nicely.
 *    NOTE: it may be necessary to have some tools to handle the cases where some higher-level
 *          drivers are added and cause some problematic dependencies that
 *          didn't exist in the local levels...
 *
 * --------------< always executed >------------------
 *
 * Maintenance of editability of settings (XXX):
 * - In order to ensure that settings that are animated can still be manipulated in the UI without
 *   requiring that keyframes are added to prevent these values from being overwritten,
 *   we use 'overrides'.
 *
 * Unresolved things:
 * - Handling of multi-user settings (i.e. time-offset, group-instancing) -> big cache grids
 *   or nodal system? but stored where?
 * - Multiple-block dependencies
 *   (i.e. drivers for settings are in both local and higher levels) -> split into separate lists?
 *
 * Current Status:
 * - Currently (as of September 2009), overrides we haven't needed to (fully) implement overrides.
 *   However, the code for this is relatively harmless, so is left in the code for now.
 */

void BKE_animsys_evaluate_animdata(ID *id,
                                   AnimData *adt,
                                   const AnimationEvalContext *anim_eval_context,
                                   eAnimData_Recalc recalc,
                                   const bool flush_to_original)
{
  PointerRNA id_ptr;

  /* sanity checks */
  if (ELEM(nullptr, id, adt)) {
    return;
  }

  /* get pointer to ID-block for RNA to use */
  RNA_id_pointer_create(id, &id_ptr);

  /* recalculate keyframe data:
   * - NLA before Active Action, as Active Action behaves as 'tweaking track'
   *   that overrides 'rough' work in NLA
   */
  /* TODO: need to double check that this all works correctly */
  if (recalc & ADT_RECALC_ANIM) {
    /* evaluate NLA data */
    if ((adt->nla_tracks.first) && !(adt->flag & ADT_NLA_EVAL_OFF)) {
      /* evaluate NLA-stack
       * - active action is evaluated as part of the NLA stack as the last item
       */
      animsys_calculate_nla(&id_ptr, adt, anim_eval_context, flush_to_original);
    }
    /* evaluate Active Action only */
    else if (adt->action) {
      animsys_evaluate_action(&id_ptr, adt->action, anim_eval_context, flush_to_original);
    }
  }

  /* recalculate drivers
   * - Drivers need to be evaluated afterwards, as they can either override
   *   or be layered on top of existing animation data.
   * - Drivers should be in the appropriate order to be evaluated without problems...
   */
  if (recalc & ADT_RECALC_DRIVERS) {
    animsys_evaluate_drivers(&id_ptr, adt, anim_eval_context);
  }

  /* always execute 'overrides'
   * - Overrides allow editing, by overwriting the value(s) set from animation-data, with the
   *   value last set by the user (and not keyframed yet).
   * - Overrides are cleared upon frame change and/or keyframing
   * - It is best that we execute this every time, so that no errors are likely to occur.
   */
  animsys_evaluate_overrides(&id_ptr, adt);
}

void BKE_animsys_evaluate_all_animation(Main *main, Depsgraph *depsgraph, float ctime)
{
  ID *id;

  if (G.debug & G_DEBUG) {
    printf("Evaluate all animation - %f\n", ctime);
  }

  const bool flush_to_original = DEG_is_active(depsgraph);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                    ctime);

/* macros for less typing
 * - only evaluate animation data for id if it has users (and not just fake ones)
 * - whether animdata exists is checked for by the evaluation function, though taking
 *   this outside of the function may make things slightly faster?
 */
#define EVAL_ANIM_IDS(first, aflag) \
  for (id = static_cast<ID *>(first); id; id = static_cast<ID *>(id->next)) { \
    if (ID_REAL_USERS(id) > 0) { \
      AnimData *adt = BKE_animdata_from_id(id); \
      BKE_animsys_evaluate_animdata(id, adt, &anim_eval_context, aflag, flush_to_original); \
    } \
  } \
  (void)0

/* Another macro for the "embedded" node-tree cases
 * - This is like #EVAL_ANIM_IDS, but this handles the case "embedded node-trees"
 *   (i.e. `scene/material/texture->nodetree`) which we need a special exception
 *   for, otherwise they'd get skipped.
 * - `ntp` stands for "node tree parent" = data-block where node tree stuff resides.
 */
#define EVAL_ANIM_NODETREE_IDS(first, NtId_Type, aflag) \
  for (id = static_cast<ID *>(first); id; id = static_cast<ID *>(id->next)) { \
    if (ID_REAL_USERS(id) > 0) { \
      AnimData *adt = BKE_animdata_from_id(id); \
      NtId_Type *ntp = (NtId_Type *)id; \
      if (ntp->nodetree) { \
        AnimData *adt2 = BKE_animdata_from_id((ID *)ntp->nodetree); \
        BKE_animsys_evaluate_animdata( \
            &ntp->nodetree->id, adt2, &anim_eval_context, ADT_RECALC_ANIM, flush_to_original); \
      } \
      BKE_animsys_evaluate_animdata(id, adt, &anim_eval_context, aflag, flush_to_original); \
    } \
  } \
  (void)0

  /* optimization:
   * when there are no actions, don't go over database and loop over heaps of data-blocks,
   * which should ultimately be empty, since it is not possible for now to have any animation
   * without some actions, and drivers wouldn't get affected by any state changes
   *
   * however, if there are some curves, we will need to make sure that their 'ctime' property gets
   * set correctly, so this optimization must be skipped in that case...
   */
  if (BLI_listbase_is_empty(&main->actions) && BLI_listbase_is_empty(&main->curves)) {
    if (G.debug & G_DEBUG) {
      printf("\tNo Actions, so no animation needs to be evaluated...\n");
    }

    return;
  }

  /* nodes */
  EVAL_ANIM_IDS(main->nodetrees.first, ADT_RECALC_ANIM);

  /* textures */
  EVAL_ANIM_NODETREE_IDS(main->textures.first, Tex, ADT_RECALC_ANIM);

  /* lights */
  EVAL_ANIM_NODETREE_IDS(main->lights.first, Light, ADT_RECALC_ANIM);

  /* materials */
  EVAL_ANIM_NODETREE_IDS(main->materials.first, Material, ADT_RECALC_ANIM);

  /* cameras */
  EVAL_ANIM_IDS(main->cameras.first, ADT_RECALC_ANIM);

  /* shapekeys */
  EVAL_ANIM_IDS(main->shapekeys.first, ADT_RECALC_ANIM);

  /* metaballs */
  EVAL_ANIM_IDS(main->metaballs.first, ADT_RECALC_ANIM);

  /* curves */
  EVAL_ANIM_IDS(main->curves.first, ADT_RECALC_ANIM);

  /* armatures */
  EVAL_ANIM_IDS(main->armatures.first, ADT_RECALC_ANIM);

  /* lattices */
  EVAL_ANIM_IDS(main->lattices.first, ADT_RECALC_ANIM);

  /* meshes */
  EVAL_ANIM_IDS(main->meshes.first, ADT_RECALC_ANIM);

  /* particles */
  EVAL_ANIM_IDS(main->particles.first, ADT_RECALC_ANIM);

  /* speakers */
  EVAL_ANIM_IDS(main->speakers.first, ADT_RECALC_ANIM);

  /* movie clips */
  EVAL_ANIM_IDS(main->movieclips.first, ADT_RECALC_ANIM);

  /* linestyles */
  EVAL_ANIM_IDS(main->linestyles.first, ADT_RECALC_ANIM);

  /* grease pencil */
  EVAL_ANIM_IDS(main->gpencils.first, ADT_RECALC_ANIM);

  /* palettes */
  EVAL_ANIM_IDS(main->palettes.first, ADT_RECALC_ANIM);

  /* cache files */
  EVAL_ANIM_IDS(main->cachefiles.first, ADT_RECALC_ANIM);

  /* Hair Curves. */
  EVAL_ANIM_IDS(main->hair_curves.first, ADT_RECALC_ANIM);

  /* pointclouds */
  EVAL_ANIM_IDS(main->pointclouds.first, ADT_RECALC_ANIM);

  /* volumes */
  EVAL_ANIM_IDS(main->volumes.first, ADT_RECALC_ANIM);

  /* objects */
  /* ADT_RECALC_ANIM doesn't need to be supplied here, since object AnimData gets
   * this tagged by Depsgraph on frame-change. This optimization means that objects
   * linked from other (not-visible) scenes will not need their data calculated.
   */
  EVAL_ANIM_IDS(main->objects.first, eAnimData_Recalc(0));

  /* masks */
  EVAL_ANIM_IDS(main->masks.first, ADT_RECALC_ANIM);

  /* worlds */
  EVAL_ANIM_NODETREE_IDS(main->worlds.first, World, ADT_RECALC_ANIM);

  /* scenes */
  EVAL_ANIM_NODETREE_IDS(main->scenes.first, Scene, ADT_RECALC_ANIM);
}

/* ***************************************** */

/* ************** */
/* Evaluation API */

void BKE_animsys_eval_animdata(Depsgraph *depsgraph, ID *id)
{
  float ctime = DEG_get_ctime(depsgraph);
  AnimData *adt = BKE_animdata_from_id(id);
  /* XXX: this is only needed for flushing RNA updates,
   * which should get handled as part of the dependency graph instead. */
  DEG_debug_print_eval_time(depsgraph, __func__, id->name, id, ctime);
  const bool flush_to_original = DEG_is_active(depsgraph);

  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                    ctime);
  BKE_animsys_evaluate_animdata(id, adt, &anim_eval_context, ADT_RECALC_ANIM, flush_to_original);
}

void BKE_animsys_update_driver_array(ID *id)
{
  AnimData *adt = BKE_animdata_from_id(id);

  /* Runtime driver map to avoid O(n^2) lookups in BKE_animsys_eval_driver.
   * Ideally the depsgraph could pass a pointer to the COW driver directly,
   * but this is difficult in the current design. */
  if (adt && adt->drivers.first) {
    BLI_assert(!adt->driver_array);

    int num_drivers = BLI_listbase_count(&adt->drivers);
    adt->driver_array = static_cast<FCurve **>(
        MEM_mallocN(sizeof(FCurve *) * num_drivers, "adt->driver_array"));

    int driver_index = 0;
    LISTBASE_FOREACH (FCurve *, fcu, &adt->drivers) {
      adt->driver_array[driver_index++] = fcu;
    }
  }
}

void BKE_animsys_eval_driver(Depsgraph *depsgraph, ID *id, int driver_index, FCurve *fcu_orig)
{
  BLI_assert(fcu_orig != nullptr);

  /* TODO(sergey): De-duplicate with BKE animsys. */
  PointerRNA id_ptr;
  bool ok = false;

  /* Lookup driver, accelerated with driver array map. */
  const AnimData *adt = BKE_animdata_from_id(id);
  FCurve *fcu;

  if (adt->driver_array) {
    fcu = adt->driver_array[driver_index];
  }
  else {
    fcu = static_cast<FCurve *>(BLI_findlink(&adt->drivers, driver_index));
  }

  DEG_debug_print_eval_subdata_index(
      depsgraph, __func__, id->name, id, "fcu", fcu->rna_path, fcu, fcu->array_index);

  RNA_id_pointer_create(id, &id_ptr);

  /* check if this driver's curve should be skipped */
  if ((fcu->flag & (FCURVE_MUTED | FCURVE_DISABLED)) == 0) {
    /* check if driver itself is tagged for recalculation */
    /* XXX driver recalc flag is not set yet by depsgraph! */
    ChannelDriver *driver_orig = fcu_orig->driver;
    if ((driver_orig) && !(driver_orig->flag & DRIVER_FLAG_INVALID)) {
      /* evaluate this using values set already in other places
       * NOTE: for 'layering' option later on, we should check if we should remove old value before
       * adding new to only be done when drivers only changed */
      // printf("\told val = %f\n", fcu->curval);

      PathResolvedRNA anim_rna;
      if (BKE_animsys_rna_path_resolve(&id_ptr, fcu->rna_path, fcu->array_index, &anim_rna)) {
        /* Evaluate driver, and write results to COW-domain destination */
        const float ctime = DEG_get_ctime(depsgraph);
        const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
            depsgraph, ctime);
        const float curval = calculate_fcurve(&anim_rna, fcu, &anim_eval_context);
        ok = BKE_animsys_write_to_rna_path(&anim_rna, curval);

        /* Flush results & status codes to original data for UI (#59984) */
        if (ok && DEG_is_active(depsgraph)) {
          animsys_write_orig_anim_rna(&id_ptr, fcu->rna_path, fcu->array_index, curval);

          /* curval is displayed in the UI, and flag contains error-status codes */
          fcu_orig->curval = fcu->curval;
          driver_orig->curval = fcu->driver->curval;
          driver_orig->flag = fcu->driver->flag;

          DriverVar *dvar_orig = static_cast<DriverVar *>(driver_orig->variables.first);
          DriverVar *dvar = static_cast<DriverVar *>(fcu->driver->variables.first);
          for (; dvar_orig && dvar; dvar_orig = dvar_orig->next, dvar = dvar->next) {
            DriverTarget *dtar_orig = &dvar_orig->targets[0];
            DriverTarget *dtar = &dvar->targets[0];
            for (int i = 0; i < MAX_DRIVER_TARGETS; i++, dtar_orig++, dtar++) {
              dtar_orig->flag = dtar->flag;
            }

            dvar_orig->curval = dvar->curval;
            dvar_orig->flag = dvar->flag;
          }
        }
      }

      /* set error-flag if evaluation failed */
      if (ok == 0) {
        CLOG_WARN(&LOG, "invalid driver - %s[%d]", fcu->rna_path, fcu->array_index);
        driver_orig->flag |= DRIVER_FLAG_INVALID;
      }
    }
  }
}
