/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup spview3d
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_vector.h"

#include "BKE_DerivedMesh.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "GPU_batch.h"
#include "GPU_immediate.h"
#include "GPU_shader.h"
#include "GPU_state.h"

#include "ED_mesh.h"

#include "UI_resources.h"

#include "DRW_engine.h"

#include "view3d_intern.h" /* bad level include */

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

  const Mesh *me = ob->data;
  {
    Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
    const Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);
    if (me_eval != NULL) {
      me = me_eval;
    }
  }

  GPU_front_facing(ob->transflag & OB_NEG_SCALE);

  /* Just to create the data to pass to immediate mode! (sigh) */
  const int *facemap_data = CustomData_get_layer(&me->pdata, CD_FACEMAP);
  if (facemap_data) {
    GPU_blend(GPU_BLEND_ALPHA);

    const float(*positions)[3] = BKE_mesh_vert_positions(me);
    const MPoly *polys = BKE_mesh_polys(me);
    const MLoop *loops = BKE_mesh_loops(me);

    int mpoly_len = me->totpoly;
    int mloop_len = me->totloop;

    facemap_data = CustomData_get_layer(&me->pdata, CD_FACEMAP);

    /* Make a batch and free it each time for now. */
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
    if (BKE_mesh_runtime_looptri_ensure(me)) {
      const MLoopTri *mlt = BKE_mesh_runtime_looptri_ensure(me);
      for (mp = polys, i = 0; i < mpoly_len; i++, mp++) {
        if (facemap_data[i] == facemap) {
          for (int j = 2; j < mp->totloop; j++) {
            copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), positions[loops[mlt->tri[0]].v]);
            copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), positions[loops[mlt->tri[1]].v]);
            copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), positions[loops[mlt->tri[2]].v]);
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
      for (mp = polys, i = 0; i < mpoly_len; i++, mp++) {
        if (facemap_data[i] == facemap) {
          const MLoop *ml_start = &loops[mp->loopstart];
          const MLoop *ml_a = ml_start + 1;
          const MLoop *ml_b = ml_start + 2;
          for (int j = 2; j < mp->totloop; j++) {
            copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), positions[ml_start->v]);
            copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), positions[ml_a->v]);
            copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), positions[ml_b->v]);
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

    GPU_blend(GPU_BLEND_NONE);
  }
}
