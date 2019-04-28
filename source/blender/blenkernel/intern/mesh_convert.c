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
 */

/** \file
 * \ingroup bke
 */

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_curve_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_edgehash.h"

#include "BKE_main.h"
#include "BKE_DerivedMesh.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_displist.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_mball.h"
/* these 2 are only used by conversion functions */
#include "BKE_curve.h"
/* -- */
#include "BKE_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

/* Define for cases when you want extra validation of mesh
 * after certain modifications.
 */
// #undef VALIDATE_MESH

#ifdef VALIDATE_MESH
#  define ASSERT_IS_VALID_MESH(mesh) \
    (BLI_assert((mesh == NULL) || (BKE_mesh_is_valid(mesh) == true)))
#else
#  define ASSERT_IS_VALID_MESH(mesh)
#endif

static CLG_LogRef LOG = {"bke.mesh_convert"};

void BKE_mesh_from_metaball(ListBase *lb, Mesh *me)
{
  DispList *dl;
  MVert *mvert;
  MLoop *mloop, *allloop;
  MPoly *mpoly;
  const float *nors, *verts;
  int a, *index;

  dl = lb->first;
  if (dl == NULL) {
    return;
  }

  if (dl->type == DL_INDEX4) {
    mvert = CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, dl->nr);
    allloop = mloop = CustomData_add_layer(&me->ldata, CD_MLOOP, CD_CALLOC, NULL, dl->parts * 4);
    mpoly = CustomData_add_layer(&me->pdata, CD_MPOLY, CD_CALLOC, NULL, dl->parts);
    me->mvert = mvert;
    me->mloop = mloop;
    me->mpoly = mpoly;
    me->totvert = dl->nr;
    me->totpoly = dl->parts;

    a = dl->nr;
    nors = dl->nors;
    verts = dl->verts;
    while (a--) {
      copy_v3_v3(mvert->co, verts);
      normal_float_to_short_v3(mvert->no, nors);
      mvert++;
      nors += 3;
      verts += 3;
    }

    a = dl->parts;
    index = dl->index;
    while (a--) {
      int count = index[2] != index[3] ? 4 : 3;

      mloop[0].v = index[0];
      mloop[1].v = index[1];
      mloop[2].v = index[2];
      if (count == 4) {
        mloop[3].v = index[3];
      }

      mpoly->totloop = count;
      mpoly->loopstart = (int)(mloop - allloop);
      mpoly->flag = ME_SMOOTH;

      mpoly++;
      mloop += count;
      me->totloop += count;
      index += 4;
    }

    BKE_mesh_update_customdata_pointers(me, true);

    BKE_mesh_calc_normals(me);

    BKE_mesh_calc_edges(me, true, false);
  }
}

/**
 * Specialized function to use when we _know_ existing edges don't overlap with poly edges.
 */
