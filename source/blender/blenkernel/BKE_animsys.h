/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Joshua Leung (original author)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_ANIMSYS_H__
#define __BKE_ANIMSYS_H__

/** \file BKE_animsys.h
 *  \ingroup bke
 *  \author Joshua Leung
 */

struct ID;
struct ListBase;
struct Main;
struct AnimData;
struct KeyingSet;
struct KS_Path;
struct PathResolvedRNA;
struct bContext;

struct PointerRNA;
struct PropertyRNA;
struct ReportList;
struct bAction;
struct bActionGroup;
struct AnimMapper;

/* ************************************* */
/* AnimData API */

/* Check if the given ID-block can have AnimData */
bool id_type_can_have_animdata(const short id_type);
bool id_can_have_animdata(const struct ID *id);

/* Get AnimData from the given ID-block */
struct AnimData *BKE_animdata_from_id(struct ID *id);

/* Add AnimData to the given ID-block */
struct AnimData *BKE_animdata_add_id(struct ID *id);

/* Set active action used by AnimData from the given ID-block */
bool BKE_animdata_set_action(struct ReportList *reports, struct ID *id, struct bAction *act);

/* Free AnimData */
void BKE_animdata_free(struct ID *id, const bool do_id_user);

/* Copy AnimData */
struct AnimData *BKE_animdata_copy(struct AnimData *adt, const bool do_action);

/* Copy AnimData */
bool BKE_animdata_copy_id(struct ID *id_to, struct ID *id_from, const bool do_action);

/* Copy AnimData Actions */
void BKE_animdata_copy_id_action(struct ID *id, const bool set_newid);

/* Merge copies of data from source AnimData block */
typedef enum eAnimData_MergeCopy_Modes {
	/* Keep destination action */
	ADT_MERGECOPY_KEEP_DST = 0,
	
	/* Use src action (make a new copy) */
	ADT_MERGECOPY_SRC_COPY = 1,
	
	/* Use src action (but just reference the existing version) */
	ADT_MERGECOPY_SRC_REF  = 2
} eAnimData_MergeCopy_Modes;

void BKE_animdata_merge_copy(struct ID *dst_id, struct ID *src_id, eAnimData_MergeCopy_Modes action_mode, bool fix_drivers);

/* ************************************* */
/* KeyingSets API */

/* Used to create a new 'custom' KeyingSet for the user, that will be automatically added to the stack */
struct KeyingSet *BKE_keyingset_add(struct ListBase *list, const char idname[], const char name[], short flag, short keyingflag);

/* Add a path to a KeyingSet */
struct KS_Path *BKE_keyingset_add_path(struct KeyingSet *ks, struct ID *id, const char group_name[], const char rna_path[], int array_index, short flag, short groupmode);

/* Find the destination matching the criteria given */
struct KS_Path *BKE_keyingset_find_path(struct KeyingSet *ks, struct ID *id, const char group_name[], const char rna_path[], int array_index, int group_mode);

/* Copy all KeyingSets in the given list */
void BKE_keyingsets_copy(struct ListBase *newlist, struct ListBase *list);

/* Free the given Keying Set path */
void BKE_keyingset_free_path(struct KeyingSet *ks, struct KS_Path *ksp);

/* Free data for KeyingSet but not set itself */
void BKE_keyingset_free(struct KeyingSet *ks);

/* Free all the KeyingSets in the given list */
void BKE_keyingsets_free(struct ListBase *list);

/* ************************************* */
/* Path Fixing API */

/* Get a "fixed" version of the given path (oldPath) */
char *BKE_animsys_fix_rna_path_rename(ID *owner_id, char *old_path, const char *prefix, const char *oldName,
                                      const char *newName, int oldSubscript, int newSubscript, bool verify_paths);

/* Fix all the paths for the given ID + Action */
void BKE_action_fix_paths_rename(struct ID *owner_id, struct bAction *act, const char *prefix, const char *oldName,
                                 const char *newName, int oldSubscript, int newSubscript, bool verify_paths);

/* Fix all the paths for the given ID+AnimData */
void BKE_animdata_fix_paths_rename(struct ID *owner_id, struct AnimData *adt, struct ID *ref_id, const char *prefix,
                                   const char *oldName, const char *newName, int oldSubscript, int newSubscript,
                                   bool verify_paths);

/* Fix all the paths for the entire database... */
void BKE_animdata_fix_paths_rename_all(ID *ref_id, const char *prefix, const char *oldName, const char *newName);

/* Fix the path after removing elements that are not ID (e.g., node) */
void BKE_animdata_fix_paths_remove(struct ID *id, const char *path);

/* -------------------------------------- */

/* Move animation data from src to destination if it's paths are based on basepaths */
void BKE_animdata_separate_by_basepath(struct ID *srcID, struct ID *dstID, struct ListBase *basepaths);

/* Move F-Curves from src to destination if it's path is based on basepath */
void action_move_fcurves_by_basepath(struct bAction *srcAct, struct bAction *dstAct, const char basepath[]);

char *BKE_animdata_driver_path_hack(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop,
                                    char *base_path);

/* ************************************* */
/* Batch AnimData API */

/* Define for callback looper used in BKE_animdata_main_cb */
typedef void (*ID_AnimData_Edit_Callback)(struct ID *id, struct AnimData *adt, void *user_data);


/* Loop over all datablocks applying callback */
void BKE_animdata_main_cb(struct Main *main, ID_AnimData_Edit_Callback func, void *user_data);

/* ************************************* */
// TODO: overrides, remapping, and path-finding api's

/* ************************************* */
/* Evaluation API */

/* ------------- Main API -------------------- */
/* In general, these ones should be called to do all animation evaluation */

/* Evaluation loop for evaluating animation data  */
void BKE_animsys_evaluate_animdata(struct Scene *scene, struct ID *id, struct AnimData *adt, float ctime, short recalc);

/* Evaluation of all ID-blocks with Animation Data blocks - Animation Data Only */
void BKE_animsys_evaluate_all_animation(struct Main *main, struct Scene *scene, float ctime);

/* TODO(sergey): This is mainly a temp public function. */
struct FCurve;
bool BKE_animsys_execute_fcurve(struct PointerRNA *ptr, struct AnimMapper *remap, struct FCurve *fcu, float curval);

/* ------------ Specialized API --------------- */
/* There are a few special tools which require these following functions. They are NOT to be used
 * for standard animation evaluation UNDER ANY CIRCUMSTANCES! 
 *
 * i.e. Pose Library (PoseLib) uses some of these for selectively applying poses, but 
 *	    Particles/Sequencer performing funky time manipulation is not ok.
 */

/* Evaluate Action (F-Curve Bag) */
void animsys_evaluate_action(struct PointerRNA *ptr, struct bAction *act, struct AnimMapper *remap, float ctime);

/* Evaluate Action Group */
void animsys_evaluate_action_group(struct PointerRNA *ptr, struct bAction *act, struct bActionGroup *agrp, struct AnimMapper *remap, float ctime);

/* ************************************* */

/* ------------ Evaluation API --------------- */

struct EvaluationContext;

void BKE_animsys_eval_animdata(struct EvaluationContext *eval_ctx, struct ID *id);
void BKE_animsys_eval_driver(struct EvaluationContext *eval_ctx, struct ID *id, struct FCurve *fcurve);

/* ************************************* */

#endif /* __BKE_ANIMSYS_H__*/
