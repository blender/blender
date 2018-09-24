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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_files_link.c
 *  \ingroup wm
 *
 * Functions for dealing with append/link operators and helpers.
 */


#include <float.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"



#include "BLI_blenlib.h"
#include "BLI_bitmap.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BLO_readfile.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_library.h"
#include "BKE_library_remap.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "BKE_idcode.h"


#include "IMB_colormanagement.h"

#include "ED_screen.h"

#include "GPU_material.h"

#include "RNA_access.h"
#include "RNA_define.h"


#include "WM_api.h"
#include "WM_types.h"

#include "wm_files.h"

/* **************** link/append *************** */

static bool wm_link_append_poll(bContext *C)
{
	if (WM_operator_winactive(C)) {
		/* linking changes active object which is pretty useful in general,
		 * but which totally confuses edit mode (i.e. it becoming not so obvious
		 * to leave from edit mode and invalid tools in toolbar might be displayed)
		 * so disable link/append when in edit mode (sergey) */
		if (CTX_data_edit_object(C))
			return 0;

		return 1;
	}

	return 0;
}

static int wm_link_append_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		return WM_operator_call_notest(C, op);
	}
	else {
		/* XXX TODO solve where to get last linked library from */
		if (G.lib[0] != '\0') {
			RNA_string_set(op->ptr, "filepath", G.lib);
		}
		else if (G.relbase_valid) {
			char path[FILE_MAX];
			BLI_strncpy(path, BKE_main_blendfile_path_from_global(), sizeof(path));
			BLI_parent_dir(path);
			RNA_string_set(op->ptr, "filepath", path);
		}
		WM_event_add_fileselect(C, op);
		return OPERATOR_RUNNING_MODAL;
	}
}

static short wm_link_append_flag(wmOperator *op)
{
	PropertyRNA *prop;
	short flag = 0;

	if (RNA_boolean_get(op->ptr, "autoselect"))
		flag |= FILE_AUTOSELECT;
	if (RNA_boolean_get(op->ptr, "active_layer"))
		flag |= FILE_ACTIVELAY;
	if ((prop = RNA_struct_find_property(op->ptr, "relative_path")) && RNA_property_boolean_get(op->ptr, prop))
		flag |= FILE_RELPATH;
	if (RNA_boolean_get(op->ptr, "link"))
		flag |= FILE_LINK;
	if (RNA_boolean_get(op->ptr, "instance_groups"))
		flag |= FILE_GROUP_INSTANCE;

	return flag;
}

typedef struct WMLinkAppendDataItem {
	char *name;
	BLI_bitmap *libraries;  /* All libs (from WMLinkAppendData.libraries) to try to load this ID from. */
	short idcode;

	ID *new_id;
	void *customdata;
} WMLinkAppendDataItem;

typedef struct WMLinkAppendData {
	LinkNodePair libraries;
	LinkNodePair items;
	int num_libraries;
	int num_items;
	int flag;  /* Combines eFileSel_Params_Flag from DNA_space_types.h and BLO_LibLinkFlags from BLO_readfile.h */

	/* Internal 'private' data */
	MemArena *memarena;
} WMLinkAppendData;

static WMLinkAppendData *wm_link_append_data_new(const int flag)
{
	MemArena *ma = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
	WMLinkAppendData *lapp_data = BLI_memarena_calloc(ma, sizeof(*lapp_data));

	lapp_data->flag = flag;
	lapp_data->memarena = ma;

	return lapp_data;
}

static void wm_link_append_data_free(WMLinkAppendData *lapp_data)
{
	BLI_memarena_free(lapp_data->memarena);
}

/* WARNING! *Never* call wm_link_append_data_library_add() after having added some items! */

static void wm_link_append_data_library_add(WMLinkAppendData *lapp_data, const char *libname)
{
	size_t len = strlen(libname) + 1;
	char *libpath = BLI_memarena_alloc(lapp_data->memarena, len);

	BLI_strncpy(libpath, libname, len);
	BLI_linklist_append_arena(&lapp_data->libraries, libpath, lapp_data->memarena);
	lapp_data->num_libraries++;
}