static void make_edges_mdata_extend(
    MEdge **r_alledge, int *r_totedge, const MPoly *mpoly, MLoop *mloop, const int totpoly)
{
  int totedge = *r_totedge;
  int totedge_new;
  EdgeHash *eh;
  unsigned int eh_reserve;
  const MPoly *mp;
  int i;

  eh_reserve = max_ii(totedge, BLI_EDGEHASH_SIZE_GUESS_FROM_POLYS(totpoly));
  eh = BLI_edgehash_new_ex(__func__, eh_reserve);

  for (i = 0, mp = mpoly; i < totpoly; i++, mp++) {
    BKE_mesh_poly_edgehash_insert(eh, mp, mloop + mp->loopstart);
  }

  totedge_new = BLI_edgehash_len(eh);

#ifdef DEBUG
  /* ensure that there's no overlap! */
  if (totedge_new) {
    MEdge *medge = *r_alledge;
    for (i = 0; i < totedge; i++, medge++) {
      BLI_assert(BLI_edgehash_haskey(eh, medge->v1, medge->v2) == false);
    }
  }
#endif

  if (totedge_new) {
    EdgeHashIterator *ehi;
    MEdge *medge;
    unsigned int e_index = totedge;

    *r_alledge = medge = (*r_alledge ?
                              MEM_reallocN(*r_alledge, sizeof(MEdge) * (totedge + totedge_new)) :
                              MEM_calloc_arrayN(totedge_new, sizeof(MEdge), __func__));
    medge += totedge;

    totedge += totedge_new;

    /* --- */
    for (ehi = BLI_edgehashIterator_new(eh); BLI_edgehashIterator_isDone(ehi) == false;
         BLI_edgehashIterator_step(ehi), ++medge, e_index++) {
      BLI_edgehashIterator_getKey(ehi, &medge->v1, &medge->v2);
      BLI_edgehashIterator_setValue(ehi, POINTER_FROM_UINT(e_index));

      medge->crease = medge->bweight = 0;
      medge->flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
    BLI_edgehashIterator_free(ehi);

    *r_totedge = totedge;

    for (i = 0, mp = mpoly; i < totpoly; i++, mp++) {
      MLoop *l = &mloop[mp->loopstart];
      MLoop *l_prev = (l + (mp->totloop - 1));
      int j;
      for (j = 0; j < mp->totloop; j++, l++) {
        /* lookup hashed edge index */
        l_prev->e = POINTER_AS_UINT(BLI_edgehash_lookup(eh, l_prev->v, l->v));
        l_prev = l;
      }
    }
  }

  BLI_edgehash_free(eh, NULL);
}

/* Initialize mverts, medges and, faces for converting nurbs to mesh and derived mesh */
/* return non-zero on error */
int BKE_mesh_nurbs_to_mdata(Object *ob,
                            MVert **r_allvert,
                            int *r_totvert,
                            MEdge **r_alledge,
                            int *r_totedge,
                            MLoop **r_allloop,
                            MPoly **r_allpoly,
                            int *r_totloop,
                            int *r_totpoly)
{
  ListBase disp = {NULL, NULL};

  if (ob->runtime.curve_cache) {
    disp = ob->runtime.curve_cache->disp;
  }

  return BKE_mesh_nurbs_displist_to_mdata(ob,
                                          &disp,
                                          r_allvert,
                                          r_totvert,
                                          r_alledge,
                                          r_totedge,
                                          r_allloop,
                                          r_allpoly,
                                          NULL,
                                          r_totloop,
                                          r_totpoly);
}

/* BMESH: this doesn't calculate all edges from polygons,
 * only free standing edges are calculated */

/* Initialize mverts, medges and, faces for converting nurbs to mesh and derived mesh */
/* use specified dispbase */
int BKE_mesh_nurbs_displist_to_mdata(Object *ob,
                                     const ListBase *dispbase,
                                     MVert **r_allvert,
                                     int *r_totvert,
                                     MEdge **r_alledge,
                                     int *r_totedge,
                                     MLoop **r_allloop,
                                     MPoly **r_allpoly,
                                     MLoopUV **r_alluv,
                                     int *r_totloop,
                                     int *r_totpoly)
{
  Curve *cu = ob->data;
  DispList *dl;
  MVert *mvert;
  MPoly *mpoly;
  MLoop *mloop;
  MLoopUV *mloopuv = NULL;
  MEdge *medge;
  const float *data;
  int a, b, ofs, vertcount, startvert, totvert = 0, totedge = 0, totloop = 0, totpoly = 0;
  int p1, p2, p3, p4, *index;
  const bool conv_polys = ((CU_DO_2DFILL(cu) ==
                            false) || /* 2d polys are filled with DL_INDEX3 displists */
                           (ob->type == OB_SURF)); /* surf polys are never filled */

  /* count */
  dl = dispbase->first;
  while (dl) {
    if (dl->type == DL_SEGM) {
      totvert += dl->parts * dl->nr;
      totedge += dl->parts * (dl->nr - 1);
    }
    else if (dl->type == DL_POLY) {
      if (conv_polys) {
        totvert += dl->parts * dl->nr;
        totedge += dl->parts * dl->nr;
      }
    }
    else if (dl->type == DL_SURF) {
      int tot;
      totvert += dl->parts * dl->nr;
      tot = (dl->parts - 1 + ((dl->flag & DL_CYCL_V) == 2)) *
            (dl->nr - 1 + (dl->flag & DL_CYCL_U));
      totpoly += tot;
      totloop += tot * 4;
    }
    else if (dl->type == DL_INDEX3) {
      int tot;
      totvert += dl->nr;
      tot = dl->parts;
      totpoly += tot;
      totloop += tot * 3;
    }
    dl = dl->next;
  }

  if (totvert == 0) {
    /* error("can't convert"); */
    /* Make Sure you check ob->data is a curve */
    return -1;
  }

  *r_allvert = mvert = MEM_calloc_arrayN(totvert, sizeof(MVert), "nurbs_init mvert");
  *r_alledge = medge = MEM_calloc_arrayN(totedge, sizeof(MEdge), "nurbs_init medge");
  *r_allloop = mloop = MEM_calloc_arrayN(
      totpoly, 4 * sizeof(MLoop), "nurbs_init mloop");  // totloop
  *r_allpoly = mpoly = MEM_calloc_arrayN(totpoly, sizeof(MPoly), "nurbs_init mloop");

  if (r_alluv) {
    *r_alluv = mloopuv = MEM_calloc_arrayN(totpoly, 4 * sizeof(MLoopUV), "nurbs_init mloopuv");
  }

  /* verts and faces */
  vertcount = 0;

  dl = dispbase->first;
  while (dl) {
    const bool is_smooth = (dl->rt & CU_SMOOTH) != 0;

    if (dl->type == DL_SEGM) {
      startvert = vertcount;
      a = dl->parts * dl->nr;
      data = dl->verts;
      while (a--) {
        copy_v3_v3(mvert->co, data);
        data += 3;
        vertcount++;
        mvert++;
      }

      for (a = 0; a < dl->parts; a++) {
        ofs = a * dl->nr;
        for (b = 1; b < dl->nr; b++) {
          medge->v1 = startvert + ofs + b - 1;
          medge->v2 = startvert + ofs + b;
          medge->flag = ME_LOOSEEDGE | ME_EDGERENDER | ME_EDGEDRAW;

          medge++;
        }
      }
    }
    else if (dl->type == DL_POLY) {
      if (conv_polys) {
        startvert = vertcount;
        a = dl->parts * dl->nr;
        data = dl->verts;
        while (a--) {
          copy_v3_v3(mvert->co, data);
          data += 3;
          vertcount++;
          mvert++;
        }

        for (a = 0; a < dl->parts; a++) {
          ofs = a * dl->nr;
          for (b = 0; b < dl->nr; b++) {
            medge->v1 = startvert + ofs + b;
            if (b == dl->nr - 1) {
              medge->v2 = startvert + ofs;
            }
            else {
              medge->v2 = startvert + ofs + b + 1;
            }
            medge->flag = ME_LOOSEEDGE | ME_EDGERENDER | ME_EDGEDRAW;
            medge++;
          }
        }
      }
    }
    else if (dl->type == DL_INDEX3) {
      startvert = vertcount;
      a = dl->nr;
      data = dl->verts;
      while (a--) {
        copy_v3_v3(mvert->co, data);
        data += 3;
        vertcount++;
        mvert++;
      }

      a = dl->parts;
      index = dl->index;
      while (a--) {
        mloop[0].v = startvert + index[0];
        mloop[1].v = startvert + index[2];
        mloop[2].v = startvert + index[1];
        mpoly->loopstart = (int)(mloop - (*r_allloop));
        mpoly->totloop = 3;
        mpoly->mat_nr = dl->col;

        if (mloopuv) {
          int i;

          for (i = 0; i < 3; i++, mloopuv++) {
            mloopuv->uv[0] = (mloop[i].v - startvert) / (float)(dl->nr - 1);
            mloopuv->uv[1] = 0.0f;
          }
        }

        if (is_smooth) {
          mpoly->flag |= ME_SMOOTH;
        }
        mpoly++;
        mloop += 3;
        index += 3;
      }
    }
    else if (dl->type == DL_SURF) {
      startvert = vertcount;
      a = dl->parts * dl->nr;
      data = dl->verts;
      while (a--) {
        copy_v3_v3(mvert->co, data);
        data += 3;
        vertcount++;
        mvert++;
      }

      for (a = 0; a < dl->parts; a++) {

        if ((dl->flag & DL_CYCL_V) == 0 && a == dl->parts - 1) {
          break;
        }

        if (dl->flag & DL_CYCL_U) {    /* p2 -> p1 -> */
          p1 = startvert + dl->nr * a; /* p4 -> p3 -> */
          p2 = p1 + dl->nr - 1;        /* -----> next row */
          p3 = p1 + dl->nr;
          p4 = p2 + dl->nr;
          b = 0;
        }
        else {
          p2 = startvert + dl->nr * a;
          p1 = p2 + 1;
          p4 = p2 + dl->nr;
          p3 = p1 + dl->nr;
          b = 1;
        }
        if ((dl->flag & DL_CYCL_V) && a == dl->parts - 1) {
          p3 -= dl->parts * dl->nr;
          p4 -= dl->parts * dl->nr;
        }

        for (; b < dl->nr; b++) {
          mloop[0].v = p1;
          mloop[1].v = p3;
          mloop[2].v = p4;
          mloop[3].v = p2;
          mpoly->loopstart = (int)(mloop - (*r_allloop));
          mpoly->totloop = 4;
          mpoly->mat_nr = dl->col;

          if (mloopuv) {
            int orco_sizeu = dl->nr - 1;
            int orco_sizev = dl->parts - 1;
            int i;

            /* exception as handled in convertblender.c too */
            if (dl->flag & DL_CYCL_U) {
              orco_sizeu++;
              if (dl->flag & DL_CYCL_V) {
                orco_sizev++;
              }
            }
            else if (dl->flag & DL_CYCL_V) {
              orco_sizev++;
            }

            for (i = 0; i < 4; i++, mloopuv++) {
              /* find uv based on vertex index into grid array */
              int v = mloop[i].v - startvert;

              mloopuv->uv[0] = (v / dl->nr) / (float)orco_sizev;
              mloopuv->uv[1] = (v % dl->nr) / (float)orco_sizeu;

              /* cyclic correction */
              if ((i == 1 || i == 2) && mloopuv->uv[0] == 0.0f) {
                mloopuv->uv[0] = 1.0f;
              }
              if ((i == 0 || i == 1) && mloopuv->uv[1] == 0.0f) {
                mloopuv->uv[1] = 1.0f;
              }
            }
          }

          if (is_smooth) {
            mpoly->flag |= ME_SMOOTH;
          }
          mpoly++;
          mloop += 4;

          p4 = p3;
          p3++;
          p2 = p1;
          p1++;
        }
      }
    }

    dl = dl->next;
  }

  if (totpoly) {
    make_edges_mdata_extend(r_alledge, &totedge, *r_allpoly, *r_allloop, totpoly);
  }

  *r_totpoly = totpoly;
  *r_totloop = totloop;
  *r_totedge = totedge;
  *r_totvert = totvert;

  return 0;
}

Mesh *BKE_mesh_new_nomain_from_curve_displist(Object *ob, ListBase *dispbase)
{
  Curve *cu = ob->data;
  Mesh *mesh;
  MVert *allvert;
  MEdge *alledge;
  MLoop *allloop;
  MPoly *allpoly;
  MLoopUV *alluv = NULL;
  int totvert, totedge, totloop, totpoly;
  bool use_orco_uv = (cu->flag & CU_UV_ORCO) != 0;

  if (BKE_mesh_nurbs_displist_to_mdata(ob,
                                       dispbase,
                                       &allvert,
                                       &totvert,
                                       &alledge,
                                       &totedge,
                                       &allloop,
                                       &allpoly,
                                       (use_orco_uv) ? &alluv : NULL,
                                       &totloop,
                                       &totpoly) != 0) {
    /* Error initializing mdata. This often happens when curve is empty */
    return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  }

  mesh = BKE_mesh_new_nomain(totvert, totedge, 0, totloop, totpoly);
  mesh->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

  memcpy(mesh->mvert, allvert, totvert * sizeof(MVert));
  memcpy(mesh->medge, alledge, totedge * sizeof(MEdge));
  memcpy(mesh->mloop, allloop, totloop * sizeof(MLoop));
  memcpy(mesh->mpoly, allpoly, totpoly * sizeof(MPoly));

  if (alluv) {
    const char *uvname = "Orco";
    CustomData_add_layer_named(&mesh->ldata, CD_MLOOPUV, CD_ASSIGN, alluv, totloop, uvname);
  }

  MEM_freeN(allvert);
  MEM_freeN(alledge);
  MEM_freeN(allloop);
  MEM_freeN(allpoly);

  return mesh;
}

Mesh *BKE_mesh_new_nomain_from_curve(Object *ob)
{
  ListBase disp = {NULL, NULL};

  if (ob->runtime.curve_cache) {
    disp = ob->runtime.curve_cache->disp;
  }

  return BKE_mesh_new_nomain_from_curve_displist(ob, &disp);
}

/* this may fail replacing ob->data, be sure to check ob->type */
void BKE_mesh_from_nurbs_displist(Main *bmain,
                                  Object *ob,
                                  ListBase *dispbase,
                                  const bool use_orco_uv,
                                  const char *obdata_name,
                                  bool temporary)
{
  Object *ob1;
  Mesh *me_eval = ob->runtime.mesh_eval;
  Mesh *me;
  Curve *cu;
  MVert *allvert = NULL;
  MEdge *alledge = NULL;
  MLoop *allloop = NULL;
  MLoopUV *alluv = NULL;
  MPoly *allpoly = NULL;
  int totvert, totedge, totloop, totpoly;

  cu = ob->data;

  if (me_eval == NULL) {
    if (BKE_mesh_nurbs_displist_to_mdata(ob,
                                         dispbase,
                                         &allvert,
                                         &totvert,
                                         &alledge,
                                         &totedge,
                                         &allloop,
                                         &allpoly,
                                         (use_orco_uv) ? &alluv : NULL,
                                         &totloop,
                                         &totpoly) != 0) {
      /* Error initializing */
      return;
    }

    /* make mesh */
    me = BKE_mesh_add(bmain, obdata_name);
    me->totvert = totvert;
    me->totedge = totedge;
    me->totloop = totloop;
    me->totpoly = totpoly;

    me->mvert = CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, allvert, me->totvert);
    me->medge = CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, alledge, me->totedge);
    me->mloop = CustomData_add_layer(&me->ldata, CD_MLOOP, CD_ASSIGN, allloop, me->totloop);
    me->mpoly = CustomData_add_layer(&me->pdata, CD_MPOLY, CD_ASSIGN, allpoly, me->totpoly);

    if (alluv) {
      const char *uvname = "Orco";
      me->mloopuv = CustomData_add_layer_named(
          &me->ldata, CD_MLOOPUV, CD_ASSIGN, alluv, me->totloop, uvname);
    }

    BKE_mesh_calc_normals(me);
  }
  else {
    me = BKE_mesh_add(bmain, obdata_name);
    ob->runtime.mesh_eval = NULL;
    BKE_mesh_nomain_to_mesh(me_eval, me, ob, &CD_MASK_MESH, true);
  }

  me->totcol = cu->totcol;
  me->mat = cu->mat;

  /* Copy evaluated texture space from curve to mesh.
   *
   * Note that we disable auto texture space feature since that will cause
   * texture space to evaluate differently for curve and mesh, since curve
   * uses CV to calculate bounding box, and mesh uses what is coming from
   * tessellated curve.
   */
  me->texflag = cu->texflag & ~CU_AUTOSPACE;
  copy_v3_v3(me->loc, cu->loc);
  copy_v3_v3(me->size, cu->size);
  copy_v3_v3(me->rot, cu->rot);
  BKE_mesh_texspace_calc(me);

  cu->mat = NULL;
  cu->totcol = 0;

  /* Do not decrement ob->data usercount here,
   * it's done at end of func with BKE_id_free_us() call. */
  ob->data = me;
  ob->type = OB_MESH;

  /* other users */
  ob1 = bmain->objects.first;
  while (ob1) {
    if (ob1->data == cu) {
      ob1->type = OB_MESH;

      id_us_min((ID *)ob1->data);
      ob1->data = ob->data;
      id_us_plus((ID *)ob1->data);
    }
    ob1 = ob1->id.next;
  }

  if (temporary) {
    /* For temporary objects in BKE_mesh_new_from_object don't remap
     * the entire scene with associated depsgraph updates, which are
     * problematic for renderers exporting data. */
    BKE_id_free(NULL, cu);
  }
  else {
    BKE_id_free_us(bmain, cu);
  }
}

