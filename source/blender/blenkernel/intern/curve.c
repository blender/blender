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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <math.h>  // floor
#include <string.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"

#include "DNA_anim_types.h"
#include "DNA_curve_types.h"
#include "DNA_material_types.h"

/* for dereferencing pointers */
#include "DNA_key_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"
#include "DNA_object_types.h"

#include "BKE_animsys.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_font.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_material.h"

#include "DEG_depsgraph.h"

#include "CLG_log.h"

/* globals */

/* local */
static CLG_LogRef LOG = {"bke.curve"};

static int cu_isectLL(const float v1[3],
                      const float v2[3],
                      const float v3[3],
                      const float v4[3],
                      short cox,
                      short coy,
                      float *lambda,
                      float *mu,
                      float vec[3]);

/* frees editcurve entirely */
void BKE_curve_editfont_free(Curve *cu)
{
  if (cu->editfont) {
    EditFont *ef = cu->editfont;

    if (ef->textbuf) {
      MEM_freeN(ef->textbuf);
    }
    if (ef->textbufinfo) {
      MEM_freeN(ef->textbufinfo);
    }
    if (ef->selboxes) {
      MEM_freeN(ef->selboxes);
    }

    MEM_freeN(ef);
    cu->editfont = NULL;
  }
}

static void curve_editNurb_keyIndex_cv_free_cb(void *val)
{
  CVKeyIndex *index = val;
  MEM_freeN(index->orig_cv);
  MEM_freeN(val);
}

void BKE_curve_editNurb_keyIndex_delCV(GHash *keyindex, const void *cv)
{
  BLI_assert(keyindex != NULL);
  BLI_ghash_remove(keyindex, cv, NULL, curve_editNurb_keyIndex_cv_free_cb);
}

void BKE_curve_editNurb_keyIndex_free(GHash **keyindex)
{
  if (!(*keyindex)) {
    return;
  }
  BLI_ghash_free(*keyindex, NULL, curve_editNurb_keyIndex_cv_free_cb);
  *keyindex = NULL;
}

void BKE_curve_editNurb_free(Curve *cu)
{
  if (cu->editnurb) {
    BKE_nurbList_free(&cu->editnurb->nurbs);
    BKE_curve_editNurb_keyIndex_free(&cu->editnurb->keyindex);
    MEM_freeN(cu->editnurb);
    cu->editnurb = NULL;
  }
}

/** Free (or release) any data used by this curve (does not free the curve itself). */
void BKE_curve_free(Curve *cu)
{
  BKE_animdata_free((ID *)cu, false);

  BKE_curve_batch_cache_free(cu);

  BKE_nurbList_free(&cu->nurb);
  BKE_curve_editfont_free(cu);

  BKE_curve_editNurb_free(cu);

  MEM_SAFE_FREE(cu->mat);
  MEM_SAFE_FREE(cu->str);
  MEM_SAFE_FREE(cu->strinfo);
  MEM_SAFE_FREE(cu->bb);
  MEM_SAFE_FREE(cu->tb);
}

void BKE_curve_init(Curve *cu)
{
  /* BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(cu, id)); */ /* cu->type is already initialized... */

  copy_v3_fl(cu->size, 1.0f);
  cu->flag = CU_DEFORM_BOUNDS_OFF | CU_PATH_RADIUS;
  cu->pathlen = 100;
  cu->resolu = cu->resolv = (cu->type == OB_SURF) ? 4 : 12;
  cu->width = 1.0;
  cu->wordspace = 1.0;
  cu->spacing = cu->linedist = 1.0;
  cu->fsize = 1.0;
  cu->ulheight = 0.05;
  cu->texflag = CU_AUTOSPACE;
  cu->smallcaps_scale = 0.75f;
  /* XXX: this one seems to be the best one in most cases, at least for curve deform... */
  cu->twist_mode = CU_TWIST_MINIMUM;
  cu->bevfac1 = 0.0f;
  cu->bevfac2 = 1.0f;
  cu->bevfac1_mapping = CU_BEVFAC_MAP_RESOLU;
  cu->bevfac2_mapping = CU_BEVFAC_MAP_RESOLU;
  cu->bevresol = 4;

  cu->bb = BKE_boundbox_alloc_unit();

  if (cu->type == OB_FONT) {
    cu->flag |= CU_FRONT | CU_BACK;
    cu->vfont = cu->vfontb = cu->vfonti = cu->vfontbi = BKE_vfont_builtin_get();
    cu->vfont->id.us += 4;
    cu->str = MEM_malloc_arrayN(12, sizeof(unsigned char), "str");
    BLI_strncpy(cu->str, "Text", 12);
    cu->len = cu->len_wchar = cu->pos = 4;
    cu->strinfo = MEM_calloc_arrayN(12, sizeof(CharInfo), "strinfo new");
    cu->totbox = cu->actbox = 1;
    cu->tb = MEM_calloc_arrayN(MAXTEXTBOX, sizeof(TextBox), "textbox");
    cu->tb[0].w = cu->tb[0].h = 0.0;
  }
}

Curve *BKE_curve_add(Main *bmain, const char *name, int type)
{
  Curve *cu;

  cu = BKE_libblock_alloc(bmain, ID_CU, name, 0);
  cu->type = type;

  BKE_curve_init(cu);

  return cu;
}

/**
 * Only copy internal data of Curve ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_curve_copy_data(Main *bmain, Curve *cu_dst, const Curve *cu_src, const int flag)
{
  BLI_listbase_clear(&cu_dst->nurb);
  BKE_nurbList_duplicate(&(cu_dst->nurb), &(cu_src->nurb));

  cu_dst->mat = MEM_dupallocN(cu_src->mat);

  cu_dst->str = MEM_dupallocN(cu_src->str);
  cu_dst->strinfo = MEM_dupallocN(cu_src->strinfo);
  cu_dst->tb = MEM_dupallocN(cu_src->tb);
  cu_dst->bb = MEM_dupallocN(cu_src->bb);
  cu_dst->batch_cache = NULL;

  if (cu_src->key && (flag & LIB_ID_COPY_SHAPEKEY)) {
    BKE_id_copy_ex(bmain, &cu_src->key->id, (ID **)&cu_dst->key, flag);
  }

  cu_dst->editnurb = NULL;
  cu_dst->editfont = NULL;
}

Curve *BKE_curve_copy(Main *bmain, const Curve *cu)
{
  Curve *cu_copy;
  BKE_id_copy(bmain, &cu->id, (ID **)&cu_copy);
  return cu_copy;
}

void BKE_curve_make_local(Main *bmain, Curve *cu, const bool lib_local)
{
  BKE_id_make_local_generic(bmain, &cu->id, true, lib_local);
}

/* Get list of nurbs from editnurbs structure */
ListBase *BKE_curve_editNurbs_get(Curve *cu)
{
  if (cu->editnurb) {
    return &cu->editnurb->nurbs;
  }

  return NULL;
}

short BKE_curve_type_get(Curve *cu)
{
  Nurb *nu;
  int type = cu->type;

  if (cu->vfont) {
    return OB_FONT;
  }

  if (!cu->type) {
    type = OB_CURVE;

    for (nu = cu->nurb.first; nu; nu = nu->next) {
      if (nu->pntsv > 1) {
        type = OB_SURF;
      }
    }
  }

  return type;
}

void BKE_curve_curve_dimension_update(Curve *cu)
{
  ListBase *nurbs = BKE_curve_nurbs_get(cu);
  Nurb *nu = nurbs->first;

  if (cu->flag & CU_3D) {
    for (; nu; nu = nu->next) {
      nu->flag &= ~CU_2D;
    }
  }
  else {
    for (; nu; nu = nu->next) {
      nu->flag |= CU_2D;
      BKE_nurb_test_2d(nu);

      /* since the handles are moved they need to be auto-located again */
      if (nu->type == CU_BEZIER) {
        BKE_nurb_handles_calc(nu);
      }
    }
  }
}

void BKE_curve_type_test(Object *ob)
{
  ob->type = BKE_curve_type_get(ob->data);

  if (ob->type == OB_CURVE) {
    BKE_curve_curve_dimension_update((Curve *)ob->data);
  }
}

void BKE_curve_boundbox_calc(Curve *cu, float r_loc[3], float r_size[3])
{
  BoundBox *bb;
  float min[3], max[3];
  float mloc[3], msize[3];

  if (cu->bb == NULL) {
    cu->bb = MEM_callocN(sizeof(BoundBox), "boundbox");
  }
  bb = cu->bb;

  if (!r_loc) {
    r_loc = mloc;
  }
  if (!r_size) {
    r_size = msize;
  }

  INIT_MINMAX(min, max);
  if (!BKE_curve_minmax(cu, true, min, max)) {
    min[0] = min[1] = min[2] = -1.0f;
    max[0] = max[1] = max[2] = 1.0f;
  }

  mid_v3_v3v3(r_loc, min, max);

  r_size[0] = (max[0] - min[0]) / 2.0f;
  r_size[1] = (max[1] - min[1]) / 2.0f;
  r_size[2] = (max[2] - min[2]) / 2.0f;

  BKE_boundbox_init_from_minmax(bb, min, max);

  bb->flag &= ~BOUNDBOX_DIRTY;
}

BoundBox *BKE_curve_boundbox_get(Object *ob)
{
  /* This is Object-level data access,
   * DO NOT touch to Mesh's bb, would be totally thread-unsafe. */
  if (ob->runtime.bb == NULL || ob->runtime.bb->flag & BOUNDBOX_DIRTY) {
    Curve *cu = ob->data;
    float min[3], max[3];

    INIT_MINMAX(min, max);
    BKE_curve_minmax(cu, true, min, max);

    if (ob->runtime.bb == NULL) {
      ob->runtime.bb = MEM_mallocN(sizeof(*ob->runtime.bb), __func__);
    }
    BKE_boundbox_init_from_minmax(ob->runtime.bb, min, max);
    ob->runtime.bb->flag &= ~BOUNDBOX_DIRTY;
  }

  return ob->runtime.bb;
}

void BKE_curve_texspace_calc(Curve *cu)
{
  float loc[3], size[3];
  int a;

  BKE_curve_boundbox_calc(cu, loc, size);

  if (cu->texflag & CU_AUTOSPACE) {
    for (a = 0; a < 3; a++) {
      if (size[a] == 0.0f) {
        size[a] = 1.0f;
      }
      else if (size[a] > 0.0f && size[a] < 0.00001f) {
        size[a] = 0.00001f;
      }
      else if (size[a] < 0.0f && size[a] > -0.00001f) {
        size[a] = -0.00001f;
      }
    }

    copy_v3_v3(cu->loc, loc);
    copy_v3_v3(cu->size, size);
    zero_v3(cu->rot);
  }
}

BoundBox *BKE_curve_texspace_get(Curve *cu, float r_loc[3], float r_rot[3], float r_size[3])
{
  if (cu->bb == NULL || (cu->bb->flag & BOUNDBOX_DIRTY)) {
    BKE_curve_texspace_calc(cu);
  }

  if (r_loc) {
    copy_v3_v3(r_loc, cu->loc);
  }
  if (r_rot) {
    copy_v3_v3(r_rot, cu->rot);
  }
  if (r_size) {
    copy_v3_v3(r_size, cu->size);
  }

  return cu->bb;
}

bool BKE_nurbList_index_get_co(ListBase *nurb, const int index, float r_co[3])
{
  Nurb *nu;
  int tot = 0;

  for (nu = nurb->first; nu; nu = nu->next) {
    int tot_nu;
    if (nu->type == CU_BEZIER) {
      tot_nu = nu->pntsu;
      if (index - tot < tot_nu) {
        copy_v3_v3(r_co, nu->bezt[index - tot].vec[1]);
        return true;
      }
    }
    else {
      tot_nu = nu->pntsu * nu->pntsv;
      if (index - tot < tot_nu) {
        copy_v3_v3(r_co, nu->bp[index - tot].vec);
        return true;
      }
    }
    tot += tot_nu;
  }

  return false;
}

int BKE_nurbList_verts_count(ListBase *nurb)
{
  Nurb *nu;
  int tot = 0;

  nu = nurb->first;
  while (nu) {
    if (nu->bezt) {
      tot += 3 * nu->pntsu;
    }
    else if (nu->bp) {
      tot += nu->pntsu * nu->pntsv;
    }

    nu = nu->next;
  }
  return tot;
}

int BKE_nurbList_verts_count_without_handles(ListBase *nurb)
{
  Nurb *nu;
  int tot = 0;

  nu = nurb->first;
  while (nu) {
    if (nu->bezt) {
      tot += nu->pntsu;
    }
    else if (nu->bp) {
      tot += nu->pntsu * nu->pntsv;
    }

    nu = nu->next;
  }
  return tot;
}

/* **************** NURBS ROUTINES ******************** */

void BKE_nurb_free(Nurb *nu)
{

  if (nu == NULL) {
    return;
  }

  if (nu->bezt) {
    MEM_freeN(nu->bezt);
  }
  nu->bezt = NULL;
  if (nu->bp) {
    MEM_freeN(nu->bp);
  }
  nu->bp = NULL;
  if (nu->knotsu) {
    MEM_freeN(nu->knotsu);
  }
  nu->knotsu = NULL;
  if (nu->knotsv) {
    MEM_freeN(nu->knotsv);
  }
  nu->knotsv = NULL;
  /* if (nu->trim.first) freeNurblist(&(nu->trim)); */

  MEM_freeN(nu);
}

void BKE_nurbList_free(ListBase *lb)
{
  Nurb *nu, *next;

  if (lb == NULL) {
    return;
  }

  nu = lb->first;
  while (nu) {
    next = nu->next;
    BKE_nurb_free(nu);
    nu = next;
  }
  BLI_listbase_clear(lb);
}

Nurb *BKE_nurb_duplicate(const Nurb *nu)
{
  Nurb *newnu;
  int len;

  newnu = (Nurb *)MEM_mallocN(sizeof(Nurb), "duplicateNurb");
  if (newnu == NULL) {
    return NULL;
  }
  memcpy(newnu, nu, sizeof(Nurb));

  if (nu->bezt) {
    newnu->bezt = (BezTriple *)MEM_malloc_arrayN(nu->pntsu, sizeof(BezTriple), "duplicateNurb2");
    memcpy(newnu->bezt, nu->bezt, nu->pntsu * sizeof(BezTriple));
  }
  else {
    len = nu->pntsu * nu->pntsv;
    newnu->bp = (BPoint *)MEM_malloc_arrayN(len, sizeof(BPoint), "duplicateNurb3");
    memcpy(newnu->bp, nu->bp, len * sizeof(BPoint));

    newnu->knotsu = newnu->knotsv = NULL;

    if (nu->knotsu) {
      len = KNOTSU(nu);
      if (len) {
        newnu->knotsu = MEM_malloc_arrayN(len, sizeof(float), "duplicateNurb4");
        memcpy(newnu->knotsu, nu->knotsu, sizeof(float) * len);
      }
    }
    if (nu->pntsv > 1 && nu->knotsv) {
      len = KNOTSV(nu);
      if (len) {
        newnu->knotsv = MEM_malloc_arrayN(len, sizeof(float), "duplicateNurb5");
        memcpy(newnu->knotsv, nu->knotsv, sizeof(float) * len);
      }
    }
  }
  return newnu;
}

/* copy the nurb but allow for different number of points (to be copied after this) */
Nurb *BKE_nurb_copy(Nurb *src, int pntsu, int pntsv)
{
  Nurb *newnu = (Nurb *)MEM_mallocN(sizeof(Nurb), "copyNurb");
  memcpy(newnu, src, sizeof(Nurb));

  if (pntsu == 1) {
    SWAP(int, pntsu, pntsv);
  }
  newnu->pntsu = pntsu;
  newnu->pntsv = pntsv;

  /* caller can manually handle these arrays */
  newnu->knotsu = NULL;
  newnu->knotsv = NULL;

  if (src->bezt) {
    newnu->bezt = (BezTriple *)MEM_malloc_arrayN(pntsu * pntsv, sizeof(BezTriple), "copyNurb2");
  }
  else {
    newnu->bp = (BPoint *)MEM_malloc_arrayN(pntsu * pntsv, sizeof(BPoint), "copyNurb3");
  }

  return newnu;
}

void BKE_nurbList_duplicate(ListBase *lb1, const ListBase *lb2)
{
  Nurb *nu, *nun;

  BKE_nurbList_free(lb1);

  nu = lb2->first;
  while (nu) {
    nun = BKE_nurb_duplicate(nu);
    BLI_addtail(lb1, nun);

    nu = nu->next;
  }
}

void BKE_nurb_test_2d(Nurb *nu)
{
  BezTriple *bezt;
  BPoint *bp;
  int a;

  if ((nu->flag & CU_2D) == 0) {
    return;
  }

  if (nu->type == CU_BEZIER) {
    a = nu->pntsu;
    bezt = nu->bezt;
    while (a--) {
      bezt->vec[0][2] = 0.0;
      bezt->vec[1][2] = 0.0;
      bezt->vec[2][2] = 0.0;
      bezt++;
    }
  }
  else {
    a = nu->pntsu * nu->pntsv;
    bp = nu->bp;
    while (a--) {
      bp->vec[2] = 0.0;
      bp++;
    }
  }
}

/**
 * if use_radius is truth, minmax will take points' radius into account,
 * which will make boundbox closer to beveled curve.
 */
void BKE_nurb_minmax(Nurb *nu, bool use_radius, float min[3], float max[3])
{
  BezTriple *bezt;
  BPoint *bp;
  int a;
  float point[3];

  if (nu->type == CU_BEZIER) {
    a = nu->pntsu;
    bezt = nu->bezt;
    while (a--) {
      if (use_radius) {
        float radius_vector[3];
        radius_vector[0] = radius_vector[1] = radius_vector[2] = bezt->radius;

        add_v3_v3v3(point, bezt->vec[1], radius_vector);
        minmax_v3v3_v3(min, max, point);

        sub_v3_v3v3(point, bezt->vec[1], radius_vector);
        minmax_v3v3_v3(min, max, point);
      }
      else {
        minmax_v3v3_v3(min, max, bezt->vec[1]);
      }
      minmax_v3v3_v3(min, max, bezt->vec[0]);
      minmax_v3v3_v3(min, max, bezt->vec[2]);
      bezt++;
    }
  }
  else {
    a = nu->pntsu * nu->pntsv;
    bp = nu->bp;
    while (a--) {
      if (nu->pntsv == 1 && use_radius) {
        float radius_vector[3];
        radius_vector[0] = radius_vector[1] = radius_vector[2] = bp->radius;

        add_v3_v3v3(point, bp->vec, radius_vector);
        minmax_v3v3_v3(min, max, point);

        sub_v3_v3v3(point, bp->vec, radius_vector);
        minmax_v3v3_v3(min, max, point);
      }
      else {
        /* Surfaces doesn't use bevel, so no need to take radius into account. */
        minmax_v3v3_v3(min, max, bp->vec);
      }
      bp++;
    }
  }
}

float BKE_nurb_calc_length(const Nurb *nu, int resolution)
{
  BezTriple *bezt, *prevbezt;
  BPoint *bp, *prevbp;
  int a, b;
  float length = 0.0f;
  int resolu = resolution ? resolution : nu->resolu;
  int pntsu = nu->pntsu;
  float *points, *pntsit, *prevpntsit;

  if (nu->type == CU_POLY) {
    a = nu->pntsu - 1;
    bp = nu->bp;
    if (nu->flagu & CU_NURB_CYCLIC) {
      ++a;
      prevbp = nu->bp + (nu->pntsu - 1);
    }
    else {
      prevbp = bp;
      bp++;
    }

    while (a--) {
      length += len_v3v3(prevbp->vec, bp->vec);
      prevbp = bp;
      ++bp;
    }
  }
  else if (nu->type == CU_BEZIER) {
    points = MEM_mallocN(sizeof(float[3]) * (resolu + 1), "getLength_bezier");
    a = nu->pntsu - 1;
    bezt = nu->bezt;
    if (nu->flagu & CU_NURB_CYCLIC) {
      ++a;
      prevbezt = nu->bezt + (nu->pntsu - 1);
    }
    else {
      prevbezt = bezt;
      ++bezt;
    }

    while (a--) {
      if (prevbezt->h2 == HD_VECT && bezt->h1 == HD_VECT) {
        length += len_v3v3(prevbezt->vec[1], bezt->vec[1]);
      }
      else {
        for (int j = 0; j < 3; j++) {
          BKE_curve_forward_diff_bezier(prevbezt->vec[1][j],
                                        prevbezt->vec[2][j],
                                        bezt->vec[0][j],
                                        bezt->vec[1][j],
                                        points + j,
                                        resolu,
                                        3 * sizeof(float));
        }

        prevpntsit = pntsit = points;
        b = resolu;
        while (b--) {
          pntsit += 3;
          length += len_v3v3(prevpntsit, pntsit);
          prevpntsit = pntsit;
        }
      }
      prevbezt = bezt;
      ++bezt;
    }

    MEM_freeN(points);
  }
  else if (nu->type == CU_NURBS) {
    if (nu->pntsv == 1) {
      /* important to zero for BKE_nurb_makeCurve. */
      points = MEM_callocN(sizeof(float[3]) * pntsu * resolu, "getLength_nurbs");

      BKE_nurb_makeCurve(nu, points, NULL, NULL, NULL, resolu, sizeof(float[3]));

      if (nu->flagu & CU_NURB_CYCLIC) {
        b = pntsu * resolu + 1;
        prevpntsit = points + 3 * (pntsu * resolu - 1);
        pntsit = points;
      }
      else {
        b = (pntsu - 1) * resolu;
        prevpntsit = points;
        pntsit = points + 3;
      }

      while (--b) {
        length += len_v3v3(prevpntsit, pntsit);
        prevpntsit = pntsit;
        pntsit += 3;
      }

      MEM_freeN(points);
    }
  }

  return length;
}

/* be sure to call makeknots after this */
void BKE_nurb_points_add(Nurb *nu, int number)
{
  BPoint *bp;
  int i;

  nu->bp = MEM_recallocN(nu->bp, (nu->pntsu + number) * sizeof(BPoint));

  for (i = 0, bp = &nu->bp[nu->pntsu]; i < number; i++, bp++) {
    bp->radius = 1.0f;
  }

  nu->pntsu += number;
}

