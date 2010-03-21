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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_object_types.h"

#include "RNA_access.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_screen.h"

#ifndef DISABLE_PYTHON
#include "BPY_extern.h"
#endif

#include <string.h>

/* struct */

struct bContext {
	int thread;

	/* windowmanager context */
	struct {
		struct wmWindowManager *manager;
		struct wmWindow *window;
		struct bScreen *screen;
		struct ScrArea *area;
		struct ARegion *region;
		struct ARegion *menu;
		struct bContextStore *store;
	} wm;
	
	/* data context */
	struct {
		struct Main *main;
		struct Scene *scene;

		int recursion;
		int py_init; /* true if python is initialized */
		void *py_context;
	} data;
	
	/* data evaluation */
	struct {
		int render;
	} eval;
};

/* context */

bContext *CTX_create()
{
	bContext *C;
	
	C= MEM_callocN(sizeof(bContext), "bContext");

	return C;
}

bContext *CTX_copy(const bContext *C)
{
	bContext *newC= MEM_dupallocN((void*)C);

	return newC;
}

void CTX_free(bContext *C)
{
	MEM_freeN(C);
}

/* store */

bContextStore *CTX_store_add(ListBase *contexts, char *name, PointerRNA *ptr)
{
	bContextStoreEntry *entry;
	bContextStore *ctx, *lastctx;

	/* ensure we have a context to put the entry in, if it was already used
	 * we have to copy the context to ensure */
	ctx= contexts->last;

	if(!ctx || ctx->used) {
		if(ctx) {
			lastctx= ctx;
			ctx= MEM_dupallocN(lastctx);
			BLI_duplicatelist(&ctx->entries, &lastctx->entries);
		}
		else
			ctx= MEM_callocN(sizeof(bContextStore), "bContextStore");

		BLI_addtail(contexts, ctx);
	}

	entry= MEM_callocN(sizeof(bContextStoreEntry), "bContextStoreEntry");
	BLI_strncpy(entry->name, name, sizeof(entry->name));
	entry->ptr= *ptr;

	BLI_addtail(&ctx->entries, entry);

	return ctx;
}

void CTX_store_set(bContext *C, bContextStore *store)
{
	C->wm.store= store;
}

bContextStore *CTX_store_copy(bContextStore *store)
{
	bContextStore *ctx;

	ctx= MEM_dupallocN(store);
	BLI_duplicatelist(&ctx->entries, &store->entries);

	return ctx;
}

void CTX_store_free(bContextStore *store)
{
	BLI_freelistN(&store->entries);
	MEM_freeN(store);
}

void CTX_store_free_list(ListBase *contexts)
{
	bContextStore *ctx;

	while((ctx= contexts->first)) {
		BLI_remlink(contexts, ctx);
		CTX_store_free(ctx);
	}
}

/* is python initialied? */
int CTX_py_init_get(bContext *C)
{
	return C->data.py_init;
}
void CTX_py_init_set(bContext *C, int value)
{
	C->data.py_init= value;
}

void *CTX_py_dict_get(bContext *C)
{
	return C->data.py_context;
}
void CTX_py_dict_set(bContext *C, void *value)
{
	C->data.py_context= value;
}

/* window manager context */

wmWindowManager *CTX_wm_manager(const bContext *C)
{
	return C->wm.manager;
}

wmWindow *CTX_wm_window(const bContext *C)
{
	return C->wm.window;
}

bScreen *CTX_wm_screen(const bContext *C)
{
	return C->wm.screen;
}

ScrArea *CTX_wm_area(const bContext *C)
{
	return C->wm.area;
}

SpaceLink *CTX_wm_space_data(const bContext *C)
{
	return (C->wm.area)? C->wm.area->spacedata.first: NULL;
}

ARegion *CTX_wm_region(const bContext *C)
{
	return C->wm.region;
}

void *CTX_wm_region_data(const bContext *C)
{
	return (C->wm.region)? C->wm.region->regiondata: NULL;
}

