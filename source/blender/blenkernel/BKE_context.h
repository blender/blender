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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_CONTEXT_H__
#define __BKE_CONTEXT_H__

/** \file BKE_context.h
 *  \ingroup bke
 */

#include "DNA_listBase.h"
#include "RNA_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct bScreen;
struct ListBase;
struct Main;
struct Object;
struct PointerRNA;
struct ReportList;
struct Scene;
struct ScrArea;
struct SpaceLink;
struct View3D;
struct RegionView3D;
struct StructRNA;
struct ToolSettings;
struct Image;
struct Text;
struct ImBuf;
struct EditBone;
struct bPoseChannel;
struct wmWindow;
struct wmWindowManager;
struct SpaceText;
struct SpaceImage;
struct SpaceClip;
struct ID;

/* Structs */

struct bContext;
typedef struct bContext bContext;

struct bContextDataResult;
typedef struct bContextDataResult bContextDataResult;

typedef int (*bContextDataCallback)(const bContext *C,
	const char *member, bContextDataResult *result);

typedef struct bContextStoreEntry {
	struct bContextStoreEntry *next, *prev;

	char name[128];
	PointerRNA ptr;
} bContextStoreEntry;

typedef struct bContextStore {
	struct bContextStore *next, *prev;

	ListBase entries;
	int used;
} bContextStore;

/* for the conrtext's rna mode enum
 * keep aligned with data_mode_strings in context.c */
enum {
	CTX_MODE_EDIT_MESH = 0,
	CTX_MODE_EDIT_CURVE,
	CTX_MODE_EDIT_SURFACE,
	CTX_MODE_EDIT_TEXT,
	CTX_MODE_EDIT_ARMATURE,
	CTX_MODE_EDIT_METABALL,
	CTX_MODE_EDIT_LATTICE,
	CTX_MODE_POSE,
	CTX_MODE_SCULPT,
	CTX_MODE_PAINT_WEIGHT,
	CTX_MODE_PAINT_VERTEX,
	CTX_MODE_PAINT_TEXTURE,
	CTX_MODE_PARTICLE,
	CTX_MODE_OBJECT
};

/* Context */

bContext *CTX_create(void);
void CTX_free(bContext *C);

bContext *CTX_copy(const bContext *C);

/* Stored Context */

bContextStore *CTX_store_add(ListBase *contexts, const char *name, PointerRNA *ptr);
void CTX_store_set(bContext *C, bContextStore *store);
bContextStore *CTX_store_copy(bContextStore *store);
void CTX_store_free(bContextStore *store);
void CTX_store_free_list(ListBase *contexts);

/* need to store if python is initialized or not */
int CTX_py_init_get(bContext *C);
void CTX_py_init_set(bContext *C, int value);

void *CTX_py_dict_get(const bContext *C);
void CTX_py_dict_set(bContext *C, void *value);

/* Window Manager Context */

struct wmWindowManager *CTX_wm_manager(const bContext *C);
struct wmWindow *CTX_wm_window(const bContext *C);
struct bScreen *CTX_wm_screen(const bContext *C);
struct ScrArea *CTX_wm_area(const bContext *C);
struct SpaceLink *CTX_wm_space_data(const bContext *C);
struct ARegion *CTX_wm_region(const bContext *C);
void *CTX_wm_region_data(const bContext *C);
struct ARegion *CTX_wm_menu(const bContext *C);
struct ReportList *CTX_wm_reports(const bContext *C);

struct View3D *CTX_wm_view3d(const bContext *C);
struct RegionView3D *CTX_wm_region_view3d(const bContext *C);
struct SpaceText *CTX_wm_space_text(const bContext *C);
struct SpaceImage *CTX_wm_space_image(const bContext *C);
struct SpaceConsole *CTX_wm_space_console(const bContext *C);
struct SpaceButs *CTX_wm_space_buts(const bContext *C);
struct SpaceFile *CTX_wm_space_file(const bContext *C);
struct SpaceSeq *CTX_wm_space_seq(const bContext *C);
struct SpaceOops *CTX_wm_space_outliner(const bContext *C);
struct SpaceNla *CTX_wm_space_nla(const bContext *C);
struct SpaceTime *CTX_wm_space_time(const bContext *C);
struct SpaceNode *CTX_wm_space_node(const bContext *C);
struct SpaceLogic *CTX_wm_space_logic(const bContext *C);
struct SpaceIpo *CTX_wm_space_graph(const bContext *C);
struct SpaceAction *CTX_wm_space_action(const bContext *C);
struct SpaceInfo *CTX_wm_space_info(const bContext *C);
struct SpaceUserPref *CTX_wm_space_userpref(const bContext *C);
struct SpaceClip *CTX_wm_space_clip(const bContext *C);

