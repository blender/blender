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

/** \file blender/windowmanager/gizmo/intern/wm_gizmo_group_type.c
 *  \ingroup wm
 */

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "BKE_context.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

/* only for own init/exit calls (wm_gizmogrouptype_init/wm_gizmogrouptype_free) */
#include "wm.h"

/* own includes */
#include "wm_gizmo_wmapi.h"
#include "wm_gizmo_intern.h"


/** \name GizmoGroup Type Append
 *
 * \note This follows conventions from #WM_operatortype_find #WM_operatortype_append & friends.
 * \{ */

static GHash *global_gizmogrouptype_hash = NULL;

wmGizmoGroupType *WM_gizmogrouptype_find(const char *idname, bool quiet)
{
	if (idname[0]) {
		wmGizmoGroupType *wgt;

		wgt = BLI_ghash_lookup(global_gizmogrouptype_hash, idname);
		if (wgt) {
			return wgt;
		}

		if (!quiet) {
			printf("search for unknown gizmo group '%s'\n", idname);
		}
	}
	else {
		if (!quiet) {
			printf("search for empty gizmo group\n");
		}
	}

	return NULL;
}

/* caller must free */
void WM_gizmogrouptype_iter(GHashIterator *ghi)
{
	BLI_ghashIterator_init(ghi, global_gizmogrouptype_hash);
}

static wmGizmoGroupType *wm_gizmogrouptype_append__begin(void)
{
	wmGizmoGroupType *wgt = MEM_callocN(sizeof(wmGizmoGroupType), "gizmogrouptype");

	return wgt;
}
static void wm_gizmogrouptype_append__end(wmGizmoGroupType *wgt)
{
	BLI_assert(wgt->name != NULL);
	BLI_assert(wgt->idname != NULL);

	wgt->type_update_flag |= WM_GIZMOMAPTYPE_KEYMAP_INIT;

	/* if not set, use default */
	if (wgt->setup_keymap == NULL) {
		if (wgt->flag & WM_GIZMOGROUPTYPE_SELECT) {
			wgt->setup_keymap = WM_gizmogroup_keymap_common_select;
		}
		else {
			wgt->setup_keymap = WM_gizmogroup_keymap_common;
		}
	}

	BLI_ghash_insert(global_gizmogrouptype_hash, (void *)wgt->idname, wgt);
}

wmGizmoGroupType *WM_gizmogrouptype_append(
        void (*wtfunc)(struct wmGizmoGroupType *))
{
	wmGizmoGroupType *wgt = wm_gizmogrouptype_append__begin();
	wtfunc(wgt);
	wm_gizmogrouptype_append__end(wgt);
	return wgt;
}

wmGizmoGroupType *WM_gizmogrouptype_append_ptr(
        void (*wtfunc)(struct wmGizmoGroupType *, void *), void *userdata)
{
	wmGizmoGroupType *wgt = wm_gizmogrouptype_append__begin();
	wtfunc(wgt, userdata);
	wm_gizmogrouptype_append__end(wgt);
	return wgt;
}

/**
 * Append and insert into a gizmo typemap.
 * This is most common for C gizmos which are enabled by default.
 */
wmGizmoGroupTypeRef *WM_gizmogrouptype_append_and_link(
        wmGizmoMapType *mmap_type,
        void (*wtfunc)(struct wmGizmoGroupType *))
{
	wmGizmoGroupType *wgt = WM_gizmogrouptype_append(wtfunc);

	wgt->mmap_params.spaceid = mmap_type->spaceid;
	wgt->mmap_params.regionid = mmap_type->regionid;

	return WM_gizmomaptype_group_link_ptr(mmap_type, wgt);
}

/**
 * Free but don't remove from ghash.
 */
static void gizmogrouptype_free(wmGizmoGroupType *wgt)
{
	if (wgt->ext.srna) { /* python gizmo group, allocs own string */
		MEM_freeN((void *)wgt->idname);
	}

	MEM_freeN(wgt);
}

void WM_gizmogrouptype_free_ptr(wmGizmoGroupType *wgt)
{
	BLI_assert(wgt == WM_gizmogrouptype_find(wgt->idname, false));

	BLI_ghash_remove(global_gizmogrouptype_hash, wgt->idname, NULL, NULL);

	gizmogrouptype_free(wgt);

	/* XXX, TODO, update the world! */
}

bool WM_gizmogrouptype_free(const char *idname)
{
	wmGizmoGroupType *wgt = BLI_ghash_lookup(global_gizmogrouptype_hash, idname);

	if (wgt == NULL) {
		return false;
	}

	WM_gizmogrouptype_free_ptr(wgt);

	return true;
}

static void wm_gizmogrouptype_ghash_free_cb(wmGizmoGroupType *wgt)
{
	gizmogrouptype_free(wgt);
}

void wm_gizmogrouptype_free(void)
{
	BLI_ghash_free(global_gizmogrouptype_hash, NULL, (GHashValFreeFP)wm_gizmogrouptype_ghash_free_cb);
	global_gizmogrouptype_hash = NULL;
}

/* called on initialize WM_init() */
void wm_gizmogrouptype_init(void)
{
	/* reserve size is set based on blender default setup */
	global_gizmogrouptype_hash = BLI_ghash_str_new_ex("wm_gizmogrouptype_init gh", 128);
}

/** \} */
