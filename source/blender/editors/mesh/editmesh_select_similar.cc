/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_kdtree.h"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_set.hh"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_material.hh"
#include "BKE_object_types.hh"
#include "BKE_report.hh"

#include "DNA_meshdata_types.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_mesh.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"

#include "mesh_intern.hh" /* own include */

using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name Select Similar (Vert/Edge/Face) Operator - common
 * \{ */

static const EnumPropertyItem prop_similar_compare_types[] = {
    {SIM_CMP_EQ, "EQUAL", 0, "Equal", ""},
    {SIM_CMP_GT, "GREATER", 0, "Greater", ""},
    {SIM_CMP_LT, "LESS", 0, "Less", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem prop_similar_types[] = {
    {SIMVERT_NORMAL, "VERT_NORMAL", 0, "Normal", ""},
    {SIMVERT_FACE, "VERT_FACES", 0, "Amount of Adjacent Faces", ""},
    {SIMVERT_VGROUP, "VERT_GROUPS", 0, "Vertex Groups", ""},
    {SIMVERT_EDGE, "VERT_EDGES", 0, "Amount of Connecting Edges", ""},
    {SIMVERT_CREASE, "VERT_CREASE", 0, "Vertex Crease", ""},

    {SIMEDGE_LENGTH, "EDGE_LENGTH", 0, "Length", ""},
    {SIMEDGE_DIR, "EDGE_DIR", 0, "Direction", ""},
    {SIMEDGE_FACE, "EDGE_FACES", 0, "Amount of Faces Around an Edge", ""},
    {SIMEDGE_FACE_ANGLE, "EDGE_FACE_ANGLE", 0, "Face Angles", ""},
    {SIMEDGE_CREASE, "EDGE_CREASE", 0, "Crease", ""},
    {SIMEDGE_BEVEL, "EDGE_BEVEL", 0, "Bevel", ""},
    {SIMEDGE_SEAM, "EDGE_SEAM", 0, "Seam", ""},
    {SIMEDGE_SHARP, "EDGE_SHARP", 0, "Sharpness", ""},
#ifdef WITH_FREESTYLE
    {SIMEDGE_FREESTYLE, "EDGE_FREESTYLE", 0, "Freestyle Edge Marks", ""},
#endif

    {SIMFACE_MATERIAL, "FACE_MATERIAL", 0, "Material", ""},
    {SIMFACE_AREA, "FACE_AREA", 0, "Area", ""},
    {SIMFACE_SIDES, "FACE_SIDES", 0, "Polygon Sides", ""},
    {SIMFACE_PERIMETER, "FACE_PERIMETER", 0, "Perimeter", ""},
    {SIMFACE_NORMAL, "FACE_NORMAL", 0, "Normal", ""},
    {SIMFACE_COPLANAR, "FACE_COPLANAR", 0, "Coplanar", ""},
    {SIMFACE_SMOOTH, "FACE_SMOOTH", 0, "Flat/Smooth", ""},
#ifdef WITH_FREESTYLE
    {SIMFACE_FREESTYLE, "FACE_FREESTYLE", 0, "Freestyle Face Marks", ""},
#endif

    {0, nullptr, 0, nullptr, nullptr},
};

static int mesh_select_similar_compare_int(const int delta, const int compare)
{
  switch (compare) {
    case SIM_CMP_EQ:
      return (delta == 0);
    case SIM_CMP_GT:
      return (delta > 0);
    case SIM_CMP_LT:
      return (delta < 0);
    default:
      BLI_assert(0);
      return 0;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Similar Face
 * \{ */

enum {
  SIMFACE_DATA_NONE = 0,
  SIMFACE_DATA_TRUE = (1 << 0),
  SIMFACE_DATA_FALSE = (1 << 1),
  SIMFACE_DATA_ALL = (SIMFACE_DATA_TRUE | SIMFACE_DATA_FALSE),
};

/**
 * Return true if we still don't know the final value for this edge data.
 * In other words, if we need to keep iterating over the objects or we can
 * just go ahead and select all the objects.
 */
static bool face_data_value_set(BMFace *face, const int hflag, int *r_value)
{
  if (BM_elem_flag_test(face, hflag)) {
    *r_value |= SIMFACE_DATA_TRUE;
  }
  else {
    *r_value |= SIMFACE_DATA_FALSE;
  }

  return *r_value != SIMFACE_DATA_ALL;
}

/**
 * World space normalized plane from a face.
 */
static void face_to_plane(const Object *ob, BMFace *face, float r_plane[4])
{
  float normal[3], co[3];
  copy_v3_v3(normal, face->no);
  mul_transposed_mat3_m4_v3(ob->world_to_object().ptr(), normal);
  normalize_v3(normal);
  mul_v3_m4v3(co, ob->object_to_world().ptr(), BM_FACE_FIRST_LOOP(face)->v->co);
  plane_from_point_normal_v3(r_plane, co, normal);
}

/* TODO(dfelinto): `types` that should technically be compared in world space but are not:
 *  -SIMFACE_AREA
 *  -SIMFACE_PERIMETER
 */
static wmOperatorStatus similar_face_select_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  const int type = RNA_enum_get(op->ptr, "type");
  const float thresh = RNA_float_get(op->ptr, "threshold");
  const float thresh_radians = thresh * float(M_PI);
  const int compare = RNA_enum_get(op->ptr, "compare");

  int tot_faces_selected_all = 0;
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *ob : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    tot_faces_selected_all += em->bm->totfacesel;
  }

  if (tot_faces_selected_all == 0) {
    BKE_report(op->reports, RPT_ERROR, "No face selected");
    return OPERATOR_CANCELLED;
  }

  KDTree_1d *tree_1d = nullptr;
  KDTree_3d *tree_3d = nullptr;
  KDTree_4d *tree_4d = nullptr;
  GSet *gset = nullptr;
  int face_data_value = SIMFACE_DATA_NONE;

  switch (type) {
    case SIMFACE_AREA:
    case SIMFACE_PERIMETER:
      tree_1d = BLI_kdtree_1d_new(tot_faces_selected_all);
      break;
    case SIMFACE_NORMAL:
      tree_3d = BLI_kdtree_3d_new(tot_faces_selected_all);
      break;
    case SIMFACE_COPLANAR:
      tree_4d = BLI_kdtree_4d_new(tot_faces_selected_all);
      break;
    case SIMFACE_SIDES:
    case SIMFACE_MATERIAL:
      gset = BLI_gset_ptr_new("Select similar face");
      break;
  }

  int tree_index = 0;
  for (Object *ob : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMesh *bm = em->bm;
    Material ***material_array = nullptr;
    invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());

    if (bm->totfacesel == 0) {
      continue;
    }

    float ob_m3[3][3];
    copy_m3_m4(ob_m3, ob->object_to_world().ptr());

    int custom_data_offset = -1;
    switch (type) {
      case SIMFACE_MATERIAL: {
        if (ob->totcol == 0) {
          continue;
        }
        material_array = BKE_object_material_array_p(ob);
        break;
      }
      case SIMFACE_FREESTYLE: {
        custom_data_offset = CustomData_get_offset_named(
            &bm->pdata, CD_PROP_BOOL, "freestyle_face");
        if (custom_data_offset == -1) {
          face_data_value |= SIMFACE_DATA_FALSE;
          continue;
        }
        break;
      }
    }

    BMFace *face; /* Mesh face. */
    BMIter iter;  /* Selected faces iterator. */

    BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(face, BM_ELEM_SELECT)) {
        switch (type) {
          case SIMFACE_SIDES:
            BLI_gset_add(gset, POINTER_FROM_INT(face->len));
            break;
          case SIMFACE_MATERIAL: {
            Material *material = (*material_array)[face->mat_nr];
            if (material != nullptr) {
              BLI_gset_add(gset, material);
            }
            break;
          }
          case SIMFACE_AREA: {
            float area = BM_face_calc_area_with_mat3(face, ob_m3);
            BLI_kdtree_1d_insert(tree_1d, tree_index++, &area);
            break;
          }
          case SIMFACE_PERIMETER: {
            float perimeter = BM_face_calc_perimeter_with_mat3(face, ob_m3);
            BLI_kdtree_1d_insert(tree_1d, tree_index++, &perimeter);
            break;
          }
          case SIMFACE_NORMAL: {
            float normal[3];
            copy_v3_v3(normal, face->no);
            mul_transposed_mat3_m4_v3(ob->world_to_object().ptr(), normal);
            normalize_v3(normal);
            BLI_kdtree_3d_insert(tree_3d, tree_index++, normal);
            break;
          }
          case SIMFACE_COPLANAR: {
            float plane[4];
            face_to_plane(ob, face, plane);
            BLI_kdtree_4d_insert(tree_4d, tree_index++, plane);
            break;
          }
          case SIMFACE_SMOOTH: {
            if (!face_data_value_set(face, BM_ELEM_SMOOTH, &face_data_value)) {
              goto face_select_all;
            }
            break;
          }
          case SIMFACE_FREESTYLE: {
            if (custom_data_offset == -1 || !BM_ELEM_CD_GET_BOOL(face, custom_data_offset)) {
              face_data_value |= SIMFACE_DATA_FALSE;
            }
            else {
              face_data_value |= SIMFACE_DATA_TRUE;
            }
            if (face_data_value == SIMFACE_DATA_ALL) {
              goto face_select_all;
            }
            break;
          }
        }
      }
    }
  }

  BLI_assert((type != SIMFACE_FREESTYLE) || (face_data_value != SIMFACE_DATA_NONE));

  if (tree_1d != nullptr) {
    BLI_kdtree_1d_deduplicate(tree_1d);
    BLI_kdtree_1d_balance(tree_1d);
  }
  if (tree_3d != nullptr) {
    BLI_kdtree_3d_deduplicate(tree_3d);
    BLI_kdtree_3d_balance(tree_3d);
  }
  if (tree_4d != nullptr) {
    BLI_kdtree_4d_deduplicate(tree_4d);
    BLI_kdtree_4d_balance(tree_4d);
  }

  for (Object *ob : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMesh *bm = em->bm;
    bool changed = false;
    Material ***material_array = nullptr;

    float ob_m3[3][3];
    copy_m3_m4(ob_m3, ob->object_to_world().ptr());

    int custom_data_offset = -1;
    switch (type) {
      case SIMFACE_MATERIAL: {
        if (ob->totcol == 0) {
          continue;
        }
        material_array = BKE_object_material_array_p(ob);
        break;
      }
      case SIMFACE_FREESTYLE: {
        custom_data_offset = CustomData_get_offset_named(
            &bm->pdata, CD_PROP_BOOL, "freestyle_face");
        if ((face_data_value == SIMFACE_DATA_TRUE) && (custom_data_offset == -1)) {
          continue;
        }
        break;
      }
    }

    BMFace *face; /* Mesh face. */
    BMIter iter;  /* Selected faces iterator. */

    BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(face, BM_ELEM_SELECT) && !BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
        bool select = false;
        switch (type) {
          case SIMFACE_SIDES: {
            const int num_sides = face->len;
            GSetIterator gs_iter;
            GSET_ITER (gs_iter, gset) {
              const int num_sides_iter = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
              const int delta_i = num_sides - num_sides_iter;
              if (mesh_select_similar_compare_int(delta_i, compare)) {
                select = true;
                break;
              }
            }
            break;
          }
          case SIMFACE_MATERIAL: {
            const Material *material = (*material_array)[face->mat_nr];
            if (material == nullptr) {
              continue;
            }

            GSetIterator gs_iter;
            GSET_ITER (gs_iter, gset) {
              const Material *material_iter = static_cast<const Material *>(
                  BLI_gsetIterator_getKey(&gs_iter));
              if (material == material_iter) {
                select = true;
                break;
              }
            }
            break;
          }
          case SIMFACE_AREA: {
            float area = BM_face_calc_area_with_mat3(face, ob_m3);
            if (ED_select_similar_compare_float_tree(tree_1d, area, thresh, eSimilarCmp(compare)))
            {
              select = true;
            }
            break;
          }
          case SIMFACE_PERIMETER: {
            float perimeter = BM_face_calc_perimeter_with_mat3(face, ob_m3);
            if (ED_select_similar_compare_float_tree(
                    tree_1d, perimeter, thresh, eSimilarCmp(compare)))
            {
              select = true;
            }
            break;
          }
          case SIMFACE_NORMAL: {
            float normal[3];
            copy_v3_v3(normal, face->no);
            mul_transposed_mat3_m4_v3(ob->world_to_object().ptr(), normal);
            normalize_v3(normal);

            /* We are treating the normals as coordinates, the "nearest" one will
             * also be the one closest to the angle. */
            KDTreeNearest_3d nearest;
            if (BLI_kdtree_3d_find_nearest(tree_3d, normal, &nearest) != -1) {
              if (angle_normalized_v3v3(normal, nearest.co) <= thresh_radians) {
                select = true;
              }
            }
            break;
          }
          case SIMFACE_COPLANAR: {
            float plane[4];
            face_to_plane(ob, face, plane);

            KDTreeNearest_4d nearest;
            if (BLI_kdtree_4d_find_nearest(tree_4d, plane, &nearest) != -1) {
              if (nearest.dist <= thresh) {
                if ((fabsf(plane[3] - nearest.co[3]) <= thresh) &&
                    (angle_v3v3(plane, nearest.co) <= thresh_radians))
                {
                  select = true;
                }
              }
            }
            break;
          }
          case SIMFACE_SMOOTH:
            if ((BM_elem_flag_test(face, BM_ELEM_SMOOTH) != 0) ==
                ((face_data_value & SIMFACE_DATA_TRUE) != 0))
            {
              select = true;
            }
            break;
          case SIMFACE_FREESTYLE: {

            if (custom_data_offset == -1) {
              BLI_assert(face_data_value == SIMFACE_DATA_FALSE);
              select = true;
              break;
            }

            const bool value = BM_ELEM_CD_GET_BOOL(face, custom_data_offset);
            if (value == ((face_data_value & SIMFACE_DATA_TRUE) != 0)) {
              select = true;
            }
            break;
          }
        }

        if (select) {
          BM_face_select_set(bm, face, true);
          changed = true;
        }
      }
    }

    if (changed) {
      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);

      EDBMUpdate_Params params{};
      params.calc_looptris = false;
      params.calc_normals = false;
      params.is_destructive = false;
      EDBM_update(static_cast<Mesh *>(ob->data), &params);
    }
  }

  if (false) {
  face_select_all:
    BLI_assert(ELEM(type, SIMFACE_SMOOTH, SIMFACE_FREESTYLE));

    for (Object *ob : objects) {
      BMEditMesh *em = BKE_editmesh_from_object(ob);
      BMesh *bm = em->bm;

      BMFace *face; /* Mesh face. */
      BMIter iter;  /* Selected faces iterator. */

      BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(face, BM_ELEM_SELECT)) {
          BM_face_select_set(bm, face, true);
        }
      }
      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);

      EDBMUpdate_Params params{};
      params.calc_looptris = false;
      params.calc_normals = false;
      params.is_destructive = false;
      EDBM_update(static_cast<Mesh *>(ob->data), &params);
    }
  }

  BLI_kdtree_1d_free(tree_1d);
  BLI_kdtree_3d_free(tree_3d);
  BLI_kdtree_4d_free(tree_4d);
  if (gset != nullptr) {
    BLI_gset_free(gset, nullptr);
  }

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Similar Edge
 * \{ */

static void edge_pos_direction_worldspace_get(Object *ob, BMEdge *edge, float *r_dir)
{
  float v1[3], v2[3];
  copy_v3_v3(v1, edge->v1->co);
  copy_v3_v3(v2, edge->v2->co);

  mul_m4_v3(ob->object_to_world().ptr(), v1);
  mul_m4_v3(ob->object_to_world().ptr(), v2);

  sub_v3_v3v3(r_dir, v1, v2);
  normalize_v3(r_dir);
}

static float edge_length_squared_worldspace_get(Object *ob, BMEdge *edge)
{
  float v1[3], v2[3];

  mul_v3_mat3_m4v3(v1, ob->object_to_world().ptr(), edge->v1->co);
  mul_v3_mat3_m4v3(v2, ob->object_to_world().ptr(), edge->v2->co);

  return len_squared_v3v3(v1, v2);
}

enum {
  SIMEDGE_DATA_NONE = 0,
  SIMEDGE_DATA_TRUE = (1 << 0),
  SIMEDGE_DATA_FALSE = (1 << 1),
  SIMEDGE_DATA_ALL = (SIMEDGE_DATA_TRUE | SIMEDGE_DATA_FALSE),
};

/**
 * Return true if we still don't know the final value for this edge data.
 * In other words, if we need to keep iterating over the objects or we can
 * just go ahead and select all the objects.
 */
static bool edge_data_value_set(BMEdge *edge, const int hflag, int *r_value)
{
  if (BM_elem_flag_test(edge, hflag)) {
    *r_value |= SIMEDGE_DATA_TRUE;
  }
  else {
    *r_value |= SIMEDGE_DATA_FALSE;
  }

  return *r_value != SIMEDGE_DATA_ALL;
}

/* TODO(dfelinto): `types` that should technically be compared in world space but are not:
 *  -SIMEDGE_FACE_ANGLE
 */
static wmOperatorStatus similar_edge_select_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  const int type = RNA_enum_get(op->ptr, "type");
  const float thresh = RNA_float_get(op->ptr, "threshold");
  const float thresh_radians = thresh * float(M_PI) + FLT_EPSILON;
  const int compare = RNA_enum_get(op->ptr, "compare");

  int tot_edges_selected_all = 0;
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *ob : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    tot_edges_selected_all += em->bm->totedgesel;
  }

  if (tot_edges_selected_all == 0) {
    BKE_report(op->reports, RPT_ERROR, "No edge selected");
    return OPERATOR_CANCELLED;
  }

  KDTree_1d *tree_1d = nullptr;
  KDTree_3d *tree_3d = nullptr;
  GSet *gset = nullptr;
  int edge_data_value = SIMEDGE_DATA_NONE;

  switch (type) {
    case SIMEDGE_CREASE:
    case SIMEDGE_BEVEL:
    case SIMEDGE_FACE_ANGLE:
    case SIMEDGE_LENGTH:
      tree_1d = BLI_kdtree_1d_new(tot_edges_selected_all);
      break;
    case SIMEDGE_DIR:
      tree_3d = BLI_kdtree_3d_new(tot_edges_selected_all * 2);
      break;
    case SIMEDGE_FACE:
      gset = BLI_gset_ptr_new("Select similar edge: face");
      break;
  }

  int tree_index = 0;
  for (Object *ob : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMesh *bm = em->bm;

    if (bm->totedgesel == 0) {
      continue;
    }

    switch (type) {
      case SIMEDGE_FREESTYLE: {
        if (!CustomData_has_layer_named(&bm->edata, CD_PROP_BOOL, "freestyle_edge")) {
          edge_data_value |= SIMEDGE_DATA_FALSE;
          continue;
        }
        break;
      }
      case SIMEDGE_CREASE: {
        if (!CustomData_has_layer_named(&bm->edata, CD_PROP_FLOAT, "crease_edge")) {
          float pos = 0.0f;
          BLI_kdtree_1d_insert(tree_1d, tree_index++, &pos);
          continue;
        }
        break;
      }
      case SIMEDGE_BEVEL: {
        if (!CustomData_has_layer_named(&bm->edata, CD_PROP_FLOAT, "bevel_weight_edge")) {
          float pos = 0.0f;
          BLI_kdtree_1d_insert(tree_1d, tree_index++, &pos);
          continue;
        }
        break;
      }
    }

    int custom_data_offset;
    switch (type) {
      case SIMEDGE_FREESTYLE: {
        custom_data_offset = CustomData_get_offset_named(
            &bm->edata, CD_PROP_BOOL, "freestyle_edge");
        break;
      }
      case SIMEDGE_CREASE:
        custom_data_offset = CustomData_get_offset_named(&bm->edata, CD_PROP_FLOAT, "crease_edge");
        break;
      case SIMEDGE_BEVEL:
        custom_data_offset = CustomData_get_offset_named(
            &bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
        break;
    }

    float ob_m3[3][3], ob_m3_inv[3][3];
    copy_m3_m4(ob_m3, ob->object_to_world().ptr());
    invert_m3_m3(ob_m3_inv, ob_m3);

    BMEdge *edge; /* Mesh edge. */
    BMIter iter;  /* Selected edges iterator. */

    BM_ITER_MESH (edge, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(edge, BM_ELEM_SELECT)) {
        switch (type) {
          case SIMEDGE_FACE:
            BLI_gset_add(gset, POINTER_FROM_INT(BM_edge_face_count(edge)));
            break;
          case SIMEDGE_DIR: {
            float dir[3], dir_flip[3];
            edge_pos_direction_worldspace_get(ob, edge, dir);
            BLI_kdtree_3d_insert(tree_3d, tree_index++, dir);
            /* Also store the flipped direction so it can be checked regardless of the verts order
             * of the edges. */
            negate_v3_v3(dir_flip, dir);
            BLI_kdtree_3d_insert(tree_3d, tree_index++, dir_flip);
            break;
          }
          case SIMEDGE_LENGTH: {
            float length = edge_length_squared_worldspace_get(ob, edge);
            BLI_kdtree_1d_insert(tree_1d, tree_index++, &length);
            break;
          }
          case SIMEDGE_FACE_ANGLE: {
            if (BM_edge_face_count_at_most(edge, 2) == 2) {
              float angle = BM_edge_calc_face_angle_with_imat3(edge, ob_m3_inv);
              BLI_kdtree_1d_insert(tree_1d, tree_index++, &angle);
            }
            break;
          }
          case SIMEDGE_SEAM:
            if (!edge_data_value_set(edge, BM_ELEM_SEAM, &edge_data_value)) {
              goto edge_select_all;
            }
            break;
          case SIMEDGE_SHARP:
            if (!edge_data_value_set(edge, BM_ELEM_SMOOTH, &edge_data_value)) {
              goto edge_select_all;
            }
            break;
          case SIMEDGE_FREESTYLE: {
            if (custom_data_offset == -1 || !BM_ELEM_CD_GET_BOOL(edge, custom_data_offset)) {
              edge_data_value |= SIMEDGE_DATA_FALSE;
            }
            else {
              edge_data_value |= SIMEDGE_DATA_TRUE;
            }
            if (edge_data_value == SIMEDGE_DATA_ALL) {
              goto edge_select_all;
            }
            break;
          }
          case SIMEDGE_CREASE:
          case SIMEDGE_BEVEL: {
            const float *value = BM_ELEM_CD_GET_FLOAT_P(edge, custom_data_offset);
            BLI_kdtree_1d_insert(tree_1d, tree_index++, value);
            break;
          }
        }
      }
    }
  }

  BLI_assert((type != SIMEDGE_FREESTYLE) || (edge_data_value != SIMEDGE_DATA_NONE));

  if (tree_1d != nullptr) {
    BLI_kdtree_1d_deduplicate(tree_1d);
    BLI_kdtree_1d_balance(tree_1d);
  }
  if (tree_3d != nullptr) {
    BLI_kdtree_3d_deduplicate(tree_3d);
    BLI_kdtree_3d_balance(tree_3d);
  }

  for (Object *ob : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMesh *bm = em->bm;
    bool changed = false;

    bool has_custom_data_layer = false;
    switch (type) {
      case SIMEDGE_FREESTYLE: {
        has_custom_data_layer = CustomData_has_layer_named(
            &bm->edata, CD_PROP_BOOL, "freestyle_edge");
        if ((edge_data_value == SIMEDGE_DATA_TRUE) && !has_custom_data_layer) {
          continue;
        }
        break;
      }
      case SIMEDGE_CREASE: {
        has_custom_data_layer = CustomData_has_layer_named(
            &bm->edata, CD_PROP_FLOAT, "crease_edge");
        if (!has_custom_data_layer) {
          /* Proceed only if we have to select all the edges that have custom data value of 0.0f.
           * In this case we will just select all the edges.
           * Otherwise continue the for loop. */
          if (!ED_select_similar_compare_float_tree(tree_1d, 0.0f, thresh, eSimilarCmp(compare))) {
            continue;
          }
        }
        break;
      }
      case SIMEDGE_BEVEL: {
        has_custom_data_layer = CustomData_has_layer_named(
            &bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
        if (!has_custom_data_layer) {
          /* Proceed only if we have to select all the edges that have custom data value of 0.0f.
           * In this case we will just select all the edges.
           * Otherwise continue the for loop. */
          if (!ED_select_similar_compare_float_tree(tree_1d, 0.0f, thresh, eSimilarCmp(compare))) {
            continue;
          }
        }
        break;
      }
    }

    float ob_m3[3][3], ob_m3_inv[3][3];
    copy_m3_m4(ob_m3, ob->object_to_world().ptr());
    invert_m3_m3(ob_m3_inv, ob_m3);

    int custom_data_offset;
    switch (type) {
      case SIMEDGE_FREESTYLE:
        custom_data_offset = CustomData_get_offset_named(
            &bm->edata, CD_PROP_BOOL, "freestyle_edge");
        break;
      case SIMEDGE_CREASE:
        custom_data_offset = CustomData_get_offset_named(&bm->edata, CD_PROP_FLOAT, "crease_edge");
        break;
      case SIMEDGE_BEVEL:
        custom_data_offset = CustomData_get_offset_named(
            &bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
        break;
    }

    BMEdge *edge; /* Mesh edge. */
    BMIter iter;  /* Selected edges iterator. */

    BM_ITER_MESH (edge, &iter, bm, BM_EDGES_OF_MESH) {
      if (!BM_elem_flag_test(edge, BM_ELEM_SELECT) && !BM_elem_flag_test(edge, BM_ELEM_HIDDEN)) {
        bool select = false;
        switch (type) {
          case SIMEDGE_FACE: {
            const int num_faces = BM_edge_face_count(edge);
            GSetIterator gs_iter;
            GSET_ITER (gs_iter, gset) {
              const int num_faces_iter = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
              const int delta_i = num_faces - num_faces_iter;
              if (mesh_select_similar_compare_int(delta_i, compare)) {
                select = true;
                break;
              }
            }
            break;
          }
          case SIMEDGE_DIR: {
            float dir[3];
            edge_pos_direction_worldspace_get(ob, edge, dir);

            /* We are treating the direction as coordinates, the "nearest" one will
             * also be the one closest to the intended direction. */
            KDTreeNearest_3d nearest;
            if (BLI_kdtree_3d_find_nearest(tree_3d, dir, &nearest) != -1) {
              if (angle_normalized_v3v3(dir, nearest.co) <= thresh_radians) {
                select = true;
              }
            }
            break;
          }
          case SIMEDGE_LENGTH: {
            float length = edge_length_squared_worldspace_get(ob, edge);
            if (ED_select_similar_compare_float_tree(
                    tree_1d, length, thresh, eSimilarCmp(compare)))
            {
              select = true;
            }
            break;
          }
          case SIMEDGE_FACE_ANGLE: {
            if (BM_edge_face_count_at_most(edge, 2) == 2) {
              float angle = BM_edge_calc_face_angle_with_imat3(edge, ob_m3_inv);
              if (ED_select_similar_compare_float_tree(tree_1d, angle, thresh, SIM_CMP_EQ)) {
                select = true;
              }
            }
            break;
          }
          case SIMEDGE_SEAM:
            if ((BM_elem_flag_test(edge, BM_ELEM_SEAM) != 0) ==
                ((edge_data_value & SIMEDGE_DATA_TRUE) != 0))
            {
              select = true;
            }
            break;
          case SIMEDGE_SHARP:
            if ((BM_elem_flag_test(edge, BM_ELEM_SMOOTH) != 0) ==
                ((edge_data_value & SIMEDGE_DATA_TRUE) != 0))
            {
              select = true;
            }
            break;
          case SIMEDGE_FREESTYLE: {
            if (!has_custom_data_layer) {
              BLI_assert(edge_data_value == SIMEDGE_DATA_FALSE);
              select = true;
              break;
            }

            const bool value = BM_ELEM_CD_GET_BOOL(edge, custom_data_offset);
            if (value == ((edge_data_value & SIMEDGE_DATA_TRUE) != 0)) {
              select = true;
            }
            break;
          }
          case SIMEDGE_CREASE:
          case SIMEDGE_BEVEL: {
            if (!has_custom_data_layer) {
              select = true;
              break;
            }

            const float *value = BM_ELEM_CD_GET_FLOAT_P(edge, custom_data_offset);
            if (ED_select_similar_compare_float_tree(
                    tree_1d, *value, thresh, eSimilarCmp(compare)))
            {
              select = true;
            }
            break;
          }
        }

        if (select) {
          BM_edge_select_set(bm, edge, true);
          changed = true;
        }
      }
    }

    if (changed) {
      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);

      EDBMUpdate_Params params{};
      params.calc_looptris = false;
      params.calc_normals = false;
      params.is_destructive = false;
      EDBM_update(static_cast<Mesh *>(ob->data), &params);
    }
  }

  if (false) {
  edge_select_all:
    BLI_assert(ELEM(type, SIMEDGE_SEAM, SIMEDGE_SHARP, SIMEDGE_FREESTYLE));

    for (Object *ob : objects) {
      BMEditMesh *em = BKE_editmesh_from_object(ob);
      BMesh *bm = em->bm;

      BMEdge *edge; /* Mesh edge. */
      BMIter iter;  /* Selected edges iterator. */

      BM_ITER_MESH (edge, &iter, bm, BM_EDGES_OF_MESH) {
        if (!BM_elem_flag_test(edge, BM_ELEM_SELECT)) {
          BM_edge_select_set(bm, edge, true);
        }
      }
      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);

      EDBMUpdate_Params params{};
      params.calc_looptris = false;
      params.calc_normals = false;
      params.is_destructive = false;
      EDBM_update(static_cast<Mesh *>(ob->data), &params);
    }
  }

  BLI_kdtree_1d_free(tree_1d);
  BLI_kdtree_3d_free(tree_3d);
  if (gset != nullptr) {
    BLI_gset_free(gset, nullptr);
  }

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Similar Vert
 * \{ */

static wmOperatorStatus similar_vert_select_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  /* get the type from RNA */
  const int type = RNA_enum_get(op->ptr, "type");
  const float thresh = RNA_float_get(op->ptr, "threshold");
  const float thresh_radians = thresh * float(M_PI) + FLT_EPSILON;
  const int compare = RNA_enum_get(op->ptr, "compare");

  int tot_verts_selected_all = 0;
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *ob : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    tot_verts_selected_all += em->bm->totvertsel;
  }

  if (tot_verts_selected_all == 0) {
    BKE_report(op->reports, RPT_ERROR, "No vertex selected");
    return OPERATOR_CANCELLED;
  }

  KDTree_3d *tree_3d = nullptr;
  KDTree_1d *tree_1d = nullptr;
  blender::Set<blender::StringRef> selected_vertex_groups;
  blender::Set<int> connected_elems_num_set;

  switch (type) {
    case SIMVERT_NORMAL:
      tree_3d = BLI_kdtree_3d_new(tot_verts_selected_all);
      break;
    case SIMVERT_CREASE:
      tree_1d = BLI_kdtree_1d_new(tot_verts_selected_all);
      break;
    case SIMVERT_EDGE:
    case SIMVERT_FACE:
    case SIMVERT_VGROUP:
      break;
  }

  int normal_tree_index = 0;
  int tree_1d_index = 0;
  for (Object *ob : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMesh *bm = em->bm;
    int cd_dvert_offset = -1;
    int cd_crease_offset = -1;
    BLI_bitmap *defbase_selected = nullptr;
    int defbase_len = 0;

    invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());

    if (bm->totvertsel == 0) {
      continue;
    }

    if (type == SIMVERT_VGROUP) {
      cd_dvert_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
      if (cd_dvert_offset == -1) {
        continue;
      }
      defbase_len = BKE_object_defgroup_count(ob);
      if (defbase_len == 0) {
        continue;
      }
      defbase_selected = BLI_BITMAP_NEW(defbase_len, __func__);
    }
    else if (type == SIMVERT_CREASE) {
      if (!CustomData_has_layer_named(&bm->vdata, CD_PROP_FLOAT, "crease_vert")) {
        float pos = 0.0f;
        BLI_kdtree_1d_insert(tree_1d, tree_1d_index++, &pos);
        continue;
      }
      cd_crease_offset = CustomData_get_offset_named(&bm->vdata, CD_PROP_FLOAT, "crease_vert");
    }

    BMVert *vert; /* Mesh vertex. */
    BMIter iter;  /* Selected verts iterator. */

    BM_ITER_MESH (vert, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(vert, BM_ELEM_SELECT)) {
        switch (type) {
          case SIMVERT_FACE:
            connected_elems_num_set.add(BM_vert_face_count(vert));
            break;
          case SIMVERT_EDGE:
            connected_elems_num_set.add(BM_vert_edge_count(vert));
            break;
          case SIMVERT_NORMAL: {
            float normal[3];
            copy_v3_v3(normal, vert->no);
            mul_transposed_mat3_m4_v3(ob->world_to_object().ptr(), normal);
            normalize_v3(normal);

            BLI_kdtree_3d_insert(tree_3d, normal_tree_index++, normal);
            break;
          }
          case SIMVERT_VGROUP: {
            MDeformVert *dvert = static_cast<MDeformVert *>(
                BM_ELEM_CD_GET_VOID_P(vert, cd_dvert_offset));
            MDeformWeight *dw = dvert->dw;

            for (int i = 0; i < dvert->totweight; i++, dw++) {
              if (dw->weight > 0.0f) {
                if (LIKELY(dw->def_nr < defbase_len)) {
                  BLI_BITMAP_ENABLE(defbase_selected, dw->def_nr);
                }
              }
            }
            break;
          }
          case SIMVERT_CREASE: {
            const float *value = BM_ELEM_CD_GET_FLOAT_P(vert, cd_crease_offset);
            BLI_kdtree_1d_insert(tree_1d, tree_1d_index++, value);
            break;
          }
        }
      }
    }

    if (type == SIMVERT_VGROUP) {
      /* We store the names of the vertex groups, so we can select
       * vertex groups with the same name in different objects. */

      const ListBase *defbase = BKE_object_defgroup_list(ob);

      int i = 0;
      LISTBASE_FOREACH (bDeformGroup *, dg, defbase) {
        if (BLI_BITMAP_TEST(defbase_selected, i)) {
          selected_vertex_groups.add_as(dg->name);
        }
        i += 1;
      }
      MEM_freeN(defbase_selected);
    }
  }

  if (type == SIMVERT_VGROUP) {
    if (selected_vertex_groups.is_empty()) {
      BKE_report(op->reports, RPT_INFO, "No vertex group among the selected vertices");
    }
  }

  /* Remove duplicated entries. */
  if (tree_1d != nullptr) {
    BLI_kdtree_1d_deduplicate(tree_1d);
    BLI_kdtree_1d_balance(tree_1d);
  }
  if (tree_3d != nullptr) {
    BLI_kdtree_3d_deduplicate(tree_3d);
    BLI_kdtree_3d_balance(tree_3d);
  }

  /* Run the matching operations. */
  for (Object *ob : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMesh *bm = em->bm;
    bool changed = false;
    bool has_crease_layer = false;
    int cd_dvert_offset = -1;
    int cd_crease_offset = -1;
    BLI_bitmap *defbase_selected = nullptr;
    int defbase_len = 0;

    if (type == SIMVERT_VGROUP) {
      cd_dvert_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
      if (cd_dvert_offset == -1) {
        continue;
      }
      const ListBase *defbase = BKE_object_defgroup_list(ob);
      defbase_len = BLI_listbase_count(defbase);
      if (defbase_len == 0) {
        continue;
      }

      /* We map back the names of the vertex groups to their corresponding indices
       * for this object. This is fast, and keep the logic for each vertex very simple. */

      defbase_selected = BLI_BITMAP_NEW(defbase_len, __func__);
      bool found_any = false;
      for (const blender::StringRef name : selected_vertex_groups) {
        int vgroup_id = BKE_defgroup_name_index(defbase, name);
        if (vgroup_id != -1) {
          BLI_BITMAP_ENABLE(defbase_selected, vgroup_id);
          found_any = true;
        }
      }
      if (found_any == false) {
        MEM_freeN(defbase_selected);
        continue;
      }
    }
    else if (type == SIMVERT_CREASE) {
      cd_crease_offset = CustomData_get_offset_named(&bm->vdata, CD_PROP_FLOAT, "crease_vert");
      has_crease_layer = CustomData_has_layer_named(&bm->vdata, CD_PROP_FLOAT, "crease_vert");
      if (!has_crease_layer) {
        /* Proceed only if we have to select all the vertices that have custom data value of 0.0f.
         * In this case we will just select all the vertices.
         * Otherwise continue the for loop. */
        if (!ED_select_similar_compare_float_tree(tree_1d, 0.0f, thresh, eSimilarCmp(compare))) {
          continue;
        }
      }
    }

    BMVert *vert; /* Mesh vertex. */
    BMIter iter;  /* Selected verts iterator. */

    BM_ITER_MESH (vert, &iter, bm, BM_VERTS_OF_MESH) {
      if (!BM_elem_flag_test(vert, BM_ELEM_SELECT) && !BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
        bool select = false;
        switch (type) {
          case SIMVERT_EDGE: {
            const int num_edges = BM_vert_edge_count(vert);
            for (const int num_edges_iter : connected_elems_num_set) {
              const int delta_i = num_edges - num_edges_iter;
              if (mesh_select_similar_compare_int(delta_i, compare)) {
                select = true;
                break;
              }
            }
            break;
          }
          case SIMVERT_FACE: {
            const int num_faces = BM_vert_face_count(vert);
            for (const int num_faces_iter : connected_elems_num_set) {
              const int delta_i = num_faces - num_faces_iter;
              if (mesh_select_similar_compare_int(delta_i, compare)) {
                select = true;
                break;
              }
            }
            break;
          }
          case SIMVERT_NORMAL: {
            float normal[3];
            copy_v3_v3(normal, vert->no);
            mul_transposed_mat3_m4_v3(ob->world_to_object().ptr(), normal);
            normalize_v3(normal);

            /* We are treating the normals as coordinates, the "nearest" one will
             * also be the one closest to the angle. */
            KDTreeNearest_3d nearest;
            if (BLI_kdtree_3d_find_nearest(tree_3d, normal, &nearest) != -1) {
              if (angle_normalized_v3v3(normal, nearest.co) <= thresh_radians) {
                select = true;
              }
            }
            break;
          }
          case SIMVERT_VGROUP: {
            MDeformVert *dvert = static_cast<MDeformVert *>(
                BM_ELEM_CD_GET_VOID_P(vert, cd_dvert_offset));
            MDeformWeight *dw = dvert->dw;

            for (int i = 0; i < dvert->totweight; i++, dw++) {
              if (dw->weight > 0.0f) {
                if (LIKELY(dw->def_nr < defbase_len)) {
                  if (BLI_BITMAP_TEST(defbase_selected, dw->def_nr)) {
                    select = true;
                    break;
                  }
                }
              }
            }
            break;
          }
          case SIMVERT_CREASE: {
            if (!has_crease_layer) {
              select = true;
              break;
            }
            const float *value = BM_ELEM_CD_GET_FLOAT_P(vert, cd_crease_offset);
            if (ED_select_similar_compare_float_tree(
                    tree_1d, *value, thresh, eSimilarCmp(compare)))
            {
              select = true;
            }
            break;
          }
        }

        if (select) {
          BM_vert_select_set(bm, vert, true);
          changed = true;
        }
      }
    }

    if (type == SIMVERT_VGROUP) {
      MEM_freeN(defbase_selected);
    }

    if (changed) {
      EDBM_selectmode_flush(em);
      EDBMUpdate_Params params{};
      params.calc_looptris = false;
      params.calc_normals = false;
      params.is_destructive = false;
      EDBM_update(static_cast<Mesh *>(ob->data), &params);
    }
  }

  BLI_kdtree_1d_free(tree_1d);
  BLI_kdtree_3d_free(tree_3d);

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Similar Operator
 * \{ */

static wmOperatorStatus edbm_select_similar_exec(bContext *C, wmOperator *op)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "threshold");

  const int type = RNA_enum_get(op->ptr, "type");

  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_float_set(op->ptr, prop, ts->select_thresh);
  }
  else {
    ts->select_thresh = RNA_property_float_get(op->ptr, prop);
  }

  if (type < 100) {
    return similar_vert_select_exec(C, op);
  }
  if (type < 200) {
    return similar_edge_select_exec(C, op);
  }
  return similar_face_select_exec(C, op);
}

