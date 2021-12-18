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

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"

#include "BLI_edgehash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_DerivedMesh.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_editmesh.h"
#include "BKE_geometry_set.hh"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_spline.hh"
/* these 2 are only used by conversion functions */
#include "BKE_curve.h"
/* -- */
#include "BKE_object.h"
/* -- */
#include "BKE_pointcloud.h"

#include "BKE_curve_to_mesh.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

/* Define for cases when you want extra validation of mesh
 * after certain modifications.
 */
// #undef VALIDATE_MESH

#ifdef VALIDATE_MESH
#  define ASSERT_IS_VALID_MESH(mesh) \
    (BLI_assert((mesh == nullptr) || (BKE_mesh_is_valid(mesh) == true)))
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

  dl = (DispList *)lb->first;
  if (dl == nullptr) {
    return;
  }

  if (dl->type == DL_INDEX4) {
    mvert = (MVert *)CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, nullptr, dl->nr);
    allloop = mloop = (MLoop *)CustomData_add_layer(
        &me->ldata, CD_MLOOP, CD_CALLOC, nullptr, dl->parts * 4);
    mpoly = (MPoly *)CustomData_add_layer(&me->pdata, CD_MPOLY, CD_CALLOC, nullptr, dl->parts);
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
  uint eh_reserve;
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
    uint e_index = totedge;

    *r_alledge = medge = (MEdge *)(*r_alledge ?
                                       MEM_reallocN(*r_alledge,
                                                    sizeof(MEdge) * (totedge + totedge_new)) :
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

  BLI_edgehash_free(eh, nullptr);
}

