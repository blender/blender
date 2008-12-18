/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_CONTEXT_H
#define BKE_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_listBase.h"

struct ARegion;
struct bScreen;
struct EditMesh;
struct ListBase;
struct Main;
struct Object;
struct PointerRNA;
struct ReportList;
struct Scene;
struct ScrArea;
struct SpaceLink;
struct StructRNA;
struct ToolSettings;
struct wmWindow;
struct wmWindowManager;

/* Structs */

struct bContext;
typedef struct bContext bContext;

struct bContextDataMember;
typedef struct bContextDataMember bContextDataMember;

extern bContextDataMember CTX_DataMain;
extern bContextDataMember CTX_DataScene;
extern bContextDataMember CTX_DataObjects;
extern bContextDataMember CTX_DataEditObject;
extern bContextDataMember CTX_DataEditArmature;
extern bContextDataMember CTX_DataEditMesh;

typedef struct bContextDataIterator {
	void *data;
	int valid;

	void (*begin)(bContext *C, struct bContextDataIterator *iter);
	void (*next)(struct bContextDataIterator *iter);
	void (*end)(struct bContextDataIterator *iter);
	void *internal;
} bContextDataIterator;

typedef struct bContextDataResult {
	void *pointer;
	bContextDataIterator iterator;
} bContextDataResult;

typedef int (*bContextDataCallback)(const bContext *C,
	const bContextDataMember *member, bContextDataResult *result);

/* Context */

bContext *CTX_create(void);
void CTX_free(bContext *C);

bContext *CTX_copy(const bContext *C, int thread);
int CTX_thread(const bContext *C);

/* Context Task and Reports */

typedef enum bContextTask {
	CTX_DRAWING = 0,
	CTX_EDITING = 1,
	CTX_EVALUATING = 2,
	CTX_UNDEFINED = 3
} bContextTask;

bContextTask CTX_task(const bContext *C);
void CTX_task_set(bContext *C, bContextTask task);

struct ReportList *CTX_reports(const bContext *C);
void CTX_reports_set(bContext *C, struct ReportList *reports);

/* Window Manager Context */

struct wmWindowManager *CTX_wm_manager(const bContext *C);
struct wmWindow *CTX_wm_window(const bContext *C);
struct bScreen *CTX_wm_screen(const bContext *C);
struct ScrArea *CTX_wm_area(const bContext *C);
struct SpaceLink *CTX_wm_space_data(const bContext *C);
struct ARegion *CTX_wm_region(const bContext *C);
void *CTX_wm_region_data(const bContext *C);
struct uiBlock *CTX_wm_ui_block(const bContext *C);

void CTX_wm_manager_set(bContext *C, struct wmWindowManager *wm);
void CTX_wm_window_set(bContext *C, struct wmWindow *win);
void CTX_wm_screen_set(bContext *C, struct bScreen *screen); /* to be removed */
void CTX_wm_area_set(bContext *C, struct ScrArea *win);
void CTX_wm_region_set(bContext *C, struct ARegion *win);
void CTX_wm_ui_block_set(bContext *C, struct uiBlock *block, bContextDataCallback cb);

/* Data Context */

struct Main *CTX_data_main(const bContext *C);
struct Scene *CTX_data_scene(const bContext *C);
struct ToolSettings *CTX_data_tool_settings(const bContext *C);

void CTX_data_main_set(bContext *C, struct Main *bmain);
void CTX_data_scene_set(bContext *C, struct Scene *bmain);

int CTX_data_objects(const bContext *C, bContextDataIterator *iter);

struct Object *CTX_data_edit_object(const bContext *C);
struct EditMesh *CTX_data_edit_mesh(const bContext *C);
struct ListBase *CTX_data_edit_armature(const bContext *C);

/* Data Evaluation Context */

float CTX_eval_frame(const bContext *C);

int CTX_eval_render_resolution(const bContext *C);
void CTX_eval_render_resolution_set(bContext *C, int render);

#ifdef __cplusplus
}
#endif
	
#endif

