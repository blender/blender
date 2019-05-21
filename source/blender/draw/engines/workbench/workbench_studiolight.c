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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */
#include "BKE_studiolight.h"

#include "workbench_private.h"

#include "BKE_object.h"

#include "BLI_math.h"

void studiolight_update_world(WORKBENCH_PrivateData *wpd,
                              StudioLight *studiolight,
                              WORKBENCH_UBO_World *wd)
{
  float view_matrix[4][4], rot_matrix[4][4];
  DRW_view_viewmat_get(NULL, view_matrix, false);

  if (USE_WORLD_ORIENTATION(wpd)) {
    axis_angle_to_mat4_single(rot_matrix, 'Z', -wpd->shading.studiolight_rot_z);
    mul_m4_m4m4(rot_matrix, view_matrix, rot_matrix);
    swap_v3_v3(rot_matrix[2], rot_matrix[1]);
    negate_v3(rot_matrix[2]);
  }
  else {
    unit_m4(rot_matrix);
  }

  if (U.edit_studio_light) {
    studiolight = BKE_studiolight_studio_edit_get();
  }

  /* Studio Lights. */
  for (int i = 0; i < 4; i++) {
    WORKBENCH_UBO_Light *light = &wd->lights[i];

    SolidLight *sl = &studiolight->light[i];
    if (sl->flag) {
      copy_v3_v3(light->light_direction, sl->vec);
      mul_mat3_m4_v3(rot_matrix, light->light_direction);
      /* We should predivide the power by PI but that makes the lights really dim. */
      copy_v3_v3(light->specular_color, sl->spec);
      copy_v3_v3(light->diffuse_color, sl->col);
      light->wrapped = sl->smooth;
    }
    else {
      copy_v3_fl3(light->light_direction, 1.0f, 0.0f, 0.0f);
      copy_v3_fl(light->specular_color, 0.0f);
      copy_v3_fl(light->diffuse_color, 0.0f);
    }
  }

  copy_v3_v3(wd->ambient_color, studiolight->light_ambient);

#if 0
  BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED);

#  if STUDIOLIGHT_SH_BANDS == 2
  /* Use Geomerics non-linear SH. */
  mul_v3_v3fl(wd->spherical_harmonics_coefs[0], sl->spherical_harmonics_coefs[0], M_1_PI);
  /* Swizzle to make shader code simpler. */
  for (int i = 0; i < 3; ++i) {
    copy_v3_fl3(wd->spherical_harmonics_coefs[i + 1],
                -sl->spherical_harmonics_coefs[3][i],
                sl->spherical_harmonics_coefs[2][i],
                -sl->spherical_harmonics_coefs[1][i]);
    mul_v3_fl(wd->spherical_harmonics_coefs[i + 1],
              M_1_PI * 1.5f); /* 1.5f is to improve the contrast a bit. */
  }

  /* Precompute as much as we can. See shader code for derivation. */
  float len_r1[3], lr1_r0[3], p[3], a[3];
  for (int i = 0; i < 3; ++i) {
    mul_v3_fl(wd->spherical_harmonics_coefs[i + 1], 0.5f);
    len_r1[i] = len_v3(wd->spherical_harmonics_coefs[i + 1]);
    mul_v3_fl(wd->spherical_harmonics_coefs[i + 1], 1.0f / len_r1[i]);
  }
  /* lr1_r0 = lenR1 / R0; */
  copy_v3_v3(lr1_r0, wd->spherical_harmonics_coefs[0]);
  invert_v3(lr1_r0);
  mul_v3_v3(lr1_r0, len_r1);
  /* p = 1.0 + 2.0 * lr1_r0; */
  copy_v3_v3(p, lr1_r0);
  mul_v3_fl(p, 2.0f);
  add_v3_fl(p, 1.0f);
  /* a = (1.0 - lr1_r0) / (1.0 + lr1_r0); */
  copy_v3_v3(a, lr1_r0);
  add_v3_fl(a, 1.0f);
  invert_v3(a);
  negate_v3(lr1_r0);
  add_v3_fl(lr1_r0, 1.0f);
  mul_v3_v3(a, lr1_r0);
  /* sh_coefs[4] = p; */
  copy_v3_v3(wd->spherical_harmonics_coefs[4], p);
  /* sh_coefs[5] = R0 * a; */
  mul_v3_v3v3(wd->spherical_harmonics_coefs[5], wd->spherical_harmonics_coefs[0], a);
  /* sh_coefs[0] = R0 * (1.0 - a) * (p + 1.0); */
  negate_v3(a);
  add_v3_fl(a, 1.0f);
  add_v3_fl(p, 1.0f);
  mul_v3_v3(a, p);
  mul_v3_v3(wd->spherical_harmonics_coefs[0], a);
