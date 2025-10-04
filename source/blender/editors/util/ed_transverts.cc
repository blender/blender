/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edutil
 */

#include "DNA_mesh_types.h"
#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_curves_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_editmesh.hh"
#include "BKE_lattice.hh"
#include "BKE_mesh_iterators.hh"
#include "BKE_mesh_types.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "ED_armature.hh"
#include "ED_curves.hh"
#include "ED_pointcloud.hh"

#include "ANIM_armature.hh"

#include "ED_transverts.hh" /* own include */

void ED_transverts_update_obedit(TransVertStore *tvs, Object *obedit)
{
  /* NOTE: copied from  `editobject.c`, now uses (almost) proper depsgraph. */

  const int mode = tvs->mode;
  BLI_assert(ED_transverts_check_obedit(obedit) == true);

  DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);

  if (obedit->type == OB_MESH) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BM_mesh_normals_update(em->bm);
  }
  else if (ELEM(obedit->type, OB_CURVES_LEGACY, OB_SURF)) {
    Curve *cu = static_cast<Curve *>(obedit->data);
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);
    Nurb *nu = static_cast<Nurb *>(nurbs->first);

    while (nu) {
      /* keep handles' vectors unchanged */
      if (nu->bezt && (mode & TM_SKIP_HANDLES)) {
        int a = nu->pntsu;
        TransVert *tv = tvs->transverts;
        BezTriple *bezt = nu->bezt;

        while (a--) {
          if (bezt->hide == 0) {
            bool skip_handle = false;
            if (bezt->f2 & SELECT) {
              skip_handle = (mode & TM_SKIP_HANDLES) != 0;
            }

            if ((bezt->f1 & SELECT) && !skip_handle) {
              BLI_assert(tv->loc == bezt->vec[0]);
              tv++;
            }

            if (bezt->f2 & SELECT) {
              float v[3];

              if (((bezt->f1 & SELECT) && !skip_handle) == 0) {
                sub_v3_v3v3(v, tv->loc, tv->oldloc);
                add_v3_v3(bezt->vec[0], v);
              }

              if (((bezt->f3 & SELECT) && !skip_handle) == 0) {
                sub_v3_v3v3(v, tv->loc, tv->oldloc);
                add_v3_v3(bezt->vec[2], v);
              }

              BLI_assert(tv->loc == bezt->vec[1]);
              tv++;
            }

            if ((bezt->f3 & SELECT) && !skip_handle) {
              BLI_assert(tv->loc == bezt->vec[2]);
              tv++;
            }
          }

          bezt++;
        }
      }

      if (CU_IS_2D(cu)) {
        BKE_nurb_project_2d(nu);
      }
      BKE_nurb_handles_test(nu, NURB_HANDLE_TEST_EACH, false); /* test for bezier too */
      nu = nu->next;
    }
  }
  else if (obedit->type == OB_ARMATURE) {
    bArmature *arm = static_cast<bArmature *>(obedit->data);
    TransVert *tv = tvs->transverts;
    int a = 0;

    /* Ensure all bone tails are correctly adjusted */
    LISTBASE_FOREACH (EditBone *, ebo, arm->edbo) {
      if (!blender::animrig::bone_is_visible(arm, ebo)) {
        continue;
      }
      /* adjust tip if both ends selected */
      if ((ebo->flag & BONE_ROOTSEL) && (ebo->flag & BONE_TIPSEL)) {
        if (tv) {
          float diffvec[3];

          sub_v3_v3v3(diffvec, tv->loc, tv->oldloc);
          add_v3_v3(ebo->tail, diffvec);

          a++;
          if (a < tvs->transverts_tot) {
            tv++;
          }
        }
      }
    }

    /* Ensure all bones are correctly adjusted */
    LISTBASE_FOREACH (EditBone *, ebo, arm->edbo) {
      if ((ebo->flag & BONE_CONNECTED) && ebo->parent) {
        /* If this bone has a parent tip that has been moved */
        if (blender::animrig::bone_is_visible(arm, ebo->parent) &&
            (ebo->parent->flag & BONE_TIPSEL))
        {
          copy_v3_v3(ebo->head, ebo->parent->tail);
        }
        /* If this bone has a parent tip that has NOT been moved */
        else {
          copy_v3_v3(ebo->parent->tail, ebo->head);
        }
      }
    }
    if (arm->flag & ARM_MIRROR_EDIT) {
      ED_armature_edit_transform_mirror_update(obedit);
    }
  }
  else if (obedit->type == OB_LATTICE) {
    Lattice *lt = static_cast<Lattice *>(obedit->data);

    if (lt->editlatt->latt->flag & LT_OUTSIDE) {
      outside_lattice(lt->editlatt->latt);
    }
  }
  else if (obedit->type == OB_CURVES) {
    Curves *curves_id = static_cast<Curves *>(obedit->data);
    blender::bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    curves.tag_positions_changed();
    curves.calculate_bezier_auto_handles();
  }
  else if (obedit->type == OB_POINTCLOUD) {
    PointCloud *pointcloud = static_cast<PointCloud *>(obedit->data);
    pointcloud->tag_positions_changed();
  }
}

