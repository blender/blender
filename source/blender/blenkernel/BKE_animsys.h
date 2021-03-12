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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_sys_types.h" /* for bool */

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct BlendDataReader;
struct BlendExpander;
struct BlendLibReader;
struct BlendWriter;
struct Depsgraph;
struct FCurve;
struct ID;
struct KS_Path;
struct KeyingSet;
struct ListBase;
struct Main;
struct NlaKeyframingContext;
struct PathResolvedRNA;
struct PointerRNA;
struct PropertyRNA;
struct bAction;
struct bActionGroup;
struct bContext;

/* Container for data required to do FCurve and Driver evaluation. */
typedef struct AnimationEvalContext {
  /* For drivers, so that they have access to the dependency graph and the current view layer. See
   * T77086. */
  struct Depsgraph *depsgraph;

  /* FCurves and Drivers can be evaluated at a different time than the current scene time, for
   * example when evaluating NLA strips. This means that, even though the current time is stored in
   * the dependency graph, we need an explicit evaluation time. */
  float eval_time;
} AnimationEvalContext;

AnimationEvalContext BKE_animsys_eval_context_construct(struct Depsgraph *depsgraph,
                                                        float eval_time);
AnimationEvalContext BKE_animsys_eval_context_construct_at(
    const AnimationEvalContext *anim_eval_context, float eval_time);

/* ************************************* */
/* KeyingSets API */

/* Used to create a new 'custom' KeyingSet for the user,
 * that will be automatically added to the stack */
struct KeyingSet *BKE_keyingset_add(
    struct ListBase *list, const char idname[], const char name[], short flag, short keyingflag);

/* Add a path to a KeyingSet */
struct KS_Path *BKE_keyingset_add_path(struct KeyingSet *ks,
                                       struct ID *id,
                                       const char group_name[],
                                       const char rna_path[],
                                       int array_index,
                                       short flag,
                                       short groupmode);

/* Find the destination matching the criteria given */
struct KS_Path *BKE_keyingset_find_path(struct KeyingSet *ks,
                                        struct ID *id,
                                        const char group_name[],
                                        const char rna_path[],
                                        int array_index,
                                        int group_mode);

/* Copy all KeyingSets in the given list */
void BKE_keyingsets_copy(struct ListBase *newlist, const struct ListBase *list);

/* Free the given Keying Set path */
void BKE_keyingset_free_path(struct KeyingSet *ks, struct KS_Path *ksp);

/* Free data for KeyingSet but not set itself */
void BKE_keyingset_free(struct KeyingSet *ks);

/* Free all the KeyingSets in the given list */
void BKE_keyingsets_free(struct ListBase *list);

void BKE_keyingsets_blend_write(struct BlendWriter *writer, struct ListBase *list);
void BKE_keyingsets_blend_read_data(struct BlendDataReader *reader, struct ListBase *list);
void BKE_keyingsets_blend_read_lib(struct BlendLibReader *reader,
                                   struct ID *id,
                                   struct ListBase *list);
void BKE_keyingsets_blend_read_expand(struct BlendExpander *expander, struct ListBase *list);

/* ************************************* */
/* Path Fixing API */

/* Get a "fixed" version of the given path (oldPath) */
char *BKE_animsys_fix_rna_path_rename(struct ID *owner_id,
                                      char *old_path,
                                      const char *prefix,
                                      const char *oldName,
                                      const char *newName,
                                      int oldSubscript,
                                      int newSubscript,
                                      bool verify_paths);

/* Fix all the paths for the given ID + Action */
void BKE_action_fix_paths_rename(struct ID *owner_id,
                                 struct bAction *act,
                                 const char *prefix,
                                 const char *oldName,
                                 const char *newName,
                                 int oldSubscript,
                                 int newSubscript,
                                 bool verify_paths);

/* Fix all the paths for the given ID+AnimData */
void BKE_animdata_fix_paths_rename(struct ID *owner_id,
                                   struct AnimData *adt,
                                   struct ID *ref_id,
                                   const char *prefix,
                                   const char *oldName,
                                   const char *newName,
                                   int oldSubscript,
                                   int newSubscript,
                                   bool verify_paths);

/* Fix all the paths for the entire database... */
void BKE_animdata_fix_paths_rename_all(struct ID *ref_id,
                                       const char *prefix,
                                       const char *oldName,
                                       const char *newName);

