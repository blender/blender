/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_asset.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_viewer_path.hh"
#include "BKE_workspace.hh"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "MEM_guardedalloc.h"

#include "BLO_read_write.hh"

/* -------------------------------------------------------------------- */

static void workspace_init_data(ID *id)
{
  WorkSpace *workspace = (WorkSpace *)id;

  workspace->runtime = MEM_new<blender::bke::WorkSpaceRuntime>(__func__);

  BKE_asset_library_reference_init_default(&workspace->asset_library_ref);
}

static void workspace_free_data(ID *id)
{
  WorkSpace *workspace = (WorkSpace *)id;

  BKE_workspace_relations_free(&workspace->hook_layout_relations);

  BLI_freelistN(&workspace->owner_ids);
  BLI_freelistN(&workspace->layouts);

  while (!BLI_listbase_is_empty(&workspace->tools)) {
    BKE_workspace_tool_remove(workspace, static_cast<bToolRef *>(workspace->tools.first));
  }

  BKE_workspace_status_clear(workspace);
  MEM_delete(workspace->runtime);

  BKE_viewer_path_clear(&workspace->viewer_path);
}

static void workspace_foreach_id(ID *id, LibraryForeachIDData *data)
{
  WorkSpace *workspace = (WorkSpace *)id;

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, workspace->pin_scene, IDWALK_CB_DIRECT_WEAK_LINK);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, workspace->sequencer_scene, IDWALK_CB_DIRECT_WEAK_LINK);

  LISTBASE_FOREACH (WorkSpaceLayout *, layout, &workspace->layouts) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, layout->screen, IDWALK_CB_USER);
  }

  BKE_viewer_path_foreach_id(data, &workspace->viewer_path);
}

static void workspace_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  WorkSpace *workspace = (WorkSpace *)id;

  BLO_write_id_struct(writer, WorkSpace, id_address, &workspace->id);
  BKE_id_blend_write(writer, &workspace->id);
  BLO_write_struct_list(writer, WorkSpaceLayout, &workspace->layouts);
  BLO_write_struct_list(writer, WorkSpaceDataRelation, &workspace->hook_layout_relations);
  BLO_write_struct_list(writer, wmOwnerID, &workspace->owner_ids);
  BLO_write_struct_list(writer, bToolRef, &workspace->tools);
  LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
    if (tref->properties) {
      IDP_BlendWrite(writer, tref->properties);
    }
  }

  BKE_viewer_path_blend_write(writer, &workspace->viewer_path);
}

static void workspace_blend_read_data(BlendDataReader *reader, ID *id)
{
  WorkSpace *workspace = (WorkSpace *)id;

  BLO_read_struct_list(reader, WorkSpaceLayout, &workspace->layouts);
  BLO_read_struct_list(reader, WorkSpaceDataRelation, &workspace->hook_layout_relations);
  BLO_read_struct_list(reader, wmOwnerID, &workspace->owner_ids);
  BLO_read_struct_list(reader, bToolRef, &workspace->tools);

  LISTBASE_FOREACH (WorkSpaceDataRelation *, relation, &workspace->hook_layout_relations) {
    /* Parent pointer does not belong to workspace data and is therefore restored in lib_link step
     * of window manager. */
    /* FIXME: Should not use that untyped #BLO_read_data_address call, especially since it's
     * reference-counting the matching data in readfile code. Problem currently is that there is no
     * type info available for this void pointer (_should_ be pointing to a #WorkSpaceLayout ?), so
     * #BLO_read_get_new_data_address_no_us cannot be used here. */
    BLO_read_data_address(reader, &relation->value);
  }

  LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
    tref->runtime = nullptr;
    BLO_read_struct(reader, IDProperty, &tref->properties);
    IDP_BlendDataRead(reader, &tref->properties);
  }

  workspace->runtime = MEM_new<blender::bke::WorkSpaceRuntime>(__func__);

  /* Do not keep the scene reference when appending a workspace. Setting a scene for a workspace is
   * a convenience feature, but the workspace should never truly depend on scene data. */
  if (ID_IS_LINKED(workspace)) {
    workspace->pin_scene = nullptr;
  }

  id_us_ensure_real(&workspace->id);

  BKE_viewer_path_blend_read_data(reader, &workspace->viewer_path);
}