#  else
  for (int i = 0; i < STUDIOLIGHT_SH_EFFECTIVE_COEFS_LEN; i++) {
    /* Can't memcpy because of alignment */
    copy_v3_v3(wd->spherical_harmonics_coefs[i], sl->spherical_harmonics_coefs[i]);
  }
#  endif
#endif
}

static void compute_parallel_lines_nor_and_dist(const float v1[2],
                                                const float v2[2],
                                                const float v3[2],
                                                float r_line[2])
{
  sub_v2_v2v2(r_line, v2, v1);
  /* Find orthogonal vector. */
  SWAP(float, r_line[0], r_line[1]);
  r_line[0] = -r_line[0];
  /* Edge distances. */
  r_line[2] = dot_v2v2(r_line, v1);
  r_line[3] = dot_v2v2(r_line, v3);
  /* Make sure r_line[2] is the minimum. */
  if (r_line[2] > r_line[3]) {
    SWAP(float, r_line[2], r_line[3]);
  }
}

void studiolight_update_light(WORKBENCH_PrivateData *wpd, const float light_direction[3])
{
  wpd->shadow_changed = !compare_v3v3(wpd->cached_shadow_direction, light_direction, 1e-5f);

  if (wpd->shadow_changed) {
    float up[3] = {0.0f, 0.0f, 1.0f};
    unit_m4(wpd->shadow_mat);

    /* TODO fix singularity. */
    copy_v3_v3(wpd->shadow_mat[2], light_direction);
    cross_v3_v3v3(wpd->shadow_mat[0], wpd->shadow_mat[2], up);
    normalize_v3(wpd->shadow_mat[0]);
    cross_v3_v3v3(wpd->shadow_mat[1], wpd->shadow_mat[2], wpd->shadow_mat[0]);

    invert_m4_m4(wpd->shadow_inv, wpd->shadow_mat);

    copy_v3_v3(wpd->cached_shadow_direction, light_direction);
  }

  float planes[6][4];
  DRW_culling_frustum_planes_get(NULL, planes);
  /* we only need the far plane. */
  copy_v4_v4(wpd->shadow_far_plane, planes[2]);

  BoundBox frustum_corners;
  DRW_culling_frustum_corners_get(NULL, &frustum_corners);

  mul_v3_mat3_m4v3(wpd->shadow_near_corners[0], wpd->shadow_inv, frustum_corners.vec[0]);
  mul_v3_mat3_m4v3(wpd->shadow_near_corners[1], wpd->shadow_inv, frustum_corners.vec[3]);
  mul_v3_mat3_m4v3(wpd->shadow_near_corners[2], wpd->shadow_inv, frustum_corners.vec[7]);
  mul_v3_mat3_m4v3(wpd->shadow_near_corners[3], wpd->shadow_inv, frustum_corners.vec[4]);

  INIT_MINMAX(wpd->shadow_near_min, wpd->shadow_near_max);
  for (int i = 0; i < 4; ++i) {
    minmax_v3v3_v3(wpd->shadow_near_min, wpd->shadow_near_max, wpd->shadow_near_corners[i]);
  }

  compute_parallel_lines_nor_and_dist(wpd->shadow_near_corners[0],
                                      wpd->shadow_near_corners[1],
                                      wpd->shadow_near_corners[2],
                                      wpd->shadow_near_sides[0]);
  compute_parallel_lines_nor_and_dist(wpd->shadow_near_corners[1],
                                      wpd->shadow_near_corners[2],
                                      wpd->shadow_near_corners[0],
                                      wpd->shadow_near_sides[1]);
}