void BKE_mesh_from_nurbs(Main *bmain, Object *ob)
{
  Curve *cu = (Curve *)ob->data;
  bool use_orco_uv = (cu->flag & CU_UV_ORCO) != 0;
  ListBase disp = {NULL, NULL};

  if (ob->runtime.curve_cache) {
    disp = ob->runtime.curve_cache->disp;
  }

  BKE_mesh_from_nurbs_displist(bmain, ob, &disp, use_orco_uv, cu->id.name, false);
}

typedef struct EdgeLink {
  struct EdgeLink *next, *prev;
  void *edge;
} EdgeLink;

typedef struct VertLink {
  Link *next, *prev;
  unsigned int index;
} VertLink;

static void prependPolyLineVert(ListBase *lb, unsigned int index)
{
  VertLink *vl = MEM_callocN(sizeof(VertLink), "VertLink");
  vl->index = index;
  BLI_addhead(lb, vl);
}

static void appendPolyLineVert(ListBase *lb, unsigned int index)
{
  VertLink *vl = MEM_callocN(sizeof(VertLink), "VertLink");
  vl->index = index;
  BLI_addtail(lb, vl);
}

void BKE_mesh_to_curve_nurblist(const Mesh *me, ListBase *nurblist, const int edge_users_test)
{
  MVert *mvert = me->mvert;
  MEdge *med, *medge = me->medge;
  MPoly *mp, *mpoly = me->mpoly;
  MLoop *mloop = me->mloop;

  int medge_len = me->totedge;
  int mpoly_len = me->totpoly;
  int totedges = 0;
  int i;

  /* only to detect edge polylines */
  int *edge_users;

  ListBase edges = {NULL, NULL};

  /* get boundary edges */
  edge_users = MEM_calloc_arrayN(medge_len, sizeof(int), __func__);
  for (i = 0, mp = mpoly; i < mpoly_len; i++, mp++) {
    MLoop *ml = &mloop[mp->loopstart];
    int j;
    for (j = 0; j < mp->totloop; j++, ml++) {
      edge_users[ml->e]++;
    }
  }

  /* create edges from all faces (so as to find edges not in any faces) */
  med = medge;
  for (i = 0; i < medge_len; i++, med++) {
    if (edge_users[i] == edge_users_test) {
      EdgeLink *edl = MEM_callocN(sizeof(EdgeLink), "EdgeLink");
      edl->edge = med;

      BLI_addtail(&edges, edl);
      totedges++;
    }
  }
  MEM_freeN(edge_users);

  if (edges.first) {
    while (edges.first) {
      /* each iteration find a polyline and add this as a nurbs poly spline */

      ListBase polyline = {NULL, NULL}; /* store a list of VertLink's */
      bool closed = false;
      int totpoly = 0;
      MEdge *med_current = ((EdgeLink *)edges.last)->edge;
      unsigned int startVert = med_current->v1;
      unsigned int endVert = med_current->v2;
      bool ok = true;

      appendPolyLineVert(&polyline, startVert);
      totpoly++;
      appendPolyLineVert(&polyline, endVert);
      totpoly++;
      BLI_freelinkN(&edges, edges.last);
      totedges--;

      while (ok) { /* while connected edges are found... */
        EdgeLink *edl = edges.last;
        ok = false;
        while (edl) {
          EdgeLink *edl_prev = edl->prev;

          med = edl->edge;

          if (med->v1 == endVert) {
            endVert = med->v2;
            appendPolyLineVert(&polyline, med->v2);
            totpoly++;
            BLI_freelinkN(&edges, edl);
            totedges--;
            ok = true;
          }
          else if (med->v2 == endVert) {
            endVert = med->v1;
            appendPolyLineVert(&polyline, endVert);
            totpoly++;
            BLI_freelinkN(&edges, edl);
            totedges--;
            ok = true;
          }
          else if (med->v1 == startVert) {
            startVert = med->v2;
            prependPolyLineVert(&polyline, startVert);
            totpoly++;
            BLI_freelinkN(&edges, edl);
            totedges--;
            ok = true;
          }
          else if (med->v2 == startVert) {
            startVert = med->v1;
            prependPolyLineVert(&polyline, startVert);
            totpoly++;
            BLI_freelinkN(&edges, edl);
            totedges--;
            ok = true;
          }

          edl = edl_prev;
        }
      }

      /* Now we have a polyline, make into a curve */
      if (startVert == endVert) {
        BLI_freelinkN(&polyline, polyline.last);
        totpoly--;
        closed = true;
      }

      /* --- nurbs --- */
      {
        Nurb *nu;
        BPoint *bp;
        VertLink *vl;

        /* create new 'nurb' within the curve */
        nu = (Nurb *)MEM_callocN(sizeof(Nurb), "MeshNurb");

        nu->pntsu = totpoly;
        nu->pntsv = 1;
        nu->orderu = 4;
        nu->flagu = CU_NURB_ENDPOINT | (closed ? CU_NURB_CYCLIC : 0); /* endpoint */
        nu->resolu = 12;

        nu->bp = (BPoint *)MEM_calloc_arrayN(totpoly, sizeof(BPoint), "bpoints");

        /* add points */
        vl = polyline.first;
        for (i = 0, bp = nu->bp; i < totpoly; i++, bp++, vl = (VertLink *)vl->next) {
          copy_v3_v3(bp->vec, mvert[vl->index].co);
          bp->f1 = SELECT;
          bp->radius = bp->weight = 1.0;
        }
        BLI_freelistN(&polyline);

        /* add nurb to curve */
        BLI_addtail(nurblist, nu);
      }
      /* --- done with nurbs --- */
    }
  }
}

