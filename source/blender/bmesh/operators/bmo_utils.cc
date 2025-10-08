/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * utility bmesh operators, e.g. transform,
 * translate, rotate, scale, etc.
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_alloca.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_attribute.h"
#include "BKE_customdata.hh"

#include "bmesh.hh"

#include "intern/bmesh_operators_private.hh" /* own include */

#define ELE_NEW 1

void bmo_create_vert_exec(BMesh *bm, BMOperator *op)
{
  float vec[3];

  BMO_slot_vec_get(op->slots_in, "co", vec);

  BMO_vert_flag_enable(bm, BM_vert_create(bm, vec, nullptr, BM_CREATE_NOP), ELE_NEW);
  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "vert.out", BM_VERT, ELE_NEW);
}

void bmo_transform_exec(BMesh *bm, BMOperator *op)
{
  BMOIter iter;
  BMVert *v;
  float mat[4][4], mat_space[4][4], imat_space[4][4];

  const uint shape_keys_len = BMO_slot_bool_get(op->slots_in, "use_shapekey") ?
                                  CustomData_number_of_layers(&bm->vdata, CD_SHAPEKEY) :
                                  0;
  const uint cd_shape_key_offset = CustomData_get_offset(&bm->vdata, CD_SHAPEKEY);

  BMO_slot_mat4_get(op->slots_in, "matrix", mat);
  BMO_slot_mat4_get(op->slots_in, "space", mat_space);

  if (!is_zero_m4(mat_space)) {
    invert_m4_m4(imat_space, mat_space);
    mul_m4_series(mat, imat_space, mat, mat_space);
  }

  BMO_ITER (v, &iter, op->slots_in, "verts", BM_VERT) {
    mul_m4_v3(mat, v->co);

    if (shape_keys_len != 0) {
      float (*co_dst)[3] = static_cast<float (*)[3]>(
          BM_ELEM_CD_GET_VOID_P(v, cd_shape_key_offset));
      for (int i = 0; i < shape_keys_len; i++, co_dst++) {
        mul_m4_v3(mat, *co_dst);
      }
    }
  }
}

void bmo_translate_exec(BMesh *bm, BMOperator *op)
{
  float mat[4][4], vec[3];

  BMO_slot_vec_get(op->slots_in, "vec", vec);

  unit_m4(mat);
  copy_v3_v3(mat[3], vec);

  BMO_op_callf(bm,
               op->flag,
               "transform matrix=%m4 space=%s verts=%s use_shapekey=%s",
               mat,
               op,
               "space",
               op,
               "verts",
               op,
               "use_shapekey");
}

void bmo_scale_exec(BMesh *bm, BMOperator *op)
{
  float mat[3][3], vec[3];

  BMO_slot_vec_get(op->slots_in, "vec", vec);

  unit_m3(mat);
  mat[0][0] = vec[0];
  mat[1][1] = vec[1];
  mat[2][2] = vec[2];

  BMO_op_callf(bm,
               op->flag,
               "transform matrix=%m3 space=%s verts=%s use_shapekey=%s",
               mat,
               op,
               "space",
               op,
               "verts",
               op,
               "use_shapekey");
}

void bmo_rotate_exec(BMesh *bm, BMOperator *op)
{
  float center[3];
  float mat[4][4];

  BMO_slot_vec_get(op->slots_in, "cent", center);
  BMO_slot_mat4_get(op->slots_in, "matrix", mat);
  transform_pivot_set_m4(mat, center);

  BMO_op_callf(bm,
               op->flag,
               "transform matrix=%m4 space=%s verts=%s use_shapekey=%s",
               mat,
               op,
               "space",
               op,
               "verts",
               op,
               "use_shapekey");
}

void bmo_reverse_faces_exec(BMesh *bm, BMOperator *op)
{
  const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
  const bool use_loop_mdisp_flip = BMO_slot_bool_get(op->slots_in, "flip_multires");
  BMOIter siter;
  BMFace *f;

  BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
    BM_face_normal_flip_ex(bm, f, cd_loop_mdisp_offset, use_loop_mdisp_flip);
  }
}

#define SEL_FLAG 1
#define SEL_ORIG 2

void bmo_flip_quad_tessellation_exec(BMesh *bm, BMOperator *op)
{
  BMOIter siter;
  BMFace *f;
  bool changed = false;
  BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
    if (f->len == 4) {
      f->l_first = f->l_first->next;
      changed = true;
    }
  }
  if (changed) {
    bm->elem_index_dirty |= BM_LOOP;
  }
}

