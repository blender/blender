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
 * The Original Code is Copyright (C) 2015, Blender Foundation
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_OUTLINER_H__
#define __ED_OUTLINER_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;
struct bContext;

bool ED_outliner_collections_editor_poll(struct bContext *C);

void ED_outliner_selected_objects_get(const struct bContext *C, struct ListBase *objects);

Base *ED_outliner_give_base_under_cursor(struct bContext *C, const int mval[2]);

void ED_outliner_select_sync_from_object_tag(struct bContext *C);
void ED_outliner_select_sync_from_edit_bone_tag(struct bContext *C);
void ED_outliner_select_sync_from_pose_bone_tag(struct bContext *C);
void ED_outliner_select_sync_from_sequence_tag(struct bContext *C);
void ED_outliner_select_sync_from_all_tag(struct bContext *C);

bool ED_outliner_select_sync_is_dirty(const struct bContext *C);

void ED_outliner_select_sync_from_outliner(struct bContext *C, struct SpaceOutliner *soops);

void ED_outliner_select_sync_flag_outliners(const struct bContext *C);

#ifdef __cplusplus
}
#endif

#endif /*  __ED_OUTLINER_H__ */