void BKE_mesh_to_curve(Main *bmain, Depsgraph *depsgraph, Scene *UNUSED(scene), Object *ob)
{
  /* make new mesh data from the original copy */
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  Mesh *me_eval = mesh_get_eval_final(depsgraph, scene_eval, ob_eval, &CD_MASK_MESH);
  ListBase nurblist = {NULL, NULL};

  BKE_mesh_to_curve_nurblist(me_eval, &nurblist, 0);
  BKE_mesh_to_curve_nurblist(me_eval, &nurblist, 1);

  if (nurblist.first) {
    Curve *cu = BKE_curve_add(bmain, ob->id.name + 2, OB_CURVE);
    cu->flag |= CU_3D;

    cu->nurb = nurblist;

    id_us_min(&((Mesh *)ob->data)->id);
    ob->data = cu;
    ob->type = OB_CURVE;

    BKE_object_free_derived_caches(ob);
  }
}

/* settings: 1 - preview, 2 - render
 *
 * The convention goes as following:
 *
 * - Passing original object with apply_modifiers=false will give a
 *   non-modified non-deformed mesh.
 *   The result mesh will point to datablocks from the original "domain". For
 *   example, materials will be original.
 *
 * - Passing original object with apply_modifiers=true will give a mesh which
 *   has all modifiers applied.
 *   The result mesh will point to datablocks from the original "domain". For
 *   example, materials will be original.
 *
 * - Passing evaluated object will ignore apply_modifiers argument, and the
 *   result always contains all modifiers applied.
 *   The result mesh will point to an evaluated datablocks. For example,
 *   materials will be an evaluated IDs from the dependency graph.
 */
