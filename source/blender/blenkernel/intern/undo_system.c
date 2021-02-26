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
 */

/** \file
 * \ingroup bke
 *
 * Used by ED_undo.h, internal implementation.
 */

#include <string.h>

#include "CLG_log.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_listBase.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_lib_override.h"
#include "BKE_main.h"
#include "BKE_undo_system.h"

#include "MEM_guardedalloc.h"

#define undo_stack _wm_undo_stack_disallow /* pass in as a variable always. */

/** Odd requirement of Blender that we always keep a memfile undo in the stack. */
#define WITH_GLOBAL_UNDO_KEEP_ONE

/** Make sure all ID's created at the point we add an undo step that uses ID's. */
#define WITH_GLOBAL_UNDO_ENSURE_UPDATED

/**
 * Make sure we don't apply edits on top of a newer memfile state, see: T56163.
 * \note Keep an eye on this, could solve differently.
 */
#define WITH_GLOBAL_UNDO_CORRECT_ORDER

/** We only need this locally. */
static CLG_LogRef LOG = {"bke.undosys"};

/* -------------------------------------------------------------------- */
/** \name Internal Nested Undo Checks
 *
 * Make sure we're not running undo operations from 'step_encode', 'step_decode' callbacks.
 * bugs caused by this situation aren't _that_ hard to spot but aren't always so obvious.
 * Best we have a check which shows the problem immediately.
 *
 * \{ */
#define WITH_NESTED_UNDO_CHECK

#ifdef WITH_NESTED_UNDO_CHECK
static bool g_undo_callback_running = false;
#  define UNDO_NESTED_ASSERT(state) BLI_assert(g_undo_callback_running == state)
#  define UNDO_NESTED_CHECK_BEGIN \
    { \
      UNDO_NESTED_ASSERT(false); \
      g_undo_callback_running = true; \
    } \
    ((void)0)
#  define UNDO_NESTED_CHECK_END \
    { \
      UNDO_NESTED_ASSERT(true); \
      g_undo_callback_running = false; \
    } \
    ((void)0)
