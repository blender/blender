/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edundo
 */

#include <atomic>
#include <cstring>

#include "CLG_log.h"

#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.hh"
#include "BLI_task_c.hh"
#include "BLI_utildefines.hh"
#include "BLI_vector.hh"

#include "BKE_blender_undo.hh"
#include "BKE_callbacks.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_undo_system.hh"
#include "BKE_workspace.hh"

#include "BLO_blend_validate.hh"

#include "ED_asset.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_render.hh"
#include "ED_screen.hh"
#include "ED_sculpt.hh"
#include "ED_undo.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

namespace blender {

/** We only need this locally. */
static CLG_LogRef LOG = {"undo"};

/* -------------------------------------------------------------------- */
/** \name Generic Undo System Access
 *
 * Non-operator undo editor functions.
 * \{ */

bool ED_undo_is_state_valid(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* Currently only checks matching begin/end calls. */
  if (wm->runtime->undo_stack == nullptr) {
    /* No undo stack is valid, nothing to do. */
    return true;
  }
  if (wm->runtime->undo_stack->group_level != 0) {
    /* If this fails #ED_undo_grouped_begin, #ED_undo_grouped_end calls don't match. */
    return false;
  }
  if (wm->runtime->undo_stack->step_active != nullptr) {
    if (wm->runtime->undo_stack->step_active->skip == true) {
      /* Skip is only allowed between begin/end calls,
       * a state that should never happen in main event loop. */
      return false;
    }
  }
  return true;
}

void ED_undo_group_begin(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  BKE_undosys_stack_group_begin(wm->runtime->undo_stack);
}

void ED_undo_group_end(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  BKE_undosys_stack_group_end(wm->runtime->undo_stack);
}

void ED_undo_push(bContext *C, const char *str, UndoEncodeHints hints)
{
  CLOG_INFO(&LOG, "Push '%s'", str);
  WM_file_tag_modified();

  wmWindowManager *wm = CTX_wm_manager(C);
  if (G.background) {
    /* Python developers may have explicitly created the undo stack in background mode,
     * otherwise allow it to be nullptr, see: #60934.
     * Otherwise it must never be nullptr, even when undo is disabled. */
    if (wm->runtime->undo_stack == nullptr) {
      return;
    }
  }

  int steps = U.undosteps;

  /* Ensure steps that have been initialized are always pushed,
   * even when undo steps are zero.
   *
   * Note that some modes (paint, sculpt) initialize an undo step before an action runs,
   * then accumulate changes there, or restore data from it in the case of 2D painting.
   *
   * For this reason we need to handle the undo step even when undo steps is set to zero.
   */
  if ((steps <= 0) && wm->runtime->undo_stack->step_init != nullptr) {
    steps = 1;
  }
  if (steps <= 0) {
    return;
  }

  eUndoPushReturn push_retval;

  /* Only apply limit if this is the last undo step. */
  if (wm->runtime->undo_stack->step_active &&
      (wm->runtime->undo_stack->step_active->next == nullptr))
  {
    BKE_undosys_stack_limit_steps_and_memory(wm->runtime->undo_stack, steps - 1, 0);
  }

  push_retval = BKE_undosys_step_push(wm->runtime->undo_stack, C, str, hints);

  if (U.undomemory != 0) {
    const size_t memory_limit = size_t(U.undomemory) * 1024 * 1024;
    BKE_undosys_stack_limit_steps_and_memory(wm->runtime->undo_stack, -1, memory_limit);
  }

  if (CLOG_CHECK(&LOG, CLG_LEVEL_DEBUG)) {
    BKE_undosys_print(wm->runtime->undo_stack);
  }

  if (push_retval & UNDO_PUSH_RET_OVERRIDE_CHANGED) {
    WM_main_add_notifier(NC_WM | ND_LIB_OVERRIDE_CHANGED, nullptr);
  }
}

/**
 * Common pre management of undo/redo (killing all running jobs, calling pre handlers, etc.).
 */
static void ed_undo_step_pre(bContext *C,
                             wmWindowManager *wm,
                             const enum eUndoStepDir undo_dir,
                             ReportList *reports)
{
  BLI_assert(ELEM(undo_dir, STEP_UNDO, STEP_REDO));

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  /* undo during jobs are running can easily lead to freeing data using by jobs,
   * or they can just lead to freezing job in some other cases */
  WM_jobs_kill_all(wm);

  if (G.debug & G_DEBUG_IO) {
    if (bmain->lock != nullptr) {
      BKE_report(
          reports, RPT_DEBUG, "Checking validity of current .blend file *BEFORE* undo step");
      BLO_main_validate_libraries(bmain, reports);
    }
  }

  /* App-Handlers (pre). */
  {
    /* NOTE: ignore grease pencil for now. */
    wm->op_undo_depth++;
    BKE_callback_exec_id(
        bmain, &scene->id, (undo_dir == STEP_UNDO) ? BKE_CB_EVT_UNDO_PRE : BKE_CB_EVT_REDO_PRE);
    wm->op_undo_depth--;
  }
}

/**
 * Common post management of undo/redo (calling post handlers, adding notifiers etc.).
 *
 * \note Also check #undo_history_exec in bottom if you change notifiers.
 */
static void ed_undo_step_post(bContext *C,
                              wmWindowManager *wm,
                              const enum eUndoStepDir undo_dir,
                              ReportList *reports)
{
  using namespace blender::ed;
  BLI_assert(ELEM(undo_dir, STEP_UNDO, STEP_REDO));

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  /* App-Handlers (post). */
  {
    wm->op_undo_depth++;
    BKE_callback_exec_id(
        bmain, &scene->id, (undo_dir == STEP_UNDO) ? BKE_CB_EVT_UNDO_POST : BKE_CB_EVT_REDO_POST);
    wm->op_undo_depth--;
  }

  if (G.debug & G_DEBUG_IO) {
    if (bmain->lock != nullptr) {
      BKE_report(reports, RPT_INFO, "Checking validity of current .blend file *AFTER* undo step");
      BLO_main_validate_libraries(bmain, reports);
    }
  }

  /* Undo/redo may have invalidated a lot of data, ensure UI has also been fully updated before
   * handling next events. */
  WM_event_handling_break(*C);

  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  WM_event_add_notifier(C, NC_WM | ND_UNDO, nullptr);

  WM_toolsystem_refresh_active(C);
  WM_toolsystem_refresh_screen_all(bmain);

  asset::list::storage_tag_main_data_dirty();

  if (CLOG_CHECK(&LOG, CLG_LEVEL_DEBUG)) {
    BKE_undosys_print(wm->runtime->undo_stack);
  }
}

/**
 * Undo or redo one step from current active one.
 * May undo or redo several steps at once only if the target step is a 'skipped' one.
 * The target step will be the one immediately before or after the active one.
 */
static wmOperatorStatus ed_undo_step_direction(bContext *C,
                                               enum eUndoStepDir step,
                                               ReportList *reports)
{
  BLI_assert(ELEM(step, STEP_UNDO, STEP_REDO));

  CLOG_INFO(&LOG, "Step direction=%s", (step == STEP_UNDO) ? "STEP_UNDO" : "STEP_REDO");

  wmWindowManager *wm = CTX_wm_manager(C);

  ed_undo_step_pre(C, wm, step, reports);

  if (step == STEP_UNDO) {
    BKE_undosys_step_undo(wm->runtime->undo_stack, C);
  }
  else {
    BKE_undosys_step_redo(wm->runtime->undo_stack, C);
  }

  ed_undo_step_post(C, wm, step, reports);

  return OPERATOR_FINISHED;
}

/**
 * Undo the step matching given name.
 * May undo several steps at once.
 * The target step will be the one immediately before given named one.
 */
static int ed_undo_step_by_name(bContext *C, const char *undo_name, ReportList *reports)
{
  BLI_assert(undo_name != nullptr);

  wmWindowManager *wm = CTX_wm_manager(C);
  UndoStep *undo_step_from_name = BKE_undosys_step_find_by_name(wm->runtime->undo_stack,
                                                                undo_name);
  if (undo_step_from_name == nullptr) {
    CLOG_ERROR(&LOG, "Step name='%s' not found in current undo stack", undo_name);

    return OPERATOR_CANCELLED;
  }

  UndoStep *undo_step_target = undo_step_from_name->prev;
  if (undo_step_target == nullptr) {
    CLOG_ERROR(&LOG, "Step name='%s' cannot be undone", undo_name);

    return OPERATOR_CANCELLED;
  }

  const int undo_dir_i = BKE_undosys_step_calc_direction(
      wm->runtime->undo_stack, undo_step_target, nullptr);
  BLI_assert(ELEM(undo_dir_i, -1, 1));
  const enum eUndoStepDir undo_dir = (undo_dir_i == -1) ? STEP_UNDO : STEP_REDO;

  CLOG_INFO(&LOG,
            "Step name='%s', found direction=%s",
            undo_name,
            (undo_dir == STEP_UNDO) ? "STEP_UNDO" : "STEP_REDO");

  ed_undo_step_pre(C, wm, undo_dir, reports);

  BKE_undosys_step_load_data_ex(wm->runtime->undo_stack, C, undo_step_target, nullptr, true);

  ed_undo_step_post(C, wm, undo_dir, reports);

  return OPERATOR_FINISHED;
}

/**
 * Load the step matching given index in the stack.
 * May undo or redo several steps at once.
 * The target step will be the one indicated by the given index.
 */
static int ed_undo_step_by_index(bContext *C, const int undo_index, ReportList *reports)
{
  BLI_assert(undo_index >= 0);

  wmWindowManager *wm = CTX_wm_manager(C);
  const int active_step_index = BLI_findindex(&wm->runtime->undo_stack->steps,
                                              wm->runtime->undo_stack->step_active);
  if (undo_index == active_step_index) {
    return OPERATOR_CANCELLED;
  }
  const enum eUndoStepDir undo_dir = (undo_index < active_step_index) ? STEP_UNDO : STEP_REDO;

  CLOG_INFO(&LOG,
            "Step index='%d', found direction=%s",
            undo_index,
            (undo_dir == STEP_UNDO) ? "STEP_UNDO" : "STEP_REDO");

  ed_undo_step_pre(C, wm, undo_dir, reports);

  BKE_undosys_step_load_from_index(wm->runtime->undo_stack, C, undo_index);

  ed_undo_step_post(C, wm, undo_dir, reports);

  return OPERATOR_FINISHED;
}

static bool ed_undo_grouped_check_by_name(const UndoStack *ustack, const char *str)
{
  const UndoStep *us = ustack->step_active;
  if (us && STREQ(str, us->name)) {
    return true;
  }
  return false;
}

void ED_undo_grouped_push(bContext *C, const char *str, UndoEncodeHints hints)
{
  /* do nothing if previous undo task is the same as this one (or from the same undo group) */
  wmWindowManager *wm = CTX_wm_manager(C);
  UndoStack *ustack = wm->runtime->undo_stack;
  /* See matching check in #ED_undo_push. */
  if (G.background) {
    if (ustack == nullptr) {
      return;
    }
  }
  if (ed_undo_grouped_check_by_name(ustack, str)) {
    BKE_undosys_stack_clear_active(ustack);
  }

  /* push as usual */
  ED_undo_push(C, str, hints);
}

void ED_undo_pop(bContext *C)
{
  ed_undo_step_direction(C, STEP_UNDO, nullptr);
}
void ED_undo_redo(bContext *C)
{
  ed_undo_step_direction(C, STEP_REDO, nullptr);
}

/**
 * Check if any ID was modified (requiring a global undo step),
 * setting `modified` as true and exiting as early as possible.
 *
 * NOTE(@ideasman42):
 * - Run in parallel to prevent many ID's of one type preventing an early exit for another.
 * - Regarding performance, in my tests with heavy files the overhead was minimal (see !161566).
 *   If we wanted we could track if any ID's have #ID::recalc_after_undo_push enabled,
 *   it's simple in principle but adds extra state to keep track of.
 */
static void ed_undo_push_check_id_changes_fn(TaskPool *__restrict pool, void *taskdata)
{
  const ID *id_first = static_cast<const ID *>(taskdata);
  std::atomic<bool> &modified = *static_cast<std::atomic<bool> *>(BLI_task_pool_user_data(pool));

  /* The #ID_KE & edit-mode id-types are typically only hit when Python scripts modify ID
   * (keys or object-data) while inside edit-mode that is not itself edit-mode data.
   * While this isn't common it's not an error and should be supported. */

  const ID_Type id_type = GS(id_first->name);
  if (id_type == ID_OB) {
    /* Object (particle system edit-mode only). */
    for (const ID *id = id_first; id && !modified.load(); id = static_cast<ID *>(id->next)) {
      if (id->recalc_after_undo_push == false) {
        continue;
      }

      /* - Every particle-edit stroke tags the underlying object.
       * - This doesn't give us a convenient way to differentiate
       *   between particle system & other object data.
       * - Resolve by checking #ID_RECALC_PSYS_ALL
       *
       * Not ideal but the alternative would be not to support particle-editing at all. */
      const Object *ob = reinterpret_cast<const Object *>(id);
      if (ob->mode & OB_MODE_PARTICLE_EDIT) {
        if ((id->recalc_after_undo_push & ~(ID_RECALC_PSYS_ALL)) == 0) {
          continue;
        }
      }
      modified.store(true); /* Breaks. */
    }
  }
  else if (id_type == ID_KE) {
    /* Key-blocks (check for edit-mode). */
    for (const ID *id = id_first; id && !modified.load(); id = static_cast<ID *>(id->next)) {
      if (id->recalc_after_undo_push == false) {
        continue;
      }
      /* Skip shape-keys on data in edit-mode for the same reason
       * as the #OB_DATA_SUPPORT_EDITMODE below. */
      const ID *key_from = reinterpret_cast<const ID *>(reinterpret_cast<const Key *>(id)->from);
      if (key_from && OB_DATA_SUPPORT_EDITMODE(GS(key_from->name)) &&
          BKE_object_data_is_in_editmode(nullptr, key_from))
      {
        continue;
      }
      modified.store(true); /* Breaks. */
    }
  }
  else if (OB_DATA_SUPPORT_EDITMODE(id_type)) {
    /* Object data (check for edit-mode). */
    for (const ID *id = id_first; id && !modified.load(); id = static_cast<ID *>(id->next)) {
      if (id->recalc_after_undo_push == false) {
        continue;
      }

      /* Skip data in edit-mode: changes are stored by the modes own undo system and
       * practically all edit-mode operations tag the data being edited. */
      if (BKE_object_data_is_in_editmode(nullptr, id)) {
        continue;
      }
      modified.store(true); /* Breaks. */
    }
  }
  else {
    /* Any ID types. */
    for (const ID *id = id_first; id && !modified.load(); id = static_cast<ID *>(id->next)) {
      if (id->recalc_after_undo_push == false) {
        continue;
      }
      modified.store(true); /* Breaks. */
    }
  }
}

/**
 * Check if any ID's have changes only the `memfile` undo system would store.
 *
 * Note that #Main::is_memfile_undo_written only detects adding/removing/renaming ID's,
 * modifying existing ID's in-place (e.g. adding a modifier to an object) is detected
 * by scanning for ID's tagged since the last `memfile` undo step,
 * using the same tag the `memfile` undo system uses to replay changes.
 */
static bool ed_undo_push_check_id_changes_outside_mode(Main *bmain)
{
  /* ID's added/removed/renamed. */
  if (bmain->is_memfile_undo_written == false) {
    return true;
  }

  /* Gather modified ID's, skipping ID types that are not stored by undo. */
  Vector<const ID *> ids;
  const MainListsArray lbarray = BKE_main_lists_get(*bmain);
  for (const ListBaseT<ID> *lb : lbarray) {
    const ID *id_first = static_cast<const ID *>(lb->first);
    if ((id_first == nullptr) || !ID_CHECK_UNDO(id_first)) {
      continue;
    }
    ids.append(id_first);
  }

  /* Check the gathered ID's in parallel, one task per ID type.
   *
   * NOTE(@ideasman42): This may create some tasks without much work:
   * Something we typically want to avoid. We could optimize this by doing
   * a single threaded pass on the first N items in each list.
   * However it adds code complexity without solving any real world problem.
   * Accept the overhead in the case where there isn't much work to improve
   * the worst when there are many ID's. */
  std::atomic<bool> modified = false;
  TaskPool *task_pool = BLI_task_pool_create(&modified, TASK_PRIORITY_HIGH);
  for (const ID *id_first : ids) {
    if (modified.load()) {
      break;
    }
    BLI_task_pool_push(
        task_pool, ed_undo_push_check_id_changes_fn, const_cast<ID *>(id_first), false, nullptr);
  }
  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  return modified.load();
}

static UndoEncodeHints undo_encode_hints_from_context(bContext *C, const char *str)
{
  const UndoStack *ustack = CTX_wm_manager(C)->runtime->undo_stack;
  UndoEncodeHints hints = UndoEncodeHints::None;

  /* The undo stack may be null in background mode where undo pushes are a no-op,
   * see matching check in #ED_undo_push. */
  if (ustack == nullptr) {
    return hints;
  }

  /* Match the undo type #BKE_undosys_step_push uses,
   * the context is only a fallback when no undo step has been initialized. */
  const UndoType *ut = ustack->step_init ? ustack->step_init->type :
                                           BKE_undosys_type_from_context(C);

  if (ELEM(ut, nullptr, BKE_UNDOSYS_TYPE_MEMFILE)) {
    return hints;
  }

  if (ut->flags & UNDOTYPE_FLAG_ENCODE_PRE_MEMFILE_SUPPORTED) {
    /* When the undo step for this operator is not stored in the memfile undo system,
     * changes to ID's made by the operator would be excluded from the undo step.
     * Request a memfile undo step before this operators step so they're included. */
    Main *bmain = CTX_data_main(C);
    if (ed_undo_push_check_id_changes_outside_mode(bmain)) {
      CLOG_DEBUG(&LOG, "adding additional MEMFILE undo step: %s", str);
      hints |= UndoEncodeHints::PreMemFileChanges;
    }
  }

  return hints;
}

void ED_undo_push_op(bContext *C, wmOperator *op)
{
  UndoEncodeHints hints = undo_encode_hints_from_context(C, op->type->name);

  /* in future, get undo string info? */
  ED_undo_push(C, op->type->name, hints);
}

void ED_undo_grouped_push_op(bContext *C, wmOperator *op)
{
  const char *str = (op->type->undo_group[0] != '\0') ? op->type->undo_group : op->type->name;
  UndoEncodeHints hints = undo_encode_hints_from_context(C, str);
  ED_undo_grouped_push(C, str, hints);
}

void ED_undo_pop_op(bContext *C, wmOperator *op)
{
  /* search back a couple of undo's, in case something else added pushes */
  ed_undo_step_by_name(C, op->type->name, op->reports);
}

bool ED_undo_is_valid(const bContext *C, const char *undoname)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  UndoStack *ustack = wm->runtime->undo_stack;
  return ustack && BKE_undosys_stack_has_undo(ustack, undoname);
}

