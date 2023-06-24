/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup python
 */

#pragma once

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
struct bPythonConstraint; /* DNA_constraint_types.h */
struct wmWindowManager;

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

void BPY_pyconstraint_exec(struct bPythonConstraint *con,
                           struct bConstraintOb *cob,
                           struct ListBase *targets);
//  void BPY_pyconstraint_settings(void *arg1, void *arg2);
void BPY_pyconstraint_target(struct bPythonConstraint *con, struct bConstraintTarget *ct);
void BPY_pyconstraint_update(struct Object *owner, struct bConstraint *con);
bool BPY_is_pyconstraint(struct Text *text);
//  void BPY_free_pyconstraint_links(struct Text *text);

/* global interpreter lock */

typedef void *BPy_ThreadStatePtr;

/**
 * Analogue of #PyEval_SaveThread()
 */
BPy_ThreadStatePtr BPY_thread_save(void);
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

void BPY_text_free_code(struct Text *text);
/**
 * Needed so the #Main pointer in `bpy.data` doesn't become out of date.
 */
void BPY_modules_update(void);
void BPY_modules_load_user(struct bContext *C);

void BPY_app_handlers_reset(bool do_all);

/**
 * Run on exit to free any cached data.
 */
void BPY_driver_exit(void);

/**
 * Update function, it gets rid of python-drivers global dictionary: `bpy.app.driver_namespace`,
 * forcing #BPY_driver_exec to recreate it. Use this when loading a new `.blend` file
 * so any variables setup by the previous blend file are cleared.
 */
void BPY_driver_reset(void);

/**
 * This evaluates Python driver expressions, `driver_orig->expression`
 * is a Python expression that should evaluate to a float number, which is returned.
 */
float BPY_driver_exec(struct PathResolvedRNA *anim_rna,
                      struct ChannelDriver *driver,
                      struct ChannelDriver *driver_orig,
                      const struct AnimationEvalContext *anim_eval_context);

/**
 * Acquire the global-interpreter-lock (GIL) and wrap `Py_DECREF`.
 * as there are some cases when this needs to be called outside the Python API code.
 */
void BPY_DECREF(void *pyob_ptr);

void BPY_DECREF_RNA_INVALIDATE(void *pyob_ptr);
int BPY_context_member_get(struct bContext *C,
                           const char *member,
                           struct bContextDataResult *result);
void BPY_context_set(struct bContext *C);
/**
 * Use for updating while a python script runs - in case of file load.
 */
void BPY_context_update(struct bContext *C);

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

void BPY_id_release(struct ID *id);

/**
 * Avoids duplicating keyword list.
 */
bool BPY_string_is_keyword(const char *str);

/* bpy_rna_callback.c */

void BPY_callback_screen_free(struct ARegionType *art);
void BPY_callback_wm_free(struct wmWindowManager *wm);

/* I18n for addons */
#ifdef WITH_INTERNATIONAL
const char *BPY_app_translations_py_pgettext(const char *msgctxt, const char *msgid);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