void BKE_nurb_bezierPoints_add(Nurb *nu, int number)
{
  BezTriple *bezt;
  int i;

  nu->bezt = MEM_recallocN(nu->bezt, (nu->pntsu + number) * sizeof(BezTriple));

  for (i = 0, bezt = &nu->bezt[nu->pntsu]; i < number; i++, bezt++) {
    bezt->radius = 1.0f;
  }

  nu->pntsu += number;
}

int BKE_nurb_index_from_uv(Nurb *nu, int u, int v)
{
  const int totu = nu->pntsu;
  const int totv = nu->pntsv;

  if (nu->flagu & CU_NURB_CYCLIC) {
    u = mod_i(u, totu);
  }
  else if (u < 0 || u >= totu) {
    return -1;
  }

  if (nu->flagv & CU_NURB_CYCLIC) {
    v = mod_i(v, totv);
  }
  else if (v < 0 || v >= totv) {
    return -1;
  }

  return (v * totu) + u;
}

void BKE_nurb_index_to_uv(Nurb *nu, int index, int *r_u, int *r_v)
{
  const int totu = nu->pntsu;
  const int totv = nu->pntsv;
  BLI_assert(index >= 0 && index < (nu->pntsu * nu->pntsv));
  *r_u = (index % totu);
  *r_v = (index / totu) % totv;
}

BezTriple *BKE_nurb_bezt_get_next(Nurb *nu, BezTriple *bezt)
{
  BezTriple *bezt_next;

  BLI_assert(ARRAY_HAS_ITEM(bezt, nu->bezt, nu->pntsu));

  if (bezt == &nu->bezt[nu->pntsu - 1]) {
    if (nu->flagu & CU_NURB_CYCLIC) {
      bezt_next = nu->bezt;
    }
    else {
      bezt_next = NULL;
    }
  }
  else {
    bezt_next = bezt + 1;
  }

  return bezt_next;
}

BPoint *BKE_nurb_bpoint_get_next(Nurb *nu, BPoint *bp)
{
  BPoint *bp_next;

  BLI_assert(ARRAY_HAS_ITEM(bp, nu->bp, nu->pntsu));

  if (bp == &nu->bp[nu->pntsu - 1]) {
    if (nu->flagu & CU_NURB_CYCLIC) {
      bp_next = nu->bp;
    }
    else {
      bp_next = NULL;
    }
  }
  else {
    bp_next = bp + 1;
  }

  return bp_next;
}

BezTriple *BKE_nurb_bezt_get_prev(Nurb *nu, BezTriple *bezt)
{
  BezTriple *bezt_prev;

  BLI_assert(ARRAY_HAS_ITEM(bezt, nu->bezt, nu->pntsu));
  BLI_assert(nu->pntsv <= 1);

  if (bezt == nu->bezt) {
    if (nu->flagu & CU_NURB_CYCLIC) {
      bezt_prev = &nu->bezt[nu->pntsu - 1];
    }
    else {
      bezt_prev = NULL;
    }
  }
  else {
    bezt_prev = bezt - 1;
  }

  return bezt_prev;
}

BPoint *BKE_nurb_bpoint_get_prev(Nurb *nu, BPoint *bp)
{
  BPoint *bp_prev;

  BLI_assert(ARRAY_HAS_ITEM(bp, nu->bp, nu->pntsu));
  BLI_assert(nu->pntsv == 1);

  if (bp == nu->bp) {
    if (nu->flagu & CU_NURB_CYCLIC) {
      bp_prev = &nu->bp[nu->pntsu - 1];
    }
    else {
      bp_prev = NULL;
    }
  }
  else {
    bp_prev = bp - 1;
  }

  return bp_prev;
}

void BKE_nurb_bezt_calc_normal(struct Nurb *UNUSED(nu), BezTriple *bezt, float r_normal[3])
{
  /* calculate the axis matrix from the spline */
  float dir_prev[3], dir_next[3];

  sub_v3_v3v3(dir_prev, bezt->vec[0], bezt->vec[1]);
  sub_v3_v3v3(dir_next, bezt->vec[1], bezt->vec[2]);

  normalize_v3(dir_prev);
  normalize_v3(dir_next);

  add_v3_v3v3(r_normal, dir_prev, dir_next);
  normalize_v3(r_normal);
}

void BKE_nurb_bezt_calc_plane(struct Nurb *nu, BezTriple *bezt, float r_plane[3])
{
  float dir_prev[3], dir_next[3];

  sub_v3_v3v3(dir_prev, bezt->vec[0], bezt->vec[1]);
  sub_v3_v3v3(dir_next, bezt->vec[1], bezt->vec[2]);

  normalize_v3(dir_prev);
  normalize_v3(dir_next);

  cross_v3_v3v3(r_plane, dir_prev, dir_next);
  if (normalize_v3(r_plane) < FLT_EPSILON) {
    BezTriple *bezt_prev = BKE_nurb_bezt_get_prev(nu, bezt);
    BezTriple *bezt_next = BKE_nurb_bezt_get_next(nu, bezt);

    if (bezt_prev) {
      sub_v3_v3v3(dir_prev, bezt_prev->vec[1], bezt->vec[1]);
      normalize_v3(dir_prev);
    }
    if (bezt_next) {
      sub_v3_v3v3(dir_next, bezt->vec[1], bezt_next->vec[1]);
      normalize_v3(dir_next);
    }
    cross_v3_v3v3(r_plane, dir_prev, dir_next);
  }

  /* matches with bones more closely */
  {
    float dir_mid[3], tvec[3];
    add_v3_v3v3(dir_mid, dir_prev, dir_next);
    cross_v3_v3v3(tvec, r_plane, dir_mid);
    copy_v3_v3(r_plane, tvec);
  }

  normalize_v3(r_plane);
}

void BKE_nurb_bpoint_calc_normal(struct Nurb *nu, BPoint *bp, float r_normal[3])
{
  BPoint *bp_prev = BKE_nurb_bpoint_get_prev(nu, bp);
  BPoint *bp_next = BKE_nurb_bpoint_get_next(nu, bp);

  zero_v3(r_normal);

  if (bp_prev) {
    float dir_prev[3];
    sub_v3_v3v3(dir_prev, bp_prev->vec, bp->vec);
    normalize_v3(dir_prev);
    add_v3_v3(r_normal, dir_prev);
  }
  if (bp_next) {
    float dir_next[3];
    sub_v3_v3v3(dir_next, bp->vec, bp_next->vec);
    normalize_v3(dir_next);
    add_v3_v3(r_normal, dir_next);
  }

  normalize_v3(r_normal);
}

void BKE_nurb_bpoint_calc_plane(struct Nurb *nu, BPoint *bp, float r_plane[3])
{
  BPoint *bp_prev = BKE_nurb_bpoint_get_prev(nu, bp);
  BPoint *bp_next = BKE_nurb_bpoint_get_next(nu, bp);

  float dir_prev[3] = {0.0f}, dir_next[3] = {0.0f};

  if (bp_prev) {
    sub_v3_v3v3(dir_prev, bp_prev->vec, bp->vec);
    normalize_v3(dir_prev);
  }
  if (bp_next) {
    sub_v3_v3v3(dir_next, bp->vec, bp_next->vec);
    normalize_v3(dir_next);
  }
  cross_v3_v3v3(r_plane, dir_prev, dir_next);

  /* matches with bones more closely */
  {
    float dir_mid[3], tvec[3];
    add_v3_v3v3(dir_mid, dir_prev, dir_next);
    cross_v3_v3v3(tvec, r_plane, dir_mid);
    copy_v3_v3(r_plane, tvec);
  }

  normalize_v3(r_plane);
}

/* ~~~~~~~~~~~~~~~~~~~~Non Uniform Rational B Spline calculations ~~~~~~~~~~~ */

static void calcknots(float *knots, const int pnts, const short order, const short flag)
{
  /* knots: number of pnts NOT corrected for cyclic */
  const int pnts_order = pnts + order;
  float k;
  int a;

  switch (flag & (CU_NURB_ENDPOINT | CU_NURB_BEZIER)) {
    case CU_NURB_ENDPOINT:
      k = 0.0;
      for (a = 1; a <= pnts_order; a++) {
        knots[a - 1] = k;
        if (a >= order && a <= pnts) {
          k += 1.0f;
        }
      }
      break;
    case CU_NURB_BEZIER:
      /* Warning, the order MUST be 2 or 4,
       * if this is not enforced, the displist will be corrupt */
      if (order == 4) {
        k = 0.34;
        for (a = 0; a < pnts_order; a++) {
          knots[a] = floorf(k);
          k += (1.0f / 3.0f);
        }
      }
      else if (order == 3) {
        k = 0.6f;
        for (a = 0; a < pnts_order; a++) {
          if (a >= order && a <= pnts) {
            k += 0.5f;
          }
          knots[a] = floorf(k);
        }
      }
      else {
        CLOG_ERROR(&LOG, "bez nurb curve order is not 3 or 4, should never happen");
      }
      break;
    default:
      for (a = 0; a < pnts_order; a++) {
        knots[a] = (float)a;
      }
      break;
  }
}

static void makecyclicknots(float *knots, int pnts, short order)
/* pnts, order: number of pnts NOT corrected for cyclic */
{
  int a, b, order2, c;

  if (knots == NULL) {
    return;
  }

  order2 = order - 1;

  /* do first long rows (order -1), remove identical knots at endpoints */
  if (order > 2) {
    b = pnts + order2;
    for (a = 1; a < order2; a++) {
      if (knots[b] != knots[b - a]) {
        break;
      }
    }
    if (a == order2) {
      knots[pnts + order - 2] += 1.0f;
    }
  }

  b = order;
  c = pnts + order + order2;
  for (a = pnts + order2; a < c; a++) {
    knots[a] = knots[a - 1] + (knots[b] - knots[b - 1]);
    b--;
  }
}

static void makeknots(Nurb *nu, short uv)
{
  if (nu->type == CU_NURBS) {
    if (uv == 1) {
      if (nu->knotsu) {
        MEM_freeN(nu->knotsu);
      }
      if (BKE_nurb_check_valid_u(nu)) {
        nu->knotsu = MEM_calloc_arrayN(KNOTSU(nu) + 1, sizeof(float), "makeknots");
        if (nu->flagu & CU_NURB_CYCLIC) {
          calcknots(nu->knotsu, nu->pntsu, nu->orderu, 0); /* cyclic should be uniform */
          makecyclicknots(nu->knotsu, nu->pntsu, nu->orderu);
        }
        else {
          calcknots(nu->knotsu, nu->pntsu, nu->orderu, nu->flagu);
        }
      }
      else {
        nu->knotsu = NULL;
      }
    }
    else if (uv == 2) {
      if (nu->knotsv) {
        MEM_freeN(nu->knotsv);
      }
      if (BKE_nurb_check_valid_v(nu)) {
        nu->knotsv = MEM_calloc_arrayN(KNOTSV(nu) + 1, sizeof(float), "makeknots");
        if (nu->flagv & CU_NURB_CYCLIC) {
          calcknots(nu->knotsv, nu->pntsv, nu->orderv, 0); /* cyclic should be uniform */
          makecyclicknots(nu->knotsv, nu->pntsv, nu->orderv);
        }
        else {
          calcknots(nu->knotsv, nu->pntsv, nu->orderv, nu->flagv);
        }
      }
      else {
        nu->knotsv = NULL;
      }
    }
  }
}

void BKE_nurb_knot_calc_u(Nurb *nu)
{
  makeknots(nu, 1);
}

void BKE_nurb_knot_calc_v(Nurb *nu)
{
  makeknots(nu, 2);
}

static void basisNurb(
    float t, short order, int pnts, float *knots, float *basis, int *start, int *end)
{
  float d, e;
  int i, i1 = 0, i2 = 0, j, orderpluspnts, opp2, o2;

  orderpluspnts = order + pnts;
  opp2 = orderpluspnts - 1;

  /* this is for float inaccuracy */
  if (t < knots[0]) {
    t = knots[0];
  }
  else if (t > knots[opp2]) {
    t = knots[opp2];
  }

  /* this part is order '1' */
  o2 = order + 1;
  for (i = 0; i < opp2; i++) {
    if (knots[i] != knots[i + 1] && t >= knots[i] && t <= knots[i + 1]) {
      basis[i] = 1.0;
      i1 = i - o2;
      if (i1 < 0) {
        i1 = 0;
      }
      i2 = i;
      i++;
      while (i < opp2) {
        basis[i] = 0.0;
        i++;
      }
      break;
    }
    else {
      basis[i] = 0.0;
    }
  }
  basis[i] = 0.0;

  /* this is order 2, 3, ... */
  for (j = 2; j <= order; j++) {

    if (i2 + j >= orderpluspnts) {
      i2 = opp2 - j;
    }

    for (i = i1; i <= i2; i++) {
      if (basis[i] != 0.0f) {
        d = ((t - knots[i]) * basis[i]) / (knots[i + j - 1] - knots[i]);
      }
      else {
        d = 0.0f;
      }

      if (basis[i + 1] != 0.0f) {
        e = ((knots[i + j] - t) * basis[i + 1]) / (knots[i + j] - knots[i + 1]);
      }
      else {
        e = 0.0;
      }

      basis[i] = d + e;
    }
  }

  *start = 1000;
  *end = 0;

  for (i = i1; i <= i2; i++) {
    if (basis[i] > 0.0f) {
      *end = i;
      if (*start == 1000) {
        *start = i;
      }
    }
  }
}

/**
 * \param coord_array: has to be (3 * 4 * resolu * resolv) in size, and zero-ed.
 */
void BKE_nurb_makeFaces(const Nurb *nu, float *coord_array, int rowstride, int resolu, int resolv)
{
  BPoint *bp;
  float *basisu, *basis, *basisv, *sum, *fp, *in;
  float u, v, ustart, uend, ustep, vstart, vend, vstep, sumdiv;
  int i, j, iofs, jofs, cycl, len, curu, curv;
  int istart, iend, jsta, jen, *jstart, *jend, ratcomp;

  int totu = nu->pntsu * resolu, totv = nu->pntsv * resolv;

  if (nu->knotsu == NULL || nu->knotsv == NULL) {
    return;
  }
  if (nu->orderu > nu->pntsu) {
    return;
  }
  if (nu->orderv > nu->pntsv) {
    return;
  }
  if (coord_array == NULL) {
    return;
  }

  /* allocate and initialize */
  len = totu * totv;
  if (len == 0) {
    return;
  }

  sum = (float *)MEM_calloc_arrayN(len, sizeof(float), "makeNurbfaces1");

  bp = nu->bp;
  i = nu->pntsu * nu->pntsv;
  ratcomp = 0;
  while (i--) {
    if (bp->vec[3] != 1.0f) {
      ratcomp = 1;
      break;
    }
    bp++;
  }

  fp = nu->knotsu;
  ustart = fp[nu->orderu - 1];
  if (nu->flagu & CU_NURB_CYCLIC) {
    uend = fp[nu->pntsu + nu->orderu - 1];
  }
  else {
    uend = fp[nu->pntsu];
  }
  ustep = (uend - ustart) / ((nu->flagu & CU_NURB_CYCLIC) ? totu : totu - 1);

  basisu = (float *)MEM_malloc_arrayN(KNOTSU(nu), sizeof(float), "makeNurbfaces3");

  fp = nu->knotsv;
  vstart = fp[nu->orderv - 1];

  if (nu->flagv & CU_NURB_CYCLIC) {
    vend = fp[nu->pntsv + nu->orderv - 1];
  }
  else {
    vend = fp[nu->pntsv];
  }
  vstep = (vend - vstart) / ((nu->flagv & CU_NURB_CYCLIC) ? totv : totv - 1);

  len = KNOTSV(nu);
  basisv = (float *)MEM_malloc_arrayN(len * totv, sizeof(float), "makeNurbfaces3");
  jstart = (int *)MEM_malloc_arrayN(totv, sizeof(float), "makeNurbfaces4");
  jend = (int *)MEM_malloc_arrayN(totv, sizeof(float), "makeNurbfaces5");

  /* precalculation of basisv and jstart, jend */
  if (nu->flagv & CU_NURB_CYCLIC) {
    cycl = nu->orderv - 1;
  }
  else {
    cycl = 0;
  }
  v = vstart;
  basis = basisv;
  curv = totv;
  while (curv--) {
    basisNurb(v, nu->orderv, nu->pntsv + cycl, nu->knotsv, basis, jstart + curv, jend + curv);
    basis += KNOTSV(nu);
    v += vstep;
  }

  if (nu->flagu & CU_NURB_CYCLIC) {
    cycl = nu->orderu - 1;
  }
  else {
    cycl = 0;
  }
  in = coord_array;
  u = ustart;
  curu = totu;
  while (curu--) {
    basisNurb(u, nu->orderu, nu->pntsu + cycl, nu->knotsu, basisu, &istart, &iend);

    basis = basisv;
    curv = totv;
    while (curv--) {
      jsta = jstart[curv];
      jen = jend[curv];

      /* calculate sum */
      sumdiv = 0.0;
      fp = sum;

      for (j = jsta; j <= jen; j++) {

        if (j >= nu->pntsv) {
          jofs = (j - nu->pntsv);
        }
        else {
          jofs = j;
        }
        bp = nu->bp + nu->pntsu * jofs + istart - 1;

        for (i = istart; i <= iend; i++, fp++) {
          if (i >= nu->pntsu) {
            iofs = i - nu->pntsu;
            bp = nu->bp + nu->pntsu * jofs + iofs;
          }
          else {
            bp++;
          }

          if (ratcomp) {
            *fp = basisu[i] * basis[j] * bp->vec[3];
            sumdiv += *fp;
          }
          else {
            *fp = basisu[i] * basis[j];
          }
        }
      }

      if (ratcomp) {
        fp = sum;
        for (j = jsta; j <= jen; j++) {
          for (i = istart; i <= iend; i++, fp++) {
            *fp /= sumdiv;
          }
        }
      }

      zero_v3(in);

      /* one! (1.0) real point now */
      fp = sum;
      for (j = jsta; j <= jen; j++) {

        if (j >= nu->pntsv) {
          jofs = (j - nu->pntsv);
        }
        else {
          jofs = j;
        }
        bp = nu->bp + nu->pntsu * jofs + istart - 1;

        for (i = istart; i <= iend; i++, fp++) {
          if (i >= nu->pntsu) {
            iofs = i - nu->pntsu;
            bp = nu->bp + nu->pntsu * jofs + iofs;
          }
          else {
            bp++;
          }

          if (*fp != 0.0f) {
            madd_v3_v3fl(in, bp->vec, *fp);
          }
        }
      }

      in += 3;
      basis += KNOTSV(nu);
    }
    u += ustep;
    if (rowstride != 0) {
      in = (float *)(((unsigned char *)in) + (rowstride - 3 * totv * sizeof(*in)));
    }
  }

  /* free */
  MEM_freeN(sum);
  MEM_freeN(basisu);
  MEM_freeN(basisv);
  MEM_freeN(jstart);
  MEM_freeN(jend);
}

/**
 * \param coord_array: Has to be 3 * 4 * pntsu * resolu in size and zero-ed
 * \param tilt_array: set when non-NULL
 * \param radius_array: set when non-NULL
 */
void BKE_nurb_makeCurve(const Nurb *nu,
                        float *coord_array,
                        float *tilt_array,
                        float *radius_array,
                        float *weight_array,
                        int resolu,
                        int stride)
{
  const float eps = 1e-6f;
  BPoint *bp;
  float u, ustart, uend, ustep, sumdiv;
  float *basisu, *sum, *fp;
  float *coord_fp = coord_array, *tilt_fp = tilt_array, *radius_fp = radius_array,
        *weight_fp = weight_array;
  int i, len, istart, iend, cycl;

  if (nu->knotsu == NULL) {
    return;
  }
  if (nu->orderu > nu->pntsu) {
    return;
  }
  if (coord_array == NULL) {
    return;
  }

  /* allocate and initialize */
  len = nu->pntsu;
  if (len == 0) {
    return;
  }
  sum = (float *)MEM_calloc_arrayN(len, sizeof(float), "makeNurbcurve1");

  resolu = (resolu * SEGMENTSU(nu));

  if (resolu == 0) {
    MEM_freeN(sum);
    return;
  }

  fp = nu->knotsu;
  ustart = fp[nu->orderu - 1];
  if (nu->flagu & CU_NURB_CYCLIC) {
    uend = fp[nu->pntsu + nu->orderu - 1];
  }
  else {
    uend = fp[nu->pntsu];
  }
  ustep = (uend - ustart) / (resolu - ((nu->flagu & CU_NURB_CYCLIC) ? 0 : 1));

  basisu = (float *)MEM_malloc_arrayN(KNOTSU(nu), sizeof(float), "makeNurbcurve3");

  if (nu->flagu & CU_NURB_CYCLIC) {
    cycl = nu->orderu - 1;
  }
  else {
    cycl = 0;
  }

  u = ustart;
  while (resolu--) {
    basisNurb(u, nu->orderu, nu->pntsu + cycl, nu->knotsu, basisu, &istart, &iend);

    /* calc sum */
    sumdiv = 0.0;
    fp = sum;
    bp = nu->bp + istart - 1;
    for (i = istart; i <= iend; i++, fp++) {
      if (i >= nu->pntsu) {
        bp = nu->bp + (i - nu->pntsu);
      }
      else {
        bp++;
      }

      *fp = basisu[i] * bp->vec[3];
      sumdiv += *fp;
    }
    if ((sumdiv != 0.0f) && (sumdiv < 1.0f - eps || sumdiv > 1.0f + eps)) {
      /* is normalizing needed? */
      fp = sum;
      for (i = istart; i <= iend; i++, fp++) {
        *fp /= sumdiv;
      }
    }

    zero_v3(coord_fp);

    /* one! (1.0) real point */
    fp = sum;
    bp = nu->bp + istart - 1;
    for (i = istart; i <= iend; i++, fp++) {
      if (i >= nu->pntsu) {
        bp = nu->bp + (i - nu->pntsu);
      }
      else {
        bp++;
      }

      if (*fp != 0.0f) {
        madd_v3_v3fl(coord_fp, bp->vec, *fp);

        if (tilt_fp) {
          (*tilt_fp) += (*fp) * bp->tilt;
        }

        if (radius_fp) {
          (*radius_fp) += (*fp) * bp->radius;
        }

        if (weight_fp) {
          (*weight_fp) += (*fp) * bp->weight;
        }
      }
    }

    coord_fp = POINTER_OFFSET(coord_fp, stride);

    if (tilt_fp) {
      tilt_fp = POINTER_OFFSET(tilt_fp, stride);
    }
    if (radius_fp) {
      radius_fp = POINTER_OFFSET(radius_fp, stride);
    }
    if (weight_fp) {
      weight_fp = POINTER_OFFSET(weight_fp, stride);
    }

    u += ustep;
  }

  /* free */
  MEM_freeN(sum);
  MEM_freeN(basisu);
}

