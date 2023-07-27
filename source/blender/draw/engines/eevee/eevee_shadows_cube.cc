/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup EEVEE
 */

#include "eevee_private.h"

void EEVEE_shadows_cube_add(EEVEE_LightsInfo *linfo, EEVEE_Light *evli, Object *ob)
{
  if (linfo->cube_len >= MAX_SHADOW_CUBE) {
    return;
  }

  const Light *la = (Light *)ob->data;
  EEVEE_Shadow *sh_data = linfo->shadow_data + linfo->shadow_len;

  /* Always update dupli lights as EEVEE_LightEngineData is not saved.
   * Same issue with dupli shadow casters. */
  bool update = (ob->base_flag & BASE_FROM_DUPLI) != 0;
  if (!update) {
    EEVEE_LightEngineData *led = EEVEE_light_data_ensure(ob);
    if (led->need_update) {
      update = true;
      led->need_update = false;
    }
  }

  if (update) {
    BLI_BITMAP_ENABLE(&linfo->sh_cube_update[0], linfo->cube_len);
  }

  sh_data->near = max_ff(la->clipsta, 1e-8f);
  sh_data->bias = max_ff(la->bias * 0.05f, 0.0f);
  eevee_contact_shadow_setup(la, sh_data);

  /* Saving light bounds for later. */
  BoundSphere *cube_bound = linfo->shadow_bounds + linfo->cube_len;
  copy_v3_v3(cube_bound->center, evli->position);
  cube_bound->radius = sqrt(1.0f / min_ff(evli->invsqrdist, evli->invsqrdist_volume));

  linfo->shadow_cube_light_indices[linfo->cube_len] = linfo->num_light;
  evli->shadow_id = linfo->shadow_len++;
  sh_data->type_data_id = linfo->cube_len++;

  /* Same as linfo->cube_len, no need to save. */
  linfo->num_cube_layer++;
}

static void shadow_cube_random_position_set(const EEVEE_Light *evli,
                                            int sample_ofs,
                                            float ws_sample_pos[3])
{
  float jitter[3];
#ifdef DEBUG_SHADOW_DISTRIBUTION
  int i = 0;
start:
#else
  int i = sample_ofs;
#endif
  switch ((int)evli->light_type) {
    case LA_AREA:
      EEVEE_sample_rectangle(i, evli->rightvec, evli->upvec, evli->sizex, evli->sizey, jitter);
      break;
    case (int)LAMPTYPE_AREA_ELLIPSE:
      EEVEE_sample_ellipse(i, evli->rightvec, evli->upvec, evli->sizex, evli->sizey, jitter);
      break;
    default:
      EEVEE_sample_ball(i, evli->radius, jitter);
  }
#ifdef DEBUG_SHADOW_DISTRIBUTION
  float p[3];
  add_v3_v3v3(p, jitter, ws_sample_pos);
  DRW_debug_sphere(p, 0.01f, blender::float4{1.0f, (sample_ofs == i) ? 1.0f : 0.0f, 0.0f, 1.0f});
  if (i++ < sample_ofs) {
    goto start;
  }
#endif
  add_v3_v3(ws_sample_pos, jitter);
}

bool EEVEE_shadows_cube_setup(EEVEE_LightsInfo *linfo, const EEVEE_Light *evli, int sample_ofs)
{
  EEVEE_Shadow *shdw_data = linfo->shadow_data + (int)evli->shadow_id;
  EEVEE_ShadowCube *cube_data = linfo->shadow_cube_data + (int)shdw_data->type_data_id;

  eevee_light_matrix_get(evli, cube_data->shadowmat);

  shdw_data->far = max_ff(sqrt(1.0f / min_ff(evli->invsqrdist, evli->invsqrdist_volume)), 3e-4);
  shdw_data->near = min_ff(shdw_data->near, shdw_data->far - 1e-4);

  bool update = false;

  if (linfo->soft_shadows) {
    shadow_cube_random_position_set(evli, sample_ofs, cube_data->shadowmat[3]);
    /* Update if position changes (avoid infinite update if soft shadows does not move).
     * Other changes are caught by depsgraph tagging. This one is for update between samples. */
    update = !compare_v3v3(cube_data->shadowmat[3], cube_data->position, 1e-10f);
    /**
     * Anti-Aliasing jitter: Add random rotation.
     *
     * The 2.0 factor is because texel angular size is not even across the cube-map,
     * so we make the rotation range a bit bigger.
     * This will not blur the shadow even if the spread is too big since we are just
     * rotating the shadow cube-map.
     * Note that this may be a rough approximation an may not converge to a perfectly
     * smooth shadow (because sample distribution is quite non-uniform) but is enough
     * in practice.
     */
    /* NOTE: this has implication for spotlight rendering optimization
     * (see EEVEE_shadows_draw_cubemap). */
    float angular_texel_size = 2.0f * DEG2RADF(90) / (float)linfo->shadow_cube_size;
    EEVEE_random_rotation_m4(sample_ofs, angular_texel_size, cube_data->shadowmat);
  }

  copy_v3_v3(cube_data->position, cube_data->shadowmat[3]);
  invert_m4(cube_data->shadowmat);

  return update;
}

