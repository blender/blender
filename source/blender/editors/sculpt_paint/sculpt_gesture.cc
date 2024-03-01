/** \file
 * \ingroup edsculpt
 * Common helper methods and structures for gesture operations.
 */
#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"

#include "BLI_bit_vector.hh"
#include "BLI_bitmap_draw_2d.h"
#include "BLI_lasso_2d.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

#include "BKE_context.hh"
#include "BKE_paint.hh"

#include "ED_view3d.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

namespace blender::ed::sculpt_paint::gesture {

void operator_properties(wmOperatorType *ot)
{
  RNA_def_boolean(ot->srna,
                  "use_front_faces_only",
                  false,
                  "Front Faces Only",
                  "Affect only faces facing towards the view");

  RNA_def_boolean(ot->srna,
                  "use_limit_to_segment",
                  false,
                  "Limit to Segment",
                  "Apply the gesture action only to the area that is contained within the "
                  "segment without extending its effect to the entire line");
}

static void init_common(bContext *C, wmOperator *op, GestureData *gesture_data)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  gesture_data->vc = ED_view3d_viewcontext_init(C, depsgraph);
  Object *ob = gesture_data->vc.obact;

  /* Operator properties. */
  gesture_data->front_faces_only = RNA_boolean_get(op->ptr, "use_front_faces_only");
  gesture_data->line.use_side_planes = RNA_boolean_get(op->ptr, "use_limit_to_segment");

  /* SculptSession */
  gesture_data->ss = ob->sculpt;

  /* Symmetry. */
  gesture_data->symm = ePaintSymmetryFlags(SCULPT_mesh_symmetry_xyz_get(ob));

  /* View Normal. */
  float mat[3][3];
  float view_dir[3] = {0.0f, 0.0f, 1.0f};
  copy_m3_m4(mat, gesture_data->vc.rv3d->viewinv);
  mul_m3_v3(mat, view_dir);
  normalize_v3_v3(gesture_data->world_space_view_normal, view_dir);
  copy_m3_m4(mat, ob->world_to_object().ptr());
  mul_m3_v3(mat, view_dir);
  normalize_v3_v3(gesture_data->true_view_normal, view_dir);

  /* View Origin. */
  copy_v3_v3(gesture_data->world_space_view_origin, gesture_data->vc.rv3d->viewinv[3]);
  copy_v3_v3(gesture_data->true_view_origin, gesture_data->vc.rv3d->viewinv[3]);
}

static void lasso_px_cb(int x, int x_end, int y, void *user_data)
{
  GestureData *gesture_data = static_cast<GestureData *>(user_data);
  LassoData *lasso = &gesture_data->lasso;
  int index = (y * lasso->width) + x;
  int index_end = (y * lasso->width) + x_end;
  do {
    lasso->mask_px[index].set();
  } while (++index != index_end);
}

GestureData *init_from_lasso(bContext *C, wmOperator *op)
{
  GestureData *gesture_data = MEM_new<GestureData>(__func__);
  gesture_data->shape_type = SCULPT_GESTURE_SHAPE_LASSO;

  init_common(C, op, gesture_data);

  int mcoords_len;
  const int(*mcoords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcoords_len);

  if (!mcoords) {
    return nullptr;
  }

  gesture_data->lasso.projviewobjmat = ED_view3d_ob_project_mat_get(gesture_data->vc.rv3d,
                                                                    gesture_data->vc.obact);
  BLI_lasso_boundbox(&gesture_data->lasso.boundbox, mcoords, mcoords_len);
  const int lasso_width = 1 + gesture_data->lasso.boundbox.xmax -
                          gesture_data->lasso.boundbox.xmin;
  const int lasso_height = 1 + gesture_data->lasso.boundbox.ymax -
                           gesture_data->lasso.boundbox.ymin;
  gesture_data->lasso.width = lasso_width;
  gesture_data->lasso.mask_px.resize(lasso_width * lasso_height);

  BLI_bitmap_draw_2d_poly_v2i_n(gesture_data->lasso.boundbox.xmin,
                                gesture_data->lasso.boundbox.ymin,
                                gesture_data->lasso.boundbox.xmax,
                                gesture_data->lasso.boundbox.ymax,
                                mcoords,
                                mcoords_len,
                                lasso_px_cb,
                                gesture_data);

  BoundBox bb;
  ED_view3d_clipping_calc(&bb,
                          gesture_data->true_clip_planes,
                          gesture_data->vc.region,
                          gesture_data->vc.obact,
                          &gesture_data->lasso.boundbox);

  gesture_data->gesture_points = static_cast<float(*)[2]>(
      MEM_malloc_arrayN(mcoords_len, sizeof(float[2]), "trim points"));
  gesture_data->tot_gesture_points = mcoords_len;
  for (int i = 0; i < mcoords_len; i++) {
    gesture_data->gesture_points[i][0] = mcoords[i][0];
    gesture_data->gesture_points[i][1] = mcoords[i][1];
  }

  MEM_freeN((void *)mcoords);

  return gesture_data;
}

