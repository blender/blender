/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_bit_vector.hh"
#include "BLI_span.hh"
#include "BLI_sys_types.h" /* for bool */

struct AnimData;
struct BlendDataReader;
struct BlendWriter;
struct Depsgraph;
struct FCurve;
struct ID;
struct KS_Path;
struct KeyingSet;
struct LibraryForeachIDData;
struct ListBase;
struct Main;
struct NlaKeyframingContext;
struct PathResolvedRNA;
struct PointerRNA;
struct PropertyRNA;
struct bAction;
struct bActionGroup;

/** Container for data required to do FCurve and Driver evaluation. */
typedef struct AnimationEvalContext {
  /* For drivers, so that they have access to the dependency graph and the current view layer. See
   * #77086. */
  struct Depsgraph *depsgraph;

  /* FCurves and Drivers can be evaluated at a different time than the current scene time, for
   * example when evaluating NLA strips. This means that, even though the current time is stored in
   * the dependency graph, we need an explicit evaluation time. */
  float eval_time;
} AnimationEvalContext;

AnimationEvalContext BKE_animsys_eval_context_construct(struct Depsgraph *depsgraph,
                                                        float eval_time) ATTR_WARN_UNUSED_RESULT;
AnimationEvalContext BKE_animsys_eval_context_construct_at(
    const AnimationEvalContext *anim_eval_context, float eval_time) ATTR_WARN_UNUSED_RESULT;

/* ************************************* */
/* KeyingSets API */

/**
 * Used to create a new 'custom' KeyingSet for the user,
 * that will be automatically added to the stack.
 */
struct KeyingSet *BKE_keyingset_add(
    struct ListBase *list, const char idname[], const char name[], short flag, short keyingflag);

/**
 * Add a path to a KeyingSet. Nothing is returned for now.
 * Checks are performed to ensure that destination is appropriate for the KeyingSet in question
 */
struct KS_Path *BKE_keyingset_add_path(struct KeyingSet *ks,
                                       struct ID *id,
                                       const char group_name[],
                                       const char rna_path[],
                                       int array_index,
                                       short flag,
                                       short groupmode);

/**
 * Find the destination matching the criteria given.
 * TODO: do we want some method to perform partial matches too?
 */
struct KS_Path *BKE_keyingset_find_path(struct KeyingSet *ks,
                                        struct ID *id,
                                        const char group_name[],
                                        const char rna_path[],
                                        int array_index,
                                        int group_mode);

/** Copy all KeyingSets in the given list. */
void BKE_keyingsets_copy(struct ListBase *newlist, const struct ListBase *list);

/**
 * Process the ID pointers inside a scene's keying-sets, in.
 * see `BKE_lib_query.hh` for details.
 */
void BKE_keyingsets_foreach_id(struct LibraryForeachIDData *data,
                               const struct ListBase *keyingsets);

/** Free the given Keying Set path. */
void BKE_keyingset_free_path(struct KeyingSet *ks, struct KS_Path *ksp);

/** Free data for KeyingSet but not set itself. */
void BKE_keyingset_free_paths(struct KeyingSet *ks);

/** Free all the KeyingSets in the given list. */
void BKE_keyingsets_free(struct ListBase *list);

void BKE_keyingsets_blend_write(struct BlendWriter *writer, struct ListBase *list);
void BKE_keyingsets_blend_read_data(struct BlendDataReader *reader, struct ListBase *list);

/* ************************************* */
/* Path Fixing API */

/**
 * Get a "fixed" version of the given path `old_path`.
 *
 * This is just an external wrapper for the RNA-Path fixing function,
 * with input validity checks on top of the basic method.
 *
 * \note it is assumed that the structure we're replacing is `<prefix><["><name><"]>`
 * i.e. `pose.bones["Bone"]`.
 */
char *BKE_animsys_fix_rna_path_rename(struct ID *owner_id,
                                      char *old_path,
                                      const char *prefix,
                                      const char *oldName,
                                      const char *newName,
                                      int oldSubscript,
                                      int newSubscript,
                                      bool verify_paths);

/**
 * Fix all the paths for the given ID + Action.
 *
 * This is just an external wrapper for the F-Curve fixing function,
 * with input validity checks on top of the basic method.
 *
 * \note it is assumed that the structure we're replacing is `<prefix><["><name><"]>`
 * i.e. `pose.bones["Bone"]`.
 */
void BKE_action_fix_paths_rename(struct ID *owner_id,
                                 struct bAction *act,
                                 int32_t /*slot_handle_t*/ slot_handle,
                                 const char *prefix,
                                 const char *oldName,
                                 const char *newName,
                                 int oldSubscript,
                                 int newSubscript,
                                 bool verify_paths);