static WMLinkAppendDataItem *wm_link_append_data_item_add(
        WMLinkAppendData *lapp_data, const char *idname, const short idcode, void *customdata)
{
	WMLinkAppendDataItem *item = BLI_memarena_alloc(lapp_data->memarena, sizeof(*item));
	size_t len = strlen(idname) + 1;

	item->name = BLI_memarena_alloc(lapp_data->memarena, len);
	BLI_strncpy(item->name, idname, len);
	item->idcode = idcode;
	item->libraries = BLI_BITMAP_NEW_MEMARENA(lapp_data->memarena, lapp_data->num_libraries);

	item->new_id = NULL;
	item->customdata = customdata;

	BLI_linklist_append_arena(&lapp_data->items, item, lapp_data->memarena);
	lapp_data->num_items++;

	return item;
}

static void wm_link_do(WMLinkAppendData *lapp_data, ReportList *reports, Main *bmain, Scene *scene, View3D *v3d)
{
	Main *mainl;
	BlendHandle *bh;
	Library *lib;

	const int flag = lapp_data->flag;

	LinkNode *liblink, *itemlink;
	int lib_idx, item_idx;

	BLI_assert(lapp_data->num_items && lapp_data->num_libraries);

	for (lib_idx = 0, liblink = lapp_data->libraries.list; liblink; lib_idx++, liblink = liblink->next) {
		char *libname = liblink->link;

		bh = BLO_blendhandle_from_file(libname, reports);

		if (bh == NULL) {
			/* Unlikely since we just browsed it, but possible
			 * Error reports will have been made by BLO_blendhandle_from_file() */
			continue;
		}

		/* here appending/linking starts */
		mainl = BLO_library_link_begin(bmain, &bh, libname);
		lib = mainl->curlib;
		BLI_assert(lib);
		UNUSED_VARS_NDEBUG(lib);

		if (mainl->versionfile < 250) {
			BKE_reportf(reports, RPT_WARNING,
			            "Linking or appending from a very old .blend file format (%d.%d), no animation conversion will "
			            "be done! You may want to re-save your lib file with current Blender",
			            mainl->versionfile, mainl->subversionfile);
		}

		/* For each lib file, we try to link all items belonging to that lib,
		 * and tag those successful to not try to load them again with the other libs. */
		for (item_idx = 0, itemlink = lapp_data->items.list; itemlink; item_idx++, itemlink = itemlink->next) {
			WMLinkAppendDataItem *item = itemlink->link;
			ID *new_id;

			if (!BLI_BITMAP_TEST(item->libraries, lib_idx)) {
				continue;
			}

			new_id = BLO_library_link_named_part_ex(mainl, &bh, item->idcode, item->name, flag, scene, v3d);

			if (new_id) {
				/* If the link is successful, clear item's libs 'todo' flags.
				 * This avoids trying to link same item with other libraries to come. */
				BLI_BITMAP_SET_ALL(item->libraries, false, lapp_data->num_libraries);
				item->new_id = new_id;
			}
		}

		BLO_library_link_end(mainl, &bh, flag, scene, v3d);
		BLO_blendhandle_close(bh);
	}
}

