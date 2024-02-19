/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"

#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_DerivedMesh.hh"
#include "BKE_curves.hh"
#include "BKE_deform.hh"
#include "BKE_displist.h"
#include "BKE_editmesh.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_mball.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"
#include "BKE_object_types.hh"
/* these 2 are only used by conversion functions */
#include "BKE_curve.hh"
/* -- */
#include "BKE_object.hh"
/* -- */
#include "BKE_pointcloud.hh"

#include "BKE_curve_to_mesh.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

using blender::float3;
using blender::IndexRange;
using blender::MutableSpan;
using blender::Span;
using blender::StringRefNull;

static CLG_LogRef LOG = {"bke.mesh_convert"};

static Mesh *mesh_nurbs_displist_to_mesh(const Curve *cu, const ListBase *dispbase)
{
  using namespace blender;
  using namespace blender::bke;
  int a, b, ofs;
  const bool conv_polys = (
      /* 2D polys are filled with #DispList.type == #DL_INDEX3. */
      (CU_DO_2DFILL(cu) == false) ||
      /* surf polys are never filled */
      BKE_curve_type_get(cu) == OB_SURF);

  /* count */
  int totvert = 0;
  int totedge = 0;
  int faces_num = 0;
  int totloop = 0;
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
        faces_num += tot;
        totloop += tot * 4;
      }
    }
    else if (dl->type == DL_INDEX3) {
      int tot;
      totvert += dl->nr;
      tot = dl->parts;
      faces_num += tot;
      totloop += tot * 3;
    }
  }

  if (totvert == 0) {
    return BKE_mesh_new_nomain(0, 0, 0, 0);
  }

  Mesh *mesh = BKE_mesh_new_nomain(totvert, totedge, faces_num, totloop);
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<blender::int2> edges = mesh->edges_for_write();
  MutableSpan<int> face_offsets = mesh->face_offsets_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();

  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_only_span<int>(
      "material_index", AttrDomain::Face);
  SpanAttributeWriter<bool> sharp_faces = attributes.lookup_or_add_for_write_span<bool>(
      "sharp_face", AttrDomain::Face);
  SpanAttributeWriter<float2> uv_attribute = attributes.lookup_or_add_for_write_span<float2>(
      DATA_("UVMap"), AttrDomain::Corner);
  MutableSpan<float2> uv_map = uv_attribute.span;

  int dst_vert = 0;
  int dst_edge = 0;
  int dst_poly = 0;
  int dst_loop = 0;
  LISTBASE_FOREACH (const DispList *, dl, dispbase) {
    const bool is_smooth = (dl->rt & CU_SMOOTH) != 0;

    if (dl->type == DL_SEGM) {
      const int startvert = dst_vert;
      a = dl->parts * dl->nr;
      const float *data = dl->verts;
      while (a--) {
        copy_v3_v3(positions[dst_vert], data);
        data += 3;
        dst_vert++;
      }

      for (a = 0; a < dl->parts; a++) {
        ofs = a * dl->nr;
        for (b = 1; b < dl->nr; b++) {
          edges[dst_edge][0] = startvert + ofs + b - 1;
          edges[dst_edge][1] = startvert + ofs + b;

          dst_edge++;
        }
      }
    }
    else if (dl->type == DL_POLY) {
      if (conv_polys) {
        const int startvert = dst_vert;
        a = dl->parts * dl->nr;
        const float *data = dl->verts;
        while (a--) {
          copy_v3_v3(positions[dst_vert], data);
          data += 3;
          dst_vert++;
        }

        for (a = 0; a < dl->parts; a++) {
          ofs = a * dl->nr;
          for (b = 0; b < dl->nr; b++) {
            edges[dst_edge][0] = startvert + ofs + b;
            if (b == dl->nr - 1) {
              edges[dst_edge][1] = startvert + ofs;
            }
            else {
              edges[dst_edge][1] = startvert + ofs + b + 1;
            }
            dst_edge++;
          }
        }
      }
    }
    else if (dl->type == DL_INDEX3) {
      const int startvert = dst_vert;
      a = dl->nr;
      const float *data = dl->verts;
      while (a--) {
        copy_v3_v3(positions[dst_vert], data);
        data += 3;
        dst_vert++;
      }

      a = dl->parts;
      const int *index = dl->index;
      while (a--) {
        corner_verts[dst_loop + 0] = startvert + index[0];
        corner_verts[dst_loop + 1] = startvert + index[2];
        corner_verts[dst_loop + 2] = startvert + index[1];
        face_offsets[dst_poly] = dst_loop;
        material_indices.span[dst_poly] = dl->col;

        for (int i = 0; i < 3; i++) {
          uv_map[dst_loop + i][0] = (corner_verts[dst_loop + i] - startvert) / float(dl->nr - 1);
          uv_map[dst_loop + i][1] = 0.0f;
        }

        sharp_faces.span[dst_poly] = !is_smooth;
        dst_poly++;
        dst_loop += 3;
        index += 3;
      }
    }
    else if (dl->type == DL_SURF) {
      const int startvert = dst_vert;
      a = dl->parts * dl->nr;
      const float *data = dl->verts;
      while (a--) {
        copy_v3_v3(positions[dst_vert], data);
        data += 3;
        dst_vert++;
      }

      for (a = 0; a < dl->parts; a++) {

        if ((dl->flag & DL_CYCL_V) == 0 && a == dl->parts - 1) {
          break;
        }

        int p1, p2, p3, p4;
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
          corner_verts[dst_loop + 0] = p1;
          corner_verts[dst_loop + 1] = p3;
          corner_verts[dst_loop + 2] = p4;
          corner_verts[dst_loop + 3] = p2;
          face_offsets[dst_poly] = dst_loop;
          material_indices.span[dst_poly] = dl->col;

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

          for (int i = 0; i < 4; i++) {
            /* find uv based on vertex index into grid array */
            int v = corner_verts[dst_loop + i] - startvert;

            uv_map[dst_loop + i][0] = (v / dl->nr) / float(orco_sizev);
            uv_map[dst_loop + i][1] = (v % dl->nr) / float(orco_sizeu);

            /* cyclic correction */
            if (ELEM(i, 1, 2) && uv_map[dst_loop + i][0] == 0.0f) {
              uv_map[dst_loop + i][0] = 1.0f;
            }
            if (ELEM(i, 0, 1) && uv_map[dst_loop + i][1] == 0.0f) {
              uv_map[dst_loop + i][1] = 1.0f;
            }
          }

          sharp_faces.span[dst_poly] = !is_smooth;
          dst_poly++;
          dst_loop += 4;

          p4 = p3;
          p3++;
          p2 = p1;
          p1++;
        }
      }
    }
  }

  if (faces_num) {
    mesh_calc_edges(*mesh, true, false);
  }

  material_indices.finish();
  sharp_faces.finish();
  uv_attribute.finish();

  return mesh;
}

