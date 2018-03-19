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

/** \file blender/blenkernel/intern/editlattice.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_curve_types.h"

#include "BLI_math_vector.h"
#include "BLI_listbase.h"

#include "BKE_deform.h"
#include "BKE_key.h"

#include "BKE_editlattice.h" /* own include */

void BKE_editlattice_free(Object *ob)
{
	Lattice *lt = ob->data;

	if (lt->editlatt) {
		Lattice *editlt = lt->editlatt->latt;

		if (editlt->def) {
			MEM_freeN(editlt->def);
		}
		if (editlt->dvert) {
			BKE_defvert_array_free(editlt->dvert, editlt->pntsu * editlt->pntsv * editlt->pntsw);
		}
		MEM_freeN(editlt);
		MEM_freeN(lt->editlatt);

		lt->editlatt = NULL;
	}
}

void BKE_editlattice_make(Object *obedit)
{
	Lattice *lt = obedit->data;
	KeyBlock *actkey;

	BKE_editlattice_free(obedit);

	actkey = BKE_keyblock_from_object(obedit);
	if (actkey) {
		BKE_keyblock_convert_to_lattice(actkey, lt);
	}
	lt->editlatt = MEM_callocN(sizeof(EditLatt), "editlatt");
	lt->editlatt->latt = MEM_dupallocN(lt);
	lt->editlatt->latt->def = MEM_dupallocN(lt->def);

	if (lt->dvert) {
		int tot = lt->pntsu * lt->pntsv * lt->pntsw;
		lt->editlatt->latt->dvert = MEM_mallocN(sizeof(MDeformVert) * tot, "Lattice MDeformVert");
		BKE_defvert_array_copy(lt->editlatt->latt->dvert, lt->dvert, tot);
	}

	if (lt->key) {
		lt->editlatt->shapenr = obedit->shapenr;
	}
}

void BKE_editlattice_load(Object *obedit)
{
	Lattice *lt, *editlt;
	KeyBlock *actkey;
	BPoint *bp;
	float *fp;
	int tot;

	lt = obedit->data;
	editlt = lt->editlatt->latt;

	if (lt->editlatt->shapenr) {
		actkey = BLI_findlink(&lt->key->block, lt->editlatt->shapenr - 1);

		/* active key: vertices */
		tot = editlt->pntsu * editlt->pntsv * editlt->pntsw;

		if (actkey->data) {
			MEM_freeN(actkey->data);
		}

		fp = actkey->data = MEM_callocN(lt->key->elemsize * tot, "actkey->data");
		actkey->totelem = tot;

		bp = editlt->def;
		while (tot--) {
			copy_v3_v3(fp, bp->vec);
			fp += 3;
			bp++;
		}
	}
	else {
		MEM_freeN(lt->def);

		lt->def = MEM_dupallocN(editlt->def);

		lt->flag = editlt->flag;

		lt->pntsu = editlt->pntsu;
		lt->pntsv = editlt->pntsv;
		lt->pntsw = editlt->pntsw;

		lt->typeu = editlt->typeu;
		lt->typev = editlt->typev;
		lt->typew = editlt->typew;
		lt->actbp = editlt->actbp;
	}

	if (lt->dvert) {
		BKE_defvert_array_free(lt->dvert, lt->pntsu * lt->pntsv * lt->pntsw);
		lt->dvert = NULL;
	}

	if (editlt->dvert) {
		tot = lt->pntsu * lt->pntsv * lt->pntsw;

		lt->dvert = MEM_mallocN(sizeof(MDeformVert) * tot, "Lattice MDeformVert");
		BKE_defvert_array_copy(lt->dvert, editlt->dvert, tot);
	}
}