/**
 * Calculate the length for arrays filled in by #BKE_curve_calc_coords_axis.
 */
unsigned int BKE_curve_calc_coords_axis_len(const unsigned int bezt_array_len,
                                            const unsigned int resolu,
                                            const bool is_cyclic,
                                            const bool use_cyclic_duplicate_endpoint)
{
  const unsigned int segments = bezt_array_len - (is_cyclic ? 0 : 1);
  const unsigned int points_len = (segments * resolu) +
                                  (is_cyclic ? (use_cyclic_duplicate_endpoint) : 1);
  return points_len;
}

/**
 * Calculate an array for the entire curve (cyclic or non-cyclic).
 * \note Call for each axis.
 *
 * \param use_cyclic_duplicate_endpoint: Duplicate values at the beginning & end of the array.
 */
void BKE_curve_calc_coords_axis(const BezTriple *bezt_array,
                                const unsigned int bezt_array_len,
                                const unsigned int resolu,
                                const bool is_cyclic,
                                const bool use_cyclic_duplicate_endpoint,
                                /* array params */
                                const unsigned int axis,
                                const unsigned int stride,
                                float *r_points)
{
  const unsigned int points_len = BKE_curve_calc_coords_axis_len(
      bezt_array_len, resolu, is_cyclic, use_cyclic_duplicate_endpoint);
  float *r_points_offset = r_points;

  const unsigned int resolu_stride = resolu * stride;
  const unsigned int bezt_array_last = bezt_array_len - 1;

  for (unsigned int i = 0; i < bezt_array_last; i++) {
    const BezTriple *bezt_curr = &bezt_array[i];
    const BezTriple *bezt_next = &bezt_array[i + 1];
    BKE_curve_forward_diff_bezier(bezt_curr->vec[1][axis],
                                  bezt_curr->vec[2][axis],
                                  bezt_next->vec[0][axis],
                                  bezt_next->vec[1][axis],
                                  r_points_offset,
                                  (int)resolu,
                                  stride);
    r_points_offset = POINTER_OFFSET(r_points_offset, resolu_stride);
  }

  if (is_cyclic) {
    const BezTriple *bezt_curr = &bezt_array[bezt_array_last];
    const BezTriple *bezt_next = &bezt_array[0];
    BKE_curve_forward_diff_bezier(bezt_curr->vec[1][axis],
                                  bezt_curr->vec[2][axis],
                                  bezt_next->vec[0][axis],
                                  bezt_next->vec[1][axis],
                                  r_points_offset,
                                  (int)resolu,
                                  stride);
    r_points_offset = POINTER_OFFSET(r_points_offset, resolu_stride);
    if (use_cyclic_duplicate_endpoint) {
      *r_points_offset = *r_points;
      r_points_offset = POINTER_OFFSET(r_points_offset, stride);
    }
  }
  else {
    float *r_points_last = POINTER_OFFSET(r_points, bezt_array_last * resolu_stride);
    *r_points_last = bezt_array[bezt_array_last].vec[1][axis];
    r_points_offset = POINTER_OFFSET(r_points_offset, stride);
  }

  BLI_assert(POINTER_OFFSET(r_points, points_len * stride) == r_points_offset);
  UNUSED_VARS_NDEBUG(points_len);
}

/* forward differencing method for bezier curve */
void BKE_curve_forward_diff_bezier(
    float q0, float q1, float q2, float q3, float *p, int it, int stride)
{
  float rt0, rt1, rt2, rt3, f;
  int a;

  f = (float)it;
  rt0 = q0;
  rt1 = 3.0f * (q1 - q0) / f;
  f *= f;
  rt2 = 3.0f * (q0 - 2.0f * q1 + q2) / f;
  f *= it;
  rt3 = (q3 - q0 + 3.0f * (q1 - q2)) / f;

  q0 = rt0;
  q1 = rt1 + rt2 + rt3;
  q2 = 2 * rt2 + 6 * rt3;
  q3 = 6 * rt3;

  for (a = 0; a <= it; a++) {
    *p = q0;
    p = POINTER_OFFSET(p, stride);
    q0 += q1;
    q1 += q2;
    q2 += q3;
  }
}

/* forward differencing method for first derivative of cubic bezier curve */
void BKE_curve_forward_diff_tangent_bezier(
    float q0, float q1, float q2, float q3, float *p, int it, int stride)
{
  float rt0, rt1, rt2, f;
  int a;

  f = 1.0f / (float)it;

  rt0 = 3.0f * (q1 - q0);
  rt1 = f * (3.0f * (q3 - q0) + 9.0f * (q1 - q2));
  rt2 = 6.0f * (q0 + q2) - 12.0f * q1;

  q0 = rt0;
  q1 = f * (rt1 + rt2);
  q2 = 2.0f * f * rt1;

  for (a = 0; a <= it; a++) {
    *p = q0;
    p = POINTER_OFFSET(p, stride);
    q0 += q1;
    q1 += q2;
  }
}