static BoundBox *studiolight_object_shadow_bbox_get(WORKBENCH_PrivateData *wpd,
                                                    Object *ob,
                                                    WORKBENCH_ObjectData *oed)
{
  if ((oed->shadow_bbox_dirty) || (wpd->shadow_changed)) {
    float tmp_mat[4][4];
    mul_m4_m4m4(tmp_mat, wpd->shadow_inv, ob->obmat);

    /* Get AABB in shadow space. */
    INIT_MINMAX(oed->shadow_min, oed->shadow_max);

    /* From object space to shadow space */
    BoundBox *bbox = BKE_object_boundbox_get(ob);
    for (int i = 0; i < 8; ++i) {
      float corner[3];
      mul_v3_m4v3(corner, tmp_mat, bbox->vec[i]);
      minmax_v3v3_v3(oed->shadow_min, oed->shadow_max, corner);
    }
    oed->shadow_depth = oed->shadow_max[2] - oed->shadow_min[2];
    /* Extend towards infinity. */
    oed->shadow_max[2] += 1e4f;

    /* Get extended AABB in world space. */
    BKE_boundbox_init_from_minmax(&oed->shadow_bbox, oed->shadow_min, oed->shadow_max);
    for (int i = 0; i < 8; ++i) {
      mul_m4_v3(wpd->shadow_mat, oed->shadow_bbox.vec[i]);
    }
    oed->shadow_bbox_dirty = false;
  }

  return &oed->shadow_bbox;
}

bool studiolight_object_cast_visible_shadow(WORKBENCH_PrivateData *wpd,
                                            Object *ob,
                                            WORKBENCH_ObjectData *oed)
{
  BoundBox *shadow_bbox = studiolight_object_shadow_bbox_get(wpd, ob, oed);
  const DRWView *default_view = DRW_view_default_get();
  return DRW_culling_box_test(default_view, shadow_bbox);
}

float studiolight_object_shadow_distance(WORKBENCH_PrivateData *wpd,
                                         Object *ob,
                                         WORKBENCH_ObjectData *oed)
{
  BoundBox *shadow_bbox = studiolight_object_shadow_bbox_get(wpd, ob, oed);

  int corners[4] = {0, 3, 4, 7};
  float dist = 1e4f, dist_isect;
  for (int i = 0; i < 4; ++i) {
    if (isect_ray_plane_v3(shadow_bbox->vec[corners[i]],
                           wpd->cached_shadow_direction,
                           wpd->shadow_far_plane,
                           &dist_isect,
                           true)) {
      if (dist_isect < dist) {
        dist = dist_isect;
      }
    }
    else {
      /* All rays are parallels. If one fails, the other will too. */
      break;
    }
  }
  return max_ii(dist - oed->shadow_depth, 0);
}

bool studiolight_camera_in_object_shadow(WORKBENCH_PrivateData *wpd,
                                         Object *ob,
                                         WORKBENCH_ObjectData *oed)
{
  /* Just to be sure the min, max are updated. */
  studiolight_object_shadow_bbox_get(wpd, ob, oed);

  /* Test if near plane is in front of the shadow. */
  if (oed->shadow_min[2] > wpd->shadow_near_max[2]) {
    return false;
  }

  /* Separation Axis Theorem test */

  /* Test bbox sides first (faster) */
  if ((oed->shadow_min[0] > wpd->shadow_near_max[0]) ||
      (oed->shadow_max[0] < wpd->shadow_near_min[0]) ||
      (oed->shadow_min[1] > wpd->shadow_near_max[1]) ||
      (oed->shadow_max[1] < wpd->shadow_near_min[1])) {
    return false;
  }

  /* Test projected near rectangle sides */
  float pts[4][2] = {
      {oed->shadow_min[0], oed->shadow_min[1]},
      {oed->shadow_min[0], oed->shadow_max[1]},
      {oed->shadow_max[0], oed->shadow_min[1]},
      {oed->shadow_max[0], oed->shadow_max[1]},
  };

  for (int i = 0; i < 2; ++i) {
    float min_dst = FLT_MAX, max_dst = -FLT_MAX;
    for (int j = 0; j < 4; ++j) {
      float dst = dot_v2v2(wpd->shadow_near_sides[i], pts[j]);
      /* Do min max */
      if (min_dst > dst) {
        min_dst = dst;
      }
      if (max_dst < dst) {
        max_dst = dst;
      }
    }

    if ((wpd->shadow_near_sides[i][2] > max_dst) || (wpd->shadow_near_sides[i][3] < min_dst)) {
      return false;
    }
  }

  /* No separation axis found. Both shape intersect. */
  return true;
}
