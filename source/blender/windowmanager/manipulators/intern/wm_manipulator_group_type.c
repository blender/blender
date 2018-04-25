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

/** \file blender/windowmanager/manipulators/intern/wm_manipulator_group_type.c
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

/* only for own init/exit calls (wm_manipulatorgrouptype_init/wm_manipulatorgrouptype_free) */
#include "wm.h"

/* own includes */
#include "wm_manipulator_wmapi.h"
#include "wm_manipulator_intern.h"


/** \name ManipulatorGroup Type Append
 *
 * \note This follows conventions from #WM_operatortype_find #WM_operatortype_append & friends.
 * \{ */

static GHash *global_manipulatorgrouptype_hash = NULL;

wmManipulatorGroupType *WM_manipulatorgrouptype_find(const char *idname, bool quiet)
{
	if (idname[0]) {
		wmManipulatorGroupType *wgt;

		wgt = BLI_ghash_lookup(global_manipulatorgrouptype_hash, idname);
		if (wgt) {
			return wgt;
		}

		if (!quiet) {
			printf("search for unknown manipulator group '%s'\n", idname);
		}
	}
	else {
		if (!quiet) {
			printf("search for empty manipulator group\n");
		}
	}

	return NULL;
}

/* caller must free */
void WM_manipulatorgrouptype_iter(GHashIterator *ghi)
{
	BLI_ghashIterator_init(ghi, global_manipulatorgrouptype_hash);
}

static wmManipulatorGroupType *wm_manipulatorgrouptype_append__begin(void)
{
	wmManipulatorGroupType *wgt = MEM_callocN(sizeof(wmManipulatorGroupType), "manipulatorgrouptype");

	return wgt;
}
static void wm_manipulatorgrouptype_append__end(wmManipulatorGroupType *wgt)
{
	BLI_assert(wgt->name != NULL);
	BLI_assert(wgt->idname != NULL);

	wgt->type_update_flag |= WM_MANIPULATORMAPTYPE_KEYMAP_INIT;

	/* if not set, use default */
	if (wgt->setup_keymap == NULL) {
		if (wgt->flag & WM_MANIPULATORGROUPTYPE_SELECT) {
			wgt->setup_keymap = WM_manipulatorgroup_keymap_common_select;
		}
		else {
			wgt->setup_keymap = WM_manipulatorgroup_keymap_common;
		}
	}

	BLI_ghash_insert(global_manipulatorgrouptype_hash, (void *)wgt->idname, wgt);
}

wmManipulatorGroupType *WM_manipulatorgrouptype_append(
        void (*wtfunc)(struct wmManipulatorGroupType *))
{
	wmManipulatorGroupType *wgt = wm_manipulatorgrouptype_append__begin();
	wtfunc(wgt);
	wm_manipulatorgrouptype_append__end(wgt);
	return wgt;
}

wmManipulatorGroupType *WM_manipulatorgrouptype_append_ptr(
        void (*wtfunc)(struct wmManipulatorGroupType *, void *), void *userdata)
{
	wmManipulatorGroupType *wgt = wm_manipulatorgrouptype_append__begin();
	wtfunc(wgt, userdata);
	wm_manipulatorgrouptype_append__end(wgt);
	return wgt;
}

/**
 * Append and insert into a manipulator typemap.
 * This is most common for C manipulators which are enabled by default.
 */
wmManipulatorGroupTypeRef *WM_manipulatorgrouptype_append_and_link(
        wmManipulatorMapType *mmap_type,
        void (*wtfunc)(struct wmManipulatorGroupType *))
{
	wmManipulatorGroupType *wgt = WM_manipulatorgrouptype_append(wtfunc);

	wgt->mmap_params.spaceid = mmap_type->spaceid;
	wgt->mmap_params.regionid = mmap_type->regionid;

	return WM_manipulatormaptype_group_link_ptr(mmap_type, wgt);
}

/**
 * Free but don't remove from ghash.
 */
static void manipulatorgrouptype_free(wmManipulatorGroupType *wgt)
{
	if (wgt->ext.srna) { /* python manipulator group, allocs own string */
		MEM_freeN((void *)wgt->idname);
	}

	MEM_freeN(wgt);
}

void WM_manipulatorgrouptype_free_ptr(wmManipulatorGroupType *wgt)
{
	BLI_assert(wgt == WM_manipulatorgrouptype_find(wgt->idname, false));

	BLI_ghash_remove(global_manipulatorgrouptype_hash, wgt->idname, NULL, NULL);

	manipulatorgrouptype_free(wgt);

	/* XXX, TODO, update the world! */
}

bool WM_manipulatorgrouptype_free(const char *idname)
{
	wmManipulatorGroupType *wgt = BLI_ghash_lookup(global_manipulatorgrouptype_hash, idname);

	if (wgt == NULL) {
		return false;
	}

	WM_manipulatorgrouptype_free_ptr(wgt);

	return true;
}

static void wm_manipulatorgrouptype_ghash_free_cb(wmManipulatorGroupType *wgt)
{
	manipulatorgrouptype_free(wgt);
}

void wm_manipulatorgrouptype_free(void)
{
	BLI_ghash_free(global_manipulatorgrouptype_hash, NULL, (GHashValFreeFP)wm_manipulatorgrouptype_ghash_free_cb);
	global_manipulatorgrouptype_hash = NULL;
}

/* called on initialize WM_init() */
void wm_manipulatorgrouptype_init(void)
{
	/* reserve size is set based on blender default setup */
	global_manipulatorgrouptype_hash = BLI_ghash_str_new_ex("wm_manipulatorgrouptype_init gh", 128);
}

/** \} */