static void bmo_face_flag_set_flush(BMesh *bm, BMFace *f, const short oflag, const bool value)
{
  BMLoop *l_iter;
  BMLoop *l_first;

  BMO_face_flag_set(bm, f, oflag, value);
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    BMO_edge_flag_set(bm, l_iter->e, oflag, value);
    BMO_vert_flag_set(bm, l_iter->v, oflag, value);
  } while ((l_iter = l_iter->next) != l_first);
}

static void bmo_region_extend_expand(BMesh *bm,
                                     BMOperator *op,
                                     const bool use_faces,
                                     const bool use_faces_step)
{
  BMOIter siter;

  if (!use_faces) {
    BMVert *v;

    BMO_ITER (v, &siter, op->slots_in, "geom", BM_VERT) {
      bool found = false;

      {
        BMIter eiter;
        BMEdge *e;

        BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
          if (!BMO_edge_flag_test(bm, e, SEL_ORIG) && !BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
            found = true;
            break;
          }
        }
      }

      if (found) {
        if (!use_faces_step) {
          BMIter eiter;
          BMEdge *e;

          BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
            if (!BMO_edge_flag_test(bm, e, SEL_FLAG) && !BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
              BMO_edge_flag_enable(bm, e, SEL_FLAG);
              BMO_vert_flag_enable(bm, BM_edge_other_vert(e, v), SEL_FLAG);
            }
          }
        }
        else {
          BMIter fiter;
          BMFace *f;

          BM_ITER_ELEM (f, &fiter, v, BM_FACES_OF_VERT) {
            if (!BMO_face_flag_test(bm, f, SEL_FLAG) && !BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
              bmo_face_flag_set_flush(bm, f, SEL_FLAG, true);
            }
          }

          /* handle wire edges (when stepping over faces) */
          {
            BMIter eiter;
            BMEdge *e;
            BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
              if (BM_edge_is_wire(e)) {
                if (!BMO_edge_flag_test(bm, e, SEL_FLAG) && !BM_elem_flag_test(e, BM_ELEM_HIDDEN))
                {
                  BMO_edge_flag_enable(bm, e, SEL_FLAG);
                  BMO_vert_flag_enable(bm, BM_edge_other_vert(e, v), SEL_FLAG);
                }
              }
            }
          }
        }
      }
    }
  }
  else {
    BMFace *f;

    BMO_ITER (f, &siter, op->slots_in, "geom", BM_FACE) {
      BMIter liter;
      BMLoop *l;

      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        if (!use_faces_step) {
          BMIter fiter;
          BMFace *f_other;

          BM_ITER_ELEM (f_other, &fiter, l->e, BM_FACES_OF_EDGE) {
            if (!BMO_face_flag_test(bm, f_other, SEL_ORIG | SEL_FLAG) &&
                !BM_elem_flag_test(f_other, BM_ELEM_HIDDEN))
            {
              BMO_face_flag_enable(bm, f_other, SEL_FLAG);
            }
          }
        }
        else {
          BMIter fiter;
          BMFace *f_other;

          BM_ITER_ELEM (f_other, &fiter, l->v, BM_FACES_OF_VERT) {
            if (!BMO_face_flag_test(bm, f_other, SEL_ORIG | SEL_FLAG) &&
                !BM_elem_flag_test(f_other, BM_ELEM_HIDDEN))
            {
              BMO_face_flag_enable(bm, f_other, SEL_FLAG);
            }
          }
        }
      }
    }
  }
}