static void set_mapped_co(void *vuserdata, int index, const float co[3], const float /*no*/[3])
{
  void **userdata = static_cast<void **>(vuserdata);
  BMEditMesh *em = static_cast<BMEditMesh *>(userdata[0]);
  TransVert *tv = static_cast<TransVert *>(userdata[1]);
  BMVert *eve = BM_vert_at_index(em->bm, index);

  if (BM_elem_index_get(eve) != TM_INDEX_SKIP) {
    tv = &tv[BM_elem_index_get(eve)];

    /* Be clever, get the closest vertex to the original,
     * behaves most logically when the mirror modifier is used for eg #33051. */
    if ((tv->flag & TX_VERT_USE_MAPLOC) == 0) {
      /* first time */
      copy_v3_v3(tv->maploc, co);
      tv->flag |= TX_VERT_USE_MAPLOC;
    }
    else {
      /* find best location to use */
      if (len_squared_v3v3(eve->co, co) < len_squared_v3v3(eve->co, tv->maploc)) {
        copy_v3_v3(tv->maploc, co);
      }
    }
  }
}

bool ED_transverts_check_obedit(const Object *obedit)
{
  return ELEM(obedit->type,
              OB_ARMATURE,
              OB_LATTICE,
              OB_MESH,
              OB_SURF,
              OB_CURVES_LEGACY,
              OB_MBALL,
              OB_CURVES,
              OB_POINTCLOUD);
}

