/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_enum_flags.hh"
#include "BLI_path_utils.hh"

#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_userdef_types.h"

struct Main;
struct UndoStep;
struct UndoType;
struct bContext;

/* IDs */
struct GreasePencil;
struct Main;
struct Mesh;
struct Object;
struct Scene;
struct Text;

struct UndoRefID {
  struct ID *ptr;
  char name[MAX_ID_NAME];
  char library_filepath_abs[FILE_MAX];
};
/* UndoRefID_Mesh & friends. */
#define UNDO_REF_ID_TYPE(ptr_ty) \
  struct UndoRefID_##ptr_ty { \
    struct ptr_ty *ptr; \
    char name[MAX_ID_NAME]; \
    char library_filepath_abs[FILE_MAX]; \
  }
UNDO_REF_ID_TYPE(GreasePencil);
UNDO_REF_ID_TYPE(Mesh);
UNDO_REF_ID_TYPE(Object);
UNDO_REF_ID_TYPE(Scene);
UNDO_REF_ID_TYPE(Text);
UNDO_REF_ID_TYPE(Image);
UNDO_REF_ID_TYPE(PaintCurve);

struct UndoStack {
  ListBase steps;
  UndoStep *step_active;
  /**
   * The last memfile state read, used so we can be sure the names from the
   * library state matches the state an undo step was written in.
   */
  UndoStep *step_active_memfile;

  /**
   * Some undo systems require begin/end, see: #UndoType.step_encode_init
   *
   * \note This is not included in the 'steps' list.
   * That is done once end is called.
   */
  UndoStep *step_init;

  /**
   * Keep track of nested group begin/end calls,
   * within which all but the last undo-step is marked for skipping.
   */
  int group_level;
};

struct UndoStep {
  UndoStep *next, *prev;
  char name[64];
  const UndoType *type;
  /** Size in bytes of all data in step (not including the step). */
  size_t data_size;
  /** Users should never see this step (only use for internal consistency). */
  bool skip;
  /** Some situations require the global state to be stored, edge cases when exiting modes. */
  bool use_memfile_step;
  /**
   * When this is true, undo/memfile read code is allowed to re-use old data-blocks for unchanged
   * IDs, and existing depsgraphs. This has to be forbidden in some cases (like renamed IDs).
   */
  bool use_old_bmain_data;
  /** For use by undo systems that accumulate changes (mesh-sculpt & image-painting). */
  bool is_applied;
  /* Over alloc 'type->struct_size'. */
};

enum eUndoStepDir {
  STEP_REDO = 1,
  STEP_UNDO = -1,
  STEP_INVALID = 0,
};

enum eUndoPushReturn {
  UNDO_PUSH_RET_FAILURE = 0,
  UNDO_PUSH_RET_SUCCESS = (1 << 0),
  UNDO_PUSH_RET_OVERRIDE_CHANGED = (1 << 1),
};
ENUM_OPERATORS(eUndoPushReturn)

using UndoTypeForEachIDRefFn = void (*)(void *user_data, UndoRefID *id_ref);

struct UndoType {
  UndoType *next, *prev;
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
  void (*step_encode_init)(bContext *C, UndoStep *us);

  bool (*step_encode)(bContext *C, Main *bmain, UndoStep *us);
  void (*step_decode)(bContext *C, Main *bmain, UndoStep *us, eUndoStepDir dir, bool is_final);

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
   * generic allocation in BKE's `undo_system.cc`. */
  size_t step_size;
};

/** #UndoType.flag bit-flags. */
enum eUndoTypeFlags {
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
};

/* -------------------------------------------------------------------- */
/** \name Public Undo Types
 *
 * Expose since we need to perform operations on specific undo types (rarely).
 * \{ */

extern const UndoType *BKE_UNDOSYS_TYPE_IMAGE;
extern const UndoType *BKE_UNDOSYS_TYPE_MEMFILE;
extern const UndoType *BKE_UNDOSYS_TYPE_PAINTCURVE;
extern const UndoType *BKE_UNDOSYS_TYPE_PARTICLE;
extern const UndoType *BKE_UNDOSYS_TYPE_SCULPT;
extern const UndoType *BKE_UNDOSYS_TYPE_TEXT;

/** \} */

#define BKE_UNDOSYS_TYPE_IS_MEMFILE_SKIP(ty) ELEM(ty, BKE_UNDOSYS_TYPE_IMAGE)

UndoStack *BKE_undosys_stack_create();
void BKE_undosys_stack_destroy(UndoStack *ustack);
void BKE_undosys_stack_clear(UndoStack *ustack);
void BKE_undosys_stack_clear_active(UndoStack *ustack);
/* name optional */
bool BKE_undosys_stack_has_undo(const UndoStack *ustack, const char *name);
void BKE_undosys_stack_init_from_main(UndoStack *ustack, Main *bmain);
/* Called after #BKE_undosys_stack_init_from_main. */
void BKE_undosys_stack_init_from_context(UndoStack *ustack, bContext *C);
UndoStep *BKE_undosys_stack_active_with_type(UndoStack *ustack, const UndoType *ut);
UndoStep *BKE_undosys_stack_init_or_active_with_type(UndoStack *ustack, const UndoType *ut);
/**
 * \param steps: Limit the number of undo steps.
 * \param memory_limit: Limit the amount of memory used by the undo stack.
 */
