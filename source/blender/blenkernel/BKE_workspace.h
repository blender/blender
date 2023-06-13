/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct bScreen;
struct bToolRef;

/* -------------------------------------------------------------------- */
/** \name Create, Delete, Initialize
 * \{ */

struct WorkSpace *BKE_workspace_add(struct Main *bmain, const char *name);
/**
 * Remove \a workspace by freeing itself and its data. This is a higher-level wrapper that
 * calls #workspace_free_data (through #BKE_id_free) to free the workspace data, and frees
 * other data-blocks owned by \a workspace and its layouts (currently that is screens only).
 *
 * Always use this to remove (and free) workspaces. Don't free non-ID workspace members here.
 */
void BKE_workspace_remove(struct Main *bmain, struct WorkSpace *workspace);

struct WorkSpaceInstanceHook *BKE_workspace_instance_hook_create(const struct Main *bmain,
                                                                 int winid);
void BKE_workspace_instance_hook_free(const struct Main *bmain,
                                      struct WorkSpaceInstanceHook *hook);

/**
 * Add a new layout to \a workspace for \a screen.
 */
struct WorkSpaceLayout *BKE_workspace_layout_add(struct Main *bmain,
                                                 struct WorkSpace *workspace,
                                                 struct bScreen *screen,
                                                 const char *name) ATTR_NONNULL();
void BKE_workspace_layout_remove(struct Main *bmain,
                                 struct WorkSpace *workspace,
                                 struct WorkSpaceLayout *layout) ATTR_NONNULL();

void BKE_workspace_relations_free(ListBase *relation_list);

/** \} */

/* -------------------------------------------------------------------- */
/** \name General Utilities
 * \{ */

struct WorkSpaceLayout *BKE_workspace_layout_find(const struct WorkSpace *workspace,
                                                  const struct bScreen *screen)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
/**
 * Find the layout for \a screen without knowing which workspace to look in.
 * Can also be used to find the workspace that contains \a screen.
 *
 * \param r_workspace: Optionally return the workspace that contains the
 * looked up layout (if found).
 */
struct WorkSpaceLayout *BKE_workspace_layout_find_global(const struct Main *bmain,
                                                         const struct bScreen *screen,
                                                         struct WorkSpace **r_workspace)
    ATTR_NONNULL(1, 2);

/**
 * Circular workspace layout iterator.
 *
 * \param callback: Custom function which gets executed for each layout.
 * Can return false to stop iterating.
 * \param arg: Custom data passed to each \a callback call.
 *
 * \return the layout at which \a callback returned false.
 */
struct WorkSpaceLayout *BKE_workspace_layout_iter_circular(
    const struct WorkSpace *workspace,
    struct WorkSpaceLayout *start,
    bool (*callback)(const struct WorkSpaceLayout *layout, void *arg),
    void *arg,
    bool iter_backward);

void BKE_workspace_tool_remove(struct WorkSpace *workspace, struct bToolRef *tref)
    ATTR_NONNULL(1, 2);

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
void BKE_workspace_tool_id_replace_table(struct WorkSpace *workspace,
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

struct WorkSpace *BKE_workspace_active_get(struct WorkSpaceInstanceHook *hook) GETTER_ATTRS;
void BKE_workspace_active_set(struct WorkSpaceInstanceHook *hook,
                              struct WorkSpace *workspace) SETTER_ATTRS;
/**
 * Get the layout that is active for \a hook (which is the visible layout for the active workspace
 * in \a hook).
 */
struct WorkSpaceLayout *BKE_workspace_active_layout_get(const struct WorkSpaceInstanceHook *hook)
    GETTER_ATTRS;
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
void BKE_workspace_active_layout_set(struct WorkSpaceInstanceHook *hook,
                                     int winid,
                                     struct WorkSpace *workspace,
                                     struct WorkSpaceLayout *layout) SETTER_ATTRS;
struct bScreen *BKE_workspace_active_screen_get(const struct WorkSpaceInstanceHook *hook)
    GETTER_ATTRS;
void BKE_workspace_active_screen_set(struct WorkSpaceInstanceHook *hook,
                                     int winid,
                                     struct WorkSpace *workspace,
                                     struct bScreen *screen) SETTER_ATTRS;

const char *BKE_workspace_layout_name_get(const struct WorkSpaceLayout *layout) GETTER_ATTRS;
void BKE_workspace_layout_name_set(struct WorkSpace *workspace,
                                   struct WorkSpaceLayout *layout,
                                   const char *new_name) ATTR_NONNULL();
struct bScreen *BKE_workspace_layout_screen_get(const struct WorkSpaceLayout *layout) GETTER_ATTRS;

/**
 * Get the layout to be activated should \a workspace become or be the active workspace in \a hook.
 */
struct WorkSpaceLayout *BKE_workspace_active_layout_for_workspace_get(
    const struct WorkSpaceInstanceHook *hook, const struct WorkSpace *workspace) GETTER_ATTRS;

bool BKE_workspace_owner_id_check(const struct WorkSpace *workspace, const char *owner_id)
    ATTR_NONNULL();

void BKE_workspace_id_tag_all_visible(struct Main *bmain, int tag) ATTR_NONNULL();

#undef GETTER_ATTRS
#undef SETTER_ATTRS

/** \} */

#ifdef __cplusplus
}
#endif