static void forward_diff_bezier_cotangent(const float p0[3],
                                          const float p1[3],
                                          const float p2[3],
                                          const float p3[3],
                                          float p[3],
                                          int it,
                                          int stride)
{
  /* note that these are not perpendicular to the curve
   * they need to be rotated for this,
   *
   * This could also be optimized like BKE_curve_forward_diff_bezier */
  int a;
  for (a = 0; a <= it; a++) {
    float t = (float)a / (float)it;

    int i;
    for (i = 0; i < 3; i++) {
      p[i] = (-6.0f * t + 6.0f) * p0[i] + (18.0f * t - 12.0f) * p1[i] +
             (-18.0f * t + 6.0f) * p2[i] + (6.0f * t) * p3[i];
    }
    normalize_v3(p);
    p = POINTER_OFFSET(p, stride);
  }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

float *BKE_curve_surf_make_orco(Object *ob)
{
  /* Note: this function is used in convertblender only atm, so
   * suppose nonzero curve's render resolution should always be used */
  Curve *cu = ob->data;
  Nurb *nu;
  int a, b, tot = 0;
  int sizeu, sizev;
  int resolu, resolv;
  float *fp, *coord_array;

  /* first calculate the size of the datablock */
  nu = cu->nurb.first;
  while (nu) {
    /* as we want to avoid the seam in a cyclic nurbs
     * texture wrapping, reserve extra orco data space to save these extra needed
     * vertex based UV coordinates for the meridian vertices.
     * Vertices on the 0/2pi boundary are not duplicated inside the displist but later in
     * the renderface/vert construction.
     *
     * See also convertblender.c: init_render_surf()
     */

    resolu = cu->resolu_ren ? cu->resolu_ren : nu->resolu;
    resolv = cu->resolv_ren ? cu->resolv_ren : nu->resolv;

    sizeu = nu->pntsu * resolu;
    sizev = nu->pntsv * resolv;
    if (nu->flagu & CU_NURB_CYCLIC) {
      sizeu++;
    }
    if (nu->flagv & CU_NURB_CYCLIC) {
      sizev++;
    }
    if (nu->pntsv > 1) {
      tot += sizeu * sizev;
    }

    nu = nu->next;
  }
  /* makeNurbfaces wants zeros */
  fp = coord_array = MEM_calloc_arrayN(tot, 3 * sizeof(float), "make_orco");

  nu = cu->nurb.first;
  while (nu) {
    resolu = cu->resolu_ren ? cu->resolu_ren : nu->resolu;
    resolv = cu->resolv_ren ? cu->resolv_ren : nu->resolv;

    if (nu->pntsv > 1) {
      sizeu = nu->pntsu * resolu;
      sizev = nu->pntsv * resolv;

      if (nu->flagu & CU_NURB_CYCLIC) {
        sizeu++;
      }
      if (nu->flagv & CU_NURB_CYCLIC) {
        sizev++;
      }

      if (cu->flag & CU_UV_ORCO) {
        for (b = 0; b < sizeu; b++) {
          for (a = 0; a < sizev; a++) {

            if (sizev < 2) {
              fp[0] = 0.0f;
            }
            else {
              fp[0] = -1.0f + 2.0f * ((float)a) / (sizev - 1);
            }

            if (sizeu < 2) {
              fp[1] = 0.0f;
            }
            else {
              fp[1] = -1.0f + 2.0f * ((float)b) / (sizeu - 1);
            }

            fp[2] = 0.0;

            fp += 3;
          }
        }
      }
      else {
        int size = (nu->pntsu * resolu) * (nu->pntsv * resolv) * 3 * sizeof(float);
        float *_tdata = MEM_mallocN(size, "temp data");
        float *tdata = _tdata;

        BKE_nurb_makeFaces(nu, tdata, 0, resolu, resolv);

        for (b = 0; b < sizeu; b++) {
          int use_b = b;
          if (b == sizeu - 1 && (nu->flagu & CU_NURB_CYCLIC)) {
            use_b = false;
          }

          for (a = 0; a < sizev; a++) {
            int use_a = a;
            if (a == sizev - 1 && (nu->flagv & CU_NURB_CYCLIC)) {
              use_a = false;
            }

            tdata = _tdata + 3 * (use_b * (nu->pntsv * resolv) + use_a);

            fp[0] = (tdata[0] - cu->loc[0]) / cu->size[0];
            fp[1] = (tdata[1] - cu->loc[1]) / cu->size[1];
            fp[2] = (tdata[2] - cu->loc[2]) / cu->size[2];
            fp += 3;
          }
        }

        MEM_freeN(_tdata);
      }
    }
    nu = nu->next;
  }

  return coord_array;
}

/* NOTE: This routine is tied to the order of vertex
 * built by displist and as passed to the renderer.
 */
float *BKE_curve_make_orco(Depsgraph *depsgraph, Scene *scene, Object *ob, int *r_numVerts)
{
  Curve *cu = ob->data;
  DispList *dl;
  int u, v, numVerts;
  float *fp, *coord_array;
  ListBase disp = {NULL, NULL};

  BKE_displist_make_curveTypes_forOrco(depsgraph, scene, ob, &disp, NULL);

  numVerts = 0;
  for (dl = disp.first; dl; dl = dl->next) {
    if (dl->type == DL_INDEX3) {
      numVerts += dl->nr;
    }
    else if (dl->type == DL_SURF) {
      /* convertblender.c uses the Surface code for creating renderfaces when cyclic U only
       * (closed circle beveling)
       */
      if (dl->flag & DL_CYCL_U) {
        if (dl->flag & DL_CYCL_V) {
          numVerts += (dl->parts + 1) * (dl->nr + 1);
        }
        else {
          numVerts += dl->parts * (dl->nr + 1);
        }
      }
      else if (dl->flag & DL_CYCL_V) {
        numVerts += (dl->parts + 1) * dl->nr;
      }
      else {
        numVerts += dl->parts * dl->nr;
      }
    }
  }

  if (r_numVerts) {
    *r_numVerts = numVerts;
  }

  fp = coord_array = MEM_malloc_arrayN(numVerts, 3 * sizeof(float), "cu_orco");
  for (dl = disp.first; dl; dl = dl->next) {
    if (dl->type == DL_INDEX3) {
      for (u = 0; u < dl->nr; u++, fp += 3) {
        if (cu->flag & CU_UV_ORCO) {
          fp[0] = 2.0f * u / (dl->nr - 1) - 1.0f;
          fp[1] = 0.0;
          fp[2] = 0.0;
        }
        else {
          copy_v3_v3(fp, &dl->verts[u * 3]);

          fp[0] = (fp[0] - cu->loc[0]) / cu->size[0];
          fp[1] = (fp[1] - cu->loc[1]) / cu->size[1];
          fp[2] = (fp[2] - cu->loc[2]) / cu->size[2];
        }
      }
    }
    else if (dl->type == DL_SURF) {
      int sizeu = dl->nr, sizev = dl->parts;

      /* exception as handled in convertblender.c too */
      if (dl->flag & DL_CYCL_U) {
        sizeu++;
        if (dl->flag & DL_CYCL_V) {
          sizev++;
        }
      }
      else if (dl->flag & DL_CYCL_V) {
        sizev++;
      }

      for (u = 0; u < sizev; u++) {
        for (v = 0; v < sizeu; v++, fp += 3) {
          if (cu->flag & CU_UV_ORCO) {
            fp[0] = 2.0f * u / (sizev - 1) - 1.0f;
            fp[1] = 2.0f * v / (sizeu - 1) - 1.0f;
            fp[2] = 0.0;
          }
          else {
            const float *vert;
            int realv = v % dl->nr;
            int realu = u % dl->parts;

            vert = dl->verts + 3 * (dl->nr * realu + realv);
            copy_v3_v3(fp, vert);

            fp[0] = (fp[0] - cu->loc[0]) / cu->size[0];
            fp[1] = (fp[1] - cu->loc[1]) / cu->size[1];
            fp[2] = (fp[2] - cu->loc[2]) / cu->size[2];
          }
        }
      }
    }
  }

  BKE_displist_free(&disp);

  return coord_array;
}

/* ***************** BEVEL ****************** */

void BKE_curve_bevel_make(Depsgraph *depsgraph,
                          Scene *scene,
                          Object *ob,
                          ListBase *disp,
                          const bool for_render,
                          const bool use_render_resolution,
                          LinkNode *ob_cyclic_list)
{
  DispList *dl, *dlnew;
  Curve *bevcu, *cu;
  float *fp, facx, facy, angle, dangle;
  int nr, a;

  cu = ob->data;
  BLI_listbase_clear(disp);

  /* if a font object is being edited, then do nothing */
  // XXX  if ( ob == obedit && ob->type == OB_FONT ) return;

  if (cu->bevobj) {
    if (cu->bevobj->type != OB_CURVE) {
      return;
    }

    bevcu = cu->bevobj->data;
    if (bevcu->ext1 == 0.0f && bevcu->ext2 == 0.0f) {
      ListBase bevdisp = {NULL, NULL};
      facx = cu->bevobj->scale[0];
      facy = cu->bevobj->scale[1];

      if (for_render) {
        if (BLI_linklist_index(ob_cyclic_list, cu->bevobj) == -1) {
          BKE_displist_make_curveTypes_forRender(depsgraph,
                                                 scene,
                                                 cu->bevobj,
                                                 &bevdisp,
                                                 NULL,
                                                 false,
                                                 use_render_resolution,
                                                 &(LinkNode){
                                                     .link = ob,
                                                     .next = ob_cyclic_list,
                                                 });
          dl = bevdisp.first;
        }
        else {
          dl = NULL;
        }
      }
      else if (cu->bevobj->runtime.curve_cache) {
        dl = cu->bevobj->runtime.curve_cache->disp.first;
      }
      else {
        BLI_assert(cu->bevobj->runtime.curve_cache != NULL);
        dl = NULL;
      }

      while (dl) {
        if (ELEM(dl->type, DL_POLY, DL_SEGM)) {
          dlnew = MEM_mallocN(sizeof(DispList), "makebevelcurve1");
          *dlnew = *dl;
          dlnew->verts = MEM_malloc_arrayN(
              dl->parts * dl->nr, 3 * sizeof(float), "makebevelcurve1");
          memcpy(dlnew->verts, dl->verts, 3 * sizeof(float) * dl->parts * dl->nr);

          if (dlnew->type == DL_SEGM) {
            dlnew->flag |= (DL_FRONT_CURVE | DL_BACK_CURVE);
          }

          BLI_addtail(disp, dlnew);
          fp = dlnew->verts;
          nr = dlnew->parts * dlnew->nr;
          while (nr--) {
            fp[2] = fp[1] * facy;
            fp[1] = -fp[0] * facx;
            fp[0] = 0.0;
            fp += 3;
          }
        }
        dl = dl->next;
      }

      BKE_displist_free(&bevdisp);
    }
  }
  else if (cu->ext1 == 0.0f && cu->ext2 == 0.0f) {
    /* pass */
  }
  else if (cu->ext2 == 0.0f) {
    dl = MEM_callocN(sizeof(DispList), "makebevelcurve2");
    dl->verts = MEM_malloc_arrayN(2, sizeof(float[3]), "makebevelcurve2");
    BLI_addtail(disp, dl);
    dl->type = DL_SEGM;
    dl->parts = 1;
    dl->flag = DL_FRONT_CURVE | DL_BACK_CURVE;
    dl->nr = 2;

    fp = dl->verts;
    fp[0] = fp[1] = 0.0;
    fp[2] = -cu->ext1;
    fp[3] = fp[4] = 0.0;
    fp[5] = cu->ext1;
  }
  else if ((cu->flag & (CU_FRONT | CU_BACK)) == 0 &&
           cu->ext1 == 0.0f) { /* we make a full round bevel in that case */
    nr = 4 + 2 * cu->bevresol;

    dl = MEM_callocN(sizeof(DispList), "makebevelcurve p1");
    dl->verts = MEM_malloc_arrayN(nr, sizeof(float[3]), "makebevelcurve p1");
    BLI_addtail(disp, dl);
    dl->type = DL_POLY;
    dl->parts = 1;
    dl->flag = DL_BACK_CURVE;
    dl->nr = nr;

    /* a circle */
    fp = dl->verts;
    dangle = (2.0f * (float)M_PI / (nr));
    angle = -(nr - 1) * dangle;

    for (a = 0; a < nr; a++) {
      fp[0] = 0.0;
      fp[1] = (cosf(angle) * (cu->ext2));
      fp[2] = (sinf(angle) * (cu->ext2)) - cu->ext1;
      angle += dangle;
      fp += 3;
    }
  }
  else {
    short dnr;

    /* bevel now in three parts, for proper vertex normals */
    /* part 1, back */

    if ((cu->flag & CU_BACK) || !(cu->flag & CU_FRONT)) {
      dnr = nr = 2 + cu->bevresol;
      if ((cu->flag & (CU_FRONT | CU_BACK)) == 0) {
        nr = 3 + 2 * cu->bevresol;
      }
      dl = MEM_callocN(sizeof(DispList), "makebevelcurve p1");
      dl->verts = MEM_malloc_arrayN(nr, sizeof(float[3]), "makebevelcurve p1");
      BLI_addtail(disp, dl);
      dl->type = DL_SEGM;
      dl->parts = 1;
      dl->flag = DL_BACK_CURVE;
      dl->nr = nr;

      /* half a circle */
      fp = dl->verts;
      dangle = ((float)M_PI_2 / (dnr - 1));
      angle = -(nr - 1) * dangle;

      for (a = 0; a < nr; a++) {
        fp[0] = 0.0;
        fp[1] = (float)(cosf(angle) * (cu->ext2));
        fp[2] = (float)(sinf(angle) * (cu->ext2)) - cu->ext1;
        angle += dangle;
        fp += 3;
      }
    }

    /* part 2, sidefaces */
    if (cu->ext1 != 0.0f) {
      nr = 2;

      dl = MEM_callocN(sizeof(DispList), "makebevelcurve p2");
      dl->verts = MEM_malloc_arrayN(nr, sizeof(float[3]), "makebevelcurve p2");
      BLI_addtail(disp, dl);
      dl->type = DL_SEGM;
      dl->parts = 1;
      dl->nr = nr;

      fp = dl->verts;
      fp[1] = cu->ext2;
      fp[2] = -cu->ext1;
      fp[4] = cu->ext2;
      fp[5] = cu->ext1;

      if ((cu->flag & (CU_FRONT | CU_BACK)) == 0) {
        dl = MEM_dupallocN(dl);
        dl->verts = MEM_dupallocN(dl->verts);
        BLI_addtail(disp, dl);

        fp = dl->verts;
        fp[1] = -fp[1];
        fp[2] = -fp[2];
        fp[4] = -fp[4];
        fp[5] = -fp[5];
      }
    }

    /* part 3, front */
    if ((cu->flag & CU_FRONT) || !(cu->flag & CU_BACK)) {
      dnr = nr = 2 + cu->bevresol;
      if ((cu->flag & (CU_FRONT | CU_BACK)) == 0) {
        nr = 3 + 2 * cu->bevresol;
      }
      dl = MEM_callocN(sizeof(DispList), "makebevelcurve p3");
      dl->verts = MEM_malloc_arrayN(nr, sizeof(float[3]), "makebevelcurve p3");
      BLI_addtail(disp, dl);
      dl->type = DL_SEGM;
      dl->flag = DL_FRONT_CURVE;
      dl->parts = 1;
      dl->nr = nr;

      /* half a circle */
      fp = dl->verts;
      angle = 0.0;
      dangle = ((float)M_PI_2 / (dnr - 1));

      for (a = 0; a < nr; a++) {
        fp[0] = 0.0;
        fp[1] = (float)(cosf(angle) * (cu->ext2));
        fp[2] = (float)(sinf(angle) * (cu->ext2)) + cu->ext1;
        angle += dangle;
        fp += 3;
      }
    }
  }
}

static int cu_isectLL(const float v1[3],
                      const float v2[3],
                      const float v3[3],
                      const float v4[3],
                      short cox,
                      short coy,
                      float *lambda,
                      float *mu,
                      float vec[3])
{
  /* return:
   * -1: collinear
   *  0: no intersection of segments
   *  1: exact intersection of segments
   *  2: cross-intersection of segments
   */
  float deler;

  deler = (v1[cox] - v2[cox]) * (v3[coy] - v4[coy]) - (v3[cox] - v4[cox]) * (v1[coy] - v2[coy]);
  if (deler == 0.0f) {
    return -1;
  }

  *lambda = (v1[coy] - v3[coy]) * (v3[cox] - v4[cox]) - (v1[cox] - v3[cox]) * (v3[coy] - v4[coy]);
  *lambda = -(*lambda / deler);

  deler = v3[coy] - v4[coy];
  if (deler == 0) {
    deler = v3[cox] - v4[cox];
    *mu = -(*lambda * (v2[cox] - v1[cox]) + v1[cox] - v3[cox]) / deler;
  }
  else {
    *mu = -(*lambda * (v2[coy] - v1[coy]) + v1[coy] - v3[coy]) / deler;
  }
  vec[cox] = *lambda * (v2[cox] - v1[cox]) + v1[cox];
  vec[coy] = *lambda * (v2[coy] - v1[coy]) + v1[coy];

  if (*lambda >= 0.0f && *lambda <= 1.0f && *mu >= 0.0f && *mu <= 1.0f) {
    if (*lambda == 0.0f || *lambda == 1.0f || *mu == 0.0f || *mu == 1.0f) {
      return 1;
    }
    return 2;
  }
  return 0;
}

static bool bevelinside(BevList *bl1, BevList *bl2)
{
  /* is bl2 INSIDE bl1 ? with left-right method and "lambda's" */
  /* returns '1' if correct hole  */
  BevPoint *bevp, *prevbevp;
  float min, max, vec[3], hvec1[3], hvec2[3], lab, mu;
  int nr, links = 0, rechts = 0, mode;

  /* take first vertex of possible hole */

  bevp = bl2->bevpoints;
  hvec1[0] = bevp->vec[0];
  hvec1[1] = bevp->vec[1];
  hvec1[2] = 0.0;
  copy_v3_v3(hvec2, hvec1);
  hvec2[0] += 1000;

  /* test it with all edges of potential surrounding poly */
  /* count number of transitions left-right  */

  bevp = bl1->bevpoints;
  nr = bl1->nr;
  prevbevp = bevp + (nr - 1);

  while (nr--) {
    min = prevbevp->vec[1];
    max = bevp->vec[1];
    if (max < min) {
      min = max;
      max = prevbevp->vec[1];
    }
    if (min != max) {
      if (min <= hvec1[1] && max >= hvec1[1]) {
        /* there's a transition, calc intersection point */
        mode = cu_isectLL(prevbevp->vec, bevp->vec, hvec1, hvec2, 0, 1, &lab, &mu, vec);
        /* if lab==0.0 or lab==1.0 then the edge intersects exactly a transition
         * only allow for one situation: we choose lab= 1.0
         */
        if (mode >= 0 && lab != 0.0f) {
          if (vec[0] < hvec1[0]) {
            links++;
          }
          else {
            rechts++;
          }
        }
      }
    }
    prevbevp = bevp;
    bevp++;
  }

  return (links & 1) && (rechts & 1);
}

struct BevelSort {
  BevList *bl;
  float left;
  int dir;
};

static int vergxcobev(const void *a1, const void *a2)
{
  const struct BevelSort *x1 = a1, *x2 = a2;

  if (x1->left > x2->left) {
    return 1;
  }
  else if (x1->left < x2->left) {
    return -1;
  }
  return 0;
}

/* this function cannot be replaced with atan2, but why? */

static void calc_bevel_sin_cos(
    float x1, float y1, float x2, float y2, float *r_sina, float *r_cosa)
{
  float t01, t02, x3, y3;

  t01 = sqrtf(x1 * x1 + y1 * y1);
  t02 = sqrtf(x2 * x2 + y2 * y2);
  if (t01 == 0.0f) {
    t01 = 1.0f;
  }
  if (t02 == 0.0f) {
    t02 = 1.0f;
  }

  x1 /= t01;
  y1 /= t01;
  x2 /= t02;
  y2 /= t02;

  t02 = x1 * x2 + y1 * y2;
  if (fabsf(t02) >= 1.0f) {
    t02 = M_PI_2;
  }
  else {
    t02 = (saacos(t02)) / 2.0f;
  }

  t02 = sinf(t02);
  if (t02 == 0.0f) {
    t02 = 1.0f;
  }

  x3 = x1 - x2;
  y3 = y1 - y2;
  if (x3 == 0 && y3 == 0) {
    x3 = y1;
    y3 = -x1;
  }
  else {
    t01 = sqrtf(x3 * x3 + y3 * y3);
    x3 /= t01;
    y3 /= t01;
  }

  *r_sina = -y3 / t02;
  *r_cosa = x3 / t02;
}

static void tilt_bezpart(BezTriple *prevbezt,
                         BezTriple *bezt,
                         Nurb *nu,
                         float *tilt_array,
                         float *radius_array,
                         float *weight_array,
                         int resolu,
                         int stride)
{
  BezTriple *pprev, *next, *last;
  float fac, dfac, t[4];
  int a;

  if (tilt_array == NULL && radius_array == NULL) {
    return;
  }

  last = nu->bezt + (nu->pntsu - 1);

  /* returns a point */
  if (prevbezt == nu->bezt) {
    if (nu->flagu & CU_NURB_CYCLIC) {
      pprev = last;
    }
    else {
      pprev = prevbezt;
    }
  }
  else {
    pprev = prevbezt - 1;
  }

  /* next point */
  if (bezt == last) {
    if (nu->flagu & CU_NURB_CYCLIC) {
      next = nu->bezt;
    }
    else {
      next = bezt;
    }
  }
  else {
    next = bezt + 1;
  }

  fac = 0.0;
  dfac = 1.0f / (float)resolu;

  for (a = 0; a < resolu; a++, fac += dfac) {
    if (tilt_array) {
      if (nu->tilt_interp ==
          KEY_CU_EASE) { /* May as well support for tilt also 2.47 ease interp */
        *tilt_array = prevbezt->tilt +
                      (bezt->tilt - prevbezt->tilt) * (3.0f * fac * fac - 2.0f * fac * fac * fac);
      }
      else {
        key_curve_position_weights(fac, t, nu->tilt_interp);
        *tilt_array = t[0] * pprev->tilt + t[1] * prevbezt->tilt + t[2] * bezt->tilt +
                      t[3] * next->tilt;
      }

      tilt_array = POINTER_OFFSET(tilt_array, stride);
    }

    if (radius_array) {
      if (nu->radius_interp == KEY_CU_EASE) {
        /* Support 2.47 ease interp
         * Note! - this only takes the 2 points into account,
         * giving much more localized results to changes in radius, sometimes you want that */
        *radius_array = prevbezt->radius + (bezt->radius - prevbezt->radius) *
                                               (3.0f * fac * fac - 2.0f * fac * fac * fac);
      }
      else {

        /* reuse interpolation from tilt if we can */
        if (tilt_array == NULL || nu->tilt_interp != nu->radius_interp) {
          key_curve_position_weights(fac, t, nu->radius_interp);
        }
        *radius_array = t[0] * pprev->radius + t[1] * prevbezt->radius + t[2] * bezt->radius +
                        t[3] * next->radius;
      }

      radius_array = POINTER_OFFSET(radius_array, stride);
    }

    if (weight_array) {
      /* basic interpolation for now, could copy tilt interp too  */
      *weight_array = prevbezt->weight + (bezt->weight - prevbezt->weight) *
                                             (3.0f * fac * fac - 2.0f * fac * fac * fac);

      weight_array = POINTER_OFFSET(weight_array, stride);
    }
  }
}

/* make_bevel_list_3D_* funcs, at a minimum these must
 * fill in the bezp->quat and bezp->dir values */

/* utility for make_bevel_list_3D_* funcs */
static void bevel_list_calc_bisect(BevList *bl)
{
  BevPoint *bevp2, *bevp1, *bevp0;
  int nr;
  bool is_cyclic = bl->poly != -1;

  if (is_cyclic) {
    bevp2 = bl->bevpoints;
    bevp1 = bevp2 + (bl->nr - 1);
    bevp0 = bevp1 - 1;
    nr = bl->nr;
  }
  else {
    /* If spline is not cyclic, direction of first and
     * last bevel points matches direction of CV handle.
     *
     * This is getting calculated earlier when we know
     * CV's handles and here we might simply skip evaluation
     * of direction for this guys.
     */

    bevp0 = bl->bevpoints;
    bevp1 = bevp0 + 1;
    bevp2 = bevp1 + 1;

    nr = bl->nr - 2;
  }

  while (nr--) {
    /* totally simple */
    bisect_v3_v3v3v3(bevp1->dir, bevp0->vec, bevp1->vec, bevp2->vec);

    bevp0 = bevp1;
    bevp1 = bevp2;
    bevp2++;
  }
}
static void bevel_list_flip_tangents(BevList *bl)
{
  BevPoint *bevp2, *bevp1, *bevp0;
  int nr;

  bevp2 = bl->bevpoints;
  bevp1 = bevp2 + (bl->nr - 1);
  bevp0 = bevp1 - 1;

  nr = bl->nr;
  while (nr--) {
    if (angle_normalized_v3v3(bevp0->tan, bevp1->tan) > DEG2RADF(90.0f)) {
      negate_v3(bevp1->tan);
    }

    bevp0 = bevp1;
    bevp1 = bevp2;
    bevp2++;
  }
}
/* apply user tilt */
static void bevel_list_apply_tilt(BevList *bl)
{
  BevPoint *bevp2, *bevp1;
  int nr;
  float q[4];

  bevp2 = bl->bevpoints;
  bevp1 = bevp2 + (bl->nr - 1);

  nr = bl->nr;
  while (nr--) {
    axis_angle_to_quat(q, bevp1->dir, bevp1->tilt);
    mul_qt_qtqt(bevp1->quat, q, bevp1->quat);
    normalize_qt(bevp1->quat);

    bevp1 = bevp2;
    bevp2++;
  }
}
/* smooth quats, this function should be optimized, it can get slow with many iterations. */
static void bevel_list_smooth(BevList *bl, int smooth_iter)
{
  BevPoint *bevp2, *bevp1, *bevp0;
  int nr;

  float q[4];
  float bevp0_quat[4];
  int a;

  for (a = 0; a < smooth_iter; a++) {
    bevp2 = bl->bevpoints;
    bevp1 = bevp2 + (bl->nr - 1);
    bevp0 = bevp1 - 1;

    nr = bl->nr;

    if (bl->poly == -1) { /* check its not cyclic */
      /* skip the first point */
      /* bevp0 = bevp1; */
      bevp1 = bevp2;
      bevp2++;
      nr--;

      bevp0 = bevp1;
      bevp1 = bevp2;
      bevp2++;
      nr--;
    }

    copy_qt_qt(bevp0_quat, bevp0->quat);

    while (nr--) {
      /* interpolate quats */
      float zaxis[3] = {0, 0, 1}, cross[3], q2[4];
      interp_qt_qtqt(q, bevp0_quat, bevp2->quat, 0.5);
      normalize_qt(q);

      mul_qt_v3(q, zaxis);
      cross_v3_v3v3(cross, zaxis, bevp1->dir);
      axis_angle_to_quat(q2, cross, angle_normalized_v3v3(zaxis, bevp1->dir));
      normalize_qt(q2);

      copy_qt_qt(bevp0_quat, bevp1->quat);
      mul_qt_qtqt(q, q2, q);
      interp_qt_qtqt(bevp1->quat, bevp1->quat, q, 0.5);
      normalize_qt(bevp1->quat);

      /* bevp0 = bevp1; */ /* UNUSED */
      bevp1 = bevp2;
      bevp2++;
    }
  }
}

static void make_bevel_list_3D_zup(BevList *bl)
{
  BevPoint *bevp = bl->bevpoints;
  int nr = bl->nr;

  bevel_list_calc_bisect(bl);

  while (nr--) {
    vec_to_quat(bevp->quat, bevp->dir, 5, 1);
    bevp++;
  }
}

static void minimum_twist_between_two_points(BevPoint *current_point, BevPoint *previous_point)
{
  float angle = angle_normalized_v3v3(previous_point->dir, current_point->dir);
  float q[4];

  if (angle > 0.0f) { /* otherwise we can keep as is */
    float cross_tmp[3];
    cross_v3_v3v3(cross_tmp, previous_point->dir, current_point->dir);
    axis_angle_to_quat(q, cross_tmp, angle);
    mul_qt_qtqt(current_point->quat, q, previous_point->quat);
  }
  else {
    copy_qt_qt(current_point->quat, previous_point->quat);
  }
}

static void make_bevel_list_3D_minimum_twist(BevList *bl)
{
  BevPoint *bevp2, *bevp1, *bevp0; /* standard for all make_bevel_list_3D_* funcs */
  int nr;
  float q[4];

  bevel_list_calc_bisect(bl);

  bevp2 = bl->bevpoints;
  bevp1 = bevp2 + (bl->nr - 1);
  bevp0 = bevp1 - 1;

  nr = bl->nr;
  while (nr--) {

    if (nr + 4 > bl->nr) { /* first time and second time, otherwise first point adjusts last */
      vec_to_quat(bevp1->quat, bevp1->dir, 5, 1);
    }
    else {
      minimum_twist_between_two_points(bevp1, bevp0);
    }

    bevp0 = bevp1;
    bevp1 = bevp2;
    bevp2++;
  }

  if (bl->poly != -1) { /* check for cyclic */

    /* Need to correct for the start/end points not matching
     * do this by calculating the tilt angle difference, then apply
     * the rotation gradually over the entire curve
     *
     * note that the split is between last and second last, rather than first/last as youd expect.
     *
     * real order is like this
     * 0,1,2,3,4 --> 1,2,3,4,0
     *
     * this is why we compare last with second last
     * */
    float vec_1[3] = {0, 1, 0}, vec_2[3] = {0, 1, 0}, angle, ang_fac, cross_tmp[3];

    BevPoint *bevp_first;
    BevPoint *bevp_last;

    bevp_first = bl->bevpoints;
    bevp_first += bl->nr - 1;
    bevp_last = bevp_first;
    bevp_last--;

    /* quats and vec's are normalized, should not need to re-normalize */
    mul_qt_v3(bevp_first->quat, vec_1);
    mul_qt_v3(bevp_last->quat, vec_2);
    normalize_v3(vec_1);
    normalize_v3(vec_2);

    /* align the vector, can avoid this and it looks 98% OK but
     * better to align the angle quat roll's before comparing */
    {
      cross_v3_v3v3(cross_tmp, bevp_last->dir, bevp_first->dir);
      angle = angle_normalized_v3v3(bevp_first->dir, bevp_last->dir);
      axis_angle_to_quat(q, cross_tmp, angle);
      mul_qt_v3(q, vec_2);
    }

    angle = angle_normalized_v3v3(vec_1, vec_2);

    /* flip rotation if needs be */
    cross_v3_v3v3(cross_tmp, vec_1, vec_2);
    normalize_v3(cross_tmp);
    if (angle_normalized_v3v3(bevp_first->dir, cross_tmp) < DEG2RADF(90.0f)) {
      angle = -angle;
    }

    bevp2 = bl->bevpoints;
    bevp1 = bevp2 + (bl->nr - 1);
    bevp0 = bevp1 - 1;

    nr = bl->nr;
    while (nr--) {
      ang_fac = angle * (1.0f - ((float)nr / bl->nr)); /* also works */

      axis_angle_to_quat(q, bevp1->dir, ang_fac);
      mul_qt_qtqt(bevp1->quat, q, bevp1->quat);

      bevp0 = bevp1;
      bevp1 = bevp2;
      bevp2++;
    }
  }
  else {
    /* Need to correct quat for the first/last point,
     * this is so because previously it was only calculated
     * using it's own direction, which might not correspond
     * the twist of neighbor point.
     */
    bevp1 = bl->bevpoints;
    bevp0 = bevp1 + 1;
    minimum_twist_between_two_points(bevp1, bevp0);

    bevp2 = bl->bevpoints;
    bevp1 = bevp2 + (bl->nr - 1);
    bevp0 = bevp1 - 1;
    minimum_twist_between_two_points(bevp1, bevp0);
  }
}

static void make_bevel_list_3D_tangent(BevList *bl)
{
  BevPoint *bevp2, *bevp1, *bevp0; /* standard for all make_bevel_list_3D_* funcs */
  int nr;

  float bevp0_tan[3];

  bevel_list_calc_bisect(bl);
  bevel_list_flip_tangents(bl);

  /* correct the tangents */
  bevp2 = bl->bevpoints;
  bevp1 = bevp2 + (bl->nr - 1);
  bevp0 = bevp1 - 1;

  nr = bl->nr;
  while (nr--) {
    float cross_tmp[3];
    cross_v3_v3v3(cross_tmp, bevp1->tan, bevp1->dir);
    cross_v3_v3v3(bevp1->tan, cross_tmp, bevp1->dir);
    normalize_v3(bevp1->tan);

    bevp0 = bevp1;
    bevp1 = bevp2;
    bevp2++;
  }

  /* now for the real twist calc */
  bevp2 = bl->bevpoints;
  bevp1 = bevp2 + (bl->nr - 1);
  bevp0 = bevp1 - 1;

  copy_v3_v3(bevp0_tan, bevp0->tan);

  nr = bl->nr;
  while (nr--) {
    /* make perpendicular, modify tan in place, is ok */
    float cross_tmp[3];
    float zero[3] = {0, 0, 0};

    cross_v3_v3v3(cross_tmp, bevp1->tan, bevp1->dir);
    normalize_v3(cross_tmp);
    tri_to_quat(bevp1->quat, zero, cross_tmp, bevp1->tan); /* XXX - could be faster */

    /* bevp0 = bevp1; */ /* UNUSED */
    bevp1 = bevp2;
    bevp2++;
  }
}

static void make_bevel_list_3D(BevList *bl, int smooth_iter, int twist_mode)
{
  switch (twist_mode) {
    case CU_TWIST_TANGENT:
      make_bevel_list_3D_tangent(bl);
      break;
    case CU_TWIST_MINIMUM:
      make_bevel_list_3D_minimum_twist(bl);
      break;
    default: /* CU_TWIST_Z_UP default, pre 2.49c */
      make_bevel_list_3D_zup(bl);
      break;
  }

  if (smooth_iter) {
    bevel_list_smooth(bl, smooth_iter);
  }

  bevel_list_apply_tilt(bl);
}

/* only for 2 points */
static void make_bevel_list_segment_3D(BevList *bl)
{
  float q[4];

  BevPoint *bevp2 = bl->bevpoints;
  BevPoint *bevp1 = bevp2 + 1;

  /* simple quat/dir */
  sub_v3_v3v3(bevp1->dir, bevp1->vec, bevp2->vec);
  normalize_v3(bevp1->dir);

  vec_to_quat(bevp1->quat, bevp1->dir, 5, 1);

  axis_angle_to_quat(q, bevp1->dir, bevp1->tilt);
  mul_qt_qtqt(bevp1->quat, q, bevp1->quat);
  normalize_qt(bevp1->quat);
  copy_v3_v3(bevp2->dir, bevp1->dir);
  copy_qt_qt(bevp2->quat, bevp1->quat);
}

/* only for 2 points */
static void make_bevel_list_segment_2D(BevList *bl)
{
  BevPoint *bevp2 = bl->bevpoints;
  BevPoint *bevp1 = bevp2 + 1;

  const float x1 = bevp1->vec[0] - bevp2->vec[0];
  const float y1 = bevp1->vec[1] - bevp2->vec[1];

  calc_bevel_sin_cos(x1, y1, -x1, -y1, &(bevp1->sina), &(bevp1->cosa));
  bevp2->sina = bevp1->sina;
  bevp2->cosa = bevp1->cosa;

  /* fill in dir & quat */
  make_bevel_list_segment_3D(bl);
}

static void make_bevel_list_2D(BevList *bl)
{
  /* note: bevp->dir and bevp->quat are not needed for beveling but are
   * used when making a path from a 2D curve, therefore they need to be set - Campbell */

  BevPoint *bevp0, *bevp1, *bevp2;
  int nr;

  if (bl->poly != -1) {
    bevp2 = bl->bevpoints;
    bevp1 = bevp2 + (bl->nr - 1);
    bevp0 = bevp1 - 1;
    nr = bl->nr;
  }
  else {
    bevp0 = bl->bevpoints;
    bevp1 = bevp0 + 1;
    bevp2 = bevp1 + 1;

    nr = bl->nr - 2;
  }

  while (nr--) {
    const float x1 = bevp1->vec[0] - bevp0->vec[0];
    const float x2 = bevp1->vec[0] - bevp2->vec[0];
    const float y1 = bevp1->vec[1] - bevp0->vec[1];
    const float y2 = bevp1->vec[1] - bevp2->vec[1];

    calc_bevel_sin_cos(x1, y1, x2, y2, &(bevp1->sina), &(bevp1->cosa));

    /* from: make_bevel_list_3D_zup, could call but avoid a second loop.
     * no need for tricky tilt calculation as with 3D curves */
    bisect_v3_v3v3v3(bevp1->dir, bevp0->vec, bevp1->vec, bevp2->vec);
    vec_to_quat(bevp1->quat, bevp1->dir, 5, 1);
    /* done with inline make_bevel_list_3D_zup */

    bevp0 = bevp1;
    bevp1 = bevp2;
    bevp2++;
  }

  /* correct non-cyclic cases */
  if (bl->poly == -1) {
    BevPoint *bevp;
    float angle;

    /* first */
    bevp = bl->bevpoints;
    angle = atan2f(bevp->dir[0], bevp->dir[1]) - (float)M_PI_2;
    bevp->sina = sinf(angle);
    bevp->cosa = cosf(angle);
    vec_to_quat(bevp->quat, bevp->dir, 5, 1);

    /* last */
    bevp = bl->bevpoints;
    bevp += (bl->nr - 1);
    angle = atan2f(bevp->dir[0], bevp->dir[1]) - (float)M_PI_2;
    bevp->sina = sinf(angle);
    bevp->cosa = cosf(angle);
    vec_to_quat(bevp->quat, bevp->dir, 5, 1);
  }
}

static void bevlist_firstlast_direction_calc_from_bpoint(Nurb *nu, BevList *bl)
{
  if (nu->pntsu > 1) {
    BPoint *first_bp = nu->bp, *last_bp = nu->bp + (nu->pntsu - 1);
    BevPoint *first_bevp, *last_bevp;

    first_bevp = bl->bevpoints;
    last_bevp = first_bevp + (bl->nr - 1);

    sub_v3_v3v3(first_bevp->dir, (first_bp + 1)->vec, first_bp->vec);
    normalize_v3(first_bevp->dir);

    sub_v3_v3v3(last_bevp->dir, last_bp->vec, (last_bp - 1)->vec);
    normalize_v3(last_bevp->dir);
  }
}

void BKE_curve_bevelList_free(ListBase *bev)
{
  BevList *bl, *blnext;
  for (bl = bev->first; bl != NULL; bl = blnext) {
    blnext = bl->next;
    if (bl->seglen != NULL) {
      MEM_freeN(bl->seglen);
    }
    if (bl->segbevcount != NULL) {
      MEM_freeN(bl->segbevcount);
    }
    if (bl->bevpoints != NULL) {
      MEM_freeN(bl->bevpoints);
    }
    MEM_freeN(bl);
  }

  BLI_listbase_clear(bev);
}

void BKE_curve_bevelList_make(Object *ob, ListBase *nurbs, bool for_render)
{
  /*
   * - convert all curves to polys, with indication of resol and flags for double-vertices
   * - possibly; do a smart vertice removal (in case Nurb)
   * - separate in individual blocks with BoundBox
   * - AutoHole detection
   */

  /* this function needs an object, because of tflag and upflag */
  Curve *cu = ob->data;
  Nurb *nu;
  BezTriple *bezt, *prevbezt;
  BPoint *bp;
  BevList *bl, *blnew, *blnext;
  BevPoint *bevp2, *bevp1 = NULL, *bevp0;
  const float treshold = 0.00001f;
  float min, inp;
  float *seglen = NULL;
  struct BevelSort *sortdata, *sd, *sd1;
  int a, b, nr, poly, resolu = 0, len = 0, segcount;
  int *segbevcount;
  bool do_tilt, do_radius, do_weight;
  bool is_editmode = false;
  ListBase *bev;

  /* segbevcount alsp requires seglen. */
  const bool need_seglen = ELEM(
                               cu->bevfac1_mapping, CU_BEVFAC_MAP_SEGMENT, CU_BEVFAC_MAP_SPLINE) ||
                           ELEM(cu->bevfac2_mapping, CU_BEVFAC_MAP_SEGMENT, CU_BEVFAC_MAP_SPLINE);

  bev = &ob->runtime.curve_cache->bev;

#if 0
  /* do we need to calculate the radius for each point? */
  do_radius = (cu->bevobj || cu->taperobj || (cu->flag & CU_FRONT) || (cu->flag & CU_BACK)) ? 0 :
                                                                                              1;
#endif

  /* STEP 1: MAKE POLYS  */

  BKE_curve_bevelList_free(&ob->runtime.curve_cache->bev);
  nu = nurbs->first;
  if (cu->editnurb && ob->type != OB_FONT) {
    is_editmode = 1;
  }

  for (; nu; nu = nu->next) {
    if (nu->hide && is_editmode) {
      continue;
    }

    /* check if we will calculate tilt data */
    do_tilt = CU_DO_TILT(cu, nu);
    do_radius = CU_DO_RADIUS(
        cu, nu); /* normal display uses the radius, better just to calculate them */
    do_weight = true;

    /* check we are a single point? also check we are not a surface and that the orderu is sane,
     * enforced in the UI but can go wrong possibly */
    if (!BKE_nurb_check_valid_u(nu)) {
      bl = MEM_callocN(sizeof(BevList), "makeBevelList1");
      bl->bevpoints = MEM_calloc_arrayN(1, sizeof(BevPoint), "makeBevelPoints1");
      BLI_addtail(bev, bl);
      bl->nr = 0;
      bl->charidx = nu->charidx;
    }
    else {
      BevPoint *bevp;

      if (for_render && cu->resolu_ren != 0) {
        resolu = cu->resolu_ren;
      }
      else {
        resolu = nu->resolu;
      }

      segcount = SEGMENTSU(nu);

      if (nu->type == CU_POLY) {
        len = nu->pntsu;
        bl = MEM_callocN(sizeof(BevList), "makeBevelList2");
        bl->bevpoints = MEM_calloc_arrayN(len, sizeof(BevPoint), "makeBevelPoints2");
        if (need_seglen && (nu->flagu & CU_NURB_CYCLIC) == 0) {
          bl->seglen = MEM_malloc_arrayN(segcount, sizeof(float), "makeBevelList2_seglen");
          bl->segbevcount = MEM_malloc_arrayN(segcount, sizeof(int), "makeBevelList2_segbevcount");
        }
        BLI_addtail(bev, bl);

        bl->poly = (nu->flagu & CU_NURB_CYCLIC) ? 0 : -1;
        bl->nr = len;
        bl->dupe_nr = 0;
        bl->charidx = nu->charidx;
        bevp = bl->bevpoints;
        bevp->offset = 0;
        bp = nu->bp;
        seglen = bl->seglen;
        segbevcount = bl->segbevcount;

        while (len--) {
          copy_v3_v3(bevp->vec, bp->vec);
          bevp->tilt = bp->tilt;
          bevp->radius = bp->radius;
          bevp->weight = bp->weight;
          bevp->split_tag = true;
          bp++;
          if (seglen != NULL && len != 0) {
            *seglen = len_v3v3(bevp->vec, bp->vec);
            bevp++;
            bevp->offset = *seglen;
            if (*seglen > treshold) {
              *segbevcount = 1;
            }
            else {
              *segbevcount = 0;
            }
            seglen++;
            segbevcount++;
          }
          else {
            bevp++;
          }
        }

        if ((nu->flagu & CU_NURB_CYCLIC) == 0) {
          bevlist_firstlast_direction_calc_from_bpoint(nu, bl);
        }
      }
      else if (nu->type == CU_BEZIER) {
        /* in case last point is not cyclic */
        len = segcount * resolu + 1;

        bl = MEM_callocN(sizeof(BevList), "makeBevelBPoints");
        bl->bevpoints = MEM_calloc_arrayN(len, sizeof(BevPoint), "makeBevelBPointsPoints");
        if (need_seglen && (nu->flagu & CU_NURB_CYCLIC) == 0) {
          bl->seglen = MEM_malloc_arrayN(segcount, sizeof(float), "makeBevelBPoints_seglen");
          bl->segbevcount = MEM_malloc_arrayN(
              segcount, sizeof(int), "makeBevelBPoints_segbevcount");
        }
        BLI_addtail(bev, bl);

        bl->poly = (nu->flagu & CU_NURB_CYCLIC) ? 0 : -1;
        bl->charidx = nu->charidx;

        bevp = bl->bevpoints;
        seglen = bl->seglen;
        segbevcount = bl->segbevcount;

        bevp->offset = 0;
        if (seglen != NULL) {
          *seglen = 0;
          *segbevcount = 0;
        }

        a = nu->pntsu - 1;
        bezt = nu->bezt;
        if (nu->flagu & CU_NURB_CYCLIC) {
          a++;
          prevbezt = nu->bezt + (nu->pntsu - 1);
        }
        else {
          prevbezt = bezt;
          bezt++;
        }

        sub_v3_v3v3(bevp->dir, prevbezt->vec[2], prevbezt->vec[1]);
        normalize_v3(bevp->dir);

        BLI_assert(segcount >= a);

        while (a--) {
          if (prevbezt->h2 == HD_VECT && bezt->h1 == HD_VECT) {

            copy_v3_v3(bevp->vec, prevbezt->vec[1]);
            bevp->tilt = prevbezt->tilt;
            bevp->radius = prevbezt->radius;
            bevp->weight = prevbezt->weight;
            bevp->split_tag = true;
            bevp->dupe_tag = false;
            bevp++;
            bl->nr++;
            bl->dupe_nr = 1;
            if (seglen != NULL) {
              *seglen = len_v3v3(prevbezt->vec[1], bezt->vec[1]);
              bevp->offset = *seglen;
              seglen++;
              /* match segbevcount to the cleaned up bevel lists (see STEP 2) */
              if (bevp->offset > treshold) {
                *segbevcount = 1;
              }
              segbevcount++;
            }
          }
          else {
            /* always do all three, to prevent data hanging around */
            int j;

            /* BevPoint must stay aligned to 4 so sizeof(BevPoint)/sizeof(float) works */
            for (j = 0; j < 3; j++) {
              BKE_curve_forward_diff_bezier(prevbezt->vec[1][j],
                                            prevbezt->vec[2][j],
                                            bezt->vec[0][j],
                                            bezt->vec[1][j],
                                            &(bevp->vec[j]),
                                            resolu,
                                            sizeof(BevPoint));
            }

            /* if both arrays are NULL do nothiong */
            tilt_bezpart(prevbezt,
                         bezt,
                         nu,
                         do_tilt ? &bevp->tilt : NULL,
                         do_radius ? &bevp->radius : NULL,
                         do_weight ? &bevp->weight : NULL,
                         resolu,
                         sizeof(BevPoint));

            if (cu->twist_mode == CU_TWIST_TANGENT) {
              forward_diff_bezier_cotangent(prevbezt->vec[1],
                                            prevbezt->vec[2],
                                            bezt->vec[0],
                                            bezt->vec[1],
                                            bevp->tan,
                                            resolu,
                                            sizeof(BevPoint));
            }

            /* indicate with handlecodes double points */
            if (prevbezt->h1 == prevbezt->h2) {
              if (prevbezt->h1 == 0 || prevbezt->h1 == HD_VECT) {
                bevp->split_tag = true;
              }
            }
            else {
              if (prevbezt->h1 == 0 || prevbezt->h1 == HD_VECT) {
                bevp->split_tag = true;
              }
              else if (prevbezt->h2 == 0 || prevbezt->h2 == HD_VECT) {
                bevp->split_tag = true;
              }
            }

            /* seglen */
            if (seglen != NULL) {
              *seglen = 0;
              *segbevcount = 0;
              for (j = 0; j < resolu; j++) {
                bevp0 = bevp;
                bevp++;
                bevp->offset = len_v3v3(bevp0->vec, bevp->vec);
                /* match seglen and segbevcount to the cleaned up bevel lists (see STEP 2) */
                if (bevp->offset > treshold) {
                  *seglen += bevp->offset;
                  *segbevcount += 1;
                }
              }
              seglen++;
              segbevcount++;
            }
            else {
              bevp += resolu;
            }
            bl->nr += resolu;
          }
          prevbezt = bezt;
          bezt++;
        }

        if ((nu->flagu & CU_NURB_CYCLIC) == 0) { /* not cyclic: endpoint */
          copy_v3_v3(bevp->vec, prevbezt->vec[1]);
          bevp->tilt = prevbezt->tilt;
          bevp->radius = prevbezt->radius;
          bevp->weight = prevbezt->weight;

          sub_v3_v3v3(bevp->dir, prevbezt->vec[1], prevbezt->vec[0]);
          normalize_v3(bevp->dir);

          bl->nr++;
        }
      }
      else if (nu->type == CU_NURBS) {
        if (nu->pntsv == 1) {
          len = (resolu * segcount);

          bl = MEM_callocN(sizeof(BevList), "makeBevelList3");
          bl->bevpoints = MEM_calloc_arrayN(len, sizeof(BevPoint), "makeBevelPoints3");
          if (need_seglen && (nu->flagu & CU_NURB_CYCLIC) == 0) {
            bl->seglen = MEM_malloc_arrayN(segcount, sizeof(float), "makeBevelList3_seglen");
            bl->segbevcount = MEM_malloc_arrayN(
                segcount, sizeof(int), "makeBevelList3_segbevcount");
          }
          BLI_addtail(bev, bl);
          bl->nr = len;
          bl->dupe_nr = 0;
          bl->poly = (nu->flagu & CU_NURB_CYCLIC) ? 0 : -1;
          bl->charidx = nu->charidx;

          bevp = bl->bevpoints;
          seglen = bl->seglen;
          segbevcount = bl->segbevcount;

          BKE_nurb_makeCurve(nu,
                             &bevp->vec[0],
                             do_tilt ? &bevp->tilt : NULL,
                             do_radius ? &bevp->radius : NULL,
                             do_weight ? &bevp->weight : NULL,
                             resolu,
                             sizeof(BevPoint));

          /* match seglen and segbevcount to the cleaned up bevel lists (see STEP 2) */
          if (seglen != NULL) {
            nr = segcount;
            bevp0 = bevp;
            bevp++;
            while (nr) {
              int j;
              *seglen = 0;
              *segbevcount = 0;
              /* We keep last bevel segment zero-length. */
              for (j = 0; j < ((nr == 1) ? (resolu - 1) : resolu); j++) {
                bevp->offset = len_v3v3(bevp0->vec, bevp->vec);
                if (bevp->offset > treshold) {
                  *seglen += bevp->offset;
                  *segbevcount += 1;
                }
                bevp0 = bevp;
                bevp++;
              }
              seglen++;
              segbevcount++;
              nr--;
            }
          }

          if ((nu->flagu & CU_NURB_CYCLIC) == 0) {
            bevlist_firstlast_direction_calc_from_bpoint(nu, bl);
          }
        }
      }
    }
  }

  /* STEP 2: DOUBLE POINTS AND AUTOMATIC RESOLUTION, REDUCE DATABLOCKS */
  bl = bev->first;
  while (bl) {
    if (bl->nr) { /* null bevel items come from single points */
      bool is_cyclic = bl->poly != -1;
      nr = bl->nr;
      if (is_cyclic) {
        bevp1 = bl->bevpoints;
        bevp0 = bevp1 + (nr - 1);
      }
      else {
        bevp0 = bl->bevpoints;
        bevp0->offset = 0;
        bevp1 = bevp0 + 1;
      }
      nr--;
      while (nr--) {
        if (seglen != NULL) {
          if (fabsf(bevp1->offset) < treshold) {
            bevp0->dupe_tag = true;
            bl->dupe_nr++;
          }
        }
        else {
          if (fabsf(bevp0->vec[0] - bevp1->vec[0]) < 0.00001f) {
            if (fabsf(bevp0->vec[1] - bevp1->vec[1]) < 0.00001f) {
              if (fabsf(bevp0->vec[2] - bevp1->vec[2]) < 0.00001f) {
                bevp0->dupe_tag = true;
                bl->dupe_nr++;
              }
            }
          }
        }
        bevp0 = bevp1;
        bevp1++;
      }
    }
    bl = bl->next;
  }
  bl = bev->first;
  while (bl) {
    blnext = bl->next;
    if (bl->nr && bl->dupe_nr) {
      nr = bl->nr - bl->dupe_nr + 1; /* +1 because vectorbezier sets flag too */
      blnew = MEM_callocN(sizeof(BevList), "makeBevelList4");
      memcpy(blnew, bl, sizeof(BevList));
      blnew->bevpoints = MEM_calloc_arrayN(nr, sizeof(BevPoint), "makeBevelPoints4");
      if (!blnew->bevpoints) {
        MEM_freeN(blnew);
        break;
      }
      blnew->segbevcount = bl->segbevcount;
      blnew->seglen = bl->seglen;
      blnew->nr = 0;
      BLI_remlink(bev, bl);
      BLI_insertlinkbefore(bev, blnext, blnew); /* to make sure bevlijst is tuned with nurblist */
      bevp0 = bl->bevpoints;
      bevp1 = blnew->bevpoints;
      nr = bl->nr;
      while (nr--) {
        if (bevp0->dupe_tag == 0) {
          memcpy(bevp1, bevp0, sizeof(BevPoint));
          bevp1++;
          blnew->nr++;
        }
        bevp0++;
      }
      if (bl->bevpoints != NULL) {
        MEM_freeN(bl->bevpoints);
      }
      MEM_freeN(bl);
      blnew->dupe_nr = 0;
    }
    bl = blnext;
  }

  /* STEP 3: POLYS COUNT AND AUTOHOLE */
  bl = bev->first;
  poly = 0;
  while (bl) {
    if (bl->nr && bl->poly >= 0) {
      poly++;
      bl->poly = poly;
      bl->hole = 0;
    }
    bl = bl->next;
  }

  /* find extreme left points, also test (turning) direction */
  if (poly > 0) {
    sd = sortdata = MEM_malloc_arrayN(poly, sizeof(struct BevelSort), "makeBevelList5");
    bl = bev->first;
    while (bl) {
      if (bl->poly > 0) {
        BevPoint *bevp;

        min = 300000.0;
        bevp = bl->bevpoints;
        nr = bl->nr;
        while (nr--) {
          if (min > bevp->vec[0]) {
            min = bevp->vec[0];
            bevp1 = bevp;
          }
          bevp++;
        }
        sd->bl = bl;
        sd->left = min;

        bevp = bl->bevpoints;
        if (bevp1 == bevp) {
          bevp0 = bevp + (bl->nr - 1);
        }
        else {
          bevp0 = bevp1 - 1;
        }
        bevp = bevp + (bl->nr - 1);
        if (bevp1 == bevp) {
          bevp2 = bl->bevpoints;
        }
        else {
          bevp2 = bevp1 + 1;
        }

        inp = ((bevp1->vec[0] - bevp0->vec[0]) * (bevp0->vec[1] - bevp2->vec[1]) +
               (bevp0->vec[1] - bevp1->vec[1]) * (bevp0->vec[0] - bevp2->vec[0]));

        if (inp > 0.0f) {
          sd->dir = 1;
        }
        else {
          sd->dir = 0;
        }

        sd++;
      }

      bl = bl->next;
    }
    qsort(sortdata, poly, sizeof(struct BevelSort), vergxcobev);

    sd = sortdata + 1;
    for (a = 1; a < poly; a++, sd++) {
      bl = sd->bl; /* is bl a hole? */
      sd1 = sortdata + (a - 1);
      for (b = a - 1; b >= 0; b--, sd1--) {    /* all polys to the left */
        if (sd1->bl->charidx == bl->charidx) { /* for text, only check matching char */
          if (bevelinside(sd1->bl, bl)) {
            bl->hole = 1 - sd1->bl->hole;
            break;
          }
        }
      }
    }

    /* turning direction */
    if ((cu->flag & CU_3D) == 0) {
      sd = sortdata;
      for (a = 0; a < poly; a++, sd++) {
        if (sd->bl->hole == sd->dir) {
          bl = sd->bl;
          bevp1 = bl->bevpoints;
          bevp2 = bevp1 + (bl->nr - 1);
          nr = bl->nr / 2;
          while (nr--) {
            SWAP(BevPoint, *bevp1, *bevp2);
            bevp1++;
            bevp2--;
          }
        }
      }
    }
    MEM_freeN(sortdata);
  }

  /* STEP 4: 2D-COSINES or 3D ORIENTATION */
  if ((cu->flag & CU_3D) == 0) {
    /* 2D Curves */
    for (bl = bev->first; bl; bl = bl->next) {
      if (bl->nr < 2) {
        BevPoint *bevp = bl->bevpoints;
        unit_qt(bevp->quat);
      }
      else if (bl->nr == 2) { /* 2 pnt, treat separate */
        make_bevel_list_segment_2D(bl);
      }
      else {
        make_bevel_list_2D(bl);
      }
    }
  }
  else {
    /* 3D Curves */
    for (bl = bev->first; bl; bl = bl->next) {
      if (bl->nr < 2) {
        BevPoint *bevp = bl->bevpoints;
        unit_qt(bevp->quat);
      }
      else if (bl->nr == 2) { /* 2 pnt, treat separate */
        make_bevel_list_segment_3D(bl);
      }
      else {
        make_bevel_list_3D(bl, (int)(resolu * cu->twist_smooth), cu->twist_mode);
      }
    }
  }
}

/* ****************** HANDLES ************** */

static void calchandleNurb_intern(BezTriple *bezt,
                                  const BezTriple *prev,
                                  const BezTriple *next,
                                  bool is_fcurve,
                                  bool skip_align,
                                  char fcurve_smoothing)
{
  /* defines to avoid confusion */
#define p2_h1 ((p2)-3)
#define p2_h2 ((p2) + 3)

  const float *p1, *p3;
  float *p2;
  float pt[3];
  float dvec_a[3], dvec_b[3];
  float len, len_a, len_b;
  float len_ratio;
  const float eps = 1e-5;

  /* assume normal handle until we check */
  bezt->f5 = HD_AUTOTYPE_NORMAL;

  if (bezt->h1 == 0 && bezt->h2 == 0) {
    return;
  }

  p2 = bezt->vec[1];

  if (prev == NULL) {
    p3 = next->vec[1];
    pt[0] = 2.0f * p2[0] - p3[0];
    pt[1] = 2.0f * p2[1] - p3[1];
    pt[2] = 2.0f * p2[2] - p3[2];
    p1 = pt;
  }
  else {
    p1 = prev->vec[1];
  }

  if (next == NULL) {
    pt[0] = 2.0f * p2[0] - p1[0];
    pt[1] = 2.0f * p2[1] - p1[1];
    pt[2] = 2.0f * p2[2] - p1[2];
    p3 = pt;
  }
  else {
    p3 = next->vec[1];
  }

  sub_v3_v3v3(dvec_a, p2, p1);
  sub_v3_v3v3(dvec_b, p3, p2);

  if (is_fcurve) {
    len_a = dvec_a[0];
    len_b = dvec_b[0];
  }
  else {
    len_a = len_v3(dvec_a);
    len_b = len_v3(dvec_b);
  }

  if (len_a == 0.0f) {
    len_a = 1.0f;
  }
  if (len_b == 0.0f) {
    len_b = 1.0f;
  }

  len_ratio = len_a / len_b;

  if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM) || ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM)) { /* auto */
    float tvec[3];
    tvec[0] = dvec_b[0] / len_b + dvec_a[0] / len_a;
    tvec[1] = dvec_b[1] / len_b + dvec_a[1] / len_a;
    tvec[2] = dvec_b[2] / len_b + dvec_a[2] / len_a;

    if (is_fcurve) {
      if (fcurve_smoothing != FCURVE_SMOOTH_NONE) {
        /* force the horizontal handle size to be 1/3 of the key interval so that
         * the X component of the parametric bezier curve is a linear spline */
        len = 6.0f / 2.5614f;
      }
      else {
        len = tvec[0];
      }
    }
    else {
      len = len_v3(tvec);
    }
    len *= 2.5614f;

    if (len != 0.0f) {
      /* only for fcurves */
      bool leftviolate = false, rightviolate = false;

      if (!is_fcurve || fcurve_smoothing == FCURVE_SMOOTH_NONE) {
        if (len_a > 5.0f * len_b) {
          len_a = 5.0f * len_b;
        }
        if (len_b > 5.0f * len_a) {
          len_b = 5.0f * len_a;
        }
      }

      if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM)) {
        len_a /= len;
        madd_v3_v3v3fl(p2_h1, p2, tvec, -len_a);

        if ((bezt->h1 == HD_AUTO_ANIM) && next && prev) { /* keep horizontal if extrema */
          float ydiff1 = prev->vec[1][1] - bezt->vec[1][1];
          float ydiff2 = next->vec[1][1] - bezt->vec[1][1];
          if ((ydiff1 <= 0.0f && ydiff2 <= 0.0f) || (ydiff1 >= 0.0f && ydiff2 >= 0.0f)) {
            bezt->vec[0][1] = bezt->vec[1][1];
            bezt->f5 = HD_AUTOTYPE_SPECIAL;
          }
          else { /* handles should not be beyond y coord of two others */
            if (ydiff1 <= 0.0f) {
              if (prev->vec[1][1] > bezt->vec[0][1]) {
                bezt->vec[0][1] = prev->vec[1][1];
                leftviolate = 1;
              }
            }
            else {
              if (prev->vec[1][1] < bezt->vec[0][1]) {
                bezt->vec[0][1] = prev->vec[1][1];
                leftviolate = 1;
              }
            }
          }
        }
      }
      if (ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM)) {
        len_b /= len;
        madd_v3_v3v3fl(p2_h2, p2, tvec, len_b);

        if ((bezt->h2 == HD_AUTO_ANIM) && next && prev) { /* keep horizontal if extrema */
          float ydiff1 = prev->vec[1][1] - bezt->vec[1][1];
          float ydiff2 = next->vec[1][1] - bezt->vec[1][1];
          if ((ydiff1 <= 0.0f && ydiff2 <= 0.0f) || (ydiff1 >= 0.0f && ydiff2 >= 0.0f)) {
            bezt->vec[2][1] = bezt->vec[1][1];
            bezt->f5 = HD_AUTOTYPE_SPECIAL;
          }
          else { /* handles should not be beyond y coord of two others */
            if (ydiff1 <= 0.0f) {
              if (next->vec[1][1] < bezt->vec[2][1]) {
                bezt->vec[2][1] = next->vec[1][1];
                rightviolate = 1;
              }
            }
            else {
              if (next->vec[1][1] > bezt->vec[2][1]) {
                bezt->vec[2][1] = next->vec[1][1];
                rightviolate = 1;
              }
            }
          }
        }
      }
      if (leftviolate || rightviolate) { /* align left handle */
        BLI_assert(is_fcurve);
        /* simple 2d calculation */
        float h1_x = p2_h1[0] - p2[0];
        float h2_x = p2[0] - p2_h2[0];

        if (leftviolate) {
          p2_h2[1] = p2[1] + ((p2[1] - p2_h1[1]) / h1_x) * h2_x;
        }
        else {
          p2_h1[1] = p2[1] + ((p2[1] - p2_h2[1]) / h2_x) * h1_x;
        }
      }
    }
  }

  if (bezt->h1 == HD_VECT) { /* vector */
    madd_v3_v3v3fl(p2_h1, p2, dvec_a, -1.0f / 3.0f);
  }
  if (bezt->h2 == HD_VECT) {
    madd_v3_v3v3fl(p2_h2, p2, dvec_b, 1.0f / 3.0f);
  }

  if (skip_align ||
      /* when one handle is free, alignming makes no sense, see: T35952 */
      (ELEM(HD_FREE, bezt->h1, bezt->h2)) ||
      /* also when no handles are aligned, skip this step */
      (!ELEM(HD_ALIGN, bezt->h1, bezt->h2) && !ELEM(HD_ALIGN_DOUBLESIDE, bezt->h1, bezt->h2))) {
    /* handles need to be updated during animation and applying stuff like hooks,
     * but in such situations it's quite difficult to distinguish in which order
     * align handles should be aligned so skip them for now */
    return;
  }

  len_a = len_v3v3(p2, p2_h1);
  len_b = len_v3v3(p2, p2_h2);

  if (len_a == 0.0f) {
    len_a = 1.0f;
  }
  if (len_b == 0.0f) {
    len_b = 1.0f;
  }

  len_ratio = len_a / len_b;

  if (bezt->f1 & SELECT) {                               /* order of calculation */
    if (ELEM(bezt->h2, HD_ALIGN, HD_ALIGN_DOUBLESIDE)) { /* aligned */
      if (len_a > eps) {
        len = 1.0f / len_ratio;
        p2_h2[0] = p2[0] + len * (p2[0] - p2_h1[0]);
        p2_h2[1] = p2[1] + len * (p2[1] - p2_h1[1]);
        p2_h2[2] = p2[2] + len * (p2[2] - p2_h1[2]);
      }
    }
    if (ELEM(bezt->h1, HD_ALIGN, HD_ALIGN_DOUBLESIDE)) {
      if (len_b > eps) {
        len = len_ratio;
        p2_h1[0] = p2[0] + len * (p2[0] - p2_h2[0]);
        p2_h1[1] = p2[1] + len * (p2[1] - p2_h2[1]);
        p2_h1[2] = p2[2] + len * (p2[2] - p2_h2[2]);
      }
    }
  }
  else {
    if (ELEM(bezt->h1, HD_ALIGN, HD_ALIGN_DOUBLESIDE)) {
      if (len_b > eps) {
        len = len_ratio;
        p2_h1[0] = p2[0] + len * (p2[0] - p2_h2[0]);
        p2_h1[1] = p2[1] + len * (p2[1] - p2_h2[1]);
        p2_h1[2] = p2[2] + len * (p2[2] - p2_h2[2]);
      }
    }
    if (ELEM(bezt->h2, HD_ALIGN, HD_ALIGN_DOUBLESIDE)) { /* aligned */
      if (len_a > eps) {
        len = 1.0f / len_ratio;
        p2_h2[0] = p2[0] + len * (p2[0] - p2_h1[0]);
        p2_h2[1] = p2[1] + len * (p2[1] - p2_h1[1]);
        p2_h2[2] = p2[2] + len * (p2[2] - p2_h1[2]);
      }
    }
  }