/**
 * Copy evaluated texture space from curve to mesh.
 *
 * \note We disable auto texture space feature since that will cause texture space to evaluate
 * differently for curve and mesh, since curves use control points and handles to calculate the
 * bounding box, and mesh uses the tessellated curve.
 */
static void mesh_copy_texture_space_from_curve_type(const Curve *cu, Mesh *mesh)
{
  mesh->texspace_flag = cu->texspace_flag & ~CU_TEXSPACE_FLAG_AUTO;
  copy_v3_v3(mesh->texspace_location, cu->texspace_location);
  copy_v3_v3(mesh->texspace_size, cu->texspace_size);
  BKE_mesh_texspace_calc(mesh);
}

Mesh *BKE_mesh_new_nomain_from_curve_displist(const Object *ob, const ListBase *dispbase)
{
  const Curve *cu = (const Curve *)ob->data;

  Mesh *mesh = mesh_nurbs_displist_to_mesh(cu, dispbase);
  mesh_copy_texture_space_from_curve_type(cu, mesh);
  mesh->mat = (Material **)MEM_dupallocN(cu->mat);
  mesh->totcol = cu->totcol;

  return mesh;
}

Mesh *BKE_mesh_new_nomain_from_curve(const Object *ob)
{
  ListBase disp = {nullptr, nullptr};

  if (ob->runtime->curve_cache) {
    disp = ob->runtime->curve_cache->disp;
  }

  return BKE_mesh_new_nomain_from_curve_displist(ob, &disp);
}