struct ARegion *CTX_wm_menu(const bContext *C)
{
	return C->wm.menu;
}

struct ReportList *CTX_wm_reports(const bContext *C)
{
	if (C->wm.manager)
		return &(C->wm.manager->reports);

	return NULL;
}

View3D *CTX_wm_view3d(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_VIEW3D)
		return C->wm.area->spacedata.first;
	return NULL;
}

RegionView3D *CTX_wm_region_view3d(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_VIEW3D)
		if(C->wm.region)
			return C->wm.region->regiondata;
	return NULL;
}

struct SpaceText *CTX_wm_space_text(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_TEXT)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceConsole *CTX_wm_space_console(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_CONSOLE)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceImage *CTX_wm_space_image(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_IMAGE)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceButs *CTX_wm_space_buts(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_BUTS)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceFile *CTX_wm_space_file(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_FILE)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceSeq *CTX_wm_space_seq(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_SEQ)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceOops *CTX_wm_space_outliner(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_OUTLINER)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceNla *CTX_wm_space_nla(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_NLA)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceTime *CTX_wm_space_time(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_TIME)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceNode *CTX_wm_space_node(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_NODE)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceLogic *CTX_wm_space_logic(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_LOGIC)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceIpo *CTX_wm_space_graph(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_IPO)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceAction *CTX_wm_space_action(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_ACTION)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceInfo *CTX_wm_space_info(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_INFO)
		return C->wm.area->spacedata.first;
	return NULL;
}

struct SpaceUserPref *CTX_wm_space_userpref(const bContext *C)
{
	if(C->wm.area && C->wm.area->spacetype==SPACE_USERPREF)
		return C->wm.area->spacedata.first;
	return NULL;
}

void CTX_wm_manager_set(bContext *C, wmWindowManager *wm)
{
	C->wm.manager= wm;
	C->wm.window= NULL;
	C->wm.screen= NULL;
	C->wm.area= NULL;
	C->wm.region= NULL;
}

void CTX_wm_window_set(bContext *C, wmWindow *win)
{
	C->wm.window= win;
	C->wm.screen= (win)? win->screen: NULL;
	C->data.scene= (C->wm.screen)? C->wm.screen->scene: NULL;
	C->wm.area= NULL;
	C->wm.region= NULL;
}

void CTX_wm_screen_set(bContext *C, bScreen *screen)
{
	C->wm.screen= screen;
	C->data.scene= (C->wm.screen)? C->wm.screen->scene: NULL;
	C->wm.area= NULL;
	C->wm.region= NULL;
}

void CTX_wm_area_set(bContext *C, ScrArea *area)
{
	C->wm.area= area;
	C->wm.region= NULL;
}

void CTX_wm_region_set(bContext *C, ARegion *region)
{
	C->wm.region= region;
}

void CTX_wm_menu_set(bContext *C, ARegion *menu)
{
	C->wm.menu= menu;
}

/* data context utility functions */

struct bContextDataResult {
	PointerRNA ptr;
	ListBase list;
	const char **dir;
};

