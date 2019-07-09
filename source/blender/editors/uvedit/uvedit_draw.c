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
 * \ingroup eduv
 */

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "../../draw/intern/draw_cache_impl.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_material.h"
#include "BKE_layer.h"

#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "GPU_batch.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"
#include "GPU_draw.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "UI_resources.h"
#include "UI_interface.h"
#include "UI_view2d.h"

#include "uvedit_intern.h"

static int draw_uvs_face_check(const ToolSettings *ts)
{
  /* checks if we are selecting only faces */
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode == SCE_SELECT_FACE) {
      return 2;
    }
    else if (ts->selectmode & SCE_SELECT_FACE) {
      return 1;
    }
    else {
      return 0;
    }
  }
  else {
    return (ts->uv_selectmode == UV_SELECT_FACE);
  }
}

/* ------------------------- */

void ED_image_draw_cursor(ARegion *ar, const float cursor[2])
{
  float zoom[2], x_fac, y_fac;

  UI_view2d_scale_get_inverse(&ar->v2d, &zoom[0], &zoom[1]);

  mul_v2_fl(zoom, 256.0f * UI_DPI_FAC);
  x_fac = zoom[0];
  y_fac = zoom[1];

  GPU_line_width(1.0f);

  GPU_matrix_translate_2fv(cursor);

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

  immUniform1i("colors_len", 2); /* "advanced" mode */
  immUniformArray4fv(
      "colors", (float *)(float[][4]){{1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, 2);
  immUniform1f("dash_width", 8.0f);
  immUniform1f("dash_factor", 0.5f);

  immBegin(GPU_PRIM_LINES, 8);

  immVertex2f(shdr_pos, -0.05f * x_fac, 0.0f);
  immVertex2f(shdr_pos, 0.0f, 0.05f * y_fac);

  immVertex2f(shdr_pos, 0.0f, 0.05f * y_fac);
  immVertex2f(shdr_pos, 0.05f * x_fac, 0.0f);

  immVertex2f(shdr_pos, 0.05f * x_fac, 0.0f);
  immVertex2f(shdr_pos, 0.0f, -0.05f * y_fac);

  immVertex2f(shdr_pos, 0.0f, -0.05f * y_fac);
  immVertex2f(shdr_pos, -0.05f * x_fac, 0.0f);

  immEnd();

  immUniformArray4fv(
      "colors", (float *)(float[][4]){{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, 2);
  immUniform1f("dash_width", 2.0f);
  immUniform1f("dash_factor", 0.5f);

  immBegin(GPU_PRIM_LINES, 8);

  immVertex2f(shdr_pos, -0.020f * x_fac, 0.0f);
  immVertex2f(shdr_pos, -0.1f * x_fac, 0.0f);

  immVertex2f(shdr_pos, 0.1f * x_fac, 0.0f);
  immVertex2f(shdr_pos, 0.020f * x_fac, 0.0f);

  immVertex2f(shdr_pos, 0.0f, -0.020f * y_fac);
  immVertex2f(shdr_pos, 0.0f, -0.1f * y_fac);

  immVertex2f(shdr_pos, 0.0f, 0.1f * y_fac);
  immVertex2f(shdr_pos, 0.0f, 0.020f * y_fac);

  immEnd();

  immUnbindProgram();

  GPU_matrix_translate_2f(-cursor[0], -cursor[1]);
}

static void uvedit_get_batches(Object *ob,
                               SpaceImage *sima,
                               const ToolSettings *ts,
                               GPUBatch **faces,
                               GPUBatch **edges,
                               GPUBatch **verts,
                               GPUBatch **facedots)
{
  int drawfaces = draw_uvs_face_check(ts);
  const bool draw_stretch = (sima->flag & SI_DRAW_STRETCH) != 0;
  const bool draw_faces = (sima->flag & SI_NO_DRAWFACES) == 0;

  DRW_mesh_batch_cache_validate(ob->data);
  *edges = DRW_mesh_batch_cache_get_edituv_edges(ob->data);
  *verts = DRW_mesh_batch_cache_get_edituv_verts(ob->data);

  if (drawfaces) {
    *facedots = DRW_mesh_batch_cache_get_edituv_facedots(ob->data);
  }
  else {
    *facedots = NULL;
  }

  if (draw_stretch && (sima->dt_uvstretch == SI_UVDT_STRETCH_AREA)) {
    *faces = DRW_mesh_batch_cache_get_edituv_faces_strech_area(ob->data);
  }
  else if (draw_stretch) {
    *faces = DRW_mesh_batch_cache_get_edituv_faces_strech_angle(ob->data);
  }
  else if (draw_faces) {
    *faces = DRW_mesh_batch_cache_get_edituv_faces(ob->data);
  }
  else {
    *faces = NULL;
  }

  DRW_mesh_batch_cache_create_requested(ob, ob->data, ts, false, false);
}

static void draw_uvs_shadow(SpaceImage *UNUSED(sima),
                            Scene *scene,
                            Object *obedit,
                            Depsgraph *depsgraph)
{
  Object *eval_ob = DEG_get_evaluated_object(depsgraph, obedit);
  Mesh *me = eval_ob->data;
  float col[4];
  UI_GetThemeColor4fv(TH_UV_SHADOW, col);

  DRW_mesh_batch_cache_validate(me);
  GPUBatch *edges = DRW_mesh_batch_cache_get_uv_edges(me);
  DRW_mesh_batch_cache_create_requested(eval_ob, me, scene->toolsettings, false, false);

  if (edges) {
    GPU_batch_program_set_builtin(edges, GPU_SHADER_2D_UV_UNIFORM_COLOR);
    GPU_batch_uniform_4fv(edges, "color", col);
    GPU_batch_draw(edges);
  }
}

static void draw_uvs_texpaint(Scene *scene, Object *ob, Depsgraph *depsgraph)
{
  Object *eval_ob = DEG_get_evaluated_object(depsgraph, ob);
  Mesh *me = eval_ob->data;
  ToolSettings *ts = scene->toolsettings;
  float col[4];
  UI_GetThemeColor4fv(TH_UV_SHADOW, col);

  if (me->mloopuv == NULL) {
    return;
  }

  DRW_mesh_batch_cache_validate(me);
  GPUBatch *geom = DRW_mesh_batch_cache_get_uv_edges(me);
  DRW_mesh_batch_cache_create_requested(eval_ob, me, scene->toolsettings, false, false);

  GPU_batch_program_set_builtin(geom, GPU_SHADER_2D_UV_UNIFORM_COLOR);
  GPU_batch_uniform_4fv(geom, "color", col);

  const bool do_material_masking = (ts->uv_flag & UV_SHOW_SAME_IMAGE);
  if (do_material_masking && me->mloopuv) {
    /* Render loops that have the active material. Minize draw calls. */
    MPoly *mpoly = me->mpoly;
    uint draw_start = 0;
    uint idx = 0;
    bool prev_ma_match = (mpoly->mat_nr == (eval_ob->actcol - 1));

    GPU_matrix_bind(geom->interface);
    GPU_batch_bind(geom);

    /* TODO(fclem): If drawcall count becomes a problem in the future
     * we can use multi draw indirect drawcalls for this.
     * (not implemented in GPU module at the time of writing). */
    for (int a = 0; a < me->totpoly; a++, mpoly++) {
      bool ma_match = (mpoly->mat_nr == (eval_ob->actcol - 1));
      if (ma_match != prev_ma_match) {
        if (ma_match == false) {
          GPU_batch_draw_advanced(geom, draw_start, idx - draw_start, 0, 0);
        }
        else {
          draw_start = idx;
        }
      }
      idx += mpoly->totloop + 1;
      prev_ma_match = ma_match;
    }
    if (prev_ma_match == true) {
      GPU_batch_draw_advanced(geom, draw_start, idx - draw_start, 0, 0);
    }

    GPU_batch_program_use_end(geom);
  }
  else {
    GPU_batch_draw(geom);
  }
}

/* draws uv's in the image space */
static void draw_uvs(SpaceImage *sima, Scene *scene, Object *obedit, Depsgraph *depsgraph)
{
  GPUBatch *faces, *edges, *verts, *facedots;
  Object *eval_ob = DEG_get_evaluated_object(depsgraph, obedit);
  const ToolSettings *ts = scene->toolsettings;
  float col1[4], col2[4], col3[4], transparent[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  if (sima->flag & SI_DRAWSHADOW) {
    bool is_cage_like_final_meshes = false;
    Mesh *me = (Mesh *)eval_ob->data;
    BMEditMesh *embm = me->edit_mesh;
    is_cage_like_final_meshes = embm && embm->mesh_eval_final &&
                                embm->mesh_eval_final->runtime.is_original;

    /* When sync selection is enabled, all faces are drawn (except for hidden)
     * so if cage is the same as the final, there is no point in drawing this. */
    if (!((ts->uv_flag & UV_SYNC_SELECTION) && is_cage_like_final_meshes)) {
      draw_uvs_shadow(sima, scene, obedit, depsgraph);
    }
  }

  uvedit_get_batches(eval_ob, sima, ts, &faces, &edges, &verts, &facedots);

  bool interpedges;
  bool draw_stretch = (sima->flag & SI_DRAW_STRETCH) != 0;
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    interpedges = (ts->selectmode & SCE_SELECT_VERTEX) != 0;
  }
  else {
    interpedges = (ts->uv_selectmode == UV_SELECT_VERTEX);
  }

  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  if (faces) {
    GPU_batch_program_set_builtin(faces,
                                  (draw_stretch) ? (sima->dt_uvstretch == SI_UVDT_STRETCH_AREA) ?
                                                   GPU_SHADER_2D_UV_FACES_STRETCH_AREA :
                                                   GPU_SHADER_2D_UV_FACES_STRETCH_ANGLE :
                                                   GPU_SHADER_2D_UV_FACES);

    if (!draw_stretch) {
      GPU_blend(true);

      UI_GetThemeColor4fv(TH_FACE, col1);
      UI_GetThemeColor4fv(TH_FACE_SELECT, col2);
      UI_GetThemeColor4fv(TH_EDITMESH_ACTIVE, col3);
      col3[3] *= 0.2; /* Simulate dithering */
      GPU_batch_uniform_4fv(faces, "faceColor", col1);
      GPU_batch_uniform_4fv(faces, "selectColor", col2);
      GPU_batch_uniform_4fv(faces, "activeColor", col3);
    }
    else if (sima->dt_uvstretch == SI_UVDT_STRETCH_ANGLE) {
      float asp[2];
      ED_space_image_get_uv_aspect(sima, &asp[0], &asp[1]);
      GPU_batch_uniform_2fv(faces, "aspect", asp);
    }

    GPU_batch_draw(faces);

    if (!draw_stretch) {
      GPU_blend(false);
    }
  }
  if (edges) {
    if (sima->flag & SI_SMOOTH_UV) {
      GPU_line_smooth(true);
      GPU_blend(true);
    }
    switch (sima->dt_uv) {
      case SI_UVDT_DASH: {
        float dash_colors[2][4] = {{0.56f, 0.56f, 0.56f, 1.0f}, {0.07f, 0.07f, 0.07f, 1.0f}};
        float viewport_size[4];
        GPU_viewport_size_get_f(viewport_size);

        GPU_line_width(1.0f);
        GPU_batch_program_set_builtin(edges, GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);
        GPU_batch_uniform_4fv_array(edges, "colors", 2, (float *)dash_colors);
        GPU_batch_uniform_2f(
            edges, "viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);
        GPU_batch_uniform_1i(edges, "colors_len", 2); /* "advanced" mode */
        GPU_batch_uniform_1f(edges, "dash_width", 4.0f);
        GPU_batch_uniform_1f(edges, "dash_factor", 0.5f);
        GPU_batch_draw(edges);
        break;
      }
      case SI_UVDT_BLACK:
      case SI_UVDT_WHITE: {
        GPU_line_width(1.0f);
        GPU_batch_program_set_builtin(edges, GPU_SHADER_2D_UNIFORM_COLOR);
        if (sima->dt_uv == SI_UVDT_WHITE) {
          GPU_batch_uniform_4f(edges, "color", 1.0f, 1.0f, 1.0f, 1.0f);
        }
        else {
          GPU_batch_uniform_4f(edges, "color", 0.0f, 0.0f, 0.0f, 1.0f);
        }
        GPU_batch_draw(edges);
        break;
      }
      case SI_UVDT_OUTLINE: {
        /* We could modify the vbo's data filling
         * instead of modifying the provoking vert. */
        glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);

        UI_GetThemeColor4fv(TH_WIRE_EDIT, col1);
        UI_GetThemeColor4fv(TH_EDGE_SELECT, col2);

        GPU_batch_program_set_builtin(
            edges, (interpedges) ? GPU_SHADER_2D_UV_EDGES_SMOOTH : GPU_SHADER_2D_UV_EDGES);
        /* Black Outline. */
        GPU_line_width(3.0f);
        GPU_batch_uniform_4f(edges, "edgeColor", 0.0f, 0.0f, 0.0f, 1.0f);
        GPU_batch_uniform_4f(edges, "selectColor", 0.0f, 0.0f, 0.0f, 1.0f);
        GPU_batch_draw(edges);
        /* Inner Line. Use depth test to insure selection is drawn on top. */
        GPU_depth_test(true);
        GPU_line_width(1.0f);
        GPU_batch_uniform_4fv(edges, "edgeColor", col1);
        GPU_batch_uniform_4fv(edges, "selectColor", col2);
        GPU_batch_draw(edges);
        GPU_depth_test(false);

        glProvokingVertex(GL_LAST_VERTEX_CONVENTION);
        break;
      }
    }
    if (sima->flag & SI_SMOOTH_UV) {
      GPU_line_smooth(false);
      GPU_blend(false);
    }
  }
  if (verts || facedots) {
    UI_GetThemeColor4fv(TH_VERTEX_SELECT, col2);
    if (verts) {
      const float point_size = UI_GetThemeValuef(TH_VERTEX_SIZE);
      const float pinned_col[4] = {1.0f, 0.0f, 0.0f, 1.0f}; /* TODO Theme? */
      UI_GetThemeColor4fv(TH_VERTEX, col1);
      GPU_blend(true);
      GPU_program_point_size(true);

      GPU_batch_program_set_builtin(verts, GPU_SHADER_2D_UV_VERTS);
      GPU_batch_uniform_4f(verts, "vertColor", col1[0], col1[1], col1[2], 1.0f);
      GPU_batch_uniform_4fv(verts, "selectColor", transparent);
      GPU_batch_uniform_4fv(verts, "pinnedColor", pinned_col);
      GPU_batch_uniform_1f(verts, "pointSize", (point_size + 1.5f) * M_SQRT2);
      GPU_batch_uniform_1f(verts, "outlineWidth", 0.75f);
      GPU_batch_draw(verts);

      /* We have problem in this mode when face order make some verts
       * appear unselected because an adjacent face is not selected and
       * render after the selected face.
       * So, to avoid sorting verts by state we just render selected verts
       * on top. A bit overkill but it's simple. */
      GPU_batch_uniform_4fv(verts, "vertColor", transparent);
      GPU_batch_uniform_4fv(verts, "selectColor", col2);
      GPU_batch_draw(verts);

      GPU_blend(false);
      GPU_program_point_size(false);
    }
    if (facedots) {
      const float point_size = UI_GetThemeValuef(TH_FACEDOT_SIZE);
      GPU_point_size(point_size);

      UI_GetThemeColor4fv(TH_WIRE, col1);
      GPU_batch_program_set_builtin(facedots, GPU_SHADER_2D_UV_FACEDOTS);
      GPU_batch_uniform_4fv(facedots, "vertColor", col1);
      GPU_batch_uniform_4fv(facedots, "selectColor", col2);
      GPU_batch_draw(facedots);
    }
  }
}

static void draw_uv_shadows_get(
    SpaceImage *sima, Object *ob, Object *obedit, bool *show_shadow, bool *show_texpaint)
{
  *show_shadow = *show_texpaint = false;

  if (ED_space_image_show_render(sima) || (sima->flag & SI_NO_DRAW_TEXPAINT)) {
    return;
  }

  if ((sima->mode == SI_MODE_PAINT) && obedit && obedit->type == OB_MESH) {
    struct BMEditMesh *em = BKE_editmesh_from_object(obedit);

    *show_shadow = EDBM_uv_check(em);
  }

  *show_texpaint = (ob && ob->type == OB_MESH && ob->mode == OB_MODE_TEXTURE_PAINT);
}

void ED_uvedit_draw_main(SpaceImage *sima,
                         Scene *scene,
                         ViewLayer *view_layer,
                         Object *obedit,
                         Object *obact,
                         Depsgraph *depsgraph)
{
  bool show_uvedit, show_uvshadow, show_texpaint_uvshadow;

  show_uvedit = ED_space_image_show_uvedit(sima, obedit);
  draw_uv_shadows_get(sima, obact, obedit, &show_uvshadow, &show_texpaint_uvshadow);

  if (show_uvedit || show_uvshadow || show_texpaint_uvshadow) {
    if (show_uvshadow) {
      draw_uvs_shadow(sima, scene, obedit, depsgraph);
    }
    else if (show_uvedit) {
      uint objects_len = 0;
      Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
          view_layer, ((View3D *)NULL), &objects_len);
      if (objects_len > 0) {
        GPU_clear_depth(1.0f);
        GPU_clear(GPU_DEPTH_BIT);
      }
      for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
        Object *ob_iter = objects[ob_index];
        draw_uvs(sima, scene, ob_iter, depsgraph);
      }
      MEM_freeN(objects);
    }
    else {
      draw_uvs_texpaint(scene, obact, depsgraph);
    }
  }
}