#else
#  define UNDO_NESTED_ASSERT(state) ((void)0)
#  define UNDO_NESTED_CHECK_BEGIN ((void)0)
#  define UNDO_NESTED_CHECK_END ((void)0)
#endif
/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Undo Types
 *
 * Unfortunately we need this for a handful of places.
 * \{ */
const UndoType *BKE_UNDOSYS_TYPE_IMAGE = NULL;
const UndoType *BKE_UNDOSYS_TYPE_MEMFILE = NULL;
const UndoType *BKE_UNDOSYS_TYPE_PAINTCURVE = NULL;
const UndoType *BKE_UNDOSYS_TYPE_PARTICLE = NULL;
const UndoType *BKE_UNDOSYS_TYPE_SCULPT = NULL;
const UndoType *BKE_UNDOSYS_TYPE_TEXT = NULL;
/** \} */

/* UndoType */

static ListBase g_undo_types = {NULL, NULL};

static const UndoType *BKE_undosys_type_from_context(bContext *C)
{
  LISTBASE_FOREACH (const UndoType *, ut, &g_undo_types) {
    /* No poll means we don't check context. */
    if (ut->poll && ut->poll(C)) {
      return ut;
    }
  }
  return NULL;
}

/* -------------------------------------------------------------------- */
/** \name Internal Callback Wrappers
 *
 * #UndoRefID is simply a way to avoid in-lining name copy and lookups,
 * since it's easy to forget a single case when done inline (crashing in some cases).
 *
 * \{ */

static void undosys_id_ref_store(void *UNUSED(user_data), UndoRefID *id_ref)
{
  BLI_assert(id_ref->name[0] == '\0');
  if (id_ref->ptr) {
    BLI_strncpy(id_ref->name, id_ref->ptr->name, sizeof(id_ref->name));
    /* Not needed, just prevents stale data access. */
    id_ref->ptr = NULL;
  }
}

static void undosys_id_ref_resolve(void *user_data, UndoRefID *id_ref)
{
  /* Note: we could optimize this,
   * for now it's not too bad since it only runs when we access undo! */
  Main *bmain = user_data;
  ListBase *lb = which_libbase(bmain, GS(id_ref->name));
  LISTBASE_FOREACH (ID *, id, lb) {
    if (STREQ(id_ref->name, id->name) && (id->lib == NULL)) {
      id_ref->ptr = id;
      break;
    }
  }
}

static bool undosys_step_encode(bContext *C, Main *bmain, UndoStack *ustack, UndoStep *us)
{
  CLOG_INFO(&LOG, 2, "addr=%p, name='%s', type='%s'", us, us->name, us->type->name);
  UNDO_NESTED_CHECK_BEGIN;
  bool ok = us->type->step_encode(C, bmain, us);
  UNDO_NESTED_CHECK_END;
  if (ok) {
    if (us->type->step_foreach_ID_ref != NULL) {
      /* Don't use from context yet because sometimes context is fake and
       * not all members are filled in. */
      us->type->step_foreach_ID_ref(us, undosys_id_ref_store, bmain);
    }

#ifdef WITH_GLOBAL_UNDO_CORRECT_ORDER
    if (us->type == BKE_UNDOSYS_TYPE_MEMFILE) {
      ustack->step_active_memfile = us;
    }
#endif
  }
  if (ok == false) {
    CLOG_INFO(&LOG, 2, "encode callback didn't create undo step");
  }
  return ok;
}

static void undosys_step_decode(bContext *C,
                                Main *bmain,
                                UndoStack *ustack,
                                UndoStep *us,
                                const eUndoStepDir dir,
                                bool is_final)
{
  CLOG_INFO(&LOG, 2, "addr=%p, name='%s', type='%s'", us, us->name, us->type->name);

  if (us->type->step_foreach_ID_ref) {
#ifdef WITH_GLOBAL_UNDO_CORRECT_ORDER
    if (us->type != BKE_UNDOSYS_TYPE_MEMFILE) {
      for (UndoStep *us_iter = us->prev; us_iter; us_iter = us_iter->prev) {
        if (us_iter->type == BKE_UNDOSYS_TYPE_MEMFILE) {
          if (us_iter == ustack->step_active_memfile) {
            /* Common case, we're already using the last memfile state. */
          }
          else {
            /* Load the previous memfile state so any ID's referenced in this
             * undo step will be correctly resolved, see: T56163. */
            undosys_step_decode(C, bmain, ustack, us_iter, dir, false);
            /* May have been freed on memfile read. */
            bmain = G_MAIN;
          }
          break;
        }
      }
    }
#endif
    /* Don't use from context yet because sometimes context is fake and
     * not all members are filled in. */
    us->type->step_foreach_ID_ref(us, undosys_id_ref_resolve, bmain);
  }

  UNDO_NESTED_CHECK_BEGIN;
  us->type->step_decode(C, bmain, us, dir, is_final);
  UNDO_NESTED_CHECK_END;

#ifdef WITH_GLOBAL_UNDO_CORRECT_ORDER
  if (us->type == BKE_UNDOSYS_TYPE_MEMFILE) {
    ustack->step_active_memfile = us;
  }
#endif
}

static void undosys_step_free_and_unlink(UndoStack *ustack, UndoStep *us)
{
  CLOG_INFO(&LOG, 2, "addr=%p, name='%s', type='%s'", us, us->name, us->type->name);
  UNDO_NESTED_CHECK_BEGIN;
  us->type->step_free(us);
  UNDO_NESTED_CHECK_END;

  BLI_remlink(&ustack->steps, us);
  MEM_freeN(us);

#ifdef WITH_GLOBAL_UNDO_CORRECT_ORDER
  if (ustack->step_active_memfile == us) {
    ustack->step_active_memfile = NULL;
  }
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Undo Stack
 * \{ */

#ifndef NDEBUG
static void undosys_stack_validate(UndoStack *ustack, bool expect_non_empty)
{
  if (ustack->step_active != NULL) {
    BLI_assert(!BLI_listbase_is_empty(&ustack->steps));
    BLI_assert(BLI_findindex(&ustack->steps, ustack->step_active) != -1);
  }
  if (expect_non_empty) {
    BLI_assert(!BLI_listbase_is_empty(&ustack->steps));
  }
}
#else
static void undosys_stack_validate(UndoStack *UNUSED(ustack), bool UNUSED(expect_non_empty))
{
}
#endif

UndoStack *BKE_undosys_stack_create(void)
{
  UndoStack *ustack = MEM_callocN(sizeof(UndoStack), __func__);
  return ustack;
}

void BKE_undosys_stack_destroy(UndoStack *ustack)
{
  BKE_undosys_stack_clear(ustack);
  MEM_freeN(ustack);
}

void BKE_undosys_stack_clear(UndoStack *ustack)
{
  UNDO_NESTED_ASSERT(false);
  CLOG_INFO(&LOG, 1, "steps=%d", BLI_listbase_count(&ustack->steps));
  for (UndoStep *us = ustack->steps.last, *us_prev; us; us = us_prev) {
    us_prev = us->prev;
    undosys_step_free_and_unlink(ustack, us);
  }
  BLI_listbase_clear(&ustack->steps);
  ustack->step_active = NULL;
}

void BKE_undosys_stack_clear_active(UndoStack *ustack)
{
  /* Remove active and all following undo-steps. */
  UndoStep *us = ustack->step_active;

  if (us) {
    ustack->step_active = us->prev;
    bool is_not_empty = ustack->step_active != NULL;

    while (ustack->steps.last != ustack->step_active) {
      UndoStep *us_iter = ustack->steps.last;
      undosys_step_free_and_unlink(ustack, us_iter);
      undosys_stack_validate(ustack, is_not_empty);
    }
  }
}

/* Caller is responsible for handling active. */
static void undosys_stack_clear_all_last(UndoStack *ustack, UndoStep *us)
{
  if (us) {
    bool is_not_empty = true;
    UndoStep *us_iter;
    do {
      us_iter = ustack->steps.last;
      BLI_assert(us_iter != ustack->step_active);
      undosys_step_free_and_unlink(ustack, us_iter);
      undosys_stack_validate(ustack, is_not_empty);
    } while ((us != us_iter));
  }
}

static void undosys_stack_clear_all_first(UndoStack *ustack, UndoStep *us, UndoStep *us_exclude)
{
  if (us && us == us_exclude) {
    us = us->prev;
  }

  if (us) {
    bool is_not_empty = true;
    UndoStep *us_iter;
    do {
      us_iter = ustack->steps.first;
      if (us_iter == us_exclude) {
        us_iter = us_iter->next;
      }
      BLI_assert(us_iter != ustack->step_active);
      undosys_step_free_and_unlink(ustack, us_iter);
      undosys_stack_validate(ustack, is_not_empty);
    } while ((us != us_iter));
  }
}

static bool undosys_stack_push_main(UndoStack *ustack, const char *name, struct Main *bmain)
{
  UNDO_NESTED_ASSERT(false);
  BLI_assert(ustack->step_init == NULL);
  CLOG_INFO(&LOG, 1, "'%s'", name);
  bContext *C_temp = CTX_create();
  CTX_data_main_set(C_temp, bmain);
  UndoPushReturn ret = BKE_undosys_step_push_with_type(
      ustack, C_temp, name, BKE_UNDOSYS_TYPE_MEMFILE);
  CTX_free(C_temp);
  return (ret & UNDO_PUSH_RET_SUCCESS);
}

void BKE_undosys_stack_init_from_main(UndoStack *ustack, struct Main *bmain)
{
  UNDO_NESTED_ASSERT(false);
  undosys_stack_push_main(ustack, IFACE_("Original"), bmain);
}

/* called after 'BKE_undosys_stack_init_from_main' */
void BKE_undosys_stack_init_from_context(UndoStack *ustack, bContext *C)
{
  const UndoType *ut = BKE_undosys_type_from_context(C);
  if (!ELEM(ut, NULL, BKE_UNDOSYS_TYPE_MEMFILE)) {
    BKE_undosys_step_push_with_type(ustack, C, IFACE_("Original Mode"), ut);
  }
}

/* name optional */
bool BKE_undosys_stack_has_undo(UndoStack *ustack, const char *name)
{
  if (name) {
    UndoStep *us = BLI_rfindstring(&ustack->steps, name, offsetof(UndoStep, name));
    return us && us->prev;
  }

  return !BLI_listbase_is_empty(&ustack->steps);
}

UndoStep *BKE_undosys_stack_active_with_type(UndoStack *ustack, const UndoType *ut)
{
  UndoStep *us = ustack->step_active;
  while (us && (us->type != ut)) {
    us = us->prev;
  }
  return us;
}

UndoStep *BKE_undosys_stack_init_or_active_with_type(UndoStack *ustack, const UndoType *ut)
{
  UNDO_NESTED_ASSERT(false);
  CLOG_INFO(&LOG, 1, "type='%s'", ut->name);
  if (ustack->step_init && (ustack->step_init->type == ut)) {
    return ustack->step_init;
  }
  return BKE_undosys_stack_active_with_type(ustack, ut);
}

/**
 * \param steps: Limit the number of undo steps.
 * \param memory_limit: Limit the amount of memory used by the undo stack.
 */
void BKE_undosys_stack_limit_steps_and_memory(UndoStack *ustack, int steps, size_t memory_limit)
{
  UNDO_NESTED_ASSERT(false);
  if ((steps == -1) && (memory_limit != 0)) {
    return;
  }

  CLOG_INFO(&LOG, 1, "steps=%d, memory_limit=%zu", steps, memory_limit);
  UndoStep *us;
  UndoStep *us_exclude = NULL;
  /* keep at least two (original + other) */
  size_t data_size_all = 0;
  size_t us_count = 0;
  for (us = ustack->steps.last; us && us->prev; us = us->prev) {
    if (memory_limit) {
      data_size_all += us->data_size;
      if (data_size_all > memory_limit) {
        break;
      }
    }
    if (steps != -1) {
      if (us_count == steps) {
        break;
      }
      if (us->skip == false) {
        us_count += 1;
      }
    }
  }

  if (us) {
#ifdef WITH_GLOBAL_UNDO_KEEP_ONE
    /* Hack, we need to keep at least one BKE_UNDOSYS_TYPE_MEMFILE. */
    if (us->type != BKE_UNDOSYS_TYPE_MEMFILE) {
      us_exclude = us->prev;
      while (us_exclude && us_exclude->type != BKE_UNDOSYS_TYPE_MEMFILE) {
        us_exclude = us_exclude->prev;
      }
      /* Once this is outside the given number of 'steps', undoing onto this state
       * may skip past many undo steps which is confusing, instead,
       * disallow stepping onto this state entirely. */
      if (us_exclude) {
        us_exclude->skip = true;
      }
    }
#endif
    /* Free from first to last, free functions may update de-duplication info
     * (see #MemFileUndoStep). */
    undosys_stack_clear_all_first(ustack, us->prev, us_exclude);
  }
}

/** \} */

UndoStep *BKE_undosys_step_push_init_with_type(UndoStack *ustack,
                                               bContext *C,
                                               const char *name,
                                               const UndoType *ut)
{
  UNDO_NESTED_ASSERT(false);
  /* We could detect and clean this up (but it should never happen!). */
  BLI_assert(ustack->step_init == NULL);
  if (ut->step_encode_init) {
    undosys_stack_validate(ustack, false);

    if (ustack->step_active) {
      undosys_stack_clear_all_last(ustack, ustack->step_active->next);
    }

    UndoStep *us = MEM_callocN(ut->step_size, __func__);
    if (name != NULL) {
      BLI_strncpy(us->name, name, sizeof(us->name));
    }
    us->type = ut;
    ustack->step_init = us;
    CLOG_INFO(&LOG, 1, "addr=%p, name='%s', type='%s'", us, us->name, us->type->name);
    ut->step_encode_init(C, us);
    undosys_stack_validate(ustack, false);
    return us;
  }

  return NULL;
}

UndoStep *BKE_undosys_step_push_init(UndoStack *ustack, bContext *C, const char *name)
{
  UNDO_NESTED_ASSERT(false);
  /* We could detect and clean this up (but it should never happen!). */
  BLI_assert(ustack->step_init == NULL);
  const UndoType *ut = BKE_undosys_type_from_context(C);
  if (ut == NULL) {
    return NULL;
  }
  return BKE_undosys_step_push_init_with_type(ustack, C, name, ut);
}

/**
 * \param C: Can be NULL from some callers if their encoding function doesn't need it
 */
UndoPushReturn BKE_undosys_step_push_with_type(UndoStack *ustack,
                                               bContext *C,
                                               const char *name,
                                               const UndoType *ut)
{
  BLI_assert((ut->flags & UNDOTYPE_FLAG_NEED_CONTEXT_FOR_ENCODE) == 0 || C != NULL);

  UNDO_NESTED_ASSERT(false);
  undosys_stack_validate(ustack, false);
  bool is_not_empty = ustack->step_active != NULL;
  UndoPushReturn retval = UNDO_PUSH_RET_FAILURE;

  /* Might not be final place for this to be called - probably only want to call it from some
   * undo handlers, not all of them? */
  if (BKE_lib_override_library_main_operations_create(G_MAIN, false)) {
    retval |= UNDO_PUSH_RET_OVERRIDE_CHANGED;
  }

  /* Remove all undo-steps after (also when 'ustack->step_active == NULL'). */
  while (ustack->steps.last != ustack->step_active) {
    UndoStep *us_iter = ustack->steps.last;
    undosys_step_free_and_unlink(ustack, us_iter);
    undosys_stack_validate(ustack, is_not_empty);
  }

  if (ustack->step_active) {
    BLI_assert(BLI_findindex(&ustack->steps, ustack->step_active) != -1);
  }

#ifdef WITH_GLOBAL_UNDO_ENSURE_UPDATED
  if (ut->step_foreach_ID_ref != NULL) {
    if (G_MAIN->is_memfile_undo_written == false) {
      const char *name_internal = "MemFile Internal (pre)";
      /* Don't let 'step_init' cause issues when adding memfile undo step. */
      void *step_init = ustack->step_init;
      ustack->step_init = NULL;
      const bool ok = undosys_stack_push_main(ustack, name_internal, G_MAIN);
      /* Restore 'step_init'. */
      ustack->step_init = step_init;
      if (ok) {
        UndoStep *us = ustack->steps.last;
        BLI_assert(STREQ(us->name, name_internal));
        us->skip = true;
#  ifdef WITH_GLOBAL_UNDO_CORRECT_ORDER
        ustack->step_active_memfile = us;
#  endif
      }
    }
  }
#endif

  bool use_memfile_step = false;
  {
    UndoStep *us = ustack->step_init ? ustack->step_init : MEM_callocN(ut->step_size, __func__);
    ustack->step_init = NULL;
    if (us->name[0] == '\0') {
      BLI_strncpy(us->name, name, sizeof(us->name));
    }
    us->type = ut;
    /* True by default, code needs to explicitly set it to false if necessary. */
    us->use_old_bmain_data = true;
    /* Initialized, not added yet. */

    CLOG_INFO(&LOG, 1, "addr=%p, name='%s', type='%s'", us, us->name, us->type->name);

    if (!undosys_step_encode(C, G_MAIN, ustack, us)) {
      MEM_freeN(us);
      undosys_stack_validate(ustack, true);
      return retval;
    }
    ustack->step_active = us;
    BLI_addtail(&ustack->steps, us);
    use_memfile_step = us->use_memfile_step;
  }

  if (use_memfile_step) {
    /* Make this the user visible undo state, so redo always applies
     * on top of the mem-file undo instead of skipping it. see: T67256. */
    UndoStep *us_prev = ustack->step_active;
    const char *name_internal = us_prev->name;
    const bool ok = undosys_stack_push_main(ustack, name_internal, G_MAIN);
    if (ok) {
      UndoStep *us = ustack->steps.last;
      BLI_assert(STREQ(us->name, name_internal));
      us_prev->skip = true;
#ifdef WITH_GLOBAL_UNDO_CORRECT_ORDER
      ustack->step_active_memfile = us;
#endif
      ustack->step_active = us;
    }
  }

  if (ustack->group_level > 0) {
    /* Temporarily set skip for the active step.
     * This is an invalid state which must be corrected once the last group ends. */
    ustack->step_active->skip = true;
  }

  undosys_stack_validate(ustack, true);
  return (retval | UNDO_PUSH_RET_SUCCESS);
}

UndoPushReturn BKE_undosys_step_push(UndoStack *ustack, bContext *C, const char *name)
{
  UNDO_NESTED_ASSERT(false);
  const UndoType *ut = ustack->step_init ? ustack->step_init->type :
                                           BKE_undosys_type_from_context(C);
  if (ut == NULL) {
    return false;
  }
  return BKE_undosys_step_push_with_type(ustack, C, name, ut);
}

/**
 * Useful when we want to diff against previous undo data but can't be sure the types match.
 */
UndoStep *BKE_undosys_step_same_type_next(UndoStep *us)
{
  if (us) {
    const UndoType *ut = us->type;
    while ((us = us->next)) {
      if (us->type == ut) {
        return us;
      }
    }
  }
  return us;
}

/**
 * Useful when we want to diff against previous undo data but can't be sure the types match.
 */
UndoStep *BKE_undosys_step_same_type_prev(UndoStep *us)
{
  if (us) {
    const UndoType *ut = us->type;
    while ((us = us->prev)) {
      if (us->type == ut) {
        return us;
      }
    }
  }
  return us;
}

UndoStep *BKE_undosys_step_find_by_name_with_type(UndoStack *ustack,
                                                  const char *name,
                                                  const UndoType *ut)
{
  for (UndoStep *us = ustack->steps.last; us; us = us->prev) {
    if (us->type == ut) {
      if (STREQ(name, us->name)) {
        return us;
      }
    }
  }
  return NULL;
}

UndoStep *BKE_undosys_step_find_by_name(UndoStack *ustack, const char *name)
{
  return BLI_rfindstring(&ustack->steps, name, offsetof(UndoStep, name));
}

UndoStep *BKE_undosys_step_find_by_type(UndoStack *ustack, const UndoType *ut)
{
  for (UndoStep *us = ustack->steps.last; us; us = us->prev) {
    if (us->type == ut) {
      return us;
    }
  }
  return NULL;
}

/**
 * Return direction of the undo/redo from `us_reference` (or `ustack->step_active` if NULL), and
 * `us_target`.
 *
 * \note If `us_reference` and `us_target` are the same, we consider this is an undo.
 *
 * \return -1 for undo, 1 for redo, 0 in case of error.
 */
eUndoStepDir BKE_undosys_step_calc_direction(const UndoStack *ustack,
                                             const UndoStep *us_target,
                                             const UndoStep *us_reference)
{
  if (us_reference == NULL) {
    us_reference = ustack->step_active;
  }

  BLI_assert(us_reference != NULL);

  /* Note that we use heuristics to make this lookup as fast as possible in most common cases,
   * assuming that:
   *  - Most cases are just undo or redo of one step from active one.
   *  - Otherwise, it is typically faster to check future steps since active one is usually close
   *    to the end of the list, rather than its start. */
  /* NOTE: in case target step is the active one, we assume we are in an undo case... */
  if (ELEM(us_target, us_reference, us_reference->prev)) {
    return STEP_UNDO;
  }
  if (us_target == us_reference->next) {
    return STEP_REDO;
  }

  /* Search forward, and then backward. */
  for (UndoStep *us_iter = us_reference->next; us_iter != NULL; us_iter = us_iter->next) {
    if (us_iter == us_target) {
      return STEP_REDO;
    }
  }
  for (UndoStep *us_iter = us_reference->prev; us_iter != NULL; us_iter = us_iter->prev) {
    if (us_iter == us_target) {
      return STEP_UNDO;
    }
  }

  BLI_assert(!"Target undo step not found, this should not happen and may indicate an undo stack corruption");
  return STEP_INVALID;
}

/**
 * Undo/Redo until the given `us_target` step becomes the active (currently loaded) one.
 *
 * \note Unless `us_target` is a 'skipped' one and `use_skip` is true, `us_target` will become the
 *       active step.
 *
 * \note In case `use_skip` is true, the final target will always be **beyond** the given one (if
 *       the given one has to be skipped).
 *
 * \param us_reference If NULL, will be set to current active step in the undo stack. Otherwise, it
 *                     is assumed to match the current state, and will be used as basis for the
 *                     undo/redo process (i.e. all steps in-between `us_reference` and `us_target`
 *                     will be processed).
 */
bool BKE_undosys_step_load_data_ex(UndoStack *ustack,
                                   bContext *C,
                                   UndoStep *us_target,
                                   UndoStep *us_reference,
                                   const bool use_skip)
{
  UNDO_NESTED_ASSERT(false);
  if (us_target == NULL) {
    CLOG_ERROR(&LOG, "called with a NULL target step");
    return false;
  }
  undosys_stack_validate(ustack, true);

  if (us_reference == NULL) {
    us_reference = ustack->step_active;
  }
  if (us_reference == NULL) {
    CLOG_ERROR(&LOG, "could not find a valid initial active target step as reference");
    return false;
  }

  /* This considers we are in undo case if both `us_target` and `us_reference` are the same. */
  const eUndoStepDir undo_dir = BKE_undosys_step_calc_direction(ustack, us_target, us_reference);
  BLI_assert(undo_dir != STEP_INVALID);

  /* This will be the active step once the undo process is complete.
   *
   * In case we do skip 'skipped' steps, the final active step may be several steps backward from
   * the one passed as parameter. */
  UndoStep *us_target_active = us_target;
  if (use_skip) {
    while (us_target_active != NULL && us_target_active->skip) {
      us_target_active = (undo_dir == -1) ? us_target_active->prev : us_target_active->next;
    }
    if (us_target_active == NULL) {
      CLOG_INFO(&LOG,
                2,
                "undo/redo did not find a step after stepping over skip-steps "
                "(undo limit exceeded)");
      return false;
    }
  }

  CLOG_INFO(&LOG,
            1,
            "addr=%p, name='%s', type='%s', undo_dir=%d",
            us_target,
            us_target->name,
            us_target->type->name,
            undo_dir);

  /* Undo/Redo steps until we reach given target step (or beyond if it has to be skipped), from
   * given reference step.
   *
   * NOTE: Unlike with redo case, where we can expect current active step to fully reflect current
   * data status, in undo case we also do reload the active step.
   * FIXME: this feels weak, and should probably not be actually needed? Or should also be done in
   * redo case? */
  bool is_processing_extra_skipped_steps = false;
  for (UndoStep *us_iter = (undo_dir == -1) ? us_reference : us_reference->next; us_iter != NULL;
       us_iter = (undo_dir == -1) ? us_iter->prev : us_iter->next) {
    BLI_assert(us_iter != NULL);

    const bool is_final = (us_iter == us_target_active);

    if (!is_final && is_processing_extra_skipped_steps) {
      BLI_assert(us_iter->skip == true);
      CLOG_INFO(&LOG,
                2,
                "undo/redo continue with skip addr=%p, name='%s', type='%s'",
                us_iter,
                us_iter->name,
                us_iter->type->name);
    }

    undosys_step_decode(C, G_MAIN, ustack, us_iter, undo_dir, is_final);
    ustack->step_active = us_iter;

    if (us_iter == us_target) {
      is_processing_extra_skipped_steps = true;
    }

    if (is_final) {
      /* Undo/Redo process is finished and successful. */
      return true;
    }
  }

  BLI_assert(
      !"This should never be reached, either undo stack is corrupted, or code above is buggy");
  return false;
}

/**
 * Undo/Redo until the given `us_target` step becomes the active (currently loaded) one.
 */
bool BKE_undosys_step_load_data(UndoStack *ustack, bContext *C, UndoStep *us_target)
{
  /* Note that here we do not skip 'skipped' steps by default. */
  return BKE_undosys_step_load_data_ex(ustack, C, us_target, NULL, false);
}

/**
 * Undo/Redo until the step matching given `index` in the undo stack becomes the active (currently
 * loaded) one.
 */
void BKE_undosys_step_load_from_index(UndoStack *ustack, bContext *C, const int index)
{
  UndoStep *us_target = BLI_findlink(&ustack->steps, index);
  BLI_assert(us_target->skip == false);
  BKE_undosys_step_load_data(ustack, C, us_target);
}

/**
 * Undo until `us_target` step becomes the active (currently loaded) one.
 *
 * \warning This function assumes that the given target step is _before_ current active one.
 *
 * \note Unless `us_target` is a 'skipped' one and `use_skip` is true, `us_target` will become the
 *       active step.
 *
 * \note In case `use_skip` is true, the final target will always be **before** the given one (if
 *       the given one has to be skipped).
 */
bool BKE_undosys_step_undo_with_data_ex(UndoStack *ustack,
                                        bContext *C,
                                        UndoStep *us_target,
                                        bool use_skip)
{
  /* In case there is no active step, we consider we just load given step, so reference must be
   * itself (due to weird 'load current active step in undo case' thing, see comments in
   * #BKE_undosys_step_load_data_ex). */
  UndoStep *us_reference = ustack->step_active != NULL ? ustack->step_active : us_target;

  BLI_assert(BKE_undosys_step_calc_direction(ustack, us_target, us_reference) == -1);

  return BKE_undosys_step_load_data_ex(ustack, C, us_target, us_reference, use_skip);
}

/**
 * Undo until `us_target` step becomes the active (currently loaded) one.
 *
 * \note See #BKE_undosys_step_undo_with_data_ex for details.
 */
bool BKE_undosys_step_undo_with_data(UndoStack *ustack, bContext *C, UndoStep *us_target)
{
  return BKE_undosys_step_undo_with_data_ex(ustack, C, us_target, true);
}

/**
 * Undo one step from current active (currently loaded) one.
 */
bool BKE_undosys_step_undo(UndoStack *ustack, bContext *C)
{
  if (ustack->step_active != NULL) {
    return BKE_undosys_step_undo_with_data(ustack, C, ustack->step_active->prev);
  }
  return false;
}

/**
 * Redo until `us_target` step becomes the active (currently loaded) one.
 *
 * \warning This function assumes that the given target step is _after_ current active one.
 *
 * \note Unless `us_target` is a 'skipped' one and `use_skip` is true, `us_target` will become the
 *       active step.
 *
 * \note In case `use_skip` is true, the final target will always be **after** the given one (if
 *       the given one has to be skipped).
 */
bool BKE_undosys_step_redo_with_data_ex(UndoStack *ustack,
                                        bContext *C,
                                        UndoStep *us_target,
                                        bool use_skip)
{
  /* In case there is no active step, we consider we just load given step, so reference must be
   * the previous one. */
  UndoStep *us_reference = ustack->step_active != NULL ? ustack->step_active : us_target->prev;

  BLI_assert(BKE_undosys_step_calc_direction(ustack, us_target, us_reference) == 1);

  return BKE_undosys_step_load_data_ex(ustack, C, us_target, us_reference, use_skip);
}

/**
 * Redo until `us_target` step becomes the active (currently loaded) one.
 *
 * \note See #BKE_undosys_step_redo_with_data_ex for details.
 */
bool BKE_undosys_step_redo_with_data(UndoStack *ustack, bContext *C, UndoStep *us_target)
{
  return BKE_undosys_step_redo_with_data_ex(ustack, C, us_target, true);
}

/**
 * Redo one step from current active one.
 */
bool BKE_undosys_step_redo(UndoStack *ustack, bContext *C)
{
  if (ustack->step_active != NULL) {
    return BKE_undosys_step_redo_with_data(ustack, C, ustack->step_active->next);
  }
  return false;
}

/**
 * Similar to #WM_operatortype_append
 */
UndoType *BKE_undosys_type_append(void (*undosys_fn)(UndoType *))
{
  UndoType *ut;

  ut = MEM_callocN(sizeof(UndoType), __func__);

  undosys_fn(ut);

  BLI_addtail(&g_undo_types, ut);

  return ut;
}

void BKE_undosys_type_free_all(void)
{
  UndoType *ut;
  while ((ut = BLI_pophead(&g_undo_types))) {
    MEM_freeN(ut);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Undo Stack Grouping
 *
 * This enables skip while group-level is set.
 * In general it's not allowed that #UndoStack.step_active have 'skip' enabled.
 *
 * This rule is relaxed for grouping, however it's important each call to
 * #BKE_undosys_stack_group_begin has a matching #BKE_undosys_stack_group_end.
 *
 * - Levels are used so nesting is supported, where the last call to #BKE_undosys_stack_group_end
 *   will set the active undo step that should not be skipped.
 *
 * - Correct begin/end is checked by an assert since any errors here will cause undo
 *   to consider all steps part of one large group.
 *
 * - Calls to begin/end with no undo steps being pushed is supported and does nothing.
 *
 * \{ */

void BKE_undosys_stack_group_begin(UndoStack *ustack)
{
  BLI_assert(ustack->group_level >= 0);
  ustack->group_level += 1;
}

void BKE_undosys_stack_group_end(UndoStack *ustack)
{
  ustack->group_level -= 1;
  BLI_assert(ustack->group_level >= 0);

  if (ustack->group_level == 0) {
    if (LIKELY(ustack->step_active != NULL)) {
      ustack->step_active->skip = false;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID Reference Utilities
 *
 * Unfortunately we need this for a handful of places.
 * \{ */

static void UNUSED_FUNCTION(BKE_undosys_foreach_ID_ref(UndoStack *ustack,
                                                       UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                                       void *user_data))
{
  LISTBASE_FOREACH (UndoStep *, us, &ustack->steps) {
    const UndoType *ut = us->type;
    if (ut->step_foreach_ID_ref != NULL) {
      ut->step_foreach_ID_ref(us, foreach_ID_ref_fn, user_data);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug Helpers
 * \{ */

void BKE_undosys_print(UndoStack *ustack)
{
  printf("Undo %d Steps (*: active, #=applied, M=memfile-active, S=skip)\n",
         BLI_listbase_count(&ustack->steps));
  int index = 0;
  LISTBASE_FOREACH (UndoStep *, us, &ustack->steps) {
    printf("[%c%c%c%c] %3d {%p} type='%s', name='%s'\n",
           (us == ustack->step_active) ? '*' : ' ',
           us->is_applied ? '#' : ' ',
           (us == ustack->step_active_memfile) ? 'M' : ' ',
           us->skip ? 'S' : ' ',
           index,
           (void *)us,
           us->type->name,
           us->name);
    index++;
  }
}

/** \} */
