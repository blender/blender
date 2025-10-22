/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurve
 */

#include <algorithm>

#include "DNA_anim_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.h"
#include "BLI_ghash.h"
#include "BLI_listbase_wrapper.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_displist.h"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_modifier.hh"
#include "BKE_object_types.hh"
#include "BKE_report.hh"

#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_curve.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_transform.hh"
#include "ED_transform_snap_object_context.hh"
#include "ED_view3d.hh"

#include "curve_intern.hh"

extern "C" {
#include "curve_fit_nd.h"
}

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

using blender::Vector;

void selectend_nurb(Object *obedit, enum eEndPoint_Types selfirst, bool doswap, bool selstatus);
static void adduplicateflagNurb(
    Object *obedit, View3D *v3d, ListBase *newnurb, const uint8_t flag, const bool split);
static bool curve_delete_segments(Object *obedit, View3D *v3d, const bool split);
static bool curve_delete_vertices(Object *obedit, View3D *v3d);

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

ListBase *object_editcurve_get(Object *ob)
{
  if (ob && ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
    Curve *cu = static_cast<Curve *>(ob->data);
    return &cu->editnurb->nurbs;
  }
  return nullptr;
}

KeyBlock *ED_curve_get_edit_shape_key(const Curve *cu)
{
  BLI_assert(cu->editnurb);

  return BKE_keyblock_find_by_index(cu->key, cu->editnurb->shapenr - 1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug Printing
 * \{ */

#if 0
void printknots(Object *obedit)
{
  ListBase *editnurb = object_editcurve_get(obedit);
  Nurb *nu;
  int a, num;

  for (nu = editnurb->first; nu; nu = nu->next) {
    if (ED_curve_nurb_select_check(nu) && nu->type == CU_NURBS) {
      if (nu->knotsu) {
        num = KNOTSU(nu);
        for (a = 0; a < num; a++) {
          printf("knotu %d: %f\n", a, nu->knotsu[a]);
        }
      }
      if (nu->knotsv) {
        num = KNOTSV(nu);
        for (a = 0; a < num; a++) {
          printf("knotv %d: %f\n", a, nu->knotsv[a]);
        }
      }
    }
  }
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape keys
 * \{ */

static CVKeyIndex *init_cvKeyIndex(
    void *cv, int key_index, int nu_index, int pt_index, int vertex_index)
{
  CVKeyIndex *cvIndex = MEM_callocN<CVKeyIndex>(__func__);

  cvIndex->orig_cv = cv;
  cvIndex->key_index = key_index;
  cvIndex->nu_index = nu_index;
  cvIndex->pt_index = pt_index;
  cvIndex->vertex_index = vertex_index;
  cvIndex->switched = false;

  return cvIndex;
}

static void init_editNurb_keyIndex(EditNurb *editnurb, ListBase *origBase)
{
  Nurb *nu = static_cast<Nurb *>(editnurb->nurbs.first);
  Nurb *orignu = static_cast<Nurb *>(origBase->first);
  GHash *gh;
  BezTriple *bezt, *origbezt;
  BPoint *bp, *origbp;
  CVKeyIndex *keyIndex;
  int a, key_index = 0, nu_index = 0, pt_index = 0, vertex_index = 0;

  if (editnurb->keyindex) {
    return;
  }

  gh = BLI_ghash_ptr_new("editNurb keyIndex");

  while (orignu) {
    if (orignu->bezt) {
      a = orignu->pntsu;
      bezt = nu->bezt;
      origbezt = orignu->bezt;
      pt_index = 0;
      while (a--) {
        /* We cannot keep *any* reference to curve obdata,
         * it might be replaced and freed while editcurve remain in use
         * (in viewport render case e.g.). Note that we could use a pool to avoid
         * lots of malloc's here, but... not really a problem for now. */
        BezTriple *origbezt_cpy = static_cast<BezTriple *>(
            MEM_mallocN(sizeof(*origbezt), __func__));
        *origbezt_cpy = *origbezt;
        keyIndex = init_cvKeyIndex(origbezt_cpy, key_index, nu_index, pt_index, vertex_index);
        BLI_ghash_insert(gh, bezt, keyIndex);
        key_index += KEYELEM_FLOAT_LEN_BEZTRIPLE;
        vertex_index += 3;
        bezt++;
        origbezt++;
        pt_index++;
      }
    }
    else {
      a = orignu->pntsu * orignu->pntsv;
      bp = nu->bp;
      origbp = orignu->bp;
      pt_index = 0;
      while (a--) {
        /* We cannot keep *any* reference to curve obdata,
         * it might be replaced and freed while editcurve remain in use
         * (in viewport render case e.g.). Note that we could use a pool to avoid
         * lots of malloc's here, but... not really a problem for now. */
        BPoint *origbp_cpy = MEM_mallocN<BPoint>(__func__);
        *origbp_cpy = *origbp;
        keyIndex = init_cvKeyIndex(origbp_cpy, key_index, nu_index, pt_index, vertex_index);
        BLI_ghash_insert(gh, bp, keyIndex);
        key_index += KEYELEM_FLOAT_LEN_BPOINT;
        bp++;
        origbp++;
        pt_index++;
        vertex_index++;
      }
    }

    nu = nu->next;
    orignu = orignu->next;
    nu_index++;
  }

  editnurb->keyindex = gh;
}

static CVKeyIndex *getCVKeyIndex(EditNurb *editnurb, const void *cv)
{
  return static_cast<CVKeyIndex *>(BLI_ghash_lookup(editnurb->keyindex, cv));
}

static CVKeyIndex *popCVKeyIndex(EditNurb *editnurb, const void *cv)
{
  return static_cast<CVKeyIndex *>(BLI_ghash_popkey(editnurb->keyindex, cv, nullptr));
}

static BezTriple *getKeyIndexOrig_bezt(EditNurb *editnurb, const BezTriple *bezt)
{
  CVKeyIndex *index = getCVKeyIndex(editnurb, bezt);

  if (!index) {
    return nullptr;
  }

  return (BezTriple *)index->orig_cv;
}

static BPoint *getKeyIndexOrig_bp(EditNurb *editnurb, BPoint *bp)
{
  CVKeyIndex *index = getCVKeyIndex(editnurb, bp);

  if (!index) {
    return nullptr;
  }

  return (BPoint *)index->orig_cv;
}

static int getKeyIndexOrig_keyIndex(EditNurb *editnurb, void *cv)
{
  CVKeyIndex *index = getCVKeyIndex(editnurb, cv);

  if (!index) {
    return -1;
  }

  return index->key_index;
}

static void keyIndex_delBezt(EditNurb *editnurb, BezTriple *bezt)
{
  if (!editnurb->keyindex) {
    return;
  }

  BKE_curve_editNurb_keyIndex_delCV(editnurb->keyindex, bezt);
}

static void keyIndex_delBP(EditNurb *editnurb, BPoint *bp)
{
  if (!editnurb->keyindex) {
    return;
  }

  BKE_curve_editNurb_keyIndex_delCV(editnurb->keyindex, bp);
}

static void keyIndex_delNurb(EditNurb *editnurb, Nurb *nu)
{
  int a;

  if (!editnurb->keyindex) {
    return;
  }

  if (nu->bezt) {
    const BezTriple *bezt = nu->bezt;
    a = nu->pntsu;

    while (a--) {
      BKE_curve_editNurb_keyIndex_delCV(editnurb->keyindex, bezt);
      bezt++;
    }
  }
  else {
    const BPoint *bp = nu->bp;
    a = nu->pntsu * nu->pntsv;

    while (a--) {
      BKE_curve_editNurb_keyIndex_delCV(editnurb->keyindex, bp);
      bp++;
    }
  }
}

static void keyIndex_delNurbList(EditNurb *editnurb, ListBase *nubase)
{
  LISTBASE_FOREACH (Nurb *, nu, nubase) {
    keyIndex_delNurb(editnurb, nu);
  }
}

static void keyIndex_updateCV(EditNurb *editnurb, char *cv, char *newcv, int count, int size)
{
  int i;
  CVKeyIndex *index;

  if (editnurb->keyindex == nullptr) {
    /* No shape keys - updating not needed */
    return;
  }

  for (i = 0; i < count; i++) {
    index = popCVKeyIndex(editnurb, cv);

    if (index) {
      BLI_ghash_insert(editnurb->keyindex, newcv, index);
    }

    newcv += size;
    cv += size;
  }
}

static void keyIndex_updateBezt(EditNurb *editnurb, BezTriple *bezt, BezTriple *newbezt, int count)
{
  keyIndex_updateCV(editnurb, (char *)bezt, (char *)newbezt, count, sizeof(BezTriple));
}

static void keyIndex_updateBP(EditNurb *editnurb, BPoint *bp, BPoint *newbp, int count)
{
  keyIndex_updateCV(editnurb, (char *)bp, (char *)newbp, count, sizeof(BPoint));
}

void ED_curve_keyindex_update_nurb(EditNurb *editnurb, Nurb *nu, Nurb *newnu)
{
  if (nu->bezt) {
    keyIndex_updateBezt(editnurb, nu->bezt, newnu->bezt, newnu->pntsu);
  }
  else {
    keyIndex_updateBP(editnurb, nu->bp, newnu->bp, newnu->pntsu * newnu->pntsv);
  }
}

static void keyIndex_swap(EditNurb *editnurb, void *a, void *b)
{
  CVKeyIndex *index1 = popCVKeyIndex(editnurb, a);
  CVKeyIndex *index2 = popCVKeyIndex(editnurb, b);

  if (index2) {
    BLI_ghash_insert(editnurb->keyindex, a, index2);
  }
  if (index1) {
    BLI_ghash_insert(editnurb->keyindex, b, index1);
  }
}

static void keyIndex_switchDirection(EditNurb *editnurb, Nurb *nu)
{
  int a;
  CVKeyIndex *index1, *index2;

  if (nu->bezt) {
    BezTriple *bezt1, *bezt2;

    a = nu->pntsu;

    bezt1 = nu->bezt;
    bezt2 = bezt1 + (a - 1);

    if (a & 1) {
      a++;
    }

    a /= 2;

    while (a--) {
      index1 = getCVKeyIndex(editnurb, bezt1);
      index2 = getCVKeyIndex(editnurb, bezt2);

      if (index1) {
        index1->switched = !index1->switched;
      }

      if (bezt1 != bezt2) {
        keyIndex_swap(editnurb, bezt1, bezt2);

        if (index2) {
          index2->switched = !index2->switched;
        }
      }

      bezt1++;
      bezt2--;
    }
  }
  else {
    BPoint *bp1, *bp2;

    if (nu->pntsv == 1) {
      a = nu->pntsu;
      bp1 = nu->bp;
      bp2 = bp1 + (a - 1);
      a /= 2;
      while (bp1 != bp2 && a > 0) {
        index1 = getCVKeyIndex(editnurb, bp1);
        index2 = getCVKeyIndex(editnurb, bp2);

        if (index1) {
          index1->switched = !index1->switched;
        }

        if (bp1 != bp2) {
          if (index2) {
            index2->switched = !index2->switched;
          }

          keyIndex_swap(editnurb, bp1, bp2);
        }

        a--;
        bp1++;
        bp2--;
      }
    }
    else {
      int b;

      for (b = 0; b < nu->pntsv; b++) {

        bp1 = &nu->bp[b * nu->pntsu];
        a = nu->pntsu;
        bp2 = bp1 + (a - 1);
        a /= 2;

        while (bp1 != bp2 && a > 0) {
          index1 = getCVKeyIndex(editnurb, bp1);
          index2 = getCVKeyIndex(editnurb, bp2);

          if (index1) {
            index1->switched = !index1->switched;
          }

          if (bp1 != bp2) {
            if (index2) {
              index2->switched = !index2->switched;
            }

            keyIndex_swap(editnurb, bp1, bp2);
          }

          a--;
          bp1++;
          bp2--;
        }
      }
    }
  }
}

static void switch_keys_direction(Curve *cu, Nurb *actnu)
{
  EditNurb *editnurb = cu->editnurb;
  ListBase *nubase = &editnurb->nurbs;
  float *fp;
  int a;

  LISTBASE_FOREACH (KeyBlock *, currkey, &cu->key->block) {
    fp = static_cast<float *>(currkey->data);

    LISTBASE_FOREACH (Nurb *, nu, nubase) {
      if (nu->bezt) {
        BezTriple *bezt = nu->bezt;
        a = nu->pntsu;
        if (nu == actnu) {
          while (a--) {
            if (getKeyIndexOrig_bezt(editnurb, bezt)) {
              swap_v3_v3(fp, fp + 6);
              *(fp + 9) = -*(fp + 9);
              fp += KEYELEM_FLOAT_LEN_BEZTRIPLE;
            }
            bezt++;
          }
        }
        else {
          fp += a * KEYELEM_FLOAT_LEN_BEZTRIPLE;
        }
      }
      else {
        BPoint *bp = nu->bp;
        a = nu->pntsu * nu->pntsv;
        if (nu == actnu) {
          while (a--) {
            if (getKeyIndexOrig_bp(editnurb, bp)) {
              *(fp + 3) = -*(fp + 3);
              fp += KEYELEM_FLOAT_LEN_BPOINT;
            }
            bp++;
          }
        }
        else {
          fp += a * KEYELEM_FLOAT_LEN_BPOINT;
        }
      }
    }
  }
}

static void keyData_switchDirectionNurb(Curve *cu, Nurb *nu)
{
  EditNurb *editnurb = cu->editnurb;

  if (!editnurb->keyindex) {
    /* no shape keys - nothing to do */
    return;
  }

  keyIndex_switchDirection(editnurb, nu);
  if (cu->key) {
    switch_keys_direction(cu, nu);
  }
}

GHash *ED_curve_keyindex_hash_duplicate(GHash *keyindex)
{
  GHash *gh;
  GHashIterator gh_iter;

  gh = BLI_ghash_ptr_new_ex("dupli_keyIndex gh", BLI_ghash_len(keyindex));

  GHASH_ITER (gh_iter, keyindex) {
    void *cv = BLI_ghashIterator_getKey(&gh_iter);
    CVKeyIndex *index = static_cast<CVKeyIndex *>(BLI_ghashIterator_getValue(&gh_iter));
    CVKeyIndex *newIndex = MEM_mallocN<CVKeyIndex>("dupli_keyIndexHash index");

    memcpy(newIndex, index, sizeof(CVKeyIndex));
    newIndex->orig_cv = MEM_dupallocN(index->orig_cv);

    BLI_ghash_insert(gh, cv, newIndex);
  }

  return gh;
}

static void key_to_bezt(float *key, BezTriple *basebezt, BezTriple *bezt)
{
  memcpy(bezt, basebezt, sizeof(BezTriple));
  memcpy(bezt->vec, key, sizeof(float[9]));
  bezt->tilt = key[9];
  bezt->radius = key[10];
}

static void bezt_to_key(BezTriple *bezt, float *key)
{
  memcpy(key, bezt->vec, sizeof(float[9]));
  key[9] = bezt->tilt;
  key[10] = bezt->radius;
}

static void calc_keyHandles(ListBase *nurb, float *key)
{
  int a;
  float *fp = key;
  BezTriple *bezt;

  LISTBASE_FOREACH (Nurb *, nu, nurb) {
    if (nu->bezt) {
      BezTriple *prevp, *nextp;
      BezTriple cur, prev, next;
      float *startfp, *prevfp, *nextfp;

      bezt = nu->bezt;
      a = nu->pntsu;
      startfp = fp;

      if (nu->flagu & CU_NURB_CYCLIC) {
        prevp = bezt + (a - 1);
        prevfp = fp + (KEYELEM_FLOAT_LEN_BEZTRIPLE * (a - 1));
      }
      else {
        prevp = nullptr;
        prevfp = nullptr;
      }

      nextp = bezt + 1;
      nextfp = fp + KEYELEM_FLOAT_LEN_BEZTRIPLE;

      while (a--) {
        key_to_bezt(fp, bezt, &cur);

        if (nextp) {
          key_to_bezt(nextfp, nextp, &next);
        }
        if (prevp) {
          key_to_bezt(prevfp, prevp, &prev);
        }

        BKE_nurb_handle_calc(&cur, prevp ? &prev : nullptr, nextp ? &next : nullptr, false, 0);
        bezt_to_key(&cur, fp);

        prevp = bezt;
        prevfp = fp;
        if (a == 1) {
          if (nu->flagu & CU_NURB_CYCLIC) {
            nextp = nu->bezt;
            nextfp = startfp;
          }
          else {
            nextp = nullptr;
            nextfp = nullptr;
          }
        }
        else {
          nextp++;
          nextfp += KEYELEM_FLOAT_LEN_BEZTRIPLE;
        }

        bezt++;
        fp += KEYELEM_FLOAT_LEN_BEZTRIPLE;
      }
    }
    else {
      a = nu->pntsu * nu->pntsv;
      fp += a * KEYELEM_FLOAT_LEN_BPOINT;
    }
  }
}

static void calc_shapeKeys(Object *obedit, ListBase *newnurbs)
{
  Curve *cu = (Curve *)obedit->data;

  if (cu->key == nullptr) {
    return;
  }

  int a, i, currkey_i;
  EditNurb *editnurb = cu->editnurb;
  KeyBlock *actkey = static_cast<KeyBlock *>(BLI_findlink(&cu->key->block, editnurb->shapenr - 1));
  BezTriple *bezt, *oldbezt;
  BPoint *bp, *oldbp;
  Nurb *newnu;
  int totvert = BKE_keyblock_curve_element_count(&editnurb->nurbs);

  float (*ofs)[3] = nullptr;
  std::optional<blender::Array<bool>> dependent;
  const float *oldkey, *ofp;
  float *newkey;

  /* editing the base key should update others */
  if (cu->key->type == KEY_RELATIVE) {
    dependent = BKE_keyblock_get_dependent_keys(cu->key, editnurb->shapenr - 1);

    if (dependent) { /* active key is a base */
      int totvec = 0;

      /* Calculate needed memory to store offset */
      LISTBASE_FOREACH (Nurb *, nu, &editnurb->nurbs) {

        if (nu->bezt) {
          /* Three vectors to store handles and one for tilt. */
          totvec += nu->pntsu * 4;
        }
        else {
          totvec += 2 * nu->pntsu * nu->pntsv;
        }
      }

      ofs = MEM_calloc_arrayN<float[3]>(totvec, "currkey->data");
      i = 0;
      LISTBASE_FOREACH (Nurb *, nu, &editnurb->nurbs) {
        if (nu->bezt) {
          bezt = nu->bezt;
          a = nu->pntsu;
          while (a--) {
            oldbezt = getKeyIndexOrig_bezt(editnurb, bezt);

            if (oldbezt) {
              int j;
              for (j = 0; j < 3; j++) {
                sub_v3_v3v3(ofs[i], bezt->vec[j], oldbezt->vec[j]);
                i++;
              }
              ofs[i][0] = bezt->tilt - oldbezt->tilt;
              ofs[i][1] = bezt->radius - oldbezt->radius;
              i++;
            }
            else {
              i += 4;
            }
            bezt++;
          }
        }
        else {
          bp = nu->bp;
          a = nu->pntsu * nu->pntsv;
          while (a--) {
            oldbp = getKeyIndexOrig_bp(editnurb, bp);
            if (oldbp) {
              sub_v3_v3v3(ofs[i], bp->vec, oldbp->vec);
              ofs[i + 1][0] = bp->tilt - oldbp->tilt;
              ofs[i + 1][1] = bp->radius - oldbp->radius;
            }
            i += 2;
            bp++;
          }
        }
      }
    }
  }

  LISTBASE_FOREACH_INDEX (KeyBlock *, currkey, &cu->key->block, currkey_i) {
    const bool apply_offset = (ofs && (currkey != actkey) && (*dependent)[currkey_i]);

    float *fp = newkey = static_cast<float *>(
        MEM_callocN(cu->key->elemsize * totvert, "currkey->data"));
    ofp = oldkey = static_cast<float *>(currkey->data);

    Nurb *nu = static_cast<Nurb *>(editnurb->nurbs.first);
    /* We need to restore to original curve into newnurb, *not* editcurve's nurbs.
     * Otherwise, in case we update obdata *without* leaving editmode (e.g. viewport render),
     * we would invalidate editcurve. */
    newnu = static_cast<Nurb *>(newnurbs->first);
    i = 0;
    while (nu) {
      if (currkey == actkey) {
        const bool restore = actkey != cu->key->refkey;

        if (nu->bezt) {
          bezt = nu->bezt;
          a = nu->pntsu;
          BezTriple *newbezt = newnu->bezt;
          while (a--) {
            int j;
            oldbezt = getKeyIndexOrig_bezt(editnurb, bezt);

            for (j = 0; j < 3; j++, i++) {
              copy_v3_v3(&fp[j * 3], bezt->vec[j]);

              if (restore && oldbezt) {
                copy_v3_v3(newbezt->vec[j], oldbezt->vec[j]);
              }
            }
            fp[9] = bezt->tilt;
            fp[10] = bezt->radius;

            if (restore && oldbezt) {
              newbezt->tilt = oldbezt->tilt;
              newbezt->radius = oldbezt->radius;
            }

            fp += KEYELEM_FLOAT_LEN_BEZTRIPLE;
            i++;
            bezt++;
            newbezt++;
          }
        }
        else {
          bp = nu->bp;
          a = nu->pntsu * nu->pntsv;
          BPoint *newbp = newnu->bp;
          while (a--) {
            oldbp = getKeyIndexOrig_bp(editnurb, bp);

            copy_v3_v3(fp, bp->vec);

            fp[3] = bp->tilt;
            fp[4] = bp->radius;

            if (restore && oldbp) {
              copy_v3_v3(newbp->vec, oldbp->vec);
              newbp->tilt = oldbp->tilt;
              newbp->radius = oldbp->radius;
            }

            fp += KEYELEM_FLOAT_LEN_BPOINT;
            bp++;
            newbp++;
            i += 2;
          }
        }
      }
      else {
        int index;
        const float *curofp;

        if (oldkey) {
          if (nu->bezt) {
            bezt = nu->bezt;
            a = nu->pntsu;

            while (a--) {
              index = getKeyIndexOrig_keyIndex(editnurb, bezt);
              if (index >= 0) {
                int j;
                curofp = ofp + index;

                for (j = 0; j < 3; j++, i++) {
                  copy_v3_v3(&fp[j * 3], &curofp[j * 3]);

                  if (apply_offset) {
                    add_v3_v3(&fp[j * 3], ofs[i]);
                  }
                }
                fp[9] = curofp[9];
                fp[10] = curofp[10];

                if (apply_offset) {
                  /* Apply tilt offsets. */
                  add_v3_v3(fp + 9, ofs[i]);
                  i++;
                }

                fp += KEYELEM_FLOAT_LEN_BEZTRIPLE;
              }
              else {
                int j;
                for (j = 0; j < 3; j++, i++) {
                  copy_v3_v3(&fp[j * 3], bezt->vec[j]);
                }
                fp[9] = bezt->tilt;
                fp[10] = bezt->radius;

                fp += KEYELEM_FLOAT_LEN_BEZTRIPLE;
              }
              bezt++;
            }
          }
          else {
            bp = nu->bp;
            a = nu->pntsu * nu->pntsv;
            while (a--) {
              index = getKeyIndexOrig_keyIndex(editnurb, bp);

              if (index >= 0) {
                curofp = ofp + index;
                copy_v3_v3(fp, curofp);
                fp[3] = curofp[3];
                fp[4] = curofp[4];

                if (apply_offset) {
                  add_v3_v3(fp, ofs[i]);
                  add_v3_v3(&fp[3], ofs[i + 1]);
                }
              }
              else {
                copy_v3_v3(fp, bp->vec);
                fp[3] = bp->tilt;
                fp[4] = bp->radius;
              }

              fp += KEYELEM_FLOAT_LEN_BPOINT;
              bp++;
              i += 2;
            }
          }
        }
      }

      nu = nu->next;
      newnu = newnu->next;
    }

    if (apply_offset) {
      /* handles could become malicious after offsets applying */
      calc_keyHandles(&editnurb->nurbs, newkey);
    }

    currkey->totelem = totvert;
    if (currkey->data) {
      MEM_freeN(currkey->data);
    }
    currkey->data = newkey;
  }

  MEM_SAFE_FREE(ofs);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Data
 * \{ */

static bool curve_is_animated(Curve *cu)
{
  AnimData *ad = BKE_animdata_from_id(&cu->id);

  return ad && (ad->action || ad->drivers.first);
}

/**
 * Rename F-Curves, but only if they haven't been processed yet.
 */
static void fcurve_path_rename(const char *orig_rna_path,
                               const char *rna_path,
                               const blender::Span<FCurve *> orig_curves,
                               blender::Set<FCurve *> &processed_fcurves)
{
  const int len = strlen(orig_rna_path);

  for (FCurve *fcu : orig_curves) {
    if (processed_fcurves.contains(fcu)) {
      continue;
    }
    if (!STREQLEN(fcu->rna_path, orig_rna_path, len)) {
      continue;
    }

    processed_fcurves.add(fcu);

    const char *suffix = fcu->rna_path + len;
    char *new_rna_path = BLI_sprintfN("%s%s", rna_path, suffix);
    MEM_SAFE_FREE(fcu->rna_path);
    fcu->rna_path = new_rna_path;
  }
}

/**
 * Rename F-Curves to account for changes in the Curve data.
 *
 * \return a vector of F-Curves that should be removed, because they refer to
 * no-longer-existing parts of the curve.
 */
[[nodiscard]] static blender::Vector<FCurve *> curve_rename_fcurves(
    Curve *cu, blender::Span<FCurve *> orig_curves)
{
  if (orig_curves.is_empty()) {
    /* If there is no animation data to operate on, better stop now. */
    return {};
  }

  int a, pt_index;
  EditNurb *editnurb = cu->editnurb;
  CVKeyIndex *keyIndex;
  char rna_path[64], orig_rna_path[64];

  blender::Set<FCurve *> processed_fcurves;
  blender::Vector<FCurve *> fcurves_to_remove;

  int nu_index = 0;
  LISTBASE_FOREACH_INDEX (Nurb *, nu, &editnurb->nurbs, nu_index) {
    if (nu->bezt) {
      BezTriple *bezt = nu->bezt;
      a = nu->pntsu;
      pt_index = 0;

      while (a--) {
        SNPRINTF_UTF8(rna_path, "splines[%d].bezier_points[%d]", nu_index, pt_index);

        keyIndex = getCVKeyIndex(editnurb, bezt);
        if (keyIndex) {
          SNPRINTF_UTF8(orig_rna_path,
                        "splines[%d].bezier_points[%d]",
                        keyIndex->nu_index,
                        keyIndex->pt_index);

          if (keyIndex->switched) {
            char handle_path[64], orig_handle_path[64];
            SNPRINTF_UTF8(orig_handle_path, "%s.handle_left", orig_rna_path);
            SNPRINTF_UTF8(handle_path, "%s.handle_right", rna_path);
            fcurve_path_rename(orig_handle_path, handle_path, orig_curves, processed_fcurves);

            SNPRINTF_UTF8(orig_handle_path, "%s.handle_right", orig_rna_path);
            SNPRINTF_UTF8(handle_path, "%s.handle_left", rna_path);
            fcurve_path_rename(orig_handle_path, handle_path, orig_curves, processed_fcurves);
          }

          fcurve_path_rename(orig_rna_path, rna_path, orig_curves, processed_fcurves);

          keyIndex->nu_index = nu_index;
          keyIndex->pt_index = pt_index;
        }
        else {
          /* In this case, the bezier point exists. It just hasn't been indexed yet (which seems to
           * happen on entering edit mode, so points added after that may not have such an index
           * yet) */

          /* This is a no-op when it comes to the manipulation of F-Curves. It does find the
           * relevant F-Curves to place them in `processed_fcurves`, which will prevent them from
           * being deleted later on. */
          fcurve_path_rename(rna_path, rna_path, orig_curves, processed_fcurves);
        }

        bezt++;
        pt_index++;
      }
    }
    else {
      BPoint *bp = nu->bp;
      a = nu->pntsu * nu->pntsv;
      pt_index = 0;

      while (a--) {
        SNPRINTF_UTF8(rna_path, "splines[%d].points[%d]", nu_index, pt_index);

        keyIndex = getCVKeyIndex(editnurb, bp);
        if (keyIndex) {
          SNPRINTF_UTF8(
              orig_rna_path, "splines[%d].points[%d]", keyIndex->nu_index, keyIndex->pt_index);
          fcurve_path_rename(orig_rna_path, rna_path, orig_curves, processed_fcurves);

          keyIndex->nu_index = nu_index;
          keyIndex->pt_index = pt_index;
        }
        else {
          /* In this case, the bezier point exists. It just hasn't been indexed yet (which seems to
           * happen on entering edit mode, so points added after that may not have such an index
           * yet) */

          /* This is a no-op when it comes to the manipulation of F-Curves. It does find the
           * relevant F-Curves to place them in `processed_fcurves`, which will prevent them from
           * being deleted later on. */
          fcurve_path_rename(rna_path, rna_path, orig_curves, processed_fcurves);
        }

        bp++;
        pt_index++;
      }
    }
  }

  /* remove paths for removed control points
   * need this to make further step with copying non-cv related curves copying
   * not touching cv's f-curves */
  for (FCurve *fcu : orig_curves) {
    if (processed_fcurves.contains(fcu)) {
      continue;
    }

    if (STRPREFIX(fcu->rna_path, "splines")) {
      const char *ch = strchr(fcu->rna_path, '.');

      if (ch && (STRPREFIX(ch, ".bezier_points") || STRPREFIX(ch, ".points"))) {
        fcurves_to_remove.append(fcu);
      }
    }
  }

  nu_index = 0;
  LISTBASE_FOREACH_INDEX (Nurb *, nu, &editnurb->nurbs, nu_index) {
    keyIndex = nullptr;
    if (nu->pntsu) {
      if (nu->bezt) {
        keyIndex = getCVKeyIndex(editnurb, &nu->bezt[0]);
      }
      else {
        keyIndex = getCVKeyIndex(editnurb, &nu->bp[0]);
      }
    }

    if (keyIndex) {
      SNPRINTF_UTF8(rna_path, "splines[%d]", nu_index);
      SNPRINTF_UTF8(orig_rna_path, "splines[%d]", keyIndex->nu_index);
      fcurve_path_rename(orig_rna_path, rna_path, orig_curves, processed_fcurves);
    }
  }

  /* the remainders in orig_curves can be copied back (like follow path) */
  /* (if it's not path to spline) */
  for (FCurve *fcu : orig_curves) {
    if (processed_fcurves.contains(fcu)) {
      continue;
    }
    if (STRPREFIX(fcu->rna_path, "splines")) {
      fcurves_to_remove.append(fcu);
    }
  }

  return fcurves_to_remove;
}

int ED_curve_updateAnimPaths(Main *bmain, Curve *cu)
{
  AnimData *adt = BKE_animdata_from_id(&cu->id);
  EditNurb *editnurb = cu->editnurb;

  if (!editnurb->keyindex) {
    return 0;
  }

  if (!curve_is_animated(cu)) {
    return 0;
  }

  if (adt->action != nullptr) {
    blender::animrig::Action &action = adt->action->wrap();
    const bool is_action_legacy = action.is_action_legacy();

    Vector<FCurve *> fcurves_to_process = blender::animrig::legacy::fcurves_for_assigned_action(
        adt);

    Vector<FCurve *> fcurves_to_remove = curve_rename_fcurves(cu, fcurves_to_process);
    for (FCurve *fcurve : fcurves_to_remove) {
      if (is_action_legacy) {
        action_groups_remove_channel(adt->action, fcurve);
        BKE_fcurve_free(fcurve);
      }
      else {
        const bool remove_ok = blender::animrig::action_fcurve_remove(action, *fcurve);
        BLI_assert(remove_ok);
        UNUSED_VARS_NDEBUG(remove_ok);
      }
    }

    BKE_action_groups_reconstruct(adt->action);
    DEG_id_tag_update(&adt->action->id, ID_RECALC_SYNC_TO_EVAL);
  }

  {
    Vector<FCurve *> fcurves_to_process = blender::listbase_to_vector<FCurve>(adt->drivers);
    Vector<FCurve *> fcurves_to_remove = curve_rename_fcurves(cu, fcurves_to_process);
    for (FCurve *driver : fcurves_to_remove) {
      BLI_remlink(&adt->drivers, driver);
      BKE_fcurve_free(driver);
    }
    DEG_id_tag_update(&cu->id, ID_RECALC_SYNC_TO_EVAL);
  }

  /* TODO(sergey): Only update if something actually changed. */
  DEG_relations_tag_update(bmain);

  return 1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Mode Conversion (Make & Load)
 * \{ */

static int *init_index_map(Object *obedit, int *r_old_totvert)
{
  Curve *curve = (Curve *)obedit->data;
  EditNurb *editnurb = curve->editnurb;
  CVKeyIndex *keyIndex;
  int *old_to_new_map;

  int old_totvert = 0;
  LISTBASE_FOREACH (Nurb *, nu, &curve->nurb) {
    if (nu->bezt) {
      old_totvert += nu->pntsu * 3;
    }
    else {
      old_totvert += nu->pntsu * nu->pntsv;
    }
  }

  old_to_new_map = MEM_malloc_arrayN<int>(old_totvert, "curve old to new index map");
  for (int i = 0; i < old_totvert; i++) {
    old_to_new_map[i] = -1;
  }

  int vertex_index = 0;
  LISTBASE_FOREACH (Nurb *, nu, &editnurb->nurbs) {
    if (nu->bezt) {
      BezTriple *bezt = nu->bezt;
      int a = nu->pntsu;

      while (a--) {
        keyIndex = getCVKeyIndex(editnurb, bezt);
        if (keyIndex && keyIndex->vertex_index + 2 < old_totvert) {
          if (keyIndex->switched) {
            old_to_new_map[keyIndex->vertex_index] = vertex_index + 2;
            old_to_new_map[keyIndex->vertex_index + 1] = vertex_index + 1;
            old_to_new_map[keyIndex->vertex_index + 2] = vertex_index;
          }
          else {
            old_to_new_map[keyIndex->vertex_index] = vertex_index;
            old_to_new_map[keyIndex->vertex_index + 1] = vertex_index + 1;
            old_to_new_map[keyIndex->vertex_index + 2] = vertex_index + 2;
          }
        }
        vertex_index += 3;
        bezt++;
      }
    }
    else {
      BPoint *bp = nu->bp;
      int a = nu->pntsu * nu->pntsv;

      while (a--) {
        keyIndex = getCVKeyIndex(editnurb, bp);
        if (keyIndex) {
          old_to_new_map[keyIndex->vertex_index] = vertex_index;
        }
        vertex_index++;
        bp++;
      }
    }
  }

  *r_old_totvert = old_totvert;
  return old_to_new_map;
}

static void remap_hooks_and_vertex_parents(Main *bmain, Object *obedit)
{
  Curve *curve = (Curve *)obedit->data;
  EditNurb *editnurb = curve->editnurb;
  int *old_to_new_map = nullptr;
  int old_totvert;

  if (editnurb->keyindex == nullptr) {
    /* TODO(sergey): Happens when separating curves, this would lead to
     * the wrong indices in the hook modifier, address this together with
     * other indices issues.
     */
    return;
  }

  LISTBASE_FOREACH (Object *, object, &bmain->objects) {
    int index;
    if ((object->parent) && (object->parent->data == curve) &&
        ELEM(object->partype, PARVERT1, PARVERT3))
    {
      if (old_to_new_map == nullptr) {
        old_to_new_map = init_index_map(obedit, &old_totvert);
      }

      if (object->par1 < old_totvert) {
        index = old_to_new_map[object->par1];
        if (index != -1) {
          object->par1 = index;
        }
      }
      if (object->par2 < old_totvert) {
        index = old_to_new_map[object->par2];
        if (index != -1) {
          object->par2 = index;
        }
      }
      if (object->par3 < old_totvert) {
        index = old_to_new_map[object->par3];
        if (index != -1) {
          object->par3 = index;
        }
      }
    }
    if (object->data == curve) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type == eModifierType_Hook) {
          HookModifierData *hmd = (HookModifierData *)md;
          int i, j;

          if (old_to_new_map == nullptr) {
            old_to_new_map = init_index_map(obedit, &old_totvert);
          }

          for (i = j = 0; i < hmd->indexar_num; i++) {
            if (hmd->indexar[i] < old_totvert) {
              index = old_to_new_map[hmd->indexar[i]];
              if (index != -1) {
                hmd->indexar[j++] = index;
              }
            }
            else {
              j++;
            }
          }

          hmd->indexar_num = j;
        }
      }
    }
  }
  if (old_to_new_map != nullptr) {
    MEM_freeN(old_to_new_map);
  }
}

void ED_curve_editnurb_load(Main *bmain, Object *obedit)
{
  ListBase *editnurb = object_editcurve_get(obedit);

  if (obedit == nullptr) {
    return;
  }

  if (ELEM(obedit->type, OB_CURVES_LEGACY, OB_SURF)) {
    Curve *cu = static_cast<Curve *>(obedit->data);
    ListBase newnurb = {nullptr, nullptr}, oldnurb = cu->nurb;

    remap_hooks_and_vertex_parents(bmain, obedit);

    LISTBASE_FOREACH (Nurb *, nu, editnurb) {
      Nurb *newnu = BKE_nurb_duplicate(nu);
      BLI_addtail(&newnurb, newnu);

      if (nu->type == CU_NURBS) {
        BKE_nurb_order_clamp_u(nu);
      }
    }

    /* We have to pass also new copied nurbs, since we want to restore original curve
     * (without edited shape-key) on obdata, but *not* on editcurve itself
     * (ED_curve_editnurb_load call does not always implies freeing
     * of editcurve, e.g. when called to generate render data). */
    calc_shapeKeys(obedit, &newnurb);

    cu->nurb = newnurb;

    ED_curve_updateAnimPaths(bmain, static_cast<Curve *>(obedit->data));

    BKE_nurbList_free(&oldnurb);
  }
}

void ED_curve_editnurb_make(Object *obedit)
{
  Curve *cu = (Curve *)obedit->data;
  EditNurb *editnurb = cu->editnurb;
  KeyBlock *actkey;

  if (ELEM(obedit->type, OB_CURVES_LEGACY, OB_SURF)) {
    actkey = BKE_keyblock_from_object(obedit);

    if (actkey) {
/* TODO(@ideasman42): undo_system: investigate why this was needed. */
#if 0
      undo_editmode_clear();
#endif
    }

    if (editnurb) {
      BKE_nurbList_free(&editnurb->nurbs);
      BKE_curve_editNurb_keyIndex_free(&editnurb->keyindex);
    }
    else {
      editnurb = MEM_callocN<EditNurb>("editnurb");
      cu->editnurb = editnurb;
    }

    LISTBASE_FOREACH (Nurb *, nu, &cu->nurb) {
      Nurb *newnu = BKE_nurb_duplicate(nu);
      BLI_addtail(&editnurb->nurbs, newnu);
    }

    /* Animation could be added in edit-mode even if there was no animdata in
     * object mode hence we always need CVs index be created. */
    init_editNurb_keyIndex(editnurb, &cu->nurb);

    if (actkey) {
      editnurb->shapenr = obedit->shapenr;
      /* Apply shape-key to new nurbs of editnurb, not those of original curve
       * (and *after* we generated keyIndex), else we do not have valid 'original' data
       * to properly restore curve when leaving edit-mode. */
      BKE_keyblock_convert_to_curve(actkey, cu, &editnurb->nurbs);
    }
  }
}

void ED_curve_editnurb_free(Object *obedit)
{
  Curve *cu = static_cast<Curve *>(obedit->data);

  BKE_curve_editNurb_free(cu);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Separate Operator
 * \{ */

static wmOperatorStatus separate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  struct {
    int changed;
    int unselected;
    int error_vertex_keys;
    int error_generic;
  } status = {0};

  WM_cursor_wait(true);

  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Base *oldbase : bases) {
    Base *newbase;
    Object *oldob, *newob;
    Curve *oldcu, *newcu;
    EditNurb *newedit;
    ListBase newnurb = {nullptr, nullptr};

    oldob = oldbase->object;
    oldcu = static_cast<Curve *>(oldob->data);

    if (oldcu->key) {
      status.error_vertex_keys++;
      continue;
    }

    if (!ED_curve_select_check(v3d, oldcu->editnurb)) {
      status.unselected++;
      continue;
    }

    /* 1. Duplicate geometry and check for valid selection for separate. */
    adduplicateflagNurb(oldob, v3d, &newnurb, SELECT, true);

    if (BLI_listbase_is_empty(&newnurb)) {
      status.error_generic++;
      continue;
    }

    /* 2. Duplicate the object and data. */

    /* Take into account user preferences for duplicating actions. */
    const eDupli_ID_Flags dupflag = eDupli_ID_Flags(U.dupflag & USER_DUP_ACT);

    newbase = blender::ed::object::add_duplicate(bmain, scene, view_layer, oldbase, dupflag);
    DEG_relations_tag_update(bmain);

    newob = newbase->object;
    newcu = static_cast<Curve *>(newob->data = BKE_id_copy(bmain, &oldcu->id));
    newcu->editnurb = nullptr;
    id_us_min(&oldcu->id); /* Because new curve is a copy: reduce user count. */

    /* 3. Put new object in editmode, clear it and set separated nurbs. */
    ED_curve_editnurb_make(newob);
    newedit = newcu->editnurb;
    BKE_nurbList_free(&newedit->nurbs);
    BKE_curve_editNurb_keyIndex_free(&newedit->keyindex);
    BLI_movelisttolist(&newedit->nurbs, &newnurb);

    /* 4. Put old object out of editmode and delete separated geometry. */
    ED_curve_editnurb_load(bmain, newob);
    ED_curve_editnurb_free(newob);
    curve_delete_segments(oldob, v3d, true);

    DEG_id_tag_update(&oldob->id, ID_RECALC_GEOMETRY); /* This is the original one. */
    DEG_id_tag_update(&newob->id, ID_RECALC_GEOMETRY); /* This is the separated one. */

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, oldob->data);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, newob);
    status.changed++;
  }
  WM_cursor_wait(false);

  if (status.unselected == bases.size()) {
    BKE_report(op->reports, RPT_ERROR, "No point was selected");
    return OPERATOR_CANCELLED;
  }

  const int tot_errors = status.error_vertex_keys + status.error_generic;
  if (tot_errors > 0) {

    /* Some curves changed, but some curves failed: don't explain why it failed. */
    if (status.changed) {
      BKE_reportf(op->reports, RPT_INFO, "%d curve(s) could not be separated", tot_errors);
      return OPERATOR_FINISHED;
    }

    /* All curves failed: If there is more than one error give a generic error report. */
    if (((status.error_vertex_keys ? 1 : 0) + (status.error_generic ? 1 : 0)) > 1) {
      BKE_report(op->reports, RPT_ERROR, "Could not separate selected curve(s)");
    }

    /* All curves failed due to the same error. */
    if (status.error_vertex_keys) {
      BKE_report(op->reports, RPT_ERROR, "Cannot separate curves with shape keys");
    }
    else {
      BLI_assert(status.error_generic);
      BKE_report(op->reports, RPT_ERROR, "Cannot separate current selection");
    }
    return OPERATOR_CANCELLED;
  }

  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

void CURVE_OT_separate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Separate";
  ot->idname = "CURVE_OT_separate";
  ot->description = "Separate selected points from connected unselected points into a new object";

  /* API callbacks. */
  ot->exec = separate_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Split Operator
 * \{ */

static wmOperatorStatus curve_split_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  bool changed = false;
  int count_failed = 0;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Curve *cu = static_cast<Curve *>(obedit->data);

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    ListBase newnurb = {nullptr, nullptr};

    adduplicateflagNurb(obedit, v3d, &newnurb, SELECT, true);

    if (BLI_listbase_is_empty(&newnurb)) {
      count_failed += 1;
      continue;
    }

    ListBase *editnurb = object_editcurve_get(obedit);
    const int len_orig = BLI_listbase_count(editnurb);

    curve_delete_segments(obedit, v3d, true);
    cu->actnu -= len_orig - BLI_listbase_count(editnurb);
    BLI_movelisttolist(editnurb, &newnurb);

    if (ED_curve_updateAnimPaths(bmain, static_cast<Curve *>(obedit->data))) {
      WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);
    }

    changed = true;
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }

  if (changed == false) {
    if (count_failed != 0) {
      BKE_report(op->reports, RPT_ERROR, "Cannot split current selection");
    }
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

void CURVE_OT_split(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Split";
  ot->idname = "CURVE_OT_split";
  ot->description = "Split off selected points from connected unselected points";

  /* API callbacks. */
  ot->exec = curve_split_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flag Utility Functions
 * \{ */

/* return true if U direction is selected and number of selected columns v */
static bool isNurbselU(Nurb *nu, int *v, int flag)
{
  BPoint *bp;
  int a, b, sel;

  *v = 0;

  for (b = 0, bp = nu->bp; b < nu->pntsv; b++) {
    sel = 0;
    for (a = 0; a < nu->pntsu; a++, bp++) {
      if (bp->f1 & flag) {
        sel++;
      }
    }
    if (sel == nu->pntsu) {
      (*v)++;
    }
    else if (sel >= 1) {
      *v = 0;
      return false;
    }
  }

  return true;
}

/* return true if V direction is selected and number of selected rows u */
static bool isNurbselV(Nurb *nu, int *u, int flag)
{
  BPoint *bp;
  int a, b, sel;

  *u = 0;

  for (a = 0; a < nu->pntsu; a++) {
    bp = &nu->bp[a];
    sel = 0;
    for (b = 0; b < nu->pntsv; b++, bp += nu->pntsu) {
      if (bp->f1 & flag) {
        sel++;
      }
    }
    if (sel == nu->pntsv) {
      (*u)++;
    }
    else if (sel >= 1) {
      *u = 0;
      return false;
    }
  }

  return true;
}

static void rotateflagNurb(ListBase *editnurb,
                           short flag,
                           const float cent[3],
                           const float rotmat[3][3])
{
  /* all verts with (flag & 'flag') rotate */
  BPoint *bp;
  int a;

  LISTBASE_FOREACH (Nurb *, nu, editnurb) {
    if (nu->type == CU_NURBS) {
      bp = nu->bp;
      a = nu->pntsu * nu->pntsv;

      while (a--) {
        if (bp->f1 & flag) {
          sub_v3_v3(bp->vec, cent);
          mul_m3_v3(rotmat, bp->vec);
          add_v3_v3(bp->vec, cent);
        }
        bp++;
      }
    }
  }
}

void ed_editnurb_translate_flag(ListBase *editnurb, uint8_t flag, const float vec[3], bool is_2d)
{
  /* all verts with ('flag' & flag) translate */
  BezTriple *bezt;
  BPoint *bp;
  int a;

  LISTBASE_FOREACH (Nurb *, nu, editnurb) {
    if (nu->type == CU_BEZIER) {
      a = nu->pntsu;
      bezt = nu->bezt;
      while (a--) {
        if (bezt->f1 & flag) {
          add_v3_v3(bezt->vec[0], vec);
        }
        if (bezt->f2 & flag) {
          add_v3_v3(bezt->vec[1], vec);
        }
        if (bezt->f3 & flag) {
          add_v3_v3(bezt->vec[2], vec);
        }
        bezt++;
      }
    }
    else {
      a = nu->pntsu * nu->pntsv;
      bp = nu->bp;
      while (a--) {
        if (bp->f1 & flag) {
          add_v3_v3(bp->vec, vec);
        }
        bp++;
      }
    }

    if (is_2d) {
      BKE_nurb_project_2d(nu);
    }
  }
}

static void weightflagNurb(ListBase *editnurb, short flag, float w)
{
  BPoint *bp;
  int a;

  LISTBASE_FOREACH (Nurb *, nu, editnurb) {
    if (nu->type == CU_NURBS) {
      a = nu->pntsu * nu->pntsv;
      bp = nu->bp;
      while (a--) {
        if (bp->f1 & flag) {
          /* a mode used to exist for replace/multiple but is was unused */
          bp->vec[3] *= w;
        }
        bp++;
      }
    }
  }
}

static void ed_surf_delete_selected(Object *obedit)
{
  Curve *cu = static_cast<Curve *>(obedit->data);
  ListBase *editnurb = object_editcurve_get(obedit);
  BPoint *bp, *bpn, *newbp;
  int a, b, newu, newv;

  BLI_assert(obedit->type == OB_SURF);

  LISTBASE_FOREACH_MUTABLE (Nurb *, nu, editnurb) {
    /* is entire nurb selected */
    bp = nu->bp;
    a = nu->pntsu * nu->pntsv;
    while (a) {
      a--;
      if (bp->f1 & SELECT) {
        /* pass */
      }
      else {
        break;
      }
      bp++;
    }
    if (a == 0) {
      if (cu->actnu == BLI_findindex(editnurb, nu)) {
        cu->actnu = CU_ACT_NONE;
      }

      BLI_remlink(editnurb, nu);
      keyIndex_delNurb(cu->editnurb, nu);
      BKE_nurb_free(nu);
      nu = nullptr;
    }
    else {
      if (isNurbselU(nu, &newv, SELECT)) {
        /* U direction selected */
        newv = nu->pntsv - newv;
        if (newv != nu->pntsv) {
          /* delete */
          bp = nu->bp;
          bpn = newbp = MEM_malloc_arrayN<BPoint>(newv * nu->pntsu, "deleteNurb");
          for (b = 0; b < nu->pntsv; b++) {
            if ((bp->f1 & SELECT) == 0) {
              memcpy(bpn, bp, nu->pntsu * sizeof(BPoint));
              keyIndex_updateBP(cu->editnurb, bp, bpn, nu->pntsu);
              bpn += nu->pntsu;
            }
            else {
              keyIndex_delBP(cu->editnurb, bp);
            }
            bp += nu->pntsu;
          }
          nu->pntsv = newv;
          MEM_freeN(nu->bp);
          nu->bp = newbp;
          BKE_nurb_order_clamp_v(nu);

          BKE_nurb_knot_calc_v(nu);
        }
      }
      else if (isNurbselV(nu, &newu, SELECT)) {
        /* V direction selected */
        newu = nu->pntsu - newu;
        if (newu != nu->pntsu) {
          /* delete */
          bp = nu->bp;
          bpn = newbp = MEM_malloc_arrayN<BPoint>(newu * nu->pntsv, "deleteNurb");
          for (b = 0; b < nu->pntsv; b++) {
            for (a = 0; a < nu->pntsu; a++, bp++) {
              if ((bp->f1 & SELECT) == 0) {
                *bpn = *bp;
                keyIndex_updateBP(cu->editnurb, bp, bpn, 1);
                bpn++;
              }
              else {
                keyIndex_delBP(cu->editnurb, bp);
              }
            }
          }
          MEM_freeN(nu->bp);
          nu->bp = newbp;
          if (newu == 1 && nu->pntsv > 1) { /* make a U spline */
            nu->pntsu = nu->pntsv;
            nu->pntsv = 1;
            std::swap(nu->orderu, nu->orderv);
            BKE_nurb_order_clamp_u(nu);
            MEM_SAFE_FREE(nu->knotsv);
          }
          else {
            nu->pntsu = newu;
            BKE_nurb_order_clamp_u(nu);
          }
          BKE_nurb_knot_calc_u(nu);
        }
      }
    }
  }
}

static void ed_curve_delete_selected(Object *obedit, View3D *v3d)
{
  Curve *cu = static_cast<Curve *>(obedit->data);
  EditNurb *editnurb = cu->editnurb;
  ListBase *nubase = &editnurb->nurbs;
  BezTriple *bezt, *bezt1;
  BPoint *bp, *bp1;
  int a, type, nuindex = 0;

  /* first loop, can we remove entire pieces? */
  LISTBASE_FOREACH_MUTABLE (Nurb *, nu, nubase) {
    if (nu->type == CU_BEZIER) {
      bezt = nu->bezt;
      a = nu->pntsu;
      if (a) {
        while (a) {
          if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
            /* pass */
          }
          else {
            break;
          }
          a--;
          bezt++;
        }
        if (a == 0) {
          if (cu->actnu == nuindex) {
            cu->actnu = CU_ACT_NONE;
          }

          BLI_remlink(nubase, nu);
          keyIndex_delNurb(editnurb, nu);
          BKE_nurb_free(nu);
          nu = nullptr;
        }
      }
    }
    else {
      bp = nu->bp;
      a = nu->pntsu * nu->pntsv;
      if (a) {
        while (a) {
          if (bp->f1 & SELECT) {
            /* pass */
          }
          else {
            break;
          }
          a--;
          bp++;
        }
        if (a == 0) {
          if (cu->actnu == nuindex) {
            cu->actnu = CU_ACT_NONE;
          }

          BLI_remlink(nubase, nu);
          keyIndex_delNurb(editnurb, nu);
          BKE_nurb_free(nu);
          nu = nullptr;
        }
      }
    }

/* Never allow the order to exceed the number of points
 * NOTE: this is ok but changes unselected nurbs, disable for now. */
#if 0
    if ((nu != nullptr) && (nu->type == CU_NURBS)) {
      clamp_nurb_order_u(nu);
    }
#endif
    nuindex++;
  }
  /* 2nd loop, delete small pieces: just for curves */
  LISTBASE_FOREACH_MUTABLE (Nurb *, nu, nubase) {
    type = 0;
    if (nu->type == CU_BEZIER) {
      bezt = nu->bezt;
      for (a = 0; a < nu->pntsu; a++) {
        if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
          memmove(bezt, bezt + 1, (nu->pntsu - a - 1) * sizeof(BezTriple));
          keyIndex_delBezt(editnurb, bezt);
          keyIndex_updateBezt(editnurb, bezt + 1, bezt, nu->pntsu - a - 1);
          nu->pntsu--;
          a--;
          type = 1;
        }
        else {
          bezt++;
        }
      }
      if (type) {
        bezt1 = MEM_malloc_arrayN<BezTriple>((nu->pntsu), "delNurb");
        memcpy(bezt1, nu->bezt, (nu->pntsu) * sizeof(BezTriple));
        keyIndex_updateBezt(editnurb, nu->bezt, bezt1, nu->pntsu);
        MEM_freeN(nu->bezt);
        nu->bezt = bezt1;
        BKE_nurb_handles_calc(nu);
      }
    }
    else if (nu->pntsv == 1) {
      bp = nu->bp;

      for (a = 0; a < nu->pntsu; a++) {
        if (bp->f1 & SELECT) {
          memmove(bp, bp + 1, (nu->pntsu - a - 1) * sizeof(BPoint));
          keyIndex_delBP(editnurb, bp);
          keyIndex_updateBP(editnurb, bp + 1, bp, nu->pntsu - a - 1);
          nu->pntsu--;
          a--;
          type = 1;
        }
        else {
          bp++;
        }
      }
      if (type) {
        bp1 = MEM_malloc_arrayN<BPoint>(nu->pntsu, "delNurb2");
        memcpy(bp1, nu->bp, (nu->pntsu) * sizeof(BPoint));
        keyIndex_updateBP(editnurb, nu->bp, bp1, nu->pntsu);
        MEM_freeN(nu->bp);
        nu->bp = bp1;

/* Never allow the order to exceed the number of points
 * NOTE: this is ok but changes unselected nurbs, disable for now. */
#if 0
        if (nu->type == CU_NURBS) {
          clamp_nurb_order_u(nu);
        }
#endif
      }
      BKE_nurb_order_clamp_u(nu);
      BKE_nurb_knot_calc_u(nu);
    }
  }
}

static void select_bpoints(BPoint *bp,
                           const int stride,
                           const int count,
                           const bool selstatus,
                           const uint8_t flag,
                           const bool hidden)
{
  for (int i = 0; i < count; i++) {
    select_bpoint(bp, selstatus, flag, hidden);
    bp += stride;
  }
}

/**
 * Calculate and return fully selected legs along i dimension.
 * Calculates intervals to create extrusion by duplicating existing points while copied to
 * destination NURBS. For ex. for curve of 3 points indexed by 0..2  to extrude first and last
 * point copy intervals would be [0, 0][0, 2][2, 2]. Representation in copy_intervals array would
 * be [0, 0, 2, 2]. Returns -1 if selection is not valid.
 */
static int sel_to_copy_ints(const BPoint *bp,
                            const int next_j,
                            const int max_j,
                            const int next_i,
                            const int max_i,
                            const uint8_t flag,
                            int copy_intervals[],
                            int *interval_count,
                            bool *out_is_first_sel)
{
  const BPoint *bp_j = bp;

  int selected_leg_count = 0;
  int ins = 0;
  int selected_in_prev_leg = -1;
  int not_full = -1;

  bool is_first_sel = false;
  bool is_last_sel = false;

  for (int j = 0; j < max_j; j++, bp_j += next_j) {
    const BPoint *bp_j_i = bp_j;
    int selected_in_curr_leg = 0;
    for (int i = 0; i < max_i; i++, bp_j_i += next_i) {
      if (bp_j_i->f1 & flag) {
        selected_in_curr_leg++;
      }
    }
    if (selected_in_curr_leg == max_i) {
      selected_leg_count++;
      if (j == 0) {
        is_first_sel = true;
      }
      else if (j + 1 == max_j) {
        is_last_sel = true;
      }
    }
    else if (not_full == -1) {
      not_full = selected_in_curr_leg;
    }
    /* We have partially selected leg in opposite dimension if condition is met. */
    else if (not_full != selected_in_curr_leg) {
      return -1;
    }
    /* Extrusion area starts/ends if met. */
    if (selected_in_prev_leg != selected_in_curr_leg) {
      copy_intervals[ins] = selected_in_curr_leg == max_i || j == 0 ? j : j - 1;
      ins++;
      selected_in_prev_leg = selected_in_curr_leg;
    }
    copy_intervals[ins] = j;
  }
  if (selected_leg_count &&
      /* Prevents leading and trailing unselected legs if all selected.
       * Unless it is extrusion from point or curve. */
      (selected_leg_count < max_j || max_j == 1))
  {
    /* Prepend unselected leg if more than one leg selected at the starting edge.
     * max_j == 1 handles extrusion from point to curve and from curve to surface cases. */
    if (is_first_sel && (copy_intervals[0] < copy_intervals[1] || max_j == 1)) {
      memmove(copy_intervals + 1, copy_intervals, (ins + 1) * sizeof(copy_intervals[0]));
      copy_intervals[0] = 0;
      ins++;
      is_first_sel = false;
    }
    /* Append unselected leg if more than one leg selected at the end. */
    if (is_last_sel && copy_intervals[ins - 1] < copy_intervals[ins]) {
      copy_intervals[ins + 1] = copy_intervals[ins];
      ins++;
    }
  }
  *interval_count = ins;
  *out_is_first_sel = ins > 1 ? is_first_sel : false;
  return selected_leg_count;
}

struct NurbDim {
  int pntsu;
  int pntsv;
};

static NurbDim editnurb_find_max_points_num(const EditNurb *editnurb)
{
  NurbDim ret = {0, 0};
  LISTBASE_FOREACH (Nurb *, nu, &editnurb->nurbs) {
    ret.pntsu = std::max(nu->pntsu, ret.pntsu);
    ret.pntsv = std::max(nu->pntsv, ret.pntsv);
  }
  return ret;
}

bool ed_editnurb_extrude_flag(EditNurb *editnurb, const uint8_t flag)
{
  const NurbDim max = editnurb_find_max_points_num(editnurb);
  /* One point induces at most one interval. Except single point case, it can give + 1.
   * Another +1 is for first element of the first interval. */
  int *const intvls_u = MEM_malloc_arrayN<int>(max.pntsu + 2, "extrudeNurb0");
  int *const intvls_v = MEM_malloc_arrayN<int>(max.pntsv + 2, "extrudeNurb1");
  bool ok = false;

  LISTBASE_FOREACH (Nurb *, nu, &editnurb->nurbs) {
    int intvl_cnt_u;
    bool is_first_sel_u;

    /* Calculate selected U legs and intervals for their extrusion. */
    const int selected_us = sel_to_copy_ints(
        nu->bp, 1, nu->pntsu, nu->pntsu, nu->pntsv, flag, intvls_u, &intvl_cnt_u, &is_first_sel_u);
    if (selected_us == -1) {
      continue;
    }
    int intvl_cnt_v;
    bool is_first_sel_v;

    const bool is_point = nu->pntsu == 1;
    const bool is_curve = nu->pntsv == 1;
    const bool extrude_every_u_point = selected_us == nu->pntsu;
    if (is_point || (is_curve && !extrude_every_u_point)) {
      intvls_v[0] = intvls_v[1] = 0;
      intvl_cnt_v = 1;
      is_first_sel_v = false;
    }
    else {
      sel_to_copy_ints(nu->bp,
                       nu->pntsu,
                       nu->pntsv,
                       1,
                       nu->pntsu,
                       flag,
                       intvls_v,
                       &intvl_cnt_v,
                       &is_first_sel_v);
    }

    const int new_pntsu = nu->pntsu + intvl_cnt_u - 1;
    const int new_pntsv = nu->pntsv + intvl_cnt_v - 1;
    BPoint *const new_bp = MEM_malloc_arrayN<BPoint>(new_pntsu * new_pntsv, "extrudeNurb2");
    BPoint *new_bp_v = new_bp;

    bool selected_v = is_first_sel_v;
    for (int j = 1; j <= intvl_cnt_v; j++, selected_v = !selected_v) {
      BPoint *old_bp_v = nu->bp + intvls_v[j - 1] * nu->pntsu;
      for (int v_j = intvls_v[j - 1]; v_j <= intvls_v[j];
           v_j++, new_bp_v += new_pntsu, old_bp_v += nu->pntsu)
      {
        BPoint *new_bp_u_v = new_bp_v;
        bool selected_u = is_first_sel_u;
        for (int i = 1; i <= intvl_cnt_u; i++, selected_u = !selected_u) {
          const int copy_from = intvls_u[i - 1];
          const int copy_to = intvls_u[i];
          const int copy_count = copy_to - copy_from + 1;
          const bool sel_status = selected_u || selected_v ? true : false;
          ED_curve_bpcpy(editnurb, new_bp_u_v, old_bp_v + copy_from, copy_count);
          select_bpoints(new_bp_u_v, 1, copy_count, sel_status, flag, HIDDEN);
          new_bp_u_v += copy_count;
        }
      }
    }

    MEM_freeN(nu->bp);
    nu->bp = new_bp;
    nu->pntsu = new_pntsu;
    if (nu->pntsv == 1 && new_pntsv > 1) {
      nu->orderv = 2;
    }
    nu->pntsv = new_pntsv;
    BKE_nurb_knot_calc_u(nu);
    BKE_nurb_knot_calc_v(nu);

    ok = true;
  }
  MEM_freeN(intvls_u);
  MEM_freeN(intvls_v);
  return ok;
}

static void calc_duplicate_actnurb(const ListBase *editnurb, const ListBase *newnurb, Curve *cu)
{
  cu->actnu = BLI_listbase_count(editnurb) + BLI_listbase_count(newnurb);
}

static bool calc_duplicate_actvert(
    const ListBase *editnurb, const ListBase *newnurb, Curve *cu, int start, int end, int vert)
{
  if (cu->actvert == -1) {
    calc_duplicate_actnurb(editnurb, newnurb, cu);
    return true;
  }

  if ((start <= cu->actvert) && (end > cu->actvert)) {
    calc_duplicate_actnurb(editnurb, newnurb, cu);
    cu->actvert = vert;
    return true;
  }
  return false;
}

static void adduplicateflagNurb(
    Object *obedit, View3D *v3d, ListBase *newnurb, const uint8_t flag, const bool split)
{
  ListBase *editnurb = object_editcurve_get(obedit);
  Nurb *newnu;
  BezTriple *bezt, *bezt1;
  BPoint *bp, *bp1, *bp2, *bp3;
  Curve *cu = (Curve *)obedit->data;
  int a, b, c, starta, enda, diffa, cyclicu, cyclicv, newu, newv;
  char *usel;

  int i = 0;
  LISTBASE_FOREACH_INDEX (Nurb *, nu, editnurb, i) {
    cyclicu = cyclicv = 0;
    if (nu->type == CU_BEZIER) {
      for (a = 0, bezt = nu->bezt; a < nu->pntsu; a++, bezt++) {
        enda = -1;
        starta = a;
        while ((bezt->f1 & flag) || (bezt->f2 & flag) || (bezt->f3 & flag)) {
          if (!split) {
            select_beztriple(bezt, false, flag, HIDDEN);
          }
          enda = a;
          if (a >= nu->pntsu - 1) {
            break;
          }
          a++;
          bezt++;
        }
        if (enda >= starta) {
          newu = diffa = enda - starta + 1; /* set newu and diffa, may use both */

          if (starta == 0 && newu != nu->pntsu && (nu->flagu & CU_NURB_CYCLIC)) {
            cyclicu = newu;
          }
          else {
            if (enda == nu->pntsu - 1) {
              newu += cyclicu;
            }
            if (i == cu->actnu) {
              calc_duplicate_actvert(
                  editnurb, newnurb, cu, starta, starta + diffa, cu->actvert - starta);
            }

            newnu = BKE_nurb_copy(nu, newu, 1);
            memcpy(newnu->bezt, &nu->bezt[starta], diffa * sizeof(BezTriple));
            if (newu != diffa) {
              memcpy(&newnu->bezt[diffa], nu->bezt, cyclicu * sizeof(BezTriple));
              if (i == cu->actnu) {
                calc_duplicate_actvert(
                    editnurb, newnurb, cu, 0, cyclicu, newu - cyclicu + cu->actvert);
              }
              cyclicu = 0;
            }

            if (newu != nu->pntsu) {
              newnu->flagu &= ~CU_NURB_CYCLIC;
            }

            for (b = 0, bezt1 = newnu->bezt; b < newnu->pntsu; b++, bezt1++) {
              select_beztriple(bezt1, true, flag, HIDDEN);
            }

            BLI_addtail(newnurb, newnu);
          }
        }
      }

      if (cyclicu != 0) {
        if (i == cu->actnu) {
          calc_duplicate_actvert(editnurb, newnurb, cu, 0, cyclicu, cu->actvert);
        }

        newnu = BKE_nurb_copy(nu, cyclicu, 1);
        memcpy(newnu->bezt, nu->bezt, cyclicu * sizeof(BezTriple));
        newnu->flagu &= ~CU_NURB_CYCLIC;

        for (b = 0, bezt1 = newnu->bezt; b < newnu->pntsu; b++, bezt1++) {
          select_beztriple(bezt1, true, flag, HIDDEN);
        }

        BLI_addtail(newnurb, newnu);
      }
    }
    else if (nu->pntsv == 1) { /* because UV Nurb has a different method for dupli */
      for (a = 0, bp = nu->bp; a < nu->pntsu; a++, bp++) {
        enda = -1;
        starta = a;
        while (bp->f1 & flag) {
          if (!split) {
            select_bpoint(bp, false, flag, HIDDEN);
          }
          enda = a;
          if (a >= nu->pntsu - 1) {
            break;
          }
          a++;
          bp++;
        }
        if (enda >= starta) {
          newu = diffa = enda - starta + 1; /* set newu and diffa, may use both */

          if (starta == 0 && newu != nu->pntsu && (nu->flagu & CU_NURB_CYCLIC)) {
            cyclicu = newu;
          }
          else {
            if (enda == nu->pntsu - 1) {
              newu += cyclicu;
            }
            if (i == cu->actnu) {
              calc_duplicate_actvert(
                  editnurb, newnurb, cu, starta, starta + diffa, cu->actvert - starta);
            }

            newnu = BKE_nurb_copy(nu, newu, 1);
            memcpy(newnu->bp, &nu->bp[starta], diffa * sizeof(BPoint));
            if (newu != diffa) {
              memcpy(&newnu->bp[diffa], nu->bp, cyclicu * sizeof(BPoint));
              if (i == cu->actnu) {
                calc_duplicate_actvert(
                    editnurb, newnurb, cu, 0, cyclicu, newu - cyclicu + cu->actvert);
              }
              cyclicu = 0;
            }

            if (newu != nu->pntsu) {
              newnu->flagu &= ~CU_NURB_CYCLIC;
            }

            for (b = 0, bp1 = newnu->bp; b < newnu->pntsu; b++, bp1++) {
              select_bpoint(bp1, true, flag, HIDDEN);
            }

            BLI_addtail(newnurb, newnu);
          }
        }
      }

      if (cyclicu != 0) {
        if (i == cu->actnu) {
          calc_duplicate_actvert(editnurb, newnurb, cu, 0, cyclicu, cu->actvert);
        }

        newnu = BKE_nurb_copy(nu, cyclicu, 1);
        memcpy(newnu->bp, nu->bp, cyclicu * sizeof(BPoint));
        newnu->flagu &= ~CU_NURB_CYCLIC;

        for (b = 0, bp1 = newnu->bp; b < newnu->pntsu; b++, bp1++) {
          select_bpoint(bp1, true, flag, HIDDEN);
        }

        BLI_addtail(newnurb, newnu);
      }
    }
    else {
      if (ED_curve_nurb_select_check(v3d, nu)) {
        /* A rectangular area in nurb has to be selected and if splitting
         * must be in U or V direction. */
        usel = MEM_calloc_arrayN<char>(nu->pntsu, "adduplicateN3");
        bp = nu->bp;
        for (a = 0; a < nu->pntsv; a++) {
          for (b = 0; b < nu->pntsu; b++, bp++) {
            if (bp->f1 & flag) {
              usel[b]++;
            }
          }
        }
        newu = 0;
        newv = 0;
        for (a = 0; a < nu->pntsu; a++) {
          if (usel[a]) {
            if (ELEM(newv, 0, usel[a])) {
              newv = usel[a];
              newu++;
            }
            else {
              newv = 0;
              break;
            }
          }
        }
        MEM_freeN(usel);

        if ((newu == 0 || newv == 0) ||
            (split && !isNurbselU(nu, &newv, SELECT) && !isNurbselV(nu, &newu, SELECT)))
        {
          if (G.debug & G_DEBUG) {
            printf("Can't duplicate Nurb\n");
          }
        }
        else {
          for (a = 0, bp1 = nu->bp; a < nu->pntsu * nu->pntsv; a++, bp1++) {
            newv = newu = 0;

            if ((bp1->f1 & flag) && !(bp1->f1 & SURF_SEEN)) {
              /* point selected, now loop over points in U and V directions */
              for (b = a % nu->pntsu, bp2 = bp1; b < nu->pntsu; b++, bp2++) {
                if (bp2->f1 & flag) {
                  newu++;
                  for (c = a / nu->pntsu, bp3 = bp2; c < nu->pntsv; c++, bp3 += nu->pntsu) {
                    if (bp3->f1 & flag) {
                      /* flag as seen so skipped on future iterations */
                      bp3->f1 |= SURF_SEEN;
                      if (newu == 1) {
                        newv++;
                      }
                    }
                    else {
                      break;
                    }
                  }
                }
                else {
                  break;
                }
              }
            }

            if ((newu + newv) > 2) {
              /* ignore single points */
              if (a == 0) {
                /* check if need to save cyclic selection and continue if so */
                if (newu == nu->pntsu && (nu->flagv & CU_NURB_CYCLIC)) {
                  cyclicv = newv;
                }
                if (newv == nu->pntsv && (nu->flagu & CU_NURB_CYCLIC)) {
                  cyclicu = newu;
                }
                if (cyclicu != 0 || cyclicv != 0) {
                  continue;
                }
              }

              if (a + newu == nu->pntsu && cyclicu != 0) {
                /* cyclic in U direction */
                newnu = BKE_nurb_copy(nu, newu + cyclicu, newv);
                for (b = 0; b < newv; b++) {
                  memcpy(&newnu->bp[b * newnu->pntsu],
                         &nu->bp[b * nu->pntsu + a],
                         newu * sizeof(BPoint));
                  memcpy(&newnu->bp[b * newnu->pntsu + newu],
                         &nu->bp[b * nu->pntsu],
                         cyclicu * sizeof(BPoint));
                }

                if (cu->actnu == i) {
                  if (cu->actvert == -1) {
                    calc_duplicate_actnurb(editnurb, newnurb, cu);
                  }
                  else {
                    for (b = 0, diffa = 0; b < newv; b++, diffa += nu->pntsu - newu) {
                      starta = b * nu->pntsu + a;
                      if (calc_duplicate_actvert(editnurb,
                                                 newnurb,
                                                 cu,
                                                 cu->actvert,
                                                 starta,
                                                 cu->actvert % nu->pntsu + newu +
                                                     b * newnu->pntsu))
                      {
                        /* actvert in cyclicu selection */
                        break;
                      }
                      if (calc_duplicate_actvert(editnurb,
                                                 newnurb,
                                                 cu,
                                                 starta,
                                                 starta + newu,
                                                 cu->actvert - starta + b * newnu->pntsu))
                      {
                        /* actvert in 'current' iteration selection */
                        break;
                      }
                    }
                  }
                }
                cyclicu = cyclicv = 0;
              }
              else if ((a / nu->pntsu) + newv == nu->pntsv && cyclicv != 0) {
                /* cyclic in V direction */
                newnu = BKE_nurb_copy(nu, newu, newv + cyclicv);
                memcpy(newnu->bp, &nu->bp[a], newu * newv * sizeof(BPoint));
                memcpy(&newnu->bp[newu * newv], nu->bp, newu * cyclicv * sizeof(BPoint));

                /* check for actvert in cyclicv selection */
                if (cu->actnu == i) {
                  calc_duplicate_actvert(
                      editnurb, newnurb, cu, cu->actvert, a, (newu * newv) + cu->actvert);
                }
                cyclicu = cyclicv = 0;
              }
              else {
                newnu = BKE_nurb_copy(nu, newu, newv);
                for (b = 0; b < newv; b++) {
                  memcpy(&newnu->bp[b * newu], &nu->bp[b * nu->pntsu + a], newu * sizeof(BPoint));
                }
              }

              /* general case if not handled by cyclicu or cyclicv */
              if (cu->actnu == i) {
                if (cu->actvert == -1) {
                  calc_duplicate_actnurb(editnurb, newnurb, cu);
                }
                else {
                  for (b = 0, diffa = 0; b < newv; b++, diffa += nu->pntsu - newu) {
                    starta = b * nu->pntsu + a;
                    if (calc_duplicate_actvert(editnurb,
                                               newnurb,
                                               cu,
                                               starta,
                                               starta + newu,
                                               cu->actvert - (a / nu->pntsu * nu->pntsu + diffa +
                                                              (starta % nu->pntsu))))
                    {
                      break;
                    }
                  }
                }
              }
              BLI_addtail(newnurb, newnu);

              if (newu != nu->pntsu) {
                newnu->flagu &= ~CU_NURB_CYCLIC;
              }
              if (newv != nu->pntsv) {
                newnu->flagv &= ~CU_NURB_CYCLIC;
              }
            }
          }

          if (cyclicu != 0 || cyclicv != 0) {
            /* copy start of a cyclic surface, or copying all selected points */
            newu = cyclicu == 0 ? nu->pntsu : cyclicu;
            newv = cyclicv == 0 ? nu->pntsv : cyclicv;

            newnu = BKE_nurb_copy(nu, newu, newv);
            for (b = 0; b < newv; b++) {
              memcpy(&newnu->bp[b * newu], &nu->bp[b * nu->pntsu], newu * sizeof(BPoint));
            }

            /* Check for `actvert` in the unused cyclic-UV selection. */
            if (cu->actnu == i) {
              if (cu->actvert == -1) {
                calc_duplicate_actnurb(editnurb, newnurb, cu);
              }
              else {
                for (b = 0, diffa = 0; b < newv; b++, diffa += nu->pntsu - newu) {
                  starta = b * nu->pntsu;
                  if (calc_duplicate_actvert(editnurb,
                                             newnurb,
                                             cu,
                                             starta,
                                             starta + newu,
                                             cu->actvert - (diffa + (starta % nu->pntsu))))
                  {
                    break;
                  }
                }
              }
            }
            BLI_addtail(newnurb, newnu);

            if (newu != nu->pntsu) {
              newnu->flagu &= ~CU_NURB_CYCLIC;
            }
            if (newv != nu->pntsv) {
              newnu->flagv &= ~CU_NURB_CYCLIC;
            }
          }

          for (b = 0, bp1 = nu->bp; b < nu->pntsu * nu->pntsv; b++, bp1++) {
            bp1->f1 &= ~SURF_SEEN;
            if (!split) {
              select_bpoint(bp1, false, flag, HIDDEN);
            }
          }
        }
      }
    }
  }

  if (BLI_listbase_is_empty(newnurb) == false) {
    LISTBASE_FOREACH (Nurb *, nu, newnurb) {
      if (nu->type == CU_BEZIER) {
        if (split) {
          /* recalc first and last */
          BKE_nurb_handle_calc_simple(nu, &nu->bezt[0]);
          BKE_nurb_handle_calc_simple(nu, &nu->bezt[nu->pntsu - 1]);
        }
      }
      else {
        /* knots done after duplicate as pntsu may change */
        BKE_nurb_order_clamp_u(nu);
        BKE_nurb_knot_calc_u(nu);

        if (obedit->type == OB_SURF) {
          for (a = 0, bp = nu->bp; a < nu->pntsu * nu->pntsv; a++, bp++) {
            bp->f1 &= ~SURF_SEEN;
          }

          BKE_nurb_order_clamp_v(nu);
          BKE_nurb_knot_calc_v(nu);
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Switch Direction Operator
 * \{ */

static wmOperatorStatus switch_direction_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Curve *cu = static_cast<Curve *>(obedit->data);

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    EditNurb *editnurb = cu->editnurb;

    int i = 0;
    LISTBASE_FOREACH_INDEX (Nurb *, nu, &editnurb->nurbs, i) {
      if (ED_curve_nurb_select_check(v3d, nu)) {
        BKE_nurb_direction_switch(nu);
        keyData_switchDirectionNurb(cu, nu);
        if ((i == cu->actnu) && (cu->actvert != CU_ACT_NONE)) {
          cu->actvert = (nu->pntsu - 1) - cu->actvert;
        }
      }
    }

    if (ED_curve_updateAnimPaths(bmain, static_cast<Curve *>(obedit->data))) {
      WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);
    }

    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }
  return OPERATOR_FINISHED;
}

void CURVE_OT_switch_direction(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Switch Direction";
  ot->description = "Switch direction of selected splines";
  ot->idname = "CURVE_OT_switch_direction";

  /* API callbacks. */
  ot->exec = switch_direction_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Weight Operator
 * \{ */

static wmOperatorStatus set_goal_weight_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    ListBase *editnurb = object_editcurve_get(obedit);
    BezTriple *bezt;
    BPoint *bp;
    float weight = RNA_float_get(op->ptr, "weight");
    int a;

    LISTBASE_FOREACH (Nurb *, nu, editnurb) {
      if (nu->bezt) {
        for (bezt = nu->bezt, a = 0; a < nu->pntsu; a++, bezt++) {
          if (bezt->f2 & SELECT) {
            bezt->weight = weight;
          }
        }
      }
      else if (nu->bp) {
        for (bp = nu->bp, a = 0; a < nu->pntsu * nu->pntsv; a++, bp++) {
          if (bp->f1 & SELECT) {
            bp->weight = weight;
          }
        }
      }
    }

    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void CURVE_OT_spline_weight_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Goal Weight";
  ot->description = "Set softbody goal weight for selected points";
  ot->idname = "CURVE_OT_spline_weight_set";

  /* API callbacks. */
  ot->exec = set_goal_weight_exec;
  ot->invoke = WM_operator_props_popup;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float_factor(ot->srna, "weight", 1.0f, 0.0f, 1.0f, "Weight", "", 0.0f, 1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Radius Operator
 * \{ */

static wmOperatorStatus set_radius_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  int totobjects = 0;

  for (Object *obedit : objects) {

    if (blender::ed::object::shape_key_report_if_locked(obedit, op->reports)) {
      continue;
    }

    totobjects++;

    ListBase *editnurb = object_editcurve_get(obedit);
    BezTriple *bezt;
    BPoint *bp;
    float radius = RNA_float_get(op->ptr, "radius");
    int a;

    LISTBASE_FOREACH (Nurb *, nu, editnurb) {
      if (nu->bezt) {
        for (bezt = nu->bezt, a = 0; a < nu->pntsu; a++, bezt++) {
          if (bezt->f2 & SELECT) {
            bezt->radius = radius;
          }
        }
      }
      else if (nu->bp) {
        for (bp = nu->bp, a = 0; a < nu->pntsu * nu->pntsv; a++, bp++) {
          if (bp->f1 & SELECT) {
            bp->radius = radius;
          }
        }
      }
    }

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }

  return totobjects ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void CURVE_OT_radius_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Curve Radius";
  ot->description = "Set per-point radius which is used for bevel tapering";
  ot->idname = "CURVE_OT_radius_set";

  /* API callbacks. */
  ot->exec = set_radius_exec;
  ot->invoke = WM_operator_props_popup;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float(
      ot->srna, "radius", 1.0f, 0.0f, OBJECT_ADD_SIZE_MAXF, "Radius", "", 0.0001f, 10.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smooth Vertices Operator
 * \{ */

static void smooth_single_bezt(BezTriple *bezt,
                               const BezTriple *bezt_orig_prev,
                               const BezTriple *bezt_orig_next,
                               float factor)
{
  BLI_assert(IN_RANGE_INCL(factor, 0.0f, 1.0f));

  for (int i = 0; i < 3; i++) {
    /* get single dimension pos of the mid handle */
    float val_old = bezt->vec[1][i];

    /* get the weights of the previous/next mid handles and calc offset */
    float val_new = (bezt_orig_prev->vec[1][i] * 0.5f) + (bezt_orig_next->vec[1][i] * 0.5f);
    float offset = (val_old * (1.0f - factor)) + (val_new * factor) - val_old;

    /* offset midpoint and 2 handles */
    bezt->vec[1][i] += offset;
    bezt->vec[0][i] += offset;
    bezt->vec[2][i] += offset;
  }
}

/**
 * Same as #smooth_single_bezt(), keep in sync.
 */
static void smooth_single_bp(BPoint *bp,
                             const BPoint *bp_orig_prev,
                             const BPoint *bp_orig_next,
                             float factor)
{
  BLI_assert(IN_RANGE_INCL(factor, 0.0f, 1.0f));

  for (int i = 0; i < 3; i++) {
    float val_old, val_new, offset;

    val_old = bp->vec[i];
    val_new = (bp_orig_prev->vec[i] * 0.5f) + (bp_orig_next->vec[i] * 0.5f);
    offset = (val_old * (1.0f - factor)) + (val_new * factor) - val_old;

    bp->vec[i] += offset;
  }
}

static wmOperatorStatus smooth_exec(bContext *C, wmOperator *op)
{
  const float factor = 1.0f / 6.0f;
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  int totobjects = 0;

  for (Object *obedit : objects) {

    if (blender::ed::object::shape_key_report_if_locked(obedit, op->reports)) {
      continue;
    }

    totobjects++;

    ListBase *editnurb = object_editcurve_get(obedit);

    int a, a_end;

    LISTBASE_FOREACH (Nurb *, nu, editnurb) {
      if (nu->bezt) {
        /* duplicate the curve to use in weight calculation */
        const BezTriple *bezt_orig = static_cast<const BezTriple *>(MEM_dupallocN(nu->bezt));
        BezTriple *bezt;
        bool changed = false;

        /* check whether its cyclic or not, and set initial & final conditions */
        if (nu->flagu & CU_NURB_CYCLIC) {
          a = 0;
          a_end = nu->pntsu;
        }
        else {
          a = 1;
          a_end = nu->pntsu - 1;
        }

        /* for all the curve points */
        for (; a < a_end; a++) {
          /* respect selection */
          bezt = &nu->bezt[a];
          if (bezt->f2 & SELECT) {
            const BezTriple *bezt_orig_prev, *bezt_orig_next;

            bezt_orig_prev = &bezt_orig[mod_i(a - 1, nu->pntsu)];
            bezt_orig_next = &bezt_orig[mod_i(a + 1, nu->pntsu)];

            smooth_single_bezt(bezt, bezt_orig_prev, bezt_orig_next, factor);

            changed = true;
          }
        }
        MEM_freeN(bezt_orig);
        if (changed) {
          BKE_nurb_handles_calc(nu);
        }
      }
      else if (nu->bp) {
        /* Same as above, keep these the same! */
        const BPoint *bp_orig = static_cast<const BPoint *>(MEM_dupallocN(nu->bp));
        BPoint *bp;

        if (nu->flagu & CU_NURB_CYCLIC) {
          a = 0;
          a_end = nu->pntsu;
        }
        else {
          a = 1;
          a_end = nu->pntsu - 1;
        }

        for (; a < a_end; a++) {
          bp = &nu->bp[a];
          if (bp->f1 & SELECT) {
            const BPoint *bp_orig_prev, *bp_orig_next;

            bp_orig_prev = &bp_orig[mod_i(a - 1, nu->pntsu)];
            bp_orig_next = &bp_orig[mod_i(a + 1, nu->pntsu)];

            smooth_single_bp(bp, bp_orig_prev, bp_orig_next, factor);
          }
        }
        MEM_freeN(bp_orig);
      }
    }

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }

  return totobjects ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void CURVE_OT_smooth(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Smooth";
  ot->description = "Flatten angles of selected points";
  ot->idname = "CURVE_OT_smooth";

  /* API callbacks. */
  ot->exec = smooth_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smooth Operator (Radius/Weight/Tilt) Utilities
 *
 * To do:
 * - Make smoothing distance based.
 * - Support cyclic curves.
 * \{ */

static void curve_smooth_value(ListBase *editnurb, const int bezt_offsetof, const int bp_offset)
{
  BezTriple *bezt;
  BPoint *bp;
  int a;

  /* use for smoothing */
  int last_sel;
  int start_sel, end_sel; /* selection indices, inclusive */
  float start_rad, end_rad, fac, range;

  LISTBASE_FOREACH (Nurb *, nu, editnurb) {
    if (nu->bezt) {
#define BEZT_VALUE(bezt) (*((float *)((char *)(bezt) + bezt_offsetof)))

      for (last_sel = 0; last_sel < nu->pntsu; last_sel++) {
        /* loop over selection segments of a curve, smooth each */

        /* Start BezTriple code,
         * this is duplicated below for points, make sure these functions stay in sync */
        start_sel = -1;
        for (bezt = &nu->bezt[last_sel], a = last_sel; a < nu->pntsu; a++, bezt++) {
          if (bezt->f2 & SELECT) {
            start_sel = a;
            break;
          }
        }
        /* in case there are no other selected verts */
        end_sel = start_sel;
        for (bezt = &nu->bezt[start_sel + 1], a = start_sel + 1; a < nu->pntsu; a++, bezt++) {
          if ((bezt->f2 & SELECT) == 0) {
            break;
          }
          end_sel = a;
        }

        if (start_sel == -1) {
          last_sel = nu->pntsu; /* next... */
        }
        else {
          last_sel = end_sel; /* before we modify it */

          /* now blend between start and end sel */
          start_rad = end_rad = FLT_MAX;

          if (start_sel == end_sel) {
            /* simple, only 1 point selected */
            if (start_sel > 0) {
              start_rad = BEZT_VALUE(&nu->bezt[start_sel - 1]);
            }
            if (end_sel != -1 && end_sel < nu->pntsu) {
              end_rad = BEZT_VALUE(&nu->bezt[start_sel + 1]);
            }

            if (start_rad != FLT_MAX && end_rad >= FLT_MAX) {
              BEZT_VALUE(&nu->bezt[start_sel]) = (start_rad + end_rad) / 2.0f;
            }
            else if (start_rad != FLT_MAX) {
              BEZT_VALUE(&nu->bezt[start_sel]) = start_rad;
            }
            else if (end_rad != FLT_MAX) {
              BEZT_VALUE(&nu->bezt[start_sel]) = end_rad;
            }
          }
          else {
            /* if endpoints selected, then use them */
            if (start_sel == 0) {
              start_rad = BEZT_VALUE(&nu->bezt[start_sel]);
              start_sel++; /* we don't want to edit the selected endpoint */
            }
            else {
              start_rad = BEZT_VALUE(&nu->bezt[start_sel - 1]);
            }
            if (end_sel == nu->pntsu - 1) {
              end_rad = BEZT_VALUE(&nu->bezt[end_sel]);
              end_sel--; /* we don't want to edit the selected endpoint */
            }
            else {
              end_rad = BEZT_VALUE(&nu->bezt[end_sel + 1]);
            }

            /* Now Blend between the points */
            range = float(end_sel - start_sel) + 2.0f;
            for (bezt = &nu->bezt[start_sel], a = start_sel; a <= end_sel; a++, bezt++) {
              fac = float(1 + a - start_sel) / range;
              BEZT_VALUE(bezt) = start_rad * (1.0f - fac) + end_rad * fac;
            }
          }
        }
      }
#undef BEZT_VALUE
    }
    else if (nu->bp) {
#define BP_VALUE(bp) (*((float *)((char *)(bp) + bp_offset)))

      /* Same as above, keep these the same! */
      for (last_sel = 0; last_sel < nu->pntsu; last_sel++) {
        /* loop over selection segments of a curve, smooth each */

        /* Start BezTriple code,
         * this is duplicated below for points, make sure these functions stay in sync */
        start_sel = -1;
        for (bp = &nu->bp[last_sel], a = last_sel; a < nu->pntsu; a++, bp++) {
          if (bp->f1 & SELECT) {
            start_sel = a;
            break;
          }
        }
        /* in case there are no other selected verts */
        end_sel = start_sel;
        for (bp = &nu->bp[start_sel + 1], a = start_sel + 1; a < nu->pntsu; a++, bp++) {
          if ((bp->f1 & SELECT) == 0) {
            break;
          }
          end_sel = a;
        }

        if (start_sel == -1) {
          last_sel = nu->pntsu; /* next... */
        }
        else {
          last_sel = end_sel; /* before we modify it */

          /* now blend between start and end sel */
          start_rad = end_rad = FLT_MAX;

          if (start_sel == end_sel) {
            /* simple, only 1 point selected */
            if (start_sel > 0) {
              start_rad = BP_VALUE(&nu->bp[start_sel - 1]);
            }
            if (end_sel != -1 && end_sel < nu->pntsu) {
              end_rad = BP_VALUE(&nu->bp[start_sel + 1]);
            }

            if (start_rad != FLT_MAX && end_rad != FLT_MAX) {
              BP_VALUE(&nu->bp[start_sel]) = (start_rad + end_rad) / 2;
            }
            else if (start_rad != FLT_MAX) {
              BP_VALUE(&nu->bp[start_sel]) = start_rad;
            }
            else if (end_rad != FLT_MAX) {
              BP_VALUE(&nu->bp[start_sel]) = end_rad;
            }
          }
          else {
            /* if endpoints selected, then use them */
            if (start_sel == 0) {
              start_rad = BP_VALUE(&nu->bp[start_sel]);
              start_sel++; /* we don't want to edit the selected endpoint */
            }
            else {
              start_rad = BP_VALUE(&nu->bp[start_sel - 1]);
            }
            if (end_sel == nu->pntsu - 1) {
              end_rad = BP_VALUE(&nu->bp[end_sel]);
              end_sel--; /* we don't want to edit the selected endpoint */
            }
            else {
              end_rad = BP_VALUE(&nu->bp[end_sel + 1]);
            }

            /* Now Blend between the points */
            range = float(end_sel - start_sel) + 2.0f;
            for (bp = &nu->bp[start_sel], a = start_sel; a <= end_sel; a++, bp++) {
              fac = float(1 + a - start_sel) / range;
              BP_VALUE(bp) = start_rad * (1.0f - fac) + end_rad * fac;
            }
          }
        }
      }
#undef BP_VALUE
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smooth Weight Operator
 * \{ */

static wmOperatorStatus curve_smooth_weight_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    ListBase *editnurb = object_editcurve_get(obedit);

    curve_smooth_value(editnurb, offsetof(BezTriple, weight), offsetof(BPoint, weight));

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }

  return OPERATOR_FINISHED;
}

void CURVE_OT_smooth_weight(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Smooth Curve Weight";
  ot->description = "Interpolate weight of selected points";
  ot->idname = "CURVE_OT_smooth_weight";

  /* API callbacks. */
  ot->exec = curve_smooth_weight_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smooth Radius Operator
 * \{ */

static wmOperatorStatus curve_smooth_radius_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  int totobjects = 0;

  for (Object *obedit : objects) {

    if (blender::ed::object::shape_key_report_if_locked(obedit, op->reports)) {
      continue;
    }

    totobjects++;

    ListBase *editnurb = object_editcurve_get(obedit);

    curve_smooth_value(editnurb, offsetof(BezTriple, radius), offsetof(BPoint, radius));

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }

  return totobjects ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void CURVE_OT_smooth_radius(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Smooth Curve Radius";
  ot->description = "Interpolate radii of selected points";
  ot->idname = "CURVE_OT_smooth_radius";

  /* API callbacks. */
  ot->exec = curve_smooth_radius_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smooth Tilt Operator
 * \{ */

static wmOperatorStatus curve_smooth_tilt_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  int totobjects = 0;

  for (Object *obedit : objects) {

    if (blender::ed::object::shape_key_report_if_locked(obedit, op->reports)) {
      continue;
    }

    totobjects++;

    ListBase *editnurb = object_editcurve_get(obedit);

    curve_smooth_value(editnurb, offsetof(BezTriple, tilt), offsetof(BPoint, tilt));

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }

  return totobjects ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void CURVE_OT_smooth_tilt(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Smooth Curve Tilt";
  ot->description = "Interpolate tilt of selected points";
  ot->idname = "CURVE_OT_smooth_tilt";

  /* API callbacks. */
  ot->exec = curve_smooth_tilt_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Operator
 * \{ */

static wmOperatorStatus hide_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  const bool invert = RNA_boolean_get(op->ptr, "unselected");

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Curve *cu = static_cast<Curve *>(obedit->data);

    if (!(invert || ED_curve_select_check(v3d, cu->editnurb))) {
      continue;
    }

    ListBase *editnurb = object_editcurve_get(obedit);
    BPoint *bp;
    BezTriple *bezt;
    int a, sel;

    LISTBASE_FOREACH (Nurb *, nu, editnurb) {
      if (nu->type == CU_BEZIER) {
        bezt = nu->bezt;
        a = nu->pntsu;
        sel = 0;
        while (a--) {
          if (invert == 0 && BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
            select_beztriple(bezt, false, SELECT, HIDDEN);
            bezt->hide = 1;
          }
          else if (invert && !BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
            select_beztriple(bezt, false, SELECT, HIDDEN);
            bezt->hide = 1;
          }
          if (bezt->hide) {
            sel++;
          }
          bezt++;
        }
        if (sel == nu->pntsu) {
          nu->hide = 1;
        }
      }
      else {
        bp = nu->bp;
        a = nu->pntsu * nu->pntsv;
        sel = 0;
        while (a--) {
          if (invert == 0 && (bp->f1 & SELECT)) {
            select_bpoint(bp, false, SELECT, HIDDEN);
            bp->hide = 1;
          }
          else if (invert && (bp->f1 & SELECT) == 0) {
            select_bpoint(bp, false, SELECT, HIDDEN);
            bp->hide = 1;
          }
          if (bp->hide) {
            sel++;
          }
          bp++;
        }
        if (sel == nu->pntsu * nu->pntsv) {
          nu->hide = 1;
        }
      }
    }

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    BKE_curve_nurb_vert_active_validate(static_cast<Curve *>(obedit->data));
  }
  return OPERATOR_FINISHED;
}

void CURVE_OT_hide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Selected";
  ot->idname = "CURVE_OT_hide";
  ot->description = "Hide (un)selected control points";

  /* API callbacks. */
  ot->exec = hide_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(
      ot->srna, "unselected", false, "Unselected", "Hide unselected rather than selected");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reveal Operator
 * \{ */

static wmOperatorStatus reveal_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool select = RNA_boolean_get(op->ptr, "select");
  bool changed_multi = false;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    ListBase *editnurb = object_editcurve_get(obedit);
    BPoint *bp;
    BezTriple *bezt;
    int a;
    bool changed = false;

    LISTBASE_FOREACH (Nurb *, nu, editnurb) {
      nu->hide = 0;
      if (nu->type == CU_BEZIER) {
        bezt = nu->bezt;
        a = nu->pntsu;
        while (a--) {
          if (bezt->hide) {
            select_beztriple(bezt, select, SELECT, HIDDEN);
            bezt->hide = 0;
            changed = true;
          }
          bezt++;
        }
      }
      else {
        bp = nu->bp;
        a = nu->pntsu * nu->pntsv;
        while (a--) {
          if (bp->hide) {
            select_bpoint(bp, select, SELECT, HIDDEN);
            bp->hide = 0;
            changed = true;
          }
          bp++;
        }
      }
    }

    if (changed) {
      DEG_id_tag_update(static_cast<ID *>(obedit->data),
                        ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SELECT | ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
      changed_multi = true;
    }
  }
  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void CURVE_OT_reveal(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reveal Hidden";
  ot->idname = "CURVE_OT_reveal";
  ot->description = "Reveal hidden control points";

  /* API callbacks. */
  ot->exec = reveal_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "select", true, "Select", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Subdivide Operator
 * \{ */

static void interp_bpoint(BPoint *bp_target,
                          const BPoint *bp_a,
                          const BPoint *bp_b,
                          const float factor)
{
  interp_v4_v4v4(bp_target->vec, bp_a->vec, bp_b->vec, factor);
  bp_target->tilt = interpf(bp_a->tilt, bp_b->tilt, factor);
  bp_target->weight = interpf(bp_a->weight, bp_b->weight, factor);
  bp_target->radius = interpf(bp_a->radius, bp_b->radius, factor);
}

/**
 * Divide the line segments associated with the currently selected
 * curve nodes (Bezier or NURB). If there are no valid segment
 * selections within the current selection, nothing happens.
 */
static void subdividenurb(Object *obedit, View3D *v3d, int number_cuts)
{
  Curve *cu = static_cast<Curve *>(obedit->data);
  EditNurb *editnurb = cu->editnurb;
  BezTriple *bezt, *beztnew, *beztn;
  BPoint *bp, *prevbp, *bpnew, *bpn;
  float vec[15];
  int a, b, sel, amount, *usel, *vsel;
  float factor;

  // printf("*** subdivideNurb: entering subdivide\n");

  LISTBASE_FOREACH (Nurb *, nu, &editnurb->nurbs) {
    amount = 0;
    if (nu->type == CU_BEZIER) {
      BezTriple *nextbezt;

      /*
       * Insert a point into a 2D Bezier curve.
       * Endpoints are preserved. Otherwise, all selected and inserted points are
       * newly created. Old points are discarded.
       */
      /* count */
      a = nu->pntsu;
      bezt = nu->bezt;
      while (a--) {
        nextbezt = BKE_nurb_bezt_get_next(nu, bezt);
        if (nextbezt == nullptr) {
          break;
        }

        if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt) && BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, nextbezt))
        {
          amount += number_cuts;
        }
        bezt++;
      }

      if (amount) {
        /* insert */
        beztnew = MEM_malloc_arrayN<BezTriple>((amount + nu->pntsu), "subdivNurb");
        beztn = beztnew;
        a = nu->pntsu;
        bezt = nu->bezt;
        while (a--) {
          memcpy(beztn, bezt, sizeof(BezTriple));
          keyIndex_updateBezt(editnurb, bezt, beztn, 1);
          beztn++;

          nextbezt = BKE_nurb_bezt_get_next(nu, bezt);
          if (nextbezt == nullptr) {
            break;
          }

          if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt) &&
              BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, nextbezt))
          {
            float prevvec[3][3];
            float prev_tilt = bezt->tilt;
            float prev_radius = bezt->radius;
            float prev_weight = bezt->weight;

            memcpy(prevvec, bezt->vec, sizeof(float[9]));

            for (int i = 0; i < number_cuts; i++) {
              factor = 1.0f / (number_cuts + 1 - i);

              memcpy(beztn, nextbezt, sizeof(BezTriple));

              /* midpoint subdividing */
              interp_v3_v3v3(vec, prevvec[1], prevvec[2], factor);
              interp_v3_v3v3(vec + 3, prevvec[2], nextbezt->vec[0], factor);
              interp_v3_v3v3(vec + 6, nextbezt->vec[0], nextbezt->vec[1], factor);

              interp_v3_v3v3(vec + 9, vec, vec + 3, factor);
              interp_v3_v3v3(vec + 12, vec + 3, vec + 6, factor);

              /* change handle of prev beztn */
              copy_v3_v3((beztn - 1)->vec[2], vec);
              /* new point */
              copy_v3_v3(beztn->vec[0], vec + 9);
              interp_v3_v3v3(beztn->vec[1], vec + 9, vec + 12, factor);
              copy_v3_v3(beztn->vec[2], vec + 12);
              /* handle of next bezt */
              if (a == 0 && i == number_cuts - 1 && (nu->flagu & CU_NURB_CYCLIC)) {
                copy_v3_v3(beztnew->vec[0], vec + 6);
              }
              else {
                copy_v3_v3(nextbezt->vec[0], vec + 6);
              }

              beztn->tilt = prev_tilt = interpf(nextbezt->tilt, prev_tilt, factor);
              beztn->radius = prev_radius = interpf(nextbezt->radius, prev_radius, factor);
              beztn->weight = prev_weight = interpf(nextbezt->weight, prev_weight, factor);

              memcpy(prevvec, beztn->vec, sizeof(float[9]));

              beztn++;
            }
          }

          bezt++;
        }

        MEM_freeN(nu->bezt);
        nu->bezt = beztnew;
        nu->pntsu += amount;

        BKE_nurb_handles_calc(nu);
      }
    } /* End of 'if (nu->type == CU_BEZIER)' */
    else if (nu->pntsv == 1) {
      BPoint *nextbp;

      /* NOTE(@nzc): All flat lines (ie. co-planar), except flat Nurbs. Flat NURB curves
       * are handled together with the regular NURB plane division, as it
       * should be. I split it off just now, let's see if it is stable. */

      /* Count. */
      a = nu->pntsu;
      bp = nu->bp;
      while (a--) {
        nextbp = BKE_nurb_bpoint_get_next(nu, bp);
        if (nextbp == nullptr) {
          break;
        }

        if ((bp->f1 & SELECT) && (nextbp->f1 & SELECT)) {
          amount += number_cuts;
        }
        bp++;
      }

      if (amount) {
        /* insert */
        bpnew = MEM_malloc_arrayN<BPoint>((amount + nu->pntsu), "subdivNurb2");
        bpn = bpnew;

        a = nu->pntsu;
        bp = nu->bp;

        while (a--) {
          /* Copy "old" point. */
          memcpy(bpn, bp, sizeof(BPoint));
          keyIndex_updateBP(editnurb, bp, bpn, 1);
          bpn++;

          nextbp = BKE_nurb_bpoint_get_next(nu, bp);
          if (nextbp == nullptr) {
            break;
          }

          if ((bp->f1 & SELECT) && (nextbp->f1 & SELECT)) {
            // printf("*** subdivideNurb: insert 'linear' point\n");
            for (int i = 0; i < number_cuts; i++) {
              factor = float(i + 1) / (number_cuts + 1);

              memcpy(bpn, nextbp, sizeof(BPoint));
              interp_bpoint(bpn, bp, nextbp, factor);
              bpn++;
            }
          }
          bp++;
        }

        MEM_freeN(nu->bp);
        nu->bp = bpnew;
        nu->pntsu += amount;

        if (nu->type & CU_NURBS) {
          BKE_nurb_knot_calc_u(nu);
        }
      }
    } /* End of 'else if (nu->pntsv == 1)' */
    else if (nu->type == CU_NURBS) {
      /* This is a very strange test ... */
      /**
       * NOTE(@nzc): Subdivide NURB surfaces
       *
       * Subdivision of a NURB curve can be effected by adding a
       * control point (insertion of a knot), or by raising the
       * degree of the functions used to build the NURB. The
       * expression
       *
       *     `degree = knots - controlpoints + 1` (J Walter piece)
       *     `degree = knots - controlpoints` (Blender implementation)
       *       ( this is confusing.... what is true? Another concern
       *       is that the JW piece allows the curve to become
       *       explicitly 1st order derivative discontinuous, while
       *       this is not what we want here... )
       *
       * is an invariant for a single NURB curve. Raising the degree
       * of the NURB is done elsewhere; the degree is assumed
       * constant during this operation. Degree is a property shared
       * by all control-points in a curve (even though it is stored
       * per control point - this can be misleading).
       * Adding a knot is done by searching for the place in the
       * knot vector where a certain knot value must be inserted, or
       * by picking an appropriate knot value between two existing
       * ones. The number of control-points that is influenced by the
       * insertion depends on the order of the curve. A certain
       * minimum number of knots is needed to form high-order
       * curves, as can be seen from the equation above. In Blender,
       * currently NURBs may be up to 6th order, so we modify at
       * most 6 points. One point is added. For an n-degree curve,
       * n points are discarded, and n+1 points inserted
       * (so effectively, n points are modified).  (that holds for
       * the JW piece, but it seems not for our NURBs)
       * In practice, the knot spacing is copied, but the tail
       * (the points following the insertion point) need to be
       * offset to keep the knot series ascending. The knot series
       * is always a series of monotonically ascending integers in
       * Blender. When not enough control points are available to
       * fit the order, duplicates of the endpoints are added as
       * needed.
       */
      /* selection-arrays */
      usel = MEM_calloc_arrayN<int>(nu->pntsu, "subivideNurb3");
      vsel = MEM_calloc_arrayN<int>(nu->pntsv, "subivideNurb3");
      sel = 0;

      /* Count the number of selected points. */
      bp = nu->bp;
      for (a = 0; a < nu->pntsv; a++) {
        for (b = 0; b < nu->pntsu; b++) {
          if (bp->f1 & SELECT) {
            usel[b]++;
            vsel[a]++;
            sel++;
          }
          bp++;
        }
      }
      if (sel == (nu->pntsu * nu->pntsv)) { /* subdivide entire nurb */
        /* Global subdivision is a special case of partial
         * subdivision. Strange it is considered separately... */

        /* count of nodes (after subdivision) along U axis */
        int countu = nu->pntsu + (nu->pntsu - 1) * number_cuts;

        /* total count of nodes after subdivision */
        int tot = ((number_cuts + 1) * nu->pntsu - number_cuts) *
                  ((number_cuts + 1) * nu->pntsv - number_cuts);

        bpn = bpnew = MEM_malloc_arrayN<BPoint>(tot, "subdivideNurb4");
        bp = nu->bp;
        /* first subdivide rows */
        for (a = 0; a < nu->pntsv; a++) {
          for (b = 0; b < nu->pntsu; b++) {
            *bpn = *bp;
            keyIndex_updateBP(editnurb, bp, bpn, 1);
            bpn++;
            bp++;
            if (b < nu->pntsu - 1) {
              prevbp = bp - 1;
              for (int i = 0; i < number_cuts; i++) {
                factor = float(i + 1) / (number_cuts + 1);
                *bpn = *bp;
                interp_bpoint(bpn, prevbp, bp, factor);
                bpn++;
              }
            }
          }
          bpn += number_cuts * countu;
        }
        /* now insert new */
        bpn = bpnew + ((number_cuts + 1) * nu->pntsu - number_cuts);
        bp = bpnew + (number_cuts + 1) * ((number_cuts + 1) * nu->pntsu - number_cuts);
        prevbp = bpnew;
        for (a = 1; a < nu->pntsv; a++) {

          for (b = 0; b < (number_cuts + 1) * nu->pntsu - number_cuts; b++) {
            BPoint *tmp = bpn;
            for (int i = 0; i < number_cuts; i++) {
              factor = float(i + 1) / (number_cuts + 1);
              *tmp = *bp;
              interp_bpoint(tmp, prevbp, bp, factor);
              tmp += countu;
            }
            bp++;
            prevbp++;
            bpn++;
          }
          bp += number_cuts * countu;
          bpn += number_cuts * countu;
          prevbp += number_cuts * countu;
        }
        MEM_freeN(nu->bp);
        nu->bp = bpnew;
        nu->pntsu = (number_cuts + 1) * nu->pntsu - number_cuts;
        nu->pntsv = (number_cuts + 1) * nu->pntsv - number_cuts;
        BKE_nurb_knot_calc_u(nu);
        BKE_nurb_knot_calc_v(nu);
      } /* End of 'if (sel == nu->pntsu * nu->pntsv)' (subdivide entire NURB) */
      else {
        /* subdivide in v direction? */
        sel = 0;
        for (a = 0; a < nu->pntsv - 1; a++) {
          if (vsel[a] == nu->pntsu && vsel[a + 1] == nu->pntsu) {
            sel += number_cuts;
          }
        }

        if (sel) { /* V direction. */
          bpn = bpnew = MEM_malloc_arrayN<BPoint>((sel + nu->pntsv) * nu->pntsu, "subdivideNurb4");
          bp = nu->bp;
          for (a = 0; a < nu->pntsv; a++) {
            for (b = 0; b < nu->pntsu; b++) {
              *bpn = *bp;
              keyIndex_updateBP(editnurb, bp, bpn, 1);
              bpn++;
              bp++;
            }
            if ((a < nu->pntsv - 1) && vsel[a] == nu->pntsu && vsel[a + 1] == nu->pntsu) {
              for (int i = 0; i < number_cuts; i++) {
                factor = float(i + 1) / (number_cuts + 1);
                prevbp = bp - nu->pntsu;
                for (b = 0; b < nu->pntsu; b++) {
                  /*
                   * This simple bisection must be replaces by a
                   * subtle resampling of a number of points. Our
                   * task is made slightly easier because each
                   * point in our curve is a separate data
                   * node. (is it?)
                   */
                  *bpn = *prevbp;
                  interp_bpoint(bpn, prevbp, bp, factor);
                  bpn++;

                  prevbp++;
                  bp++;
                }
                bp -= nu->pntsu;
              }
            }
          }
          MEM_freeN(nu->bp);
          nu->bp = bpnew;
          nu->pntsv += sel;
          BKE_nurb_knot_calc_v(nu);
        }
        else {
          /* or in u direction? */
          sel = 0;
          for (a = 0; a < nu->pntsu - 1; a++) {
            if (usel[a] == nu->pntsv && usel[a + 1] == nu->pntsv) {
              sel += number_cuts;
            }
          }

          if (sel) { /* U direction. */
            /* Inserting U points is sort of 'default' Flat curves only get
             * U points inserted in them. */
            bpn = bpnew = MEM_malloc_arrayN<BPoint>((sel + nu->pntsu) * nu->pntsv,
                                                    "subdivideNurb4");
            bp = nu->bp;
            for (a = 0; a < nu->pntsv; a++) {
              for (b = 0; b < nu->pntsu; b++) {
                *bpn = *bp;
                keyIndex_updateBP(editnurb, bp, bpn, 1);
                bpn++;
                bp++;
                if ((b < nu->pntsu - 1) && usel[b] == nu->pntsv && usel[b + 1] == nu->pntsv) {
                  /*
                   * One thing that bugs me here is that the
                   * orders of things are not the same as in
                   * the JW piece. Also, this implies that we
                   * handle at most 3rd order curves? I miss
                   * some symmetry here...
                   */
                  for (int i = 0; i < number_cuts; i++) {
                    factor = float(i + 1) / (number_cuts + 1);
                    prevbp = bp - 1;
                    *bpn = *prevbp;
                    interp_bpoint(bpn, prevbp, bp, factor);
                    bpn++;
                  }
                }
              }
            }
            MEM_freeN(nu->bp);
            nu->bp = bpnew;
            nu->pntsu += sel;
            BKE_nurb_knot_calc_u(nu); /* shift knots forward */
          }
        }
      }
      MEM_freeN(usel);
      MEM_freeN(vsel);

    } /* End of `if (nu->type == CU_NURBS)`. */
  }
}

static wmOperatorStatus subdivide_exec(bContext *C, wmOperator *op)
{
  const int number_cuts = RNA_int_get(op->ptr, "number_cuts");

  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Curve *cu = static_cast<Curve *>(obedit->data);

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    subdividenurb(obedit, v3d, number_cuts);

    if (ED_curve_updateAnimPaths(bmain, cu)) {
      WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);
    }

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, cu);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }

  return OPERATOR_FINISHED;
}

void CURVE_OT_subdivide(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Subdivide";
  ot->description = "Subdivide selected segments";
  ot->idname = "CURVE_OT_subdivide";

  /* API callbacks. */
  ot->exec = subdivide_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_int(ot->srna, "number_cuts", 1, 1, 1000, "Number of Cuts", "", 1, 10);
  /* Avoid re-using last var because it can cause _very_ high poly meshes
   * and annoy users (or worse crash). */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Spline Type Operator
 * \{ */

static wmOperatorStatus set_spline_type_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  wmOperatorStatus ret_value = OPERATOR_CANCELLED;

  for (Object *obedit : objects) {
    Main *bmain = CTX_data_main(C);
    View3D *v3d = CTX_wm_view3d(C);
    ListBase *editnurb = object_editcurve_get(obedit);
    bool changed = false;
    bool changed_size = false;
    const bool use_handles = RNA_boolean_get(op->ptr, "use_handles");
    const int type = RNA_enum_get(op->ptr, "type");

    LISTBASE_FOREACH (Nurb *, nu, editnurb) {
      if (ED_curve_nurb_select_check(v3d, nu)) {
        const int pntsu_prev = nu->pntsu;
        const char *err_msg = nullptr;
        if (BKE_nurb_type_convert(nu, type, use_handles, &err_msg)) {
          changed = true;
          if (pntsu_prev != nu->pntsu) {
            changed_size = true;
          }
        }
        else {
          BKE_report(op->reports, RPT_ERROR, err_msg);
        }
      }
    }

    if (changed) {
      if (ED_curve_updateAnimPaths(bmain, static_cast<Curve *>(obedit->data))) {
        WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);
      }

      DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

      if (changed_size) {
        Curve *cu = static_cast<Curve *>(obedit->data);
        cu->actvert = CU_ACT_NONE;
      }

      ret_value = OPERATOR_FINISHED;
    }
  }

  return ret_value;
}

void CURVE_OT_spline_type_set(wmOperatorType *ot)
{
  static const EnumPropertyItem type_items[] = {
      {CU_POLY, "POLY", 0, "Poly", ""},
      {CU_BEZIER, "BEZIER", 0, "Bézier", ""},
      {CU_NURBS, "NURBS", 0, "NURBS", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Set Spline Type";
  ot->description = "Set type of active spline";
  ot->idname = "CURVE_OT_spline_type_set";

  /* API callbacks. */
  ot->exec = set_spline_type_exec;
  ot->invoke = WM_menu_invoke;
  ot->poll = ED_operator_editcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", type_items, CU_POLY, "Type", "Spline type");
  RNA_def_boolean(ot->srna,
                  "use_handles",
                  false,
                  "Handles",
                  "Use handles when converting Bézier curves into polygons");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Handle Type Operator
 * \{ */

static wmOperatorStatus set_handle_type_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  const int handle_type = RNA_enum_get(op->ptr, "type");
  const bool hide_handles = (v3d && (v3d->overlay.handle_display == CURVE_HANDLE_NONE));
  const eNurbHandleTest_Mode handle_mode = hide_handles ? NURB_HANDLE_TEST_KNOT_ONLY :
                                                          NURB_HANDLE_TEST_KNOT_OR_EACH;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Curve *cu = static_cast<Curve *>(obedit->data);

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    ListBase *editnurb = object_editcurve_get(obedit);
    BKE_nurbList_handles_set(editnurb, handle_mode, handle_type);

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }
  return OPERATOR_FINISHED;
}

void CURVE_OT_handle_type_set(wmOperatorType *ot)
{
  /* keep in sync with graphkeys_handle_type_items */
  static const EnumPropertyItem editcurve_handle_type_items[] = {
      {HD_AUTO, "AUTOMATIC", ICON_HANDLE_AUTO, "Automatic", ""},
      {HD_VECT, "VECTOR", ICON_HANDLE_VECTOR, "Vector", ""},
      {5, "ALIGNED", ICON_HANDLE_ALIGNED, "Aligned", ""},
      {6, "FREE_ALIGN", ICON_HANDLE_FREE, "Free", ""},
      {3, "TOGGLE_FREE_ALIGN", 0, "Toggle Free/Align", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Set Handle Type";
  ot->description = "Set type of handles for selected control points";
  ot->idname = "CURVE_OT_handle_type_set";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = set_handle_type_exec;
  ot->poll = ED_operator_editcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", editcurve_handle_type_items, 1, "Type", "Spline type");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalculate Handles Operator
 * \{ */

static wmOperatorStatus curve_normals_make_consistent_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  const bool calc_length = RNA_boolean_get(op->ptr, "calc_length");

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  int totobjects = 0;

  for (Object *obedit : objects) {
    Curve *cu = static_cast<Curve *>(obedit->data);

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    if (blender::ed::object::shape_key_report_if_locked(obedit, op->reports)) {
      continue;
    }

    totobjects++;

    ListBase *editnurb = object_editcurve_get(obedit);
    BKE_nurbList_handles_recalculate(editnurb, calc_length, SELECT);

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }
  return totobjects ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void CURVE_OT_normals_make_consistent(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Recalculate Handles";
  ot->description = "Recalculate the direction of selected handles";
  ot->idname = "CURVE_OT_normals_make_consistent";

  /* API callbacks. */
  ot->exec = curve_normals_make_consistent_exec;
  ot->poll = ED_operator_editcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(ot->srna, "calc_length", false, "Length", "Recalculate handle length");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Segment Operator
 *
 * Also handles skinning & lofting.
 * \{ */

static void switchdirection_knots(float *base, int tot)
{
  float *fp1, *fp2, *tempf;
  int a;

  if (base == nullptr || tot == 0) {
    return;
  }

  /* reverse knots */
  a = tot;
  fp1 = base;
  fp2 = fp1 + (a - 1);
  a /= 2;
  while (fp1 != fp2 && a > 0) {
    std::swap(*fp1, *fp2);
    a--;
    fp1++;
    fp2--;
  }

  /* and make in increasing order again */
  a = tot - 1;
  fp1 = base;
  fp2 = tempf = MEM_malloc_arrayN<float>(tot, "switchdirect");
  while (a--) {
    fp2[0] = fabsf(fp1[1] - fp1[0]);
    fp1++;
    fp2++;
  }
  fp2[0] = 0.0f;

  a = tot - 1;
  fp1 = base;
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

static void rotate_direction_nurb(Nurb *nu)
{
  BPoint *bp1, *bp2, *temp;
  int u, v;

  std::swap(nu->pntsu, nu->pntsv);
  std::swap(nu->orderu, nu->orderv);
  std::swap(nu->resolu, nu->resolv);
  std::swap(nu->flagu, nu->flagv);

  std::swap(nu->knotsu, nu->knotsv);
  switchdirection_knots(nu->knotsv, KNOTSV(nu));

  temp = static_cast<BPoint *>(MEM_dupallocN(nu->bp));
  bp1 = nu->bp;
  for (v = 0; v < nu->pntsv; v++) {
    for (u = 0; u < nu->pntsu; u++, bp1++) {
      bp2 = temp + (nu->pntsu - u - 1) * (nu->pntsv) + v;
      *bp1 = *bp2;
    }
  }

  MEM_freeN(temp);
}

static bool is_u_selected(Nurb *nu, int u)
{
  BPoint *bp;
  int v;

  /* what about resolu == 2? */
  bp = &nu->bp[u];
  for (v = 0; v < nu->pntsv - 1; v++, bp += nu->pntsu) {
    if ((v != 0) && (bp->f1 & SELECT)) {
      return true;
    }
  }

  return false;
}

struct NurbSort {
  NurbSort *next, *prev;
  Nurb *nu;
  float vec[3];
};

static void make_selection_list_nurb(View3D *v3d, ListBase *editnurb, ListBase *nsortbase)
{
  ListBase nbase = {nullptr, nullptr};
  NurbSort *nus, *nustest, *headdo, *taildo;
  BPoint *bp;
  float dist, headdist, taildist;
  int a;

  LISTBASE_FOREACH (Nurb *, nu, editnurb) {
    if (ED_curve_nurb_select_check(v3d, nu)) {

      nus = MEM_callocN<NurbSort>("sort");
      BLI_addhead(&nbase, nus);
      nus->nu = nu;

      bp = nu->bp;
      a = nu->pntsu;
      while (a--) {
        add_v3_v3(nus->vec, bp->vec);
        bp++;
      }
      mul_v3_fl(nus->vec, 1.0f / float(nu->pntsu));
    }
  }

  /* just add the first one */
  nus = static_cast<NurbSort *>(nbase.first);
  BLI_remlink(&nbase, nus);
  BLI_addtail(nsortbase, nus);

  /* now add, either at head or tail, the closest one */
  while (nbase.first) {

    headdist = taildist = 1.0e30;
    headdo = taildo = nullptr;

    nustest = static_cast<NurbSort *>(nbase.first);
    while (nustest) {
      dist = len_v3v3(nustest->vec, ((NurbSort *)nsortbase->first)->vec);

      if (dist < headdist) {
        headdist = dist;
        headdo = nustest;
      }
      dist = len_v3v3(nustest->vec, ((NurbSort *)nsortbase->last)->vec);

      if (dist < taildist) {
        taildist = dist;
        taildo = nustest;
      }
      nustest = nustest->next;
    }

    if (headdist < taildist) {
      BLI_remlink(&nbase, headdo);
      BLI_addhead(nsortbase, headdo);
    }
    else {
      BLI_remlink(&nbase, taildo);
      BLI_addtail(nsortbase, taildo);
    }
  }
}

enum {
  CURVE_MERGE_OK = 0,
  CURVE_MERGE_ERR_FEW_SELECTION,
  CURVE_MERGE_ERR_RESOLUTION_ALL,
  CURVE_MERGE_ERR_RESOLUTION_SOME,
};

static bool merge_2_nurb(Curve *cu, ListBase *editnurb, Nurb *nu1, Nurb *nu2)
{
  BPoint *bp, *bp1, *bp2, *temp;
  float len1, len2;
  int origu, u, v;

  /* first nurbs will be changed to make u = resolu-1 selected */
  /* 2nd nurbs will be changed to make u = 0 selected */

  /* first nurbs: u = resolu-1 selected */

  if (is_u_selected(nu1, nu1->pntsu - 1)) {
    /* pass */
  }
  else {
    /* For 2D curves blender uses (orderv = 0). It doesn't make any sense mathematically. */
    /* but after rotating (orderu = 0) will be confusing. */
    if (nu1->orderv == 0) {
      nu1->orderv = 1;
    }

    rotate_direction_nurb(nu1);
    if (is_u_selected(nu1, nu1->pntsu - 1)) {
      /* pass */
    }
    else {
      rotate_direction_nurb(nu1);
      if (is_u_selected(nu1, nu1->pntsu - 1)) {
        /* pass */
      }
      else {
        rotate_direction_nurb(nu1);
        if (is_u_selected(nu1, nu1->pntsu - 1)) {
          /* pass */
        }
        else {
          /* rotate again, now its OK! */
          if (nu1->pntsv != 1) {
            rotate_direction_nurb(nu1);
          }
          return true;
        }
      }
    }
  }

  /* 2nd nurbs: u = 0 selected */
  if (is_u_selected(nu2, 0)) {
    /* pass */
  }
  else {
    if (nu2->orderv == 0) {
      nu2->orderv = 1;
    }
    rotate_direction_nurb(nu2);
    if (is_u_selected(nu2, 0)) {
      /* pass */
    }
    else {
      rotate_direction_nurb(nu2);
      if (is_u_selected(nu2, 0)) {
        /* pass */
      }
      else {
        rotate_direction_nurb(nu2);
        if (is_u_selected(nu2, 0)) {
          /* pass */
        }
        else {
          /* rotate again, now its OK! */
          if (nu1->pntsu == 1) {
            rotate_direction_nurb(nu1);
          }
          if (nu2->pntsv != 1) {
            rotate_direction_nurb(nu2);
          }
          return true;
        }
      }
    }
  }

  if (nu1->pntsv != nu2->pntsv) {
    return false;
  }

  /* ok, now nu1 has the rightmost column and nu2 the leftmost column selected */
  /* maybe we need a 'v' flip of nu2? */

  bp1 = &nu1->bp[nu1->pntsu - 1];
  bp2 = nu2->bp;
  len1 = 0.0;

  for (v = 0; v < nu1->pntsv; v++, bp1 += nu1->pntsu, bp2 += nu2->pntsu) {
    len1 += len_v3v3(bp1->vec, bp2->vec);
  }

  bp1 = &nu1->bp[nu1->pntsu - 1];
  bp2 = &nu2->bp[nu2->pntsu * (nu2->pntsv - 1)];
  len2 = 0.0;

  for (v = 0; v < nu1->pntsv; v++, bp1 += nu1->pntsu, bp2 -= nu2->pntsu) {
    len2 += len_v3v3(bp1->vec, bp2->vec);
  }

  /* merge */
  origu = nu1->pntsu;
  nu1->pntsu += nu2->pntsu;
  if (nu1->orderu < 3 && nu1->orderu < nu1->pntsu) {
    nu1->orderu++;
  }
  if (nu1->orderv < 3 && nu1->orderv < nu1->pntsv) {
    nu1->orderv++;
  }
  temp = nu1->bp;
  nu1->bp = MEM_malloc_arrayN<BPoint>(nu1->pntsu * nu1->pntsv, "mergeBP");

  bp = nu1->bp;
  bp1 = temp;

  for (v = 0; v < nu1->pntsv; v++) {

    /* switch direction? */
    if (len1 < len2) {
      bp2 = &nu2->bp[v * nu2->pntsu];
    }
    else {
      bp2 = &nu2->bp[(nu1->pntsv - v - 1) * nu2->pntsu];
    }

    for (u = 0; u < nu1->pntsu; u++, bp++) {
      if (u < origu) {
        keyIndex_updateBP(cu->editnurb, bp1, bp, 1);
        *bp = *bp1;
        bp1++;
        select_bpoint(bp, true, SELECT, HIDDEN);
      }
      else {
        keyIndex_updateBP(cu->editnurb, bp2, bp, 1);
        *bp = *bp2;
        bp2++;
      }
    }
  }

  if (nu1->type == CU_NURBS) {
    /* merge knots */
    BKE_nurb_knot_calc_u(nu1);

    /* make knots, for merged curved for example */
    BKE_nurb_knot_calc_v(nu1);
  }

  MEM_freeN(temp);
  BLI_remlink(editnurb, nu2);
  BKE_nurb_free(nu2);
  return true;
}

static int merge_nurb(View3D *v3d, Object *obedit)
{
  Curve *cu = static_cast<Curve *>(obedit->data);
  ListBase *editnurb = object_editcurve_get(obedit);
  NurbSort *nus1, *nus2;
  bool ok = true;
  ListBase nsortbase = {nullptr, nullptr};

  make_selection_list_nurb(v3d, editnurb, &nsortbase);

  if (nsortbase.first == nsortbase.last) {
    BLI_freelistN(&nsortbase);
    return CURVE_MERGE_ERR_FEW_SELECTION;
  }

  nus1 = static_cast<NurbSort *>(nsortbase.first);
  nus2 = nus1->next;

  /* resolution match, to avoid uv rotations */
  if (nus1->nu->pntsv == 1) {
    if (ELEM(nus1->nu->pntsu, nus2->nu->pntsu, nus2->nu->pntsv)) {
      /* pass */
    }
    else {
      ok = false;
    }
  }
  else if (nus2->nu->pntsv == 1) {
    if (ELEM(nus2->nu->pntsu, nus1->nu->pntsu, nus1->nu->pntsv)) {
      /* pass */
    }
    else {
      ok = false;
    }
  }
  else if (nus1->nu->pntsu == nus2->nu->pntsu || nus1->nu->pntsv == nus2->nu->pntsv) {
    /* pass */
  }
  else if (nus1->nu->pntsu == nus2->nu->pntsv || nus1->nu->pntsv == nus2->nu->pntsu) {
    /* pass */
  }
  else {
    ok = false;
  }

  if (ok == false) {
    BLI_freelistN(&nsortbase);
    return CURVE_MERGE_ERR_RESOLUTION_ALL;
  }

  while (nus2) {
    /* There is a change a few curves merged properly, but not all.
     * In this case we still update the curve, yet report the error. */
    ok &= merge_2_nurb(cu, editnurb, nus1->nu, nus2->nu);
    nus2 = nus2->next;
  }

  BLI_freelistN(&nsortbase);
  BKE_curve_nurb_active_set(static_cast<Curve *>(obedit->data), nullptr);

  return ok ? CURVE_MERGE_OK : CURVE_MERGE_ERR_RESOLUTION_SOME;
}

static wmOperatorStatus make_segment_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  struct {
    int changed;
    int unselected;
    int error_selected_few;
    int error_resolution;
    int error_generic;
  } status = {0};

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Curve *cu = static_cast<Curve *>(obedit->data);

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      status.unselected++;
      continue;
    }

    ListBase *nubase = object_editcurve_get(obedit);
    Nurb *nu, *nu1 = nullptr, *nu2 = nullptr;
    BPoint *bp;
    bool ok = false;

    /* first decide if this is a surface merge! */
    if (obedit->type == OB_SURF) {
      nu = static_cast<Nurb *>(nubase->first);
    }
    else {
      nu = nullptr;
    }

    while (nu) {
      const int nu_select_num = ED_curve_nurb_select_count(v3d, nu);
      if (nu_select_num) {

        if (nu->pntsu > 1 && nu->pntsv > 1) {
          break;
        }

        if (nu_select_num > 1) {
          break;
        }
        /* only 1 selected, not first or last, a little complex, but intuitive */
        if (nu->pntsv == 1) {
          if ((nu->bp->f1 & SELECT) || (nu->bp[nu->pntsu - 1].f1 & SELECT)) {
            /* pass */
          }
          else {
            break;
          }
        }
      }
      nu = nu->next;
    }

    if (nu) {
      int merge_result = merge_nurb(v3d, obedit);
      switch (merge_result) {
        case CURVE_MERGE_OK:
          status.changed++;
          goto curve_merge_tag_object;
        case CURVE_MERGE_ERR_RESOLUTION_SOME:
          status.error_resolution++;
          goto curve_merge_tag_object;
        case CURVE_MERGE_ERR_FEW_SELECTION:
          status.error_selected_few++;
          break;
        case CURVE_MERGE_ERR_RESOLUTION_ALL:
          status.error_resolution++;
          break;
      }
      continue;
    }

    /* find both nurbs and points, nu1 will be put behind nu2 */
    LISTBASE_FOREACH (Nurb *, nu, nubase) {
      if (nu->pntsu == 1) {
        nu->flagu &= ~CU_NURB_CYCLIC;
      }

      if ((nu->flagu & CU_NURB_CYCLIC) == 0) { /* not cyclic */
        if (nu->type == CU_BEZIER) {
          if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, &(nu->bezt[nu->pntsu - 1]))) {
            /* Last point is selected, preferred for nu2 */
            if (nu2 == nullptr) {
              nu2 = nu;
            }
            else if (nu1 == nullptr) {
              nu1 = nu;

              /* Just in case both of first/last CV are selected check
               * whether we really need to switch the direction.
               */
              if (!BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, nu1->bezt)) {
                BKE_nurb_direction_switch(nu1);
                keyData_switchDirectionNurb(cu, nu1);
              }
            }
          }
          else if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, nu->bezt)) {
            /* First point is selected, preferred for nu1 */
            if (nu1 == nullptr) {
              nu1 = nu;
            }
            else if (nu2 == nullptr) {
              nu2 = nu;

              /* Just in case both of first/last CV are selected check
               * whether we really need to switch the direction.
               */
              if (!BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, &(nu->bezt[nu2->pntsu - 1]))) {
                BKE_nurb_direction_switch(nu2);
                keyData_switchDirectionNurb(cu, nu2);
              }
            }
          }
        }
        else if (nu->pntsv == 1) {
          /* Same logic as above: if first point is selected spline is
           * preferred for nu1, if last point is selected spline is
           * preferred for u2u.
           */

          bp = nu->bp;
          if (bp[nu->pntsu - 1].f1 & SELECT) {
            if (nu2 == nullptr) {
              nu2 = nu;
            }
            else if (nu1 == nullptr) {
              nu1 = nu;

              if ((bp->f1 & SELECT) == 0) {
                BKE_nurb_direction_switch(nu);
                keyData_switchDirectionNurb(cu, nu);
              }
            }
          }
          else if (bp->f1 & SELECT) {
            if (nu1 == nullptr) {
              nu1 = nu;
            }
            else if (nu2 == nullptr) {
              nu2 = nu;

              if ((bp[nu->pntsu - 1].f1 & SELECT) == 0) {
                BKE_nurb_direction_switch(nu);
                keyData_switchDirectionNurb(cu, nu);
              }
            }
          }
        }
      }

      if (nu1 && nu2) {
        /* Got second spline, no need to loop over rest of the splines. */
        break;
      }
    }

    if ((nu1 && nu2) && (nu1 != nu2)) {
      if (nu1->type == nu2->type) {
        if (nu1->type == CU_BEZIER) {
          BezTriple *bezt = MEM_malloc_arrayN<BezTriple>((nu1->pntsu + nu2->pntsu), "addsegmentN");
          ED_curve_beztcpy(cu->editnurb, bezt, nu2->bezt, nu2->pntsu);
          ED_curve_beztcpy(cu->editnurb, bezt + nu2->pntsu, nu1->bezt, nu1->pntsu);

          MEM_freeN(nu1->bezt);
          nu1->bezt = bezt;
          nu1->pntsu += nu2->pntsu;
          BLI_remlink(nubase, nu2);
          keyIndex_delNurb(cu->editnurb, nu2);
          BKE_nurb_free(nu2);
          nu2 = nullptr;
          BKE_nurb_handles_calc(nu1);
        }
        else {
          bp = MEM_malloc_arrayN<BPoint>((nu1->pntsu + nu2->pntsu), "addsegmentN2");
          ED_curve_bpcpy(cu->editnurb, bp, nu2->bp, nu2->pntsu);
          ED_curve_bpcpy(cu->editnurb, bp + nu2->pntsu, nu1->bp, nu1->pntsu);
          MEM_freeN(nu1->bp);
          nu1->bp = bp;

          // a = nu1->pntsu + nu1->orderu; /* UNUSED */

          nu1->pntsu += nu2->pntsu;
          BLI_remlink(nubase, nu2);

          /* now join the knots */
          if (nu1->type == CU_NURBS) {
            MEM_SAFE_FREE(nu1->knotsu);

            BKE_nurb_knot_calc_u(nu1);
          }
          keyIndex_delNurb(cu->editnurb, nu2);
          BKE_nurb_free(nu2);
          nu2 = nullptr;
        }

        BKE_curve_nurb_active_set(cu, nu1); /* for selected */
        ok = true;
      }
    }
    else if ((nu1 && !nu2) || (!nu1 && nu2)) {
      if (nu2) {
        std::swap(nu1, nu2);
      }

      if (!(nu1->flagu & CU_NURB_CYCLIC) && nu1->pntsu > 1) {
        if (nu1->type == CU_BEZIER && BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, nu1->bezt) &&
            BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, &nu1->bezt[nu1->pntsu - 1]))
        {
          nu1->flagu |= CU_NURB_CYCLIC;
          BKE_nurb_handles_calc(nu1);
          ok = true;
        }
        else if (ELEM(nu1->type, CU_NURBS, CU_POLY) && nu1->bp->f1 & SELECT &&
                 (nu1->bp[nu1->pntsu - 1].f1 & SELECT))
        {
          nu1->flagu |= CU_NURB_CYCLIC;
          BKE_nurb_knot_calc_u(nu1);
          ok = true;
        }
      }
    }

    if (!ok) {
      status.error_generic++;
      continue;
    }

    if (ED_curve_updateAnimPaths(bmain, static_cast<Curve *>(obedit->data))) {
      WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);
    }

    status.changed++;

  curve_merge_tag_object:
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }

  if (status.unselected == objects.size()) {
    BKE_report(op->reports, RPT_ERROR, "No points were selected");
    return OPERATOR_CANCELLED;
  }

  const int tot_errors = status.error_selected_few + status.error_resolution +
                         status.error_generic;
  if (tot_errors > 0) {
    /* Some curves changed, but some curves failed: don't explain why it failed. */
    if (status.changed) {
      BKE_reportf(op->reports, RPT_INFO, "%d curves could not make segments", tot_errors);
      return OPERATOR_FINISHED;
    }

    /* All curves failed: If there is more than one error give a generic error report. */
    if (((status.error_selected_few ? 1 : 0) + (status.error_resolution ? 1 : 0) +
         (status.error_generic ? 1 : 0)) > 1)
    {
      BKE_report(op->reports, RPT_ERROR, "Could not make new segments");
    }

    /* All curves failed due to the same error. */
    if (status.error_selected_few) {
      BKE_report(op->reports, RPT_ERROR, "Too few selections to merge");
    }
    else if (status.error_resolution) {
      BKE_report(op->reports, RPT_ERROR, "Resolution does not match");
    }
    else {
      BLI_assert(status.error_generic);
      BKE_report(op->reports, RPT_ERROR, "Cannot make segment");
    }
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void CURVE_OT_make_segment(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Segment";
  ot->idname = "CURVE_OT_make_segment";
  ot->description = "Join two curves by their selected ends";

  /* API callbacks. */
  ot->exec = make_segment_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pick Select from 3D View
 * \{ */

bool ED_curve_editnurb_select_pick(bContext *C,
                                   const int mval[2],
                                   const int dist_px,
                                   const SelectPick_Params &params)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Nurb *nu;
  BezTriple *bezt = nullptr;
  BPoint *bp = nullptr;
  Base *basact = nullptr;
  short hand;
  bool changed = false;

  view3d_operator_needs_gpu(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  copy_v2_v2_int(vc.mval, mval);

  const bool use_handle_select = (vc.v3d->overlay.handle_display != CURVE_HANDLE_NONE);

  bool found = ED_curve_pick_vert_ex(&vc, true, dist_px, &nu, &bezt, &bp, &hand, &basact);

  if (params.sel_op == SEL_OP_SET) {
    if ((found && params.select_passthrough) &&
        (((bezt ? (&bezt->f1)[hand] : bp->f1) & SELECT) != 0))
    {
      found = false;
    }
    else if (found || params.deselect_all) {
      /* Deselect everything. */
      Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
          vc.scene, vc.view_layer, vc.v3d);
      for (Object *ob_iter : objects) {
        ED_curve_deselect_all(((Curve *)ob_iter->data)->editnurb);
        DEG_id_tag_update(static_cast<ID *>(ob_iter->data),
                          ID_RECALC_SELECT | ID_RECALC_SYNC_TO_EVAL);
        WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob_iter->data);
      }
      changed = true;
    }
  }

  if (found) {
    Object *obedit = basact->object;
    Curve *cu = static_cast<Curve *>(obedit->data);
    ListBase *editnurb = object_editcurve_get(obedit);
    const void *vert = BKE_curve_vert_active_get(cu);

    switch (params.sel_op) {
      case SEL_OP_ADD: {
        if (bezt) {
          if (hand == 1) {
            if (use_handle_select) {
              bezt->f2 |= SELECT;
            }
            else {
              select_beztriple(bezt, true, SELECT, HIDDEN);
            }
          }
          else {
            if (hand == 0) {
              bezt->f1 |= SELECT;
            }
            else {
              bezt->f3 |= SELECT;
            }
          }
          BKE_curve_nurb_vert_active_set(cu, nu, bezt);
        }
        else {
          select_bpoint(bp, true, SELECT, HIDDEN);
          BKE_curve_nurb_vert_active_set(cu, nu, bp);
        }
        break;
      }
      case SEL_OP_SUB: {
        if (bezt) {
          if (hand == 1) {
            if (use_handle_select) {
              bezt->f2 &= ~SELECT;
            }
            else {
              select_beztriple(bezt, false, SELECT, HIDDEN);
            }
            if (bezt == vert) {
              cu->actvert = CU_ACT_NONE;
            }
          }
          else if (hand == 0) {
            bezt->f1 &= ~SELECT;
          }
          else {
            bezt->f3 &= ~SELECT;
          }
        }
        else {
          select_bpoint(bp, false, SELECT, HIDDEN);
          if (bp == vert) {
            cu->actvert = CU_ACT_NONE;
          }
        }
        break;
      }
      case SEL_OP_XOR: {
        if (bezt) {
          if (hand == 1) {
            if (bezt->f2 & SELECT) {
              if (use_handle_select) {
                bezt->f2 &= ~SELECT;
              }
              else {
                select_beztriple(bezt, false, SELECT, HIDDEN);
              }
              if (bezt == vert) {
                cu->actvert = CU_ACT_NONE;
              }
            }
            else {
              if (use_handle_select) {
                bezt->f2 |= SELECT;
              }
              else {
                select_beztriple(bezt, true, SELECT, HIDDEN);
              }
              BKE_curve_nurb_vert_active_set(cu, nu, bezt);
            }
          }
          else if (hand == 0) {
            bezt->f1 ^= SELECT;
          }
          else {
            bezt->f3 ^= SELECT;
          }
        }
        else {
          if (bp->f1 & SELECT) {
            select_bpoint(bp, false, SELECT, HIDDEN);
            if (bp == vert) {
              cu->actvert = CU_ACT_NONE;
            }
          }
          else {
            select_bpoint(bp, true, SELECT, HIDDEN);
            BKE_curve_nurb_vert_active_set(cu, nu, bp);
          }
        }
        break;
      }
      case SEL_OP_SET: {
        BKE_nurbList_flag_set(editnurb, SELECT, false);

        if (bezt) {

          if (hand == 1) {
            if (use_handle_select) {
              bezt->f2 |= SELECT;
            }
            else {
              select_beztriple(bezt, true, SELECT, HIDDEN);
            }
          }
          else {
            if (hand == 0) {
              bezt->f1 |= SELECT;
            }
            else {
              bezt->f3 |= SELECT;
            }
          }
          BKE_curve_nurb_vert_active_set(cu, nu, bezt);
        }
        else {
          select_bpoint(bp, true, SELECT, HIDDEN);
          BKE_curve_nurb_vert_active_set(cu, nu, bp);
        }
        break;
      }
      case SEL_OP_AND: {
        BLI_assert_unreachable(); /* Doesn't make sense for picking. */
        break;
      }
    }

    if (nu != BKE_curve_nurb_active_get(cu)) {
      cu->actvert = CU_ACT_NONE;
      BKE_curve_nurb_active_set(cu, nu);
    }

    /* Change active material on object. */
    blender::ed::object::material_active_index_set(obedit, nu->mat_nr);

    BKE_view_layer_synced_ensure(vc.scene, vc.view_layer);
    if (BKE_view_layer_active_base_get(vc.view_layer) != basact) {
      blender::ed::object::base_activate(C, basact);
    }

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT | ID_RECALC_SYNC_TO_EVAL);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

    changed = true;
  }

  return changed || found;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Spin Operator
 * \{ */

bool ed_editnurb_spin(
    float viewmat[4][4], View3D *v3d, Object *obedit, const float axis[3], const float cent[3])
{
  Curve *cu = (Curve *)obedit->data;
  ListBase *editnurb = object_editcurve_get(obedit);
  float cmat[3][3], tmat[3][3], imat[3][3];
  float bmat[3][3], rotmat[3][3], scalemat1[3][3], scalemat2[3][3];
  float persmat[3][3], persinv[3][3];
  bool ok, changed = false;
  int a;

  copy_m3_m4(persmat, viewmat);
  invert_m3_m3(persinv, persmat);

  /* imat and center and size */
  copy_m3_m4(bmat, obedit->object_to_world().ptr());
  invert_m3_m3(imat, bmat);

  axis_angle_to_mat3(cmat, axis, M_PI_4);
  mul_m3_m3m3(tmat, cmat, bmat);
  mul_m3_m3m3(rotmat, imat, tmat);

  unit_m3(scalemat1);
  scalemat1[0][0] = M_SQRT2;
  scalemat1[1][1] = M_SQRT2;

  mul_m3_m3m3(tmat, persmat, bmat);
  mul_m3_m3m3(cmat, scalemat1, tmat);
  mul_m3_m3m3(tmat, persinv, cmat);
  mul_m3_m3m3(scalemat1, imat, tmat);

  unit_m3(scalemat2);
  scalemat2[0][0] /= float(M_SQRT2);
  scalemat2[1][1] /= float(M_SQRT2);

  mul_m3_m3m3(tmat, persmat, bmat);
  mul_m3_m3m3(cmat, scalemat2, tmat);
  mul_m3_m3m3(tmat, persinv, cmat);
  mul_m3_m3m3(scalemat2, imat, tmat);

  ok = true;

  for (a = 0; a < 7; a++) {
    ok = ed_editnurb_extrude_flag(cu->editnurb, SELECT);

    if (ok == false) {
      return changed;
    }

    changed = true;

    rotateflagNurb(editnurb, SELECT, cent, rotmat);

    if ((a & 1) == 0) {
      rotateflagNurb(editnurb, SELECT, cent, scalemat1);
      weightflagNurb(editnurb, SELECT, 0.5 * M_SQRT2);
    }
    else {
      rotateflagNurb(editnurb, SELECT, cent, scalemat2);
      weightflagNurb(editnurb, SELECT, 2.0 / M_SQRT2);
    }
  }

  if (ok) {
    LISTBASE_FOREACH (Nurb *, nu, editnurb) {
      if (ED_curve_nurb_select_check(v3d, nu)) {
        nu->orderv = 3;
        /* It is challenging to create a good approximation of a circle with uniform knots vector
         * (which is forced in Blender for cyclic NURBS curves). Here a NURBS circle is constructed
         * by connecting four Bezier arcs. */
        nu->flagv |= CU_NURB_CYCLIC | CU_NURB_BEZIER | CU_NURB_ENDPOINT;
        BKE_nurb_knot_calc_v(nu);
      }
    }
  }

  return changed;
}

static wmOperatorStatus spin_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = ED_view3d_context_rv3d(C);
  float cent[3], axis[3], viewmat[4][4];
  bool changed = false;
  int count_failed = 0;

  RNA_float_get_array(op->ptr, "center", cent);
  RNA_float_get_array(op->ptr, "axis", axis);

  if (rv3d) {
    copy_m4_m4(viewmat, rv3d->viewmat);
  }
  else {
    unit_m4(viewmat);
  }

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Curve *cu = (Curve *)obedit->data;

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    invert_m4_m4(obedit->runtime->world_to_object.ptr(), obedit->object_to_world().ptr());
    mul_m4_v3(obedit->world_to_object().ptr(), cent);

    if (!ed_editnurb_spin(viewmat, v3d, obedit, axis, cent)) {
      count_failed += 1;
      continue;
    }

    changed = true;
    if (ED_curve_updateAnimPaths(bmain, cu)) {
      WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);
    }

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }

  if (changed == false) {
    if (count_failed != 0) {
      BKE_report(op->reports, RPT_ERROR, "Cannot spin");
    }
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

static wmOperatorStatus spin_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Scene *scene = CTX_data_scene(C);
  RegionView3D *rv3d = ED_view3d_context_rv3d(C);
  float axis[3] = {0.0f, 0.0f, 1.0f};

  if (rv3d) {
    copy_v3_v3(axis, rv3d->viewinv[2]);
  }

  RNA_float_set_array(op->ptr, "center", scene->cursor.location);
  RNA_float_set_array(op->ptr, "axis", axis);

  return spin_exec(C, op);
}

void CURVE_OT_spin(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Spin";
  ot->idname = "CURVE_OT_spin";
  ot->description = "Extrude selected boundary row around pivot point and current view axis";

  /* API callbacks. */
  ot->exec = spin_exec;
  ot->invoke = spin_invoke;
  ot->poll = ED_operator_editsurf;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float_vector_xyz(ot->srna,
                           "center",
                           3,
                           nullptr,
                           -OBJECT_ADD_SIZE_MAXF,
                           OBJECT_ADD_SIZE_MAXF,
                           "Center",
                           "Center in global view space",
                           -1000.0f,
                           1000.0f);
  RNA_def_float_vector(
      ot->srna, "axis", 3, nullptr, -1.0f, 1.0f, "Axis", "Axis in global view space", -1.0f, 1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extrude Vertex Operator
 * \{ */

static bool ed_editcurve_extrude(Curve *cu, EditNurb *editnurb, View3D *v3d)
{
  bool changed = false;

  Nurb *cu_actnu;
  union {
    BezTriple *bezt;
    BPoint *bp;
    void *p;
  } cu_actvert;

  if (BLI_listbase_is_empty(&editnurb->nurbs)) {
    return changed;
  }

  BKE_curve_nurb_vert_active_get(cu, &cu_actnu, &cu_actvert.p);
  int act_offset = 0;

  LISTBASE_FOREACH (Nurb *, nu, &editnurb->nurbs) {
    BLI_assert(nu->pntsu > 0);
    int i;
    int pnt_len = nu->pntsu;
    int new_points = 0;
    int offset = 0;
    bool is_prev_selected = false;
    bool duplic_first = false;
    bool duplic_last = false;
    if (nu->type == CU_BEZIER) {
      BezTriple *bezt, *bezt_prev = nullptr;
      BezTriple bezt_stack;
      bool is_cyclic = false;
      if (pnt_len == 1) {
        /* Single point extrusion.
         * Keep `is_prev_selected` false to force extrude. */
        bezt_prev = &nu->bezt[0];
      }
      else if (nu->flagu & CU_NURB_CYCLIC) {
        is_cyclic = true;
        bezt_prev = &nu->bezt[pnt_len - 1];
        is_prev_selected = BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt_prev);
      }
      else {
        duplic_first = BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, &nu->bezt[0]) &&
                       BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, &nu->bezt[1]);

        duplic_last = BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, &nu->bezt[pnt_len - 2]) &&
                      BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, &nu->bezt[pnt_len - 1]);

        if (duplic_first) {
          bezt_stack = nu->bezt[0];
          BEZT_DESEL_ALL(&bezt_stack);
          bezt_prev = &bezt_stack;
        }
        if (duplic_last) {
          new_points++;
        }
      }
      i = pnt_len;
      for (bezt = &nu->bezt[0]; i--; bezt++) {
        bool is_selected = BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt);
        if (bezt_prev && is_prev_selected != is_selected) {
          new_points++;
        }
        if (bezt == cu_actvert.bezt) {
          act_offset = new_points;
        }
        bezt_prev = bezt;
        is_prev_selected = is_selected;
      }

      if (new_points) {
        if (pnt_len == 1) {
          /* Single point extrusion.
           * Set `is_prev_selected` as false to force extrude. */
          BLI_assert(bezt_prev == &nu->bezt[0]);
          is_prev_selected = false;
        }
        else if (is_cyclic) {
          BLI_assert(bezt_prev == &nu->bezt[pnt_len - 1]);
          BLI_assert(is_prev_selected == BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt_prev));
        }
        else if (duplic_first) {
          bezt_prev = &bezt_stack;
          is_prev_selected = false;
        }
        else {
          bezt_prev = nullptr;
        }
        BezTriple *bezt_src, *bezt_dst, *bezt_src_iter, *bezt_dst_iter;
        const int new_len = pnt_len + new_points;

        bezt_src = nu->bezt;
        bezt_dst = MEM_malloc_arrayN<BezTriple>(new_len, __func__);
        bezt_src_iter = &bezt_src[0];
        bezt_dst_iter = &bezt_dst[0];
        i = 0;
        for (bezt = &nu->bezt[0]; i < pnt_len; i++, bezt++) {
          bool is_selected = BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt);
          /* While this gets de-selected, selecting here ensures newly created verts are selected.
           * without this, the vertices are copied but only the handles are transformed.
           * which seems buggy from a user perspective. */
          if (is_selected) {
            bezt->f2 |= SELECT;
          }
          if (bezt_prev && is_prev_selected != is_selected) {
            int count = i - offset + 1;
            if (is_prev_selected) {
              ED_curve_beztcpy(editnurb, bezt_dst_iter, bezt_src_iter, count - 1);
              ED_curve_beztcpy(editnurb, &bezt_dst_iter[count - 1], bezt_prev, 1);
            }
            else {
              ED_curve_beztcpy(editnurb, bezt_dst_iter, bezt_src_iter, count);
            }
            ED_curve_beztcpy(editnurb, &bezt_dst_iter[count], bezt, 1);
            BEZT_DESEL_ALL(&bezt_dst_iter[count - 1]);

            bezt_dst_iter += count + 1;
            bezt_src_iter += count;
            offset = i + 1;
          }
          bezt_prev = bezt;
          is_prev_selected = is_selected;
        }

        int remain = pnt_len - offset;
        if (remain) {
          ED_curve_beztcpy(editnurb, bezt_dst_iter, bezt_src_iter, remain);
        }

        if (duplic_last) {
          ED_curve_beztcpy(editnurb, &bezt_dst[new_len - 1], &bezt_src[pnt_len - 1], 1);
          BEZT_DESEL_ALL(&bezt_dst[new_len - 1]);
        }

        MEM_freeN(nu->bezt);
        nu->bezt = bezt_dst;
        nu->pntsu += new_points;
        changed = true;
      }
    }
    else {
      BPoint *bp, *bp_prev = nullptr;
      BPoint bp_stack;
      if (pnt_len == 1) {
        /* Single point extrusion.
         * Reference a `prev_bp` to force extrude. */
        bp_prev = &nu->bp[0];
      }
      else {
        duplic_first = (nu->bp[0].f1 & SELECT) && (nu->bp[1].f1 & SELECT);
        duplic_last = (nu->bp[pnt_len - 2].f1 & SELECT) && (nu->bp[pnt_len - 1].f1 & SELECT);
        if (duplic_first) {
          bp_stack = nu->bp[0];
          bp_stack.f1 &= ~SELECT;
          bp_prev = &bp_stack;
        }
        if (duplic_last) {
          new_points++;
        }
      }

      i = pnt_len;
      for (bp = &nu->bp[0]; i--; bp++) {
        bool is_selected = (bp->f1 & SELECT) != 0;
        if (bp_prev && is_prev_selected != is_selected) {
          new_points++;
        }
        if (bp == cu_actvert.bp) {
          act_offset = new_points;
        }
        bp_prev = bp;
        is_prev_selected = is_selected;
      }

      if (new_points) {
        BPoint *bp_src, *bp_dst, *bp_src_iter, *bp_dst_iter;
        const int new_len = pnt_len + new_points;

        is_prev_selected = false;
        if (pnt_len == 1) {
          /* Single point extrusion.
           * Keep `is_prev_selected` false to force extrude. */
          BLI_assert(bp_prev == &nu->bp[0]);
        }
        else if (duplic_first) {
          bp_prev = &bp_stack;
          is_prev_selected = false;
        }
        else {
          bp_prev = nullptr;
        }
        bp_src = nu->bp;
        bp_dst = MEM_malloc_arrayN<BPoint>(new_len, __func__);
        bp_src_iter = &bp_src[0];
        bp_dst_iter = &bp_dst[0];
        i = 0;
        for (bp = &nu->bp[0]; i < pnt_len; i++, bp++) {
          bool is_selected = (bp->f1 & SELECT) != 0;
          if (bp_prev && is_prev_selected != is_selected) {
            int count = i - offset + 1;
            if (is_prev_selected) {
              ED_curve_bpcpy(editnurb, bp_dst_iter, bp_src_iter, count - 1);
              ED_curve_bpcpy(editnurb, &bp_dst_iter[count - 1], bp_prev, 1);
            }
            else {
              ED_curve_bpcpy(editnurb, bp_dst_iter, bp_src_iter, count);
            }
            ED_curve_bpcpy(editnurb, &bp_dst_iter[count], bp, 1);
            bp_dst_iter[count - 1].f1 &= ~SELECT;

            bp_dst_iter += count + 1;
            bp_src_iter += count;
            offset = i + 1;
          }
          bp_prev = bp;
          is_prev_selected = is_selected;
        }

        int remain = pnt_len - offset;
        if (remain) {
          ED_curve_bpcpy(editnurb, bp_dst_iter, bp_src_iter, remain);
        }

        if (duplic_last) {
          ED_curve_bpcpy(editnurb, &bp_dst[new_len - 1], &bp_src[pnt_len - 1], 1);
          bp_dst[new_len - 1].f1 &= ~SELECT;
        }

        MEM_freeN(nu->bp);
        nu->bp = bp_dst;
        nu->pntsu += new_points;

        BKE_nurb_knot_calc_u(nu);
        changed = true;
      }
    }
  }

  cu->actvert += act_offset;

  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Vertex Operator
 * \{ */

int ed_editcurve_addvert(Curve *cu, EditNurb *editnurb, View3D *v3d, const float location_init[3])
{
  float center[3];
  float temp[3];
  uint verts_len;
  bool changed = false;

  zero_v3(center);
  verts_len = 0;

  LISTBASE_FOREACH (Nurb *, nu, &editnurb->nurbs) {
    int i;
    if (nu->type == CU_BEZIER) {
      BezTriple *bezt;

      for (i = 0, bezt = nu->bezt; i < nu->pntsu; i++, bezt++) {
        if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
          add_v3_v3(center, bezt->vec[1]);
          verts_len += 1;
        }
      }
    }
    else {
      BPoint *bp;

      for (i = 0, bp = nu->bp; i < nu->pntsu; i++, bp++) {
        if (bp->f1 & SELECT) {
          add_v3_v3(center, bp->vec);
          verts_len += 1;
        }
      }
    }
  }

  if (verts_len && ed_editcurve_extrude(cu, editnurb, v3d)) {
    float ofs[3];
    int i;

    mul_v3_fl(center, 1.0f / float(verts_len));
    sub_v3_v3v3(ofs, location_init, center);

    if (CU_IS_2D(cu)) {
      ofs[2] = 0.0f;
    }

    LISTBASE_FOREACH (Nurb *, nu, &editnurb->nurbs) {
      if (nu->type == CU_BEZIER) {
        BezTriple *bezt;
        for (i = 0, bezt = nu->bezt; i < nu->pntsu; i++, bezt++) {
          if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
            add_v3_v3(bezt->vec[0], ofs);
            add_v3_v3(bezt->vec[1], ofs);
            add_v3_v3(bezt->vec[2], ofs);

            if (((nu->flagu & CU_NURB_CYCLIC) == 0) && ELEM(i, 0, nu->pntsu - 1)) {
              BKE_nurb_handle_calc_simple_auto(nu, bezt);
            }
          }
        }

        BKE_nurb_handles_calc(nu);
      }
      else {
        BPoint *bp;

        for (i = 0, bp = nu->bp; i < nu->pntsu; i++, bp++) {
          if (bp->f1 & SELECT) {
            add_v3_v3(bp->vec, ofs);
          }
        }
      }
    }
    changed = true;
  }
  else {
    float location[3];

    copy_v3_v3(location, location_init);

    if (CU_IS_2D(cu)) {
      location[2] = 0.0f;
    }

    /* nothing selected: create a new curve */
    Nurb *nu = BKE_curve_nurb_active_get(cu);

    Nurb *nurb_new;
    if (!nu) {
      /* Bezier as default. */
      nurb_new = MEM_callocN<Nurb>("BLI_editcurve_addvert new_bezt_nurb 2");
      nurb_new->type = CU_BEZIER;
      nurb_new->resolu = cu->resolu;
      nurb_new->orderu = 4;
      nurb_new->flag |= CU_SMOOTH;
      BKE_nurb_bezierPoints_add(nurb_new, 1);
    }
    else {
      /* Copy the active nurb settings. */
      nurb_new = BKE_nurb_copy(nu, 1, 1);
      if (nu->bezt) {
        memcpy(nurb_new->bezt, nu->bezt, sizeof(BezTriple));
      }
      else {
        memcpy(nurb_new->bp, nu->bp, sizeof(BPoint));
      }
    }

    if (nurb_new->type == CU_BEZIER) {
      BezTriple *bezt_new = nurb_new->bezt;

      BEZT_SEL_ALL(bezt_new);

      bezt_new->h1 = HD_AUTO;
      bezt_new->h2 = HD_AUTO;

      temp[0] = 1.0f;
      temp[1] = 0.0f;
      temp[2] = 0.0f;

      copy_v3_v3(bezt_new->vec[1], location);
      sub_v3_v3v3(bezt_new->vec[0], location, temp);
      add_v3_v3v3(bezt_new->vec[2], location, temp);
    }
    else {
      BPoint *bp_new = nurb_new->bp;

      bp_new->f1 |= SELECT;

      copy_v3_v3(bp_new->vec, location);

      BKE_nurb_knot_calc_u(nurb_new);
    }

    BLI_addtail(&editnurb->nurbs, nurb_new);
    changed = true;
  }

  return changed;
}

static wmOperatorStatus add_vertex_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *obedit = CTX_data_edit_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  Curve *cu = static_cast<Curve *>(obedit->data);
  EditNurb *editnurb = cu->editnurb;
  float location[3];
  float imat[4][4];

  RNA_float_get_array(op->ptr, "location", location);

  invert_m4_m4(imat, obedit->object_to_world().ptr());
  mul_m4_v3(imat, location);

  if (ed_editcurve_addvert(cu, editnurb, v3d, location)) {
    if (ED_curve_updateAnimPaths(bmain, static_cast<Curve *>(obedit->data))) {
      WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);
    }

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus add_vertex_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  if (vc.rv3d && !RNA_struct_property_is_set(op->ptr, "location")) {
    Curve *cu;
    float location[3];
    const bool use_proj = ((vc.scene->toolsettings->snap_flag & SCE_SNAP) &&
                           (vc.scene->toolsettings->snap_mode &
                            (SCE_SNAP_TO_FACE | SCE_SNAP_INDIVIDUAL_PROJECT)));

    Nurb *nu;
    BezTriple *bezt;
    BPoint *bp;

    cu = static_cast<Curve *>(vc.obedit->data);

    ED_curve_nurb_vert_selected_find(cu, vc.v3d, &nu, &bezt, &bp);

    if (bezt) {
      mul_v3_m4v3(location, vc.obedit->object_to_world().ptr(), bezt->vec[1]);
    }
    else if (bp) {
      mul_v3_m4v3(location, vc.obedit->object_to_world().ptr(), bp->vec);
    }
    else {
      copy_v3_v3(location, vc.scene->cursor.location);
    }

    ED_view3d_win_to_3d_int(vc.v3d, vc.region, location, event->mval, location);

    if (use_proj) {
      const float mval[2] = {float(event->mval[0]), float(event->mval[1])};

      blender::ed::transform::SnapObjectContext *snap_context =
          blender::ed::transform::snap_object_context_create(vc.scene, 0);

      blender::ed::transform::SnapObjectParams params{};
      params.snap_target_select = (vc.obedit != nullptr) ? SCE_SNAP_TARGET_NOT_ACTIVE :
                                                           SCE_SNAP_TARGET_ALL;
      params.edit_mode_type = blender::ed::transform::SNAP_GEOM_FINAL;
      blender::ed::transform::snap_object_project_view3d(snap_context,
                                                         vc.depsgraph,
                                                         vc.region,
                                                         vc.v3d,
                                                         SCE_SNAP_TO_FACE,
                                                         &params,
                                                         nullptr,
                                                         mval,
                                                         nullptr,
                                                         nullptr,
                                                         location,
                                                         nullptr);

      blender::ed::transform::snap_object_context_destroy(snap_context);
    }

    if (CU_IS_2D(cu)) {
      const float eps = 1e-6f;

      /* get the view vector to 'location' */
      float view_dir[3];
      ED_view3d_global_to_vector(vc.rv3d, location, view_dir);

      /* get the plane */
      const float *plane_co = vc.obedit->object_to_world().location();
      float plane_no[3];
      /* only normalize to avoid precision errors */
      normalize_v3_v3(plane_no, vc.obedit->object_to_world()[2]);

      if (fabsf(dot_v3v3(view_dir, plane_no)) < eps) {
        /* can't project on an aligned plane. */
      }
      else {
        float lambda;
        if (isect_ray_plane_v3_factor(location, view_dir, plane_co, plane_no, &lambda)) {
          /* check if we're behind the viewport */
          float location_test[3];
          madd_v3_v3v3fl(location_test, location, view_dir, lambda);
          if ((vc.rv3d->is_persp == false) ||
              (mul_project_m4_v3_zfac(vc.rv3d->persmat, location_test) > 0.0f))
          {
            copy_v3_v3(location, location_test);
          }
        }
      }
    }

    RNA_float_set_array(op->ptr, "location", location);
  }

  /* Support dragging to move after extrude, see: #114282. */
  wmOperatorStatus retval = add_vertex_exec(C, op);
  if (retval & OPERATOR_FINISHED) {
    retval |= OPERATOR_PASS_THROUGH;
  }
  return WM_operator_flag_only_pass_through_on_press(retval, event);
}

void CURVE_OT_vertex_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extrude to Cursor or Add";
  ot->idname = "CURVE_OT_vertex_add";
  ot->description = "Add a new control point (linked to only selected end-curve one, if any)";

  /* API callbacks. */
  ot->exec = add_vertex_exec;
  ot->invoke = add_vertex_invoke;
  ot->poll = ED_operator_editcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  RNA_def_float_vector_xyz(ot->srna,
                           "location",
                           3,
                           nullptr,
                           -OBJECT_ADD_SIZE_MAXF,
                           OBJECT_ADD_SIZE_MAXF,
                           "Location",
                           "Location to add new vertex at",
                           -1.0e4f,
                           1.0e4f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extrude Operator
 * \{ */

static wmOperatorStatus curve_extrude_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Curve *cu = static_cast<Curve *>(obedit->data);
    EditNurb *editnurb = cu->editnurb;
    bool changed = false;

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    if (obedit->type == OB_CURVES_LEGACY) {
      changed = ed_editcurve_extrude(cu, editnurb, v3d);
    }
    else {
      changed = ed_editnurb_extrude_flag(editnurb, SELECT);
    }

    if (changed) {
      if (ED_curve_updateAnimPaths(bmain, static_cast<Curve *>(obedit->data))) {
        WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);
      }

      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
      DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
    }
  }
  return OPERATOR_FINISHED;
}

void CURVE_OT_extrude(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extrude";
  ot->description = "Extrude selected control point(s)";
  ot->idname = "CURVE_OT_extrude";

  /* API callbacks. */
  ot->exec = curve_extrude_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* to give to transform */
  RNA_def_enum(ot->srna,
               "mode",
               rna_enum_transform_mode_type_items,
               blender::ed::transform::TFM_TRANSLATION,
               "Mode",
               "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Cyclic Operator
 * \{ */

bool curve_toggle_cyclic(View3D *v3d, ListBase *editnurb, int direction)
{
  BezTriple *bezt;
  BPoint *bp;
  int a;
  bool changed = false;

  LISTBASE_FOREACH (Nurb *, nu, editnurb) {
    if (nu->pntsu > 1 || nu->pntsv > 1) {
      if (nu->type == CU_POLY) {
        a = nu->pntsu;
        bp = nu->bp;
        while (a--) {
          if (bp->f1 & SELECT) {
            nu->flagu ^= CU_NURB_CYCLIC;
            changed = true;
            break;
          }
          bp++;
        }
      }
      else if (nu->type == CU_BEZIER) {
        a = nu->pntsu;
        bezt = nu->bezt;
        while (a--) {
          if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
            nu->flagu ^= CU_NURB_CYCLIC;
            changed = true;
            break;
          }
          bezt++;
        }
        BKE_nurb_handles_calc(nu);
      }
      else if (nu->pntsv == 1 && nu->type == CU_NURBS) {
        if (nu->knotsu) { /* if check_valid_nurb_u fails the knotsu can be nullptr */
          a = nu->pntsu;
          bp = nu->bp;
          while (a--) {
            if (bp->f1 & SELECT) {
              nu->flagu ^= CU_NURB_CYCLIC;
              /* 1==u  type is ignored for cyclic curves */
              BKE_nurb_knot_calc_u(nu);
              changed = true;
              break;
            }
            bp++;
          }
        }
      }
      else if (nu->type == CU_NURBS) {
        a = nu->pntsu * nu->pntsv;
        bp = nu->bp;
        while (a--) {

          if (bp->f1 & SELECT) {
            if (direction == 0 && nu->pntsu > 1) {
              nu->flagu ^= CU_NURB_CYCLIC;
              /* 1==u  type is ignored for cyclic curves */
              BKE_nurb_knot_calc_u(nu);
              changed = true;
            }
            if (direction == 1 && nu->pntsv > 1) {
              nu->flagv ^= CU_NURB_CYCLIC;
              /* 2==v  type is ignored for cyclic curves */
              BKE_nurb_knot_calc_v(nu);
              changed = true;
            }
            break;
          }
          bp++;
        }
      }
    }
  }
  return changed;
}

static wmOperatorStatus toggle_cyclic_exec(bContext *C, wmOperator *op)
{
  const int direction = RNA_enum_get(op->ptr, "direction");
  View3D *v3d = CTX_wm_view3d(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  bool changed_multi = false;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Curve *cu = static_cast<Curve *>(obedit->data);

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    ListBase *editnurb = object_editcurve_get(obedit);
    if (curve_toggle_cyclic(v3d, editnurb, direction)) {
      changed_multi = true;
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
      DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
    }
  }

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static wmOperatorStatus toggle_cyclic_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent * /*event*/)
{
  Object *obedit = CTX_data_edit_object(C);
  ListBase *editnurb = object_editcurve_get(obedit);
  uiPopupMenu *pup;
  uiLayout *layout;

  if (obedit->type == OB_SURF) {
    LISTBASE_FOREACH (Nurb *, nu, editnurb) {
      if (nu->pntsu > 1 || nu->pntsv > 1) {
        if (nu->type == CU_NURBS) {
          pup = UI_popup_menu_begin(C, IFACE_("Direction"), ICON_NONE);
          layout = UI_popup_menu_layout(pup);
          layout->op_enum(op->type->idname, "direction");
          UI_popup_menu_end(C, pup);
          return OPERATOR_INTERFACE;
        }
      }
    }
  }

  return toggle_cyclic_exec(C, op);
}

void CURVE_OT_cyclic_toggle(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {0, "CYCLIC_U", 0, "Cyclic U", ""},
      {1, "CYCLIC_V", 0, "Cyclic V", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Toggle Cyclic";
  ot->description = "Make active spline closed/opened loop";
  ot->idname = "CURVE_OT_cyclic_toggle";

  /* API callbacks. */
  ot->exec = toggle_cyclic_exec;
  ot->invoke = toggle_cyclic_invoke;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna,
               "direction",
               direction_items,
               0,
               "Direction",
               "Direction to make surface cyclic in");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Duplicate Operator
 * \{ */

static wmOperatorStatus duplicate_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  bool changed = false;
  int count_failed = 0;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Curve *cu = static_cast<Curve *>(obedit->data);

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    ListBase newnurb = {nullptr, nullptr};
    adduplicateflagNurb(obedit, v3d, &newnurb, SELECT, false);

    if (BLI_listbase_is_empty(&newnurb)) {
      count_failed += 1;
      continue;
    }

    changed = true;
    BLI_movelisttolist(object_editcurve_get(obedit), &newnurb);
    DEG_id_tag_update(&cu->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, &cu->id);
  }

  if (changed == false) {
    if (count_failed != 0) {
      BKE_report(op->reports, RPT_ERROR, "Cannot duplicate current selection");
    }
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

void CURVE_OT_duplicate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Curve";
  ot->description = "Duplicate selected control points";
  ot->idname = "CURVE_OT_duplicate";

  /* API callbacks. */
  ot->exec = duplicate_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Operator
 * \{ */

static bool curve_delete_vertices(Object *obedit, View3D *v3d)
{
  if (obedit->type == OB_SURF) {
    ed_surf_delete_selected(obedit);
  }
  else {
    ed_curve_delete_selected(obedit, v3d);
  }

  return true;
}

static bool curve_delete_segments(Object *obedit, View3D *v3d, const bool split)
{
  Curve *cu = static_cast<Curve *>(obedit->data);
  EditNurb *editnurb = cu->editnurb;
  ListBase *nubase = &editnurb->nurbs, newnurb = {nullptr, nullptr};
  Nurb *nu1;
  BezTriple *bezt, *bezt1, *bezt2;
  BPoint *bp, *bp1, *bp2;
  int a, b, starta, enda, cut, cyclicut;

  LISTBASE_FOREACH (Nurb *, nu, nubase) {
    nu1 = nullptr;
    starta = enda = cut = -1;
    cyclicut = 0;

    if (nu->type == CU_BEZIER) {
      for (a = 0, bezt = nu->bezt; a < nu->pntsu; a++, bezt++) {
        if (!BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
          enda = a;
          if (starta == -1) {
            starta = a;
          }
          if (a < nu->pntsu - 1) {
            continue;
          }
        }
        else if (a < nu->pntsu - 1 && !BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt + 1)) {
          /* if just single selected point then continue */
          continue;
        }

        if (starta >= 0) {
          /* got selected segment, now check where and copy */
          if (starta <= 1 && a == nu->pntsu - 1) {
            /* copying all points in spline */
            if (starta == 1 && enda != a) {
              nu->flagu &= ~CU_NURB_CYCLIC;
            }

            starta = 0;
            enda = a;
            cut = enda - starta + 1;
            nu1 = BKE_nurb_copy(nu, cut, 1);
          }
          else if (starta == 0) {
            /* if start of curve copy next end point */
            enda++;
            cut = enda - starta + 1;
            bezt1 = &nu->bezt[nu->pntsu - 1];
            bezt2 = &nu->bezt[nu->pntsu - 2];

            if ((nu->flagu & CU_NURB_CYCLIC) && BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt1) &&
                BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt2))
            {
              /* check if need to join start of spline to end */
              nu1 = BKE_nurb_copy(nu, cut + 1, 1);
              ED_curve_beztcpy(editnurb, &nu1->bezt[1], nu->bezt, cut);
              starta = nu->pntsu - 1;
              cut = 1;
            }
            else {
              if (nu->flagu & CU_NURB_CYCLIC) {
                cyclicut = cut;
              }
              else {
                nu1 = BKE_nurb_copy(nu, cut, 1);
              }
            }
          }
          else if (enda == nu->pntsu - 1) {
            /* if end of curve copy previous start point */
            starta--;
            cut = enda - starta + 1;
            bezt1 = nu->bezt;
            bezt2 = &nu->bezt[1];

            if ((nu->flagu & CU_NURB_CYCLIC) && BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt1) &&
                BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt2))
            {
              /* check if need to join start of spline to end */
              nu1 = BKE_nurb_copy(nu, cut + 1, 1);
              ED_curve_beztcpy(editnurb, &nu1->bezt[cut], nu->bezt, 1);
            }
            else if (cyclicut != 0) {
              /* if cyclicut exists it is a cyclic spline, start and end should be connected */
              nu1 = BKE_nurb_copy(nu, cut + cyclicut, 1);
              ED_curve_beztcpy(editnurb, &nu1->bezt[cut], nu->bezt, cyclicut);
              cyclicut = 0;
            }
            else {
              nu1 = BKE_nurb_copy(nu, cut, 1);
            }
          }
          else {
            /* mid spline selection, copy adjacent start and end */
            starta--;
            enda++;
            cut = enda - starta + 1;
            nu1 = BKE_nurb_copy(nu, cut, 1);
          }

          if (nu1 != nullptr) {
            ED_curve_beztcpy(editnurb, nu1->bezt, &nu->bezt[starta], cut);
            BLI_addtail(&newnurb, nu1);

            if (starta != 0 || enda != nu->pntsu - 1) {
              nu1->flagu &= ~CU_NURB_CYCLIC;
            }
            nu1 = nullptr;
          }
          starta = enda = -1;
        }
      }

      if (!split && cut != -1 && nu->pntsu > 2 && !(nu->flagu & CU_NURB_CYCLIC)) {
        /* start and points copied if connecting segment was deleted and not cyclic spline */
        bezt1 = nu->bezt;
        bezt2 = &nu->bezt[1];

        if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt1) && BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt2)) {
          nu1 = BKE_nurb_copy(nu, 1, 1);
          ED_curve_beztcpy(editnurb, nu1->bezt, bezt1, 1);
          BLI_addtail(&newnurb, nu1);
        }

        bezt1 = &nu->bezt[nu->pntsu - 1];
        bezt2 = &nu->bezt[nu->pntsu - 2];

        if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt1) && BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt2)) {
          nu1 = BKE_nurb_copy(nu, 1, 1);
          ED_curve_beztcpy(editnurb, nu1->bezt, bezt1, 1);
          BLI_addtail(&newnurb, nu1);
        }
      }
    }
    else if (nu->pntsv >= 1) {
      int u, v;

      if (isNurbselV(nu, &u, SELECT)) {
        for (a = 0, bp = nu->bp; a < nu->pntsu; a++, bp++) {
          if (!(bp->f1 & SELECT)) {
            enda = a;
            if (starta == -1) {
              starta = a;
            }
            if (a < nu->pntsu - 1) {
              continue;
            }
          }
          else if (a < nu->pntsu - 1 && !((bp + 1)->f1 & SELECT)) {
            /* if just single selected point then continue */
            continue;
          }

          if (starta >= 0) {
            /* got selected segment, now check where and copy */
            if (starta <= 1 && a == nu->pntsu - 1) {
              /* copying all points in spline */
              if (starta == 1 && enda != a) {
                nu->flagu &= ~CU_NURB_CYCLIC;
              }

              starta = 0;
              enda = a;
              cut = enda - starta + 1;
              nu1 = BKE_nurb_copy(nu, cut, nu->pntsv);
            }
            else if (starta == 0) {
              /* if start of curve copy next end point */
              enda++;
              cut = enda - starta + 1;
              bp1 = &nu->bp[nu->pntsu - 1];
              bp2 = &nu->bp[nu->pntsu - 2];

              if ((nu->flagu & CU_NURB_CYCLIC) && (bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
                /* check if need to join start of spline to end */
                nu1 = BKE_nurb_copy(nu, cut + 1, nu->pntsv);
                for (b = 0; b < nu->pntsv; b++) {
                  ED_curve_bpcpy(
                      editnurb, &nu1->bp[b * nu1->pntsu + 1], &nu->bp[b * nu->pntsu], cut);
                }
                starta = nu->pntsu - 1;
                cut = 1;
              }
              else {
                if (nu->flagu & CU_NURB_CYCLIC) {
                  cyclicut = cut;
                }
                else {
                  nu1 = BKE_nurb_copy(nu, cut, nu->pntsv);
                }
              }
            }
            else if (enda == nu->pntsu - 1) {
              /* if end of curve copy previous start point */
              starta--;
              cut = enda - starta + 1;
              bp1 = nu->bp;
              bp2 = &nu->bp[1];

              if ((nu->flagu & CU_NURB_CYCLIC) && (bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
                /* check if need to join start of spline to end */
                nu1 = BKE_nurb_copy(nu, cut + 1, nu->pntsv);
                for (b = 0; b < nu->pntsv; b++) {
                  ED_curve_bpcpy(
                      editnurb, &nu1->bp[b * nu1->pntsu + cut], &nu->bp[b * nu->pntsu], 1);
                }
              }
              else if (cyclicut != 0) {
                /* if cyclicut exists it is a cyclic spline, start and end should be connected */
                nu1 = BKE_nurb_copy(nu, cut + cyclicut, nu->pntsv);
                for (b = 0; b < nu->pntsv; b++) {
                  ED_curve_bpcpy(
                      editnurb, &nu1->bp[b * nu1->pntsu + cut], &nu->bp[b * nu->pntsu], cyclicut);
                }
              }
              else {
                nu1 = BKE_nurb_copy(nu, cut, nu->pntsv);
              }
            }
            else {
              /* mid spline selection, copy adjacent start and end */
              starta--;
              enda++;
              cut = enda - starta + 1;
              nu1 = BKE_nurb_copy(nu, cut, nu->pntsv);
            }

            if (nu1 != nullptr) {
              for (b = 0; b < nu->pntsv; b++) {
                ED_curve_bpcpy(
                    editnurb, &nu1->bp[b * nu1->pntsu], &nu->bp[b * nu->pntsu + starta], cut);
              }
              BLI_addtail(&newnurb, nu1);

              if (starta != 0 || enda != nu->pntsu - 1) {
                nu1->flagu &= ~CU_NURB_CYCLIC;
              }
              nu1 = nullptr;
            }
            starta = enda = -1;
          }
        }

        if (!split && cut != -1 && nu->pntsu > 2 && !(nu->flagu & CU_NURB_CYCLIC)) {
          /* start and points copied if connecting segment was deleted and not cyclic spline */
          bp1 = nu->bp;
          bp2 = &nu->bp[1];

          if ((bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
            nu1 = BKE_nurb_copy(nu, 1, nu->pntsv);
            for (b = 0; b < nu->pntsv; b++) {
              ED_curve_bpcpy(editnurb, &nu1->bp[b], &nu->bp[b * nu->pntsu], 1);
            }
            BLI_addtail(&newnurb, nu1);
          }

          bp1 = &nu->bp[nu->pntsu - 1];
          bp2 = &nu->bp[nu->pntsu - 2];

          if ((bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
            nu1 = BKE_nurb_copy(nu, 1, nu->pntsv);
            for (b = 0; b < nu->pntsv; b++) {
              ED_curve_bpcpy(editnurb, &nu1->bp[b], &nu->bp[b * nu->pntsu + nu->pntsu - 1], 1);
            }
            BLI_addtail(&newnurb, nu1);
          }
        }
      }
      else if (isNurbselU(nu, &v, SELECT)) {
        for (a = 0, bp = nu->bp; a < nu->pntsv; a++, bp += nu->pntsu) {
          if (!(bp->f1 & SELECT)) {
            enda = a;
            if (starta == -1) {
              starta = a;
            }
            if (a < nu->pntsv - 1) {
              continue;
            }
          }
          else if (a < nu->pntsv - 1 && !((bp + nu->pntsu)->f1 & SELECT)) {
            /* if just single selected point then continue */
            continue;
          }

          if (starta >= 0) {
            /* got selected segment, now check where and copy */
            if (starta <= 1 && a == nu->pntsv - 1) {
              /* copying all points in spline */
              if (starta == 1 && enda != a) {
                nu->flagv &= ~CU_NURB_CYCLIC;
              }

              starta = 0;
              enda = a;
              cut = enda - starta + 1;
              nu1 = BKE_nurb_copy(nu, nu->pntsu, cut);
            }
            else if (starta == 0) {
              /* if start of curve copy next end point */
              enda++;
              cut = enda - starta + 1;
              bp1 = &nu->bp[nu->pntsv * nu->pntsu - nu->pntsu];
              bp2 = &nu->bp[nu->pntsv * nu->pntsu - (nu->pntsu * 2)];

              if ((nu->flagv & CU_NURB_CYCLIC) && (bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
                /* check if need to join start of spline to end */
                nu1 = BKE_nurb_copy(nu, nu->pntsu, cut + 1);
                ED_curve_bpcpy(editnurb, &nu1->bp[nu->pntsu], nu->bp, cut * nu->pntsu);
                starta = nu->pntsv - 1;
                cut = 1;
              }
              else {
                if (nu->flagv & CU_NURB_CYCLIC) {
                  cyclicut = cut;
                }
                else {
                  nu1 = BKE_nurb_copy(nu, nu->pntsu, cut);
                }
              }
            }
            else if (enda == nu->pntsv - 1) {
              /* if end of curve copy previous start point */
              starta--;
              cut = enda - starta + 1;
              bp1 = nu->bp;
              bp2 = &nu->bp[nu->pntsu];

              if ((nu->flagv & CU_NURB_CYCLIC) && (bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
                /* check if need to join start of spline to end */
                nu1 = BKE_nurb_copy(nu, nu->pntsu, cut + 1);
                ED_curve_bpcpy(editnurb, &nu1->bp[cut * nu->pntsu], nu->bp, nu->pntsu);
              }
              else if (cyclicut != 0) {
                /* if cyclicut exists it is a cyclic spline, start and end should be connected */
                nu1 = BKE_nurb_copy(nu, nu->pntsu, cut + cyclicut);
                ED_curve_bpcpy(editnurb, &nu1->bp[cut * nu->pntsu], nu->bp, nu->pntsu * cyclicut);
                cyclicut = 0;
              }
              else {
                nu1 = BKE_nurb_copy(nu, nu->pntsu, cut);
              }
            }
            else {
              /* mid spline selection, copy adjacent start and end */
              starta--;
              enda++;
              cut = enda - starta + 1;
              nu1 = BKE_nurb_copy(nu, nu->pntsu, cut);
            }

            if (nu1 != nullptr) {
              ED_curve_bpcpy(editnurb, nu1->bp, &nu->bp[starta * nu->pntsu], cut * nu->pntsu);
              BLI_addtail(&newnurb, nu1);

              if (starta != 0 || enda != nu->pntsv - 1) {
                nu1->flagv &= ~CU_NURB_CYCLIC;
              }
              nu1 = nullptr;
            }
            starta = enda = -1;
          }
        }

        if (!split && cut != -1 && nu->pntsv > 2 && !(nu->flagv & CU_NURB_CYCLIC)) {
          /* start and points copied if connecting segment was deleted and not cyclic spline */
          bp1 = nu->bp;
          bp2 = &nu->bp[nu->pntsu];

          if ((bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
            nu1 = BKE_nurb_copy(nu, nu->pntsu, 1);
            ED_curve_bpcpy(editnurb, nu1->bp, nu->bp, nu->pntsu);
            BLI_addtail(&newnurb, nu1);
          }

          bp1 = &nu->bp[nu->pntsu * nu->pntsv - nu->pntsu];
          bp2 = &nu->bp[nu->pntsu * nu->pntsv - (nu->pntsu * 2)];

          if ((bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
            nu1 = BKE_nurb_copy(nu, nu->pntsu, 1);
            ED_curve_bpcpy(
                editnurb, nu1->bp, &nu->bp[nu->pntsu * nu->pntsv - nu->pntsu], nu->pntsu);
            BLI_addtail(&newnurb, nu1);
          }
        }
      }
      else {
        /* selection not valid, just copy nurb to new list */
        nu1 = BKE_nurb_copy(nu, nu->pntsu, nu->pntsv);
        ED_curve_bpcpy(editnurb, nu1->bp, nu->bp, nu->pntsu * nu->pntsv);
        BLI_addtail(&newnurb, nu1);
      }
    }
  }

  LISTBASE_FOREACH (Nurb *, nu, &newnurb) {
    if (nu->type == CU_BEZIER) {
      if (split) {
        /* deselect for split operator */
        for (b = 0, bezt1 = nu->bezt; b < nu->pntsu; b++, bezt1++) {
          select_beztriple(bezt1, false, SELECT, eVisible_Types(true));
        }
      }

      BKE_nurb_handles_calc(nu);
    }
    else {
      if (split) {
        /* deselect for split operator */
        for (b = 0, bp1 = nu->bp; b < nu->pntsu * nu->pntsv; b++, bp1++) {
          select_bpoint(bp1, false, SELECT, HIDDEN);
        }
      }

      BKE_nurb_order_clamp_u(nu);
      BKE_nurb_knot_calc_u(nu);

      if (nu->pntsv > 1) {
        BKE_nurb_order_clamp_v(nu);
        BKE_nurb_knot_calc_v(nu);
      }
    }
  }

  keyIndex_delNurbList(editnurb, nubase);
  BKE_nurbList_free(nubase);
  BLI_movelisttolist(nubase, &newnurb);

  return true;
}

static wmOperatorStatus curve_delete_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  View3D *v3d = CTX_wm_view3d(C);
  eCurveElem_Types type = eCurveElem_Types(RNA_enum_get(op->ptr, "type"));
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  bool changed_multi = false;

  for (Object *obedit : objects) {
    Curve *cu = (Curve *)obedit->data;
    bool changed = false;

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    if (type == CURVE_VERTEX) {
      changed = curve_delete_vertices(obedit, v3d);
    }
    else if (type == CURVE_SEGMENT) {
      changed = curve_delete_segments(obedit, v3d, false);
      cu->actnu = CU_ACT_NONE;
    }
    else {
      BLI_assert(0);
    }

    if (changed) {
      changed_multi = true;
      cu->actvert = CU_ACT_NONE;

      if (ED_curve_updateAnimPaths(bmain, static_cast<Curve *>(obedit->data))) {
        WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);
      }

      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
      DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
    }
  }

  if (changed_multi) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static const EnumPropertyItem curve_delete_type_items[] = {
    {CURVE_VERTEX, "VERT", 0, "Vertices", ""},
    {CURVE_SEGMENT, "SEGMENT", 0, "Segments", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem *rna_curve_delete_type_itemf(bContext *C,
                                                           PointerRNA * /*ptr*/,
                                                           PropertyRNA * /*prop*/,
                                                           bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  int totitem = 0;

  if (!C) { /* needed for docs and i18n tools */
    return curve_delete_type_items;
  }

  RNA_enum_items_add_value(&item, &totitem, curve_delete_type_items, CURVE_VERTEX);
  RNA_enum_items_add_value(&item, &totitem, curve_delete_type_items, CURVE_SEGMENT);
  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

void CURVE_OT_delete(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Delete";
  ot->description = "Delete selected control points or segments";
  ot->idname = "CURVE_OT_delete";

  /* API callbacks. */
  ot->exec = curve_delete_exec;
  ot->invoke = WM_menu_invoke;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(
      ot->srna, "type", curve_delete_type_items, 0, "Type", "Which elements to delete");
  RNA_def_enum_funcs(prop, rna_curve_delete_type_itemf);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dissolve Vertices
 * \{ */

static bool test_bezt_is_sel_any(const void *bezt_v, void *user_data)
{
  View3D *v3d = static_cast<View3D *>(user_data);
  const BezTriple *bezt = static_cast<const BezTriple *>(bezt_v);
  return BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt);
}

void ed_dissolve_bez_segment(BezTriple *bezt_prev,
                             BezTriple *bezt_next,
                             const Nurb *nu,
                             const Curve *cu,
                             const uint span_len,
                             const uint span_step[2])
{
  int i_span_edge_len = span_len + 1;
  const int dims = 3;

  const int points_len = ((cu->resolu - 1) * i_span_edge_len) + 1;
  float *points = MEM_malloc_arrayN<float>(points_len * dims, __func__);
  float *points_stride = points;
  const int points_stride_len = (cu->resolu - 1);

  for (int segment = 0; segment < i_span_edge_len; segment++) {
    BezTriple *bezt_a = &nu->bezt[mod_i((span_step[0] + segment) - 1, nu->pntsu)];
    BezTriple *bezt_b = &nu->bezt[mod_i((span_step[0] + segment), nu->pntsu)];

    for (int axis = 0; axis < dims; axis++) {
      BKE_curve_forward_diff_bezier(bezt_a->vec[1][axis],
                                    bezt_a->vec[2][axis],
                                    bezt_b->vec[0][axis],
                                    bezt_b->vec[1][axis],
                                    points_stride + axis,
                                    points_stride_len,
                                    dims * sizeof(float));
    }

    points_stride += dims * points_stride_len;
  }

  BLI_assert(points_stride + dims == points + (points_len * dims));

  float tan_l[3], tan_r[3], error_sq_dummy;
  uint error_index_dummy;

  sub_v3_v3v3(tan_l, bezt_prev->vec[1], bezt_prev->vec[2]);
  normalize_v3(tan_l);
  sub_v3_v3v3(tan_r, bezt_next->vec[0], bezt_next->vec[1]);
  normalize_v3(tan_r);

  curve_fit_cubic_to_points_single_fl(points,
                                      points_len,
                                      nullptr,
                                      dims,
                                      FLT_EPSILON,
                                      tan_l,
                                      tan_r,
                                      bezt_prev->vec[2],
                                      bezt_next->vec[0],
                                      &error_sq_dummy,
                                      &error_index_dummy);

  if (!ELEM(bezt_prev->h2, HD_FREE, HD_ALIGN)) {
    bezt_prev->h2 = (bezt_prev->h2 == HD_VECT) ? HD_FREE : HD_ALIGN;
  }
  if (!ELEM(bezt_next->h1, HD_FREE, HD_ALIGN)) {
    bezt_next->h1 = (bezt_next->h1 == HD_VECT) ? HD_FREE : HD_ALIGN;
  }

  MEM_freeN(points);
}

static wmOperatorStatus curve_dissolve_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Curve *cu = (Curve *)obedit->data;

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    ListBase *editnurb = object_editcurve_get(obedit);

    LISTBASE_FOREACH (Nurb *, nu, editnurb) {
      if ((nu->type == CU_BEZIER) && (nu->pntsu > 2)) {
        uint span_step[2] = {uint(nu->pntsu), uint(nu->pntsu)};
        uint span_len;

        while (BLI_array_iter_span(nu->bezt,
                                   nu->pntsu,
                                   (nu->flagu & CU_NURB_CYCLIC) != 0,
                                   false,
                                   test_bezt_is_sel_any,
                                   v3d,
                                   span_step,
                                   &span_len))
        {
          BezTriple *bezt_prev = &nu->bezt[mod_i(span_step[0] - 1, nu->pntsu)];
          BezTriple *bezt_next = &nu->bezt[mod_i(span_step[1] + 1, nu->pntsu)];

          ed_dissolve_bez_segment(bezt_prev, bezt_next, nu, cu, span_len, span_step);
        }
      }
    }

    ed_curve_delete_selected(obedit, v3d);

    cu->actnu = cu->actvert = CU_ACT_NONE;

    if (ED_curve_updateAnimPaths(bmain, static_cast<Curve *>(obedit->data))) {
      WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);
    }

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }
  return OPERATOR_FINISHED;
}

void CURVE_OT_dissolve_verts(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Dissolve Vertices";
  ot->description = "Delete selected control points, correcting surrounding handles";
  ot->idname = "CURVE_OT_dissolve_verts";

  /* API callbacks. */
  ot->exec = curve_dissolve_exec;
  ot->poll = ED_operator_editcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Decimate Operator
 * \{ */

static bool nurb_bezt_flag_any(const Nurb *nu, const char flag_test)
{
  const BezTriple *bezt;
  int i;

  for (i = nu->pntsu, bezt = nu->bezt; i--; bezt++) {
    if (bezt->f2 & flag_test) {
      return true;
    }
  }

  return false;
}

static wmOperatorStatus curve_decimate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const float error_sq_max = FLT_MAX;
  float ratio = RNA_float_get(op->ptr, "ratio");
  bool all_supported_multi = true;

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Curve *cu = (Curve *)obedit->data;
    bool all_supported = true;
    bool changed = false;

    {
      ListBase *editnurb = object_editcurve_get(obedit);

      LISTBASE_FOREACH (Nurb *, nu, editnurb) {
        if (nu->type == CU_BEZIER) {
          if ((nu->pntsu > 2) && nurb_bezt_flag_any(nu, SELECT)) {
            const int error_target_len = max_ii(2, nu->pntsu * ratio);
            if (error_target_len != nu->pntsu) {
              BKE_curve_decimate_nurb(nu, cu->resolu, error_sq_max, error_target_len);
              changed = true;
            }
          }
        }
        else {
          all_supported = false;
        }
      }
    }

    if (all_supported == false) {
      all_supported_multi = false;
    }

    if (changed) {
      cu->actnu = cu->actvert = CU_ACT_NONE;
      if (ED_curve_updateAnimPaths(bmain, static_cast<Curve *>(obedit->data))) {
        WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);
      }

      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
      DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
    }
  }

  if (all_supported_multi == false) {
    BKE_report(op->reports, RPT_WARNING, "Only Bézier curves are supported");
  }

  return OPERATOR_FINISHED;
}

void CURVE_OT_decimate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Decimate Curve";
  ot->description = "Simplify selected curves";
  ot->idname = "CURVE_OT_decimate";

  /* API callbacks. */
  ot->exec = curve_decimate_exec;
  ot->poll = ED_operator_editcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float_factor(ot->srna, "ratio", 1.0f, 0.0f, 1.0f, "Ratio", "", 0.0f, 1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shade Smooth/Flat Operator
 * \{ */

static wmOperatorStatus shade_smooth_exec(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  int clear = STREQ(op->idname, "CURVE_OT_shade_flat");
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  wmOperatorStatus ret_value = OPERATOR_CANCELLED;

  for (Object *obedit : objects) {
    ListBase *editnurb = object_editcurve_get(obedit);

    if (obedit->type != OB_CURVES_LEGACY) {
      continue;
    }

    LISTBASE_FOREACH (Nurb *, nu, editnurb) {
      if (ED_curve_nurb_select_check(v3d, nu)) {
        if (!clear) {
          nu->flag |= CU_SMOOTH;
        }
        else {
          nu->flag &= ~CU_SMOOTH;
        }
      }
    }

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
    ret_value = OPERATOR_FINISHED;
  }

  return ret_value;
}

void CURVE_OT_shade_smooth(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Shade Smooth";
  ot->idname = "CURVE_OT_shade_smooth";
  ot->description = "Set shading to smooth";

  /* API callbacks. */
  ot->exec = shade_smooth_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void CURVE_OT_shade_flat(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Shade Flat";
  ot->idname = "CURVE_OT_shade_flat";
  ot->description = "Set shading to flat";

  /* API callbacks. */
  ot->exec = shade_smooth_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Join Operator
 * \{ */

wmOperatorStatus ED_curve_join_objects_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob_active = CTX_data_active_object(C);
  Curve *cu;
  BezTriple *bezt;
  BPoint *bp;
  ListBase tempbase;
  float imat[4][4], cmat[4][4];
  int a;
  bool ok = false;

  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter == ob_active) {
      ok = true;
      break;
    }
  }
  CTX_DATA_END;

  /* that way the active object is always selected */
  if (ok == false) {
    BKE_report(op->reports, RPT_WARNING, "Active object is not a selected curve");
    return OPERATOR_CANCELLED;
  }

  BLI_listbase_clear(&tempbase);

  /* Inverse transform for all selected curves in this object,
   * See object_join_exec for detailed comment on why the safe version is used. */
  invert_m4_m4_safe_ortho(imat, ob_active->object_to_world().ptr());

  Curve *cu_active = static_cast<Curve *>(ob_active->data);

  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter->type == ob_active->type) {
      if (ob_iter != ob_active) {

        cu = static_cast<Curve *>(ob_iter->data);

        if (cu->nurb.first) {
          /* watch it: switch order here really goes wrong */
          mul_m4_m4m4(cmat, imat, ob_iter->object_to_world().ptr());

          /* Compensate for different bevel depth. */
          bool do_radius = false;
          float compensate_radius = 0.0f;
          if (cu->bevel_radius != 0.0f && cu_active->bevel_radius != 0.0f) {
            float compensate_scale = mat4_to_scale(cmat);
            compensate_radius = cu->bevel_radius / cu_active->bevel_radius * compensate_scale;
            do_radius = true;
          }

          LISTBASE_FOREACH (Nurb *, nu, &cu->nurb) {
            Nurb *newnu = BKE_nurb_duplicate(nu);
            if (ob_active->totcol) { /* TODO: merge material lists. */
              CLAMP(newnu->mat_nr, 0, ob_active->totcol - 1);
            }
            else {
              newnu->mat_nr = 0;
            }
            BLI_addtail(&tempbase, newnu);

            if ((bezt = newnu->bezt)) {
              a = newnu->pntsu;
              while (a--) {
                /* Compensate for different bevel depth. */
                if (do_radius) {
                  bezt->radius *= compensate_radius;
                }

                mul_m4_v3(cmat, bezt->vec[0]);
                mul_m4_v3(cmat, bezt->vec[1]);
                mul_m4_v3(cmat, bezt->vec[2]);
                bezt++;
              }
              BKE_nurb_handles_calc(newnu);
            }
            if ((bp = newnu->bp)) {
              a = newnu->pntsu * nu->pntsv;
              while (a--) {
                mul_m4_v3(cmat, bp->vec);
                bp++;
              }
            }
          }
        }

        blender::ed::object::base_free_and_unlink(bmain, scene, ob_iter);
      }
    }
  }
  CTX_DATA_END;

  cu = static_cast<Curve *>(ob_active->data);
  BLI_movelisttolist(&cu->nurb, &tempbase);

  if (ob_active->type == OB_CURVES_LEGACY && CU_IS_2D(cu)) {
    /* Account for mixed 2D/3D curves when joining */
    BKE_curve_dimension_update(cu);
  }

  DEG_relations_tag_update(bmain); /* because we removed object(s), call before editmode! */

  DEG_id_tag_update(&ob_active->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);

  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Tilt Operator
 * \{ */

static wmOperatorStatus clear_tilt_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  int totobjects = 0;

  for (Object *obedit : objects) {
    Curve *cu = static_cast<Curve *>(obedit->data);

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    if (blender::ed::object::shape_key_report_if_locked(obedit, op->reports)) {
      continue;
    }

    totobjects++;

    ListBase *editnurb = object_editcurve_get(obedit);
    BezTriple *bezt;
    BPoint *bp;
    int a;

    LISTBASE_FOREACH (Nurb *, nu, editnurb) {
      if (nu->bezt) {
        bezt = nu->bezt;
        a = nu->pntsu;
        while (a--) {
          if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
            bezt->tilt = 0.0;
          }
          bezt++;
        }
      }
      else if (nu->bp) {
        bp = nu->bp;
        a = nu->pntsu * nu->pntsv;
        while (a--) {
          if (bp->f1 & SELECT) {
            bp->tilt = 0.0f;
          }
          bp++;
        }
      }
    }

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }
  return totobjects ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void CURVE_OT_tilt_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Tilt";
  ot->idname = "CURVE_OT_tilt_clear";
  ot->description = "Clear the tilt of selected control points";

  /* API callbacks. */
  ot->exec = clear_tilt_exec;
  ot->poll = ED_operator_editcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void ED_curve_beztcpy(EditNurb *editnurb, BezTriple *dst, BezTriple *src, int count)
{
  memcpy(dst, src, count * sizeof(BezTriple));
  keyIndex_updateBezt(editnurb, src, dst, count);
}

void ED_curve_bpcpy(EditNurb *editnurb, BPoint *dst, BPoint *src, int count)
{
  memcpy(dst, src, count * sizeof(BPoint));
  keyIndex_updateBP(editnurb, src, dst, count);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Match Texture Space Operator
 * \{ */

static bool match_texture_space_poll(bContext *C)
{
  Object *object = CTX_data_active_object(C);

  return object && ELEM(object->type, OB_CURVES_LEGACY, OB_SURF, OB_FONT);
}

static wmOperatorStatus match_texture_space_exec(bContext *C, wmOperator * /*op*/)
{
  /* Need to ensure the dependency graph is fully evaluated, so the display list is at a correct
   * state. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  (void)depsgraph;

  Object *object = CTX_data_active_object(C);
  Object *object_eval = DEG_get_evaluated(depsgraph, object);
  Curve *curve = (Curve *)object->data;
  float min[3], max[3], texspace_size[3], texspace_location[3];
  int a;

  BLI_assert(object_eval->runtime->curve_cache != nullptr);

  INIT_MINMAX(min, max);
  BKE_displist_minmax(&object_eval->runtime->curve_cache->disp, min, max);

  mid_v3_v3v3(texspace_location, min, max);

  texspace_size[0] = (max[0] - min[0]) / 2.0f;
  texspace_size[1] = (max[1] - min[1]) / 2.0f;
  texspace_size[2] = (max[2] - min[2]) / 2.0f;

  for (a = 0; a < 3; a++) {
    if (texspace_size[a] == 0.0f) {
      texspace_size[a] = 1.0f;
    }
    else if (texspace_size[a] > 0.0f && texspace_size[a] < 0.00001f) {
      texspace_size[a] = 0.00001f;
    }
    else if (texspace_size[a] < 0.0f && texspace_size[a] > -0.00001f) {
      texspace_size[a] = -0.00001f;
    }
  }

  copy_v3_v3(curve->texspace_location, texspace_location);
  copy_v3_v3(curve->texspace_size, texspace_size);

  curve->texspace_flag &= ~CU_TEXSPACE_FLAG_AUTO;

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, curve);
  DEG_id_tag_update(&curve->id, ID_RECALC_GEOMETRY);

  return OPERATOR_FINISHED;
}

void CURVE_OT_match_texture_space(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Match Texture Space";
  ot->idname = "CURVE_OT_match_texture_space";
  ot->description = "Match texture space to object's bounding box";

  /* API callbacks. */
  ot->exec = match_texture_space_exec;
  ot->poll = match_texture_space_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
