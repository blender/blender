/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup EEVEE
 */

#include "BLI_rect.h"
#include "BLI_sys_types.h" /* bool */

#include "BKE_object.h"

#include "eevee_private.h"

#include "BLI_rand.h" /* needs to be after for some reason. */

void EEVEE_shadows_cascade_add(EEVEE_LightsInfo *linfo, EEVEE_Light *evli, Object *ob)
{
  if (linfo->cascade_len >= MAX_SHADOW_CASCADE) {
    return;
  }

  const Light *la = (Light *)ob->data;
  EEVEE_Shadow *sh_data = linfo->shadow_data + linfo->shadow_len;
  EEVEE_ShadowCascade *csm_data = linfo->shadow_cascade_data + linfo->cascade_len;
  EEVEE_ShadowCascadeRender *csm_render = linfo->shadow_cascade_render + linfo->cascade_len;

  eevee_contact_shadow_setup(la, sh_data);

  linfo->shadow_cascade_light_indices[linfo->cascade_len] = linfo->num_light;
  evli->shadow_id = linfo->shadow_len++;
  sh_data->type_data_id = linfo->cascade_len++;
  csm_data->tex_id = linfo->num_cascade_layer;
  csm_render->cascade_fade = la->cascade_fade;
  csm_render->cascade_count = la->cascade_count;
  csm_render->cascade_exponent = la->cascade_exponent;
  csm_render->cascade_max_dist = la->cascade_max_dist;
  csm_render->original_bias = max_ff(la->bias, 0.0f);

  linfo->num_cascade_layer += la->cascade_count;
}

static void shadow_cascade_random_matrix_set(float mat[4][4], float radius, int sample_ofs)
{
  float jitter[3];
#ifndef DEBUG_SHADOW_DISTRIBUTION
  EEVEE_sample_ellipse(sample_ofs, mat[0], mat[1], radius, radius, jitter);
#else
  for (int i = 0; i <= sample_ofs; i++) {
    EEVEE_sample_ellipse(i, mat[0], mat[1], radius, radius, jitter);
    float p[3];
    add_v3_v3v3(p, jitter, mat[2]);
    DRW_debug_sphere(p, 0.01f, blender::float4{1.0f, (sample_ofs == i) ? 1.0f : 0.0f, 0.0f, 1.0f});
  }
#endif
  add_v3_v3(mat[2], jitter);
  orthogonalize_m4(mat, 2);
}

static double round_to_digits(double value, int digits)
{
  double factor = pow(10.0, digits - ceil(log10(fabs(value))));
  return round(value * factor) / factor;
}

static void frustum_min_bounding_sphere(const float corners[8][3],
                                        float r_center[3],
                                        float *r_radius)
{
#if 0 /* Simple solution but waste too much space. */
  float minvec[3], maxvec[3];

  /* compute the bounding box */
  INIT_MINMAX(minvec, maxvec);
  for (int i = 0; i < 8; i++) {
    minmax_v3v3_v3(minvec, maxvec, corners[i]);
  }

  /* compute the bounding sphere of this box */
  r_radius = len_v3v3(minvec, maxvec) * 0.5f;
  add_v3_v3v3(r_center, minvec, maxvec);
  mul_v3_fl(r_center, 0.5f);
#else
  /* Find averaged center. */
  zero_v3(r_center);
  for (int i = 0; i < 8; i++) {
    add_v3_v3(r_center, corners[i]);
  }
  mul_v3_fl(r_center, 1.0f / 8.0f);

  /* Search the largest distance from the sphere center. */
  *r_radius = 0.0f;
  for (int i = 0; i < 8; i++) {
    float rad = len_squared_v3v3(corners[i], r_center);
    if (rad > *r_radius) {
      *r_radius = rad;
    }
  }

  /* TODO: try to reduce the radius further by moving the center.
   * Remember we need a __stable__ solution! */

  /* Try to reduce float imprecision leading to shimmering. */
  *r_radius = (float)round_to_digits(sqrtf(*r_radius), 3);
#endif
}