GestureData *init_from_box(bContext *C, wmOperator *op)
{
  GestureData *gesture_data = MEM_new<GestureData>(__func__);
  gesture_data->shape_type = SCULPT_GESTURE_SHAPE_BOX;

  init_common(C, op, gesture_data);

  rcti rect;
  WM_operator_properties_border_to_rcti(op, &rect);

  BoundBox bb;
  ED_view3d_clipping_calc(
      &bb, gesture_data->true_clip_planes, gesture_data->vc.region, gesture_data->vc.obact, &rect);

  gesture_data->gesture_points = static_cast<float(*)[2]>(
      MEM_calloc_arrayN(4, sizeof(float[2]), "trim points"));
  gesture_data->tot_gesture_points = 4;

  gesture_data->gesture_points[0][0] = rect.xmax;
  gesture_data->gesture_points[0][1] = rect.ymax;

  gesture_data->gesture_points[1][0] = rect.xmax;
  gesture_data->gesture_points[1][1] = rect.ymin;

  gesture_data->gesture_points[2][0] = rect.xmin;
  gesture_data->gesture_points[2][1] = rect.ymin;

  gesture_data->gesture_points[3][0] = rect.xmin;
  gesture_data->gesture_points[3][1] = rect.ymax;
  return gesture_data;
}

static void line_plane_from_tri(float *r_plane,
                                GestureData *gesture_data,
                                const bool flip,
                                const float p1[3],
                                const float p2[3],
                                const float p3[3])
{
  float normal[3];
  normal_tri_v3(normal, p1, p2, p3);
  mul_v3_mat3_m4v3(normal, gesture_data->vc.obact->world_to_object().ptr(), normal);
  if (flip) {
    mul_v3_fl(normal, -1.0f);
  }
  float plane_point_object_space[3];
  mul_v3_m4v3(plane_point_object_space, gesture_data->vc.obact->world_to_object().ptr(), p1);
  plane_from_point_normal_v3(r_plane, plane_point_object_space, normal);
}

/* Creates 4 points in the plane defined by the line and 2 extra points with an offset relative to
 * this plane. */
static void line_calculate_plane_points(GestureData *gesture_data,
                                        float line_points[2][2],
                                        float r_plane_points[4][3],
                                        float r_offset_plane_points[2][3])
{
  float depth_point[3];
  add_v3_v3v3(depth_point, gesture_data->true_view_origin, gesture_data->true_view_normal);
  ED_view3d_win_to_3d(gesture_data->vc.v3d,
                      gesture_data->vc.region,
                      depth_point,
                      line_points[0],
                      r_plane_points[0]);
  ED_view3d_win_to_3d(gesture_data->vc.v3d,
                      gesture_data->vc.region,
                      depth_point,
                      line_points[1],
                      r_plane_points[3]);

  madd_v3_v3v3fl(
      depth_point, gesture_data->true_view_origin, gesture_data->true_view_normal, 10.0f);
  ED_view3d_win_to_3d(gesture_data->vc.v3d,
                      gesture_data->vc.region,
                      depth_point,
                      line_points[0],
                      r_plane_points[1]);
  ED_view3d_win_to_3d(gesture_data->vc.v3d,
                      gesture_data->vc.region,
                      depth_point,
                      line_points[1],
                      r_plane_points[2]);

  float normal[3];
  normal_tri_v3(normal, r_plane_points[0], r_plane_points[1], r_plane_points[2]);
  add_v3_v3v3(r_offset_plane_points[0], r_plane_points[0], normal);
  add_v3_v3v3(r_offset_plane_points[1], r_plane_points[3], normal);
}

