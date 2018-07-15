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

/** \file blender/windowmanager/gizmo/intern/wm_gizmo_type.c
 *  \ingroup wm
 */

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "BKE_context.h"
#include "BKE_main.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

/* only for own init/exit calls (wm_gizmotype_init/wm_gizmotype_free) */
#include "wm.h"

/* own includes */
#include "wm_gizmo_wmapi.h"
#include "wm_gizmo_intern.h"


/** \name Gizmo Type Append
 *
 * \note This follows conventions from #WM_operatortype_find #WM_operatortype_append & friends.
 * \{ */

static GHash *global_gizmotype_hash = NULL;

const wmGizmoType *WM_gizmotype_find(const char *idname, bool quiet)
{
	if (idname[0]) {
		wmGizmoType *gzt;

		gzt = BLI_ghash_lookup(global_gizmotype_hash, idname);
		if (gzt) {
			return gzt;
		}

		if (!quiet) {
			printf("search for unknown gizmo '%s'\n", idname);
		}
	}
	else {
		if (!quiet) {
			printf("search for empty gizmo\n");
		}
	}

	return NULL;
}

/* caller must free */
void WM_gizmotype_iter(GHashIterator *ghi)
{
	BLI_ghashIterator_init(ghi, global_gizmotype_hash);
}

static wmGizmoType *wm_gizmotype_append__begin(void)
{
	wmGizmoType *gzt = MEM_callocN(sizeof(wmGizmoType), "gizmotype");
	gzt->srna = RNA_def_struct_ptr(&BLENDER_RNA, "", &RNA_GizmoProperties);
#if 0
	/* Set the default i18n context now, so that opfunc can redefine it if needed! */
	RNA_def_struct_translation_context(ot->srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
	ot->translation_context = BLT_I18NCONTEXT_OPERATOR_DEFAULT;
#endif
	return gzt;
}
static void wm_gizmotype_append__end(wmGizmoType *gzt)
{
	BLI_assert(gzt->struct_size >= sizeof(wmGizmo));

	RNA_def_struct_identifier(&BLENDER_RNA, gzt->srna, gzt->idname);

	BLI_ghash_insert(global_gizmotype_hash, (void *)gzt->idname, gzt);
}

void WM_gizmotype_append(void (*gtfunc)(struct wmGizmoType *))
{
	wmGizmoType *gzt = wm_gizmotype_append__begin();
	gtfunc(gzt);
	wm_gizmotype_append__end(gzt);
}

void WM_gizmotype_append_ptr(void (*gtfunc)(struct wmGizmoType *, void *), void *userdata)
{
	wmGizmoType *mt = wm_gizmotype_append__begin();
	gtfunc(mt, userdata);
	wm_gizmotype_append__end(mt);
}

/**
 * Free but don't remove from ghash.
 */
static void gizmotype_free(wmGizmoType *gzt)
{
	if (gzt->ext.srna) { /* python gizmo, allocs own string */
		MEM_freeN((void *)gzt->idname);
	}

	BLI_freelistN(&gzt->target_property_defs);
	MEM_freeN(gzt);
}

/**
 * \param C: May be NULL.
 */
static void gizmotype_unlink(
        bContext *C, Main *bmain, wmGizmoType *gzt)
{
	/* Free instances. */
	for (bScreen *sc = bmain->screen.first; sc; sc = sc->id.next) {
		for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
			for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
				ListBase *lb = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
				for (ARegion *ar = lb->first; ar; ar = ar->next) {
					wmGizmoMap *gzmap = ar->gizmo_map;
					if (gzmap) {
						wmGizmoGroup *gzgroup;
						for (gzgroup = gzmap->groups.first; gzgroup; gzgroup = gzgroup->next) {
							for (wmGizmo *gz = gzgroup->gizmos.first, *gz_next;  gz; gz = gz_next) {
								gz_next = gz->next;
								BLI_assert(gzgroup->parent_gzmap == gzmap);
								if (gz->type == gzt) {
									WM_gizmo_unlink(&gzgroup->gizmos, gzgroup->parent_gzmap, gz, C);
									ED_region_tag_redraw(ar);
								}
							}
						}
					}
				}
			}
		}
	}
}

void WM_gizmotype_remove_ptr(bContext *C, Main *bmain, wmGizmoType *gzt)
{
	BLI_assert(gzt == WM_gizmotype_find(gzt->idname, false));

	BLI_ghash_remove(global_gizmotype_hash, gzt->idname, NULL, NULL);

	gizmotype_unlink(C, bmain, gzt);

	gizmotype_free(gzt);
}

bool WM_gizmotype_remove(bContext *C, Main *bmain, const char *idname)
{
	wmGizmoType *gzt = BLI_ghash_lookup(global_gizmotype_hash, idname);

	if (gzt == NULL) {
		return false;
	}

	WM_gizmotype_remove_ptr(C, bmain, gzt);

	return true;
}

static void wm_gizmotype_ghash_free_cb(wmGizmoType *mt)
{
	gizmotype_free(mt);
}

void wm_gizmotype_free(void)
{
	BLI_ghash_free(global_gizmotype_hash, NULL, (GHashValFreeFP)wm_gizmotype_ghash_free_cb);
	global_gizmotype_hash = NULL;
}

/* called on initialize WM_init() */
void wm_gizmotype_init(void)
{
	/* reserve size is set based on blender default setup */
	global_gizmotype_hash = BLI_ghash_str_new_ex("wm_gizmotype_init gh", 128);
}

/** \} */