static int wm_link_append_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	PropertyRNA *prop;
	WMLinkAppendData *lapp_data;
	char path[FILE_MAX_LIBEXTRA], root[FILE_MAXDIR], libname[FILE_MAX_LIBEXTRA], relname[FILE_MAX];
	char *group, *name;
	int totfiles = 0;

	RNA_string_get(op->ptr, "filename", relname);
	RNA_string_get(op->ptr, "directory", root);

	BLI_join_dirfile(path, sizeof(path), root, relname);

	/* test if we have a valid data */
	if (!BLO_library_path_explode(path, libname, &group, &name)) {
		BKE_reportf(op->reports, RPT_ERROR, "'%s': not a library", path);
		return OPERATOR_CANCELLED;
	}
	else if (!group) {
		BKE_reportf(op->reports, RPT_ERROR, "'%s': nothing indicated", path);
		return OPERATOR_CANCELLED;
	}
	else if (BLI_path_cmp(BKE_main_blendfile_path(bmain), libname) == 0) {
		BKE_reportf(op->reports, RPT_ERROR, "'%s': cannot use current file as library", path);
		return OPERATOR_CANCELLED;
	}

	/* check if something is indicated for append/link */
	prop = RNA_struct_find_property(op->ptr, "files");
	if (prop) {
		totfiles = RNA_property_collection_length(op->ptr, prop);
		if (totfiles == 0) {
			if (!name) {
				BKE_reportf(op->reports, RPT_ERROR, "'%s': nothing indicated", path);
				return OPERATOR_CANCELLED;
			}
		}
	}
	else if (!name) {
		BKE_reportf(op->reports, RPT_ERROR, "'%s': nothing indicated", path);
		return OPERATOR_CANCELLED;
	}

	short flag = wm_link_append_flag(op);

	/* sanity checks for flag */
	if (scene && scene->id.lib) {
		BKE_reportf(op->reports, RPT_WARNING,
		            "Scene '%s' is linked, instantiation of objects & groups is disabled", scene->id.name + 2);
		flag &= ~FILE_GROUP_INSTANCE;
		scene = NULL;
	}

	/* We need to add nothing from BLO_LibLinkFlags to flag here. */

	/* from here down, no error returns */

	if (scene && RNA_boolean_get(op->ptr, "autoselect")) {
		BKE_scene_base_deselect_all(scene);
	}

	/* tag everything, all untagged data can be made local
	 * its also generally useful to know what is new
	 *
	 * take extra care BKE_main_id_flag_all(bmain, LIB_TAG_PRE_EXISTING, false) is called after! */
	BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, true);

	/* We define our working data...
	 * Note that here, each item 'uses' one library, and only one. */
	lapp_data = wm_link_append_data_new(flag);
	if (totfiles != 0) {
		GHash *libraries = BLI_ghash_new(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, __func__);
		int lib_idx = 0;

		RNA_BEGIN (op->ptr, itemptr, "files")
		{
			RNA_string_get(&itemptr, "name", relname);

			BLI_join_dirfile(path, sizeof(path), root, relname);

			if (BLO_library_path_explode(path, libname, &group, &name)) {
				if (!group || !name) {
					continue;
				}

				if (!BLI_ghash_haskey(libraries, libname)) {
					BLI_ghash_insert(libraries, BLI_strdup(libname), POINTER_FROM_INT(lib_idx));
					lib_idx++;
					wm_link_append_data_library_add(lapp_data, libname);
				}
			}
		}
		RNA_END;

		RNA_BEGIN (op->ptr, itemptr, "files")
		{
			RNA_string_get(&itemptr, "name", relname);

			BLI_join_dirfile(path, sizeof(path), root, relname);

			if (BLO_library_path_explode(path, libname, &group, &name)) {
				WMLinkAppendDataItem *item;
				if (!group || !name) {
					printf("skipping %s\n", path);
					continue;
				}

				lib_idx = POINTER_AS_INT(BLI_ghash_lookup(libraries, libname));

				item = wm_link_append_data_item_add(lapp_data, name, BKE_idcode_from_name(group), NULL);
				BLI_BITMAP_ENABLE(item->libraries, lib_idx);
			}
		}
		RNA_END;

		BLI_ghash_free(libraries, MEM_freeN, NULL);
	}
	else {
		WMLinkAppendDataItem *item;

		wm_link_append_data_library_add(lapp_data, libname);
		item = wm_link_append_data_item_add(lapp_data, name, BKE_idcode_from_name(group), NULL);
		BLI_BITMAP_ENABLE(item->libraries, 0);
	}

	if (lapp_data->num_items == 0) {
		/* Early out in case there is nothing to link. */
		wm_link_append_data_free(lapp_data);
		return OPERATOR_CANCELLED;
	}

	/* XXX We'd need re-entrant locking on Main for this to work... */
	/* BKE_main_lock(bmain); */

	wm_link_do(lapp_data, op->reports, bmain, scene, CTX_wm_view3d(C));

	/* BKE_main_unlock(bmain); */

	/* mark all library linked objects to be updated */
	BKE_main_lib_objects_recalc_all(bmain);
	IMB_colormanagement_check_file_config(bmain);

	/* append, rather than linking */
	if ((flag & FILE_LINK) == 0) {
		const bool set_fake = RNA_boolean_get(op->ptr, "set_fake");
		const bool use_recursive = RNA_boolean_get(op->ptr, "use_recursive");

		if (use_recursive) {
			BKE_library_make_local(bmain, NULL, NULL, true, set_fake);
		}
		else {
			LinkNode *itemlink;
			GSet *done_libraries = BLI_gset_new_ex(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp,
			                                       __func__, lapp_data->num_libraries);

			for (itemlink = lapp_data->items.list; itemlink; itemlink = itemlink->next) {
				ID *new_id = ((WMLinkAppendDataItem *)(itemlink->link))->new_id;

				if (new_id && !BLI_gset_haskey(done_libraries, new_id->lib)) {
					BKE_library_make_local(bmain, new_id->lib, NULL, true, set_fake);
					BLI_gset_insert(done_libraries, new_id->lib);
				}
			}

			BLI_gset_free(done_libraries, NULL);
		}
	}

	wm_link_append_data_free(lapp_data);

	/* important we unset, otherwise these object wont
	 * link into other scenes from this blend file */
	BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);

	/* recreate dependency graph to include new objects */
	if (scene) {
		DAG_scene_relations_rebuild(bmain, scene);
	}

	/* free gpu materials, some materials depend on existing objects, such as lamps so freeing correctly refreshes */
	GPU_materials_free(bmain);

	/* XXX TODO: align G.lib with other directory storage (like last opened image etc...) */
	BLI_strncpy(G.lib, root, FILE_MAX);

	WM_event_add_notifier(C, NC_WINDOW, NULL);

	return OPERATOR_FINISHED;
}