GestureData *init_from_line(bContext *C, wmOperator *op)
{
  GestureData *gesture_data = MEM_new<GestureData>(__func__);
  gesture_data->shape_type = SCULPT_GESTURE_SHAPE_LINE;

  init_common(C, op, gesture_data);

  float line_points[2][2];
  line_points[0][0] = RNA_int_get(op->ptr, "xstart");
  line_points[0][1] = RNA_int_get(op->ptr, "ystart");
  line_points[1][0] = RNA_int_get(op->ptr, "xend");
  line_points[1][1] = RNA_int_get(op->ptr, "yend");

  gesture_data->line.flip = RNA_boolean_get(op->ptr, "flip");

  float plane_points[4][3];
  float offset_plane_points[2][3];
  line_calculate_plane_points(gesture_data, line_points, plane_points, offset_plane_points);

  /* Calculate line plane and normal. */
  const bool flip = gesture_data->line.flip ^ (!gesture_data->vc.rv3d->is_persp);
  line_plane_from_tri(gesture_data->line.true_plane,
                      gesture_data,
                      flip,
                      plane_points[0],
                      plane_points[1],
                      plane_points[2]);

  /* Calculate the side planes. */
  line_plane_from_tri(gesture_data->line.true_side_plane[0],
                      gesture_data,
                      false,
                      plane_points[1],
                      plane_points[0],
                      offset_plane_points[0]);
  line_plane_from_tri(gesture_data->line.true_side_plane[1],
                      gesture_data,
                      false,
                      plane_points[3],
                      plane_points[2],
                      offset_plane_points[1]);

  return gesture_data;
}

void free_data(GestureData *gesture_data)
{
  MEM_SAFE_FREE(gesture_data->gesture_points);
  MEM_SAFE_FREE(gesture_data->operation);
  MEM_delete(gesture_data);
}

static void flip_plane(float out[4], const float in[4], const char symm)
{
  if (symm & PAINT_SYMM_X) {
    out[0] = -in[0];
  }
  else {
    out[0] = in[0];
  }
  if (symm & PAINT_SYMM_Y) {
    out[1] = -in[1];
  }
  else {
    out[1] = in[1];
  }
  if (symm & PAINT_SYMM_Z) {
    out[2] = -in[2];
  }
  else {
    out[2] = in[2];
  }

  out[3] = in[3];
}

static void flip_for_symmetry_pass(GestureData *gesture_data, const ePaintSymmetryFlags symmpass)
{
  gesture_data->symmpass = symmpass;
  for (int j = 0; j < 4; j++) {
    flip_plane(gesture_data->clip_planes[j], gesture_data->true_clip_planes[j], symmpass);
  }

  negate_m4(gesture_data->clip_planes);

  flip_v3_v3(gesture_data->view_normal, gesture_data->true_view_normal, symmpass);
  flip_v3_v3(gesture_data->view_origin, gesture_data->true_view_origin, symmpass);
  flip_plane(gesture_data->line.plane, gesture_data->line.true_plane, symmpass);
  flip_plane(gesture_data->line.side_plane[0], gesture_data->line.true_side_plane[0], symmpass);
  flip_plane(gesture_data->line.side_plane[1], gesture_data->line.true_side_plane[1], symmpass);
}

static Vector<PBVHNode *> update_affected_nodes_by_line_plane(GestureData *gesture_data)
{
  SculptSession *ss = gesture_data->ss;
  float clip_planes[3][4];
  copy_v4_v4(clip_planes[0], gesture_data->line.plane);
  copy_v4_v4(clip_planes[1], gesture_data->line.side_plane[0]);
  copy_v4_v4(clip_planes[2], gesture_data->line.side_plane[1]);

  PBVHFrustumPlanes frustum{};
  frustum.planes = clip_planes;
  frustum.num_planes = gesture_data->line.use_side_planes ? 3 : 1;

  return gesture_data->nodes = bke::pbvh::search_gather(ss->pbvh, [&](PBVHNode &node) {
           return BKE_pbvh_node_frustum_contain_AABB(&node, &frustum);
         });
}