bool ED_undo_has_redo_step(const bContext *C)
{
  const wmWindowManager *wm = CTX_wm_manager(C);
  UndoStack *ustack = wm->runtime->undo_stack;
  return ustack && BKE_undosys_stack_has_redo(ustack);
}

bool ED_undo_is_memfile_compatible(const bContext *C)
{
  /* Some modes don't co-exist with memfile undo, disable their use: #60593
   * (this matches 2.7x behavior). */
  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  if (view_layer != nullptr) {
    BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
    Object *obact = BKE_view_layer_active_object_get(view_layer);
    if (obact != nullptr) {
      if (obact->mode & OB_MODE_EDIT) {
        return false;
      }
    }
  }
  return true;
}

std::optional<UndoEncodeHints> ED_undo_is_legacy_compatible_for_property(bContext *C,
                                                                         ID *id,
                                                                         const PointerRNA &ptr,
                                                                         const PropertyRNA &prop)
{
  if (!RNA_property_undo_check(&prop, ptr.type)) {
    return std::nullopt;
  }
  /* If the whole ID type doesn't support undo there is no need to check the current context. */
  if (id && !ID_CHECK_UNDO(id)) {
    return std::nullopt;
  }

  if (ELEM(&prop,
           /* Exception for renaming ID data, we always need undo pushes in this case,
            * because undo systems track data by their ID, see: #67002. */
           &rna_ID_name,
           /* Exception for active shape-key, since changing this in edit-mode updates
            * the shape key from object mode data. */
           &rna_Object_active_shape_key_index))
  {
    return UndoEncodeHints::None;
  }

  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  if (view_layer != nullptr) {
    BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
    Object *obact = BKE_view_layer_active_object_get(view_layer);
    if (obact != nullptr) {
      if (obact->mode & OB_MODE_SCULPT) {
        /* Changing properties while in sculpt mode is expensive due to the paint BVH rebuild.
         * Avoid pushing such undo steps for now. */
        CLOG_DEBUG(&LOG, "skipping undo for sculpt-mode");
        return std::nullopt;
      }
      if (obact->mode & OB_MODE_PARTICLE_EDIT) {
        /* Particle-edit undo steps only store point-cache data
         * (see #particle_undosys_step_encode), never generic ID/property state,
         * so redirect property changes to a `memfile` step. */
        return UndoEncodeHints::PreMemFileChanges | UndoEncodeHints::HasMemFileChangesOnly;
      }
      if (obact->mode & OB_MODE_EDIT) {
        /* Check if the `id` is covered by edit-mode undo. */

        /* When the object is in edit-mode, returning None here is fine as the edit-mode
         * data will track the undo state.
         *
         * Checking if the ID itself is in edit-mode ensures (for example),
         * that a mesh pinned in the properties editor which is *not* in edit-mode
         * has its undo data tracked even the active mesh is in edit-mode. */

        const bool is_id_in_editmode = (id && obact->data &&
                                        GS(id->name) == GS(((ID *)obact->data)->name) &&
                                        /* Only object-data that stores it's own edit-mode
                                         * data tracks it's own undo steps.
                                         * So passing object as null is OK here. */
                                        BKE_object_data_is_in_editmode(nullptr, id));

        if (is_id_in_editmode == false) {
          bool skip_undo = true;
          /* Make a special exception for mesh and tool-settings
           * because the undo steps store some tool-settings. */
          if (obact->type == OB_MESH) {
            if (RNA_struct_is_a(ptr.type, RNA_ToolSettings)) {
              skip_undo = false;
            }
          }

          if (skip_undo) {
            /* The property isn't stored by edit-mode undo steps (id type mismatch),
             * store the change in a memfile step, the edit-mode step has no changes itself. */
            CLOG_DEBUG(&LOG, "memfile-only undo for edit-mode");
            return UndoEncodeHints::PreMemFileChanges | UndoEncodeHints::HasMemFileChangesOnly;
          }
        }
      }
    }
  }
  return UndoEncodeHints::None;
}