Mesh *BKE_mesh_new_from_object(Depsgraph *depsgraph,
                               Main *bmain,
                               Scene *sce,
                               Object *ob,
                               const bool apply_modifiers,
                               const bool calc_undeformed)
{
  Mesh *tmpmesh;
  Curve *tmpcu = NULL, *copycu;
  int i;
  const bool render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  bool effective_apply_modifiers = apply_modifiers;
  bool do_mat_id_data_us = true;

  Object *object_input = ob;
  Object *object_eval = DEG_get_evaluated_object(depsgraph, object_input);
  Object object_for_eval;

  if (object_eval == object_input) {
    /* Evaluated mesh contains all modifiers applied already.
     * The other types of object has them applied, but are stored in other
     * data structures than a mesh. So need to apply modifiers again on a
     * temporary copy before converting result to mesh. */
    if (object_input->type == OB_MESH) {
      effective_apply_modifiers = false;
    }
    else {
      effective_apply_modifiers = true;
    }
    object_for_eval = *object_eval;
  }
  else {
    if (apply_modifiers) {
      object_for_eval = *object_eval;
      if (object_for_eval.runtime.mesh_orig != NULL) {
        object_for_eval.data = object_for_eval.runtime.mesh_orig;
      }
    }
    else {
      object_for_eval = *object_input;
    }
  }

  const bool cage = !effective_apply_modifiers;

  /* perform the mesh extraction based on type */
  switch (object_for_eval.type) {
    case OB_FONT:
    case OB_CURVE:
    case OB_SURF: {
      ListBase dispbase = {NULL, NULL};
      Mesh *me_eval_final = NULL;
      int uv_from_orco;

      /* copies object and modifiers (but not the data) */
      Object *tmpobj;
      BKE_id_copy_ex(NULL, &object_for_eval.id, (ID **)&tmpobj, LIB_ID_COPY_LOCALIZE);
      tmpcu = (Curve *)tmpobj->data;

      /* Copy cached display list, it might be needed by the stack evaluation.
       * Ideally stack should be able to use render-time display list, but doing
       * so is quite tricky and not safe so close to the release.
       *
       * TODO(sergey): Look into more proper solution.
       */
      if (object_for_eval.runtime.curve_cache != NULL) {
        if (tmpobj->runtime.curve_cache == NULL) {
          tmpobj->runtime.curve_cache = MEM_callocN(sizeof(CurveCache),
                                                    "CurveCache for curve types");
        }
        BKE_displist_copy(&tmpobj->runtime.curve_cache->disp,
                          &object_for_eval.runtime.curve_cache->disp);
      }

      /* if getting the original caged mesh, delete object modifiers */
      if (cage) {
        BKE_object_free_modifiers(tmpobj, LIB_ID_CREATE_NO_USER_REFCOUNT);
      }

      /* copies the data, but *not* the shapekeys. */
      BKE_id_copy_ex(NULL, object_for_eval.data, (ID **)&copycu, LIB_ID_COPY_LOCALIZE);
      tmpobj->data = copycu;

      /* make sure texture space is calculated for a copy of curve,
       * it will be used for the final result.
       */
      BKE_curve_texspace_calc(copycu);

      /* temporarily set edit so we get updates from edit mode, but
       * also because for text datablocks copying it while in edit
       * mode gives invalid data structures */
      copycu->editfont = tmpcu->editfont;
      copycu->editnurb = tmpcu->editnurb;

      /* get updated display list, and convert to a mesh */
      BKE_displist_make_curveTypes_forRender(
          depsgraph, sce, tmpobj, &dispbase, &me_eval_final, false, render, NULL);

      copycu->editfont = NULL;
      copycu->editnurb = NULL;

      tmpobj->runtime.mesh_eval = me_eval_final;

      /* convert object type to mesh */
      uv_from_orco = (tmpcu->flag & CU_UV_ORCO) != 0;
      BKE_mesh_from_nurbs_displist(
          bmain, tmpobj, &dispbase, uv_from_orco, tmpcu->id.name + 2, true);
      /* Function above also frees copycu (aka tmpobj->data), make this obvious here. */
      copycu = NULL;

      tmpmesh = tmpobj->data;
      id_us_min(
          &tmpmesh->id); /* Gets one user from its creation in BKE_mesh_from_nurbs_displist(). */

      BKE_displist_free(&dispbase);

      /* BKE_mesh_from_nurbs changes the type to a mesh, check it worked.
       * if it didn't the curve did not have any segments or otherwise
       * would have generated an empty mesh */
      if (tmpobj->type != OB_MESH) {
        BKE_id_free(NULL, tmpobj);
        return NULL;
      }

      BKE_id_free(NULL, tmpobj);

      /* XXX The curve to mesh conversion is convoluted...
       *     But essentially, BKE_mesh_from_nurbs_displist()
       *     already transfers the ownership of materials from the temp copy of the Curve ID to the
       *     new Mesh ID, so we do not want to increase materials' usercount later. */
      do_mat_id_data_us = false;

      break;
    }

    case OB_MBALL: {
      /* metaballs don't have modifiers, so just convert to mesh */
      Object *basis_ob = BKE_mball_basis_find(sce, object_input);
      /* todo, re-generatre for render-res */
      /* metaball_polygonize(scene, ob) */

      if (basis_ob != object_input) {
        /* Only do basis metaball. */
        return NULL;
      }

      tmpmesh = BKE_mesh_add(bmain, ((ID *)object_for_eval.data)->name + 2);
      /* BKE_mesh_add gives us a user count we don't need */
      id_us_min(&tmpmesh->id);

      if (render) {
        ListBase disp = {NULL, NULL};
        BKE_displist_make_mball_forRender(depsgraph, sce, &object_for_eval, &disp);
        BKE_mesh_from_metaball(&disp, tmpmesh);
        BKE_displist_free(&disp);
      }
      else {
        ListBase disp = {NULL, NULL};
        if (object_for_eval.runtime.curve_cache) {
          disp = object_for_eval.runtime.curve_cache->disp;
        }
        BKE_mesh_from_metaball(&disp, tmpmesh);
      }

      BKE_mesh_texspace_copy_from_object(tmpmesh, &object_for_eval);

      break;
    }
    case OB_MESH:
      /* copies object and modifiers (but not the data) */
      if (cage) {
        /* copies the data (but *not* the shapekeys). */
        Mesh *mesh = object_for_eval.data;
        BKE_id_copy_ex(bmain, &mesh->id, (ID **)&tmpmesh, 0);
        /* XXX BKE_mesh_copy() already handles materials usercount. */
        do_mat_id_data_us = false;
      }
      /* if not getting the original caged mesh, get final derived mesh */
      else {
        /* Make a dummy mesh, saves copying */
        Mesh *me_eval;
        CustomData_MeshMasks mask = CD_MASK_MESH; /* this seems more suitable, exporter,
                                                   * for example, needs CD_MASK_MDEFORMVERT */

        if (calc_undeformed) {
          mask.vmask |= CD_MASK_ORCO;
        }

        if (render) {
          me_eval = mesh_create_eval_final_render(depsgraph, sce, &object_for_eval, &mask);
        }
        else {
          me_eval = mesh_create_eval_final_view(depsgraph, sce, &object_for_eval, &mask);
        }

        tmpmesh = BKE_mesh_add(bmain, ((ID *)object_for_eval.data)->name + 2);
        BKE_mesh_nomain_to_mesh(me_eval, tmpmesh, &object_for_eval, &mask, true);

        /* Copy autosmooth settings from original mesh. */
        Mesh *me = (Mesh *)object_for_eval.data;
        tmpmesh->flag |= (me->flag & ME_AUTOSMOOTH);
        tmpmesh->smoothresh = me->smoothresh;
      }

      /* BKE_mesh_add/copy gives us a user count we don't need */
      id_us_min(&tmpmesh->id);

      break;
    default:
      /* "Object does not have geometry data") */
      return NULL;
  }

  /* Copy materials to new mesh */
  switch (object_for_eval.type) {
    case OB_SURF:
    case OB_FONT:
    case OB_CURVE:
      tmpmesh->totcol = tmpcu->totcol;

      /* free old material list (if it exists) and adjust user counts */
      if (tmpcu->mat) {
        for (i = tmpcu->totcol; i-- > 0;) {
          /* are we an object material or data based? */
          tmpmesh->mat[i] = give_current_material(object_input, i + 1);

          if (((object_for_eval.matbits && object_for_eval.matbits[i]) || do_mat_id_data_us) &&
              tmpmesh->mat[i]) {
            id_us_plus(&tmpmesh->mat[i]->id);
          }
        }
      }
      break;

    case OB_MBALL: {
      MetaBall *tmpmb = (MetaBall *)object_for_eval.data;
      tmpmesh->mat = MEM_dupallocN(tmpmb->mat);
      tmpmesh->totcol = tmpmb->totcol;

      /* free old material list (if it exists) and adjust user counts */
      if (tmpmb->mat) {
        for (i = tmpmb->totcol; i-- > 0;) {
          /* are we an object material or data based? */
          tmpmesh->mat[i] = give_current_material(object_input, i + 1);

          if (((object_for_eval.matbits && object_for_eval.matbits[i]) || do_mat_id_data_us) &&
              tmpmesh->mat[i]) {
            id_us_plus(&tmpmesh->mat[i]->id);
          }
        }
      }
      break;
    }

    case OB_MESH:
      if (!cage) {
        Mesh *origmesh = object_for_eval.data;
        tmpmesh->flag = origmesh->flag;
        tmpmesh->mat = MEM_dupallocN(origmesh->mat);
        tmpmesh->totcol = origmesh->totcol;
        tmpmesh->smoothresh = origmesh->smoothresh;
        if (origmesh->mat) {
          for (i = origmesh->totcol; i-- > 0;) {
            /* are we an object material or data based? */
            tmpmesh->mat[i] = give_current_material(object_input, i + 1);

            if (((object_for_eval.matbits && object_for_eval.matbits[i]) || do_mat_id_data_us) &&
                tmpmesh->mat[i]) {
              id_us_plus(&tmpmesh->mat[i]->id);
            }
          }
        }
      }
      break;
  } /* end copy materials */

  return tmpmesh;
}

