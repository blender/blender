/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgizmolib
 */

#include "GPU_batch.h"
#include "GPU_immediate.h"

#include "MEM_guardedalloc.h"

#include "WM_types.hh"

/* only for own init/exit calls (wm_gizmotype_init/wm_gizmotype_free) */

/* own includes */
#include "gizmo_library_intern.h"

void wm_gizmo_geometryinfo_draw(const GizmoGeomInfo *info,
                                const bool /*select*/,
                                const float color[4])
{
  /* TODO: store the Batches inside the GizmoGeomInfo and updated it when geom changes
   * So we don't need to re-created and discard it every time */

  GPUVertBuf *vbo;
  GPUIndexBuf *el;
  GPUBatch *batch;
  GPUIndexBufBuilder elb = {0};

  GPUVertFormat format = {0};
  uint pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  /* Elements */
  GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, info->ntris, info->nverts);
  for (int i = 0; i < info->ntris; i++) {
    const ushort *idx = &info->indices[i * 3];
    GPU_indexbuf_add_tri_verts(&elb, idx[0], idx[1], idx[2]);
  }
  el = GPU_indexbuf_build(&elb);

  vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, info->nverts);

  GPU_vertbuf_attr_fill(vbo, pos_id, info->verts);

  batch = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, el, GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_UNIFORM_COLOR);

  GPU_batch_uniform_4fv(batch, "color", color);

/* We may want to re-visit this, for now disable
 * since it causes issues leaving the GL state modified. */
#if 0
  GPU_face_culling(GPU_CULL_BACK);
  GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
#endif

  GPU_batch_draw(batch);

#if 0
  GPU_depth_test(GPU_DEPTH_NONE);
  GPU_face_culling(GPU_CULL_NONE);
#endif

  GPU_batch_discard(batch);
}

void wm_gizmo_vec_draw(
    const float color[4], const float (*verts)[3], uint vert_count, uint pos, uint primitive_type)
{
  immUniformColor4fv(color);

  if (primitive_type == GPU_PRIM_LINE_LOOP) {
    /* Line loop alternative for Metal/Vulkan. */
    immBegin(GPU_PRIM_LINES, vert_count * 2);
    immVertex3fv(pos, verts[0]);
    for (int i = 1; i < vert_count; i++) {
      immVertex3fv(pos, verts[i]);
      immVertex3fv(pos, verts[i]);
    }
    immVertex3fv(pos, verts[0]);
    immEnd();
  }
  else if (primitive_type == GPU_PRIM_TRI_FAN) {
    /* NOTE(Metal): Tri-fan alternative for Metal. Triangle List is more efficient for small
     * primitive counts. */
    int tri_count = vert_count - 2;
    immBegin(GPU_PRIM_TRIS, tri_count * 3);
    for (int i = 0; i < tri_count; i++) {
      immVertex3fv(pos, verts[0]);
      immVertex3fv(pos, verts[i + 1]);
      immVertex3fv(pos, verts[i + 2]);
    }
    immEnd();
  }
  else {
    immBegin(GPUPrimType(primitive_type), vert_count);
    for (int i = 0; i < vert_count; i++) {
      immVertex3fv(pos, verts[i]);
    }
    immEnd();
  }
}