void BKE_undosys_stack_limit_steps_and_memory(UndoStack *ustack, int steps, size_t memory_limit);
#define BKE_undosys_stack_limit_steps_and_memory_defaults(ustack) \
  BKE_undosys_stack_limit_steps_and_memory(ustack, U.undosteps, (size_t)U.undomemory * 1024 * 1024)

void BKE_undosys_stack_group_begin(UndoStack *ustack);
void BKE_undosys_stack_group_end(UndoStack *ustack);

/**
 * Only some UndoType's require init.
 */
UndoStep *BKE_undosys_step_push_init_with_type(UndoStack *ustack,
                                               bContext *C,
                                               const char *name,
                                               const UndoType *ut);
UndoStep *BKE_undosys_step_push_init(UndoStack *ustack, bContext *C, const char *name);

/**
 * \param C: Can be NULL from some callers if their encoding function doesn't need it
 */
eUndoPushReturn BKE_undosys_step_push_with_type(UndoStack *ustack,
                                                bContext *C,
                                                const char *name,
                                                const UndoType *ut);
eUndoPushReturn BKE_undosys_step_push(UndoStack *ustack, bContext *C, const char *name);

UndoStep *BKE_undosys_step_find_by_name_with_type(UndoStack *ustack,
                                                  const char *name,
                                                  const UndoType *ut);
UndoStep *BKE_undosys_step_find_by_type(UndoStack *ustack, const UndoType *ut);
UndoStep *BKE_undosys_step_find_by_name(UndoStack *ustack, const char *name);

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
                                             const UndoStep *us_reference);

/**
 * Undo/Redo until the given `us_target` step becomes the active (currently loaded) one.
 *
 * \note Unless `us_target` is a 'skipped' one and `use_skip` is true, `us_target`
 * will become the active step.
 *
 * \note In case `use_skip` is true, the final target will always be **beyond** the given one
 * (if the given one has to be skipped).
 *
 * \param us_reference: If NULL, will be set to current active step in the undo stack. Otherwise,
 * it is assumed to match the current state, and will be used as basis for the undo/redo process
 * (i.e. all steps in-between `us_reference` and `us_target` will be processed).
 */
bool BKE_undosys_step_load_data_ex(
    UndoStack *ustack, bContext *C, UndoStep *us_target, UndoStep *us_reference, bool use_skip);
/**
 * Undo/Redo until the given `us_target` step becomes the active (currently loaded) one.
 */
bool BKE_undosys_step_load_data(UndoStack *ustack, bContext *C, UndoStep *us_target);
/**
 * Undo/Redo until the step matching given `index` in the undo stack becomes the active
 * (currently loaded) one.
 */
void BKE_undosys_step_load_from_index(UndoStack *ustack, bContext *C, int index);

/**
 * Undo until `us_target` step becomes the active (currently loaded) one.
 *
 * \warning This function assumes that the given target step is _before_ current active one.
 *
 * \note Unless `us_target` is a 'skipped' one and `use_skip` is true,
 * `us_target` will become the active step.
 *
 * \note In case `use_skip` is true, the final target will always be **before** the given one
 * (if the given one has to be skipped).
 */
bool BKE_undosys_step_undo_with_data_ex(UndoStack *ustack,
                                        bContext *C,
                                        UndoStep *us_target,
                                        bool use_skip);
/**
 * Undo until `us_target` step becomes the active (currently loaded) one.
 *
 * \note See #BKE_undosys_step_undo_with_data_ex for details.
 */
bool BKE_undosys_step_undo_with_data(UndoStack *ustack, bContext *C, UndoStep *us_target);
/**
 * Undo one step from current active (currently loaded) one.
 */
bool BKE_undosys_step_undo(UndoStack *ustack, bContext *C);

/**
 * Redo until `us_target` step becomes the active (currently loaded) one.
 *
 * \warning This function assumes that the given target step is _after_ current active one.
 *
 * \note Unless `us_target` is a 'skipped' one and `use_skip` is true,
 * `us_target` will become the active step.
 *
 * \note In case `use_skip` is true, the final target will always be **after** the given one
 * (if the given one has to be skipped).
 */
bool BKE_undosys_step_redo_with_data_ex(UndoStack *ustack,
                                        bContext *C,
                                        UndoStep *us_target,
                                        bool use_skip);
/**
 * Redo until `us_target` step becomes the active (currently loaded) one.
 *
 * \note See #BKE_undosys_step_redo_with_data_ex for details.
 */
bool BKE_undosys_step_redo_with_data(UndoStack *ustack, bContext *C, UndoStep *us_target);
/**
 * Redo one step from current active one.
 */
bool BKE_undosys_step_redo(UndoStack *ustack, bContext *C);

/**
 * Useful when we want to diff against previous undo data but can't be sure the types match.
 */
UndoStep *BKE_undosys_step_same_type_next(UndoStep *us);
/**
 * Useful when we want to diff against previous undo data but can't be sure the types match.
 */
UndoStep *BKE_undosys_step_same_type_prev(UndoStep *us);

/* Type System. */

/**
 * Similar to #WM_operatortype_append
 */
UndoType *BKE_undosys_type_append(void (*undosys_fn)(UndoType *));
void BKE_undosys_type_free_all();

/* ID Accessor. */

#if 0 /* functionality is only used internally for now. */
void BKE_undosys_foreach_ID_ref(UndoStack *ustack,
                                UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                void *user_data);
#endif

void BKE_undosys_print(UndoStack *ustack);