static void wm_link_append_properties_common(wmOperatorType *ot, bool is_link)
{
	PropertyRNA *prop;

	/* better not save _any_ settings for this operator */
	/* properties */
	prop = RNA_def_boolean(ot->srna, "link", is_link,
	                       "Link", "Link the objects or data-blocks rather than appending");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
	prop = RNA_def_boolean(ot->srna, "autoselect", true,
	                       "Select", "Select new objects");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "active_layer", true,
	                       "Active Layer", "Put new objects on the active layer");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "instance_groups", is_link,
	                       "Instance Groups", "Create Dupli-Group instances for each group");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

void WM_OT_link(wmOperatorType *ot)
{
	ot->name = "Link from Library";
	ot->idname = "WM_OT_link";
	ot->description = "Link from a Library .blend file";

	ot->invoke = wm_link_append_invoke;
	ot->exec = wm_link_append_exec;
	ot->poll = wm_link_append_poll;

	ot->flag |= OPTYPE_UNDO;

	WM_operator_properties_filesel(
	        ot, FILE_TYPE_FOLDER | FILE_TYPE_BLENDER | FILE_TYPE_BLENDERLIB, FILE_LOADLIB, FILE_OPENFILE,
	        WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME | WM_FILESEL_RELPATH | WM_FILESEL_FILES,
	        FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);

	wm_link_append_properties_common(ot, true);
}