static void update_affected_nodes_by_clip_planes(GestureData *gesture_data)
{
  SculptSession *ss = gesture_data->ss;
  float clip_planes[4][4];
  copy_m4_m4(clip_planes, gesture_data->clip_planes);
  negate_m4(clip_planes);

  PBVHFrustumPlanes frustum{};
  frustum.planes = clip_planes;
  frustum.num_planes = 4;

  gesture_data->nodes = bke::pbvh::search_gather(ss->pbvh, [&](PBVHNode &node) {
    return BKE_pbvh_node_frustum_contain_AABB(&node, &frustum);
  });
}

static void update_affected_nodes(GestureData *gesture_data)
{
  switch (gesture_data->shape_type) {
    case SCULPT_GESTURE_SHAPE_BOX:
    case SCULPT_GESTURE_SHAPE_LASSO:
      update_affected_nodes_by_clip_planes(gesture_data);
      break;
    case SCULPT_GESTURE_SHAPE_LINE:
      update_affected_nodes_by_line_plane(gesture_data);
      break;
  }
}

static bool is_affected_lasso(GestureData *gesture_data, const float co[3])
{
  int scr_co_s[2];
  float co_final[3];

  flip_v3_v3(co_final, co, gesture_data->symmpass);

  /* First project point to 2d space. */
  const float2 scr_co_f = ED_view3d_project_float_v2_m4(
      gesture_data->vc.region, co_final, gesture_data->lasso.projviewobjmat);

  scr_co_s[0] = scr_co_f[0];
  scr_co_s[1] = scr_co_f[1];

  /* Clip against lasso boundbox. */
  LassoData *lasso = &gesture_data->lasso;
  if (!BLI_rcti_isect_pt(&lasso->boundbox, scr_co_s[0], scr_co_s[1])) {
    return false;
  }

  scr_co_s[0] -= lasso->boundbox.xmin;
  scr_co_s[1] -= lasso->boundbox.ymin;

  return lasso->mask_px[scr_co_s[1] * lasso->width + scr_co_s[0]].test();
}

bool is_affected(GestureData *gesture_data, const float3 &co, const float3 &vertex_normal)
{
  float dot = dot_v3v3(gesture_data->view_normal, vertex_normal);
  const bool is_effected_front_face = !(gesture_data->front_faces_only && dot < 0.0f);

  if (!is_effected_front_face) {
    return false;
  }

  switch (gesture_data->shape_type) {
    case SCULPT_GESTURE_SHAPE_BOX:
      return isect_point_planes_v3(gesture_data->clip_planes, 4, co);
    case SCULPT_GESTURE_SHAPE_LASSO:
      return is_affected_lasso(gesture_data, co);
    case SCULPT_GESTURE_SHAPE_LINE:
      if (gesture_data->line.use_side_planes) {
        return plane_point_side_v3(gesture_data->line.plane, co) > 0.0f &&
               plane_point_side_v3(gesture_data->line.side_plane[0], co) > 0.0f &&
               plane_point_side_v3(gesture_data->line.side_plane[1], co) > 0.0f;
      }
      return plane_point_side_v3(gesture_data->line.plane, co) > 0.0f;
  }
  return false;
}

void apply(bContext *C, GestureData *gesture_data, wmOperator *op)
{
  Operation *operation = gesture_data->operation;
  undo::push_begin(CTX_data_active_object(C), op);

  operation->begin(C, gesture_data);

  for (int symmpass = 0; symmpass <= gesture_data->symm; symmpass++) {
    if (SCULPT_is_symmetry_iteration_valid(symmpass, gesture_data->symm)) {
      flip_for_symmetry_pass(gesture_data, ePaintSymmetryFlags(symmpass));
      update_affected_nodes(gesture_data);

      operation->apply_for_symmetry_pass(C, gesture_data);
    }
  }

  operation->end(C, gesture_data);

  Object *ob = CTX_data_active_object(C);
  undo::push_end(ob);

  SCULPT_tag_update_overlays(C);
}
}  // namespace blender::ed::sculpt_paint::gesture