/**
 * Fix all the paths for the given ID+AnimData
 *
 * \note it is assumed that the structure we're replacing is `<prefix><["><name><"]>`
 * i.e. `pose.bones["Bone"]`.
 */
void BKE_animdata_fix_paths_rename(struct ID *owner_id,
                                   struct AnimData *adt,
                                   struct ID *ref_id,
                                   const char *prefix,
                                   const char *oldName,
                                   const char *newName,
                                   int oldSubscript,
                                   int newSubscript,
                                   bool verify_paths);

/**
 * Fix all RNA-Paths throughout the database (directly access the #Global.main version).
 *
 * \note it is assumed that the structure we're replacing is `<prefix><["><name><"]>`
 * i.e. `pose.bones["Bone"]`
 */
void BKE_animdata_fix_paths_rename_all_ex(struct Main *bmain,
                                          struct ID *ref_id,
                                          const char *prefix,
                                          const char *oldName,
                                          const char *newName,
                                          int oldSubscript,
                                          int newSubscript,
                                          bool verify_paths);

/** See #BKE_animdata_fix_paths_rename_all_ex */
void BKE_animdata_fix_paths_rename_all(struct ID *ref_id,
                                       const char *prefix,
                                       const char *oldName,
                                       const char *newName);

/**
 * Remove any animation data (F-Curves from Actions, and drivers) that have an
 * RNA path starting with `prefix`.
 *
 * Return true if any animation data was affected.
 */
bool BKE_animdata_fix_paths_remove(struct ID *id, const char *prefix);

/**
 * Remove drivers that have an RNA path starting with `prefix`.
 *
 * \return true if any driver was removed.
 */
bool BKE_animdata_driver_path_remove(struct ID *id, const char *prefix);

/**
 * Remove all drivers from the given struct.
 *
 * \param type: needs to be a struct owned by the given ID.
 * \param data: the actual struct data, needs to be the data for the StructRNA.
 *
 * \return true if any driver was removed.
 */
bool BKE_animdata_drivers_remove_for_rna_struct(struct ID &owner_id,
                                                struct StructRNA &type,
                                                void *data);

/* -------------------------------------- */

typedef struct AnimationBasePathChange {
  struct AnimationBasePathChange *next, *prev;
  const char *src_basepath;
  const char *dst_basepath;
} AnimationBasePathChange;

/**
 * Move animation data from source to destination if its paths are based on `basepaths`.
 *
 * Transfer the animation data from `srcID` to `dstID` where the `srcID` animation data
 * is based off `basepath`, creating new #AnimData and associated data as necessary.
 *
 * \param basepaths: A list of #AnimationBasePathChange.
 */
void BKE_animdata_transfer_by_basepath(struct Main *bmain,
                                       struct ID *srcID,
                                       struct ID *dstID,
                                       struct ListBase *basepaths);

/* ------------ NLA Keyframing --------------- */

typedef struct NlaKeyframingContext NlaKeyframingContext;

/**
 * Prepare data necessary to compute correct keyframe values for NLA strips
 * with non-Replace mode or influence different from 1.
 *
 * \param cache: List used to cache contexts for reuse when keying
 * multiple channels in one operation.
 * \param ptr: RNA pointer to the ID with the animation.
 * \return Keyframing context, or NULL if not necessary.
 */
struct NlaKeyframingContext *BKE_animsys_get_nla_keyframing_context(
    struct ListBase *cache,
    struct PointerRNA *ptr,
    struct AnimData *adt,
    const struct AnimationEvalContext *anim_eval_context);
/**
 * Apply correction from the NLA context to the values about to be keyframed.
 *
 * \param context: Context to use (may be NULL).
 * \param prop_ptr: Property about to be keyframed.
 * \param[in,out] values: Span of property values to adjust.
 * \param index: Index of the element about to be updated, or -1.
 * \param[out] r_force_all: For array properties, set to true if the property
 * should be treated as all-or-nothing (i.e. where either all elements get keyed
 * or none do). Irrelevant for non-array properties. May be NULL.
 * \param[out] r_values_mask: A mask for the elements of `values`, where bits
 * are set to true for the elements that were both indicated by `index` and for
 * which valid keying values were successfully computed.  In short, this is a
 * mask for the indices that can get keyed.
 */
void BKE_animsys_nla_remap_keyframe_values(struct NlaKeyframingContext *context,
                                           struct PointerRNA *prop_ptr,
                                           struct PropertyRNA *prop,
                                           const blender::MutableSpan<float> values,
                                           int index,
                                           const struct AnimationEvalContext *anim_eval_context,
                                           bool *r_force_all,
                                           blender::BitVector<> &r_values_mask);

