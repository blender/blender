/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#include "BLI_sys_types.h" /* bool */

#include "BKE_object.h"

#include "DEG_depsgraph_query.h"

#include "eevee_private.h"

void eevee_light_matrix_get(const EEVEE_Light *evli, float r_mat[4][4])
{
  copy_v3_v3(r_mat[0], evli->rightvec);
  copy_v3_v3(r_mat[1], evli->upvec);
  negate_v3_v3(r_mat[2], evli->forwardvec);
  copy_v3_v3(r_mat[3], evli->position);
  r_mat[0][3] = 0.0f;
  r_mat[1][3] = 0.0f;
  r_mat[2][3] = 0.0f;
  r_mat[3][3] = 1.0f;
}

static float light_attenuation_radius_get(const Light *la,
                                          float light_threshold,
                                          float light_power)
{
  if (la->mode & LA_CUSTOM_ATTENUATION) {
    return la->att_dist;
  }
  /* Compute the distance (using the inverse square law)
   * at which the light power reaches the light_threshold. */
  return sqrtf(max_ff(1e-16, light_power / max_ff(1e-16, light_threshold)));
}

static void light_shape_parameters_set(EEVEE_Light *evli, const Light *la, const float scale[3])
{
  if (la->type == LA_SPOT) {
    /* Spot size & blend */
    evli->sizex = scale[0] / scale[2];
    evli->sizey = scale[1] / scale[2];
    evli->spotsize = cosf(la->spotsize * 0.5f);
    evli->spotblend = (1.0f - evli->spotsize) * la->spotblend;
    evli->radius = max_ff(0.001f, la->radius);
  }
  else if (la->type == LA_AREA) {
    evli->sizex = max_ff(0.003f, la->area_size * scale[0] * 0.5f);
    if (ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE)) {
      evli->sizey = max_ff(0.003f, la->area_sizey * scale[1] * 0.5f);
    }
    else {
      evli->sizey = max_ff(0.003f, la->area_size * scale[1] * 0.5f);
    }
    /* For volume point lighting. */
    evli->radius = max_ff(0.001f, hypotf(evli->sizex, evli->sizey) * 0.5f);
  }
  else if (la->type == LA_SUN) {
    evli->radius = max_ff(0.001f, tanf(min_ff(la->sun_angle, DEG2RADF(179.9f)) / 2.0f));
  }
  else {
    evli->radius = max_ff(0.001f, la->radius);
  }
}

static float light_shape_radiance_get(const Light *la, const EEVEE_Light *evli)
{
  /* Make illumination power constant. */
  switch (la->type) {
    case LA_AREA: {
      /* Rectangle area. */
      float area = (evli->sizex * 2.0f) * (evli->sizey * 2.0f);
      /* Scale for the lower area of the ellipse compared to the surrounding rectangle. */
      if (ELEM(la->area_shape, LA_AREA_DISK, LA_AREA_ELLIPSE)) {
        area *= M_PI / 4.0f;
      }
      /* NOTE: The 4 factor is from Cycles definition of power. */
      /* NOTE: Missing a factor of PI here to match Cycles. */
      return 1.0f / (4.0f * area);
    }
    case LA_SPOT:
    case LA_LOCAL: {
      /* Sphere area. */
      float area = 4.0f * (float)M_PI * square_f(evli->radius);
      /* NOTE: Presence of a factor of PI here to match Cycles. But it should be missing to be
       * consistent with the other cases. */
      return 1.0f / (area * (float)M_PI);
    }
    default:
    case LA_SUN: {
      /* Disk area. */
      float area = (float)M_PI * square_f(evli->radius);
      /* Make illumination power closer to cycles for bigger radii. Cycles uses a cos^3 term that
       * we cannot reproduce so we account for that by scaling the light power. This function is
       * the result of a rough manual fitting. */
      float sun_scaling = 1.0f + square_f(evli->radius) / 2.0f;
      /* NOTE: Missing a factor of PI here to match Cycles. */
      return sun_scaling / area;
    }
  }
}

/* Returns a factor to apply to light power to get a point light radiance instead of a shape
 * radiance. */
static float light_volume_radiance_factor_get(const Light *la,
                                              const EEVEE_Light *evli,
                                              float area_power)
{
  /* Volume light is evaluated as point lights. Remove the shape power. */
  float power = 1.0f / area_power;

  switch (la->type) {
    case LA_AREA: {
      /* This corrects for area light most representative point trick. The fit was found by
       * reducing the average error compared to cycles. */
      float area = (evli->sizex * 2.0f) * (evli->sizey * 2.0f);
      float tmp = M_PI_2 / (M_PI_2 + sqrtf(area));
      /* Lerp between 1.0 and the limit (1 / pi). */
      float mrp_scaling = tmp + (1.0f - tmp) * M_1_PI;
      /* NOTE: The 4 factor is from Cycles definition of power. */
      /* NOTE: Missing a factor of PI here to match Cycles. */
      power *= mrp_scaling / 4.0f;
      break;
    }
    case LA_SPOT:
    case LA_LOCAL: {
      /* Sphere solid angle. */
      float area = 4.0f * (float)M_PI;
      /* NOTE: Missing a factor of PI here to match Cycles. */
      power *= 1.0f / area;
      break;
    }
    default:
    case LA_SUN: {
      /* NOTE: Missing a factor of PI here to match Cycles. */
      /* Do nothing. */
      break;
    }
  }
  return power;
}

