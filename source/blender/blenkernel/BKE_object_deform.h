/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief Functions for dealing with objects and deform verts,
 *        used by painting and tools.
 */

#include "DNA_scene_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct MDeformVert;
struct Object;
struct bDeformGroup;

/* General vgroup operations. */

/**
 * Update users of vgroups from this object, according to given map.
 *
 * Use it when you remove or reorder vgroups in the object.
 *
 * \param map: an array mapping old indices to new indices.
 */
void BKE_object_defgroup_remap_update_users(struct Object *ob, const int *map);

/**
 * Get #MDeformVert vgroup data from given object. Should only be used in Object mode.
 *
 * \return True if the id type supports weights.
 */
bool BKE_object_defgroup_array_get(struct ID *id, struct MDeformVert **dvert_arr, int *dvert_tot);

/**
 * Add a vgroup of default name to object. *Does not* handle #MDeformVert data at all!
 */
struct bDeformGroup *BKE_object_defgroup_add(struct Object *ob);
/**
 * Add a vgroup of given name to object. *Does not* handle #MDeformVert data at all!
 */
struct bDeformGroup *BKE_object_defgroup_add_name(struct Object *ob, const char *name);
/**
 * Create #MDeformVert data for given ID. Work in Object mode only.
 */
struct MDeformVert *BKE_object_defgroup_data_create(struct ID *id);

/**
 * Remove all verts (or only selected ones) from given vgroup. Work in Object and Edit modes.
 *
 * \param use_selection: Only operate on selection.
 * \return True if any vertex was removed, false otherwise.
 */
bool BKE_object_defgroup_clear(struct Object *ob, struct bDeformGroup *dg, bool use_selection);
/**
 * Remove all verts (or only selected ones) from all vgroups. Work in Object and Edit modes.
 *
 * \param use_selection: Only operate on selection.
 * \return True if any vertex was removed, false otherwise.
 */
bool BKE_object_defgroup_clear_all(struct Object *ob, bool use_selection);

/**
 * Remove given vgroup from object. Work in Object and Edit modes.
 */
void BKE_object_defgroup_remove(struct Object *ob, struct bDeformGroup *defgroup);
/**
 * Remove all vgroups from object. Work in Object and Edit modes.
 * When only_unlocked=true, locked vertex groups are not removed.
 */
void BKE_object_defgroup_remove_all_ex(struct Object *ob, bool only_unlocked);
/**
 * Remove all vgroups from object. Work in Object and Edit modes.
 */
void BKE_object_defgroup_remove_all(struct Object *ob);

/**
 * Compute mapping for vertex groups with matching name, -1 is used for no remapping.
 * Returns null if no remapping is required.
 * The returned array has to be freed.
 */
int *BKE_object_defgroup_index_map_create(struct Object *ob_src,
                                          struct Object *ob_dst,
                                          int *r_map_len);
void BKE_object_defgroup_index_map_apply(struct MDeformVert *dvert,
                                         int dvert_len,
                                         const int *map,
                                         int map_len);

/* Select helpers. */

/**
 * Return the subset type of the Vertex Group Selection.
 */
bool *BKE_object_defgroup_subset_from_select_type(struct Object *ob,
                                                  enum eVGroupSelect subset_type,
                                                  int *r_defgroup_tot,
                                                  int *r_subset_count);
/**
 * Store indices from the defgroup_validmap (faster lookups in some cases).
 */
void BKE_object_defgroup_subset_to_index_array(const bool *defgroup_validmap,
                                               int defgroup_tot,
                                               int *r_defgroup_subset_map);

/* ********** */

/**
 * Gets the status of "flag" for each #bDeformGroup
 * in the object data's vertex group list and returns an array containing them
 */
bool *BKE_object_defgroup_lock_flags_get(struct Object *ob, int defbase_tot);
bool *BKE_object_defgroup_validmap_get(struct Object *ob, int defbase_tot);
/**
 * Returns total selected vgroups,
 * `wpi.defbase_sel` is assumed malloc'd, all values are set.
 */
bool *BKE_object_defgroup_selected_get(struct Object *ob,
                                       int defbase_tot,
                                       int *r_dg_flags_sel_tot);

/**
 * Checks if the lock relative mode is applicable.
 *
 * \return true if an unlocked deform group is active.
 */
bool BKE_object_defgroup_check_lock_relative(const bool *lock_flags,
                                             const bool *validmap,
                                             int index);
/**
 * Additional check for whether the lock relative mode is applicable in multi-paint mode.
 *
 * \return true if none of the selected groups are locked.
 */
bool BKE_object_defgroup_check_lock_relative_multi(int defbase_tot,
                                                   const bool *lock_flags,
                                                   const bool *selected,
                                                   int sel_tot);

/**
 * Return lock status of active vertex group.
 */
bool BKE_object_defgroup_active_is_locked(const struct Object *ob);

/**
 * Takes a pair of boolean masks of all locked and all deform groups, and computes
 * a pair of masks for locked deform and unlocked deform groups. Output buffers may
 * reuse the input ones.
 */
void BKE_object_defgroup_split_locked_validmap(
    int defbase_tot, const bool *locked, const bool *deform, bool *r_locked, bool *r_unlocked);

/**
 * Marks mirror vgroups in output and counts them.
 * Output and counter assumed to be already initialized.
 * Designed to be usable after BKE_object_defgroup_selected_get to extend selection to mirror.
 */
void BKE_object_defgroup_mirror_selection(struct Object *ob,
                                          int defbase_tot,
                                          const bool *selection,
                                          bool *dg_flags_sel,
                                          int *r_dg_flags_sel_tot);

#ifdef __cplusplus
}
#endif