static const EnumPropertyItem *select_similar_type_itemf(bContext *C,
                                                         PointerRNA * /*ptr*/,
                                                         PropertyRNA * /*prop*/,
                                                         bool *r_free)
{
  Object *obedit;

  if (!C) { /* needed for docs and i18n tools */
    return prop_similar_types;
  }

  obedit = CTX_data_edit_object(C);

  if (obedit && obedit->type == OB_MESH) {
    EnumPropertyItem *item = nullptr;
    int a, totitem = 0;
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (em->selectmode & SCE_SELECT_VERTEX) {
      for (a = SIMVERT_NORMAL; a < SIMEDGE_LENGTH; a++) {
        RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
      }
    }
    else if (em->selectmode & SCE_SELECT_EDGE) {
      for (a = SIMEDGE_LENGTH; a < SIMFACE_MATERIAL; a++) {
        RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
      }
    }
    else if (em->selectmode & SCE_SELECT_FACE) {
#ifdef WITH_FREESTYLE
      const int a_end = SIMFACE_FREESTYLE;
#else
      const int a_end = SIMFACE_SMOOTH;
#endif
      for (a = SIMFACE_MATERIAL; a <= a_end; a++) {
        RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
      }
    }
    RNA_enum_item_end(&item, &totitem);

    *r_free = true;

    return item;
  }

  return prop_similar_types;
}