/* Update buffer with light data */
static void eevee_light_setup(Object *ob, EEVEE_Light *evli)
{
  const Light *la = (Light *)ob->data;
  float mat[4][4], scale[3];

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const float light_threshold = draw_ctx->scene->eevee.light_threshold;

  /* Position */
  copy_v3_v3(evli->position, ob->object_to_world[3]);

  /* Color */
  copy_v3_v3(evli->color, &la->r);

  evli->diff = la->diff_fac;
  evli->spec = la->spec_fac;
  evli->volume = la->volume_fac;

  float max_power = max_fff(la->r, la->g, la->b) * fabsf(la->energy / 100.0f);
  float surface_max_power = max_ff(evli->diff, evli->spec) * max_power;
  float volume_max_power = evli->volume * max_power;

  /* Influence Radii. */
  float att_radius = light_attenuation_radius_get(la, light_threshold, surface_max_power);
  float att_radius_volume = light_attenuation_radius_get(la, light_threshold, volume_max_power);
  /* Take the inverse square of this distance. */
  evli->invsqrdist = 1.0f / max_ff(1e-4f, square_f(att_radius));
  evli->invsqrdist_volume = 1.0f / max_ff(1e-4f, square_f(att_radius_volume));

  /* Vectors */
  normalize_m4_m4_ex(mat, ob->object_to_world, scale);
  copy_v3_v3(evli->forwardvec, mat[2]);
  normalize_v3(evli->forwardvec);
  negate_v3(evli->forwardvec);

  copy_v3_v3(evli->rightvec, mat[0]);
  normalize_v3(evli->rightvec);

  copy_v3_v3(evli->upvec, mat[1]);
  normalize_v3(evli->upvec);

  /* Make sure we have a consistent Right Hand coord frame.
   * (in case of negatively scaled Z axis) */
  float cross[3];
  cross_v3_v3v3(cross, evli->rightvec, evli->forwardvec);
  if (dot_v3v3(cross, evli->upvec) < 0.0) {
    negate_v3(evli->upvec);
  }

  light_shape_parameters_set(evli, la, scale);

  /* Light Type */
  evli->light_type = (float)la->type;
  if ((la->type == LA_AREA) && ELEM(la->area_shape, LA_AREA_DISK, LA_AREA_ELLIPSE)) {
    evli->light_type = LAMPTYPE_AREA_ELLIPSE;
  }

  float shape_power = light_shape_radiance_get(la, evli);
  mul_v3_fl(evli->color, shape_power * la->energy);

  evli->volume *= light_volume_radiance_factor_get(la, evli, shape_power);

  /* No shadow by default */
  evli->shadow_id = -1.0f;
}

void EEVEE_lights_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_LightsInfo *linfo = sldata->lights;
  linfo->num_light = 0;

  EEVEE_shadows_cache_init(sldata, vedata);
}

void EEVEE_lights_cache_add(EEVEE_ViewLayerData *sldata, Object *ob)
{
  EEVEE_LightsInfo *linfo = sldata->lights;
  const Light *la = (Light *)ob->data;

  if (linfo->num_light >= MAX_LIGHT) {
    printf("Too many lights in the scene !!!\n");
    return;
  }

  /* Early out if light has no power. */
  if (la->energy == 0.0f || is_zero_v3(&la->r)) {
    return;
  }

  EEVEE_Light *evli = linfo->light_data + linfo->num_light;
  eevee_light_setup(ob, evli);

  if (la->mode & LA_SHADOW) {
    if (la->type == LA_SUN) {
      EEVEE_shadows_cascade_add(linfo, evli, ob);
    }
    else if (ELEM(la->type, LA_SPOT, LA_LOCAL, LA_AREA)) {
      EEVEE_shadows_cube_add(linfo, evli, ob);
    }
  }

  linfo->num_light++;
}

void EEVEE_lights_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_LightsInfo *linfo = sldata->lights;

  sldata->common_data.la_num_light = linfo->num_light;

  /* Clamp volume lights power. */
  float upper_bound = vedata->stl->effects->volume_light_clamp;
  for (int i = 0; i < linfo->num_light; i++) {
    EEVEE_Light *evli = linfo->light_data + i;

    float power = max_fff(UNPACK3(evli->color)) * evli->volume;
    if (power > 0.0f && evli->light_type != LA_SUN) {
      /* The limit of the power attenuation function when the distance to the light goes to 0 is
       * `2 / r^2` where r is the light radius. We need to find the right radius that emits at most
       * the volume light upper bound. Inverting the function we get: */
      float min_radius = 1.0f / sqrtf(0.5f * upper_bound / power);
      /* Square it here to avoid a multiplication inside the shader. */
      evli->volume_radius = square_f(max_ff(min_radius, evli->radius));
    }
  }

  GPU_uniformbuf_update(sldata->light_ubo, &linfo->light_data);
}