static void add_shapekey_layers(Mesh *mesh_dest, Mesh *mesh_src)
{
  KeyBlock *kb;
  Key *key = mesh_src->key;
  int i;

  if (!mesh_src->key) {
    return;
  }

  /* ensure we can use mesh vertex count for derived mesh custom data */
  if (mesh_src->totvert != mesh_dest->totvert) {
    CLOG_ERROR(&LOG,
               "vertex size mismatch (mesh/dm) '%s' (%d != %d)",
               mesh_src->id.name + 2,
               mesh_src->totvert,
               mesh_dest->totvert);
    return;
  }

  for (i = 0, kb = key->block.first; kb; kb = kb->next, i++) {
    int ci;
    float *array;

    if (mesh_src->totvert != kb->totelem) {
      CLOG_ERROR(&LOG,
                 "vertex size mismatch (Mesh '%s':%d != KeyBlock '%s':%d)",
                 mesh_src->id.name + 2,
                 mesh_src->totvert,
                 kb->name,
                 kb->totelem);
      array = MEM_calloc_arrayN((size_t)mesh_src->totvert, 3 * sizeof(float), __func__);
    }
    else {
      array = MEM_malloc_arrayN((size_t)mesh_src->totvert, 3 * sizeof(float), __func__);
      memcpy(array, kb->data, (size_t)mesh_src->totvert * 3 * sizeof(float));
    }

    CustomData_add_layer_named(
        &mesh_dest->vdata, CD_SHAPEKEY, CD_ASSIGN, array, mesh_dest->totvert, kb->name);
    ci = CustomData_get_layer_index_n(&mesh_dest->vdata, CD_SHAPEKEY, i);

    mesh_dest->vdata.layers[ci].uid = kb->uid;
  }
}