static int ctx_data_get(bContext *C, const char *member, bContextDataResult *result)
{
	int done= 0, recursion= C->data.recursion;
	int ret= 0;

	memset(result, 0, sizeof(bContextDataResult));
#ifndef DISABLE_PYTHON
	if(CTX_py_dict_get(C)) {
		return BPY_context_get(C, member, result);
//		if (BPY_context_get(C, member, result))
//			return 1;
	}
#endif
	/* we check recursion to ensure that we do not get infinite
	 * loops requesting data from ourselfs in a context callback */

	/* Ok, this looks evil...
	 * if(ret) done= -(-ret | -done);
	 *
	 * Values in order of importance
	 * (0, -1, 1) - Where 1 is highest priority
	 * */
	if(done!=1 && recursion < 1 && C->wm.store) {
		bContextStoreEntry *entry;

		C->data.recursion= 1;

		for(entry=C->wm.store->entries.first; entry; entry=entry->next) {
			if(strcmp(entry->name, member) == 0) {
				result->ptr= entry->ptr;
				done= 1;
			}
		}
	}
	if(done!=1 && recursion < 2 && C->wm.region) {
		C->data.recursion= 2;
		if(C->wm.region->type && C->wm.region->type->context) {
			ret = C->wm.region->type->context(C, member, result);
			if(ret) done= -(-ret | -done);

		}
	}
	if(done!=1 && recursion < 3 && C->wm.area) {
		C->data.recursion= 3;
		if(C->wm.area->type && C->wm.area->type->context) {
			ret = C->wm.area->type->context(C, member, result);
			if(ret) done= -(-ret | -done);
		}
	}
	if(done!=1 && recursion < 4 && C->wm.screen) {
		bContextDataCallback cb= C->wm.screen->context;
		C->data.recursion= 4;
		if(cb) {
			ret = cb(C, member, result);
			if(ret) done= -(-ret | -done);
		}
	}

	C->data.recursion= recursion;

	return done;
}

static void *ctx_data_pointer_get(const bContext *C, const char *member)
{
	bContextDataResult result;

	if(C && ctx_data_get((bContext*)C, member, &result)==1)
		return result.ptr.data;

	return NULL;
}

static int ctx_data_pointer_verify(const bContext *C, const char *member, void **pointer)
{
	bContextDataResult result;

	/* if context is NULL, pointer must be NULL too and that is a valid return */
	if (C == NULL) {
		*pointer= NULL;
		return 1;
	}
	else if(ctx_data_get((bContext*)C, member, &result)==1) {
		*pointer= result.ptr.data;
		return 1;
	}
	else {
		*pointer= NULL;
		return 0;
	}
}

static int ctx_data_collection_get(const bContext *C, const char *member, ListBase *list)
{
	bContextDataResult result;

	if(ctx_data_get((bContext*)C, member, &result)==1) {
		*list= result.list;
		return 1;
	}

	list->first= NULL;
	list->last= NULL;

	return 0;
}

PointerRNA CTX_data_pointer_get(const bContext *C, const char *member)
{
	bContextDataResult result;

	if(ctx_data_get((bContext*)C, member, &result)==1)
		return result.ptr;
	else
		return PointerRNA_NULL;
}

PointerRNA CTX_data_pointer_get_type(const bContext *C, const char *member, StructRNA *type)
{
	PointerRNA ptr = CTX_data_pointer_get(C, member);

	if(ptr.data && RNA_struct_is_a(ptr.type, type))
		return ptr;
	
	return PointerRNA_NULL;
}

ListBase CTX_data_collection_get(const bContext *C, const char *member)
{
	bContextDataResult result;

	if(ctx_data_get((bContext*)C, member, &result)==1) {
		return result.list;
	}
	else {
		ListBase list;
		memset(&list, 0, sizeof(list));
		return list;
	}
}

/* 1:found,  -1:found but not set,  0:not found */
int CTX_data_get(const bContext *C, const char *member, PointerRNA *r_ptr, ListBase *r_lb)
{
	bContextDataResult result;
	int ret= ctx_data_get((bContext*)C, member, &result);

	if(ret==1) {
		*r_ptr= result.ptr;
		*r_lb= result.list;
	}
	else {
		memset(r_ptr, 0, sizeof(*r_ptr));
		memset(r_lb, 0, sizeof(*r_lb));
	}

	return ret;
}

static void data_dir_add(ListBase *lb, const char *member)
{
	LinkData *link;

	if(strcmp(member, "scene") == 0) /* exception */
		return;

	for(link=lb->first; link; link=link->next)
		if(strcmp(link->data, member) == 0)
			return;
	
	link= MEM_callocN(sizeof(LinkData), "LinkData");
	link->data= (void*)member;
	BLI_addtail(lb, link);
}