static void eevee_ensure_cube_views(
    float near, float far, int cube_res, const float viewmat[4][4], DRWView *view[6])
{
  float winmat[4][4];
  float side = near;

  /* TODO: shadow-cube array. */
  if (true) {
    /* This half texel offset is used to ensure correct filtering between faces. */
    /* FIXME: This exhibit float precision issue with lower cube_res.
     * But it seems to be caused by the perspective_m4. */
    side *= ((float)cube_res + 1.0f) / (float)(cube_res);
  }

  perspective_m4(winmat, -side, side, -side, side, near, far);

  for (int i = 0; i < 6; i++) {
    float tmp[4][4];
    mul_m4_m4m4(tmp, cubefacemat[i], viewmat);

    if (view[i] == nullptr) {
      view[i] = DRW_view_create(tmp, winmat, nullptr, nullptr, nullptr);
    }
    else {
      DRW_view_update(view[i], tmp, winmat, nullptr, nullptr);
    }
  }
}

/* Does a spot angle fits a single cubeface. */
static bool spot_angle_fit_single_face(const EEVEE_Light *evli)
{
  /* alpha = spot/cone half angle. */
  /* beta = scaled spot/cone half angle. */
  float cos_alpha = evli->spotsize;
  float sin_alpha = sqrtf(max_ff(0.0f, 1.0f - cos_alpha * cos_alpha));
  float cos_beta = min_ff(cos_alpha / hypotf(cos_alpha, sin_alpha * evli->sizex),
                          cos_alpha / hypotf(cos_alpha, sin_alpha * evli->sizey));
  /* Don't use 45 degrees because AA jitter can offset the face. */
  return cos_beta > cosf(DEG2RADF(42.0f));
}

void EEVEE_shadows_draw_cubemap(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, int cube_index)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;
  EEVEE_LightsInfo *linfo = sldata->lights;

  EEVEE_Light *evli = linfo->light_data + linfo->shadow_cube_light_indices[cube_index];
  EEVEE_Shadow *shdw_data = linfo->shadow_data + (int)evli->shadow_id;
  EEVEE_ShadowCube *cube_data = linfo->shadow_cube_data + (int)shdw_data->type_data_id;

  eevee_ensure_cube_views(shdw_data->near,
                          shdw_data->far,
                          linfo->shadow_cube_size,
                          cube_data->shadowmat,
                          g_data->cube_views);

  /* Render shadow cube */
  /* Render 6 faces separately: seems to be faster for the general case.
   * The only time it's more beneficial is when the CPU culling overhead
   * outweigh the instancing overhead. which is rarely the case. */
  for (int j = 0; j < 6; j++) {
    /* Optimization: Only render the needed faces. */
    /* Skip all but -Z face. */
    if (evli->light_type == LA_SPOT && j != 5 && spot_angle_fit_single_face(evli)) {
      continue;
    }
    /* Skip +Z face. */
    if (evli->light_type != LA_LOCAL && j == 4) {
      continue;
    }
    /* TODO(fclem): some cube sides can be invisible in the main views. Cull them. */
    // if (frustum_intersect(g_data->cube_views[j], main_view))
    //   continue;

    DRW_view_set_active(g_data->cube_views[j]);
    int layer = cube_index * 6 + j;
    GPU_framebuffer_texture_layer_attach(sldata->shadow_fb, sldata->shadow_cube_pool, 0, layer, 0);
    GPU_framebuffer_bind(sldata->shadow_fb);
    GPU_framebuffer_clear_depth(sldata->shadow_fb, 1.0f);
    DRW_draw_pass(psl->shadow_pass);
  }

  BLI_BITMAP_SET(&linfo->sh_cube_update[0], cube_index, false);
}