static bool edbm_select_similar_poll_property(const bContext * /*C*/,
                                              wmOperator *op,
                                              const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);
  const int type = RNA_enum_get(op->ptr, "type");

  /* Only show compare when it is used. */
  if (STREQ(prop_id, "compare")) {
    if (type == SIMVERT_VGROUP) {
      return false;
    }
  }
  /* Only show threshold when it is used. */
  else if (STREQ(prop_id, "threshold")) {
    if (!ELEM(type,
              SIMVERT_NORMAL,
              SIMEDGE_BEVEL,
              SIMEDGE_CREASE,
              SIMEDGE_DIR,
              SIMEDGE_LENGTH,
              SIMEDGE_FACE_ANGLE,
              SIMFACE_AREA,
              SIMFACE_PERIMETER,
              SIMFACE_NORMAL,
              SIMFACE_COPLANAR))
    {
      return false;
    }
  }

  return true;
}

void MESH_OT_select_similar(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Similar";
  ot->idname = "MESH_OT_select_similar";
  ot->description = "Select similar vertices, edges or faces by property types";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = edbm_select_similar_exec;
  ot->poll = ED_operator_editmesh;
  ot->poll_property = edbm_select_similar_poll_property;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = ot->prop = RNA_def_enum(ot->srna, "type", prop_similar_types, SIMVERT_NORMAL, "Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MESH);
  RNA_def_enum_funcs(prop, select_similar_type_itemf);

  RNA_def_enum(ot->srna, "compare", prop_similar_compare_types, SIM_CMP_EQ, "Compare", "");

  prop = RNA_def_float(ot->srna, "threshold", 0.0f, 0.0f, 100000.0f, "Threshold", "", 0.0f, 1.0f);
  /* Very small values are needed sometimes, similar area of small faces for e.g: see #87823 */
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.01, 5);
}

/** \} */