Mesh *BKE_mesh_create_derived_for_modifier(struct Depsgraph *depsgraph,
                                           Scene *scene,
                                           Object *ob_eval,
                                           ModifierData *md_eval,
                                           int build_shapekey_layers)
{
  Mesh *me = ob_eval->runtime.mesh_orig ? ob_eval->runtime.mesh_orig : ob_eval->data;
  const ModifierTypeInfo *mti = modifierType_getInfo(md_eval->type);
  Mesh *result;
  KeyBlock *kb;
  ModifierEvalContext mectx = {depsgraph, ob_eval, 0};

  if (!(md_eval->mode & eModifierMode_Realtime)) {
    return NULL;
  }

  if (mti->isDisabled && mti->isDisabled(scene, md_eval, 0)) {
    return NULL;
  }

  if (build_shapekey_layers && me->key &&
      (kb = BLI_findlink(&me->key->block, ob_eval->shapenr - 1))) {
    BKE_keyblock_convert_to_mesh(kb, me);
  }

  if (mti->type == eModifierTypeType_OnlyDeform) {
    int numVerts;
    float(*deformedVerts)[3] = BKE_mesh_vertexCos_get(me, &numVerts);

    mti->deformVerts(md_eval, &mectx, NULL, deformedVerts, numVerts);
    BKE_id_copy_ex(NULL, &me->id, (ID **)&result, LIB_ID_COPY_LOCALIZE);
    BKE_mesh_apply_vert_coords(result, deformedVerts);

    if (build_shapekey_layers) {
      add_shapekey_layers(result, me);
    }

    MEM_freeN(deformedVerts);
  }
  else {
    Mesh *mesh_temp;
    BKE_id_copy_ex(NULL, &me->id, (ID **)&mesh_temp, LIB_ID_COPY_LOCALIZE);

    if (build_shapekey_layers) {
      add_shapekey_layers(mesh_temp, me);
    }

    result = mti->applyModifier(md_eval, &mectx, mesh_temp);
    ASSERT_IS_VALID_MESH(result);

    if (mesh_temp != result) {
      BKE_id_free(NULL, mesh_temp);
    }
  }

  return result;
}

/* This is a Mesh-based copy of the same function in DerivedMesh.c */
static void shapekey_layers_to_keyblocks(Mesh *mesh_src, Mesh *mesh_dst, int actshape_uid)
{
  KeyBlock *kb;
  int i, j, tot;

  if (!mesh_dst->key) {
    return;
  }

  tot = CustomData_number_of_layers(&mesh_src->vdata, CD_SHAPEKEY);
  for (i = 0; i < tot; i++) {
    CustomDataLayer *layer =
        &mesh_src->vdata.layers[CustomData_get_layer_index_n(&mesh_src->vdata, CD_SHAPEKEY, i)];
    float(*cos)[3], (*kbcos)[3];

    for (kb = mesh_dst->key->block.first; kb; kb = kb->next) {
      if (kb->uid == layer->uid) {
        break;
      }
    }

    if (!kb) {
      kb = BKE_keyblock_add(mesh_dst->key, layer->name);
      kb->uid = layer->uid;
    }

    if (kb->data) {
      MEM_freeN(kb->data);
    }

    cos = CustomData_get_layer_n(&mesh_src->vdata, CD_SHAPEKEY, i);
    kb->totelem = mesh_src->totvert;

    kb->data = kbcos = MEM_malloc_arrayN(kb->totelem, 3 * sizeof(float), __func__);
    if (kb->uid == actshape_uid) {
      MVert *mvert = mesh_src->mvert;

      for (j = 0; j < mesh_src->totvert; j++, kbcos++, mvert++) {
        copy_v3_v3(*kbcos, mvert->co);
      }
    }
    else {
      for (j = 0; j < kb->totelem; j++, cos++, kbcos++) {
        copy_v3_v3(*kbcos, *cos);
      }
    }
  }

  for (kb = mesh_dst->key->block.first; kb; kb = kb->next) {
    if (kb->totelem != mesh_src->totvert) {
      if (kb->data) {
        MEM_freeN(kb->data);
      }

      kb->totelem = mesh_src->totvert;
      kb->data = MEM_calloc_arrayN(kb->totelem, 3 * sizeof(float), __func__);
      CLOG_ERROR(&LOG, "lost a shapekey layer: '%s'! (bmesh internal error)", kb->name);
    }
  }
}