static void workspace_blend_read_after_liblink(BlendLibReader *reader, ID *id)
{
  WorkSpace *workspace = reinterpret_cast<WorkSpace *>(id);
  Main *bmain = BLO_read_lib_get_main(reader);

  /* Restore proper 'parent' pointers to relevant data, and clean up unused/invalid entries. */
  LISTBASE_FOREACH_MUTABLE (WorkSpaceDataRelation *, relation, &workspace->hook_layout_relations) {
    relation->parent = nullptr;
    LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
      LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
        if (win->winid == relation->parentid) {
          relation->parent = win->workspace_hook;
        }
      }
    }
    if (relation->parent == nullptr) {
      BLI_freelinkN(&workspace->hook_layout_relations, relation);
    }
  }

  LISTBASE_FOREACH_MUTABLE (WorkSpaceLayout *, layout, &workspace->layouts) {
    if (layout->screen) {
      if (ID_IS_LINKED(id)) {
        layout->screen->winid = 0;
        if (layout->screen->temp) {
          /* delete temp layouts when appending */
          BKE_workspace_layout_remove(bmain, workspace, layout);
        }
      }
    }
    else {
      /* If we're reading a layout without screen stored, it's useless and we shouldn't keep it
       * around. */
      BKE_workspace_layout_remove(bmain, workspace, layout);
    }
  }
}