static void bmo_region_extend_contract(BMesh *bm,
                                       BMOperator *op,
                                       const bool use_faces,
                                       const bool use_faces_step)
{
  BMOIter siter;

  if (!use_faces) {
    BMVert *v;

    BMO_ITER (v, &siter, op->slots_in, "geom", BM_VERT) {
      bool found = false;

      if (!use_faces_step) {
        BMIter eiter;
        BMEdge *e;

        BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
          if (!BMO_edge_flag_test(bm, e, SEL_ORIG)) {
            found = true;
            break;
          }
        }
      }
      else {
        BMIter fiter;
        BMFace *f;

        BM_ITER_ELEM (f, &fiter, v, BM_FACES_OF_VERT) {
          if (!BMO_face_flag_test(bm, f, SEL_ORIG)) {
            found = true;
            break;
          }
        }

        /* handle wire edges (when stepping over faces) */
        if (!found) {
          BMIter eiter;
          BMEdge *e;

          BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
            if (BM_edge_is_wire(e)) {
              if (!BMO_edge_flag_test(bm, e, SEL_ORIG)) {
                found = true;
                break;
              }
            }
          }
        }
      }

      if (found) {
        BMIter eiter;
        BMEdge *e;

        BMO_vert_flag_enable(bm, v, SEL_FLAG);

        BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
          BMO_edge_flag_enable(bm, e, SEL_FLAG);
        }
      }
    }
  }
  else {
    BMFace *f;

    BMO_ITER (f, &siter, op->slots_in, "geom", BM_FACE) {
      BMIter liter;
      BMLoop *l;

      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {

        if (!use_faces_step) {
          BMIter fiter;
          BMFace *f_other;

          BM_ITER_ELEM (f_other, &fiter, l->e, BM_FACES_OF_EDGE) {
            if (!BMO_face_flag_test(bm, f_other, SEL_ORIG)) {
              BMO_face_flag_enable(bm, f, SEL_FLAG);
              break;
            }
          }
        }
        else {
          BMIter fiter;
          BMFace *f_other;

          BM_ITER_ELEM (f_other, &fiter, l->v, BM_FACES_OF_VERT) {
            if (!BMO_face_flag_test(bm, f_other, SEL_ORIG)) {
              BMO_face_flag_enable(bm, f, SEL_FLAG);
              break;
            }
          }
        }
      }
    }
  }
}

void bmo_region_extend_exec(BMesh *bm, BMOperator *op)
{
  const bool use_faces = BMO_slot_bool_get(op->slots_in, "use_faces");
  const bool use_face_step = BMO_slot_bool_get(op->slots_in, "use_face_step");
  const bool constrict = BMO_slot_bool_get(op->slots_in, "use_contract");

  BMO_slot_buffer_flag_enable(bm, op->slots_in, "geom", BM_ALL_NOLOOP, SEL_ORIG);

  if (constrict) {
    bmo_region_extend_contract(bm, op, use_faces, use_face_step);
  }
  else {
    bmo_region_extend_expand(bm, op, use_faces, use_face_step);
  }

  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom.out", BM_ALL_NOLOOP, SEL_FLAG);
}

void bmo_smooth_vert_exec(BMesh * /*bm*/, BMOperator *op)
{
  BMOIter siter;
  BMIter iter;
  BMVert *v;
  BMEdge *e;
  float (*cos)[3] = static_cast<float (*)[3]>(
      MEM_mallocN(sizeof(*cos) * BMO_slot_buffer_len(op->slots_in, "verts"), __func__));
  float *co, *co2, clip_dist = BMO_slot_float_get(op->slots_in, "clip_dist");
  const float fac = BMO_slot_float_get(op->slots_in, "factor");
  int i, j, clipx, clipy, clipz;
  int xaxis, yaxis, zaxis;

  clipx = BMO_slot_bool_get(op->slots_in, "mirror_clip_x");
  clipy = BMO_slot_bool_get(op->slots_in, "mirror_clip_y");
  clipz = BMO_slot_bool_get(op->slots_in, "mirror_clip_z");

  xaxis = BMO_slot_bool_get(op->slots_in, "use_axis_x");
  yaxis = BMO_slot_bool_get(op->slots_in, "use_axis_y");
  zaxis = BMO_slot_bool_get(op->slots_in, "use_axis_z");

  i = 0;
  BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {

    co = cos[i];
    zero_v3(co);

    j = 0;
    BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
      co2 = BM_edge_other_vert(e, v)->co;
      add_v3_v3v3(co, co, co2);
      j += 1;
    }

    if (!j) {
      copy_v3_v3(co, v->co);
      i++;
      continue;
    }

    mul_v3_fl(co, 1.0f / float(j));
    interp_v3_v3v3(co, v->co, co, fac);

    if (clipx && fabsf(v->co[0]) <= clip_dist) {
      co[0] = 0.0f;
    }
    if (clipy && fabsf(v->co[1]) <= clip_dist) {
      co[1] = 0.0f;
    }
    if (clipz && fabsf(v->co[2]) <= clip_dist) {
      co[2] = 0.0f;
    }

    i++;
  }

  i = 0;
  BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
    if (xaxis) {
      v->co[0] = cos[i][0];
    }
    if (yaxis) {
      v->co[1] = cos[i][1];
    }
    if (zaxis) {
      v->co[2] = cos[i][2];
    }

    i++;
  }

  MEM_freeN(cos);
}