ListBase CTX_data_dir_get(const bContext *C)
{
	bContextDataResult result;
	ListBase lb;
	int a;

	memset(&lb, 0, sizeof(lb));

	if(C->wm.store) {
		bContextStoreEntry *entry;

		for(entry=C->wm.store->entries.first; entry; entry=entry->next)
			data_dir_add(&lb, entry->name);
	}
	if(C->wm.region && C->wm.region->type && C->wm.region->type->context) {
		memset(&result, 0, sizeof(result));
		C->wm.region->type->context(C, "", &result);

		if(result.dir)
			for(a=0; result.dir[a]; a++)
				data_dir_add(&lb, result.dir[a]);
	}
	if(C->wm.area && C->wm.area->type && C->wm.area->type->context) {
		memset(&result, 0, sizeof(result));
		C->wm.area->type->context(C, "", &result);

		if(result.dir)
			for(a=0; result.dir[a]; a++)
				data_dir_add(&lb, result.dir[a]);
	}
	if(C->wm.screen && C->wm.screen->context) {
		bContextDataCallback cb= C->wm.screen->context;
		memset(&result, 0, sizeof(result));
		cb(C, "", &result);

		if(result.dir)
			for(a=0; result.dir[a]; a++)
				data_dir_add(&lb, result.dir[a]);
	}

	return lb;
}

int CTX_data_equals(const char *member, const char *str)
{
	return (strcmp(member, str) == 0);
}

int CTX_data_dir(const char *member)
{
	return (strcmp(member, "") == 0);
}

void CTX_data_id_pointer_set(bContextDataResult *result, ID *id)
{
	RNA_id_pointer_create(id, &result->ptr);
}

void CTX_data_pointer_set(bContextDataResult *result, ID *id, StructRNA *type, void *data)
{
	RNA_pointer_create(id, type, data, &result->ptr);
}

void CTX_data_id_list_add(bContextDataResult *result, ID *id)
{
	CollectionPointerLink *link;

	link= MEM_callocN(sizeof(CollectionPointerLink), "CTX_data_id_list_add");
	RNA_id_pointer_create(id, &link->ptr);

	BLI_addtail(&result->list, link);
}

void CTX_data_list_add(bContextDataResult *result, ID *id, StructRNA *type, void *data)
{
	CollectionPointerLink *link;

	link= MEM_callocN(sizeof(CollectionPointerLink), "CTX_data_list_add");
	RNA_pointer_create(id, type, data, &link->ptr);

	BLI_addtail(&result->list, link);
}

int ctx_data_list_count(const bContext *C, int (*func)(const bContext*, ListBase*))
{
	ListBase list;

	if(func(C, &list)) {
		int tot= BLI_countlist(&list);
		BLI_freelistN(&list);
		return tot;
	}
	else
		return 0;
}

void CTX_data_dir_set(bContextDataResult *result, const char **dir)
{
	result->dir= dir;
}

/* data context */

Main *CTX_data_main(const bContext *C)
{
	Main *bmain;

	if(ctx_data_pointer_verify(C, "main", (void*)&bmain))
		return bmain;
	else
		return C->data.main;
}

void CTX_data_main_set(bContext *C, Main *bmain)
{
	C->data.main= bmain;
}

Scene *CTX_data_scene(const bContext *C)
{
	Scene *scene;

	if(ctx_data_pointer_verify(C, "scene", (void*)&scene))
		return scene;
	else
		return C->data.scene;
}

