/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"

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

  lt->fu = editlt->fu;
  lt->fv = editlt->fv;
  lt->fw = editlt->fw;
  lt->du = editlt->du;
  lt->dv = editlt->dv;
  lt->dw = editlt->dw;

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
