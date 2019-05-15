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
 * \ingroup spview3d
 */

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"

#include "BKE_editmesh.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "GPU_draw.h"
#include "GPU_shader.h"
#include "GPU_immediate.h"
#include "GPU_batch.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_mesh.h"

#include "UI_resources.h"

#include "DRW_engine.h"

#include "view3d_intern.h" /* bad level include */

int view3d_effective_drawtype(const struct View3D *v3d)
{
  if (v3d->shading.type == OB_RENDER) {
    return v3d->shading.prev_type;
  }
  return v3d->shading.type;
}

/* OpenGL Circle Drawing - Tables for Optimized Drawing Speed */
/* 32 values of sin function (still same result!) */
#define CIRCLE_RESOL 32

static const float sinval[CIRCLE_RESOL] = {
    0.00000000,  0.20129852,  0.39435585,  0.57126821,  0.72479278,  0.84864425,  0.93775213,
    0.98846832,  0.99871650,  0.96807711,  0.89780453,  0.79077573,  0.65137248,  0.48530196,
    0.29936312,  0.10116832,  -0.10116832, -0.29936312, -0.48530196, -0.65137248, -0.79077573,
    -0.89780453, -0.96807711, -0.99871650, -0.98846832, -0.93775213, -0.84864425, -0.72479278,
    -0.57126821, -0.39435585, -0.20129852, 0.00000000,
};

/* 32 values of cos function (still same result!) */
static const float cosval[CIRCLE_RESOL] = {
    1.00000000,  0.97952994,  0.91895781,  0.82076344,  0.68896691,  0.52896401,  0.34730525,
    0.15142777,  -0.05064916, -0.25065253, -0.44039415, -0.61210598, -0.75875812, -0.87434661,
    -0.95413925, -0.99486932, -0.99486932, -0.95413925, -0.87434661, -0.75875812, -0.61210598,
    -0.44039415, -0.25065253, -0.05064916, 0.15142777,  0.34730525,  0.52896401,  0.68896691,
    0.82076344,  0.91895781,  0.97952994,  1.00000000,
};

static void circball_array_fill(float verts[CIRCLE_RESOL][3],
                                const float cent[3],
                                float rad,
                                const float tmat[4][4])
{
  float vx[3], vy[3];
  float *viter = (float *)verts;

  mul_v3_v3fl(vx, tmat[0], rad);
  mul_v3_v3fl(vy, tmat[1], rad);

  for (uint a = 0; a < CIRCLE_RESOL; a++, viter += 3) {
    viter[0] = cent[0] + sinval[a] * vx[0] + cosval[a] * vy[0];
    viter[1] = cent[1] + sinval[a] * vx[1] + cosval[a] * vy[1];
    viter[2] = cent[2] + sinval[a] * vx[2] + cosval[a] * vy[2];
  }
}

void imm_drawcircball(const float cent[3], float rad, const float tmat[4][4], unsigned pos)
{
  float verts[CIRCLE_RESOL][3];

  circball_array_fill(verts, cent, rad, tmat);

  immBegin(GPU_PRIM_LINE_LOOP, CIRCLE_RESOL);
  for (int i = 0; i < CIRCLE_RESOL; ++i) {
    immVertex3fv(pos, verts[i]);
  }
  immEnd();
}

#ifdef VIEW3D_CAMERA_BORDER_HACK
uchar view3d_camera_border_hack_col[3];
bool view3d_camera_border_hack_test = false;
#endif

/* ***************** BACKBUF SEL (BBS) ********* */

void ED_draw_object_facemap(Depsgraph *depsgraph,
                            Object *ob,
                            const float col[4],
                            const int facemap)
{
  /* happens on undo */
  if (ob->type != OB_MESH || !ob->data) {
    return;
  }

  Mesh *me = ob->data;
  {
    Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
    if (ob_eval->runtime.mesh_eval) {
      me = ob_eval->runtime.mesh_eval;
    }
  }

  glFrontFace((ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW);

  /* Just to create the data to pass to immediate mode, grr! */
  const int *facemap_data = CustomData_get_layer(&me->pdata, CD_FACEMAP);
  if (facemap_data) {
    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    GPU_blend(true);

    const MVert *mvert = me->mvert;
    const MPoly *mpoly = me->mpoly;
    const MLoop *mloop = me->mloop;

    int mpoly_len = me->totpoly;
    int mloop_len = me->totloop;

    facemap_data = CustomData_get_layer(&me->pdata, CD_FACEMAP);

    /* use gawain immediate mode fore now */
    const int looptris_len = poly_to_tri_count(mpoly_len, mloop_len);
    const int vbo_len_capacity = looptris_len * 3;
    int vbo_len_used = 0;

    GPUVertFormat format_pos = {0};
    const uint pos_id = GPU_vertformat_attr_add(
        &format_pos, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

    GPUVertBuf *vbo_pos = GPU_vertbuf_create_with_format(&format_pos);
    GPU_vertbuf_data_alloc(vbo_pos, vbo_len_capacity);

    GPUVertBufRaw pos_step;
    GPU_vertbuf_attr_get_raw_data(vbo_pos, pos_id, &pos_step);

    const MPoly *mp;
    int i;
    if (me->runtime.looptris.array) {
      MLoopTri *mlt = me->runtime.looptris.array;
      for (mp = mpoly, i = 0; i < mpoly_len; i++, mp++) {
        if (facemap_data[i] == facemap) {
          for (int j = 2; j < mp->totloop; j++) {
            copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), mvert[mloop[mlt->tri[0]].v].co);
            copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), mvert[mloop[mlt->tri[1]].v].co);
            copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), mvert[mloop[mlt->tri[2]].v].co);
            vbo_len_used += 3;
            mlt++;
          }
        }
        else {
          mlt += mp->totloop - 2;
        }
      }
    }
    else {
      /* No tessellation data, fan-fill. */
      for (mp = mpoly, i = 0; i < mpoly_len; i++, mp++) {
        if (facemap_data[i] == facemap) {
          const MLoop *ml_start = &mloop[mp->loopstart];
          const MLoop *ml_a = ml_start + 1;
          const MLoop *ml_b = ml_start + 2;
          for (int j = 2; j < mp->totloop; j++) {
            copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), mvert[ml_start->v].co);
            copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), mvert[ml_a->v].co);
            copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), mvert[ml_b->v].co);
            vbo_len_used += 3;

            ml_a++;
            ml_b++;
          }
        }
      }
    }

    if (vbo_len_capacity != vbo_len_used) {
      GPU_vertbuf_data_resize(vbo_pos, vbo_len_used);
    }

    GPUBatch *draw_batch = GPU_batch_create(GPU_PRIM_TRIS, vbo_pos, NULL);
    GPU_batch_program_set_builtin(draw_batch, GPU_SHADER_3D_UNIFORM_COLOR);
    GPU_batch_uniform_4fv(draw_batch, "color", col);
    GPU_batch_draw(draw_batch);
    GPU_batch_discard(draw_batch);
    GPU_vertbuf_discard(vbo_pos);

    GPU_blend(false);
  }
}
