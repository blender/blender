/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup python
 */

#pragma once

#include <optional>
#include <string>

#include "BLI_sys_types.h"

#ifdef WITH_INTERNATIONAL

#  include "BLI_string_ref.hh"

#endif

struct ARegionType;
struct AnimationEvalContext;
struct ChannelDriver; /* DNA_anim_types.h */
struct ID;            /* DNA_ID.h */
struct ListBase;      /* DNA_listBase.h */
struct Object;        /* DNA_object_types.h */
struct PathResolvedRNA;
struct Text;              /* defined in DNA_text_types.h */
struct bConstraint;       /* DNA_constraint_types.h */
struct bConstraintOb;     /* DNA_constraint_types.h */
struct bConstraintTarget; /* DNA_constraint_types.h */
struct bContext;
struct bContextDataResult;
struct StructRNA;
struct wmWindowManager;

/* global interpreter lock */

using BPy_ThreadStatePtr = void *;

/**
 * Analogue of #PyEval_SaveThread()
 */
[[nodiscard]] BPy_ThreadStatePtr BPY_thread_save();
/**
 * Analogue of #PyEval_RestoreThread()
 */
void BPY_thread_restore(BPy_ThreadStatePtr tstate);

/** Our own wrappers to #Py_BEGIN_ALLOW_THREADS / #Py_END_ALLOW_THREADS */
#define BPy_BEGIN_ALLOW_THREADS \
  { \
    BPy_ThreadStatePtr _bpy_saved_tstate = BPY_thread_save(); \
    (void)0
#define BPy_END_ALLOW_THREADS \
  BPY_thread_restore(_bpy_saved_tstate); \
  } \
  (void)0

/**
 * Print the Python backtrace of the current thread state.
 *
 * Should be safe to call from anywhere at any point, may not output anything if there is no valid
 * python thread state available.
 */
void BPY_thread_backtrace_print();

void BPY_text_free_code(Text *text);
/**
 * Needed so the #Main pointer in `bpy.data` doesn't become out of date.
 */
void BPY_modules_update();
void BPY_modules_load_user(bContext *C);

void BPY_app_handlers_reset(bool do_all);

/**
 * Run on exit to free any cached data.
 */
void BPY_driver_exit();

/**
 * Update function, it gets rid of python-drivers global dictionary: `bpy.app.driver_namespace`,
 * forcing #BPY_driver_exec to recreate it. Use this when loading a new `.blend` file
 * so any variables setup by the previous blend file are cleared.
 */
void BPY_driver_reset();

/**
 * This evaluates Python driver expressions, `driver_orig->expression`
 * is a Python expression that should evaluate to a float number, which is returned.
 */
[[nodiscard]] float BPY_driver_exec(PathResolvedRNA *anim_rna,
                                    ChannelDriver *driver,
                                    ChannelDriver *driver_orig,
                                    const AnimationEvalContext *anim_eval_context);

/**
 * Acquire the global-interpreter-lock (GIL) and wrap `Py_DECREF`.
 * as there are some cases when this needs to be called outside the Python API code.
 */
void BPY_DECREF(void *pyob_ptr);

void BPY_DECREF_RNA_INVALIDATE(void *pyob_ptr);
[[nodiscard]] bool BPY_context_member_get(bContext *C,
                                          const char *member,
                                          bContextDataResult *result);
void BPY_context_set(bContext *C);
/**
 * Use for updating while a python script runs - in case of file load.
 */
void BPY_context_update(bContext *C);

/**
 * Use for `CTX_*_set(..)` functions need to set values which are later read back as expected.
 * In this case we don't want the Python context to override the values as it causes problems
 * see #66256.
 *
 * \param dict_p: A pointer to #bContext.data.py_context so we can assign a new value.
 * \param dict_orig: The value of #bContext.data.py_context_orig to check if we need to copy.
 */
void BPY_context_dict_clear_members_array(void **dict_p,
                                          void *dict_orig,
                                          const char *context_members[],
                                          uint context_members_len);

void BPY_id_release(ID *id);

/**
 * Free (actually dereference) the Python type object representing the given #StrucRNA type,
 * if it is defined.
 */
void BPY_free_srna_pytype(StructRNA *srna);

/**
 * Avoids duplicating keyword list.
 */
[[nodiscard]] bool BPY_string_is_keyword(const char *str);

/**
 * Get current Python stack location.
 * Returns a string like `filename.py:123` if available, #std::nullopt otherwise.
 */
[[nodiscard]] std::optional<std::string> BPY_python_current_file_and_line(void);

/* `bpy_rna_callback.cc` */

void BPY_callback_screen_free(ARegionType *art);
void BPY_callback_wm_free(wmWindowManager *wm);

/* I18n for addons */
#ifdef WITH_INTERNATIONAL
[[nodiscard]] std::optional<blender::StringRefNull> BPY_app_translations_py_pgettext(
    blender::StringRef msgctxt, blender::StringRef msgid);
#endif