#undef p2_h1
#undef p2_h2
}

static void calchandlesNurb_intern(Nurb *nu, bool skip_align)
{
  BezTriple *bezt, *prev, *next;
  int a;

  if (nu->type != CU_BEZIER) {
    return;
  }
  if (nu->pntsu < 2) {
    return;
  }

  a = nu->pntsu;
  bezt = nu->bezt;
  if (nu->flagu & CU_NURB_CYCLIC) {
    prev = bezt + (a - 1);
  }
  else {
    prev = NULL;
  }
  next = bezt + 1;

  while (a--) {
    calchandleNurb_intern(bezt, prev, next, 0, skip_align, 0);
    prev = bezt;
    if (a == 1) {
      if (nu->flagu & CU_NURB_CYCLIC) {
        next = nu->bezt;
      }
      else {
        next = NULL;
      }
    }
    else {
      next++;
    }

    bezt++;
  }
}

/* A utility function for allocating a number of arrays of the same length
 * with easy error checking and deallocation, and an easy way to add or remove
 * arrays that are processed in this way when changing code.
 *
 * floats, chars: NULL-terminated arrays of pointers to array pointers that need to be allocated.
 *
 * Returns: pointer to the buffer that contains all of the arrays.
 */
static void *allocate_arrays(int count, float ***floats, char ***chars, const char *name)
{
  size_t num_floats = 0, num_chars = 0;

  while (floats && floats[num_floats]) {
    num_floats++;
  }

  while (chars && chars[num_chars]) {
    num_chars++;
  }

  void *buffer = (float *)MEM_malloc_arrayN(count, (sizeof(float) * num_floats + num_chars), name);

  if (!buffer) {
    return NULL;
  }

  float *fptr = buffer;

  for (int i = 0; i < num_floats; i++, fptr += count) {
    *floats[i] = fptr;
  }

  char *cptr = (char *)fptr;

  for (int i = 0; i < num_chars; i++, cptr += count) {
    *chars[i] = cptr;
  }

  return buffer;
}