/* This is a Mesh-based copy of DM_to_mesh() */
void BKE_mesh_nomain_to_mesh(Mesh *mesh_src,
                             Mesh *mesh_dst,
                             Object *ob,
                             const CustomData_MeshMasks *mask,
                             bool take_ownership)
{
  /* mesh_src might depend on mesh_dst, so we need to do everything with a local copy */
  /* TODO(Sybren): the above claim came from DM_to_mesh();
   * check whether it is still true with Mesh */
  Mesh tmp = *mesh_dst;
  int totvert, totedge /*, totface */ /* UNUSED */, totloop, totpoly;
  int did_shapekeys = 0;
  eCDAllocType alloctype = CD_DUPLICATE;

  if (take_ownership /* && dm->type == DM_TYPE_CDDM && dm->needsFree */) {
    bool has_any_referenced_layers = CustomData_has_referenced(&mesh_src->vdata) ||
                                     CustomData_has_referenced(&mesh_src->edata) ||
                                     CustomData_has_referenced(&mesh_src->ldata) ||
                                     CustomData_has_referenced(&mesh_src->fdata) ||
                                     CustomData_has_referenced(&mesh_src->pdata);
    if (!has_any_referenced_layers) {
      alloctype = CD_ASSIGN;
    }
  }
  CustomData_reset(&tmp.vdata);
  CustomData_reset(&tmp.edata);
  CustomData_reset(&tmp.fdata);
  CustomData_reset(&tmp.ldata);
  CustomData_reset(&tmp.pdata);

  BKE_mesh_ensure_normals(mesh_src);

  totvert = tmp.totvert = mesh_src->totvert;
  totedge = tmp.totedge = mesh_src->totedge;
  totloop = tmp.totloop = mesh_src->totloop;
  totpoly = tmp.totpoly = mesh_src->totpoly;
  tmp.totface = 0;

  CustomData_copy(&mesh_src->vdata, &tmp.vdata, mask->vmask, alloctype, totvert);
  CustomData_copy(&mesh_src->edata, &tmp.edata, mask->emask, alloctype, totedge);
  CustomData_copy(&mesh_src->ldata, &tmp.ldata, mask->lmask, alloctype, totloop);
  CustomData_copy(&mesh_src->pdata, &tmp.pdata, mask->pmask, alloctype, totpoly);
  tmp.cd_flag = mesh_src->cd_flag;
  tmp.runtime.deformed_only = mesh_src->runtime.deformed_only;

  if (CustomData_has_layer(&mesh_src->vdata, CD_SHAPEKEY)) {
    KeyBlock *kb;
    int uid;

    if (ob) {
      kb = BLI_findlink(&mesh_dst->key->block, ob->shapenr - 1);
      if (kb) {
        uid = kb->uid;
      }
      else {
        CLOG_ERROR(&LOG, "could not find active shapekey %d!", ob->shapenr - 1);

        uid = INT_MAX;
      }
    }
    else {
      /* if no object, set to INT_MAX so we don't mess up any shapekey layers */
      uid = INT_MAX;
    }

    shapekey_layers_to_keyblocks(mesh_src, mesh_dst, uid);
    did_shapekeys = 1;
  }

  /* copy texture space */
  if (ob) {
    BKE_mesh_texspace_copy_from_object(&tmp, ob);
  }

  /* not all DerivedMeshes store their verts/edges/faces in CustomData, so
   * we set them here in case they are missing */
  /* TODO(Sybren): we could probably replace CD_ASSIGN with alloctype and
   * always directly pass mesh_src->mxxx, instead of using a ternary operator. */
  if (!CustomData_has_layer(&tmp.vdata, CD_MVERT)) {
    CustomData_add_layer(&tmp.vdata,
                         CD_MVERT,
                         CD_ASSIGN,
                         (alloctype == CD_ASSIGN) ? mesh_src->mvert :
                                                    MEM_dupallocN(mesh_src->mvert),
                         totvert);
  }
  if (!CustomData_has_layer(&tmp.edata, CD_MEDGE)) {
    CustomData_add_layer(&tmp.edata,
                         CD_MEDGE,
                         CD_ASSIGN,
                         (alloctype == CD_ASSIGN) ? mesh_src->medge :
                                                    MEM_dupallocN(mesh_src->medge),
                         totedge);
  }
  if (!CustomData_has_layer(&tmp.pdata, CD_MPOLY)) {
    /* TODO(Sybren): assignment to tmp.mxxx is probably not necessary due to the
     * BKE_mesh_update_customdata_pointers() call below. */
    tmp.mloop = (alloctype == CD_ASSIGN) ? mesh_src->mloop : MEM_dupallocN(mesh_src->mloop);
    tmp.mpoly = (alloctype == CD_ASSIGN) ? mesh_src->mpoly : MEM_dupallocN(mesh_src->mpoly);

    CustomData_add_layer(&tmp.ldata, CD_MLOOP, CD_ASSIGN, tmp.mloop, tmp.totloop);
    CustomData_add_layer(&tmp.pdata, CD_MPOLY, CD_ASSIGN, tmp.mpoly, tmp.totpoly);
  }

  /* object had got displacement layer, should copy this layer to save sculpted data */
  /* NOTE: maybe some other layers should be copied? nazgul */
  if (CustomData_has_layer(&mesh_dst->ldata, CD_MDISPS)) {
    if (totloop == mesh_dst->totloop) {
      MDisps *mdisps = CustomData_get_layer(&mesh_dst->ldata, CD_MDISPS);
      CustomData_add_layer(&tmp.ldata, CD_MDISPS, alloctype, mdisps, totloop);
    }
  }

  /* yes, must be before _and_ after tessellate */
  BKE_mesh_update_customdata_pointers(&tmp, false);

  /* since 2.65 caller must do! */
  // BKE_mesh_tessface_calc(&tmp);

  CustomData_free(&mesh_dst->vdata, mesh_dst->totvert);
  CustomData_free(&mesh_dst->edata, mesh_dst->totedge);
  CustomData_free(&mesh_dst->fdata, mesh_dst->totface);
  CustomData_free(&mesh_dst->ldata, mesh_dst->totloop);
  CustomData_free(&mesh_dst->pdata, mesh_dst->totpoly);

  /* ok, this should now use new CD shapekey data,
   * which should be fed through the modifier
   * stack */
  if (tmp.totvert != mesh_dst->totvert && !did_shapekeys && mesh_dst->key) {
    CLOG_ERROR(&LOG, "YEEK! this should be recoded! Shape key loss!: ID '%s'", tmp.id.name);
    if (tmp.key && !(tmp.id.tag & LIB_TAG_NO_MAIN)) {
      id_us_min(&tmp.key->id);
    }
    tmp.key = NULL;
  }

  /* Clear selection history */
  MEM_SAFE_FREE(tmp.mselect);
  tmp.totselect = 0;
  BLI_assert(ELEM(tmp.bb, NULL, mesh_dst->bb));
  if (mesh_dst->bb) {
    MEM_freeN(mesh_dst->bb);
    tmp.bb = NULL;
  }

  /* skip the listbase */
  MEMCPY_STRUCT_AFTER(mesh_dst, &tmp, id.prev);

  if (take_ownership) {
    if (alloctype == CD_ASSIGN) {
      CustomData_free_typemask(&mesh_src->vdata, mesh_src->totvert, ~mask->vmask);
      CustomData_free_typemask(&mesh_src->edata, mesh_src->totedge, ~mask->emask);
      CustomData_free_typemask(&mesh_src->ldata, mesh_src->totloop, ~mask->lmask);
      CustomData_free_typemask(&mesh_src->pdata, mesh_src->totpoly, ~mask->pmask);
    }
    BKE_id_free(NULL, mesh_src);
  }
}

/* This is a Mesh-based copy of DM_to_meshkey() */
void BKE_mesh_nomain_to_meshkey(Mesh *mesh_src, Mesh *mesh_dst, KeyBlock *kb)
{
  int a, totvert = mesh_src->totvert;
  float *fp;
  MVert *mvert;

  if (totvert == 0 || mesh_dst->totvert == 0 || mesh_dst->totvert != totvert) {
    return;
  }

  if (kb->data) {
    MEM_freeN(kb->data);
  }
  kb->data = MEM_malloc_arrayN(mesh_dst->key->elemsize, mesh_dst->totvert, "kb->data");
  kb->totelem = totvert;

  fp = kb->data;
  mvert = mesh_src->mvert;

  for (a = 0; a < kb->totelem; a++, fp += 3, mvert++) {
    copy_v3_v3(fp, mvert->co);
  }
}