void WM_OT_append(wmOperatorType *ot)
{
	ot->name = "Append from Library";
	ot->idname = "WM_OT_append";
	ot->description = "Append from a Library .blend file";

	ot->invoke = wm_link_append_invoke;
	ot->exec = wm_link_append_exec;
	ot->poll = wm_link_append_poll;

	ot->flag |= OPTYPE_UNDO;

	WM_operator_properties_filesel(
	        ot, FILE_TYPE_FOLDER | FILE_TYPE_BLENDER | FILE_TYPE_BLENDERLIB, FILE_LOADLIB, FILE_OPENFILE,
	        WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME | WM_FILESEL_FILES,
	        FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);

	wm_link_append_properties_common(ot, false);
	RNA_def_boolean(ot->srna, "set_fake", false, "Fake User",
	                "Set Fake User for appended items (except Objects and Groups)");
	RNA_def_boolean(ot->srna, "use_recursive", true, "Localize All",
	                "Localize all appended data, including those indirectly linked from other libraries");
}

/** \name Reload/relocate libraries.
 *
 * \{ */

static int wm_lib_relocate_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Library *lib;
	char lib_name[MAX_NAME];

	RNA_string_get(op->ptr, "library", lib_name);
	lib = (Library *)BKE_libblock_find_name(CTX_data_main(C), ID_LI, lib_name);

	if (lib) {
		if (lib->parent) {
			BKE_reportf(op->reports, RPT_ERROR_INVALID_INPUT,
			            "Cannot relocate indirectly linked library '%s'", lib->filepath);
			return OPERATOR_CANCELLED;
		}
		RNA_string_set(op->ptr, "filepath", lib->filepath);

		WM_event_add_fileselect(C, op);

		return OPERATOR_RUNNING_MODAL;
	}

	return OPERATOR_CANCELLED;
}

