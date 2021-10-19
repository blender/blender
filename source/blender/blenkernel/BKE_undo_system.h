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
#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_ID.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct UndoStep;
struct bContext;

/* ID's */
struct Main;
struct Mesh;
struct Object;
struct Scene;
struct Text;

typedef struct UndoRefID {
  struct ID *ptr;
  char name[MAX_ID_NAME];
} UndoRefID;
/* UndoRefID_Mesh & friends. */
#define UNDO_REF_ID_TYPE(ptr_ty) \
  typedef struct UndoRefID_##ptr_ty { \
    struct ptr_ty *ptr; \
    char name[MAX_ID_NAME]; \
  } UndoRefID_##ptr_ty
UNDO_REF_ID_TYPE(Mesh);
UNDO_REF_ID_TYPE(Object);
UNDO_REF_ID_TYPE(Scene);
UNDO_REF_ID_TYPE(Text);
UNDO_REF_ID_TYPE(Image);
UNDO_REF_ID_TYPE(PaintCurve);

typedef struct UndoStack {
  ListBase steps;
  struct UndoStep *step_active;
  /**
   * The last memfile state read, used so we can be sure the names from the
   * library state matches the state an undo step was written in.
   */
  struct UndoStep *step_active_memfile;

  /**
   * Some undo systems require begin/end, see: #UndoType.step_encode_init
   *
   * \note This is not included in the 'steps' list.
   * That is done once end is called.
   */
  struct UndoStep *step_init;

  /**
   * Keep track of nested group begin/end calls,
   * within which all but the last undo-step is marked for skipping.
   */
  int group_level;
} UndoStack;

typedef struct UndoStep {
  struct UndoStep *next, *prev;
  char name[64];
  const struct UndoType *type;
  /** Size in bytes of all data in step (not including the step). */
  size_t data_size;
  /** Users should never see this step (only use for internal consistency). */
  bool skip;
  /** Some situations require the global state to be stored, edge cases when exiting modes. */
  bool use_memfile_step;
  /** When this is true, undo/memfile read code is allowed to re-use old data-blocks for unchanged
   * IDs, and existing depsgraphes. This has to be forbidden in some cases (like renamed IDs). */
  bool use_old_bmain_data;
  /** For use by undo systems that accumulate changes (text editor, painting). */
  bool is_applied;
  /* Over alloc 'type->struct_size'. */
} UndoStep;

typedef enum eUndoStepDir {
  STEP_REDO = 1,
  STEP_UNDO = -1,
  STEP_INVALID = 0,
} eUndoStepDir;

typedef enum eUndoPushReturn {
  UNDO_PUSH_RET_FAILURE = 0,
  UNDO_PUSH_RET_SUCCESS = (1 << 0),
  UNDO_PUSH_RET_OVERRIDE_CHANGED = (1 << 1),
} eUndoPushReturn;

typedef void (*UndoTypeForEachIDRefFn)(void *user_data, struct UndoRefID *id_ref);

typedef struct UndoType {
  struct UndoType *next, *prev;
  /** Only for debugging. */
  const char *name;

  /**
   * When NULL, we don't consider this undo type for context checks.
   * Operators must explicitly set the undo type and handle adding the undo step.
   * This is needed when tools operate on data which isn't the primary mode
   * (eg, paint-curve in sculpt mode).
   */
  bool (*poll)(struct bContext *C);

  /**
   * None of these callbacks manage list add/removal.
   *
   * Note that 'step_encode_init' is optional,
   * some undo types need to perform operations before undo push finishes.
   */
  void (*step_encode_init)(struct bContext *C, UndoStep *us);

  bool (*step_encode)(struct bContext *C, struct Main *bmain, UndoStep *us);
  void (*step_decode)(
      struct bContext *C, struct Main *bmain, UndoStep *us, const eUndoStepDir dir, bool is_final);

  /**
   * \note When freeing all steps,
   * free from the last since #BKE_UNDOSYS_TYPE_MEMFILE
   * will merge with the next undo type in the list.
   */
  void (*step_free)(UndoStep *us);

  void (*step_foreach_ID_ref)(UndoStep *us,
                              UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                              void *user_data);

  /** Information for the generic undo system to refine handling of this specific undo type. */
  uint flags;

  /**
   * The size of the undo struct 'inherited' from #UndoStep for that specific type. Used for
   * generic allocation in BKE's `undo_system.c`. */
  size_t step_size;
} UndoType;

/** #UndoType.flag bitflags. */
typedef enum eUndoTypeFlags {
  /**
   * This undo type `encode` callback needs a valid context, it will fail otherwise.
   * \note Callback is still supposed to properly deal with a NULL context pointer.
   */
  UNDOTYPE_FLAG_NEED_CONTEXT_FOR_ENCODE = 1 << 0,

  /**
   * When the active undo step is of this type, it must be read before loading other undo steps.
   *
   * This is typically used for undo systems that store both before/after states.
   */
  UNDOTYPE_FLAG_DECODE_ACTIVE_STEP = 1 << 1,
} eUndoTypeFlags;

