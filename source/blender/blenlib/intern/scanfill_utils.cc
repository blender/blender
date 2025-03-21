/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <cstdlib>

#include "DNA_listBase.h"
#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLI_scanfill.h" /* own include */

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

struct PolyInfo {
  ScanFillEdge *edge_first, *edge_last;
  ScanFillVert *vert_outer;
};

struct ScanFillIsect {
  ScanFillIsect *next, *prev;
  float co[3];

  /* newly created vertex */
  ScanFillVert *v;
};

#define V_ISISECT 1
#define E_ISISECT 1
#define E_ISDELETE 2

#define EFLAG_SET(eed, val) \
  { \
    CHECK_TYPE(eed, ScanFillEdge *); \
    (eed)->user_flag = (eed)->user_flag | uint(val); \
  } \
  (void)0
#if 0
#  define EFLAG_CLEAR(eed, val) \
    { \
      CHECK_TYPE(eed, ScanFillEdge *); \
      (eed)->user_flag = (eed)->user_flag & ~(uint)val; \
    } \
    (void)0
#endif

#define VFLAG_SET(eve, val) \
  { \
    CHECK_TYPE(eve, ScanFillVert *); \
    (eve)->user_flag = (eve)->user_flag | uint(val); \
  } \
  (void)0
#if 0
#  define VFLAG_CLEAR(eve, val) \
    { \
      CHECK_TYPE(eve, ScanFillVert *); \
      (eve)->user_flags = (eve)->user_flag & ~(uint)val; \
    } \
    (void)0
#endif

#if 0
void BLI_scanfill_obj_dump(ScanFillContext *sf_ctx)
{
  FILE *f = fopen("test.obj", "w");
  uint i = 1;

  ScanFillVert *eve;
  ScanFillEdge *eed;

  for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next, i++) {
    fprintf(f, "v %f %f %f\n", UNPACK3(eve->co));
    eve->keyindex = i;
  }
  for (eed = sf_ctx->filledgebase.first; eed; eed = eed->next) {
    fprintf(f, "f %d %d\n", eed->v1->keyindex, eed->v2->keyindex);
  }
  fclose(f);
}
#endif

static ListBase *edge_isect_ls_ensure(GHash *isect_hash, ScanFillEdge *eed)
{
  void **val_p;

  if (!BLI_ghash_ensure_p(isect_hash, eed, &val_p)) {
    *val_p = MEM_callocN<ListBase>(__func__);
  }

  return static_cast<ListBase *>(*val_p);
}

static ListBase *edge_isect_ls_add(GHash *isect_hash, ScanFillEdge *eed, ScanFillIsect *isect)
{
  ListBase *e_ls;
  LinkData *isect_link;
  e_ls = edge_isect_ls_ensure(isect_hash, eed);
  isect_link = MEM_callocN<LinkData>(__func__);
  isect_link->data = isect;
  EFLAG_SET(eed, E_ISISECT);
  BLI_addtail(e_ls, isect_link);
  return e_ls;
}

static int edge_isect_ls_sort_cb(void *thunk, const void *def_a_ptr, const void *def_b_ptr)
{
  const float *co = static_cast<const float *>(thunk);

  const ScanFillIsect *i_a = static_cast<const ScanFillIsect *>(
      ((const LinkData *)def_a_ptr)->data);
  const ScanFillIsect *i_b = static_cast<const ScanFillIsect *>(
      ((const LinkData *)def_b_ptr)->data);
  const float a = len_squared_v2v2(co, i_a->co);
  const float b = len_squared_v2v2(co, i_b->co);

  if (a > b) {
    return -1;
  }

  return (a < b);
}

static ScanFillEdge *edge_step(PolyInfo *poly_info,
                               const ushort poly_nr,
                               ScanFillVert *v_prev,
                               ScanFillVert *v_curr,
                               ScanFillEdge *e_curr)
{
  ScanFillEdge *eed;

  BLI_assert(ELEM(v_prev, e_curr->v1, e_curr->v2));
  BLI_assert(ELEM(v_curr, e_curr->v1, e_curr->v2));

  eed = (e_curr->next && e_curr != poly_info[poly_nr].edge_last) ? e_curr->next :
                                                                   poly_info[poly_nr].edge_first;
  if (ELEM(v_curr, eed->v1, eed->v2) == true && ELEM(v_prev, eed->v1, eed->v2) == false) {
    return eed;
  }

  eed = (e_curr->prev && e_curr != poly_info[poly_nr].edge_first) ? e_curr->prev :
                                                                    poly_info[poly_nr].edge_last;
  if (ELEM(v_curr, eed->v1, eed->v2) == true && ELEM(v_prev, eed->v1, eed->v2) == false) {
    return eed;
  }

  BLI_assert(0);
  return nullptr;
}

