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
#ifndef __BKE_UNDO_SYSTEM_H__
#define __BKE_UNDO_SYSTEM_H__

/** \file
 * \ingroup bke
 */

struct Main;
struct UndoStep;
struct bContext;

/* ID's */
struct Main;
struct Mesh;
struct Object;
struct Scene;
struct Text;

#include "DNA_ID.h"
#include "DNA_listBase.h"

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
  /** For use by undo systems that accumulate changes (text editor, painting). */
  bool is_applied;
  /* Over alloc 'type->struct_size'. */
} UndoStep;

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
      struct bContext *C, struct Main *bmain, UndoStep *us, int dir, bool is_final);

  /**
   * \note When freeing all steps,
   * free from the last since #MemFileUndoType will merge with the next undo type in the list. */
  void (*step_free)(UndoStep *us);

  void (*step_foreach_ID_ref)(UndoStep *us,
                              UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                              void *user_data);

  bool use_context;

  int step_size;
} UndoType;

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
bool BKE_undosys_stack_has_undo(UndoStack *ustack, const char *name);
void BKE_undosys_stack_init_from_main(UndoStack *ustack, struct Main *bmain);
void BKE_undosys_stack_init_from_context(UndoStack *ustack, struct bContext *C);
UndoStep *BKE_undosys_stack_active_with_type(UndoStack *ustack, const UndoType *ut);
UndoStep *BKE_undosys_stack_init_or_active_with_type(UndoStack *ustack, const UndoType *ut);
void BKE_undosys_stack_limit_steps_and_memory(UndoStack *ustack, int steps, size_t memory_limit);

/* Only some UndoType's require init. */
UndoStep *BKE_undosys_step_push_init_with_type(UndoStack *ustack,
                                               struct bContext *C,
                                               const char *name,
                                               const UndoType *ut);
UndoStep *BKE_undosys_step_push_init(UndoStack *ustack, struct bContext *C, const char *name);

bool BKE_undosys_step_push_with_type(UndoStack *ustack,
                                     struct bContext *C,
                                     const char *name,
                                     const UndoType *ut);
bool BKE_undosys_step_push(UndoStack *ustack, struct bContext *C, const char *name);

UndoStep *BKE_undosys_step_find_by_name_with_type(UndoStack *ustack,
                                                  const char *name,
                                                  const UndoType *ut);
UndoStep *BKE_undosys_step_find_by_type(UndoStack *ustack, const UndoType *ut);
UndoStep *BKE_undosys_step_find_by_name(UndoStack *ustack, const char *name);

bool BKE_undosys_step_undo_with_data_ex(UndoStack *ustack,
                                        struct bContext *C,
                                        UndoStep *us,
                                        bool use_skip);
bool BKE_undosys_step_undo_with_data(UndoStack *ustack, struct bContext *C, UndoStep *us);
bool BKE_undosys_step_undo(UndoStack *ustack, struct bContext *C);

bool BKE_undosys_step_redo_with_data_ex(UndoStack *ustack,
                                        struct bContext *C,
                                        UndoStep *us,
                                        bool use_skip);
bool BKE_undosys_step_redo_with_data(UndoStack *ustack, struct bContext *C, UndoStep *us);
bool BKE_undosys_step_redo(UndoStack *ustack, struct bContext *C);

bool BKE_undosys_step_load_data(UndoStack *ustack, struct bContext *C, UndoStep *us);

void BKE_undosys_step_undo_from_index(UndoStack *ustack, struct bContext *C, int index);
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

/* Use when the undo step stores many arbitrary pointers. */
struct UndoIDPtrMap;
struct UndoIDPtrMap *BKE_undosys_ID_map_create(void);
void BKE_undosys_ID_map_destroy(struct UndoIDPtrMap *map);
void BKE_undosys_ID_map_add(struct UndoIDPtrMap *map, ID *id);
struct ID *BKE_undosys_ID_map_lookup(const struct UndoIDPtrMap *map, const struct ID *id_src);

void BKE_undosys_ID_map_add_with_prev(struct UndoIDPtrMap *map,
                                      struct ID *id,
                                      struct ID **id_prev);
struct ID *BKE_undosys_ID_map_lookup_with_prev(const struct UndoIDPtrMap *map,
                                               struct ID *id_src,
                                               struct ID *id_prev_match[2]);

void BKE_undosys_ID_map_foreach_ID_ref(struct UndoIDPtrMap *map,
                                       UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                       void *user_data);

void BKE_undosys_print(UndoStack *ustack);

#endif /* __BKE_UNDO_SYSTEM_H__ */
