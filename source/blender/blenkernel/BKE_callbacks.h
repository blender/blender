/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct ID;
struct Main;
struct PointerRNA;

/**
 * Callbacks for One Off Actions
 * =============================
 *
 * - `{ACTION}` use in cases where only a single callback is required,
 *   `VERSION_UPDATE` and `RENDER_STATS` for example.
 *
 * \note avoid single callbacks if there is a chance `PRE/POST` are useful to differentiate
 * since renaming callbacks may break Python scripts.
 *
 * Callbacks for Common Actions
 * ============================
 *
 * - `{ACTION}_PRE` run before the action.
 * - `{ACTION}_POST` run after the action.
 *
 * Optional Additional Callbacks
 * -----------------------------
 *
 * - `{ACTION}_INIT` when the handler may manipulate the context used to run the action.
 *
 *   Examples where `INIT` functions may be useful are:
 *
 *   - When rendering, an `INIT` function may change the camera or render settings,
 *     things which a `PRE` function can't support as this information has already been used.
 *   - When saving an `INIT` function could temporarily change the preferences.
 *
 * - `{ACTION}_POST_FAIL` should be included if the action may fail.
 *
 *   Use this so a call to the `PRE` callback always has a matching call to `POST` or `POST_FAIL`.
 *
 * \note in most cases only `PRE/POST` are required.
 *
 * Callbacks for Background/Modal Tasks
 * ====================================
 *
 * - `{ACTION}_INIT`
 * - `{ACTION}_COMPLETE` when a background job has finished.
 * - `{ACTION}_CANCEL` When a background job is canceled partway through.
 *
 *   While cancellation may be caused by any number of reasons, common causes may include:
 *
 *   - Explicit user cancellation.
 *   - Exiting Blender.
 *   - Failure to acquire resources (such as disk-full, out of memory ... etc).
 *
 * \note `PRE/POST` handlers may be used along side modal task handlers
 * as is the case for rendering, where rendering an animation uses modal task handlers,
 * rendering a single frame has `PRE/POST` handlers.
 *
 * Python Access
 * =============
 *
 * All callbacks here must be exposed via the Python module `bpy.app.handlers`,
 * see `bpy_app_handlers.cc`.
 */
typedef enum {
  BKE_CB_EVT_FRAME_CHANGE_PRE,
  BKE_CB_EVT_FRAME_CHANGE_POST,
  BKE_CB_EVT_RENDER_PRE,
  BKE_CB_EVT_RENDER_POST,
  BKE_CB_EVT_RENDER_WRITE,
  BKE_CB_EVT_RENDER_STATS,
  BKE_CB_EVT_RENDER_INIT,
  BKE_CB_EVT_RENDER_COMPLETE,
  BKE_CB_EVT_RENDER_CANCEL,
  BKE_CB_EVT_LOAD_PRE,
  BKE_CB_EVT_LOAD_POST,
  BKE_CB_EVT_LOAD_POST_FAIL,
  BKE_CB_EVT_SAVE_PRE,
  BKE_CB_EVT_SAVE_POST,
  BKE_CB_EVT_SAVE_POST_FAIL,
  BKE_CB_EVT_UNDO_PRE,
  BKE_CB_EVT_UNDO_POST,
  BKE_CB_EVT_REDO_PRE,
  BKE_CB_EVT_REDO_POST,
  BKE_CB_EVT_DEPSGRAPH_UPDATE_PRE,
  BKE_CB_EVT_DEPSGRAPH_UPDATE_POST,
  BKE_CB_EVT_VERSION_UPDATE,
  BKE_CB_EVT_LOAD_FACTORY_USERDEF_POST,
  BKE_CB_EVT_LOAD_FACTORY_STARTUP_POST,
  BKE_CB_EVT_XR_SESSION_START_PRE,
  BKE_CB_EVT_ANNOTATION_PRE,
  BKE_CB_EVT_ANNOTATION_POST,
  BKE_CB_EVT_OBJECT_BAKE_PRE,
  BKE_CB_EVT_OBJECT_BAKE_COMPLETE,
  BKE_CB_EVT_OBJECT_BAKE_CANCEL,
  BKE_CB_EVT_COMPOSITE_PRE,
  BKE_CB_EVT_COMPOSITE_POST,
  BKE_CB_EVT_COMPOSITE_CANCEL,
  BKE_CB_EVT_ANIMATION_PLAYBACK_PRE,
  BKE_CB_EVT_ANIMATION_PLAYBACK_POST,
  BKE_CB_EVT_EXTENSION_REPOS_UPDATE_PRE,
  BKE_CB_EVT_EXTENSION_REPOS_UPDATE_POST,
  BKE_CB_EVT_TOT,
} eCbEvent;

typedef struct bCallbackFuncStore {
  struct bCallbackFuncStore *next, *prev;
  void (*func)(struct Main *, struct PointerRNA **, int num_pointers, void *arg);
  void *arg;
  short alloc;
} bCallbackFuncStore;

void BKE_callback_exec(struct Main *bmain,
                       struct PointerRNA **pointers,
                       int num_pointers,
                       eCbEvent evt);
void BKE_callback_exec_null(struct Main *bmain, eCbEvent evt);
void BKE_callback_exec_id(struct Main *bmain, struct ID *id, eCbEvent evt);
void BKE_callback_exec_id_depsgraph(struct Main *bmain,
                                    struct ID *id,
                                    struct Depsgraph *depsgraph,
                                    eCbEvent evt);
void BKE_callback_exec_string(struct Main *bmain, eCbEvent evt, const char *str);
void BKE_callback_add(bCallbackFuncStore *funcstore, eCbEvent evt);
void BKE_callback_remove(bCallbackFuncStore *funcstore, eCbEvent evt);

void BKE_callback_global_init(void);
/**
 * Call on application exit.
 */
void BKE_callback_global_finalize(void);

#ifdef __cplusplus
}
#endif