static bool scanfill_preprocess_self_isect(ScanFillContext *sf_ctx,
                                           PolyInfo *poly_info,
                                           const ushort poly_nr,
                                           ListBase *filledgebase)
{
  PolyInfo *pi = &poly_info[poly_nr];
  GHash *isect_hash = nullptr;
  ListBase isect_lb = {nullptr};

  /* warning, O(n2) check here, should use spatial lookup */
  {
    ScanFillEdge *eed;

    for (eed = pi->edge_first; eed; eed = (eed == pi->edge_last) ? nullptr : eed->next) {
      ScanFillEdge *eed_other;

      for (eed_other = eed->next; eed_other;
           eed_other = (eed_other == pi->edge_last) ? nullptr : eed_other->next)
      {
        if (!ELEM(eed->v1, eed_other->v1, eed_other->v2) &&
            !ELEM(eed->v2, eed_other->v1, eed_other->v2) && (eed != eed_other))
        {
          /* check isect */
          float pt[2];
          BLI_assert(eed != eed_other);

          if (isect_seg_seg_v2_point(
                  eed->v1->co, eed->v2->co, eed_other->v1->co, eed_other->v2->co, pt) == 1)
          {
            ScanFillIsect *isect;

            if (UNLIKELY(isect_hash == nullptr)) {
              isect_hash = BLI_ghash_ptr_new(__func__);
            }

            isect = MEM_mallocN<ScanFillIsect>(__func__);

            BLI_addtail(&isect_lb, isect);

            copy_v2_v2(isect->co, pt);
            isect->co[2] = eed->v1->co[2];
            isect->v = BLI_scanfill_vert_add(sf_ctx, isect->co);

            /* NOTE: vert may belong to 2 polys now */
            isect->v->poly_nr = eed->v1->poly_nr;

            VFLAG_SET(isect->v, V_ISISECT);
            edge_isect_ls_add(isect_hash, eed, isect);
            edge_isect_ls_add(isect_hash, eed_other, isect);
          }
        }
      }
    }
  }

  if (isect_hash == nullptr) {
    return false;
  }

  /* now subdiv the edges */
  {
    ScanFillEdge *eed;

    for (eed = pi->edge_first; eed; eed = (eed == pi->edge_last) ? nullptr : eed->next) {
      if (eed->user_flag & E_ISISECT) {
        ListBase *e_ls = static_cast<ListBase *>(BLI_ghash_lookup(isect_hash, eed));

        if (UNLIKELY(e_ls == nullptr)) {
          /* only happens in very rare cases (entirely overlapping splines).
           * in this case we can't do much useful. but at least don't crash */
          continue;
        }

        /* Maintain correct terminating edge. */
        if (pi->edge_last == eed) {
          pi->edge_last = nullptr;
        }

        if (BLI_listbase_is_single(e_ls) == false) {
          BLI_listbase_sort_r(e_ls, edge_isect_ls_sort_cb, eed->v2->co);
        }

        /* move original edge to filledgebase and add replacement
         * (which gets subdivided next) */
        {
          ScanFillEdge *eed_tmp;
          eed_tmp = BLI_scanfill_edge_add(sf_ctx, eed->v1, eed->v2);
          BLI_remlink(&sf_ctx->filledgebase, eed_tmp);
          BLI_insertlinkafter(&sf_ctx->filledgebase, eed, eed_tmp);
          BLI_remlink(&sf_ctx->filledgebase, eed);
          BLI_addtail(filledgebase, eed);
          if (pi->edge_first == eed) {
            pi->edge_first = eed_tmp;
          }
          eed = eed_tmp;
        }

        LISTBASE_FOREACH (LinkData *, isect_link, e_ls) {
          ScanFillIsect *isect = static_cast<ScanFillIsect *>(isect_link->data);
          ScanFillEdge *eed_subd;

          eed_subd = BLI_scanfill_edge_add(sf_ctx, isect->v, eed->v2);
          eed_subd->poly_nr = poly_nr;
          eed->v2 = isect->v;

          BLI_remlink(&sf_ctx->filledgebase, eed_subd);
          BLI_insertlinkafter(&sf_ctx->filledgebase, eed, eed_subd);

          /* step to the next edge and continue dividing */
          eed = eed_subd;
        }

        BLI_freelistN(e_ls);
        MEM_freeN(e_ls);

        if (pi->edge_last == nullptr) {
          pi->edge_last = eed;
        }
      }
    }
  }

  BLI_freelistN(&isect_lb);
  BLI_ghash_free(isect_hash, nullptr, nullptr);

  {
    ScanFillEdge *e_init;
    ScanFillEdge *e_curr;
    ScanFillEdge *e_next;

    ScanFillVert *v_prev;
    ScanFillVert *v_curr;

    bool inside = false;

    /* first vert */
#if 0
    e_init = pi->edge_last;
    e_curr = e_init;
    e_next = pi->edge_first;

    v_prev = e_curr->v1;
    v_curr = e_curr->v2;
#else

    /* find outside vertex */
    {
      ScanFillEdge *eed;
      ScanFillEdge *eed_prev;
      float min_x = FLT_MAX;

      e_curr = pi->edge_last;
      e_next = pi->edge_first;

      eed_prev = pi->edge_last;
      for (eed = pi->edge_first; eed; eed = (eed == pi->edge_last) ? nullptr : eed->next) {
        if (eed->v2->co[0] < min_x) {
          min_x = eed->v2->co[0];
          e_curr = eed_prev;
          e_next = eed;
        }
        eed_prev = eed;
      }

      e_init = e_curr;
      v_prev = e_curr->v1;
      v_curr = e_curr->v2;
    }
#endif

    BLI_assert(e_curr->poly_nr == poly_nr);
    BLI_assert(pi->edge_last->poly_nr == poly_nr);

    do {
      ScanFillVert *v_next;

      v_next = (e_next->v1 == v_curr) ? e_next->v2 : e_next->v1;
      BLI_assert(ELEM(v_curr, e_next->v1, e_next->v2));

      /* track intersections */
      if (inside) {
        EFLAG_SET(e_next, E_ISDELETE);
      }
      if (v_next->user_flag & V_ISISECT) {
        inside = !inside;
      }
      /* now step... */

      v_prev = v_curr;
      v_curr = v_next;
      e_curr = e_next;

      e_next = edge_step(poly_info, poly_nr, v_prev, v_curr, e_curr);

    } while (e_curr != e_init);
  }

  return true;
}

