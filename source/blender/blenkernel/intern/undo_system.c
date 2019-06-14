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

#include "BLI_utildefines.h"
#include "BLI_sys_types.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "DNA_listBase.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library_override.h"
#include "BKE_main.h"
#include "BKE_undo_system.h"

#include "MEM_guardedalloc.h"

#define undo_stack _wm_undo_stack_disallow /* pass in as a variable always. */

/** Odd requirement of Blender that we always keep a memfile undo in the stack. */
#define WITH_GLOBAL_UNDO_KEEP_ONE

/** Make sure all ID's created at the point we add an undo step that uses ID's. */
#define WITH_GLOBAL_UNDO_ENSURE_UPDATED

/** Make sure we don't apply edits ontop of a newer memfile state, see: T56163.
 * \note Keep an eye on this, could solve differently. */
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
 */
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
  for (const UndoType *ut = g_undo_types.first; ut; ut = ut->next) {
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
 * #UndoRefID is simply a way to avoid inlining name copy and lookups,
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
  for (ID *id = lb->first; id; id = id->next) {
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

static void undosys_step_decode(bContext *C, Main *bmain, UndoStack *ustack, UndoStep *us, int dir)
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
            undosys_step_decode(C, bmain, ustack, us_iter, dir);
            /* May have been freed on memfile read. */
            bmain = G.main;
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
  us->type->step_decode(C, bmain, us, dir);
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
static void undosys_stack_validate(UndoStack *ustack, bool expect_non_empty)
{
  UNUSED_VARS(ustack, expect_non_empty);
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
  /* Remove active and all following undos. */
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

static void undosys_stack_clear_all_first(UndoStack *ustack, UndoStep *us)
{
  if (us) {
    bool is_not_empty = true;
    UndoStep *us_iter;
    do {
      us_iter = ustack->steps.first;
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
  bool ok = BKE_undosys_step_push_with_type(ustack, C_temp, name, BKE_UNDOSYS_TYPE_MEMFILE);
  CTX_free(C_temp);
  return ok;
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
  if ((ut != NULL) && (ut != BKE_UNDOSYS_TYPE_MEMFILE)) {
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
  if (!(steps || memory_limit)) {
    return;
  }

  CLOG_INFO(&LOG, 1, "steps=%d, memory_limit=%zu", steps, memory_limit);
  UndoStep *us;
#ifdef WITH_GLOBAL_UNDO_KEEP_ONE
  UndoStep *us_exclude = NULL;
#endif
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
    if (steps) {
      if (us_count == steps) {
        break;
      }
      if (us->skip == false) {
        us_count += 1;
      }
    }
  }

  if (us) {
    if (us->prev && us->prev->prev) {
      us = us->prev;
    }

#ifdef WITH_GLOBAL_UNDO_KEEP_ONE
    /* Hack, we need to keep at least one BKE_UNDOSYS_TYPE_MEMFILE. */
    if (us->type != BKE_UNDOSYS_TYPE_MEMFILE) {
      us_exclude = us->prev;
      while (us_exclude && us->type != BKE_UNDOSYS_TYPE_MEMFILE) {
        us_exclude = us_exclude->prev;
      }
      if (us_exclude) {
        BLI_remlink(&ustack->steps, us_exclude);
      }
    }
#endif
    /* Free from first to last, free functions may update de-duplication info
     * (see #MemFileUndoStep). */
    undosys_stack_clear_all_first(ustack, us->prev);

#ifdef WITH_GLOBAL_UNDO_KEEP_ONE
    if (us_exclude) {
      BLI_addhead(&ustack->steps, us_exclude);
    }
#endif
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
    CLOG_INFO(&LOG, 1, "addr=%p, name='%s', type='%s'", us, name, ut->name);
    if (name != NULL) {
      BLI_strncpy(us->name, name, sizeof(us->name));
    }
    us->type = ut;
    ustack->step_init = us;
    ut->step_encode_init(C, us);
    undosys_stack_validate(ustack, false);
    return us;
  }
  else {
    return NULL;
  }
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
bool BKE_undosys_step_push_with_type(UndoStack *ustack,
                                     bContext *C,
                                     const char *name,
                                     const UndoType *ut)
{
  UNDO_NESTED_ASSERT(false);
  undosys_stack_validate(ustack, false);
  bool is_not_empty = ustack->step_active != NULL;

  /* Might not be final place for this to be called - probably only want to call it from some
   * undo handlers, not all of them? */
  if (BKE_override_library_is_enabled()) {
    BKE_main_override_library_operations_create(G.main, false);
  }

  /* Remove all undos after (also when 'ustack->step_active == NULL'). */
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
    /* initialized, not added yet. */

    if (!undosys_step_encode(C, G_MAIN, ustack, us)) {
      MEM_freeN(us);
      undosys_stack_validate(ustack, true);
      return false;
    }
    ustack->step_active = us;
    BLI_addtail(&ustack->steps, us);
    use_memfile_step = us->use_memfile_step;
  }

  if (use_memfile_step) {
    const char *name_internal = "MemFile Internal (post)";
    const bool ok = undosys_stack_push_main(ustack, name_internal, G_MAIN);
    if (ok) {
      UndoStep *us = ustack->steps.last;
      BLI_assert(STREQ(us->name, name_internal));
      us->skip = true;
#ifdef WITH_GLOBAL_UNDO_CORRECT_ORDER
      ustack->step_active_memfile = us;
#endif
      ustack->step_active = us;
    }
  }

  undosys_stack_validate(ustack, true);
  return true;
}

bool BKE_undosys_step_push(UndoStack *ustack, bContext *C, const char *name)
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

bool BKE_undosys_step_undo_with_data_ex(UndoStack *ustack,
                                        bContext *C,
                                        UndoStep *us,
                                        bool use_skip)
{
  UNDO_NESTED_ASSERT(false);
  if (us) {
    undosys_stack_validate(ustack, true);
  }
  UndoStep *us_prev = us ? us->prev : NULL;
  if (us) {
    /* The current state is a copy, we need to load the previous state. */
    us = us_prev;
  }

  if (us != NULL) {
    CLOG_INFO(&LOG, 1, "addr=%p, name='%s', type='%s'", us, us->name, us->type->name);

    /* Handle accumulate steps. */
    if (ustack->step_active) {
      UndoStep *us_iter = ustack->step_active;
      while (us_iter != us) {
        /* TODO:
         * - skip successive steps that store the same data, eg: memfile steps.
         * - or steps that include another steps data, eg: a memfile step includes text undo data.
         */
        undosys_step_decode(C, G_MAIN, ustack, us_iter, -1);
        us_iter = us_iter->prev;
      }
    }

    undosys_step_decode(C, G_MAIN, ustack, us, -1);

    ustack->step_active = us_prev;
    undosys_stack_validate(ustack, true);
    if (use_skip) {
      if (ustack->step_active && ustack->step_active->skip) {
        CLOG_INFO(
            &LOG, 2, "undo continue with skip %p '%s', type='%s'", us, us->name, us->type->name);
        BKE_undosys_step_undo_with_data(ustack, C, ustack->step_active);
      }
    }
    return true;
  }
  return false;
}
bool BKE_undosys_step_undo_with_data(UndoStack *ustack, bContext *C, UndoStep *us)
{
  return BKE_undosys_step_undo_with_data_ex(ustack, C, us, true);
}

bool BKE_undosys_step_undo(UndoStack *ustack, bContext *C)
{
  return BKE_undosys_step_undo_with_data(ustack, C, ustack->step_active);
}

void BKE_undosys_step_undo_from_index(UndoStack *ustack, bContext *C, int index)
{
  UndoStep *us = BLI_findlink(&ustack->steps, index);
  BLI_assert(us->skip == false);
  BKE_undosys_step_load_data(ustack, C, us);
}

bool BKE_undosys_step_redo_with_data_ex(UndoStack *ustack,
                                        bContext *C,
                                        UndoStep *us,
                                        bool use_skip)
{
  UNDO_NESTED_ASSERT(false);
  UndoStep *us_next = us ? us->next : NULL;
  /* Unlike undo accumulate, we always use the next. */
  us = us_next;

  if (us != NULL) {
    CLOG_INFO(&LOG, 1, "addr=%p, name='%s', type='%s'", us, us->name, us->type->name);

    /* Handle accumulate steps. */
    if (ustack->step_active && ustack->step_active->next) {
      UndoStep *us_iter = ustack->step_active->next;
      while (us_iter != us) {
        undosys_step_decode(C, G_MAIN, ustack, us_iter, 1);
        us_iter = us_iter->next;
      }
    }

    undosys_step_decode(C, G_MAIN, ustack, us, 1);
    ustack->step_active = us_next;
    if (use_skip) {
      if (ustack->step_active && ustack->step_active->skip) {
        CLOG_INFO(
            &LOG, 2, "redo continue with skip %p '%s', type='%s'", us, us->name, us->type->name);
        BKE_undosys_step_redo_with_data(ustack, C, ustack->step_active);
      }
    }
    return true;
  }
  return false;
}
bool BKE_undosys_step_redo_with_data(UndoStack *ustack, bContext *C, UndoStep *us)
{
  return BKE_undosys_step_redo_with_data_ex(ustack, C, us, true);
}

bool BKE_undosys_step_redo(UndoStack *ustack, bContext *C)
{
  return BKE_undosys_step_redo_with_data(ustack, C, ustack->step_active);
}

bool BKE_undosys_step_load_data(UndoStack *ustack, bContext *C, UndoStep *us)
{
  UNDO_NESTED_ASSERT(false);
  const int index_active = BLI_findindex(&ustack->steps, ustack->step_active);
  const int index_target = BLI_findindex(&ustack->steps, us);
  BLI_assert(!ELEM(-1, index_active, index_target));
  bool ok = true;

  if (index_target < index_active) {
    uint i = index_active - index_target;
    while (i-- && ok) {
      ok = BKE_undosys_step_undo_with_data_ex(ustack, C, ustack->step_active, false);
    }
  }
  else if (index_target > index_active) {
    uint i = index_target - index_active;
    while (i-- && ok) {
      ok = BKE_undosys_step_redo_with_data_ex(ustack, C, ustack->step_active, false);
    }
  }

  if (ok) {
    BLI_assert(ustack->step_active == us);
  }

  return ok;
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
/** \name ID Reference Utilities
 *
 * Unfortunately we need this for a handful of places.
 */

/* Disable for now since it accesses freed memory.
 * The pointer can only be a key, we can't read it's contents. */
#define USE_LIB_SKIP

static void UNUSED_FUNCTION(BKE_undosys_foreach_ID_ref(UndoStack *ustack,
                                                       UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                                       void *user_data))
{
  for (UndoStep *us = ustack->steps.first; us; us = us->next) {
    const UndoType *ut = us->type;
    if (ut->step_foreach_ID_ref != NULL) {
      ut->step_foreach_ID_ref(us, foreach_ID_ref_fn, user_data);
    }
  }
}

typedef struct UndoIDPtrMapItem {
  /** Never changes (matches undo data). Use as sort key for binary search. */
  const void *ptr;
  /** Write the new pointers here. */
  uint index;
} UndoIDPtrMapItem;

typedef struct UndoIDPtrMap {
  UndoRefID *refs;
  /**
   * Pointer map, update 'dst' members before use.
   * This is always sorted (adds some overhead when adding, in practice it's acceptable since).
   */
  UndoIDPtrMapItem *pmap;

  /** Length for both 'refs' & 'pmap' */
  uint len;
  uint len_alloc;
} UndoIDPtrMap;

#ifdef DEBUG
#  define PMAP_DEFAULT_ALLOC 1
#else
#  define PMAP_DEFAULT_ALLOC 32
#endif

void BKE_undosys_ID_map_foreach_ID_ref(UndoIDPtrMap *map,
                                       UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                       void *user_data)
{
  for (uint i = 0; i < map->len; i++) {
    foreach_ID_ref_fn(user_data, &map->refs[i]);
  }
}

/**
 * Return true when found, otherwise index is set to the index we should insert.
 */
static bool undosys_ID_map_lookup_index(const UndoIDPtrMap *map, const void *key, uint *r_index)
{
  const UndoIDPtrMapItem *pmap = map->pmap;
  const uint len = map->len;
  if (len == 0) {
    if (r_index) {
      *r_index = 0;
    }
    return false;
  }
  int min = 0, max = len - 1;
  while (min <= max) {
    const uint mid = (min + max) / 2;
    if (pmap[mid].ptr < key) {
      min = mid + 1;
    }
    else if (pmap[mid].ptr == key) {
      if (r_index) {
        *r_index = mid;
      }
      return true;
    }
    else if (pmap[mid].ptr > key) {
      max = mid - 1;
    }
  }
  if (r_index) {
    *r_index = min;
  }
  return false;
}

/**
 * A set of ID's use for efficient decoding, so we can map pointers back to the newly loaded data
 * without performing full look ups each time.
 *
 * This can be used as an old_pointer -> new_pointer lookup.
 */
UndoIDPtrMap *BKE_undosys_ID_map_create(void)
{
  UndoIDPtrMap *map = MEM_mallocN(sizeof(*map), __func__);
  map->len_alloc = PMAP_DEFAULT_ALLOC;
  map->refs = MEM_mallocN(sizeof(*map->refs) * map->len_alloc, __func__);
  map->pmap = MEM_mallocN(sizeof(*map->pmap) * map->len_alloc, __func__);
  map->len = 0;
  return map;
}
void BKE_undosys_ID_map_destroy(UndoIDPtrMap *idpmap)
{
  MEM_SAFE_FREE(idpmap->refs);
  MEM_SAFE_FREE(idpmap->pmap);
  MEM_freeN(idpmap);
}

void BKE_undosys_ID_map_add(UndoIDPtrMap *map, ID *id)
{
  uint index;
#ifdef USE_LIB_SKIP
  if (id->lib != NULL) {
    return;
  }
#endif

  if (undosys_ID_map_lookup_index(map, id, &index)) {
    return; /* exists. */
  }

  const uint len_src = map->len;
  const uint len_dst = map->len + 1;
  if (len_dst > map->len_alloc) {
    map->len_alloc *= 2;
    BLI_assert(map->len_alloc >= len_dst);
    map->pmap = MEM_reallocN(map->pmap, sizeof(*map->pmap) * map->len_alloc);
    map->refs = MEM_reallocN(map->refs, sizeof(*map->refs) * map->len_alloc);
  }

#if 0 /* Will be done automatically in callback. */
  BLI_strncpy(map->refs[len_src].name, id->name, sizeof(id->name));
#else
  map->refs[len_src].name[0] = '\0';
#endif
  map->refs[len_src].ptr = id;

  if (len_src != 0 && index != len_src) {
    memmove(&map->pmap[index + 1], &map->pmap[index], sizeof(*map->pmap) * (len_src - index));
  }
  map->pmap[index].ptr = id;
  map->pmap[index].index = len_src;

  map->len = len_dst;
}

ID *BKE_undosys_ID_map_lookup(const UndoIDPtrMap *map, const ID *id_src)
{
  /* We should only ever lookup indices which exist! */
  uint index;
  if (!undosys_ID_map_lookup_index(map, id_src, &index)) {
    BLI_assert(0);
  }
  index = map->pmap[index].index;
  ID *id_dst = map->refs[index].ptr;
  BLI_assert(id_dst != NULL);
  BLI_assert(STREQ(id_dst->name, map->refs[index].name));
  return id_dst;
}

void BKE_undosys_ID_map_add_with_prev(UndoIDPtrMap *map, ID *id, ID **id_prev)
{
  if (id == *id_prev) {
    return;
  }
  *id_prev = id;
  BKE_undosys_ID_map_add(map, id);
}

ID *BKE_undosys_ID_map_lookup_with_prev(const UndoIDPtrMap *map, ID *id_src, ID *id_prev_match[2])
{
  if (id_src == id_prev_match[0]) {
    return id_prev_match[1];
  }
  else {
#ifdef USE_LIB_SKIP
    ID *id_dst = BKE_undosys_ID_map_lookup(map, id_src);
#else
    ID *id_dst = (id_src->lib == NULL) ? BKE_undosys_ID_map_lookup(map, id_src) : id_src;
#endif
    id_prev_match[0] = id_src;
    id_prev_match[1] = id_dst;
    return id_dst;
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
  for (UndoStep *us = ustack->steps.first; us; us = us->next) {
    printf("[%c%c%c%c] %3d type='%s', name='%s'\n",
           (us == ustack->step_active) ? '*' : ' ',
           us->is_applied ? '#' : ' ',
           (us == ustack->step_active_memfile) ? 'M' : ' ',
           us->skip ? 'S' : ' ',
           index,
           us->type->name,
           us->name);
    index++;
  }
}

/** \} */
