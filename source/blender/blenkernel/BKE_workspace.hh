/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_compiler_attrs.h"

struct Main;
struct bScreen;
struct bToolRef;
struct WorkSpace;
struct WorkSpaceInstanceHook;
struct WorkSpaceLayout;

struct WorkSpaceStatusItem {
  int icon = 0;
  std::string text = {};
  float space_factor = 0.0f;
  bool inverted = false;
};

namespace blender::bke {

struct WorkSpaceRuntime {
  blender::Vector<WorkSpaceStatusItem> status;
};

}  // namespace blender::bke

/* -------------------------------------------------------------------- */
/** \name Create, Delete, Initialize
 * \{ */

WorkSpace *BKE_workspace_add(Main *bmain, const char *name);
/**
 * Remove \a workspace by freeing itself and its data. This is a higher-level wrapper that
 * calls #workspace_free_data (through #BKE_id_free) to free the workspace data, and frees
 * other data-blocks owned by \a workspace and its layouts (currently that is screens only).
 *
 * Always use this to remove (and free) workspaces. Don't free non-ID workspace members here.
 */
void BKE_workspace_remove(Main *bmain, WorkSpace *workspace);

WorkSpaceInstanceHook *BKE_workspace_instance_hook_create(const Main *bmain, int winid);
void BKE_workspace_instance_hook_free(const Main *bmain, WorkSpaceInstanceHook *hook);

/**
 * Add a new layout to \a workspace for \a screen.
 */
WorkSpaceLayout *BKE_workspace_layout_add(Main *bmain,
                                          WorkSpace *workspace,
                                          bScreen *screen,
                                          const char *name) ATTR_NONNULL();
void BKE_workspace_layout_remove(Main *bmain, WorkSpace *workspace, WorkSpaceLayout *layout)
    ATTR_NONNULL();

void BKE_workspace_relations_free(ListBase *relation_list);

/** \} */

/* -------------------------------------------------------------------- */
/** \name General Utilities
 * \{ */

WorkSpaceLayout *BKE_workspace_layout_find(const WorkSpace *workspace, const bScreen *screen)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
/**
 * Find the layout for \a screen without knowing which workspace to look in.
 * Can also be used to find the workspace that contains \a screen.
 *
 * \param r_workspace: Optionally return the workspace that contains the
 * looked up layout (if found).
 */
WorkSpaceLayout *BKE_workspace_layout_find_global(const Main *bmain,
                                                  const bScreen *screen,
                                                  WorkSpace **r_workspace) ATTR_NONNULL(1, 2);

/**
 * Circular workspace layout iterator.
 *
 * \param callback: Custom function which gets executed for each layout.
 * Can return false to stop iterating.
 * \param arg: Custom data passed to each \a callback call.
 *
 * \return the layout at which \a callback returned false.
 */
WorkSpaceLayout *BKE_workspace_layout_iter_circular(const WorkSpace *workspace,
                                                    WorkSpaceLayout *start,
                                                    bool (*callback)(const WorkSpaceLayout *layout,
                                                                     void *arg),
                                                    void *arg,
                                                    bool iter_backward);

void BKE_workspace_tool_remove(WorkSpace *workspace, bToolRef *tref) ATTR_NONNULL(1, 2);

/**
 * Replace tools ID's, intended for use in versioning code.
 * \param space_type: The space-type to match #bToolRef::space_type.
 * \param mode: The space-type to match #bToolRef::mode.
 * \param idname_prefix_skip: Ignore when NULL, otherwise only operate
 * on tools that have this text as the #bToolRef::idname prefix, which is skipped before
 * the replacement runs. This avoids having to duplicate a common prefix in the replacement text.
 * \param replace_table: An array of (source, destination) pairs.
 * \param replace_table_num: The number of items in `replace_table`.
 */
void BKE_workspace_tool_id_replace_table(WorkSpace *workspace,
                                         const int space_type,
                                         const int mode,
                                         const char *idname_prefix_skip,
                                         const char *replace_table[][2],
                                         int replace_table_num) ATTR_NONNULL(1, 5);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Getters/Setters
 * \{ */

#define GETTER_ATTRS ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT
#define SETTER_ATTRS ATTR_NONNULL(1)

WorkSpace *BKE_workspace_active_get(WorkSpaceInstanceHook *hook) GETTER_ATTRS;
void BKE_workspace_active_set(WorkSpaceInstanceHook *hook, WorkSpace *workspace) SETTER_ATTRS;
/**
 * Get the layout that is active for \a hook (which is the visible layout for the active workspace
 * in \a hook).
 */
WorkSpaceLayout *BKE_workspace_active_layout_get(const WorkSpaceInstanceHook *hook) GETTER_ATTRS;
/**
 * \brief Activate a layout
 *
 * Sets \a layout as active for \a workspace when activated through or already active in \a hook.
 * So when the active workspace of \a hook is \a workspace, \a layout becomes the active layout of
 * \a hook too. See #BKE_workspace_active_set().
 *
 * \a workspace does not need to be active for this.
 *
 * #WorkSpaceInstanceHook.act_layout should only be modified directly to update the layout pointer.
 */
void BKE_workspace_active_layout_set(WorkSpaceInstanceHook *hook,
                                     int winid,
                                     WorkSpace *workspace,
                                     WorkSpaceLayout *layout) SETTER_ATTRS;
bScreen *BKE_workspace_active_screen_get(const WorkSpaceInstanceHook *hook) GETTER_ATTRS;
void BKE_workspace_active_screen_set(WorkSpaceInstanceHook *hook,
                                     int winid,
                                     WorkSpace *workspace,
                                     bScreen *screen) SETTER_ATTRS;

const char *BKE_workspace_layout_name_get(const WorkSpaceLayout *layout) GETTER_ATTRS;
void BKE_workspace_layout_name_set(WorkSpace *workspace,
                                   WorkSpaceLayout *layout,
                                   const char *new_name) ATTR_NONNULL();
bScreen *BKE_workspace_layout_screen_get(const WorkSpaceLayout *layout) GETTER_ATTRS;

/**
 * Get the layout to be activated should \a workspace become or be the active workspace in \a hook.
 */
WorkSpaceLayout *BKE_workspace_active_layout_for_workspace_get(
    const WorkSpaceInstanceHook *hook, const WorkSpace *workspace) GETTER_ATTRS;

bool BKE_workspace_owner_id_check(const WorkSpace *workspace, const char *owner_id) ATTR_NONNULL();

void BKE_workspace_id_tag_all_visible(Main *bmain, int tag) ATTR_NONNULL();

/**
 * Empty the Workspace status items to clear the status bar.
 */
void BKE_workspace_status_clear(struct WorkSpace *workspace);

#undef GETTER_ATTRS
#undef SETTER_ATTRS

/** \} */