int CTX_data_mode_enum(const bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);

	if(obedit) {
		switch(obedit->type) {
			case OB_MESH:
				return CTX_MODE_EDIT_MESH;
			case OB_CURVE:
				return CTX_MODE_EDIT_CURVE;
			case OB_SURF:
				return CTX_MODE_EDIT_SURFACE;
			case OB_FONT:
				return CTX_MODE_EDIT_TEXT;
			case OB_ARMATURE:
				return CTX_MODE_EDIT_ARMATURE;
			case OB_MBALL:
				return CTX_MODE_EDIT_METABALL;
			case OB_LATTICE:
				return CTX_MODE_EDIT_LATTICE;
		}
	}
	else {
		Object *ob = CTX_data_active_object(C);

		if(ob) {
			if(ob->mode & OB_MODE_POSE) return CTX_MODE_POSE;
			else if(ob->mode & OB_MODE_SCULPT)  return CTX_MODE_SCULPT;
			else if(ob->mode & OB_MODE_WEIGHT_PAINT) return CTX_MODE_PAINT_WEIGHT;
			else if(ob->mode & OB_MODE_VERTEX_PAINT) return CTX_MODE_PAINT_VERTEX;
			else if(ob->mode & OB_MODE_TEXTURE_PAINT) return CTX_MODE_PAINT_TEXTURE;
			else if(ob->mode & OB_MODE_PARTICLE_EDIT) return CTX_MODE_PARTICLE;
		}
	}

	return CTX_MODE_OBJECT;
}


/* would prefer if we can use the enum version below over this one - Campbell */
/* must be aligned with above enum  */
static char *data_mode_strings[] = {
	"mesh_edit",
	"curve_edit",
	"surface_edit",
	"text_edit",
	"armature_edit",
	"mball_edit",
	"lattice_edit",
	"posemode",
	"sculpt_mode",
	"weightpaint",
	"vertexpaint",
	"texturepaint",
	"particlemode",
	"objectmode",
	0
};
char *CTX_data_mode_string(const bContext *C)
{
	return data_mode_strings[CTX_data_mode_enum(C)];
}

void CTX_data_scene_set(bContext *C, Scene *scene)
{
	C->data.scene= scene;
}

ToolSettings *CTX_data_tool_settings(const bContext *C)
{
	Scene *scene = CTX_data_scene(C);

	if(scene)
		return scene->toolsettings;
	else
		return NULL;
}

int CTX_data_selected_nodes(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "selected_nodes", list);
}

int CTX_data_selected_editable_objects(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "selected_editable_objects", list);
}

int CTX_data_selected_editable_bases(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "selected_editable_bases", list);
}

int CTX_data_selected_objects(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "selected_objects", list);
}

int CTX_data_selected_bases(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "selected_bases", list);
}

int CTX_data_visible_objects(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "visible_objects", list);
}

int CTX_data_visible_bases(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "visible_bases", list);
}

int CTX_data_selectable_objects(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "selectable_objects", list);
}

int CTX_data_selectable_bases(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "selectable_bases", list);
}

struct Object *CTX_data_active_object(const bContext *C)
{
	return ctx_data_pointer_get(C, "active_object");
}

struct Base *CTX_data_active_base(const bContext *C)
{
	return ctx_data_pointer_get(C, "active_base");
}

struct Object *CTX_data_edit_object(const bContext *C)
{
	return ctx_data_pointer_get(C, "edit_object");
}

struct Image *CTX_data_edit_image(const bContext *C)
{
	return ctx_data_pointer_get(C, "edit_image");
}

struct Text *CTX_data_edit_text(const bContext *C)
{
	return ctx_data_pointer_get(C, "edit_text");
}

struct EditBone *CTX_data_active_bone(const bContext *C)
{
	return ctx_data_pointer_get(C, "active_bone");
}

int CTX_data_selected_bones(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "selected_bones", list);
}

int CTX_data_selected_editable_bones(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "selected_editable_bones", list);
}

int CTX_data_visible_bones(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "visible_bones", list);
}

int CTX_data_editable_bones(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "editable_bones", list);
}

struct bPoseChannel *CTX_data_active_pose_bone(const bContext *C)
{
	return ctx_data_pointer_get(C, "active_pose_bone");
}

int CTX_data_selected_pose_bones(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "selected_pose_bones", list);
}

int CTX_data_visible_pose_bones(const bContext *C, ListBase *list)
{
	return ctx_data_collection_get(C, "visible_pose_bones", list);
}