UndoStack *ED_undo_stack_get()
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
  return wm->runtime->undo_stack;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Undo, Undo Push & Redo Operators
 * \{ */

/**
 * Refresh to run after user activated undo/redo actions.
 */
static void ed_undo_refresh_for_op(bContext *C)
{
  /* The "last operator" should disappear, later we can tie this with undo stack nicer. */
  WM_operator_stack_clear(CTX_wm_manager(C));

  /* Keep button under the cursor active. */
  WM_event_add_mousemove(CTX_wm_window(C));

  ED_outliner_select_sync_from_all_tag(C);
}

static wmOperatorStatus ed_undo_exec(bContext *C, wmOperator *op)
{
  /* "last operator" should disappear, later we can tie this with undo stack nicer */
  WM_operator_stack_clear(CTX_wm_manager(C));
  wmOperatorStatus ret = ed_undo_step_direction(C, STEP_UNDO, op->reports);
  if (ret & OPERATOR_FINISHED) {
    ed_undo_refresh_for_op(C);
  }
  return ret;
}

static wmOperatorStatus ed_undo_push_exec(bContext *C, wmOperator *op)
{
  if (G.background) {
    /* Exception for background mode, see: #60934.
     * NOTE: since the undo stack isn't initialized on startup, background mode behavior
     * won't match regular usage, this is just for scripts to do explicit undo pushes. */
    wmWindowManager *wm = CTX_wm_manager(C);
    if (wm->runtime->undo_stack == nullptr) {
      wm->runtime->undo_stack = BKE_undosys_stack_create();
    }
  }
  char str[BKE_UNDO_STR_MAX];
  RNA_string_get(op->ptr, "message", str);
  ED_undo_push(C, str);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus ed_redo_exec(bContext *C, wmOperator *op)
{
  wmOperatorStatus ret = ed_undo_step_direction(C, STEP_REDO, op->reports);
  if (ret & OPERATOR_FINISHED) {
    ed_undo_refresh_for_op(C);
  }
  return ret;
}

static wmOperatorStatus ed_undo_redo_exec(bContext *C, wmOperator * /*op*/)
{
  wmOperator *last_op = WM_operator_last_redo(C);
  wmOperatorStatus ret = ED_undo_operator_repeat(C, last_op) ? OPERATOR_FINISHED :
                                                               OPERATOR_CANCELLED;
  if (ret & OPERATOR_FINISHED) {
    /* Keep button under the cursor active. */
    WM_event_add_mousemove(CTX_wm_window(C));
  }
  return ret;
}

/* Disable in background mode, we could support if it's useful, #60934. */

static bool ed_undo_is_init_poll(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm->runtime->undo_stack == nullptr) {
    /* This message is intended for Python developers,
     * it will be part of the exception when attempting to call undo in background mode. */
    CTX_wm_operator_poll_msg_set(
        C,
        "Undo disabled at startup in background-mode "
        "(call `ed.undo_push()` to explicitly initialize the undo-system)");
    return false;
  }
  return true;
}

static bool ed_undo_is_init_and_screenactive_poll(bContext *C)
{
  if (ed_undo_is_init_poll(C) == false) {
    return false;
  }
  return ED_operator_screenactive(C);
}

static bool ed_undo_redo_poll(bContext *C)
{
  wmOperator *last_op = WM_operator_last_redo(C);
  return (last_op && ed_undo_is_init_and_screenactive_poll(C) &&
          WM_operator_check_ui_enabled(C, last_op->type->name));
}

static bool ed_undo_poll(bContext *C)
{
  if (!ed_undo_is_init_and_screenactive_poll(C)) {
    return false;
  }
  UndoStack *undo_stack = CTX_wm_manager(C)->runtime->undo_stack;
  return (undo_stack->step_active != nullptr) && (undo_stack->step_active->prev != nullptr);
}

void ED_OT_undo(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Undo";
  ot->description = "Undo previous action";
  ot->idname = "ED_OT_undo";

  /* API callbacks. */
  ot->exec = ed_undo_exec;
  ot->poll = ed_undo_poll;
}

void ED_OT_undo_push(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Undo Push";
  ot->description = "Add an undo state (internal use only)";
  ot->idname = "ED_OT_undo_push";

  /* API callbacks. */
  ot->exec = ed_undo_push_exec;
  /* Unlike others undo operators this initializes undo stack. */
  ot->poll = ED_operator_screenactive;

  ot->flag = OPTYPE_INTERNAL;

  RNA_def_string(ot->srna,
                 "message",
                 "Add an undo step *function may be moved*",
                 BKE_UNDO_STR_MAX,
                 "Undo Message",
                 "");
}

static bool ed_redo_poll(bContext *C)
{
  if (!ed_undo_is_init_and_screenactive_poll(C)) {
    return false;
  }
  UndoStack *undo_stack = CTX_wm_manager(C)->runtime->undo_stack;
  return (undo_stack->step_active != nullptr) && (undo_stack->step_active->next != nullptr);
}

void ED_OT_redo(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Redo";
  ot->description = "Redo previous action";
  ot->idname = "ED_OT_redo";

  /* API callbacks. */
  ot->exec = ed_redo_exec;
  ot->poll = ed_redo_poll;
}

void ED_OT_undo_redo(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Undo and Redo";
  ot->description = "Undo and redo previous action";
  ot->idname = "ED_OT_undo_redo";

  /* API callbacks. */
  ot->exec = ed_undo_redo_exec;
  ot->poll = ed_undo_redo_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Repeat
 * \{ */

bool ED_undo_operator_repeat(bContext *C, wmOperator *op)
{
  bool success = false;

  if (op) {
    CLOG_INFO(&LOG, "Operator repeat idname='%s'", op->type->idname);
    wmWindowManager *wm = CTX_wm_manager(C);
    const ScrArea *area = CTX_wm_area(C);
    Scene *scene = CTX_data_scene(C);

    /* keep in sync with logic in view3d_panel_operator_redo() */
    ARegion *region_orig = CTX_wm_region(C);
    /* If the redo is called from a HUD, this knows about the region type the operator was
     * initially called in, so attempt to restore that. */
    ARegion *redo_region_from_hud = (region_orig->regiontype == RGN_TYPE_HUD) ?
                                        ui::ED_area_type_hud_redo_region_find(area, region_orig) :
                                        nullptr;
    ARegion *region_repeat = redo_region_from_hud ? redo_region_from_hud :
                                                    BKE_area_find_region_active_win(area);

    if (region_repeat) {
      CTX_wm_region_set(C, region_repeat);
    }

    if (WM_operator_repeat_check(C, op) && WM_operator_poll(C, op->type) &&
        /* NOTE: undo/redo can't run if there are jobs active,
         * check for screen jobs only so jobs like material/texture/world preview
         * (which copy their data), won't stop redo, see #29579.
         *
         * NOTE: WM_operator_check_ui_enabled() jobs test _must_ stay in sync with this. */
        (WM_jobs_test(wm, scene, WM_JOB_TYPE_ANY) == 0))
    {
      if (G.debug & G_DEBUG) {
        printf("redo_cb: operator redo %s\n", op->type->name);
      }

      WM_operator_free_all_after(wm, op);

      ED_undo_pop_op(C, op);

      if (op->type->check) {
        if (op->type->check(C, op)) {
          /* check for popup and re-layout buttons */
          ARegion *region_popup = CTX_wm_region_popup(C);
          if (region_popup) {
            ED_region_tag_refresh_ui(region_popup);
          }
        }
      }

      const wmOperatorStatus retval = WM_operator_repeat(C, op);
      if ((retval & OPERATOR_FINISHED) == 0) {
        if (G.debug & G_DEBUG) {
          printf("redo_cb: operator redo failed: %s, return %d\n", op->type->name, retval);
        }
        ED_undo_redo(C);
      }
      else {
        success = true;
      }
    }
    else {
      if (G.debug & G_DEBUG) {
        printf("redo_cb: WM_operator_repeat_check returned false %s\n", op->type->name);
      }
    }

    /* set region back */
    CTX_wm_region_set(C, region_orig);
  }
  else {
    CLOG_WARN(&LOG, "called with nullptr 'op'");
  }

  return success;
}

void ED_undo_operator_repeat_cb(bContext *C, void *arg_op, void * /*arg_unused*/)
{
  ED_undo_operator_repeat(C, static_cast<wmOperator *>(arg_op));
}

void ED_undo_operator_repeat_cb_evt(bContext *C, void *arg_op, int /*arg_unused*/)
{
  ED_undo_operator_repeat(C, static_cast<wmOperator *>(arg_op));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Undo History Operator
 *
 * See `TOPBAR_MT_undo_history` which is used to access this operator.
 * \{ */

/* NOTE: also check #ed_undo_step() in top if you change notifiers. */
static wmOperatorStatus undo_history_exec(bContext *C, wmOperator *op)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "item");
  if (RNA_property_is_set(op->ptr, prop)) {
    const int item = RNA_property_int_get(op->ptr, prop);
    const int ret = ed_undo_step_by_index(C, item, op->reports);
    if (ret & OPERATOR_FINISHED) {
      ed_undo_refresh_for_op(C);

      WM_event_add_notifier(C, NC_WINDOW, nullptr);
      return OPERATOR_FINISHED;
    }
  }
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus undo_history_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "item");
  if (RNA_property_is_set(op->ptr, prop)) {
    return undo_history_exec(C, op);
  }

  WM_menu_name_call(C, "TOPBAR_MT_undo_history", wm::OpCallContext::InvokeDefault);
  return OPERATOR_FINISHED;
}

void ED_OT_undo_history(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Undo History";
  ot->description =
      "Undo or redo from the current active step until the specified action in history";
  ot->idname = "ED_OT_undo_history";

  /* API callbacks. */
  ot->invoke = undo_history_invoke;
  ot->exec = undo_history_exec;
  ot->poll = ed_undo_is_init_and_screenactive_poll;

  RNA_def_int(ot->srna, "item", 0, 0, INT_MAX, "Item", "", 0, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Undo Helper Functions
 * \{ */

void ED_undo_object_set_active_or_warn(const Main &bmain,
                                       Scene *scene,
                                       ViewLayer *view_layer,
                                       Object *ob,
                                       const char *info,
                                       CLG_LogRef *log)
{
  using namespace blender::ed;
  BKE_view_layer_synced_ensure(bmain, scene, view_layer);
  Object *ob_prev = BKE_view_layer_active_object_get(view_layer);
  if (ob_prev != ob) {
    Base *base = BKE_view_layer_base_find(view_layer, ob);
    if (base != nullptr) {
      view_layer->basact = base;
      object::base_active_refresh(G_MAIN, scene, view_layer);
    }
    else {
      /* Should never fail, may not crash but can give odd behavior. */
      CLOG_WARN(log, "'%s' failed to restore active object: '%s'", info, ob->id.name + 2);
    }
  }
}

void ED_undo_object_editmode_validate_scene_from_windows(wmWindowManager *wm,
                                                         const Scene *scene_ref,
                                                         Scene **scene_p,
                                                         ViewLayer **view_layer_p)
{
  if (*scene_p == scene_ref) {
    return;
  }
  for (wmWindow &win : wm->windows) {
    if (win.scene == scene_ref) {
      *scene_p = win.scene;
      *view_layer_p = WM_window_get_active_view_layer(&win);
      return;
    }
  }
}

void ED_undo_object_editmode_restore_helper(Scene *scene,
                                            ViewLayer *view_layer,
                                            Object **object_array,
                                            uint object_array_len,
                                            uint object_array_stride)
{
  using namespace blender::ed;
  Main *bmain = G_MAIN;
  /* Don't request unique data because we want to de-select objects when exiting edit-mode
   * for that to be done on all objects we can't skip ones that share data. */
  Vector<Base *> bases = ED_undo_editmode_bases_from_view_layer(*bmain, scene, view_layer);
  for (Base *base : bases) {
    (base->object->data)->tag |= ID_TAG_DOIT;
  }
  Object **ob_p = object_array;
  for (uint i = 0; i < object_array_len;
       i++, ob_p = static_cast<Object **>(POINTER_OFFSET(ob_p, object_array_stride)))
  {
    Object *obedit = *ob_p;
    object::editmode_enter_ex(bmain, scene, obedit, object::EM_NO_CONTEXT);
    (obedit->data)->tag &= ~ID_TAG_DOIT;
  }
  for (Base *base : bases) {
    const ID *id = base->object->data;
    if (id->tag & ID_TAG_DOIT) {
      object::editmode_exit_ex(bmain, scene, base->object, object::EM_FREEDATA);
      /* Ideally we would know the selection state it was before entering edit-mode,
       * for now follow the convention of having them unselected when exiting the mode. */
      object::base_select(base, object::BA_DESELECT);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Undo View Layer Helper Functions
 *
 * Needed because view layer functions such as
 * #BKE_view_layer_array_from_objects_in_edit_mode_unique_data also check visibility,
 * which is not reliable when it comes to object undo operations,
 * since hidden objects can be operated on in the properties editor,
 * and local collections may be used.
 * \{ */

Vector<Object *> ED_undo_editmode_objects_from_view_layer(const Main &bmain,
                                                          const Scene *scene,
                                                          ViewLayer *view_layer)
{
  BKE_view_layer_synced_ensure(bmain, scene, view_layer);
  Base *baseact = BKE_view_layer_active_base_get(view_layer);
  if ((baseact == nullptr) || (baseact->object->mode & OB_MODE_EDIT) == 0) {
    return {};
  }
  Set<const ID *> object_data;
  const short object_type = baseact->object->type;
  Vector<Object *> objects(object_data.size());
  /* Base iteration, starting with the active-base to ensure it's the first item in the array.
   * Looping over the active-base twice is OK as the tag check prevents it being handled twice. */
  for (Base *base = baseact,
            *base_next = static_cast<Base *>(BKE_view_layer_object_bases_get(view_layer)->first);
       base;
       base = base_next, base_next = base_next ? base_next->next : nullptr)
  {
    Object *ob = base->object;
    if ((ob->type == object_type) && (ob->mode & OB_MODE_EDIT)) {
      if (object_data.add(static_cast<const ID *>(ob->data))) {
        objects.append(ob);
      }
    }
  }
  BLI_assert(!object_data.is_empty());
  BLI_assert(objects[0] == baseact->object);
  return objects;
}

Vector<Base *> ED_undo_editmode_bases_from_view_layer(const Main &bmain,
                                                      const Scene *scene,
                                                      ViewLayer *view_layer)
{
  BKE_view_layer_synced_ensure(bmain, scene, view_layer);
  Base *baseact = BKE_view_layer_active_base_get(view_layer);
  if ((baseact == nullptr) || (baseact->object->mode & OB_MODE_EDIT) == 0) {
    return {};
  }
  Set<const ID *> object_data;
  const short object_type = baseact->object->type;
  Vector<Base *> bases;
  /* Base iteration, starting with the active-base to ensure it's the first item in the array.
   * Looping over the active-base twice is OK as the tag check prevents it being handled twice. */
  for (Base *base = BKE_view_layer_active_base_get(view_layer),
            *base_next = static_cast<Base *>(BKE_view_layer_object_bases_get(view_layer)->first);
       base;
       base = base_next, base_next = base_next ? base_next->next : nullptr)
  {
    Object *ob = base->object;
    if ((ob->type == object_type) && (ob->mode & OB_MODE_EDIT)) {
      if (object_data.add(static_cast<const ID *>(ob->data))) {
        bases.append(base);
      }
    }
  }

  BLI_assert(!object_data.is_empty());
  BLI_assert(bases[0] == baseact);
  return bases;
}

size_t ED_undosys_total_memory_calc(UndoStack *ustack)
{
  size_t total_memory = 0;

  for (UndoStep *us = static_cast<UndoStep *>(ustack->steps.first); us != nullptr; us = us->next) {
    if (us->type == BKE_UNDOSYS_TYPE_SCULPT) {
      total_memory += ed::sculpt_paint::undo::step_memory_size_get(us);
    }
    else if (us->data_size > 0) {
      total_memory += us->data_size;
    }
  }

  return total_memory;
}

/** \} */

}  // namespace blender