/* Initialize mverts, medges and, faces for converting nurbs to mesh and derived mesh */
/* use specified dispbase */
static int mesh_nurbs_displist_to_mdata(const Curve *cu,
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
  MVert *mvert;
  MPoly *mpoly;
  MLoop *mloop;
  MLoopUV *mloopuv = nullptr;
  MEdge *medge;
  const float *data;
  int a, b, ofs, vertcount, startvert, totvert = 0, totedge = 0, totloop = 0, totpoly = 0;
  int p1, p2, p3, p4, *index;
  const bool conv_polys = (
      /* 2D polys are filled with #DispList.type == #DL_INDEX3. */
      (CU_DO_2DFILL(cu) == false) ||
      /* surf polys are never filled */
      BKE_curve_type_get(cu) == OB_SURF);

  /* count */
  LISTBASE_FOREACH (const DispList *, dl, dispbase) {
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
      if (dl->parts != 0) {
        int tot;
        totvert += dl->parts * dl->nr;
        tot = (((dl->flag & DL_CYCL_U) ? 1 : 0) + (dl->nr - 1)) *
              (((dl->flag & DL_CYCL_V) ? 1 : 0) + (dl->parts - 1));
        totpoly += tot;
        totloop += tot * 4;
      }
    }
    else if (dl->type == DL_INDEX3) {
      int tot;
      totvert += dl->nr;
      tot = dl->parts;
      totpoly += tot;
      totloop += tot * 3;
    }
  }

  if (totvert == 0) {
    /* Make Sure you check ob->data is a curve. */
    // error("can't convert");
    return -1;
  }

  *r_allvert = mvert = (MVert *)MEM_calloc_arrayN(totvert, sizeof(MVert), "nurbs_init mvert");
  *r_alledge = medge = (MEdge *)MEM_calloc_arrayN(totedge, sizeof(MEdge), "nurbs_init medge");
  *r_allloop = mloop = (MLoop *)MEM_calloc_arrayN(
      totpoly, sizeof(MLoop[4]), "nurbs_init mloop"); /* totloop */
  *r_allpoly = mpoly = (MPoly *)MEM_calloc_arrayN(totpoly, sizeof(MPoly), "nurbs_init mloop");

  if (r_alluv) {
    *r_alluv = mloopuv = (MLoopUV *)MEM_calloc_arrayN(
        totpoly, sizeof(MLoopUV[4]), "nurbs_init mloopuv");
  }

  /* verts and faces */
  vertcount = 0;

  LISTBASE_FOREACH (const DispList *, dl, dispbase) {
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
          for (int i = 0; i < 3; i++, mloopuv++) {
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

            for (int i = 0; i < 4; i++, mloopuv++) {
              /* find uv based on vertex index into grid array */
              int v = mloop[i].v - startvert;

              mloopuv->uv[0] = (v / dl->nr) / (float)orco_sizev;
              mloopuv->uv[1] = (v % dl->nr) / (float)orco_sizeu;

              /* cyclic correction */
              if ((ELEM(i, 1, 2)) && mloopuv->uv[0] == 0.0f) {
                mloopuv->uv[0] = 1.0f;
              }
              if ((ELEM(i, 0, 1)) && mloopuv->uv[1] == 0.0f) {
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

/**
 * Copy evaluated texture space from curve to mesh.
 *
 * \note We disable auto texture space feature since that will cause texture space to evaluate
 * differently for curve and mesh, since curves use control points and handles to calculate the
 * bounding box, and mesh uses the tessellated curve.
 */
static void mesh_copy_texture_space_from_curve_type(const Curve *cu, Mesh *me)
{
  me->texflag = cu->texflag & ~CU_AUTOSPACE;
  copy_v3_v3(me->loc, cu->loc);
  copy_v3_v3(me->size, cu->size);
  BKE_mesh_texspace_calc(me);
}

Mesh *BKE_mesh_new_nomain_from_curve_displist(const Object *ob, const ListBase *dispbase)
{
  const Curve *cu = (const Curve *)ob->data;
  Mesh *mesh;
  MVert *allvert;
  MEdge *alledge;
  MLoop *allloop;
  MPoly *allpoly;
  MLoopUV *alluv = nullptr;
  int totvert, totedge, totloop, totpoly;

  if (mesh_nurbs_displist_to_mdata(cu,
                                   dispbase,
                                   &allvert,
                                   &totvert,
                                   &alledge,
                                   &totedge,
                                   &allloop,
                                   &allpoly,
                                   &alluv,
                                   &totloop,
                                   &totpoly) != 0) {
    /* Error initializing mdata. This often happens when curve is empty */
    return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  }

  mesh = BKE_mesh_new_nomain(totvert, totedge, 0, totloop, totpoly);
  BKE_mesh_normals_tag_dirty(mesh);

  if (totvert != 0) {
    memcpy(mesh->mvert, allvert, totvert * sizeof(MVert));
  }
  if (totedge != 0) {
    memcpy(mesh->medge, alledge, totedge * sizeof(MEdge));
  }
  if (totloop != 0) {
    memcpy(mesh->mloop, allloop, totloop * sizeof(MLoop));
  }
  if (totpoly != 0) {
    memcpy(mesh->mpoly, allpoly, totpoly * sizeof(MPoly));
  }

  if (alluv) {
    const char *uvname = "UVMap";
    CustomData_add_layer_named(&mesh->ldata, CD_MLOOPUV, CD_ASSIGN, alluv, totloop, uvname);
  }

  mesh_copy_texture_space_from_curve_type(cu, mesh);

  /* Copy curve materials. */
  mesh->mat = (Material **)MEM_dupallocN(cu->mat);
  mesh->totcol = cu->totcol;

  MEM_freeN(allvert);
  MEM_freeN(alledge);
  MEM_freeN(allloop);
  MEM_freeN(allpoly);

  return mesh;
}

Mesh *BKE_mesh_new_nomain_from_curve(const Object *ob)
{
  ListBase disp = {nullptr, nullptr};

  if (ob->runtime.curve_cache) {
    disp = ob->runtime.curve_cache->disp;
  }

  return BKE_mesh_new_nomain_from_curve_displist(ob, &disp);
}

struct EdgeLink {
  struct EdgeLink *next, *prev;
  void *edge;
};

struct VertLink {
  Link *next, *prev;
  uint index;
};

static void prependPolyLineVert(ListBase *lb, uint index)
{
  VertLink *vl = (VertLink *)MEM_callocN(sizeof(VertLink), "VertLink");
  vl->index = index;
  BLI_addhead(lb, vl);
}

static void appendPolyLineVert(ListBase *lb, uint index)
{
  VertLink *vl = (VertLink *)MEM_callocN(sizeof(VertLink), "VertLink");
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

  ListBase edges = {nullptr, nullptr};

  /* get boundary edges */
  edge_users = (int *)MEM_calloc_arrayN(medge_len, sizeof(int), __func__);
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
      EdgeLink *edl = (EdgeLink *)MEM_callocN(sizeof(EdgeLink), "EdgeLink");
      edl->edge = med;

      BLI_addtail(&edges, edl);
      totedges++;
    }
  }
  MEM_freeN(edge_users);

  if (edges.first) {
    while (edges.first) {
      /* each iteration find a polyline and add this as a nurbs poly spline */

      ListBase polyline = {nullptr, nullptr}; /* store a list of VertLink's */
      bool closed = false;
      int totpoly = 0;
      MEdge *med_current = (MEdge *)((EdgeLink *)edges.last)->edge;
      uint startVert = med_current->v1;
      uint endVert = med_current->v2;
      bool ok = true;

      appendPolyLineVert(&polyline, startVert);
      totpoly++;
      appendPolyLineVert(&polyline, endVert);
      totpoly++;
      BLI_freelinkN(&edges, edges.last);
      totedges--;

      while (ok) { /* while connected edges are found... */
        EdgeLink *edl = (EdgeLink *)edges.last;
        ok = false;
        while (edl) {
          EdgeLink *edl_prev = edl->prev;

          med = (MEdge *)edl->edge;

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
        vl = (VertLink *)polyline.first;
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
  ListBase nurblist = {nullptr, nullptr};

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

void BKE_pointcloud_from_mesh(Mesh *me, PointCloud *pointcloud)
{
  BLI_assert(me != nullptr);

  pointcloud->totpoint = me->totvert;
  CustomData_realloc(&pointcloud->pdata, pointcloud->totpoint);

  /* Copy over all attributes. */
  CustomData_merge(&me->vdata, &pointcloud->pdata, CD_MASK_PROP_ALL, CD_DUPLICATE, me->totvert);
  BKE_pointcloud_update_customdata_pointers(pointcloud);
  CustomData_update_typemap(&pointcloud->pdata);

  MVert *mvert;
  mvert = me->mvert;
  for (int i = 0; i < me->totvert; i++, mvert++) {
    copy_v3_v3(pointcloud->co[i], mvert->co);
  }
}

void BKE_mesh_to_pointcloud(Main *bmain, Depsgraph *depsgraph, Scene *UNUSED(scene), Object *ob)
{
  BLI_assert(ob->type == OB_MESH);

  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  Mesh *me_eval = mesh_get_eval_final(depsgraph, scene_eval, ob_eval, &CD_MASK_MESH);

  PointCloud *pointcloud = (PointCloud *)BKE_pointcloud_add(bmain, ob->id.name + 2);

  BKE_pointcloud_from_mesh(me_eval, pointcloud);

  BKE_id_materials_copy(bmain, (ID *)ob->data, (ID *)pointcloud);

  id_us_min(&((Mesh *)ob->data)->id);
  ob->data = pointcloud;
  ob->type = OB_POINTCLOUD;

  BKE_object_free_derived_caches(ob);
}

void BKE_mesh_from_pointcloud(const PointCloud *pointcloud, Mesh *me)
{
  BLI_assert(pointcloud != nullptr);

  me->totvert = pointcloud->totpoint;

  /* Merge over all attributes. */
  CustomData_merge(
      &pointcloud->pdata, &me->vdata, CD_MASK_PROP_ALL, CD_DUPLICATE, pointcloud->totpoint);

  /* Convert the Position attribute to a mesh vertex. */
  me->mvert = (MVert *)CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, nullptr, me->totvert);
  CustomData_update_typemap(&me->vdata);

  const int layer_idx = CustomData_get_named_layer_index(
      &me->vdata, CD_PROP_FLOAT3, POINTCLOUD_ATTR_POSITION);
  CustomDataLayer *pos_layer = &me->vdata.layers[layer_idx];
  float(*positions)[3] = (float(*)[3])pos_layer->data;

  MVert *mvert;
  mvert = me->mvert;
  for (int i = 0; i < me->totvert; i++, mvert++) {
    copy_v3_v3(mvert->co, positions[i]);
  }

  /* Delete Position attribute since it is now in vertex coordinates. */
  CustomData_free_layer(&me->vdata, CD_PROP_FLOAT3, me->totvert, layer_idx);
}

void BKE_mesh_edges_set_draw_render(Mesh *mesh)
{
  MEdge *med = mesh->medge;
  for (int i = 0; i < mesh->totedge; i++, med++) {
    med->flag |= ME_EDGEDRAW | ME_EDGERENDER;
  }
}

void BKE_pointcloud_to_mesh(Main *bmain, Depsgraph *depsgraph, Scene *UNUSED(scene), Object *ob)
{
  BLI_assert(ob->type == OB_POINTCLOUD);

  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  PointCloud *pointcloud_eval = (PointCloud *)ob_eval->runtime.data_eval;

  Mesh *me = BKE_mesh_add(bmain, ob->id.name + 2);

  BKE_mesh_from_pointcloud(pointcloud_eval, me);

  BKE_id_materials_copy(bmain, (ID *)ob->data, (ID *)me);

  id_us_min(&((PointCloud *)ob->data)->id);
  ob->data = me;
  ob->type = OB_MESH;

  BKE_object_free_derived_caches(ob);
}

/* Create a temporary object to be used for nurbs-to-mesh conversion. */
static Object *object_for_curve_to_mesh_create(const Object *object)
{
  const Curve *curve = (const Curve *)object->data;

  /* Create a temporary object which can be evaluated and modified by generic
   * curve evaluation (hence the #LIB_ID_COPY_SET_COPIED_ON_WRITE flag). */
  Object *temp_object = (Object *)BKE_id_copy_ex(
      nullptr, &object->id, nullptr, LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_SET_COPIED_ON_WRITE);

  /* Remove all modifiers, since we don't want them to be applied. */
  BKE_object_free_modifiers(temp_object, LIB_ID_CREATE_NO_USER_REFCOUNT);

  /* Need to create copy of curve itself as well, since it will be changed by the curve evaluation
   * process. NOTE: Copies the data, but not the shape-keys. */
  temp_object->data = BKE_id_copy_ex(nullptr,
                                     (const ID *)object->data,
                                     nullptr,
                                     LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_SET_COPIED_ON_WRITE);
  Curve *temp_curve = (Curve *)temp_object->data;

  /* Make sure texture space is calculated for a copy of curve, it will be used for the final
   * result. */
  BKE_curve_texspace_calc(temp_curve);

  /* Temporarily set edit so we get updates from edit mode, but also because for text data-blocks
   * copying it while in edit mode gives invalid data structures. */
  temp_curve->editfont = curve->editfont;
  temp_curve->editnurb = curve->editnurb;

  return temp_object;
}

/**
 * Populate `object->runtime.curve_cache` which is then used to create the mesh.
 */
static void curve_to_mesh_eval_ensure(Object &object)
{
  BLI_assert(GS(static_cast<ID *>(object.data)->name) == ID_CU);
  Curve &curve = *static_cast<Curve *>(object.data);
  /* Clear all modifiers for the bevel object.
   *
   * This is because they can not be reliably evaluated for an original object (at least because
   * the state of dependencies is not know).
   *
   * So we create temporary copy of the object which will use same data as the original bevel, but
   * will have no modifiers. */
  Object bevel_object = {{nullptr}};
  if (curve.bevobj != nullptr) {
    memcpy(&bevel_object, curve.bevobj, sizeof(bevel_object));
    BLI_listbase_clear(&bevel_object.modifiers);
    BKE_object_runtime_reset(&bevel_object);
    curve.bevobj = &bevel_object;
  }

  /* Same thing for taper. */
  Object taper_object = {{nullptr}};
  if (curve.taperobj != nullptr) {
    memcpy(&taper_object, curve.taperobj, sizeof(taper_object));
    BLI_listbase_clear(&taper_object.modifiers);
    BKE_object_runtime_reset(&taper_object);
    curve.taperobj = &taper_object;
  }

  /* NOTE: We don't have dependency graph or scene here, so we pass nullptr. This is all fine since
   * they are only used for modifier stack, which we have explicitly disabled for all objects.
   *
   * TODO(sergey): This is a very fragile logic, but proper solution requires re-writing quite a
   * bit of internal functions (#BKE_mesh_nomain_to_mesh) and also Mesh From Curve operator.
   * Brecht says hold off with that. */
  BKE_displist_make_curveTypes(nullptr, nullptr, &object, true);

  BKE_object_runtime_free_data(&bevel_object);
  BKE_object_runtime_free_data(&taper_object);
}

/* Necessary because #BKE_object_get_evaluated_mesh doesn't look in the geometry set yet. */
static const Mesh *get_evaluated_mesh_from_object(const Object *object)
{
  const Mesh *mesh = BKE_object_get_evaluated_mesh(object);
  if (mesh) {
    return mesh;
  }
  GeometrySet *geometry_set_eval = object->runtime.geometry_set_eval;
  if (geometry_set_eval) {
    return geometry_set_eval->get_mesh_for_read();
  }
  return nullptr;
}

static const CurveEval *get_evaluated_curve_from_object(const Object *object)
{
  GeometrySet *geometry_set_eval = object->runtime.geometry_set_eval;
  if (geometry_set_eval) {
    return geometry_set_eval->get_curve_for_read();
  }
  return nullptr;
}

static Mesh *mesh_new_from_evaluated_curve_type_object(const Object *evaluated_object)
{
  const Mesh *mesh = get_evaluated_mesh_from_object(evaluated_object);
  if (mesh) {
    return BKE_mesh_copy_for_eval(mesh, false);
  }
  const CurveEval *curve = get_evaluated_curve_from_object(evaluated_object);
  if (curve) {
    return blender::bke::curve_to_wire_mesh(*curve);
  }
  return nullptr;
}

static Mesh *mesh_new_from_curve_type_object(const Object *object)
{
  /* If the object is evaluated, it should either have an evaluated mesh or curve data already.
   * The mesh can be duplicated, or the curve converted to wire mesh edges. */
  if (DEG_is_evaluated_object(object)) {
    return mesh_new_from_evaluated_curve_type_object(object);
  }

  /* Otherwise, create a temporary "fake" evaluated object and try again. This might have
   * different results, since in order to avoid having adverse affects to other original objects,
   * modifiers are cleared. An alternative would be to create a temporary depsgraph only for this
   * object and its dependencies. */
  Object *temp_object = object_for_curve_to_mesh_create(object);
  ID *temp_data = static_cast<ID *>(temp_object->data);
  curve_to_mesh_eval_ensure(*temp_object);

  /* If evaluating the curve replaced object data with different data, free the original data. */
  if (temp_data != temp_object->data) {
    BKE_id_free(nullptr, temp_data);
  }

  Mesh *mesh = mesh_new_from_evaluated_curve_type_object(temp_object);

  BKE_id_free(nullptr, temp_object->data);
  BKE_id_free(nullptr, temp_object);

  return mesh;
}

static Mesh *mesh_new_from_mball_object(Object *object)
{
  MetaBall *mball = (MetaBall *)object->data;

  /* NOTE: We can only create mesh for a polygonized meta ball. This figures out all original meta
   * balls and all evaluated child meta balls (since polygonization is only stored in the mother
   * ball).
   *
   * Create empty mesh so script-authors don't run into None objects. */
  if (!DEG_is_evaluated_object(object) || object->runtime.curve_cache == nullptr ||
      BLI_listbase_is_empty(&object->runtime.curve_cache->disp)) {
    return (Mesh *)BKE_id_new_nomain(ID_ME, ((ID *)object->data)->name + 2);
  }

  Mesh *mesh_result = (Mesh *)BKE_id_new_nomain(ID_ME, ((ID *)object->data)->name + 2);
  BKE_mesh_from_metaball(&object->runtime.curve_cache->disp, mesh_result);
  BKE_mesh_texspace_copy_from_object(mesh_result, object);

  /* Copy materials. */
  mesh_result->totcol = mball->totcol;
  mesh_result->mat = (Material **)MEM_dupallocN(mball->mat);
  if (mball->mat != nullptr) {
    for (int i = mball->totcol; i-- > 0;) {
      mesh_result->mat[i] = BKE_object_material_get(object, i + 1);
    }
  }

  return mesh_result;
}

static Mesh *mesh_new_from_mesh(Object *object, Mesh *mesh)
{
  /* While we could copy this into the new mesh,
   * add the data to 'mesh' so future calls to this function don't need to re-convert the data. */
  BKE_mesh_wrapper_ensure_mdata(mesh);

  Mesh *mesh_result = (Mesh *)BKE_id_copy_ex(
      nullptr, &mesh->id, nullptr, LIB_ID_CREATE_NO_MAIN | LIB_ID_CREATE_NO_USER_REFCOUNT);
  /* NOTE: Materials should already be copied. */
  /* Copy original mesh name. This is because edit meshes might not have one properly set name. */
  BLI_strncpy(mesh_result->id.name, ((ID *)object->data)->name, sizeof(mesh_result->id.name));
  return mesh_result;
}

static Mesh *mesh_new_from_mesh_object_with_layers(Depsgraph *depsgraph,
                                                   Object *object,
                                                   const bool preserve_origindex)
{
  if (DEG_is_original_id(&object->id)) {
    return mesh_new_from_mesh(object, (Mesh *)object->data);
  }

  if (depsgraph == nullptr) {
    return nullptr;
  }

  Object object_for_eval;
  memcpy(&object_for_eval, object, sizeof(object_for_eval));
  if (object_for_eval.runtime.data_orig != nullptr) {
    object_for_eval.data = object_for_eval.runtime.data_orig;
  }

  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  CustomData_MeshMasks mask = CD_MASK_MESH;
  if (preserve_origindex) {
    mask.vmask |= CD_MASK_ORIGINDEX;
    mask.emask |= CD_MASK_ORIGINDEX;
    mask.lmask |= CD_MASK_ORIGINDEX;
    mask.pmask |= CD_MASK_ORIGINDEX;
  }
  Mesh *result = mesh_create_eval_final(depsgraph, scene, &object_for_eval, &mask);
  return result;
}

static Mesh *mesh_new_from_mesh_object(Depsgraph *depsgraph,
                                       Object *object,
                                       const bool preserve_all_data_layers,
                                       const bool preserve_origindex)
{
  if (preserve_all_data_layers || preserve_origindex) {
    return mesh_new_from_mesh_object_with_layers(depsgraph, object, preserve_origindex);
  }
  Mesh *mesh_input = (Mesh *)object->data;
  /* If we are in edit mode, use evaluated mesh from edit structure, matching to what
   * viewport is using for visualization. */
  if (mesh_input->edit_mesh != nullptr && mesh_input->edit_mesh->mesh_eval_final) {
    mesh_input = mesh_input->edit_mesh->mesh_eval_final;
  }
  return mesh_new_from_mesh(object, mesh_input);
}

Mesh *BKE_mesh_new_from_object(Depsgraph *depsgraph,
                               Object *object,
                               const bool preserve_all_data_layers,
                               const bool preserve_origindex)
{
  Mesh *new_mesh = nullptr;
  switch (object->type) {
    case OB_FONT:
    case OB_CURVE:
    case OB_SURF:
      new_mesh = mesh_new_from_curve_type_object(object);
      break;
    case OB_MBALL:
      new_mesh = mesh_new_from_mball_object(object);
      break;
    case OB_MESH:
      new_mesh = mesh_new_from_mesh_object(
          depsgraph, object, preserve_all_data_layers, preserve_origindex);
      break;
    default:
      /* Object does not have geometry data. */
      return nullptr;
  }
  if (new_mesh == nullptr) {
    /* Happens in special cases like request of mesh for non-mother meta ball. */
    return nullptr;
  }

  /* The result must have 0 users, since it's just a mesh which is free-dangling data-block.
   * All the conversion functions are supposed to ensure mesh is not counted. */
  BLI_assert(new_mesh->id.us == 0);

  /* It is possible that mesh came from modifier stack evaluation, which preserves edit_mesh
   * pointer (which allows draw manager to access edit mesh when drawing). Normally this does
   * not cause ownership problems because evaluated object runtime is keeping track of the real
   * ownership.
   *
   * Here we are constructing a mesh which is supposed to be independent, which means no shared
   * ownership is allowed, so we make sure edit mesh is reset to nullptr (which is similar to as if
   * one duplicates the objects and applies all the modifiers). */
  new_mesh->edit_mesh = nullptr;

  return new_mesh;
}

static int foreach_libblock_make_original_callback(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_p = cb_data->id_pointer;
  if (*id_p == nullptr) {
    return IDWALK_RET_NOP;
  }
  *id_p = DEG_get_original_id(*id_p);

  return IDWALK_RET_NOP;
}

static int foreach_libblock_make_usercounts_callback(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_p = cb_data->id_pointer;
  if (*id_p == nullptr) {
    return IDWALK_RET_NOP;
  }

  const int cb_flag = cb_data->cb_flag;
  if (cb_flag & IDWALK_CB_USER) {
    id_us_plus(*id_p);
  }
  else if (cb_flag & IDWALK_CB_USER_ONE) {
    /* NOTE: in that context, that one should not be needed (since there should be at least already
     * one USER_ONE user of that ID), but better be consistent. */
    id_us_ensure_real(*id_p);
  }
  return IDWALK_RET_NOP;
}

Mesh *BKE_mesh_new_from_object_to_bmain(Main *bmain,
                                        Depsgraph *depsgraph,
                                        Object *object,
                                        bool preserve_all_data_layers)
{
  BLI_assert(ELEM(object->type, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL, OB_MESH));

  Mesh *mesh = BKE_mesh_new_from_object(depsgraph, object, preserve_all_data_layers, false);
  if (mesh == nullptr) {
    /* Unable to convert the object to a mesh, return an empty one. */
    Mesh *mesh_in_bmain = BKE_mesh_add(bmain, ((ID *)object->data)->name + 2);
    id_us_min(&mesh_in_bmain->id);
    return mesh_in_bmain;
  }

  /* Make sure mesh only points original data-blocks, also increase users of materials and other
   * possibly referenced data-blocks.
   *
   * Going to original data-blocks is required to have bmain in a consistent state, where
   * everything is only allowed to reference original data-blocks.
   *
   * Note that user-count updates has to be done *after* mesh has been transferred to Main database
   * (since doing refcounting on non-Main IDs is forbidden). */
  BKE_library_foreach_ID_link(
      nullptr, &mesh->id, foreach_libblock_make_original_callback, nullptr, IDWALK_NOP);

  /* Append the mesh to 'bmain'.
   * We do it a bit longer way since there is no simple and clear way of adding existing data-block
   * to the 'bmain'. So we allocate new empty mesh in the 'bmain' (which guarantees all the naming
   * and orders and flags) and move the temporary mesh in place there. */
  Mesh *mesh_in_bmain = BKE_mesh_add(bmain, mesh->id.name + 2);

  /* NOTE: BKE_mesh_nomain_to_mesh() does not copy materials and instead it preserves them in the
   * destination mesh. So we "steal" all related fields before calling it.
   *
   * TODO(sergey): We really better have a function which gets and ID and accepts it for the bmain.
   */
  mesh_in_bmain->mat = mesh->mat;
  mesh_in_bmain->totcol = mesh->totcol;
  mesh_in_bmain->flag = mesh->flag;
  mesh_in_bmain->smoothresh = mesh->smoothresh;
  mesh->mat = nullptr;

  BKE_mesh_nomain_to_mesh(mesh, mesh_in_bmain, nullptr, &CD_MASK_MESH, true);

  /* User-count is required because so far mesh was in a limbo, where library management does
   * not perform any user management (i.e. copy of a mesh will not increase users of materials). */
  BKE_library_foreach_ID_link(
      nullptr, &mesh_in_bmain->id, foreach_libblock_make_usercounts_callback, nullptr, IDWALK_NOP);

  /* Make sure user count from BKE_mesh_add() is the one we expect here and bring it down to 0. */
  BLI_assert(mesh_in_bmain->id.us == 1);
  id_us_min(&mesh_in_bmain->id);

  return mesh_in_bmain;
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

  for (i = 0, kb = (KeyBlock *)key->block.first; kb; kb = kb->next, i++) {
    int ci;
    float *array;

    if (mesh_src->totvert != kb->totelem) {
      CLOG_ERROR(&LOG,
                 "vertex size mismatch (Mesh '%s':%d != KeyBlock '%s':%d)",
                 mesh_src->id.name + 2,
                 mesh_src->totvert,
                 kb->name,
                 kb->totelem);
      array = (float *)MEM_calloc_arrayN((size_t)mesh_src->totvert, sizeof(float[3]), __func__);
    }
    else {
      array = (float *)MEM_malloc_arrayN((size_t)mesh_src->totvert, sizeof(float[3]), __func__);
      memcpy(array, kb->data, sizeof(float[3]) * (size_t)mesh_src->totvert);
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
                                           const bool use_virtual_modifiers,
                                           const bool build_shapekey_layers)
{
  Mesh *me = ob_eval->runtime.data_orig ? (Mesh *)ob_eval->runtime.data_orig :
                                          (Mesh *)ob_eval->data;
  const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md_eval->type);
  Mesh *result = nullptr;
  KeyBlock *kb;
  ModifierEvalContext mectx = {depsgraph, ob_eval, MOD_APPLY_TO_BASE_MESH};

  if (!(md_eval->mode & eModifierMode_Realtime)) {
    return result;
  }

  if (mti->isDisabled && mti->isDisabled(scene, md_eval, false)) {
    return result;
  }

  if (build_shapekey_layers && me->key &&
      (kb = (KeyBlock *)BLI_findlink(&me->key->block, ob_eval->shapenr - 1))) {
    BKE_keyblock_convert_to_mesh(kb, me);
  }

  Mesh *mesh_temp = (Mesh *)BKE_id_copy_ex(nullptr, &me->id, nullptr, LIB_ID_COPY_LOCALIZE);
  int numVerts = 0;
  float(*deformedVerts)[3] = nullptr;

  if (use_virtual_modifiers) {
    VirtualModifierData virtualModifierData;
    for (ModifierData *md_eval_virt =
             BKE_modifiers_get_virtual_modifierlist(ob_eval, &virtualModifierData);
         md_eval_virt && (md_eval_virt != ob_eval->modifiers.first);
         md_eval_virt = md_eval_virt->next) {
      if (!BKE_modifier_is_enabled(scene, md_eval_virt, eModifierMode_Realtime)) {
        continue;
      }
      /* All virtual modifiers are deform modifiers. */
      const ModifierTypeInfo *mti_virt = BKE_modifier_get_info((ModifierType)md_eval_virt->type);
      BLI_assert(mti_virt->type == eModifierTypeType_OnlyDeform);
      if (mti_virt->type != eModifierTypeType_OnlyDeform) {
        continue;
      }

      if (deformedVerts == nullptr) {
        deformedVerts = BKE_mesh_vert_coords_alloc(me, &numVerts);
      }
      mti_virt->deformVerts(md_eval_virt, &mectx, mesh_temp, deformedVerts, numVerts);
    }
  }

  if (mti->type == eModifierTypeType_OnlyDeform) {
    if (deformedVerts == nullptr) {
      deformedVerts = BKE_mesh_vert_coords_alloc(me, &numVerts);
    }
    result = mesh_temp;
    mti->deformVerts(md_eval, &mectx, result, deformedVerts, numVerts);
    BKE_mesh_vert_coords_apply(result, deformedVerts);

    if (build_shapekey_layers) {
      add_shapekey_layers(result, me);
    }
  }
  else {
    if (deformedVerts != nullptr) {
      BKE_mesh_vert_coords_apply(mesh_temp, deformedVerts);
    }

    if (build_shapekey_layers) {
      add_shapekey_layers(mesh_temp, me);
    }

    result = mti->modifyMesh(md_eval, &mectx, mesh_temp);
    ASSERT_IS_VALID_MESH(result);

    if (mesh_temp != result) {
      BKE_id_free(nullptr, mesh_temp);
    }
  }

  if (deformedVerts != nullptr) {
    MEM_freeN(deformedVerts);
  }

  return result;
}

/* This is a Mesh-based copy of the same function in DerivedMesh.cc */
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

    for (kb = (KeyBlock *)mesh_dst->key->block.first; kb; kb = kb->next) {
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

    cos = (float(*)[3])CustomData_get_layer_n(&mesh_src->vdata, CD_SHAPEKEY, i);
    kb->totelem = mesh_src->totvert;

    kb->data = kbcos = (float(*)[3])MEM_malloc_arrayN(kb->totelem, sizeof(float[3]), __func__);
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

  for (kb = (KeyBlock *)mesh_dst->key->block.first; kb; kb = kb->next) {
    if (kb->totelem != mesh_src->totvert) {
      if (kb->data) {
        MEM_freeN(kb->data);
      }

      kb->totelem = mesh_src->totvert;
      kb->data = MEM_calloc_arrayN(kb->totelem, sizeof(float[3]), __func__);
      CLOG_ERROR(&LOG, "lost a shapekey layer: '%s'! (bmesh internal error)", kb->name);
    }
  }
}

void BKE_mesh_nomain_to_mesh(Mesh *mesh_src,
                             Mesh *mesh_dst,
                             Object *ob,
                             const CustomData_MeshMasks *mask,
                             bool take_ownership)
{
  BLI_assert(mesh_src->id.tag & LIB_TAG_NO_MAIN);

  /* mesh_src might depend on mesh_dst, so we need to do everything with a local copy */
  /* TODO(Sybren): the above claim came from 2.7x derived-mesh code (DM_to_mesh);
   * check whether it is still true with Mesh */
  Mesh tmp;
  memcpy(&tmp, mesh_dst, sizeof(tmp));
  int totvert, totedge /*, totface */ /* UNUSED */, totloop, totpoly;
  bool did_shapekeys = false;
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
      kb = (KeyBlock *)BLI_findlink(&mesh_dst->key->block, ob->shapenr - 1);
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
    did_shapekeys = true;
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
    CustomData_add_layer(&tmp.ldata,
                         CD_MLOOP,
                         CD_ASSIGN,
                         (alloctype == CD_ASSIGN) ? mesh_src->mloop :
                                                    MEM_dupallocN(mesh_src->mloop),
                         tmp.totloop);
    CustomData_add_layer(&tmp.pdata,
                         CD_MPOLY,
                         CD_ASSIGN,
                         (alloctype == CD_ASSIGN) ? mesh_src->mpoly :
                                                    MEM_dupallocN(mesh_src->mpoly),
                         tmp.totpoly);
  }

  /* object had got displacement layer, should copy this layer to save sculpted data */
  /* NOTE(nazgul): maybe some other layers should be copied? */
  if (CustomData_has_layer(&mesh_dst->ldata, CD_MDISPS)) {
    if (totloop == mesh_dst->totloop) {
      MDisps *mdisps = (MDisps *)CustomData_get_layer(&mesh_dst->ldata, CD_MDISPS);
      CustomData_add_layer(&tmp.ldata, CD_MDISPS, alloctype, mdisps, totloop);
      if (alloctype == CD_ASSIGN) {
        /* Assign nullptr to prevent double-free. */
        CustomData_set_layer(&mesh_dst->ldata, CD_MDISPS, nullptr);
      }
    }
  }

  /* yes, must be before _and_ after tessellate */
  BKE_mesh_update_customdata_pointers(&tmp, false);

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
    tmp.key = nullptr;
  }

  /* Clear selection history */
  MEM_SAFE_FREE(tmp.mselect);
  tmp.totselect = 0;
  tmp.texflag &= ~ME_AUTOSPACE_EVALUATED;

  /* Clear any run-time data.
   * Even though this mesh won't typically have run-time data, the Python API can for e.g.
   * create loop-triangle cache here, which is confusing when left in the mesh, see: T81136. */
  BKE_mesh_runtime_clear_geometry(&tmp);

  /* skip the listbase */
  MEMCPY_STRUCT_AFTER(mesh_dst, &tmp, id.prev);

  BLI_freelistN(&mesh_dst->vertex_group_names);
  BKE_defgroup_copy_list(&mesh_dst->vertex_group_names, &mesh_src->vertex_group_names);
  mesh_dst->vertex_group_active_index = mesh_src->vertex_group_active_index;

  if (take_ownership) {
    if (alloctype == CD_ASSIGN) {
      CustomData_free_typemask(&mesh_src->vdata, mesh_src->totvert, ~mask->vmask);
      CustomData_free_typemask(&mesh_src->edata, mesh_src->totedge, ~mask->emask);
      CustomData_free_typemask(&mesh_src->ldata, mesh_src->totloop, ~mask->lmask);
      CustomData_free_typemask(&mesh_src->pdata, mesh_src->totpoly, ~mask->pmask);
    }
    BKE_id_free(nullptr, mesh_src);
  }
}

void BKE_mesh_nomain_to_meshkey(Mesh *mesh_src, Mesh *mesh_dst, KeyBlock *kb)
{
  BLI_assert(mesh_src->id.tag & LIB_TAG_NO_MAIN);

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

  fp = (float *)kb->data;
  mvert = mesh_src->mvert;

  for (a = 0; a < kb->totelem; a++, fp += 3, mvert++) {
    copy_v3_v3(fp, mvert->co);
  }
}