/* Fix all the paths for the entire bmain with extra parameters. */
void BKE_animdata_fix_paths_rename_all_ex(struct Main *bmain,
                                          struct ID *ref_id,
                                          const char *prefix,
                                          const char *oldName,
                                          const char *newName,
                                          const int oldSubscript,
                                          const int newSubscript,
                                          const bool verify_paths);

/* Fix the path after removing elements that are not ID (e.g., node).
 * Return true if any animation data was affected. */
bool BKE_animdata_fix_paths_remove(struct ID *id, const char *path);

/* -------------------------------------- */

typedef struct AnimationBasePathChange {
  struct AnimationBasePathChange *next, *prev;
  const char *src_basepath;
  const char *dst_basepath;
} AnimationBasePathChange;

/* Move animation data from src to destination if its paths are based on basepaths */
void BKE_animdata_transfer_by_basepath(struct Main *bmain,
                                       struct ID *srcID,
                                       struct ID *dstID,
                                       struct ListBase *basepaths);

char *BKE_animdata_driver_path_hack(struct bContext *C,
                                    struct PointerRNA *ptr,
                                    struct PropertyRNA *prop,
                                    char *base_path);

/* ************************************* */
/* Batch AnimData API */

/* Define for callback looper used in BKE_animdata_main_cb */
typedef void (*ID_AnimData_Edit_Callback)(struct ID *id, struct AnimData *adt, void *user_data);

/* Define for callback looper used in BKE_fcurves_main_cb */
typedef void (*ID_FCurve_Edit_Callback)(struct ID *id, struct FCurve *fcu, void *user_data);

/* Loop over all datablocks applying callback */
void BKE_animdata_main_cb(struct Main *bmain, ID_AnimData_Edit_Callback func, void *user_data);

/* Loop over all datablocks applying callback to all its F-Curves */
void BKE_fcurves_main_cb(struct Main *bmain, ID_FCurve_Edit_Callback func, void *user_data);

/* Look over all f-curves of a given ID. */
void BKE_fcurves_id_cb(struct ID *id, ID_FCurve_Edit_Callback func, void *user_data);

/* ************************************* */
// TODO: overrides, remapping, and path-finding api's

/* ------------ NLA Keyframing --------------- */

typedef struct NlaKeyframingContext NlaKeyframingContext;

struct NlaKeyframingContext *BKE_animsys_get_nla_keyframing_context(
    struct ListBase *cache,
    struct PointerRNA *ptr,
    struct AnimData *adt,
    const struct AnimationEvalContext *anim_eval_context);
bool BKE_animsys_nla_remap_keyframe_values(struct NlaKeyframingContext *context,
                                           struct PointerRNA *prop_ptr,
                                           struct PropertyRNA *prop,
                                           float *values,
                                           int count,
                                           int index,
                                           bool *r_force_all);
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
                                  const int array_index,
                                  struct PathResolvedRNA *r_result);
bool BKE_animsys_read_from_rna_path(struct PathResolvedRNA *anim_rna, float *r_value);
bool BKE_animsys_write_to_rna_path(struct PathResolvedRNA *anim_rna, const float value);

/* Evaluation loop for evaluating animation data  */
void BKE_animsys_evaluate_animdata(struct ID *id,
                                   struct AnimData *adt,
                                   const struct AnimationEvalContext *anim_eval_context,
                                   eAnimData_Recalc recalc,
                                   const bool flush_to_original);

/* Evaluation of all ID-blocks with Animation Data blocks - Animation Data Only */
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

/* Evaluate Action (F-Curve Bag) */
void animsys_evaluate_action(struct PointerRNA *ptr,
                             struct bAction *act,
                             const struct AnimationEvalContext *anim_eval_context,
                             bool flush_to_original);

/* Evaluate Action Group */
void animsys_evaluate_action_group(struct PointerRNA *ptr,
                                   struct bAction *act,
                                   struct bActionGroup *agrp,
                                   const struct AnimationEvalContext *anim_eval_context);

/* ************************************* */

/* ------------ Evaluation API --------------- */

struct Depsgraph;

void BKE_animsys_eval_animdata(struct Depsgraph *depsgraph, struct ID *id);
void BKE_animsys_eval_driver(struct Depsgraph *depsgraph,
                             struct ID *id,
                             int driver_index,
                             struct FCurve *fcu_orig);

void BKE_animsys_update_driver_array(struct ID *id);

/* ************************************* */

#ifdef __cplusplus
}
#endif
