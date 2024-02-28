/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_sys_types.h" /* for bool */

struct AnimData;
struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct ID;
struct LibraryForeachIDData;
struct Main;
struct ReportList;
struct bAction;

/* ************************************* */
/* AnimData API */

/**
 * Check if the given ID-block can have AnimData.
 */
bool id_type_can_have_animdata(short id_type);
bool id_can_have_animdata(const ID *id);

/**
 * Get #AnimData from the given ID-block.
 */
AnimData *BKE_animdata_from_id(const ID *id);

/**
 * Ensure #AnimData exists in the given ID-block (when supported).
 */
AnimData *BKE_animdata_ensure_id(ID *id);

/**
 * Set active action used by AnimData from the given ID-block.
 *
 * Called when user tries to change the active action of an #AnimData block
 * (via RNA, Outliner, etc.)
 *
 * \param reports: Can be NULL.
 * \param id: The owner of the animation data
 * \param act: The Action to set, or NULL to clear.
 *
 * \return true when the action was successfully updated, false otherwise.
 */
bool BKE_animdata_set_action(ReportList *reports, ID *id, bAction *act);

/**
 * Same as BKE_animdata_set_action(), except sets `tmpact` instead of `action`.
 */
bool BKE_animdata_set_tmpact(ReportList *reports, ID *id, bAction *act);

bool BKE_animdata_action_editable(const AnimData *adt);

/**
 * Ensure that the action's idroot is set correctly given the ID type of the owner.
 * Return true if it is, false if it was already set to an incompatible type.
 */
bool BKE_animdata_action_ensure_idroot(const ID *owner, bAction *action);

/**
 * Free AnimData used by the nominated ID-block, and clear ID-block's AnimData pointer.
 */
void BKE_animdata_free(ID *id, bool do_id_user);

/**
 * Return true if the ID-block has non-empty AnimData.
 */
bool BKE_animdata_id_is_animated(const ID *id);

/**
 * Callback used by lib_query to walk over all ID usages
 * (mimics `foreach_id` callback of #IDTypeInfo structure).
 */
void BKE_animdata_foreach_id(AnimData *adt, LibraryForeachIDData *data);

/**
 * Make a copy of the given AnimData - to be used when copying data-blocks.
 * \param flag: Control ID pointers management,
 * see LIB_ID_CREATE_.../LIB_ID_COPY_... flags in BKE_lib_id.hh
 * \return The copied animdata.
 */
AnimData *BKE_animdata_copy(Main *bmain, AnimData *adt, int flag);

/**
 * \param flag: Control ID pointers management,
 * see LIB_ID_CREATE_.../LIB_ID_COPY_... flags in BKE_lib_id.hh
 * \return true is successfully copied.
 */
bool BKE_animdata_copy_id(Main *bmain, ID *id_to, ID *id_from, int flag);

/**
 * Copy AnimData Actions.
 */
void BKE_animdata_copy_id_action(Main *bmain, ID *id);

void BKE_animdata_duplicate_id_action(Main *bmain, ID *id, uint duplicate_flags);

/* Merge copies of data from source AnimData block */
typedef enum eAnimData_MergeCopy_Modes {
  /* Keep destination action */
  ADT_MERGECOPY_KEEP_DST = 0,

  /* Use src action (make a new copy) */
  ADT_MERGECOPY_SRC_COPY = 1,

  /* Use src action (but just reference the existing version) */
  ADT_MERGECOPY_SRC_REF = 2,
} eAnimData_MergeCopy_Modes;

/**
 * Merge copies of the data from the src AnimData into the destination AnimData.
 */
void BKE_animdata_merge_copy(
    Main *bmain, ID *dst_id, ID *src_id, eAnimData_MergeCopy_Modes action_mode, bool fix_drivers);

void BKE_animdata_blend_write(BlendWriter *writer, ID *id);
void BKE_animdata_blend_read_data(BlendDataReader *reader, ID *id);
