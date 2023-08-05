/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct Base;
struct ListBase;
struct SpaceOutliner;
struct bContext;

bool ED_outliner_collections_editor_poll(struct bContext *C);

/**
 * Populates the \param objects: ListBase with all the outliner selected objects
 * We store it as (Object *)LinkData->data
 * \param objects: expected to be empty
 */
void ED_outliner_selected_objects_get(const struct bContext *C, struct ListBase *objects);

/**
 * Get base of object under cursor. Used for eyedropper tool.
 */
struct Base *ED_outliner_give_base_under_cursor(struct bContext *C, const int mval[2]);

/**
 * Functions for tagging outliner selection syncing is dirty from operators.
 */
void ED_outliner_select_sync_from_object_tag(struct bContext *C);
void ED_outliner_select_sync_from_edit_bone_tag(struct bContext *C);
void ED_outliner_select_sync_from_pose_bone_tag(struct bContext *C);
void ED_outliner_select_sync_from_sequence_tag(struct bContext *C);
void ED_outliner_select_sync_from_all_tag(struct bContext *C);

bool ED_outliner_select_sync_is_dirty(const struct bContext *C);

/**
 * Set clean outliner and mark other outliners for syncing.
 */
void ED_outliner_select_sync_from_outliner(struct bContext *C,
                                           struct SpaceOutliner *space_outliner);

/**
 * Copy sync select dirty flag from window manager to all outliners to be synced lazily on draw.
 */
void ED_outliner_select_sync_flag_outliners(const struct bContext *C);