static void lib_relocate_do(
        Main *bmain, Scene *scene,
        Library *library, WMLinkAppendData *lapp_data, ReportList *reports, const bool do_reload)
{
	ListBase *lbarray[MAX_LIBARRAY];
	int lba_idx;

	LinkNode *itemlink;
	int item_idx;

	/* Remove all IDs to be reloaded from Main. */
	lba_idx = set_listbasepointers(bmain, lbarray);
	while (lba_idx--) {
		ID *id = lbarray[lba_idx]->first;
		const short idcode = id ? GS(id->name) : 0;

		if (!id || !BKE_idcode_is_linkable(idcode)) {
			/* No need to reload non-linkable datatypes, those will get relinked with their 'users ID'. */
			continue;
		}

		for (; id; id = id->next) {
			if (id->lib == library) {
				WMLinkAppendDataItem *item;

				/* We remove it from current Main, and add it to items to link... */
				/* Note that non-linkable IDs (like e.g. shapekeys) are also explicitly linked here... */
				BLI_remlink(lbarray[lba_idx], id);
				item = wm_link_append_data_item_add(lapp_data, id->name + 2, idcode, id);
				BLI_BITMAP_SET_ALL(item->libraries, true, lapp_data->num_libraries);

#ifdef PRINT_DEBUG
				printf("\tdatablock to seek for: %s\n", id->name);
#endif
			}
		}
	}

	if (lapp_data->num_items == 0) {
		/* Early out in case there is nothing to do. */
		return;
	}

	BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, true);

	/* We do not want any instantiation here! */
	wm_link_do(lapp_data, reports, bmain, NULL, NULL);

	BKE_main_lock(bmain);

	/* We add back old id to bmain.
	 * We need to do this in a first, separated loop, otherwise some of those may not be handled by
	 * ID remapping, which means they would still reference old data to be deleted... */
	for (item_idx = 0, itemlink = lapp_data->items.list; itemlink; item_idx++, itemlink = itemlink->next) {
		WMLinkAppendDataItem *item = itemlink->link;
		ID *old_id = item->customdata;

		BLI_assert(old_id);
		BLI_addtail(which_libbase(bmain, GS(old_id->name)), old_id);
	}

	/* Note that in reload case, we also want to replace indirect usages. */
	const short remap_flags = ID_REMAP_SKIP_NEVER_NULL_USAGE | ID_REMAP_NO_INDIRECT_PROXY_DATA_USAGE |
	                          (do_reload ? 0 : ID_REMAP_SKIP_INDIRECT_USAGE);
	for (item_idx = 0, itemlink = lapp_data->items.list; itemlink; item_idx++, itemlink = itemlink->next) {
		WMLinkAppendDataItem *item = itemlink->link;
		ID *old_id = item->customdata;
		ID *new_id = item->new_id;

		BLI_assert(old_id);
		if (do_reload) {
			/* Since we asked for placeholders in case of missing IDs, we expect to always get a valid one. */
			BLI_assert(new_id);
		}
		if (new_id) {
#ifdef PRINT_DEBUG
			printf("before remap, old_id users: %d, new_id users: %d\n", old_id->us, new_id->us);
#endif
			BKE_libblock_remap_locked(bmain, old_id, new_id, remap_flags);

			if (old_id->flag & LIB_FAKEUSER) {
				id_fake_user_clear(old_id);
				id_fake_user_set(new_id);
			}

#ifdef PRINT_DEBUG
			printf("after remap, old_id users: %d, new_id users: %d\n", old_id->us, new_id->us);
#endif

			/* In some cases, new_id might become direct link, remove parent of library in this case. */
			if (new_id->lib->parent && (new_id->tag & LIB_TAG_INDIRECT) == 0) {
				if (do_reload) {
					BLI_assert(0);  /* Should not happen in 'pure' reload case... */
				}
				new_id->lib->parent = NULL;
			}
		}

		if (old_id->us > 0 && new_id && old_id->lib == new_id->lib) {
			/* Note that this *should* not happen - but better be safe than sorry in this area, at least until we are
			 * 100% sure this cannot ever happen.
			 * Also, we can safely assume names were unique so far, so just replacing '.' by '~' should work,
			 * but this does not totally rules out the possibility of name collision. */
			size_t len = strlen(old_id->name);
			size_t dot_pos;
			bool has_num = false;

			for (dot_pos = len; dot_pos--;) {
				char c = old_id->name[dot_pos];
				if (c == '.') {
					break;
				}
				else if (c < '0' || c > '9') {
					has_num = false;
					break;
				}
				has_num = true;
			}

			if (has_num) {
				old_id->name[dot_pos] = '~';
			}
			else {
				len = MIN2(len, MAX_ID_NAME - 7);
				BLI_strncpy(&old_id->name[len], "~000", 7);
			}

			id_sort_by_name(which_libbase(bmain, GS(old_id->name)), old_id);

			BKE_reportf(reports, RPT_WARNING,
			            "Lib Reload: Replacing all references to old data-block '%s' by reloaded one failed, "
			            "old one (%d remaining users) had to be kept and was renamed to '%s'",
			            new_id->name, old_id->us, old_id->name);
		}
	}

	BKE_main_unlock(bmain);

	for (item_idx = 0, itemlink = lapp_data->items.list; itemlink; item_idx++, itemlink = itemlink->next) {
		WMLinkAppendDataItem *item = itemlink->link;
		ID *old_id = item->customdata;

		if (old_id->us == 0) {
			BKE_libblock_free(bmain, old_id);
		}
	}

	/* Some datablocks can get reloaded/replaced 'silently' because they are not linkable (shape keys e.g.),
	 * so we need another loop here to clear old ones if possible. */
	lba_idx = set_listbasepointers(bmain, lbarray);
	while (lba_idx--) {
		ID *id, *id_next;
		for (id  = lbarray[lba_idx]->first; id; id = id_next) {
			id_next = id->next;
			/* XXX That check may be a bit to generic/permissive? */
			if (id->lib && (id->flag & LIB_TAG_PRE_EXISTING) && id->us == 0) {
				BKE_libblock_free(bmain, id);
			}
		}
	}

	/* Get rid of no more used libraries... */
	BKE_main_id_tag_idcode(bmain, ID_LI, LIB_TAG_DOIT, true);
	lba_idx = set_listbasepointers(bmain, lbarray);
	while (lba_idx--) {
		ID *id;
		for (id = lbarray[lba_idx]->first; id; id = id->next) {
			if (id->lib) {
				id->lib->id.tag &= ~LIB_TAG_DOIT;
			}
		}
	}
	Library *lib, *lib_next;
	for (lib = which_libbase(bmain, ID_LI)->first; lib; lib = lib_next) {
		lib_next = lib->id.next;
		if (lib->id.tag & LIB_TAG_DOIT) {
			id_us_clear_real(&lib->id);
			if (lib->id.us == 0) {
				BKE_libblock_free(bmain, (ID *)lib);
			}
		}
	}

	BKE_main_lib_objects_recalc_all(bmain);
	IMB_colormanagement_check_file_config(bmain);

	/* important we unset, otherwise these object wont
	 * link into other scenes from this blend file */
	BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);

	/* recreate dependency graph to include new objects */
	DAG_scene_relations_rebuild(bmain, scene);

	/* free gpu materials, some materials depend on existing objects, such as lamps so freeing correctly refreshes */
	GPU_materials_free(bmain);
}

