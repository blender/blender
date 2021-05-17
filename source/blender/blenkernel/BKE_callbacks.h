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
 * Common suffix uses:
 * - ``_PRE/_POST``:
 *   For handling discrete non-interactive events.
 * - ``_INIT/_COMPLETE/_CANCEL``:
 *   For handling jobs (which may in turn cause other handlers to be called).
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
  BKE_CB_EVT_SAVE_PRE,
  BKE_CB_EVT_SAVE_POST,
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
  BKE_CB_EVT_TOT,
} eCbEvent;

typedef struct bCallbackFuncStore {
  struct bCallbackFuncStore *next, *prev;
  void (*func)(struct Main *, struct PointerRNA **, const int num_pointers, void *arg);
  void *arg;
  short alloc;
} bCallbackFuncStore;

void BKE_callback_exec(struct Main *bmain,
                       struct PointerRNA **pointers,
                       const int num_pointers,
                       eCbEvent evt);
void BKE_callback_exec_null(struct Main *bmain, eCbEvent evt);
void BKE_callback_exec_id(struct Main *bmain, struct ID *id, eCbEvent evt);
void BKE_callback_exec_id_depsgraph(struct Main *bmain,
                                    struct ID *id,
                                    struct Depsgraph *depsgraph,
                                    eCbEvent evt);
void BKE_callback_add(bCallbackFuncStore *funcstore, eCbEvent evt);

void BKE_callback_global_init(void);
void BKE_callback_global_finalize(void);

#ifdef __cplusplus
}
#endif