/**************************************************************************** *
 * Cycle UVs for a face
 **************************************************************************** */

void bmo_rotate_uvs_exec(BMesh *bm, BMOperator *op)
{
  BMOIter fs_iter; /* selected faces iterator */
  BMFace *fs;      /* current face */
  BMIter l_iter;   /* iteration loop */

  const bool use_ccw = BMO_slot_bool_get(op->slots_in, "use_ccw");
  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_PROP_FLOAT2);

  if (cd_loop_uv_offset != -1) {
    BMO_ITER (fs, &fs_iter, op->slots_in, "faces", BM_FACE) {
      if (use_ccw == false) { /* same loops direction */
        BMLoop *lf;           /* current face loops */
        float *f_luv;         /* first face loop uv */
        float p_uv[2];        /* previous uvs */
        float t_uv[2];        /* temp uvs */

        int n = 0;
        BM_ITER_ELEM (lf, &l_iter, fs, BM_LOOPS_OF_FACE) {
          /* current loop uv is the previous loop uv */
          float *luv = BM_ELEM_CD_GET_FLOAT_P(lf, cd_loop_uv_offset);
          if (n == 0) {
            f_luv = luv;
            copy_v2_v2(p_uv, luv);
          }
          else {
            copy_v2_v2(t_uv, luv);
            copy_v2_v2(luv, p_uv);
            copy_v2_v2(p_uv, t_uv);
          }
          n++;
        }

        copy_v2_v2(f_luv, p_uv);
      }
      else {          /* counter loop direction */
        BMLoop *lf;   /* current face loops */
        float *p_luv; /* previous loop uv */
        float *luv;
        float t_uv[2]; /* current uvs */

        int n = 0;
        BM_ITER_ELEM (lf, &l_iter, fs, BM_LOOPS_OF_FACE) {
          /* previous loop uv is the current loop uv */
          luv = BM_ELEM_CD_GET_FLOAT_P(lf, cd_loop_uv_offset);
          if (n == 0) {
            p_luv = luv;
            copy_v2_v2(t_uv, luv);
          }
          else {
            copy_v2_v2(p_luv, luv);
            p_luv = luv;
          }
          n++;
        }

        copy_v2_v2(luv, t_uv);
      }
    }
  }
}

/**************************************************************************** *
 * Reverse UVs for a face
 **************************************************************************** */

static void bm_face_reverse_uvs(BMFace *f, const int cd_loop_uv_offset)
{
  BMIter iter;
  BMLoop *l;
  int i;

  float (*uvs)[2] = BLI_array_alloca(uvs, f->len);

  BM_ITER_ELEM_INDEX (l, &iter, f, BM_LOOPS_OF_FACE, i) {
    float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
    copy_v2_v2(uvs[i], luv);
  }

  /* now that we have the uvs in the array, reverse! */
  BM_ITER_ELEM_INDEX (l, &iter, f, BM_LOOPS_OF_FACE, i) {
    /* current loop uv is the previous loop uv */
    float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
    copy_v2_v2(luv, uvs[(f->len - i - 1)]);
  }
}
void bmo_reverse_uvs_exec(BMesh *bm, BMOperator *op)
{
  BMOIter iter;
  BMFace *f;
  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_PROP_FLOAT2);

  if (cd_loop_uv_offset != -1) {
    BMO_ITER (f, &iter, op->slots_in, "faces", BM_FACE) {
      bm_face_reverse_uvs(f, cd_loop_uv_offset);
    }
  }
}

/**************************************************************************** *
 * Cycle colors for a face
 **************************************************************************** */

static void bmo_get_loop_color_ref(BMesh *bm,
                                   int index,
                                   int *r_cd_color_offset,
                                   std::optional<eCustomDataType> *r_cd_color_type)
{
  int color_index = 0;
  for (const CustomDataLayer &layer : blender::Span(bm->ldata.layers, bm->ldata.totlayer)) {
    if (CD_TYPE_AS_MASK(eCustomDataType(layer.type)) & CD_MASK_COLOR_ALL) {
      if (color_index == index) {
        *r_cd_color_offset = layer.offset;
        *r_cd_color_type = eCustomDataType(layer.type);
        return;
      }
      color_index++;
    }
  }
  *r_cd_color_offset = -1;
}