/* Expose since we need to perform operations on specific undo types (rarely). */
extern const UndoType *BKE_UNDOSYS_TYPE_IMAGE;
extern const UndoType *BKE_UNDOSYS_TYPE_MEMFILE;
extern const UndoType *BKE_UNDOSYS_TYPE_PAINTCURVE;
extern const UndoType *BKE_UNDOSYS_TYPE_PARTICLE;
extern const UndoType *BKE_UNDOSYS_TYPE_SCULPT;
extern const UndoType *BKE_UNDOSYS_TYPE_TEXT;

#define BKE_UNDOSYS_TYPE_IS_MEMFILE_SKIP(ty) ELEM(ty, BKE_UNDOSYS_TYPE_IMAGE)

UndoStack *BKE_undosys_stack_create(void);
void BKE_undosys_stack_destroy(UndoStack *ustack);
void BKE_undosys_stack_clear(UndoStack *ustack);
void BKE_undosys_stack_clear_active(UndoStack *ustack);
bool BKE_undosys_stack_has_undo(const UndoStack *ustack, const char *name);
void BKE_undosys_stack_init_from_main(UndoStack *ustack, struct Main *bmain);
void BKE_undosys_stack_init_from_context(UndoStack *ustack, struct bContext *C);
UndoStep *BKE_undosys_stack_active_with_type(UndoStack *ustack, const UndoType *ut);
UndoStep *BKE_undosys_stack_init_or_active_with_type(UndoStack *ustack, const UndoType *ut);
void BKE_undosys_stack_limit_steps_and_memory(UndoStack *ustack, int steps, size_t memory_limit);
#define BKE_undosys_stack_limit_steps_and_memory_defaults(ustack) \
  BKE_undosys_stack_limit_steps_and_memory(ustack, U.undosteps, (size_t)U.undomemory * 1024 * 1024)

void BKE_undosys_stack_group_begin(UndoStack *ustack);
void BKE_undosys_stack_group_end(UndoStack *ustack);

/* Only some UndoType's require init. */
UndoStep *BKE_undosys_step_push_init_with_type(UndoStack *ustack,
                                               struct bContext *C,
                                               const char *name,
                                               const UndoType *ut);
UndoStep *BKE_undosys_step_push_init(UndoStack *ustack, struct bContext *C, const char *name);

eUndoPushReturn BKE_undosys_step_push_with_type(UndoStack *ustack,
                                                struct bContext *C,
                                                const char *name,
                                                const UndoType *ut);
eUndoPushReturn BKE_undosys_step_push(UndoStack *ustack, struct bContext *C, const char *name);

UndoStep *BKE_undosys_step_find_by_name_with_type(UndoStack *ustack,
                                                  const char *name,
                                                  const UndoType *ut);
UndoStep *BKE_undosys_step_find_by_type(UndoStack *ustack, const UndoType *ut);
UndoStep *BKE_undosys_step_find_by_name(UndoStack *ustack, const char *name);

eUndoStepDir BKE_undosys_step_calc_direction(const UndoStack *ustack,
                                             const UndoStep *us_target,
                                             const UndoStep *us_reference);

bool BKE_undosys_step_load_data_ex(UndoStack *ustack,
                                   struct bContext *C,
                                   UndoStep *us_target,
                                   UndoStep *us_reference,
                                   const bool use_skip);
bool BKE_undosys_step_load_data(UndoStack *ustack, struct bContext *C, UndoStep *us_target);
void BKE_undosys_step_load_from_index(UndoStack *ustack, struct bContext *C, const int index);

bool BKE_undosys_step_undo_with_data_ex(UndoStack *ustack,
                                        struct bContext *C,
                                        UndoStep *us,
                                        bool use_skip);
bool BKE_undosys_step_undo_with_data(UndoStack *ustack, struct bContext *C, UndoStep *us_target);
bool BKE_undosys_step_undo(UndoStack *ustack, struct bContext *C);

bool BKE_undosys_step_redo_with_data_ex(UndoStack *ustack,
                                        struct bContext *C,
                                        UndoStep *us,
                                        bool use_skip);
bool BKE_undosys_step_redo_with_data(UndoStack *ustack, struct bContext *C, UndoStep *us_target);
bool BKE_undosys_step_redo(UndoStack *ustack, struct bContext *C);

UndoStep *BKE_undosys_step_same_type_next(UndoStep *us);
UndoStep *BKE_undosys_step_same_type_prev(UndoStep *us);

/* Type System */
UndoType *BKE_undosys_type_append(void (*undosys_fn)(UndoType *));
void BKE_undosys_type_free_all(void);

/* ID Accessor */
#if 0 /* functionality is only used internally for now. */
void BKE_undosys_foreach_ID_ref(UndoStack *ustack,
                                UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                void *user_data);
#endif

void BKE_undosys_print(UndoStack *ustack);

#ifdef __cplusplus
}
#endif