IDTypeInfo IDType_ID_WS = {
    /*id_code*/ WorkSpace::id_type,
    /*id_filter*/ FILTER_ID_WS,
    /*dependencies_id_types*/ FILTER_ID_SCE,
    /*main_listbase_index*/ INDEX_ID_WS,
    /*struct_size*/ sizeof(WorkSpace),
    /*name*/ "WorkSpace",
    /*name_plural*/ N_("workspaces"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_WORKSPACE,
    /*flags*/ IDTYPE_FLAGS_NO_COPY | IDTYPE_FLAGS_ONLY_APPEND | IDTYPE_FLAGS_NO_ANIMDATA |
        IDTYPE_FLAGS_NO_MEMFILE_UNDO | IDTYPE_FLAGS_NEVER_UNUSED,
    /*asset_type_info*/ nullptr,

    /*init_data*/ workspace_init_data,
    /*copy_data*/ nullptr,
    /*free_data*/ workspace_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ workspace_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ workspace_blend_write,
    /*blend_read_data*/ workspace_blend_read_data,
    /*blend_read_after_liblink*/ workspace_blend_read_after_liblink,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

/* -------------------------------------------------------------------- */
/** \name Internal Utils
 * \{ */

static void workspace_layout_name_set(WorkSpace *workspace,
                                      WorkSpaceLayout *layout,
                                      const char *new_name)
{
  STRNCPY(layout->name, new_name);
  BLI_uniquename(&workspace->layouts,
                 layout,
                 "Layout",
                 '.',
                 offsetof(WorkSpaceLayout, name),
                 sizeof(layout->name));
}

/**
 * This should only be used directly when it is to be expected that there isn't
 * a layout within \a workspace that wraps \a screen. Usually - especially outside
 * of BKE_workspace - #BKE_workspace_layout_find should be used!
 */
static WorkSpaceLayout *workspace_layout_find_exec(const WorkSpace *workspace,
                                                   const bScreen *screen)
{
  return static_cast<WorkSpaceLayout *>(
      BLI_findptr(&workspace->layouts, screen, offsetof(WorkSpaceLayout, screen)));
}

static void workspace_relation_add(ListBase *relation_list,
                                   void *parent,
                                   const int parentid,
                                   void *data)
{
  WorkSpaceDataRelation *relation = MEM_callocN<WorkSpaceDataRelation>(__func__);
  relation->parent = parent;
  relation->parentid = parentid;
  relation->value = data;
  /* add to head, if we switch back to it soon we find it faster. */
  BLI_addhead(relation_list, relation);
}
static void workspace_relation_remove(ListBase *relation_list, WorkSpaceDataRelation *relation)
{
  BLI_remlink(relation_list, relation);
  MEM_freeN(relation);
}

static void workspace_relation_ensure_updated(ListBase *relation_list,
                                              void *parent,
                                              const int parentid,
                                              void *data)
{
  WorkSpaceDataRelation *relation = static_cast<WorkSpaceDataRelation *>(BLI_listbase_bytes_find(
      relation_list, &parentid, sizeof(parentid), offsetof(WorkSpaceDataRelation, parentid)));
  if (relation != nullptr) {
    relation->parent = parent;
    relation->value = data;
    /* reinsert at the head of the list, so that more commonly used relations are found faster. */
    BLI_remlink(relation_list, relation);
    BLI_addhead(relation_list, relation);
  }
  else {
    /* no matching relation found, add new one */
    workspace_relation_add(relation_list, parent, parentid, data);
  }
}

static void *workspace_relation_get_data_matching_parent(const ListBase *relation_list,
                                                         const void *parent)
{
  WorkSpaceDataRelation *relation = static_cast<WorkSpaceDataRelation *>(
      BLI_findptr(relation_list, parent, offsetof(WorkSpaceDataRelation, parent)));
  if (relation != nullptr) {
    return relation->value;
  }

  return nullptr;
}

/**
 * Checks if \a screen is already used within any workspace. A screen should never be assigned to
 * multiple WorkSpaceLayouts, but that should be ensured outside of the BKE_workspace module
 * and without such checks.
 * Hence, this should only be used as assert check before assigning a screen to a workspace.
 */
#ifndef NDEBUG
static bool workspaces_is_screen_used
#else
static bool UNUSED_FUNCTION(workspaces_is_screen_used)
#endif
    (const Main *bmain, bScreen *screen)
{
  for (WorkSpace *workspace = static_cast<WorkSpace *>(bmain->workspaces.first); workspace;
       workspace = static_cast<WorkSpace *>(workspace->id.next))
  {
    if (workspace_layout_find_exec(workspace, screen)) {
      return true;
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Create, Delete, Init
 * \{ */

WorkSpace *BKE_workspace_add(Main *bmain, const char *name)
{
  WorkSpace *new_workspace = BKE_id_new<WorkSpace>(bmain, name);
  id_us_ensure_real(&new_workspace->id);
  return new_workspace;
}

void BKE_workspace_remove(Main *bmain, WorkSpace *workspace)
{
  for (WorkSpaceLayout *layout = static_cast<WorkSpaceLayout *>(workspace->layouts.first),
                       *layout_next;
       layout;
       layout = layout_next)
  {
    layout_next = layout->next;
    BKE_workspace_layout_remove(bmain, workspace, layout);
  }
  BKE_id_free(bmain, workspace);
}

WorkSpaceInstanceHook *BKE_workspace_instance_hook_create(const Main *bmain, const int winid)
{
  WorkSpaceInstanceHook *hook = MEM_callocN<WorkSpaceInstanceHook>(__func__);

  /* set an active screen-layout for each possible window/workspace combination */
  for (WorkSpace *workspace = static_cast<WorkSpace *>(bmain->workspaces.first); workspace;
       workspace = static_cast<WorkSpace *>(workspace->id.next))
  {
    BKE_workspace_active_layout_set(
        hook, winid, workspace, static_cast<WorkSpaceLayout *>(workspace->layouts.first));
  }

  return hook;
}
void BKE_workspace_instance_hook_free(const Main *bmain, WorkSpaceInstanceHook *hook)
{
  /* workspaces should never be freed before wm (during which we call this function).
   * However, when running in background mode, loading a blend file may allocate windows (that need
   * to be freed) without creating workspaces. This happens in BlendfileLoadingBaseTest. */
  BLI_assert(!BLI_listbase_is_empty(&bmain->workspaces) || G.background);

  /* Free relations for this hook */
  for (WorkSpace *workspace = static_cast<WorkSpace *>(bmain->workspaces.first); workspace;
       workspace = static_cast<WorkSpace *>(workspace->id.next))
  {
    for (WorkSpaceDataRelation *relation = static_cast<WorkSpaceDataRelation *>(
                                   workspace->hook_layout_relations.first),
                               *relation_next;
         relation;
         relation = relation_next)
    {
      relation_next = relation->next;
      if (relation->parent == hook) {
        workspace_relation_remove(&workspace->hook_layout_relations, relation);
      }
    }
  }

  MEM_freeN(hook);
}

WorkSpaceLayout *BKE_workspace_layout_add(Main *bmain,
                                          WorkSpace *workspace,
                                          bScreen *screen,
                                          const char *name)
{
  WorkSpaceLayout *layout = MEM_callocN<WorkSpaceLayout>(__func__);

  BLI_assert(!workspaces_is_screen_used(bmain, screen));
#ifdef NDEBUG
  UNUSED_VARS(bmain);
#endif
  layout->screen = screen;
  id_us_plus(&layout->screen->id);
  workspace_layout_name_set(workspace, layout, name);
  BLI_addtail(&workspace->layouts, layout);

  return layout;
}

void BKE_workspace_layout_remove(Main *bmain, WorkSpace *workspace, WorkSpaceLayout *layout)
{
  /* Screen should usually be set, but we call this from file reading to get rid of invalid
   * layouts. */
  if (layout->screen) {
    id_us_min(&layout->screen->id);
    BKE_id_free(bmain, layout->screen);
  }
  BLI_freelinkN(&workspace->layouts, layout);
}

void BKE_workspace_relations_free(ListBase *relation_list)
{
  for (WorkSpaceDataRelation *
           relation = static_cast<WorkSpaceDataRelation *>(relation_list->first),
          *relation_next;
       relation;
       relation = relation_next)
  {
    relation_next = relation->next;
    workspace_relation_remove(relation_list, relation);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name General Utils
 * \{ */

WorkSpaceLayout *BKE_workspace_layout_find(const WorkSpace *workspace, const bScreen *screen)
{
  WorkSpaceLayout *layout = workspace_layout_find_exec(workspace, screen);
  if (layout) {
    return layout;
  }

  printf(
      "%s: Couldn't find layout in this workspace: '%s' screen: '%s'. "
      "This should not happen!\n",
      __func__,
      workspace->id.name + 2,
      screen->id.name + 2);

  return nullptr;
}

WorkSpaceLayout *BKE_workspace_layout_find_global(const Main *bmain,
                                                  const bScreen *screen,
                                                  WorkSpace **r_workspace)
{
  if (r_workspace) {
    *r_workspace = nullptr;
  }

  for (WorkSpace *workspace = static_cast<WorkSpace *>(bmain->workspaces.first); workspace;
       workspace = static_cast<WorkSpace *>(workspace->id.next))
  {
    WorkSpaceLayout *layout = workspace_layout_find_exec(workspace, screen);
    if (layout) {
      if (r_workspace) {
        *r_workspace = workspace;
      }

      return layout;
    }
  }

  return nullptr;
}

WorkSpaceLayout *BKE_workspace_layout_iter_circular(const WorkSpace *workspace,
                                                    WorkSpaceLayout *start,
                                                    bool (*callback)(const WorkSpaceLayout *layout,
                                                                     void *arg),
                                                    void *arg,
                                                    const bool iter_backward)
{
  WorkSpaceLayout *iter_layout;

  if (iter_backward) {
    LISTBASE_CIRCULAR_BACKWARD_BEGIN (WorkSpaceLayout *, &workspace->layouts, iter_layout, start) {
      if (!callback(iter_layout, arg)) {
        return iter_layout;
      }
    }
    LISTBASE_CIRCULAR_BACKWARD_END(WorkSpaceLayout *, &workspace->layouts, iter_layout, start);
  }
  else {
    LISTBASE_CIRCULAR_FORWARD_BEGIN (WorkSpaceLayout *, &workspace->layouts, iter_layout, start) {
      if (!callback(iter_layout, arg)) {
        return iter_layout;
      }
    }
    LISTBASE_CIRCULAR_FORWARD_END(WorkSpaceLayout *, &workspace->layouts, iter_layout, start);
  }

  return nullptr;
}

void BKE_workspace_tool_remove(WorkSpace *workspace, bToolRef *tref)
{
  if (tref->runtime) {
    MEM_freeN(tref->runtime);
  }
  if (tref->properties) {
    IDP_FreeProperty(tref->properties);
  }
  BLI_remlink(&workspace->tools, tref);
  MEM_freeN(tref);
}

void BKE_workspace_tool_id_replace_table(WorkSpace *workspace,
                                         const int space_type,
                                         const int mode,
                                         const char *idname_prefix_skip,
                                         const char *replace_table[][2],
                                         int replace_table_num)
{
  const size_t idname_prefix_len = idname_prefix_skip ? strlen(idname_prefix_skip) : 0;
  const size_t idname_suffix_maxncpy = sizeof(bToolRef::idname) - idname_prefix_len;

  LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
    if (!(tref->space_type == space_type && tref->mode == mode)) {
      continue;
    }
    char *idname_suffix = tref->idname;
    if (idname_prefix_skip) {
      if (!STRPREFIX(idname_suffix, idname_prefix_skip)) {
        continue;
      }
      idname_suffix += idname_prefix_len;
    }
    BLI_string_replace_table_exact(
        idname_suffix, idname_suffix_maxncpy, replace_table, replace_table_num);
  }
}

bool BKE_workspace_owner_id_check(const WorkSpace *workspace, const char *owner_id)
{
  if ((*owner_id == '\0') || ((workspace->flags & WORKSPACE_USE_FILTER_BY_ORIGIN) == 0)) {
    return true;
  }

  /* We could use hash lookup, for now this list is highly likely under < ~16 items. */
  return BLI_findstring(&workspace->owner_ids, owner_id, offsetof(wmOwnerID, name)) != nullptr;
}

void BKE_workspace_id_tag_all_visible(Main *bmain, int tag)
{
  BKE_main_id_tag_listbase(&bmain->workspaces, tag, false);
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    WorkSpace *workspace = BKE_workspace_active_get(win->workspace_hook);
    workspace->id.tag |= tag;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Getters/Setters
 * \{ */

WorkSpace *BKE_workspace_active_get(WorkSpaceInstanceHook *hook)
{
  return hook->active;
}
void BKE_workspace_active_set(WorkSpaceInstanceHook *hook, WorkSpace *workspace)
{
  /* DO NOT check for `hook->active == workspace` here. Caller code is supposed to do it if
   * that optimization is possible and needed.
   * This code can be called from places where we might have this equality, but still want to
   * ensure/update the active layout below.
   * Known case where this is buggy and will crash later due to nullptr active layout: reading
   * a blend file, when the new read workspace ID happens to have the exact same memory address
   * as when it was saved in the blend file (extremely unlikely, but possible). */

  hook->active = workspace;
  if (workspace) {
    WorkSpaceLayout *layout = static_cast<WorkSpaceLayout *>(
        workspace_relation_get_data_matching_parent(&workspace->hook_layout_relations, hook));
    if (layout) {
      hook->act_layout = layout;
    }
  }
}

WorkSpaceLayout *BKE_workspace_active_layout_get(const WorkSpaceInstanceHook *hook)
{
  return hook->act_layout;
}

WorkSpaceLayout *BKE_workspace_active_layout_for_workspace_get(const WorkSpaceInstanceHook *hook,
                                                               const WorkSpace *workspace)
{
  /* If the workspace is active, the active layout can be returned, no need for a lookup. */
  if (hook->active == workspace) {
    return hook->act_layout;
  }

  /* Inactive workspace */
  return static_cast<WorkSpaceLayout *>(
      workspace_relation_get_data_matching_parent(&workspace->hook_layout_relations, hook));
}

void BKE_workspace_active_layout_set(WorkSpaceInstanceHook *hook,
                                     const int winid,
                                     WorkSpace *workspace,
                                     WorkSpaceLayout *layout)
{
  hook->act_layout = layout;
  workspace_relation_ensure_updated(&workspace->hook_layout_relations, hook, winid, layout);
}

bScreen *BKE_workspace_active_screen_get(const WorkSpaceInstanceHook *hook)
{
  return hook->act_layout->screen;
}
void BKE_workspace_active_screen_set(WorkSpaceInstanceHook *hook,
                                     const int winid,
                                     WorkSpace *workspace,
                                     bScreen *screen)
{
  /* we need to find the WorkspaceLayout that wraps this screen */
  WorkSpaceLayout *layout = BKE_workspace_layout_find(hook->active, screen);
  BKE_workspace_active_layout_set(hook, winid, workspace, layout);
}

const char *BKE_workspace_layout_name_get(const WorkSpaceLayout *layout)
{
  return layout->name;
}
void BKE_workspace_layout_name_set(WorkSpace *workspace,
                                   WorkSpaceLayout *layout,
                                   const char *new_name)
{
  workspace_layout_name_set(workspace, layout, new_name);
}

bScreen *BKE_workspace_layout_screen_get(const WorkSpaceLayout *layout)
{
  return layout->screen;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Status
 * \{ */

void BKE_workspace_status_clear(WorkSpace *workspace)
{
  workspace->runtime->status.clear_and_shrink();
}

/** \} */