void ED_transverts_create_from_obedit(TransVertStore *tvs, const Object *obedit, const int mode)
{
  using namespace blender;
  BLI_assert(DEG_is_evaluated(obedit));

  Nurb *nu;
  BezTriple *bezt;
  BPoint *bp;
  TransVert *tv = nullptr;
  MetaElem *ml;
  BMVert *eve;
  int a;

  tvs->transverts_tot = 0;

  if (obedit->type == OB_MESH) {
    const Object *object_orig = DEG_get_original(obedit);
    const Mesh &mesh = *static_cast<Mesh *>(object_orig->data);
    BMEditMesh *em = mesh.runtime->edit_mesh.get();
    BMesh *bm = em->bm;
    BMIter iter;
    void *userdata[2] = {em, nullptr};
    // int proptrans = 0; /*UNUSED*/

    /* abuses vertex index all over, set, just set dirty here,
     * perhaps this could use its own array instead? - campbell */

    /* transform now requires awareness for select mode, so we tag the f1 flags in verts */
    tvs->transverts_tot = 0;
    if (em->selectmode & SCE_SELECT_VERTEX) {
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN) && BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
          BM_elem_index_set(eve, TM_INDEX_ON); /* set_dirty! */
          tvs->transverts_tot++;
        }
        else {
          BM_elem_index_set(eve, TM_INDEX_OFF); /* set_dirty! */
        }
      }
    }
    else if (em->selectmode & SCE_SELECT_EDGE) {
      BMEdge *eed;

      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        BM_elem_index_set(eve, TM_INDEX_OFF); /* set_dirty! */
      }

      BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
        if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
          BM_elem_index_set(eed->v1, TM_INDEX_ON); /* set_dirty! */
          BM_elem_index_set(eed->v2, TM_INDEX_ON); /* set_dirty! */
        }
      }

      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (BM_elem_index_get(eve) == TM_INDEX_ON) {
          tvs->transverts_tot++;
        }
      }
    }
    else {
      BMFace *efa;

      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        BM_elem_index_set(eve, TM_INDEX_OFF); /* set_dirty! */
      }

      BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          BMIter liter;
          BMLoop *l;

          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            BM_elem_index_set(l->v, TM_INDEX_ON); /* set_dirty! */
          }
        }
      }

      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (BM_elem_index_get(eve) == TM_INDEX_ON) {
          tvs->transverts_tot++;
        }
      }
    }
    /* for any of the 3 loops above which all dirty the indices */
    bm->elem_index_dirty |= BM_VERT;

    /* and now make transverts */
    if (tvs->transverts_tot) {
      tv = tvs->transverts = MEM_calloc_arrayN<TransVert>(tvs->transverts_tot, __func__);

      a = 0;
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (BM_elem_index_get(eve)) {
          BM_elem_index_set(eve, a); /* set_dirty! */
          copy_v3_v3(tv->oldloc, eve->co);
          tv->loc = eve->co;
          tv->flag = (BM_elem_index_get(eve) == TM_INDEX_ON) ? SELECT : 0;

          if (mode & TM_CALC_NORMALS) {
            tv->flag |= TX_VERT_USE_NORMAL;
            copy_v3_v3(tv->normal, eve->no);
          }

          tv++;
          a++;
        }
        else {
          BM_elem_index_set(eve, TM_INDEX_SKIP); /* set_dirty! */
        }
      }
      /* set dirty already, above */

      userdata[1] = tvs->transverts;
    }

    if (mode & TM_CALC_MAPLOC) {
      const Mesh *editmesh_eval_cage = BKE_object_get_editmesh_eval_cage(obedit);
      if (tvs->transverts && editmesh_eval_cage) {
        BM_mesh_elem_table_ensure(bm, BM_VERT);
        BKE_mesh_foreach_mapped_vert(
            editmesh_eval_cage, set_mapped_co, userdata, MESH_FOREACH_NOP);
      }
    }
  }
  else if (obedit->type == OB_ARMATURE) {
    bArmature *arm = static_cast<bArmature *>(obedit->data);
    int totmalloc = BLI_listbase_count(arm->edbo);

    totmalloc *= 2; /* probably overkill but bones can have 2 trans verts each */

    tv = tvs->transverts = MEM_calloc_arrayN<TransVert>(totmalloc, __func__);

    LISTBASE_FOREACH (EditBone *, ebo, arm->edbo) {
      if (blender::animrig::bone_is_visible(arm, ebo)) {
        const bool tipsel = (ebo->flag & BONE_TIPSEL) != 0;
        const bool rootsel = (ebo->flag & BONE_ROOTSEL) != 0;
        const bool rootok = !(ebo->parent && (ebo->flag & BONE_CONNECTED) &&
                              (blender::animrig::bone_is_visible(arm, ebo->parent) &&
                               (ebo->parent->flag & BONE_TIPSEL)));

        if ((tipsel && rootsel) || (rootsel)) {
          /* Don't add the tip (unless mode & TM_ALL_JOINTS, for getting all joints),
           * otherwise we get zero-length bones as tips will snap to the same
           * location as heads.
           */
          if (rootok) {
            copy_v3_v3(tv->oldloc, ebo->head);
            tv->loc = ebo->head;
            tv->flag = SELECT;
            tv++;
            tvs->transverts_tot++;
          }

          if ((mode & TM_ALL_JOINTS) && (tipsel)) {
            copy_v3_v3(tv->oldloc, ebo->tail);
            tv->loc = ebo->tail;
            tv->flag = SELECT;
            tv++;
            tvs->transverts_tot++;
          }
        }
        else if (tipsel) {
          copy_v3_v3(tv->oldloc, ebo->tail);
          tv->loc = ebo->tail;
          tv->flag = SELECT;
          tv++;
          tvs->transverts_tot++;
        }
      }
    }
  }
  else if (ELEM(obedit->type, OB_CURVES_LEGACY, OB_SURF)) {
    Curve *cu = static_cast<Curve *>(obedit->data);
    int totmalloc = 0;
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);

    LISTBASE_FOREACH (Nurb *, nu, nurbs) {
      if (nu->type == CU_BEZIER) {
        totmalloc += 3 * nu->pntsu;
      }
      else {
        totmalloc += nu->pntsu * nu->pntsv;
      }
    }
    tv = tvs->transverts = MEM_calloc_arrayN<TransVert>(totmalloc, __func__);

    nu = static_cast<Nurb *>(nurbs->first);
    while (nu) {
      if (nu->type == CU_BEZIER) {
        a = nu->pntsu;
        bezt = nu->bezt;
        while (a--) {
          if (bezt->hide == 0) {
            bool skip_handle = false;
            if (bezt->f2 & SELECT) {
              skip_handle = (mode & TM_SKIP_HANDLES) != 0;
            }

            if ((bezt->f1 & SELECT) && !skip_handle) {
              copy_v3_v3(tv->oldloc, bezt->vec[0]);
              tv->loc = bezt->vec[0];
              tv->flag = bezt->f1 & SELECT;

              if (mode & TM_CALC_NORMALS) {
                tv->flag |= TX_VERT_USE_NORMAL;
                BKE_nurb_bezt_calc_plane(nu, bezt, tv->normal);
              }

              tv++;
              tvs->transverts_tot++;
            }
            if (bezt->f2 & SELECT) {
              copy_v3_v3(tv->oldloc, bezt->vec[1]);
              tv->loc = bezt->vec[1];
              tv->flag = bezt->f2 & SELECT;

              if (mode & TM_CALC_NORMALS) {
                tv->flag |= TX_VERT_USE_NORMAL;
                BKE_nurb_bezt_calc_plane(nu, bezt, tv->normal);
              }

              tv++;
              tvs->transverts_tot++;
            }
            if ((bezt->f3 & SELECT) && !skip_handle) {
              copy_v3_v3(tv->oldloc, bezt->vec[2]);
              tv->loc = bezt->vec[2];
              tv->flag = bezt->f3 & SELECT;

              if (mode & TM_CALC_NORMALS) {
                tv->flag |= TX_VERT_USE_NORMAL;
                BKE_nurb_bezt_calc_plane(nu, bezt, tv->normal);
              }

              tv++;
              tvs->transverts_tot++;
            }
          }
          bezt++;
        }
      }
      else {
        a = nu->pntsu * nu->pntsv;
        bp = nu->bp;
        while (a--) {
          if (bp->hide == 0) {
            if (bp->f1 & SELECT) {
              copy_v3_v3(tv->oldloc, bp->vec);
              tv->loc = bp->vec;
              tv->flag = bp->f1 & SELECT;
              tv++;
              tvs->transverts_tot++;
            }
          }
          bp++;
        }
      }
      nu = nu->next;
    }
  }
  else if (obedit->type == OB_MBALL) {
    MetaBall *mb = static_cast<MetaBall *>(obedit->data);
    int totmalloc = BLI_listbase_count(mb->editelems);

    tv = tvs->transverts = MEM_calloc_arrayN<TransVert>(totmalloc, __func__);

    ml = static_cast<MetaElem *>(mb->editelems->first);
    while (ml) {
      if (ml->flag & SELECT) {
        tv->loc = &ml->x;
        copy_v3_v3(tv->oldloc, tv->loc);
        tv->flag = SELECT;
        tv++;
        tvs->transverts_tot++;
      }
      ml = ml->next;
    }
  }
  else if (obedit->type == OB_LATTICE) {
    Lattice *lt = static_cast<Lattice *>(obedit->data);

    bp = lt->editlatt->latt->def;

    a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;

    tv = tvs->transverts = MEM_calloc_arrayN<TransVert>(a, __func__);

    while (a--) {
      if (bp->f1 & SELECT) {
        if (bp->hide == 0) {
          copy_v3_v3(tv->oldloc, bp->vec);
          tv->loc = bp->vec;
          tv->flag = bp->f1 & SELECT;
          tv++;
          tvs->transverts_tot++;
        }
      }
      bp++;
    }
  }
  else if (obedit->type == OB_CURVES) {
    Curves *curves_id = static_cast<Curves *>(obedit->data);
    blender::ed::curves::transverts_from_curves_positions_create(
        curves_id->geometry.wrap(), tvs, ((mode & TM_SKIP_HANDLES) != 0));
  }
  else if (obedit->type == OB_POINTCLOUD) {
    PointCloud *pointcloud = static_cast<PointCloud *>(obedit->data);

    IndexMaskMemory memory;
    const IndexMask selection = blender::ed::pointcloud::retrieve_selected_points(*pointcloud,
                                                                                  memory);
    MutableSpan<float3> positions = pointcloud->positions_for_write();

    tvs->transverts = MEM_calloc_arrayN<TransVert>(selection.size(), __func__);
    tvs->transverts_tot = selection.size();

    selection.foreach_index(GrainSize(1024), [&](const int64_t i, const int64_t pos) {
      TransVert &tv = tvs->transverts[pos];
      tv.loc = positions[i];
      tv.flag = SELECT;
      copy_v3_v3(tv.oldloc, tv.loc);
    });
  }

  if (!tvs->transverts_tot && tvs->transverts) {
    /* Prevent memory leak. happens for curves/lattices due to
     * difficult condition of adding points to trans data. */
    MEM_freeN(tvs->transverts);
    tvs->transverts = nullptr;
  }

  tvs->mode = mode;
}

void ED_transverts_free(TransVertStore *tvs)
{
  MEM_SAFE_FREE(tvs->transverts);
  tvs->transverts_tot = 0;
}

bool ED_transverts_poll(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit) {
    if (ED_transverts_check_obedit(obedit)) {
      return true;
    }
  }
  return false;
}