static void eevee_shadow_cascade_setup(EEVEE_LightsInfo *linfo,
                                       EEVEE_Light *evli,
                                       DRWView *view,
                                       float view_near,
                                       float view_far,
                                       int sample_ofs)
{
  EEVEE_Shadow *shdw_data = linfo->shadow_data + (int)evli->shadow_id;
  EEVEE_ShadowCascade *csm_data = linfo->shadow_cascade_data + (int)shdw_data->type_data_id;
  EEVEE_ShadowCascadeRender *csm_render = linfo->shadow_cascade_render +
                                          (int)shdw_data->type_data_id;
  int cascade_count = csm_render->cascade_count;
  float cascade_fade = csm_render->cascade_fade;
  float cascade_max_dist = csm_render->cascade_max_dist;
  float cascade_exponent = csm_render->cascade_exponent;

  float jitter_ofs[2];
  double ht_point[2];
  double ht_offset[2] = {0.0, 0.0};
  const uint ht_primes[2] = {2, 3};

  BLI_halton_2d(ht_primes, ht_offset, sample_ofs, ht_point);

  /* Not really sure why we need 4.0 factor here. */
  jitter_ofs[0] = (ht_point[0] * 2.0 - 1.0) * 4.0 / linfo->shadow_cascade_size;
  jitter_ofs[1] = (ht_point[1] * 2.0 - 1.0) * 4.0 / linfo->shadow_cascade_size;

  /* Camera Matrices */
  float persinv[4][4], vp_projmat[4][4];
  DRW_view_persmat_get(view, persinv, true);
  DRW_view_winmat_get(view, vp_projmat, false);
  bool is_persp = DRW_view_is_persp_get(view);

  /* obmat = Object Space > World Space */
  /* viewmat = World Space > View Space */
  float(*viewmat)[4] = csm_render->viewmat;
  eevee_light_matrix_get(evli, viewmat);
  /* At this point, viewmat == normalize_m4(obmat) */

  if (linfo->soft_shadows) {
    shadow_cascade_random_matrix_set(viewmat, evli->radius, sample_ofs);
  }

  copy_m4_m4(csm_render->viewinv, viewmat);
  invert_m4(viewmat);

  copy_v3_v3(csm_data->shadow_vec, csm_render->viewinv[2]);

  /* Compute near and far value based on all shadow casters cumulated AABBs. */
  float sh_near = -1.0e30f, sh_far = 1.0e30f;
  BoundBox shcaster_bounds;
  BKE_boundbox_init_from_minmax(
      &shcaster_bounds, linfo->shcaster_aabb.min, linfo->shcaster_aabb.max);
#ifdef DEBUG_CSM
  float dbg_col1[4] = {1.0f, 0.5f, 0.6f, 1.0f};
  DRW_debug_bbox(&shcaster_bounds, dbg_col1);
#endif
  for (int i = 0; i < 8; i++) {
    mul_m4_v3(viewmat, shcaster_bounds.vec[i]);
    sh_near = max_ff(sh_near, shcaster_bounds.vec[i][2]);
    sh_far = min_ff(sh_far, shcaster_bounds.vec[i][2]);
  }
#ifdef DEBUG_CSM
  float dbg_col2[4] = {0.5f, 1.0f, 0.6f, 1.0f};
  float pts[2][3] = {{0.0, 0.0, sh_near}, {0.0, 0.0, sh_far}};
  mul_m4_v3(csm_render->viewinv, pts[0]);
  mul_m4_v3(csm_render->viewinv, pts[1]);
  DRW_debug_sphere(pts[0], 1.0f, dbg_col1);
  DRW_debug_sphere(pts[1], 1.0f, dbg_col2);
#endif
  /* The rest of the function is assuming inverted Z. */
  /* Add a little bias to avoid invalid matrices. */
  sh_far = -(sh_far - 1e-3);
  sh_near = -sh_near;

  /* The technique consists into splitting
   * the view frustum into several sub-frustum
   * that are individually receiving one shadow map */

  float csm_start, csm_end;

  if (is_persp) {
    csm_start = view_near;
    csm_end = max_ff(view_far, -cascade_max_dist);
    /* Avoid artifacts */
    csm_end = min_ff(view_near, csm_end);
  }
  else {
    csm_start = -view_far;
    csm_end = view_far;
  }

  /* init near/far */
  for (int c = 0; c < MAX_CASCADE_NUM; c++) {
    csm_data->split_start[c] = csm_end;
    csm_data->split_end[c] = csm_end;
  }

  /* Compute split planes */
  float splits_start_ndc[MAX_CASCADE_NUM];
  float splits_end_ndc[MAX_CASCADE_NUM];

  {
    /* Nearest plane */
    float p[4] = {1.0f, 1.0f, csm_start, 1.0f};
    /* TODO: we don't need full m4 multiply here */
    mul_m4_v4(vp_projmat, p);
    splits_start_ndc[0] = p[2];
    if (is_persp) {
      splits_start_ndc[0] /= p[3];
    }
  }

  {
    /* Farthest plane */
    float p[4] = {1.0f, 1.0f, csm_end, 1.0f};
    /* TODO: we don't need full m4 multiply here */
    mul_m4_v4(vp_projmat, p);
    splits_end_ndc[cascade_count - 1] = p[2];
    if (is_persp) {
      splits_end_ndc[cascade_count - 1] /= p[3];
    }
  }

  csm_data->split_start[0] = csm_start;
  csm_data->split_end[cascade_count - 1] = csm_end;

  for (int c = 1; c < cascade_count; c++) {
    /* View Space */
    float linear_split = interpf(csm_end, csm_start, c / (float)cascade_count);
    float exp_split = csm_start * powf(csm_end / csm_start, c / (float)cascade_count);

    if (is_persp) {
      csm_data->split_start[c] = interpf(exp_split, linear_split, cascade_exponent);
    }
    else {
      csm_data->split_start[c] = linear_split;
    }
    csm_data->split_end[c - 1] = csm_data->split_start[c];

    /* Add some overlap for smooth transition */
    csm_data->split_start[c] = interpf((c > 1) ? csm_data->split_end[c - 2] :
                                                 csm_data->split_start[0],
                                       csm_data->split_end[c - 1],
                                       cascade_fade);

    /* NDC Space */
    {
      float p[4] = {1.0f, 1.0f, csm_data->split_start[c], 1.0f};
      /* TODO: we don't need full m4 multiply here */
      mul_m4_v4(vp_projmat, p);
      splits_start_ndc[c] = p[2];

      if (is_persp) {
        splits_start_ndc[c] /= p[3];
      }
    }

    {
      float p[4] = {1.0f, 1.0f, csm_data->split_end[c - 1], 1.0f};
      /* TODO: we don't need full m4 multiply here */
      mul_m4_v4(vp_projmat, p);
      splits_end_ndc[c - 1] = p[2];

      if (is_persp) {
        splits_end_ndc[c - 1] /= p[3];
      }
    }
  }

  /* Set last cascade split fade distance into the first split_start. */
  float prev_split = (cascade_count > 1) ? csm_data->split_end[cascade_count - 2] :
                                           csm_data->split_start[0];
  csm_data->split_start[0] = interpf(
      prev_split, csm_data->split_end[cascade_count - 1], cascade_fade);

  /* For each cascade */
  for (int c = 0; c < cascade_count; c++) {
    float(*projmat)[4] = csm_render->projmat[c];
    /* Given 8 frustum corners */
    float corners[8][3] = {
        /* Near Cap */
        {1.0f, -1.0f, splits_start_ndc[c]},
        {-1.0f, -1.0f, splits_start_ndc[c]},
        {-1.0f, 1.0f, splits_start_ndc[c]},
        {1.0f, 1.0f, splits_start_ndc[c]},
        /* Far Cap */
        {1.0f, -1.0f, splits_end_ndc[c]},
        {-1.0f, -1.0f, splits_end_ndc[c]},
        {-1.0f, 1.0f, splits_end_ndc[c]},
        {1.0f, 1.0f, splits_end_ndc[c]},
    };

    /* Transform them into world space */
    for (int i = 0; i < 8; i++) {
      mul_project_m4_v3(persinv, corners[i]);
    }

    float center[3];
    frustum_min_bounding_sphere(corners, center, &(csm_render->radius[c]));

#ifdef DEBUG_CSM
    float dbg_col[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    if (c < 3) {
      dbg_col[c] = 1.0f;
    }
    DRW_debug_bbox((const BoundBox *)&corners, dbg_col);
    DRW_debug_sphere(center, csm_render->radius[c], dbg_col);
#endif

    /* Project into light-space. */
    mul_m4_v3(viewmat, center);

    /* Snap projection center to nearest texel to cancel shimmering. */
    float shadow_origin[2], shadow_texco[2];
    /* Light to texture space. */
    mul_v2_v2fl(
        shadow_origin, center, linfo->shadow_cascade_size / (2.0f * csm_render->radius[c]));

    /* Find the nearest texel. */
    shadow_texco[0] = roundf(shadow_origin[0]);
    shadow_texco[1] = roundf(shadow_origin[1]);

    /* Compute offset. */
    sub_v2_v2(shadow_texco, shadow_origin);
    /* Texture to light space. */
    mul_v2_fl(shadow_texco, (2.0f * csm_render->radius[c]) / linfo->shadow_cascade_size);

    /* Apply offset. */
    add_v2_v2(center, shadow_texco);

    /* Expand the projection to cover frustum range */
    rctf rect_cascade;
    BLI_rctf_init_pt_radius(&rect_cascade, center, csm_render->radius[c]);
    orthographic_m4(projmat,
                    rect_cascade.xmin,
                    rect_cascade.xmax,
                    rect_cascade.ymin,
                    rect_cascade.ymax,
                    sh_near,
                    sh_far);

    /* Anti-Aliasing */
    if (linfo->soft_shadows) {
      add_v2_v2(projmat[3], jitter_ofs);
    }

    float viewprojmat[4][4];
    mul_m4_m4m4(viewprojmat, projmat, viewmat);
    mul_m4_m4m4(csm_data->shadowmat[c], texcomat, viewprojmat);

#ifdef DEBUG_CSM
    DRW_debug_m4_as_bbox(viewprojmat, true, dbg_col);
#endif
  }

  /* Bias is in clip-space, divide by range. */
  shdw_data->bias = csm_render->original_bias * 0.05f / fabsf(sh_far - sh_near);
  shdw_data->near = sh_near;
  shdw_data->far = sh_far;
}

static void eevee_ensure_cascade_views(EEVEE_ShadowCascadeRender *csm_render,
                                       DRWView *view[MAX_CASCADE_NUM])
{
  for (int i = 0; i < csm_render->cascade_count; i++) {
    if (view[i] == nullptr) {
      view[i] = DRW_view_create(
          csm_render->viewmat, csm_render->projmat[i], nullptr, nullptr, nullptr);
    }
    else {
      DRW_view_update(view[i], csm_render->viewmat, csm_render->projmat[i], nullptr, nullptr);
    }
  }
}

void EEVEE_shadows_draw_cascades(EEVEE_ViewLayerData *sldata,
                                 EEVEE_Data *vedata,
                                 DRWView *view,
                                 int cascade_index)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_PrivateData *g_data = stl->g_data;
  EEVEE_LightsInfo *linfo = sldata->lights;

  EEVEE_Light *evli = linfo->light_data + linfo->shadow_cascade_light_indices[cascade_index];
  EEVEE_Shadow *shdw_data = linfo->shadow_data + (int)evli->shadow_id;
  EEVEE_ShadowCascade *csm_data = linfo->shadow_cascade_data + (int)shdw_data->type_data_id;
  EEVEE_ShadowCascadeRender *csm_render = linfo->shadow_cascade_render +
                                          (int)shdw_data->type_data_id;

  float near = DRW_view_near_distance_get(view);
  float far = DRW_view_far_distance_get(view);

  eevee_shadow_cascade_setup(linfo, evli, view, near, far, effects->taa_current_sample - 1);

  /* Meh, Reusing the cube views. */
  BLI_assert(MAX_CASCADE_NUM <= 6);
  eevee_ensure_cascade_views(csm_render, g_data->cube_views);

  /* Render shadow cascades */
  /* Render cascade separately: seems to be faster for the general case.
   * The only time it's more beneficial is when the CPU culling overhead
   * outweigh the instancing overhead. which is rarely the case. */
  for (int j = 0; j < csm_render->cascade_count; j++) {
    DRW_view_set_active(g_data->cube_views[j]);
    int layer = csm_data->tex_id + j;
    GPU_framebuffer_texture_layer_attach(
        sldata->shadow_fb, sldata->shadow_cascade_pool, 0, layer, 0);
    GPU_framebuffer_bind(sldata->shadow_fb);
    GPU_framebuffer_clear_depth(sldata->shadow_fb, 1.0f);
    DRW_draw_pass(psl->shadow_pass);
  }
}
