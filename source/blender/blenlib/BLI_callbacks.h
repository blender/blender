/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BLI_callbacks.h
 *  \ingroup bli
 */

#ifndef __BLI_CALLBACKS_H__
#define __BLI_CALLBACKS_H__

struct Main;
struct ID;

/**
 * Common suffix uses:
 * - ``_PRE/_POST``:
 *   For handling discrete non-interactive events.
 * - ``_INIT/_COMPLETE/_CANCEL``:
 *   For handling jobs (which may in turn cause other handlers to be called).
 */
typedef enum {
	BLI_CB_EVT_FRAME_CHANGE_PRE,
	BLI_CB_EVT_FRAME_CHANGE_POST,
	BLI_CB_EVT_RENDER_PRE,
	BLI_CB_EVT_RENDER_POST,
	BLI_CB_EVT_RENDER_WRITE,
	BLI_CB_EVT_RENDER_STATS,
	BLI_CB_EVT_RENDER_INIT,
	BLI_CB_EVT_RENDER_COMPLETE,
	BLI_CB_EVT_RENDER_CANCEL,
	BLI_CB_EVT_LOAD_PRE,
	BLI_CB_EVT_LOAD_POST,
	BLI_CB_EVT_SAVE_PRE,
	BLI_CB_EVT_SAVE_POST,
	BLI_CB_EVT_UNDO_PRE,
	BLI_CB_EVT_UNDO_POST,
	BLI_CB_EVT_REDO_PRE,
	BLI_CB_EVT_REDO_POST,
	BLI_CB_EVT_VERSION_UPDATE,
	BLI_CB_EVT_TOT
} eCbEvent;


typedef struct bCallbackFuncStore {
	struct bCallbackFuncStore *next, *prev;
	void (*func)(struct Main *, struct ID *, void *arg);
	void *arg;
	short alloc;
} bCallbackFuncStore;


void BLI_callback_exec(struct Main *bmain, struct ID *self, eCbEvent evt);
void BLI_callback_add(bCallbackFuncStore *funcstore, eCbEvent evt);

void BLI_callback_global_init(void);
void BLI_callback_global_finalize(void);

#endif /* __BLI_CALLBACKS_H__ */