void WM_lib_reload(Library *lib, bContext *C, ReportList *reports)
{
	if (!BLO_has_bfile_extension(lib->filepath)) {
		BKE_reportf(reports, RPT_ERROR, "'%s' is not a valid library filepath", lib->filepath);
		return;
	}

	if (!BLI_exists(lib->filepath)) {
		BKE_reportf(reports, RPT_ERROR,
		            "Trying to reload library '%s' from invalid path '%s'", lib->id.name, lib->filepath);
		return;
	}

	WMLinkAppendData *lapp_data = wm_link_append_data_new(BLO_LIBLINK_USE_PLACEHOLDERS | BLO_LIBLINK_FORCE_INDIRECT);

	wm_link_append_data_library_add(lapp_data, lib->filepath);

	lib_relocate_do(CTX_data_main(C), CTX_data_scene(C), lib, lapp_data, reports, true);

	wm_link_append_data_free(lapp_data);

	WM_event_add_notifier(C, NC_WINDOW, NULL);
}

static int wm_lib_relocate_exec_do(bContext *C, wmOperator *op, bool do_reload)
{
	Library *lib;
	char lib_name[MAX_NAME];

	RNA_string_get(op->ptr, "library", lib_name);
	lib = (Library *)BKE_libblock_find_name(CTX_data_main(C), ID_LI, lib_name);

	if (lib) {
		Main *bmain = CTX_data_main(C);
		Scene *scene = CTX_data_scene(C);
		PropertyRNA *prop;
		WMLinkAppendData *lapp_data;

		char path[FILE_MAX], root[FILE_MAXDIR], libname[FILE_MAX], relname[FILE_MAX];
		short flag = 0;

		if (RNA_boolean_get(op->ptr, "relative_path")) {
			flag |= FILE_RELPATH;
		}

		if (lib->parent && !do_reload) {
			BKE_reportf(op->reports, RPT_ERROR_INVALID_INPUT,
			            "Cannot relocate indirectly linked library '%s'", lib->filepath);
			return OPERATOR_CANCELLED;
		}

		RNA_string_get(op->ptr, "directory", root);
		RNA_string_get(op->ptr, "filename", libname);

		if (!BLO_has_bfile_extension(libname)) {
			BKE_report(op->reports, RPT_ERROR, "Not a library");
			return OPERATOR_CANCELLED;
		}

		BLI_join_dirfile(path, sizeof(path), root, libname);

		if (!BLI_exists(path)) {
			BKE_reportf(op->reports, RPT_ERROR_INVALID_INPUT,
			            "Trying to reload or relocate library '%s' to invalid path '%s'", lib->id.name, path);
			return OPERATOR_CANCELLED;
		}

		if (BLI_path_cmp(lib->filepath, path) == 0) {
#ifdef PRINT_DEBUG
			printf("We are supposed to reload '%s' lib (%d)...\n", lib->filepath, lib->id.us);
#endif

			do_reload = true;

			lapp_data = wm_link_append_data_new(flag);
			wm_link_append_data_library_add(lapp_data, path);
		}
		else {
			int totfiles = 0;

#ifdef PRINT_DEBUG
			printf("We are supposed to relocate '%s' lib to new '%s' one...\n", lib->filepath, libname);
#endif

			/* Check if something is indicated for relocate. */
			prop = RNA_struct_find_property(op->ptr, "files");
			if (prop) {
				totfiles = RNA_property_collection_length(op->ptr, prop);
				if (totfiles == 0) {
					if (!libname[0]) {
						BKE_report(op->reports, RPT_ERROR, "Nothing indicated");
						return OPERATOR_CANCELLED;
					}
				}
			}

			lapp_data = wm_link_append_data_new(flag);

			if (totfiles) {
				RNA_BEGIN (op->ptr, itemptr, "files")
				{
					RNA_string_get(&itemptr, "name", relname);

					BLI_join_dirfile(path, sizeof(path), root, relname);

					if (BLI_path_cmp(path, lib->filepath) == 0 || !BLO_has_bfile_extension(relname)) {
						continue;
					}

#ifdef PRINT_DEBUG
					printf("\t candidate new lib to reload datablocks from: %s\n", path);
#endif
					wm_link_append_data_library_add(lapp_data, path);
				}
				RNA_END;
			}
			else {
#ifdef PRINT_DEBUG
				printf("\t candidate new lib to reload datablocks from: %s\n", path);
#endif
				wm_link_append_data_library_add(lapp_data, path);
			}
		}

		if (do_reload) {
			lapp_data->flag |= BLO_LIBLINK_USE_PLACEHOLDERS | BLO_LIBLINK_FORCE_INDIRECT;
		}

		lib_relocate_do(bmain, scene, lib, lapp_data, op->reports, do_reload);

		wm_link_append_data_free(lapp_data);

		/* XXX TODO: align G.lib with other directory storage (like last opened image etc...) */
		BLI_strncpy(G.lib, root, FILE_MAX);

		WM_event_add_notifier(C, NC_WINDOW, NULL);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

static int wm_lib_relocate_exec(bContext *C, wmOperator *op)
{
	return wm_lib_relocate_exec_do(C, op, false);
}

void WM_OT_lib_relocate(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "Relocate Library";
	ot->idname = "WM_OT_lib_relocate";
	ot->description = "Relocate the given library to one or several others";

	ot->invoke = wm_lib_relocate_invoke;
	ot->exec = wm_lib_relocate_exec;

	ot->flag |= OPTYPE_UNDO;

	prop = RNA_def_string(ot->srna, "library", NULL, MAX_NAME, "Library", "Library to relocate");
	RNA_def_property_flag(prop, PROP_HIDDEN);

	WM_operator_properties_filesel(
	            ot, FILE_TYPE_FOLDER | FILE_TYPE_BLENDER, FILE_BLENDER, FILE_OPENFILE,
	            WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME | WM_FILESEL_FILES | WM_FILESEL_RELPATH,
	            FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);
}

static int wm_lib_reload_exec(bContext *C, wmOperator *op)
{
	return wm_lib_relocate_exec_do(C, op, true);
}

void WM_OT_lib_reload(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "Reload Library";
	ot->idname = "WM_OT_lib_reload";
	ot->description = "Reload the given library";

	ot->exec = wm_lib_reload_exec;

	ot->flag |= OPTYPE_UNDO;

	prop = RNA_def_string(ot->srna, "library", NULL, MAX_NAME, "Library", "Library to reload");
	RNA_def_property_flag(prop, PROP_HIDDEN);

	WM_operator_properties_filesel(
	            ot, FILE_TYPE_FOLDER | FILE_TYPE_BLENDER, FILE_BLENDER, FILE_OPENFILE,
	            WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME | WM_FILESEL_RELPATH,
	            FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);
}

/** \} */
