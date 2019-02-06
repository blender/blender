
/*
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup bke
 */

#include <string.h>

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_listbase.h"

#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_object.h"
#include "BKE_object_facemap.h"  /* own include */
#include "BKE_object_deform.h"

#include "BLT_translation.h"

#include "MEM_guardedalloc.h"

#include "RNA_define.h"
#include "RNA_access.h"

static bool fmap_unique_check(void *arg, const char *name)
{
	struct {Object *ob; void *fm; } *data = arg;

	bFaceMap *fmap;

	for (fmap = data->ob->fmaps.first; fmap; fmap = fmap->next) {
		if (data->fm != fmap) {
			if (!strcmp(fmap->name, name)) {
				return true;
			}
		}
	}

	return false;
}

static bFaceMap *fmap_duplicate(bFaceMap *infmap)
{
	bFaceMap *outfmap;

	if (!infmap)
		return NULL;

	outfmap = MEM_callocN(sizeof(bFaceMap), "copy facemap");

	/* For now, just copy everything over. */
	memcpy(outfmap, infmap, sizeof(bFaceMap));

	outfmap->next = outfmap->prev = NULL;

	return outfmap;
}

void BKE_object_facemap_copy_list(ListBase *outbase, const ListBase *inbase)
{
	bFaceMap *fmap, *fmapn;

	BLI_listbase_clear(outbase);

	for (fmap = inbase->first; fmap; fmap = fmap->next) {
		fmapn = fmap_duplicate(fmap);
		BLI_addtail(outbase, fmapn);
	}
}

void BKE_object_facemap_unique_name(Object *ob, bFaceMap *fmap)
{
	struct {Object *ob; void *fmap; } data;
	data.ob = ob;
	data.fmap = fmap;

	BLI_uniquename_cb(fmap_unique_check, &data, DATA_("Group"), '.', fmap->name, sizeof(fmap->name));
}

bFaceMap *BKE_object_facemap_add_name(Object *ob, const char *name)
{
	bFaceMap *fmap;

	if (!ob || ob->type != OB_MESH)
		return NULL;

	fmap = MEM_callocN(sizeof(bFaceMap), __func__);

	BLI_strncpy(fmap->name, name, sizeof(fmap->name));

	BLI_addtail(&ob->fmaps, fmap);

	ob->actfmap = BLI_listbase_count(&ob->fmaps);

	BKE_object_facemap_unique_name(ob, fmap);

	return fmap;
}

bFaceMap *BKE_object_facemap_add(Object *ob)
{
	return BKE_object_facemap_add_name(ob, DATA_("FaceMap"));
}


static void object_fmap_remove_edit_mode(Object *ob, bFaceMap *fmap, bool do_selected, bool purge)
{
	const int fmap_nr = BLI_findindex(&ob->fmaps, fmap);

	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;

		if (me->edit_btmesh) {
			BMEditMesh *em = me->edit_btmesh;
			const int cd_fmap_offset = CustomData_get_offset(&em->bm->pdata, CD_FACEMAP);

			if (cd_fmap_offset != -1) {
				BMFace *efa;
				BMIter iter;
				int *map;

				if (purge) {
					BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
						map = BM_ELEM_CD_GET_VOID_P(efa, cd_fmap_offset);

						if (map) {
							if (*map == fmap_nr)
								*map = -1;
							else if (*map > fmap_nr)
								*map -= 1;
						}
					}
				}
				else {
					BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
						map = BM_ELEM_CD_GET_VOID_P(efa, cd_fmap_offset);

						if (map && *map == fmap_nr && (!do_selected || BM_elem_flag_test(efa, BM_ELEM_SELECT))) {
							*map = -1;
						}
					}
				}
			}

			if (ob->actfmap == BLI_listbase_count(&ob->fmaps))
				ob->actfmap--;

			BLI_remlink(&ob->fmaps, fmap);
			MEM_freeN(fmap);
		}
	}
}

static void object_fmap_remove_object_mode(Object *ob, bFaceMap *fmap, bool purge)
{
	const int fmap_nr = BLI_findindex(&ob->fmaps, fmap);

	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;

		if (CustomData_has_layer(&me->pdata, CD_FACEMAP)) {
			int *map = CustomData_get_layer(&me->pdata, CD_FACEMAP);
			int i;

			if (map) {
				for (i = 0; i < me->totpoly; i++) {
					if (map[i] == fmap_nr)
						map[i] = -1;
					else if (purge && map[i] > fmap_nr)
						map[i]--;
				}
			}
		}

		if (ob->actfmap == BLI_listbase_count(&ob->fmaps))
			ob->actfmap--;

		BLI_remlink(&ob->fmaps, fmap);
		MEM_freeN(fmap);
	}
}

static void fmap_remove_exec(Object *ob, bFaceMap *fmap, const bool is_edit_mode, const bool purge)
{
	if (is_edit_mode)
		object_fmap_remove_edit_mode(ob, fmap, false, purge);
	else
		object_fmap_remove_object_mode(ob, fmap, purge);
}

void BKE_object_facemap_remove(Object *ob, bFaceMap *fmap)
{
	fmap_remove_exec(ob, fmap, BKE_object_is_in_editmode(ob), true);
}

void BKE_object_facemap_clear(Object *ob)
{
	bFaceMap *fmap = (bFaceMap *)ob->fmaps.first;

	if (fmap) {
		const bool edit_mode = BKE_object_is_in_editmode_vgroup(ob);

		while (fmap) {
			bFaceMap *next_fmap = fmap->next;
			fmap_remove_exec(ob, fmap, edit_mode, false);
			fmap = next_fmap;
		}
	}
	/* remove all face-maps */
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		CustomData_free_layer(&me->pdata, CD_FACEMAP, me->totpoly, 0);
	}
	ob->actfmap = 0;
}

int BKE_object_facemap_name_index(Object *ob, const char *name)
{
	return (name) ? BLI_findstringindex(&ob->fmaps, name, offsetof(bFaceMap, name)) : -1;
}

bFaceMap *BKE_object_facemap_find_name(Object *ob, const char *name)
{
	return BLI_findstring(&ob->fmaps, name, offsetof(bFaceMap, name));
}