bool BLI_scanfill_calc_self_isect(ScanFillContext *sf_ctx,
                                  ListBase *remvertbase,
                                  ListBase *remedgebase)
{
  const uint poly_num = uint(sf_ctx->poly_nr) + 1;
  bool changed = false;

  if (UNLIKELY(sf_ctx->poly_nr == SF_POLY_UNSET)) {
    return false;
  }

  PolyInfo *poly_info = MEM_calloc_arrayN<PolyInfo>(poly_num, __func__);

  /* get the polygon span */
  if (sf_ctx->poly_nr == 0) {
    poly_info->edge_first = static_cast<ScanFillEdge *>(sf_ctx->filledgebase.first);
    poly_info->edge_last = static_cast<ScanFillEdge *>(sf_ctx->filledgebase.last);
  }
  else {
    ushort poly_nr = 0;
    uint eed_index = 0;

    LISTBASE_FOREACH_INDEX (ScanFillEdge *, eed, &sf_ctx->filledgebase, eed_index) {
      BLI_assert(eed->poly_nr == eed->v1->poly_nr);
      BLI_assert(eed->poly_nr == eed->v2->poly_nr);

      if ((poly_info[poly_nr].edge_last != nullptr) &&
          (poly_info[poly_nr].edge_last->poly_nr != eed->poly_nr))
      {
        poly_nr++;
      }

      if (poly_info[poly_nr].edge_first == nullptr) {
        poly_info[poly_nr].edge_first = eed;
        poly_info[poly_nr].edge_last = eed;
      }
      else if (poly_info[poly_nr].edge_last->poly_nr == eed->poly_nr) {
        poly_info[poly_nr].edge_last = eed;
      }

      BLI_assert(poly_info[poly_nr].edge_first->poly_nr == poly_info[poly_nr].edge_last->poly_nr);
    }
  }

  /* self-intersect each polygon */
  {
    ushort poly_nr;
    for (poly_nr = 0; poly_nr < poly_num; poly_nr++) {
      changed |= scanfill_preprocess_self_isect(sf_ctx, poly_info, poly_nr, remedgebase);
    }
  }

  MEM_freeN(poly_info);

  if (changed == false) {
    return false;
  }

  /* move free edges into their own list */
  {
    LISTBASE_FOREACH_MUTABLE (ScanFillEdge *, eed, &sf_ctx->filledgebase) {
      if (eed->user_flag & E_ISDELETE) {
        BLI_remlink(&sf_ctx->filledgebase, eed);
        BLI_addtail(remedgebase, eed);
      }
    }
  }

  /* move free vertices into their own list */
  {
    LISTBASE_FOREACH (ScanFillVert *, eve, &sf_ctx->fillvertbase) {
      eve->user_flag = 0;
      eve->poly_nr = SF_POLY_UNSET;
    }
    LISTBASE_FOREACH (ScanFillEdge *, eed, &sf_ctx->filledgebase) {
      eed->v1->user_flag = 1;
      eed->v2->user_flag = 1;
      eed->poly_nr = SF_POLY_UNSET;
    }

    LISTBASE_FOREACH_MUTABLE (ScanFillVert *, eve, &sf_ctx->fillvertbase) {
      if (eve->user_flag != 1) {
        BLI_remlink(&sf_ctx->fillvertbase, eve);
        BLI_addtail(remvertbase, eve);
      }
      else {
        eve->user_flag = 0;
      }
    }
  }

  /* polygon id's are no longer meaningful,
   * when removing self intersections we may have created new isolated polys */
  sf_ctx->poly_nr = SF_POLY_UNSET;

#if 0
  BLI_scanfill_view3d_dump(sf_ctx);
  BLI_scanfill_obj_dump(sf_ctx);
#endif

  return changed;
}
