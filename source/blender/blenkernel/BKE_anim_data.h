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
struct ID;
struct LibraryForeachIDData;
struct Main;
struct ReportList;
struct bAction;

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

/* Return true if the ID-block has non-empty AnimData. */
bool BKE_animdata_id_is_animated(const struct ID *id);

void BKE_animdata_foreach_id(struct AnimData *adt, struct LibraryForeachIDData *data);

/* Copy AnimData */
struct AnimData *BKE_animdata_copy(struct Main *bmain, struct AnimData *adt, const int flag);

/* Copy AnimData */
bool BKE_animdata_copy_id(struct Main *bmain,
                          struct ID *id_to,
                          struct ID *id_from,
                          const int flag);

/* Copy AnimData Actions */
void BKE_animdata_copy_id_action(struct Main *bmain, struct ID *id);

void BKE_animdata_duplicate_id_action(struct Main *bmain,
                                      struct ID *id,
                                      const uint duplicate_flags);

/* Merge copies of data from source AnimData block */
typedef enum eAnimData_MergeCopy_Modes {
  /* Keep destination action */
  ADT_MERGECOPY_KEEP_DST = 0,

  /* Use src action (make a new copy) */
  ADT_MERGECOPY_SRC_COPY = 1,

  /* Use src action (but just reference the existing version) */
  ADT_MERGECOPY_SRC_REF = 2,
} eAnimData_MergeCopy_Modes;

void BKE_animdata_merge_copy(struct Main *bmain,
                             struct ID *dst_id,
                             struct ID *src_id,
                             eAnimData_MergeCopy_Modes action_mode,
                             bool fix_drivers);

#ifdef __cplusplus
}
#endif