struct EdgeLink {
  EdgeLink *next, *prev;
  const void *edge;
};

struct VertLink {
  Link *next, *prev;
  uint index;
};

static void prependPolyLineVert(ListBase *lb, uint index)
{
  VertLink *vl = MEM_cnew<VertLink>("VertLink");
  vl->index = index;
  BLI_addhead(lb, vl);
}

static void appendPolyLineVert(ListBase *lb, uint index)
{
  VertLink *vl = MEM_cnew<VertLink>("VertLink");
  vl->index = index;
  BLI_addtail(lb, vl);
}

void BKE_mesh_to_curve_nurblist(const Mesh *mesh, ListBase *nurblist, const int edge_users_test)
{
  const Span<float3> positions = mesh->vert_positions();
  const Span<blender::int2> mesh_edges = mesh->edges();
  const blender::OffsetIndices polys = mesh->faces();
  const Span<int> corner_edges = mesh->corner_edges();

  /* only to detect edge polylines */
  int *edge_users;

  ListBase edges = {nullptr, nullptr};

  /* get boundary edges */
  edge_users = (int *)MEM_calloc_arrayN(mesh_edges.size(), sizeof(int), __func__);
  for (const int i : polys.index_range()) {
    for (const int edge : corner_edges.slice(polys[i])) {
      edge_users[edge]++;
    }
  }

  /* create edges from all faces (so as to find edges not in any faces) */
  for (const int i : mesh_edges.index_range()) {
    if (edge_users[i] == edge_users_test) {
      EdgeLink *edl = MEM_cnew<EdgeLink>("EdgeLink");
      edl->edge = &mesh_edges[i];

      BLI_addtail(&edges, edl);
    }
  }
  MEM_freeN(edge_users);

  if (edges.first) {
    while (edges.first) {
      /* each iteration find a polyline and add this as a nurbs poly spline */

      ListBase polyline = {nullptr, nullptr}; /* store a list of VertLink's */
      bool closed = false;
      int faces_num = 0;
      blender::int2 &edge_current = *(blender::int2 *)((EdgeLink *)edges.last)->edge;
      uint startVert = edge_current[0];
      uint endVert = edge_current[1];
      bool ok = true;

      appendPolyLineVert(&polyline, startVert);
      faces_num++;
      appendPolyLineVert(&polyline, endVert);
      faces_num++;
      BLI_freelinkN(&edges, edges.last);

      while (ok) { /* while connected edges are found... */
        EdgeLink *edl = (EdgeLink *)edges.last;
        ok = false;
        while (edl) {
          EdgeLink *edl_prev = edl->prev;

          const blender::int2 &edge = *(blender::int2 *)edl->edge;

          if (edge[0] == endVert) {
            endVert = edge[1];
            appendPolyLineVert(&polyline, endVert);
            faces_num++;
            BLI_freelinkN(&edges, edl);
            ok = true;
          }
          else if (edge[1] == endVert) {
            endVert = edge[0];
            appendPolyLineVert(&polyline, endVert);
            faces_num++;
            BLI_freelinkN(&edges, edl);
            ok = true;
          }
          else if (edge[0] == startVert) {
            startVert = edge[1];
            prependPolyLineVert(&polyline, startVert);
            faces_num++;
            BLI_freelinkN(&edges, edl);
            ok = true;
          }
          else if (edge[1] == startVert) {
            startVert = edge[0];
            prependPolyLineVert(&polyline, startVert);
            faces_num++;
            BLI_freelinkN(&edges, edl);
            ok = true;
          }

          edl = edl_prev;
        }
      }

      /* Now we have a polyline, make into a curve */
      if (startVert == endVert) {
        BLI_freelinkN(&polyline, polyline.last);
        faces_num--;
        closed = true;
      }

      /* --- nurbs --- */
      {
        Nurb *nu;
        BPoint *bp;
        VertLink *vl;

        /* create new 'nurb' within the curve */
        nu = MEM_new<Nurb>("MeshNurb", blender::dna::shallow_zero_initialize());

        nu->pntsu = faces_num;
        nu->pntsv = 1;
        nu->orderu = 4;
        nu->flagu = CU_NURB_ENDPOINT | (closed ? CU_NURB_CYCLIC : 0); /* endpoint */
        nu->resolu = 12;

        nu->bp = (BPoint *)MEM_calloc_arrayN(faces_num, sizeof(BPoint), "bpoints");

        /* add points */
        vl = (VertLink *)polyline.first;
        int i;
        for (i = 0, bp = nu->bp; i < faces_num; i++, bp++, vl = (VertLink *)vl->next) {
          copy_v3_v3(bp->vec, positions[vl->index]);
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

void BKE_mesh_to_curve(Main *bmain, Depsgraph *depsgraph, Scene * /*scene*/, Object *ob)
{
  const Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  if (!ob_eval) {
    return;
  }
  const Mesh *me_eval = BKE_object_get_evaluated_mesh_no_subsurf(ob_eval);
  if (!me_eval) {
    return;
  }

  ListBase nurblist = {nullptr, nullptr};

  BKE_mesh_to_curve_nurblist(me_eval, &nurblist, 0);
  BKE_mesh_to_curve_nurblist(me_eval, &nurblist, 1);

  if (nurblist.first) {
    Curve *cu = BKE_curve_add(bmain, ob->id.name + 2, OB_CURVES_LEGACY);
    cu->flag |= CU_3D;

    cu->nurb = nurblist;

    id_us_min(&((Mesh *)ob->data)->id);
    ob->data = cu;
    ob->type = OB_CURVES_LEGACY;

    BKE_object_free_derived_caches(ob);
  }
}

void BKE_mesh_to_pointcloud(Main *bmain, Depsgraph *depsgraph, Scene * /*scene*/, Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  const Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  if (!ob_eval) {
    return;
  }
  const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
  if (!mesh_eval) {
    return;
  }

  PointCloud *pointcloud = (PointCloud *)BKE_pointcloud_add(bmain, ob->id.name + 2);

  CustomData_free(&pointcloud->pdata, pointcloud->totpoint);
  pointcloud->totpoint = mesh_eval->verts_num;
  CustomData_merge(
      &mesh_eval->vert_data, &pointcloud->pdata, CD_MASK_PROP_ALL, mesh_eval->verts_num);

  BKE_id_materials_copy(bmain, (ID *)ob->data, (ID *)pointcloud);

  id_us_min(&((Mesh *)ob->data)->id);
  ob->data = pointcloud;
  ob->type = OB_POINTCLOUD;

  BKE_object_free_derived_caches(ob);
}

void BKE_pointcloud_to_mesh(Main *bmain, Depsgraph *depsgraph, Scene * /*scene*/, Object *ob)
{
  BLI_assert(ob->type == OB_POINTCLOUD);

  const Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  const blender::bke::GeometrySet geometry = blender::bke::object_get_evaluated_geometry_set(
      *ob_eval);

  Mesh *mesh = BKE_mesh_add(bmain, ob->id.name + 2);

  if (const PointCloud *points = geometry.get_pointcloud()) {
    mesh->verts_num = points->totpoint;
    CustomData_merge(&points->pdata, &mesh->vert_data, CD_MASK_PROP_ALL, points->totpoint);
  }

  BKE_id_materials_copy(bmain, (ID *)ob->data, (ID *)mesh);

  id_us_min(&((PointCloud *)ob->data)->id);
  ob->data = mesh;
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

static void object_for_curve_to_mesh_free(Object *temp_object)
{
  /* Clear edit mode pointers that were explicitly copied to the temporary curve. */
  ID *final_object_data = static_cast<ID *>(temp_object->data);
  if (GS(final_object_data->name) == ID_CU_LEGACY) {
    Curve &curve = *reinterpret_cast<Curve *>(final_object_data);
    curve.editfont = nullptr;
    curve.editnurb = nullptr;
  }

  /* Only free the final object data if it is *not* stored in the #data_eval field. This is still
   * necessary because #temp_object's data could be replaced by a #Curve data-block that isn't also
   * assigned to #data_eval. */
  const bool object_data_stored_in_data_eval = final_object_data ==
                                               temp_object->runtime->data_eval;

  BKE_id_free(nullptr, temp_object);
  if (!object_data_stored_in_data_eval) {
    BKE_id_free(nullptr, final_object_data);
  }
}

/**
 * Populate `object->runtime->curve_cache` which is then used to create the mesh.
 */
static void curve_to_mesh_eval_ensure(Object &object)
{
  BLI_assert(GS(static_cast<ID *>(object.data)->name) == ID_CU_LEGACY);
  Curve &curve = *static_cast<Curve *>(object.data);
  /* Clear all modifiers for the bevel object.
   *
   * This is because they can not be reliably evaluated for an original object (at least because
   * the state of dependencies is not know).
   *
   * So we create temporary copy of the object which will use same data as the original bevel, but
   * will have no modifiers. */
  Object bevel_object = blender::dna::shallow_zero_initialize();
  blender::bke::ObjectRuntime bevel_runtime;
  if (curve.bevobj != nullptr) {
    bevel_object = blender::dna::shallow_copy(*curve.bevobj);
    bevel_runtime = *curve.bevobj->runtime;
    bevel_object.runtime = &bevel_runtime;

    BLI_listbase_clear(&bevel_object.modifiers);
    BKE_object_runtime_reset(&bevel_object);
    curve.bevobj = &bevel_object;
  }

  /* Same thing for taper. */
  Object taper_object = blender::dna::shallow_zero_initialize();
  blender::bke::ObjectRuntime taper_runtime;
  if (curve.taperobj != nullptr) {
    taper_object = blender::dna::shallow_copy(*curve.taperobj);
    taper_runtime = *curve.taperobj->runtime;
    taper_object.runtime = &taper_runtime;

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

  if (bevel_object.runtime) {
    BKE_object_runtime_free_data(&bevel_object);
  }
  if (taper_object.runtime) {
    BKE_object_runtime_free_data(&taper_object);
  }
}

static const Curves *get_evaluated_curves_from_object(const Object *object)
{
  if (blender::bke::GeometrySet *geometry_set_eval = object->runtime->geometry_set_eval) {
    return geometry_set_eval->get_curves();
  }
  return nullptr;
}

static Mesh *mesh_new_from_evaluated_curve_type_object(const Object *evaluated_object)
{
  if (const Mesh *mesh = BKE_object_get_evaluated_mesh(evaluated_object)) {
    return BKE_mesh_copy_for_eval(mesh);
  }
  if (const Curves *curves = get_evaluated_curves_from_object(evaluated_object)) {
    const blender::bke::AnonymousAttributePropagationInfo propagation_info;
    return blender::bke::curve_to_wire_mesh(curves->geometry.wrap(), propagation_info);
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
    if (GS(temp_data->name) == ID_CU_LEGACY) {
      /* Clear edit mode pointers that were explicitly copied to the temporary curve. */
      Curve *curve = reinterpret_cast<Curve *>(temp_data);
      curve->editfont = nullptr;
      curve->editnurb = nullptr;
    }
    BKE_id_free(nullptr, temp_data);
  }

  Mesh *mesh = mesh_new_from_evaluated_curve_type_object(temp_object);

  object_for_curve_to_mesh_free(temp_object);

  return mesh;
}

static Mesh *mesh_new_from_mball_object(Object *object)
{
  /* NOTE: We can only create mesh for a polygonized meta ball. This figures out all original meta
   * balls and all evaluated child meta balls (since polygonization is only stored in the mother
   * ball).
   *
   * Create empty mesh so script-authors don't run into None objects. */
  if (!DEG_is_evaluated_object(object)) {
    return (Mesh *)BKE_id_new_nomain(ID_ME, ((ID *)object->data)->name + 2);
  }

  const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(object);
  if (mesh_eval == nullptr) {
    return (Mesh *)BKE_id_new_nomain(ID_ME, ((ID *)object->data)->name + 2);
  }

  return BKE_mesh_copy_for_eval(mesh_eval);
}

static Mesh *mesh_new_from_mesh(Object *object, Mesh *mesh)
{
  /* While we could copy this into the new mesh,
   * add the data to 'mesh' so future calls to this function don't need to re-convert the data. */
  if (mesh->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH) {
    BKE_mesh_wrapper_ensure_mdata(mesh);
  }
  else {
    mesh = BKE_mesh_wrapper_ensure_subdivision(mesh);
  }

  Mesh *mesh_result = (Mesh *)BKE_id_copy_ex(
      nullptr, &mesh->id, nullptr, LIB_ID_CREATE_NO_MAIN | LIB_ID_CREATE_NO_USER_REFCOUNT);
  /* NOTE: Materials should already be copied. */
  /* Copy original mesh name. This is because edit meshes might not have one properly set name. */
  STRNCPY(mesh_result->id.name, ((ID *)object->data)->name);
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

  Object object_for_eval = blender::dna::shallow_copy(*object);
  blender::bke::ObjectRuntime runtime = *object->runtime;
  object_for_eval.runtime = &runtime;

  if (object_for_eval.runtime->data_orig != nullptr) {
    object_for_eval.data = object_for_eval.runtime->data_orig;
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
  return BKE_mesh_wrapper_ensure_subdivision(result);
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
  if (mesh_input->edit_mesh != nullptr) {
    Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(object);
    if (editmesh_eval_final != nullptr) {
      mesh_input = editmesh_eval_final;
    }
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
    case OB_CURVES_LEGACY:
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
  BLI_assert(ELEM(object->type, OB_FONT, OB_CURVES_LEGACY, OB_SURF, OB_MBALL, OB_MESH));

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
   * (since doing reference-counting on non-Main IDs is forbidden). */
  BKE_library_foreach_ID_link(
      nullptr, &mesh->id, foreach_libblock_make_original_callback, nullptr, IDWALK_NOP);

  /* Append the mesh to 'bmain'.
   * We do it a bit longer way since there is no simple and clear way of adding existing data-block
   * to the 'bmain'. So we allocate new empty mesh in the 'bmain' (which guarantees all the naming
   * and orders and flags) and move the temporary mesh in place there. */
  Mesh *mesh_in_bmain = BKE_mesh_add(bmain, mesh->id.name + 2);

  /* NOTE: BKE_mesh_nomain_to_mesh() does not copy materials and instead it preserves them in the
   * destination mesh. So we "steal" materials before calling it.
   *
   * TODO(sergey): We really better have a function which gets and ID and accepts it for the bmain.
   */
  mesh_in_bmain->mat = mesh->mat;
  mesh_in_bmain->totcol = mesh->totcol;
  mesh->mat = nullptr;

  BKE_mesh_nomain_to_mesh(mesh, mesh_in_bmain, nullptr);

  /* Anonymous attributes shouldn't exist on original data. */
  mesh_in_bmain->attributes_for_write().remove_anonymous();

  /* User-count is required because so far mesh was in a limbo, where library management does
   * not perform any user management (i.e. copy of a mesh will not increase users of materials). */
  BKE_library_foreach_ID_link(
      nullptr, &mesh_in_bmain->id, foreach_libblock_make_usercounts_callback, nullptr, IDWALK_NOP);

  /* Make sure user count from BKE_mesh_add() is the one we expect here and bring it down to 0. */
  BLI_assert(mesh_in_bmain->id.us == 1);
  id_us_min(&mesh_in_bmain->id);

  return mesh_in_bmain;
}

static void copy_loose_vert_hint(const Mesh &src, Mesh &dst)
{
  const auto &src_cache = src.runtime->loose_verts_cache;
  if (src_cache.is_cached() && src_cache.data().count == 0) {
    dst.tag_loose_verts_none();
  }
}

static void copy_loose_edge_hint(const Mesh &src, Mesh &dst)
{
  const auto &src_cache = src.runtime->loose_edges_cache;
  if (src_cache.is_cached() && src_cache.data().count == 0) {
    dst.tag_loose_edges_none();
  }
}

static void copy_overlapping_hint(const Mesh &src, Mesh &dst)
{
  if (src.no_overlapping_topology()) {
    dst.tag_overlapping_none();
  }
}

static KeyBlock *keyblock_ensure_from_uid(Key &key, const int uid, const StringRefNull name)
{
  if (KeyBlock *kb = BKE_keyblock_find_uid(&key, uid)) {
    return kb;
  }
  KeyBlock *kb = BKE_keyblock_add(&key, name.c_str());
  kb->uid = uid;
  return kb;
}

static int find_object_active_key_uid(const Key &key, const Object &object)
{
  const int active_kb_index = object.shapenr - 1;
  const KeyBlock *kb = (const KeyBlock *)BLI_findlink(&key.block, active_kb_index);
  if (!kb) {
    CLOG_ERROR(&LOG, "Could not find object's active shapekey %d", active_kb_index);
    return -1;
  }
  return kb->uid;
}

static void move_shapekey_layers_to_keyblocks(const Mesh &mesh,
                                              const CustomData &custom_data,
                                              Key &key_dst,
                                              const int actshape_uid)
{
  using namespace blender::bke;
  for (const int i : IndexRange(CustomData_number_of_layers(&custom_data, CD_SHAPEKEY))) {
    const int layer_index = CustomData_get_layer_index_n(&custom_data, CD_SHAPEKEY, i);
    const CustomDataLayer &layer = custom_data.layers[layer_index];

    KeyBlock *kb = keyblock_ensure_from_uid(key_dst, layer.uid, layer.name);
    MEM_SAFE_FREE(kb->data);

    kb->totelem = mesh.verts_num;
    kb->data = MEM_malloc_arrayN(kb->totelem, sizeof(float3), __func__);
    MutableSpan<float3> kb_coords(static_cast<float3 *>(kb->data), kb->totelem);
    if (kb->uid == actshape_uid) {
      mesh.attributes().lookup<float3>("position").varray.materialize(kb_coords);
    }
    else {
      kb_coords.copy_from({static_cast<const float3 *>(layer.data), mesh.verts_num});
    }
  }

  LISTBASE_FOREACH (KeyBlock *, kb, &key_dst.block) {
    if (kb->totelem != mesh.verts_num) {
      MEM_SAFE_FREE(kb->data);
      kb->totelem = mesh.verts_num;
      kb->data = MEM_cnew_array<float3>(kb->totelem, __func__);
      CLOG_ERROR(&LOG, "Data for shape key '%s' on mesh missing from evaluated mesh ", kb->name);
    }
  }
}

void BKE_mesh_nomain_to_mesh(Mesh *mesh_src, Mesh *mesh_dst, Object *ob)
{
  using namespace blender::bke;
  BLI_assert(mesh_src->id.tag & LIB_TAG_NO_MAIN);
  if (ob) {
    BLI_assert(mesh_dst == ob->data);
  }

  BKE_mesh_clear_geometry_and_metadata(mesh_dst);

  const bool verts_num_changed = mesh_dst->verts_num != mesh_src->verts_num;
  mesh_dst->verts_num = mesh_src->verts_num;
  mesh_dst->edges_num = mesh_src->edges_num;
  mesh_dst->faces_num = mesh_src->faces_num;
  mesh_dst->corners_num = mesh_src->corners_num;

  /* Using #CD_MASK_MESH ensures that only data that should exist in Main meshes is moved. */
  const CustomData_MeshMasks mask = CD_MASK_MESH;
  CustomData_copy(&mesh_src->vert_data, &mesh_dst->vert_data, mask.vmask, mesh_src->verts_num);
  CustomData_copy(&mesh_src->edge_data, &mesh_dst->edge_data, mask.emask, mesh_src->edges_num);
  CustomData_copy(&mesh_src->face_data, &mesh_dst->face_data, mask.pmask, mesh_src->faces_num);
  CustomData_copy(
      &mesh_src->corner_data, &mesh_dst->corner_data, mask.lmask, mesh_src->corners_num);
  std::swap(mesh_dst->face_offset_indices, mesh_src->face_offset_indices);
  std::swap(mesh_dst->runtime->face_offsets_sharing_info,
            mesh_src->runtime->face_offsets_sharing_info);

  /* Make sure attribute names are moved. */
  std::swap(mesh_dst->active_color_attribute, mesh_src->active_color_attribute);
  std::swap(mesh_dst->default_color_attribute, mesh_src->default_color_attribute);
  std::swap(mesh_dst->vertex_group_names, mesh_src->vertex_group_names);

  BKE_mesh_copy_parameters(mesh_dst, mesh_src);

  /* For original meshes, shape key data is stored in the #Key data-block, so it
   * must be moved from the storage in #CustomData layers used for evaluation. */
  if (Key *key_dst = mesh_dst->key) {
    if (CustomData_has_layer(&mesh_src->vert_data, CD_SHAPEKEY)) {
      /* If no object, set to -1 so we don't mess up any shapekey layers. */
      const int uid_active = ob ? find_object_active_key_uid(*key_dst, *ob) : -1;
      move_shapekey_layers_to_keyblocks(*mesh_dst, mesh_src->vert_data, *key_dst, uid_active);
    }
    else if (verts_num_changed) {
      CLOG_WARN(&LOG, "Shape key data lost when replacing mesh '%s' in Main", mesh_src->id.name);
      id_us_min(&mesh_dst->key->id);
      mesh_dst->key = nullptr;
    }
  }

  /* Caches can have a large memory impact and aren't necessarily used, so don't indiscriminately
   * store all of them in the #Main data-base mesh. However, some caches are quite small and
   * copying them is "free" relative to how much work would be required if the data was needed. */
  copy_loose_vert_hint(*mesh_src, *mesh_dst);
  copy_loose_edge_hint(*mesh_src, *mesh_dst);
  copy_overlapping_hint(*mesh_src, *mesh_dst);

  BKE_id_free(nullptr, mesh_src);
}

void BKE_mesh_nomain_to_meshkey(Mesh *mesh_src, Mesh *mesh_dst, KeyBlock *kb)
{
  BLI_assert(mesh_src->id.tag & LIB_TAG_NO_MAIN);

  const int totvert = mesh_src->verts_num;

  if (totvert == 0 || mesh_dst->verts_num == 0 || mesh_dst->verts_num != totvert) {
    return;
  }

  if (kb->data) {
    MEM_freeN(kb->data);
  }
  kb->data = MEM_malloc_arrayN(mesh_dst->key->elemsize, mesh_dst->verts_num, "kb->data");
  kb->totelem = totvert;
  MutableSpan(static_cast<float3 *>(kb->data), kb->totelem).copy_from(mesh_src->vert_positions());
}