static void free_arrays(void *buffer)
{
  MEM_freeN(buffer);
}

/* computes in which direction to change h[i] to satisfy conditions better */
static float bezier_relax_direction(
    float *a, float *b, float *c, float *d, float *h, int i, int count)
{
  /* current deviation between sides of the equation */
  float state = a[i] * h[(i + count - 1) % count] + b[i] * h[i] + c[i] * h[(i + 1) % count] - d[i];

  /* only the sign is meaningful */
  return -state * b[i];
}

static void bezier_lock_unknown(float *a, float *b, float *c, float *d, int i, float value)
{
  a[i] = c[i] = 0.0f;
  b[i] = 1.0f;
  d[i] = value;
}

static void bezier_restore_equation(
    float *a, float *b, float *c, float *d, float *a0, float *b0, float *c0, float *d0, int i)
{
  a[i] = a0[i];
  b[i] = b0[i];
  c[i] = c0[i];
  d[i] = d0[i];
}

static bool tridiagonal_solve_with_limits(
    float *a, float *b, float *c, float *d, float *h, float *hmin, float *hmax, int solve_count)
{
  float *a0, *b0, *c0, *d0;
  float **arrays[] = {&a0, &b0, &c0, &d0, NULL};
  char *is_locked, *num_unlocks;
  char **flagarrays[] = {&is_locked, &num_unlocks, NULL};

  void *tmps = allocate_arrays(solve_count, arrays, flagarrays, "tridiagonal_solve_with_limits");
  if (!tmps) {
    return false;
  }

  memcpy(a0, a, sizeof(float) * solve_count);
  memcpy(b0, b, sizeof(float) * solve_count);
  memcpy(c0, c, sizeof(float) * solve_count);
  memcpy(d0, d, sizeof(float) * solve_count);

  memset(is_locked, 0, solve_count);
  memset(num_unlocks, 0, solve_count);

  bool overshoot, unlocked;

  do {
    if (!BLI_tridiagonal_solve_cyclic(a, b, c, d, h, solve_count)) {
      free_arrays(tmps);
      return false;
    }

    /* first check if any handles overshoot the limits, and lock them */
    bool all = false, locked = false;

    overshoot = unlocked = false;

    do {
      for (int i = 0; i < solve_count; i++) {
        if (h[i] >= hmin[i] && h[i] <= hmax[i]) {
          continue;
        }

        overshoot = true;

        float target = h[i] > hmax[i] ? hmax[i] : hmin[i];

        /* heuristically only lock handles that go in the right direction if there are such ones */
        if (target != 0.0f || all) {
          /* mark item locked */
          is_locked[i] = 1;

          bezier_lock_unknown(a, b, c, d, i, target);
          locked = true;
        }
      }

      all = true;
    } while (overshoot && !locked);

    /* If no handles overshot and were locked,
     * see if it may be a good idea to unlock some handles. */
    if (!locked) {
      for (int i = 0; i < solve_count; i++) {
        // to definitely avoid infinite loops limit this to 2 times
        if (!is_locked[i] || num_unlocks[i] >= 2) {
          continue;
        }

        /* if the handle wants to move in allowable direction, release it */
        float relax = bezier_relax_direction(a0, b0, c0, d0, h, i, solve_count);

        if ((relax > 0 && h[i] < hmax[i]) || (relax < 0 && h[i] > hmin[i])) {
          bezier_restore_equation(a, b, c, d, a0, b0, c0, d0, i);

          is_locked[i] = 0;
          num_unlocks[i]++;
          unlocked = true;
        }
      }
    }
  } while (overshoot || unlocked);

  free_arrays(tmps);
  return true;
}

/*
 * This function computes the handles of a series of auto bezier points
 * on the basis of 'no acceleration discontinuities' at the points.
 * The first and last bezier points are considered 'fixed' (their handles are not touched)
 * The result is the smoothest possible trajectory going through intermediate points.
 * The difficulty is that the handles depends on their neighbors.
 *
 * The exact solution is found by solving a tridiagonal matrix equation formed
 * by the continuity and boundary conditions. Although theoretically handle position
 * is affected by all other points of the curve segment, in practice the influence
 * decreases exponentially with distance.
 *
 * Note: this algorithm assumes that the handle horizontal size if always 1/3 of the
 * of the interval to the next point. This rule ensures linear interpolation of time.
 *
 * ^ height (co 1)
 * |                                            yN
 * |                                   yN-1     |
 * |                      y2           |        |
 * |           y1         |            |        |
 * |    y0     |          |            |        |
 * |    |      |          |            |        |
 * |    |      |          |            |        |
 * |    |      |          |            |        |
 * |-------t1---------t2--------- ~ --------tN-------------------> time (co 0)
 * Mathematical basis:
 *
 * 1. Handle lengths on either side of each point are connected by a factor
 *    ensuring continuity of the first derivative:
 *
 *    l[i] = t[i+1]/t[i]
 *
 * 2. The tridiagonal system is formed by the following equation, which is derived
 *    by differentiating the bezier curve and specifies second derivative continuity
 *    at every point:
 *
 *    l[i]^2 * h[i-1] + (2*l[i]+2) * h[i] + 1/l[i+1] * h[i+1] = (y[i]-y[i-1])*l[i]^2 + y[i+1]-y[i]
 *
 * 3. If this point is adjacent to a manually set handle with X size not equal to 1/3
 *    of the horizontal interval, this equation becomes slightly more complex:
 *
 *    l[i]^2 * h[i-1] + (3*(1-R[i-1])*l[i] + 3*(1-L[i+1])) * h[i] + 1/l[i+1] * h[i+1] = (y[i]-y[i-1])*l[i]^2 + y[i+1]-y[i]
 *
 *    The difference between equations amounts to this, and it's obvious that when R[i-1]
 *    and L[i+1] are both 1/3, it becomes zero:
 *
 *    ( (1-3*R[i-1])*l[i] + (1-3*L[i+1]) ) * h[i]
 *
 * 4. The equations for zero acceleration border conditions are basically the above
 *    equation with parts omitted, so the handle size correction also applies.
 */

static void bezier_eq_continuous(
    float *a, float *b, float *c, float *d, float *dy, float *l, int i)
{
  a[i] = l[i] * l[i];
  b[i] = 2.0f * (l[i] + 1);
  c[i] = 1.0f / l[i + 1];
  d[i] = dy[i] * l[i] * l[i] + dy[i + 1];
}

static void bezier_eq_noaccel_right(
    float *a, float *b, float *c, float *d, float *dy, float *l, int i)
{
  a[i] = 0.0f;
  b[i] = 2.0f;
  c[i] = 1.0f / l[i + 1];
  d[i] = dy[i + 1];
}

static void bezier_eq_noaccel_left(
    float *a, float *b, float *c, float *d, float *dy, float *l, int i)
{
  a[i] = l[i] * l[i];
  b[i] = 2.0f * l[i];
  c[i] = 0.0f;
  d[i] = dy[i] * l[i] * l[i];
}

/* auto clamp prevents its own point going the wrong way, and adjacent handles overshooting */
static void bezier_clamp(
    float *hmax, float *hmin, int i, float dy, bool no_reverse, bool no_overshoot)
{
  if (dy > 0) {
    if (no_overshoot) {
      hmax[i] = min_ff(hmax[i], dy);
    }
    if (no_reverse) {
      hmin[i] = 0.0f;
    }
  }
  else if (dy < 0) {
    if (no_reverse) {
      hmax[i] = 0.0f;
    }
    if (no_overshoot) {
      hmin[i] = max_ff(hmin[i], dy);
    }
  }
  else if (no_reverse || no_overshoot) {
    hmax[i] = hmin[i] = 0.0f;
  }
}

/* write changes to a bezier handle */
static void bezier_output_handle_inner(BezTriple *bezt, bool right, float newval[3], bool endpoint)
{
  float tmp[3];

  int idx = right ? 2 : 0;
  char hr = right ? bezt->h2 : bezt->h1;
  char hm = right ? bezt->h1 : bezt->h2;

  /* only assign Auto/Vector handles */
  if (!ELEM(hr, HD_AUTO, HD_AUTO_ANIM, HD_VECT)) {
    return;
  }

  copy_v3_v3(bezt->vec[idx], newval);

  /* fix up the Align handle if any */
  if (ELEM(hm, HD_ALIGN, HD_ALIGN_DOUBLESIDE)) {
    float hlen = len_v3v3(bezt->vec[1], bezt->vec[2 - idx]);
    float h2len = len_v3v3(bezt->vec[1], bezt->vec[idx]);

    sub_v3_v3v3(tmp, bezt->vec[1], bezt->vec[idx]);
    madd_v3_v3v3fl(bezt->vec[2 - idx], bezt->vec[1], tmp, hlen / h2len);
  }
  /* at end points of the curve, mirror handle to the other side */
  else if (endpoint && ELEM(hm, HD_AUTO, HD_AUTO_ANIM, HD_VECT)) {
    sub_v3_v3v3(tmp, bezt->vec[1], bezt->vec[idx]);
    add_v3_v3v3(bezt->vec[2 - idx], bezt->vec[1], tmp);
  }
}

static void bezier_output_handle(BezTriple *bezt, bool right, float dy, bool endpoint)
{
  float tmp[3];

  copy_v3_v3(tmp, bezt->vec[right ? 2 : 0]);

  tmp[1] = bezt->vec[1][1] + dy;

  bezier_output_handle_inner(bezt, right, tmp, endpoint);
}

static bool bezier_check_solve_end_handle(BezTriple *bezt, char htype, bool end)
{
  return (htype == HD_VECT) ||
         (end && ELEM(htype, HD_AUTO, HD_AUTO_ANIM) && bezt->f5 == HD_AUTOTYPE_NORMAL);
}

static float bezier_calc_handle_adj(float hsize[2], float dx)
{
  /* if handles intersect in x direction, they are scaled to fit */
  float fac = dx / (hsize[0] + dx / 3.0f);
  if (fac < 1.0f) {
    mul_v2_fl(hsize, fac);
  }
  return 1.0f - 3.0f * hsize[0] / dx;
}

static void bezier_handle_calc_smooth_fcurve(
    BezTriple *bezt, int total, int start, int count, bool cycle)
{
  float *dx, *dy, *l, *a, *b, *c, *d, *h, *hmax, *hmin;
  float **arrays[] = {&dx, &dy, &l, &a, &b, &c, &d, &h, &hmax, &hmin, NULL};

  int solve_count = count;

  /* verify index ranges */

  if (count < 2) {
    return;
  }

  BLI_assert(start < total - 1 && count <= total);
  BLI_assert(start + count <= total || cycle);

  bool full_cycle = (start == 0 && count == total && cycle);

  BezTriple *bezt_first = &bezt[start];
  BezTriple *bezt_last =
      &bezt[(start + count > total) ? start + count - total : start + count - 1];

  bool solve_first = bezier_check_solve_end_handle(bezt_first, bezt_first->h2, start == 0);
  bool solve_last = bezier_check_solve_end_handle(
      bezt_last, bezt_last->h1, start + count == total);

  if (count == 2 && !full_cycle && solve_first == solve_last) {
    return;
  }

  /* allocate all */

  void *tmp_buffer = allocate_arrays(count, arrays, NULL, "bezier_calc_smooth_tmp");
  if (!tmp_buffer) {
    return;
  }

  /* point locations */

  dx[0] = dy[0] = NAN_FLT;

  for (int i = 1, j = start + 1; i < count; i++, j++) {
    dx[i] = bezt[j].vec[1][0] - bezt[j - 1].vec[1][0];
    dy[i] = bezt[j].vec[1][1] - bezt[j - 1].vec[1][1];

    /* when cyclic, jump from last point to first */
    if (cycle && j == total - 1) {
      j = 0;
    }
  }

  /* ratio of x intervals */

  l[0] = l[count - 1] = 1.0f;

  for (int i = 1; i < count - 1; i++) {
    l[i] = dx[i + 1] / dx[i];
  }

  /* compute handle clamp ranges */

  bool clamped_prev = false, clamped_cur = ELEM(HD_AUTO_ANIM, bezt_first->h1, bezt_first->h2);

  for (int i = 0; i < count; i++) {
    hmax[i] = FLT_MAX;
    hmin[i] = -FLT_MAX;
  }

  for (int i = 1, j = start + 1; i < count; i++, j++) {
    clamped_prev = clamped_cur;
    clamped_cur = ELEM(HD_AUTO_ANIM, bezt[j].h1, bezt[j].h2);

    if (cycle && j == total - 1) {
      j = 0;
      clamped_cur = clamped_cur || ELEM(HD_AUTO_ANIM, bezt[j].h1, bezt[j].h2);
    }

    bezier_clamp(hmax, hmin, i - 1, dy[i], clamped_prev, clamped_prev);
    bezier_clamp(hmax, hmin, i, dy[i] * l[i], clamped_cur, clamped_cur);
  }

  /* full cycle merges first and last points into continuous loop */

  float first_handle_adj = 0.0f, last_handle_adj = 0.0f;

  if (full_cycle) {
    /* reduce the number of unknowns by one */
    int i = solve_count = count - 1;

    dx[0] = dx[i];
    dy[0] = dy[i];

    l[0] = l[i] = dx[1] / dx[0];

    hmin[0] = max_ff(hmin[0], hmin[i]);
    hmax[0] = min_ff(hmax[0], hmax[i]);

    solve_first = solve_last = true;

    bezier_eq_continuous(a, b, c, d, dy, l, 0);
  }
  else {
    float tmp[2];

    /* boundary condition: fixed handles or zero curvature */
    if (!solve_first) {
      sub_v2_v2v2(tmp, bezt_first->vec[2], bezt_first->vec[1]);
      first_handle_adj = bezier_calc_handle_adj(tmp, dx[1]);

      bezier_lock_unknown(a, b, c, d, 0, tmp[1]);
    }
    else {
      bezier_eq_noaccel_right(a, b, c, d, dy, l, 0);
    }

    if (!solve_last) {
      sub_v2_v2v2(tmp, bezt_last->vec[1], bezt_last->vec[0]);
      last_handle_adj = bezier_calc_handle_adj(tmp, dx[count - 1]);

      bezier_lock_unknown(a, b, c, d, count - 1, tmp[1]);
    }
    else {
      bezier_eq_noaccel_left(a, b, c, d, dy, l, count - 1);
    }
  }

  /* main tridiagonal system of equations */

  for (int i = 1; i < count - 1; i++) {
    bezier_eq_continuous(a, b, c, d, dy, l, i);
  }

  /* apply correction for user-defined handles with nonstandard x positions */

  if (!full_cycle) {
    if (count > 2 || solve_last) {
      b[1] += l[1] * first_handle_adj;
    }

    if (count > 2 || solve_first) {
      b[count - 2] += last_handle_adj;
    }
  }

  /* solve and output results */

  if (tridiagonal_solve_with_limits(a, b, c, d, h, hmin, hmax, solve_count)) {
    if (full_cycle) {
      h[count - 1] = h[0];
    }

    for (int i = 1, j = start + 1; i < count - 1; i++, j++) {
      bool end = (j == total - 1);

      bezier_output_handle(&bezt[j], false, -h[i] / l[i], end);

      if (end) {
        j = 0;
      }

      bezier_output_handle(&bezt[j], true, h[i], end);
    }

    if (solve_first) {
      bezier_output_handle(bezt_first, true, h[0], start == 0);
    }

    if (solve_last) {
      bezier_output_handle(bezt_last, false, -h[count - 1] / l[count - 1], start + count == total);
    }
  }

  /* free all */

  free_arrays(tmp_buffer);
}