void CTX_wm_manager_set(bContext *C, struct wmWindowManager *wm);
void CTX_wm_window_set(bContext *C, struct wmWindow *win);
void CTX_wm_screen_set(bContext *C, struct bScreen *screen); /* to be removed */
void CTX_wm_area_set(bContext *C, struct ScrArea *sa);
void CTX_wm_region_set(bContext *C, struct ARegion *region);
void CTX_wm_menu_set(bContext *C, struct ARegion *menu);
const char *CTX_wm_operator_poll_msg_get(struct bContext *C);
void CTX_wm_operator_poll_msg_set(struct bContext *C, const char *msg);

/* Data Context

   - listbases consist of CollectionPointerLink items and must be
	 freed with BLI_freelistN!
   - the dir listbase consits of LinkData items */

/* data type, needed so we can tell between a NULL pointer and an empty list */
enum {
	CTX_DATA_TYPE_POINTER = 0,
	CTX_DATA_TYPE_COLLECTION
};

PointerRNA CTX_data_pointer_get(const bContext *C, const char *member);
PointerRNA CTX_data_pointer_get_type(const bContext *C, const char *member, StructRNA *type);
ListBase CTX_data_collection_get(const bContext *C, const char *member);
ListBase CTX_data_dir_get(const bContext *C);
int CTX_data_get(const bContext *C, const char *member, PointerRNA *r_ptr, ListBase *r_lb, short *r_type);

void CTX_data_id_pointer_set(bContextDataResult *result, struct ID *id);
void CTX_data_pointer_set(bContextDataResult *result, struct ID *id, StructRNA *type, void *data);

void CTX_data_id_list_add(bContextDataResult *result, struct ID *id);
void CTX_data_list_add(bContextDataResult *result, struct ID *id, StructRNA *type, void *data);

void CTX_data_dir_set(bContextDataResult *result, const char **member);

void CTX_data_type_set(struct bContextDataResult *result, short type);
short CTX_data_type_get(struct bContextDataResult *result);

int CTX_data_equals(const char *member, const char *str);
int CTX_data_dir(const char *member);

/*void CTX_data_pointer_set(bContextDataResult *result, void *data);
void CTX_data_list_add(bContextDataResult *result, void *data);*/

#define CTX_DATA_BEGIN(C, Type, instance, member)                             \
	{                                                                         \
		ListBase ctx_data_list;                                               \
		CollectionPointerLink *ctx_link;                                      \
		CTX_data_##member(C, &ctx_data_list);                                 \
		for(ctx_link=ctx_data_list.first; ctx_link; ctx_link=ctx_link->next) {\
			Type instance= ctx_link->ptr.data;

#define CTX_DATA_END                                                          \
		}                                                                     \
		BLI_freelistN(&ctx_data_list);                                        \
	}

int ctx_data_list_count(const bContext *C, int (*func)(const bContext*, ListBase*));

#define CTX_DATA_COUNT(C, member) \
	ctx_data_list_count(C, CTX_data_##member)

/* Data Context Members */

struct Main *CTX_data_main(const bContext *C);
struct Scene *CTX_data_scene(const bContext *C);
struct ToolSettings *CTX_data_tool_settings(const bContext *C);

const char *CTX_data_mode_string(const bContext *C);
int CTX_data_mode_enum(const bContext *C);

void CTX_data_main_set(bContext *C, struct Main *bmain);
void CTX_data_scene_set(bContext *C, struct Scene *bmain);

int CTX_data_selected_editable_objects(const bContext *C, ListBase *list);
int CTX_data_selected_editable_bases(const bContext *C, ListBase *list);

int CTX_data_selected_objects(const bContext *C, ListBase *list);
int CTX_data_selected_bases(const bContext *C, ListBase *list);

int CTX_data_visible_objects(const bContext *C, ListBase *list);
int CTX_data_visible_bases(const bContext *C, ListBase *list);

int CTX_data_selectable_objects(const bContext *C, ListBase *list);
int CTX_data_selectable_bases(const bContext *C, ListBase *list);

struct Object *CTX_data_active_object(const bContext *C);
struct Base *CTX_data_active_base(const bContext *C);
struct Object *CTX_data_edit_object(const bContext *C);

struct Image *CTX_data_edit_image(const bContext *C);

struct Text *CTX_data_edit_text(const bContext *C);
struct MovieClip *CTX_data_edit_movieclip(const bContext *C);

int CTX_data_selected_nodes(const bContext *C, ListBase *list);

struct EditBone *CTX_data_active_bone(const bContext *C);
int CTX_data_selected_bones(const bContext *C, ListBase *list);
int CTX_data_selected_editable_bones(const bContext *C, ListBase *list);
int CTX_data_visible_bones(const bContext *C, ListBase *list);
int CTX_data_editable_bones(const bContext *C, ListBase *list);

struct bPoseChannel *CTX_data_active_pose_bone(const bContext *C);
int CTX_data_selected_pose_bones(const bContext *C, ListBase *list);
int CTX_data_visible_pose_bones(const bContext *C, ListBase *list);

#ifdef __cplusplus
}
#endif
	
#endif