/**
 * Free all cached contexts from the list.
 */
void BKE_animsys_free_nla_keyframing_context_cache(struct ListBase *cache);

/* ************************************* */
/* Evaluation API */

/* ------------- Main API -------------------- */
/* In general, these ones should be called to do all animation evaluation */

/* Flags for recalc parameter, indicating which part to recalculate. */
typedef enum eAnimData_Recalc {
  ADT_RECALC_DRIVERS = (1 << 0),
  ADT_RECALC_ANIM = (1 << 1),
  ADT_RECALC_ALL = (ADT_RECALC_DRIVERS | ADT_RECALC_ANIM),
} eAnimData_Recalc;

bool BKE_animsys_rna_path_resolve(struct PointerRNA *ptr,
                                  const char *rna_path,
                                  int array_index,
                                  struct PathResolvedRNA *r_result);
bool BKE_animsys_read_from_rna_path(struct PathResolvedRNA *anim_rna, float *r_value);
/**
 * Write the given value to a setting using RNA, and return success.
 *
 * \param force_write: When false, this function will only call the RNA setter when `value` is
 * different from the property's current value. When true, this function will skip that check and
 * always call the RNA setter.
 */
bool BKE_animsys_write_to_rna_path(struct PathResolvedRNA *anim_rna,
                                   float value,
                                   bool force_write = false);

/**
 * Evaluation loop for evaluation animation data
 *
 * This assumes that the animation-data provided belongs to the ID block in question,
 * and that the flags for which parts of the animation-data settings need to be recalculated
 * have been set already by the depsgraph. Now, we use the recalculate.
 */
void BKE_animsys_evaluate_animdata(struct ID *id,
                                   struct AnimData *adt,
                                   const struct AnimationEvalContext *anim_eval_context,
                                   eAnimData_Recalc recalc,
                                   bool flush_to_original);

/**
 * Evaluation of all ID-blocks with Animation Data blocks - Animation Data Only
 *
 * This will evaluate only the animation info available in the animation data-blocks
 * encountered. In order to enforce the system by which some settings controlled by a
 * 'local' (i.e. belonging in the nearest ID-block that setting is related to, not a
 * standard 'root') block are overridden by a larger 'user'
 */
void BKE_animsys_evaluate_all_animation(struct Main *main,
                                        struct Depsgraph *depsgraph,
                                        float ctime);

/* ------------ Specialized API --------------- */
/* There are a few special tools which require these following functions. They are NOT to be used
 * for standard animation evaluation UNDER ANY CIRCUMSTANCES!
 *
 * i.e. Pose Library (PoseLib) uses some of these for selectively applying poses, but
 *      Particles/Sequencer performing funky time manipulation is not ok.
 */

/**
 * Evaluate Action (F-Curve Bag).
 */
void animsys_evaluate_action(struct PointerRNA *ptr,
                             struct bAction *act,
                             int32_t action_slot_handle,
                             const struct AnimationEvalContext *anim_eval_context,
                             bool flush_to_original);

/**
 * Evaluate action, and blend the result into the current values (instead of overwriting fully).
 */
void animsys_blend_in_action(struct PointerRNA *ptr,
                             struct bAction *act,
                             int32_t action_slot_handle,
                             const AnimationEvalContext *anim_eval_context,
                             float blend_factor);

/** Evaluate Action Group. */
void animsys_evaluate_action_group(struct PointerRNA *ptr,
                                   struct bAction *act,
                                   struct bActionGroup *agrp,
                                   const struct AnimationEvalContext *anim_eval_context);

/* ************************************* */

/* ------------ Evaluation API --------------- */

struct Depsgraph;

void BKE_animsys_eval_animdata(struct Depsgraph *depsgraph, struct ID *id);
void BKE_animsys_eval_driver_unshare(Depsgraph *depsgraph, ID *id);
void BKE_animsys_eval_driver(struct Depsgraph *depsgraph,
                             struct ID *id,
                             int driver_index,
                             struct FCurve *fcu_orig);

void BKE_animsys_update_driver_array(struct ID *id);

/* ************************************* */

void BKE_time_markers_blend_write(BlendWriter *writer, ListBase /* TimeMarker */ &markers);
void BKE_time_markers_blend_read(BlendDataReader *reader, ListBase /* TimeMarker */ &markers);

/**
 * Copy a list of time markers.
 *
 * Note: this is meant to be called in the context of duplicating an ID.
 *
 * \param flag: ID copy flags. Corresponds to the `flag` parameter of `BKE_id_copy_ex()`.
 */
void BKE_copy_time_markers(ListBase /* TimeMarker */ &markers_dst,
                           const ListBase /* TimeMarker */ &markers_src,
                           int flag);
