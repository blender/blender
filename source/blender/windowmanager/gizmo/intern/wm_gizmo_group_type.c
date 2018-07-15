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
		wmGizmoGroupType *gzgt;

		gzgt = BLI_ghash_lookup(global_gizmogrouptype_hash, idname);
		if (gzgt) {
			return gzgt;
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
	wmGizmoGroupType *gzgt = MEM_callocN(sizeof(wmGizmoGroupType), "gizmogrouptype");

	return gzgt;
}
static void wm_gizmogrouptype_append__end(wmGizmoGroupType *gzgt)
{
	BLI_assert(gzgt->name != NULL);
	BLI_assert(gzgt->idname != NULL);

	gzgt->type_update_flag |= WM_GIZMOMAPTYPE_KEYMAP_INIT;

	/* if not set, use default */
	if (gzgt->setup_keymap == NULL) {
		if (gzgt->flag & WM_GIZMOGROUPTYPE_SELECT) {
			gzgt->setup_keymap = WM_gizmogroup_keymap_common_select;
		}
		else {
			gzgt->setup_keymap = WM_gizmogroup_keymap_common;
		}
	}

	BLI_ghash_insert(global_gizmogrouptype_hash, (void *)gzgt->idname, gzgt);
}

wmGizmoGroupType *WM_gizmogrouptype_append(
        void (*wtfunc)(struct wmGizmoGroupType *))
{
	wmGizmoGroupType *gzgt = wm_gizmogrouptype_append__begin();
	wtfunc(gzgt);
	wm_gizmogrouptype_append__end(gzgt);
	return gzgt;
}

wmGizmoGroupType *WM_gizmogrouptype_append_ptr(
        void (*wtfunc)(struct wmGizmoGroupType *, void *), void *userdata)
{
	wmGizmoGroupType *gzgt = wm_gizmogrouptype_append__begin();
	wtfunc(gzgt, userdata);
	wm_gizmogrouptype_append__end(gzgt);
	return gzgt;
}

/**
 * Append and insert into a gizmo typemap.
 * This is most common for C gizmos which are enabled by default.
 */
wmGizmoGroupTypeRef *WM_gizmogrouptype_append_and_link(
        wmGizmoMapType *gzmap_type,
        void (*wtfunc)(struct wmGizmoGroupType *))
{
	wmGizmoGroupType *gzgt = WM_gizmogrouptype_append(wtfunc);

	gzgt->gzmap_params.spaceid = gzmap_type->spaceid;
	gzgt->gzmap_params.regionid = gzmap_type->regionid;

	return WM_gizmomaptype_group_link_ptr(gzmap_type, gzgt);
}

/**
 * Free but don't remove from ghash.
 */
static void gizmogrouptype_free(wmGizmoGroupType *gzgt)
{
	if (gzgt->ext.srna) { /* python gizmo group, allocs own string */
		MEM_freeN((void *)gzgt->idname);
	}

	MEM_freeN(gzgt);
}

void WM_gizmogrouptype_free_ptr(wmGizmoGroupType *gzgt)
{
	BLI_assert(gzgt == WM_gizmogrouptype_find(gzgt->idname, false));

	BLI_ghash_remove(global_gizmogrouptype_hash, gzgt->idname, NULL, NULL);

	gizmogrouptype_free(gzgt);

	/* XXX, TODO, update the world! */
}

bool WM_gizmogrouptype_free(const char *idname)
{
	wmGizmoGroupType *gzgt = BLI_ghash_lookup(global_gizmogrouptype_hash, idname);

	if (gzgt == NULL) {
		return false;
	}

	WM_gizmogrouptype_free_ptr(gzgt);

	return true;
}

static void wm_gizmogrouptype_ghash_free_cb(wmGizmoGroupType *gzgt)
{
	gizmogrouptype_free(gzgt);
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