static bool is_free_auto_point(BezTriple *bezt)
{
  return BEZT_IS_AUTOH(bezt) && bezt->f5 == HD_AUTOTYPE_NORMAL;
}

void BKE_nurb_handle_smooth_fcurve(BezTriple *bezt, int total, bool cycle)
{
  /* ignore cyclic extrapolation if end points are locked */
  cycle = cycle && is_free_auto_point(&bezt[0]) && is_free_auto_point(&bezt[total - 1]);

  /* if cyclic, try to find a sequence break point */
  int search_base = 0;

  if (cycle) {
    for (int i = 1; i < total - 1; i++) {
      if (!is_free_auto_point(&bezt[i])) {
        search_base = i;
        break;
      }
    }

    /* all points of the curve are freely changeable auto handles - solve as full cycle */
    if (search_base == 0) {
      bezier_handle_calc_smooth_fcurve(bezt, total, 0, total, cycle);
      return;
    }
  }

  /* Find continuous subsequences of free auto handles and smooth them, starting at
   * search_base. In cyclic mode these subsequences can span the cycle boundary. */
  int start = search_base, count = 1;

  for (int i = 1, j = start + 1; i < total; i++, j++) {
    /* in cyclic mode: jump from last to first point when necessary */
    if (j == total - 1 && cycle) {
      j = 0;
    }

    /* non auto handle closes the list (we come here at least for the last handle, see above) */
    if (!is_free_auto_point(&bezt[j])) {
      bezier_handle_calc_smooth_fcurve(bezt, total, start, count + 1, cycle);
      start = j;
      count = 1;
    }
    else {
      count++;
    }
  }

  if (count > 1) {
    bezier_handle_calc_smooth_fcurve(bezt, total, start, count, cycle);
  }
}

void BKE_nurb_handle_calc(
    BezTriple *bezt, BezTriple *prev, BezTriple *next, const bool is_fcurve, const char smoothing)
{
  calchandleNurb_intern(bezt, prev, next, is_fcurve, false, smoothing);
}

void BKE_nurb_handles_calc(Nurb *nu) /* first, if needed, set handle flags */
{
  calchandlesNurb_intern(nu, false);
}

/**
 * Workaround #BKE_nurb_handles_calc logic
 * that makes unselected align to the selected handle.
 */
static void nurbList_handles_swap_select(Nurb *nu)
{
  BezTriple *bezt;
  int i;

  for (i = nu->pntsu, bezt = nu->bezt; i--; bezt++) {
    if ((bezt->f1 & SELECT) != (bezt->f3 & SELECT)) {
      bezt->f1 ^= SELECT;
      bezt->f3 ^= SELECT;
    }
  }
}

/* internal use only (weak) */
static void nurb_handles_calc__align_selected(Nurb *nu)
{
  nurbList_handles_swap_select(nu);
  BKE_nurb_handles_calc(nu);
  nurbList_handles_swap_select(nu);
}

/* similar to BKE_nurb_handle_calc but for curves and
 * figures out the previous and next for us */
void BKE_nurb_handle_calc_simple(Nurb *nu, BezTriple *bezt)
{
  if (nu->pntsu > 1) {
    BezTriple *prev = BKE_nurb_bezt_get_prev(nu, bezt);
    BezTriple *next = BKE_nurb_bezt_get_next(nu, bezt);
    BKE_nurb_handle_calc(bezt, prev, next, 0, 0);
  }
}

void BKE_nurb_handle_calc_simple_auto(Nurb *nu, BezTriple *bezt)
{
  if (nu->pntsu > 1) {
    const char h1_back = bezt->h1, h2_back = bezt->h2;

    bezt->h1 = bezt->h2 = HD_AUTO;

    /* Override handle types to HD_AUTO and recalculate */
    BKE_nurb_handle_calc_simple(nu, bezt);

    bezt->h1 = h1_back;
    bezt->h2 = h2_back;
  }
}

/**
 * Use when something has changed handle positions.
 *
 * The caller needs to recalculate handles.
 */
void BKE_nurb_bezt_handle_test(BezTriple *bezt, const bool use_handle)
{
  short flag = 0;

#define SEL_F1 (1 << 0)
#define SEL_F2 (1 << 1)
#define SEL_F3 (1 << 2)

  if (use_handle) {
    if (bezt->f1 & SELECT) {
      flag |= SEL_F1;
    }
    if (bezt->f2 & SELECT) {
      flag |= SEL_F2;
    }
    if (bezt->f3 & SELECT) {
      flag |= SEL_F3;
    }
  }
  else {
    flag = (bezt->f2 & SELECT) ? (SEL_F1 | SEL_F2 | SEL_F3) : 0;
  }

  /* check for partial selection */
  if (!ELEM(flag, 0, SEL_F1 | SEL_F2 | SEL_F3)) {
    if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM)) {
      bezt->h1 = HD_ALIGN;
    }
    if (ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM)) {
      bezt->h2 = HD_ALIGN;
    }

    if (bezt->h1 == HD_VECT) {
      if ((!(flag & SEL_F1)) != (!(flag & SEL_F2))) {
        bezt->h1 = HD_FREE;
      }
    }
    if (bezt->h2 == HD_VECT) {
      if ((!(flag & SEL_F3)) != (!(flag & SEL_F2))) {
        bezt->h2 = HD_FREE;
      }
    }
  }

#undef SEL_F1
#undef SEL_F2
#undef SEL_F3
}

void BKE_nurb_handles_test(Nurb *nu, const bool use_handle)
{
  BezTriple *bezt;
  int a;

  if (nu->type != CU_BEZIER) {
    return;
  }

  bezt = nu->bezt;
  a = nu->pntsu;
  while (a--) {
    BKE_nurb_bezt_handle_test(bezt, use_handle);
    bezt++;
  }

  BKE_nurb_handles_calc(nu);
}

void BKE_nurb_handles_autocalc(Nurb *nu, int flag)
{
  /* checks handle coordinates and calculates type */
  const float eps = 0.0001f;
  const float eps_sq = eps * eps;

  BezTriple *bezt2, *bezt1, *bezt0;
  int i;

  if (nu == NULL || nu->bezt == NULL) {
    return;
  }

  bezt2 = nu->bezt;
  bezt1 = bezt2 + (nu->pntsu - 1);
  bezt0 = bezt1 - 1;
  i = nu->pntsu;

  while (i--) {
    bool align = false, leftsmall = false, rightsmall = false;

    /* left handle: */
    if (flag == 0 || (bezt1->f1 & flag)) {
      bezt1->h1 = HD_FREE;
      /* distance too short: vectorhandle */
      if (len_squared_v3v3(bezt1->vec[1], bezt0->vec[1]) < eps_sq) {
        bezt1->h1 = HD_VECT;
        leftsmall = true;
      }
      else {
        /* aligned handle? */
        if (dist_squared_to_line_v3(bezt1->vec[1], bezt1->vec[0], bezt1->vec[2]) < eps_sq) {
          align = true;
          bezt1->h1 = HD_ALIGN;
        }
        /* or vector handle? */
        if (dist_squared_to_line_v3(bezt1->vec[0], bezt1->vec[1], bezt0->vec[1]) < eps_sq) {
          bezt1->h1 = HD_VECT;
        }
      }
    }
    /* right handle: */
    if (flag == 0 || (bezt1->f3 & flag)) {
      bezt1->h2 = HD_FREE;
      /* distance too short: vectorhandle */
      if (len_squared_v3v3(bezt1->vec[1], bezt2->vec[1]) < eps_sq) {
        bezt1->h2 = HD_VECT;
        rightsmall = true;
      }
      else {
        /* aligned handle? */
        if (align) {
          bezt1->h2 = HD_ALIGN;
        }

        /* or vector handle? */
        if (dist_squared_to_line_v3(bezt1->vec[2], bezt1->vec[1], bezt2->vec[1]) < eps_sq) {
          bezt1->h2 = HD_VECT;
        }
      }
    }
    if (leftsmall && bezt1->h2 == HD_ALIGN) {
      bezt1->h2 = HD_FREE;
    }
    if (rightsmall && bezt1->h1 == HD_ALIGN) {
      bezt1->h1 = HD_FREE;
    }

    /* undesired combination: */
    if (bezt1->h1 == HD_ALIGN && bezt1->h2 == HD_VECT) {
      bezt1->h1 = HD_FREE;
    }
    if (bezt1->h2 == HD_ALIGN && bezt1->h1 == HD_VECT) {
      bezt1->h2 = HD_FREE;
    }

    bezt0 = bezt1;
    bezt1 = bezt2;
    bezt2++;
  }

  BKE_nurb_handles_calc(nu);
}

void BKE_nurbList_handles_autocalc(ListBase *editnurb, int flag)
{
  Nurb *nu;

  nu = editnurb->first;
  while (nu) {
    BKE_nurb_handles_autocalc(nu, flag);
    nu = nu->next;
  }
}

void BKE_nurbList_handles_set(ListBase *editnurb, const char code)
{
  /* code==1: set autohandle */
  /* code==2: set vectorhandle */
  /* code==3 (HD_ALIGN) it toggle, vectorhandles become HD_FREE */
  /* code==4: sets icu flag to become IPO_AUTO_HORIZ, horizontal extremes on auto-handles */
  /* code==5: Set align, like 3 but no toggle */
  /* code==6: Clear align, like 3 but no toggle */
  Nurb *nu;
  BezTriple *bezt;
  int a;

  if (ELEM(code, HD_AUTO, HD_VECT)) {
    nu = editnurb->first;
    while (nu) {
      if (nu->type == CU_BEZIER) {
        bezt = nu->bezt;
        a = nu->pntsu;
        while (a--) {
          if ((bezt->f1 & SELECT) || (bezt->f3 & SELECT)) {
            if (bezt->f1 & SELECT) {
              bezt->h1 = code;
            }
            if (bezt->f3 & SELECT) {
              bezt->h2 = code;
            }
            if (bezt->h1 != bezt->h2) {
              if (ELEM(bezt->h1, HD_ALIGN, HD_AUTO)) {
                bezt->h1 = HD_FREE;
              }
              if (ELEM(bezt->h2, HD_ALIGN, HD_AUTO)) {
                bezt->h2 = HD_FREE;
              }
            }
          }
          bezt++;
        }

        /* like BKE_nurb_handles_calc but moves selected */
        nurb_handles_calc__align_selected(nu);
      }
      nu = nu->next;
    }
  }
  else {
    char h_new = HD_FREE;

    /* there is 1 handle not FREE: FREE it all, else make ALIGNED  */
    if (code == 5) {
      h_new = HD_ALIGN;
    }
    else if (code == 6) {
      h_new = HD_FREE;
    }
    else {
      /* Toggle */
      for (nu = editnurb->first; nu; nu = nu->next) {
        if (nu->type == CU_BEZIER) {
          bezt = nu->bezt;
          a = nu->pntsu;
          while (a--) {
            if (((bezt->f1 & SELECT) && bezt->h1 != HD_FREE) ||
                ((bezt->f3 & SELECT) && bezt->h2 != HD_FREE)) {
              h_new = HD_AUTO;
              break;
            }
            bezt++;
          }
        }
      }
      h_new = (h_new == HD_FREE) ? HD_ALIGN : HD_FREE;
    }
    for (nu = editnurb->first; nu; nu = nu->next) {
      if (nu->type == CU_BEZIER) {
        bezt = nu->bezt;
        a = nu->pntsu;
        while (a--) {
          if (bezt->f1 & SELECT) {
            bezt->h1 = h_new;
          }
          if (bezt->f3 & SELECT) {
            bezt->h2 = h_new;
          }

          bezt++;
        }

        /* like BKE_nurb_handles_calc but moves selected */
        nurb_handles_calc__align_selected(nu);
      }
    }
  }
}

void BKE_nurbList_handles_recalculate(ListBase *editnurb, const bool calc_length, const char flag)
{
  Nurb *nu;
  BezTriple *bezt;
  int a;

  for (nu = editnurb->first; nu; nu = nu->next) {
    if (nu->type == CU_BEZIER) {
      bool changed = false;

      for (a = nu->pntsu, bezt = nu->bezt; a--; bezt++) {

        const bool h1_select = (bezt->f1 & flag) == flag;
        const bool h2_select = (bezt->f3 & flag) == flag;

        if (h1_select || h2_select) {

          float co1_back[3], co2_back[3];

          copy_v3_v3(co1_back, bezt->vec[0]);
          copy_v3_v3(co2_back, bezt->vec[2]);

          BKE_nurb_handle_calc_simple_auto(nu, bezt);

          if (h1_select) {
            if (!calc_length) {
              dist_ensure_v3_v3fl(bezt->vec[0], bezt->vec[1], len_v3v3(co1_back, bezt->vec[1]));
            }
          }
          else {
            copy_v3_v3(bezt->vec[0], co1_back);
          }

          if (h2_select) {
            if (!calc_length) {
              dist_ensure_v3_v3fl(bezt->vec[2], bezt->vec[1], len_v3v3(co2_back, bezt->vec[1]));
            }
          }
          else {
            copy_v3_v3(bezt->vec[2], co2_back);
          }

          changed = true;
        }
      }

      if (changed) {
        /* Recalculate the whole curve */
        BKE_nurb_handles_calc(nu);
      }
    }
  }
}

void BKE_nurbList_flag_set(ListBase *editnurb, short flag)
{
  Nurb *nu;
  BezTriple *bezt;
  BPoint *bp;
  int a;

  for (nu = editnurb->first; nu; nu = nu->next) {
    if (nu->type == CU_BEZIER) {
      a = nu->pntsu;
      bezt = nu->bezt;
      while (a--) {
        bezt->f1 = bezt->f2 = bezt->f3 = flag;
        bezt++;
      }
    }
    else {
      a = nu->pntsu * nu->pntsv;
      bp = nu->bp;
      while (a--) {
        bp->f1 = flag;
        bp++;
      }
    }
  }
}

void BKE_nurb_direction_switch(Nurb *nu)
{
  BezTriple *bezt1, *bezt2;
  BPoint *bp1, *bp2;
  float *fp1, *fp2, *tempf;
  int a, b;

  if (nu->pntsu == 1 && nu->pntsv == 1) {
    return;
  }

  if (nu->type == CU_BEZIER) {
    a = nu->pntsu;
    bezt1 = nu->bezt;
    bezt2 = bezt1 + (a - 1);
    if (a & 1) {
      a += 1; /* if odd, also swap middle content */
    }
    a /= 2;
    while (a > 0) {
      if (bezt1 != bezt2) {
        SWAP(BezTriple, *bezt1, *bezt2);
      }

      swap_v3_v3(bezt1->vec[0], bezt1->vec[2]);

      if (bezt1 != bezt2) {
        swap_v3_v3(bezt2->vec[0], bezt2->vec[2]);
      }

      SWAP(char, bezt1->h1, bezt1->h2);
      SWAP(char, bezt1->f1, bezt1->f3);

      if (bezt1 != bezt2) {
        SWAP(char, bezt2->h1, bezt2->h2);
        SWAP(char, bezt2->f1, bezt2->f3);
        bezt1->tilt = -bezt1->tilt;
        bezt2->tilt = -bezt2->tilt;
      }
      else {
        bezt1->tilt = -bezt1->tilt;
      }
      a--;
      bezt1++;
      bezt2--;
    }
  }
  else if (nu->pntsv == 1) {
    a = nu->pntsu;
    bp1 = nu->bp;
    bp2 = bp1 + (a - 1);
    a /= 2;
    while (bp1 != bp2 && a > 0) {
      SWAP(BPoint, *bp1, *bp2);
      a--;
      bp1->tilt = -bp1->tilt;
      bp2->tilt = -bp2->tilt;
      bp1++;
      bp2--;
    }
    /* If there're odd number of points no need to touch coord of middle one,
     * but still need to change it's tilt.
     */
    if (nu->pntsu & 1) {
      bp1->tilt = -bp1->tilt;
    }
    if (nu->type == CU_NURBS) {
      /* no knots for too short paths */
      if (nu->knotsu) {
        /* inverse knots */
        a = KNOTSU(nu);
        fp1 = nu->knotsu;
        fp2 = fp1 + (a - 1);
        a /= 2;
        while (fp1 != fp2 && a > 0) {
          SWAP(float, *fp1, *fp2);
          a--;
          fp1++;
          fp2--;
        }
        /* and make in increasing order again */
        a = KNOTSU(nu);
        fp1 = nu->knotsu;
        fp2 = tempf = MEM_malloc_arrayN(a, sizeof(float), "switchdirect");
        a--;
        fp2[a] = fp1[a];
        while (a--) {
          fp2[0] = fabsf(fp1[1] - fp1[0]);
          fp1++;
          fp2++;
        }

        a = KNOTSU(nu) - 1;
        fp1 = nu->knotsu;
        fp2 = tempf;
        fp1[0] = 0.0;
        fp1++;
        while (a--) {
          fp1[0] = fp1[-1] + fp2[0];
          fp1++;
          fp2++;
        }
        MEM_freeN(tempf);
      }
    }
  }
  else {
    for (b = 0; b < nu->pntsv; b++) {
      bp1 = nu->bp + b * nu->pntsu;
      a = nu->pntsu;
      bp2 = bp1 + (a - 1);
      a /= 2;

      while (bp1 != bp2 && a > 0) {
        SWAP(BPoint, *bp1, *bp2);
        a--;
        bp1++;
        bp2--;
      }
    }
  }
}

float (*BKE_curve_nurbs_vertexCos_get(ListBase *lb, int *r_numVerts))[3]
{
  int i, numVerts = *r_numVerts = BKE_nurbList_verts_count(lb);
  float *co, (*cos)[3] = MEM_malloc_arrayN(numVerts, sizeof(*cos), "cu_vcos");
  Nurb *nu;

  co = cos[0];
  for (nu = lb->first; nu; nu = nu->next) {
    if (nu->type == CU_BEZIER) {
      BezTriple *bezt = nu->bezt;

      for (i = 0; i < nu->pntsu; i++, bezt++) {
        copy_v3_v3(co, bezt->vec[0]);
        co += 3;
        copy_v3_v3(co, bezt->vec[1]);
        co += 3;
        copy_v3_v3(co, bezt->vec[2]);
        co += 3;
      }
    }
    else {
      BPoint *bp = nu->bp;

      for (i = 0; i < nu->pntsu * nu->pntsv; i++, bp++) {
        copy_v3_v3(co, bp->vec);
        co += 3;
      }
    }
  }

  return cos;
}

void BK_curve_nurbs_vertexCos_apply(ListBase *lb, float (*vertexCos)[3])
{
  const float *co = vertexCos[0];
  Nurb *nu;
  int i;

  for (nu = lb->first; nu; nu = nu->next) {
    if (nu->type == CU_BEZIER) {
      BezTriple *bezt = nu->bezt;

      for (i = 0; i < nu->pntsu; i++, bezt++) {
        copy_v3_v3(bezt->vec[0], co);
        co += 3;
        copy_v3_v3(bezt->vec[1], co);
        co += 3;
        copy_v3_v3(bezt->vec[2], co);
        co += 3;
      }
    }
    else {
      BPoint *bp = nu->bp;

      for (i = 0; i < nu->pntsu * nu->pntsv; i++, bp++) {
        copy_v3_v3(bp->vec, co);
        co += 3;
      }
    }

    calchandlesNurb_intern(nu, true);
  }
}

float (*BKE_curve_nurbs_keyVertexCos_get(ListBase *lb, float *key))[3]
{
  int i, numVerts = BKE_nurbList_verts_count(lb);
  float *co, (*cos)[3] = MEM_malloc_arrayN(numVerts, sizeof(*cos), "cu_vcos");
  Nurb *nu;

  co = cos[0];
  for (nu = lb->first; nu; nu = nu->next) {
    if (nu->type == CU_BEZIER) {
      BezTriple *bezt = nu->bezt;

      for (i = 0; i < nu->pntsu; i++, bezt++) {
        copy_v3_v3(co, &key[0]);
        co += 3;
        copy_v3_v3(co, &key[3]);
        co += 3;
        copy_v3_v3(co, &key[6]);
        co += 3;
        key += KEYELEM_FLOAT_LEN_BEZTRIPLE;
      }
    }
    else {
      BPoint *bp = nu->bp;

      for (i = 0; i < nu->pntsu * nu->pntsv; i++, bp++) {
        copy_v3_v3(co, key);
        co += 3;
        key += KEYELEM_FLOAT_LEN_BPOINT;
      }
    }
  }

  return cos;
}

void BKE_curve_nurbs_keyVertexTilts_apply(ListBase *lb, float *key)
{
  Nurb *nu;
  int i;

  for (nu = lb->first; nu; nu = nu->next) {
    if (nu->type == CU_BEZIER) {
      BezTriple *bezt = nu->bezt;

      for (i = 0; i < nu->pntsu; i++, bezt++) {
        bezt->tilt = key[9];
        bezt->radius = key[10];
        key += KEYELEM_FLOAT_LEN_BEZTRIPLE;
      }
    }
    else {
      BPoint *bp = nu->bp;

      for (i = 0; i < nu->pntsu * nu->pntsv; i++, bp++) {
        bp->tilt = key[3];
        bp->radius = key[4];
        key += KEYELEM_FLOAT_LEN_BPOINT;
      }
    }
  }
}