void bmo_rotate_colors_exec(BMesh *bm, BMOperator *op)
{
  BMOIter fs_iter; /* selected faces iterator */
  BMFace *fs;      /* current face */
  BMIter l_iter;   /* iteration loop */

  const bool use_ccw = BMO_slot_bool_get(op->slots_in, "use_ccw");

  const int color_index = BMO_slot_int_get(op->slots_in, "color_index");

  int cd_loop_color_offset;
  std::optional<eCustomDataType> cd_loop_color_type;
  bmo_get_loop_color_ref(bm, color_index, &cd_loop_color_offset, &cd_loop_color_type);

  if (cd_loop_color_offset == -1) {
    BMO_error_raise(bm, op, BMO_ERROR_CANCEL, "color_index is invalid");
    return;
  }

  const size_t size = cd_loop_color_type == CD_PROP_COLOR ? sizeof(MPropCol) : sizeof(MLoopCol);
  void *p_col;                /* previous color */
  void *t_col = alloca(size); /* Temp color. */

  BMO_ITER (fs, &fs_iter, op->slots_in, "faces", BM_FACE) {
    if (use_ccw == false) { /* same loops direction */
      BMLoop *lf;           /* current face loops */
      void *f_lcol;         /* first face loop color */

      int n = 0;
      BM_ITER_ELEM (lf, &l_iter, fs, BM_LOOPS_OF_FACE) {
        /* current loop color is the previous loop color */
        void *lcol = BM_ELEM_CD_GET_VOID_P(lf, cd_loop_color_offset);

        if (n == 0) {
          f_lcol = lcol;
          p_col = lcol;
        }
        else {
          memcpy(t_col, lcol, size);
          memcpy(lcol, p_col, size);
          memcpy(p_col, t_col, size);
        }
        n++;
      }

      memcpy(f_lcol, p_col, size);
    }
    else {        /* counter loop direction */
      BMLoop *lf; /* current face loops */
      void *lcol, *p_lcol;

      int n = 0;
      BM_ITER_ELEM (lf, &l_iter, fs, BM_LOOPS_OF_FACE) {
        /* previous loop color is the current loop color */
        lcol = BM_ELEM_CD_GET_VOID_P(lf, cd_loop_color_offset);
        if (n == 0) {
          p_lcol = lcol;
          memcpy(t_col, lcol, size);
        }
        else {
          memcpy(p_lcol, lcol, size);
          p_lcol = lcol;
        }
        n++;
      }

      memcpy(lcol, t_col, size);
    }
  }
}

/*************************************************************************** *
 * Reverse colors for a face
 *************************************************************************** */
static void bm_face_reverse_colors(BMFace *f,
                                   const int cd_loop_color_offset,
                                   const eCustomDataType cd_loop_color_type)
{
  BMIter iter;
  BMLoop *l;
  int i;

  const size_t size = cd_loop_color_type == CD_PROP_COLOR ? sizeof(MPropCol) : sizeof(MLoopCol);

  char *cols = static_cast<char *>(alloca(size * f->len));

  char *col = cols;
  BM_ITER_ELEM_INDEX (l, &iter, f, BM_LOOPS_OF_FACE, i) {
    void *lcol = BM_ELEM_CD_GET_VOID_P(l, cd_loop_color_offset);
    memcpy((void *)col, lcol, size);
    col += size;
  }

  /* now that we have the uvs in the array, reverse! */
  BM_ITER_ELEM_INDEX (l, &iter, f, BM_LOOPS_OF_FACE, i) {
    /* current loop uv is the previous loop color */
    void *lcol = BM_ELEM_CD_GET_VOID_P(l, cd_loop_color_offset);

    col = cols + (f->len - i - 1) * size;
    memcpy(lcol, (void *)col, size);
  }
}

void bmo_reverse_colors_exec(BMesh *bm, BMOperator *op)
{
  BMOIter iter;
  BMFace *f;

  const int color_index = BMO_slot_int_get(op->slots_in, "color_index");

  int cd_loop_color_offset;
  std::optional<eCustomDataType> cd_loop_color_type;
  bmo_get_loop_color_ref(bm, color_index, &cd_loop_color_offset, &cd_loop_color_type);

  if (cd_loop_color_offset == -1) {
    BMO_error_raise(bm, op, BMO_ERROR_CANCEL, "color_index is invalid");
    return;
  }

  BMO_ITER (f, &iter, op->slots_in, "faces", BM_FACE) {
    bm_face_reverse_colors(f, cd_loop_color_offset, *cd_loop_color_type);
  }
}
