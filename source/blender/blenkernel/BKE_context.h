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
struct Image;
struct ImBuf;
struct wmWindow;
struct wmWindowManager;

/* Structs */

struct bContext;
typedef struct bContext bContext;

typedef void bContextDataMember;

struct bContextDataResult;
typedef struct bContextDataResult bContextDataResult;

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

/* Data Context

   - note: listbases consist of LinkData items and must be
     freed with BLI_freelistN! */

void CTX_data_pointer_set(bContextDataResult *result, void *data);
void CTX_data_list_add(bContextDataResult *result, void *data);

#define CTX_DATA_BEGIN(C, Type, instance, member) \
	{ \
		ListBase ctx_data_list; \
		LinkData *link; \
		CTX_data_##member(C, &ctx_data_list); \
		for(link=ctx_data_list.first; link; link=link->next) { \
			Type instance= link->data;

#define CTX_DATA_END \
		} \
		BLI_freelistN(&ctx_data_list); \
	}

#define CTX_DATA_COUNT(C, member, i) \
	CTX_DATA_BEGIN(C, void*, unused, member) \
		i++; \
	CTX_DATA_END

/* Data Context Members */

struct Main *CTX_data_main(const bContext *C);
struct Scene *CTX_data_scene(const bContext *C);
struct ToolSettings *CTX_data_tool_settings(const bContext *C);

void CTX_data_main_set(bContext *C, struct Main *bmain);
void CTX_data_scene_set(bContext *C, struct Scene *bmain);

int CTX_data_selected_objects(const bContext *C, ListBase *list);
int CTX_data_selected_bases(const bContext *C, ListBase *list);

int CTX_data_visible_objects(const bContext *C, ListBase *list);
int CTX_data_visible_bases(const bContext *C, ListBase *list);

struct Object *CTX_data_active_object(const bContext *C);
struct Base *CTX_data_active_base(const bContext *C);
struct Object *CTX_data_edit_object(const bContext *C);

struct Image *CTX_data_edit_image(const bContext *C);
struct ImBuf *CTX_data_edit_image_buffer(const bContext *C);

int CTX_data_selected_nodes(const bContext *C, ListBase *list);

/* Data Evaluation Context */

float CTX_eval_frame(const bContext *C);

int CTX_eval_render_resolution(const bContext *C);
void CTX_eval_render_resolution_set(bContext *C, int render);

#ifdef __cplusplus
}
#endif
	
#endif