bool BKE_nurb_check_valid_u(struct Nurb *nu)
{
  if (nu->pntsu <= 1) {
    return false;
  }
  if (nu->type != CU_NURBS) {
    return true; /* not a nurb, lets assume its valid */
  }

  if (nu->pntsu < nu->orderu) {
    return false;
  }
  if (((nu->flagu & CU_NURB_CYCLIC) == 0) &&
      (nu->flagu & CU_NURB_BEZIER)) { /* Bezier U Endpoints */
    if (nu->orderu == 4) {
      if (nu->pntsu < 5) {
        return false; /* bezier with 4 orderu needs 5 points */
      }
    }
    else {
      if (nu->orderu != 3) {
        return false; /* order must be 3 or 4 */
      }
    }
  }
  return true;
}
bool BKE_nurb_check_valid_v(struct Nurb *nu)
{
  if (nu->pntsv <= 1) {
    return false;
  }
  if (nu->type != CU_NURBS) {
    return true; /* not a nurb, lets assume its valid */
  }

  if (nu->pntsv < nu->orderv) {
    return false;
  }
  if (((nu->flagv & CU_NURB_CYCLIC) == 0) &&
      (nu->flagv & CU_NURB_BEZIER)) { /* Bezier V Endpoints */
    if (nu->orderv == 4) {
      if (nu->pntsv < 5) {
        return false; /* bezier with 4 orderu needs 5 points */
      }
    }
    else {
      if (nu->orderv != 3) {
        return false; /* order must be 3 or 4 */
      }
    }
  }
  return true;
}

bool BKE_nurb_check_valid_uv(struct Nurb *nu)
{
  if (!BKE_nurb_check_valid_u(nu)) {
    return false;
  }
  if ((nu->pntsv > 1) && !BKE_nurb_check_valid_v(nu)) {
    return false;
  }

  return true;
}

bool BKE_nurb_order_clamp_u(struct Nurb *nu)
{
  bool changed = false;
  if (nu->pntsu < nu->orderu) {
    nu->orderu = max_ii(2, nu->pntsu);
    changed = true;
  }
  if (((nu->flagu & CU_NURB_CYCLIC) == 0) && (nu->flagu & CU_NURB_BEZIER)) {
    CLAMP(nu->orderu, 3, 4);
    changed = true;
  }
  return changed;
}

bool BKE_nurb_order_clamp_v(struct Nurb *nu)
{
  bool changed = false;
  if (nu->pntsv < nu->orderv) {
    nu->orderv = max_ii(2, nu->pntsv);
    changed = true;
  }
  if (((nu->flagv & CU_NURB_CYCLIC) == 0) && (nu->flagv & CU_NURB_BEZIER)) {
    CLAMP(nu->orderv, 3, 4);
    changed = true;
  }
  return changed;
}

/**
 * \note caller must ensure active vertex remains valid.
 */
bool BKE_nurb_type_convert(Nurb *nu, const short type, const bool use_handles)
{
  BezTriple *bezt;
  BPoint *bp;
  int a, c, nr;

  if (nu->type == CU_POLY) {
    if (type == CU_BEZIER) { /* to Bezier with vecthandles  */
      nr = nu->pntsu;
      bezt = (BezTriple *)MEM_calloc_arrayN(nr, sizeof(BezTriple), "setsplinetype2");
      nu->bezt = bezt;
      a = nr;
      bp = nu->bp;
      while (a--) {
        copy_v3_v3(bezt->vec[1], bp->vec);
        bezt->f1 = bezt->f2 = bezt->f3 = bp->f1;
        bezt->h1 = bezt->h2 = HD_VECT;
        bezt->weight = bp->weight;
        bezt->radius = bp->radius;
        bp++;
        bezt++;
      }
      MEM_freeN(nu->bp);
      nu->bp = NULL;
      nu->pntsu = nr;
      nu->pntsv = 0;
      nu->type = CU_BEZIER;
      BKE_nurb_handles_calc(nu);
    }
    else if (type == CU_NURBS) {
      nu->type = CU_NURBS;
      nu->orderu = 4;
      nu->flagu &= CU_NURB_CYCLIC; /* disable all flags except for cyclic */
      BKE_nurb_knot_calc_u(nu);
      a = nu->pntsu * nu->pntsv;
      bp = nu->bp;
      while (a--) {
        bp->vec[3] = 1.0;
        bp++;
      }
    }
  }
  else if (nu->type == CU_BEZIER) { /* Bezier */
    if (type == CU_POLY || type == CU_NURBS) {
      nr = use_handles ? (3 * nu->pntsu) : nu->pntsu;
      nu->bp = MEM_calloc_arrayN(nr, sizeof(BPoint), "setsplinetype");
      a = nu->pntsu;
      bezt = nu->bezt;
      bp = nu->bp;
      while (a--) {
        if ((type == CU_POLY && bezt->h1 == HD_VECT && bezt->h2 == HD_VECT) ||
            (use_handles == false)) {
          /* vector handle becomes 1 poly vertice */
          copy_v3_v3(bp->vec, bezt->vec[1]);
          bp->vec[3] = 1.0;
          bp->f1 = bezt->f2;
          if (use_handles) {
            nr -= 2;
          }
          bp->radius = bezt->radius;
          bp->weight = bezt->weight;
          bp++;
        }
        else {
          const char *f = &bezt->f1;
          for (c = 0; c < 3; c++, f++) {
            copy_v3_v3(bp->vec, bezt->vec[c]);
            bp->vec[3] = 1.0;
            bp->f1 = *f;
            bp->radius = bezt->radius;
            bp->weight = bezt->weight;
            bp++;
          }
        }
        bezt++;
      }
      MEM_freeN(nu->bezt);
      nu->bezt = NULL;
      nu->pntsu = nr;
      nu->pntsv = 1;
      nu->orderu = 4;
      nu->orderv = 1;
      nu->type = type;

      if (type == CU_NURBS) {
        nu->flagu &= CU_NURB_CYCLIC; /* disable all flags except for cyclic */
        nu->flagu |= CU_NURB_BEZIER;
        BKE_nurb_knot_calc_u(nu);
      }
    }
  }
  else if (nu->type == CU_NURBS) {
    if (type == CU_POLY) {
      nu->type = CU_POLY;
      if (nu->knotsu) {
        MEM_freeN(nu->knotsu); /* python created nurbs have a knotsu of zero */
      }
      nu->knotsu = NULL;
      if (nu->knotsv) {
        MEM_freeN(nu->knotsv);
      }
      nu->knotsv = NULL;
    }
    else if (type == CU_BEZIER) { /* to Bezier */
      nr = nu->pntsu / 3;

      if (nr < 2) {
        return false; /* conversion impossible */
      }
      else {
        bezt = MEM_calloc_arrayN(nr, sizeof(BezTriple), "setsplinetype2");
        nu->bezt = bezt;
        a = nr;
        bp = nu->bp;
        while (a--) {
          copy_v3_v3(bezt->vec[0], bp->vec);
          bezt->f1 = bp->f1;
          bp++;
          copy_v3_v3(bezt->vec[1], bp->vec);
          bezt->f2 = bp->f1;
          bp++;
          copy_v3_v3(bezt->vec[2], bp->vec);
          bezt->f3 = bp->f1;
          bezt->radius = bp->radius;
          bezt->weight = bp->weight;
          bp++;
          bezt++;
        }
        MEM_freeN(nu->bp);
        nu->bp = NULL;
        MEM_freeN(nu->knotsu);
        nu->knotsu = NULL;
        nu->pntsu = nr;
        nu->type = CU_BEZIER;
      }
    }
  }

  return true;
}

/* Get edit nurbs or normal nurbs list */
ListBase *BKE_curve_nurbs_get(Curve *cu)
{
  if (cu->editnurb) {
    return BKE_curve_editNurbs_get(cu);
  }

  return &cu->nurb;
}

void BKE_curve_nurb_active_set(Curve *cu, const Nurb *nu)
{
  if (nu == NULL) {
    cu->actnu = CU_ACT_NONE;
  }
  else {
    BLI_assert(!nu->hide);
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);
    cu->actnu = BLI_findindex(nurbs, nu);
  }
}

Nurb *BKE_curve_nurb_active_get(Curve *cu)
{
  ListBase *nurbs = BKE_curve_editNurbs_get(cu);
  return BLI_findlink(nurbs, cu->actnu);
}

/* Get active vert for curve */
void *BKE_curve_vert_active_get(Curve *cu)
{
  Nurb *nu = NULL;
  void *vert = NULL;

  BKE_curve_nurb_vert_active_get(cu, &nu, &vert);
  return vert;
}

int BKE_curve_nurb_vert_index_get(const Nurb *nu, const void *vert)
{
  if (nu->type == CU_BEZIER) {
    BLI_assert(ARRAY_HAS_ITEM((BezTriple *)vert, nu->bezt, nu->pntsu));
    return (BezTriple *)vert - nu->bezt;
  }
  else {
    BLI_assert(ARRAY_HAS_ITEM((BPoint *)vert, nu->bp, nu->pntsu * nu->pntsv));
    return (BPoint *)vert - nu->bp;
  }
}

/* Set active nurb and active vert for curve */
void BKE_curve_nurb_vert_active_set(Curve *cu, const Nurb *nu, const void *vert)
{
  if (nu) {
    BKE_curve_nurb_active_set(cu, nu);

    if (vert) {
      cu->actvert = BKE_curve_nurb_vert_index_get(nu, vert);
    }
    else {
      cu->actvert = CU_ACT_NONE;
    }
  }
  else {
    cu->actnu = cu->actvert = CU_ACT_NONE;
  }
}

/* Get points to active active nurb and active vert for curve */
bool BKE_curve_nurb_vert_active_get(Curve *cu, Nurb **r_nu, void **r_vert)
{
  Nurb *nu = NULL;
  void *vert = NULL;

  if (cu->actvert != CU_ACT_NONE) {
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);
    nu = BLI_findlink(nurbs, cu->actnu);

    if (nu) {
      if (nu->type == CU_BEZIER) {
        BLI_assert(nu->pntsu > cu->actvert);
        vert = &nu->bezt[cu->actvert];
      }
      else {
        BLI_assert((nu->pntsu * nu->pntsv) > cu->actvert);
        vert = &nu->bp[cu->actvert];
      }
    }
  }

  *r_nu = nu;
  *r_vert = vert;

  return (*r_vert != NULL);
}

void BKE_curve_nurb_vert_active_validate(Curve *cu)
{
  Nurb *nu;
  void *vert;

  if (BKE_curve_nurb_vert_active_get(cu, &nu, &vert)) {
    if (nu->type == CU_BEZIER) {
      BezTriple *bezt = vert;
      if (BEZT_ISSEL_ANY(bezt) == 0) {
        cu->actvert = CU_ACT_NONE;
      }
    }
    else {
      BPoint *bp = vert;
      if ((bp->f1 & SELECT) == 0) {
        cu->actvert = CU_ACT_NONE;
      }
    }

    if (nu->hide) {
      cu->actnu = CU_ACT_NONE;
    }
  }
}

/* basic vertex data functions */
bool BKE_curve_minmax(Curve *cu, bool use_radius, float min[3], float max[3])
{
  ListBase *nurb_lb = BKE_curve_nurbs_get(cu);
  ListBase temp_nurb_lb = {NULL, NULL};
  const bool is_font = (BLI_listbase_is_empty(nurb_lb)) && (cu->len != 0);
  /* For font curves we generate temp list of splines.
   *
   * This is likely to be fine, this function is not supposed to be called
   * often, and it's the only way to get meaningful bounds for fonts.
   */
  if (is_font) {
    nurb_lb = &temp_nurb_lb;
    BKE_vfont_to_curve_ex(NULL, cu, FO_EDIT, nurb_lb, NULL, NULL, NULL, NULL);
    use_radius = false;
  }
  /* Do bounding box based on splines. */
  for (Nurb *nu = nurb_lb->first; nu; nu = nu->next) {
    BKE_nurb_minmax(nu, use_radius, min, max);
  }
  const bool result = (BLI_listbase_is_empty(nurb_lb) == false);
  /* Cleanup if needed. */
  BKE_nurbList_free(&temp_nurb_lb);
  return result;
}

bool BKE_curve_center_median(Curve *cu, float cent[3])
{
  ListBase *nurb_lb = BKE_curve_nurbs_get(cu);
  Nurb *nu;
  int total = 0;

  zero_v3(cent);

  for (nu = nurb_lb->first; nu; nu = nu->next) {
    int i;

    if (nu->type == CU_BEZIER) {
      BezTriple *bezt;
      i = nu->pntsu;
      total += i * 3;
      for (bezt = nu->bezt; i--; bezt++) {
        add_v3_v3(cent, bezt->vec[0]);
        add_v3_v3(cent, bezt->vec[1]);
        add_v3_v3(cent, bezt->vec[2]);
      }
    }
    else {
      BPoint *bp;
      i = nu->pntsu * nu->pntsv;
      total += i;
      for (bp = nu->bp; i--; bp++) {
        add_v3_v3(cent, bp->vec);
      }
    }
  }

  if (total) {
    mul_v3_fl(cent, 1.0f / (float)total);
  }

  return (total != 0);
}

bool BKE_curve_center_bounds(Curve *cu, float cent[3])
{
  float min[3], max[3];
  INIT_MINMAX(min, max);
  if (BKE_curve_minmax(cu, false, min, max)) {
    mid_v3_v3v3(cent, min, max);
    return true;
  }

  return false;
}

void BKE_curve_transform_ex(
    Curve *cu, float mat[4][4], const bool do_keys, const bool do_props, const float unit_scale)
{
  Nurb *nu;
  BPoint *bp;
  BezTriple *bezt;
  int i;

  for (nu = cu->nurb.first; nu; nu = nu->next) {
    if (nu->type == CU_BEZIER) {
      i = nu->pntsu;
      for (bezt = nu->bezt; i--; bezt++) {
        mul_m4_v3(mat, bezt->vec[0]);
        mul_m4_v3(mat, bezt->vec[1]);
        mul_m4_v3(mat, bezt->vec[2]);
        if (do_props) {
          bezt->radius *= unit_scale;
        }
      }
      BKE_nurb_handles_calc(nu);
    }
    else {
      i = nu->pntsu * nu->pntsv;
      for (bp = nu->bp; i--; bp++) {
        mul_m4_v3(mat, bp->vec);
        if (do_props) {
          bp->radius *= unit_scale;
        }
      }
    }
  }

  if (do_keys && cu->key) {
    KeyBlock *kb;
    for (kb = cu->key->block.first; kb; kb = kb->next) {
      float *fp = kb->data;
      int n = kb->totelem;

      for (nu = cu->nurb.first; nu; nu = nu->next) {
        if (nu->type == CU_BEZIER) {
          for (i = nu->pntsu; i && (n -= KEYELEM_ELEM_LEN_BEZTRIPLE) >= 0; i--) {
            mul_m4_v3(mat, &fp[0]);
            mul_m4_v3(mat, &fp[3]);
            mul_m4_v3(mat, &fp[6]);
            if (do_props) {
              fp[10] *= unit_scale; /* radius */
            }
            fp += KEYELEM_FLOAT_LEN_BEZTRIPLE;
          }
        }
        else {
          for (i = nu->pntsu * nu->pntsv; i && (n -= KEYELEM_ELEM_LEN_BPOINT) >= 0; i--) {
            mul_m4_v3(mat, fp);
            if (do_props) {
              fp[4] *= unit_scale; /* radius */
            }
            fp += KEYELEM_FLOAT_LEN_BPOINT;
          }
        }
      }
    }
  }
}

void BKE_curve_transform(Curve *cu, float mat[4][4], const bool do_keys, const bool do_props)
{
  float unit_scale = mat4_to_scale(mat);
  BKE_curve_transform_ex(cu, mat, do_keys, do_props, unit_scale);
}

void BKE_curve_translate(Curve *cu, float offset[3], const bool do_keys)
{
  ListBase *nurb_lb = BKE_curve_nurbs_get(cu);
  Nurb *nu;
  int i;

  for (nu = nurb_lb->first; nu; nu = nu->next) {
    BezTriple *bezt;
    BPoint *bp;

    if (nu->type == CU_BEZIER) {
      i = nu->pntsu;
      for (bezt = nu->bezt; i--; bezt++) {
        add_v3_v3(bezt->vec[0], offset);
        add_v3_v3(bezt->vec[1], offset);
        add_v3_v3(bezt->vec[2], offset);
      }
    }
    else {
      i = nu->pntsu * nu->pntsv;
      for (bp = nu->bp; i--; bp++) {
        add_v3_v3(bp->vec, offset);
      }
    }
  }

  if (do_keys && cu->key) {
    KeyBlock *kb;
    for (kb = cu->key->block.first; kb; kb = kb->next) {
      float *fp = kb->data;
      int n = kb->totelem;

      for (nu = cu->nurb.first; nu; nu = nu->next) {
        if (nu->type == CU_BEZIER) {
          for (i = nu->pntsu; i && (n -= KEYELEM_ELEM_LEN_BEZTRIPLE) >= 0; i--) {
            add_v3_v3(&fp[0], offset);
            add_v3_v3(&fp[3], offset);
            add_v3_v3(&fp[6], offset);
            fp += KEYELEM_FLOAT_LEN_BEZTRIPLE;
          }
        }
        else {
          for (i = nu->pntsu * nu->pntsv; i && (n -= KEYELEM_ELEM_LEN_BPOINT) >= 0; i--) {
            add_v3_v3(fp, offset);
            fp += KEYELEM_FLOAT_LEN_BPOINT;
          }
        }
      }
    }
  }
}

void BKE_curve_material_index_remove(Curve *cu, int index)
{
  const int curvetype = BKE_curve_type_get(cu);

  if (curvetype == OB_FONT) {
    struct CharInfo *info = cu->strinfo;
    int i;
    for (i = cu->len_wchar - 1; i >= 0; i--, info++) {
      if (info->mat_nr && info->mat_nr >= index) {
        info->mat_nr--;
      }
    }
  }
  else {
    Nurb *nu;

    for (nu = cu->nurb.first; nu; nu = nu->next) {
      if (nu->mat_nr && nu->mat_nr >= index) {
        nu->mat_nr--;
      }
    }
  }
}

void BKE_curve_material_index_clear(Curve *cu)
{
  const int curvetype = BKE_curve_type_get(cu);

  if (curvetype == OB_FONT) {
    struct CharInfo *info = cu->strinfo;
    int i;
    for (i = cu->len_wchar - 1; i >= 0; i--, info++) {
      info->mat_nr = 0;
    }
  }
  else {
    Nurb *nu;

    for (nu = cu->nurb.first; nu; nu = nu->next) {
      nu->mat_nr = 0;
    }
  }
}

bool BKE_curve_material_index_validate(Curve *cu)
{
  const int curvetype = BKE_curve_type_get(cu);
  bool is_valid = true;

  if (curvetype == OB_FONT) {
    CharInfo *info = cu->strinfo;
    const int max_idx = max_ii(0, cu->totcol); /* OB_FONT use 1 as first mat index, not 0!!! */
    int i;
    for (i = cu->len_wchar - 1; i >= 0; i--, info++) {
      if (info->mat_nr > max_idx) {
        info->mat_nr = 0;
        is_valid = false;
      }
    }
  }
  else {
    Nurb *nu;
    const int max_idx = max_ii(0, cu->totcol - 1);
    for (nu = cu->nurb.first; nu; nu = nu->next) {
      if (nu->mat_nr > max_idx) {
        nu->mat_nr = 0;
        is_valid = false;
      }
    }
  }

  if (!is_valid) {
    DEG_id_tag_update(&cu->id, ID_RECALC_GEOMETRY);
    return true;
  }
  else {
    return false;
  }
}

void BKE_curve_material_remap(Curve *cu, const unsigned int *remap, unsigned int remap_len)
{
  const int curvetype = BKE_curve_type_get(cu);
  const short remap_len_short = (short)remap_len;

#define MAT_NR_REMAP(n) \
  if (n < remap_len_short) { \
    BLI_assert(n >= 0 && remap[n] < remap_len_short); \
    n = remap[n]; \
  } \
  ((void)0)

  if (curvetype == OB_FONT) {
    struct CharInfo *strinfo;
    int charinfo_len, i;

    if (cu->editfont) {
      EditFont *ef = cu->editfont;
      strinfo = ef->textbufinfo;
      charinfo_len = ef->len;
    }
    else {
      strinfo = cu->strinfo;
      charinfo_len = cu->len_wchar;
    }

    for (i = 0; i <= charinfo_len; i++) {
      if (strinfo[i].mat_nr > 0) {
        strinfo[i].mat_nr -= 1;
        MAT_NR_REMAP(strinfo[i].mat_nr);
        strinfo[i].mat_nr += 1;
      }
    }
  }
  else {
    Nurb *nu;
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);

    if (nurbs) {
      for (nu = nurbs->first; nu; nu = nu->next) {
        MAT_NR_REMAP(nu->mat_nr);
      }
    }
  }

#undef MAT_NR_REMAP
}

void BKE_curve_rect_from_textbox(const struct Curve *cu,
                                 const struct TextBox *tb,
                                 struct rctf *r_rect)
{
  r_rect->xmin = cu->xof + tb->x;
  r_rect->ymax = cu->yof + tb->y + cu->fsize;

  r_rect->xmax = r_rect->xmin + tb->w;
  r_rect->ymin = r_rect->ymax - tb->h;
}

/* **** Depsgraph evaluation **** */

void BKE_curve_eval_geometry(Depsgraph *depsgraph, Curve *curve)
{
  DEG_debug_print_eval(depsgraph, __func__, curve->id.name, curve);
  if (curve->bb == NULL || (curve->bb->flag & BOUNDBOX_DIRTY)) {
    BKE_curve_texspace_calc(curve);
  }
}

/* Draw Engine */
void (*BKE_curve_batch_cache_dirty_tag_cb)(Curve *cu, int mode) = NULL;
void (*BKE_curve_batch_cache_free_cb)(Curve *cu) = NULL;

void BKE_curve_batch_cache_dirty_tag(Curve *cu, int mode)
{
  if (cu->batch_cache) {
    BKE_curve_batch_cache_dirty_tag_cb(cu, mode);
  }
}
void BKE_curve_batch_cache_free(Curve *cu)
{
  if (cu->batch_cache) {
    BKE_curve_batch_cache_free_cb(cu);
  }
}
