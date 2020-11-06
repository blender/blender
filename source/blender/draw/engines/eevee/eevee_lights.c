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
 * \ingroup DNA
 */

#include "BLI_sys_types.h" /* bool */

#include "BKE_object.h"

#include "DEG_depsgraph_query.h"

#include "eevee_private.h"

/* Reconstruct local obmat from EEVEE_light. (normalized) */
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

static float light_attenuation_radius_get(const Light *la, float light_threshold)
{
  if (la->mode & LA_CUSTOM_ATTENUATION) {
    return la->att_dist;
  }

  /* Compute max light power. */
  float power = max_fff(la->r, la->g, la->b);
  power *= fabsf(la->energy / 100.0f);
  power *= max_ff(1.0f, la->spec_fac);
  /* Compute the distance (using the inverse square law)
   * at which the light power reaches the light_threshold. */
  float distance = sqrtf(max_ff(1e-16, power / max_ff(1e-16, light_threshold)));
  return distance;
}

static void light_shape_parameters_set(EEVEE_Light *evli, const Light *la, const float scale[3])
{
  if (la->type == LA_SPOT) {
    /* Spot size & blend */
    evli->sizex = scale[0] / scale[2];
    evli->sizey = scale[1] / scale[2];
    evli->spotsize = cosf(la->spotsize * 0.5f);
    evli->spotblend = (1.0f - evli->spotsize) * la->spotblend;
    evli->radius = max_ff(0.001f, la->area_size);
  }
  else if (la->type == LA_AREA) {
    evli->sizex = max_ff(0.003f, la->area_size * scale[0] * 0.5f);
    if (ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE)) {
      evli->sizey = max_ff(0.003f, la->area_sizey * scale[1] * 0.5f);
    }
    else {
      evli->sizey = max_ff(0.003f, la->area_size * scale[1] * 0.5f);
    }
  }
  else if (la->type == LA_SUN) {
    evli->radius = max_ff(0.001f, tanf(min_ff(la->sun_angle, DEG2RADF(179.9f)) / 2.0f));
  }
  else {
    evli->radius = max_ff(0.001f, la->area_size);
  }
}

static float light_shape_power_get(const Light *la, const EEVEE_Light *evli)
{
  float power;
  /* Make illumination power constant */
  if (la->type == LA_AREA) {
    power = 1.0f / (evli->sizex * evli->sizey * 4.0f * M_PI) * /* 1/(w*h*Pi) */
            0.8f; /* XXX : Empirical, Fit cycles power */
    if (ELEM(la->area_shape, LA_AREA_DISK, LA_AREA_ELLIPSE)) {
      /* Scale power to account for the lower area of the ellipse compared to the surrounding
       * rectangle. */
      power *= 4.0f / M_PI;
    }
  }
  else if (ELEM(la->type, LA_SPOT, LA_LOCAL)) {
    power = 1.0f / (4.0f * evli->radius * evli->radius * M_PI * M_PI); /* 1/(4*r²*Pi²) */

    /* for point lights (a.k.a radius == 0.0) */
    // power = M_PI * M_PI * 0.78; /* XXX : Empirical, Fit cycles power */
  }
  else {
    power = 1.0f / (evli->radius * evli->radius * M_PI); /* 1/(r²*Pi) */
    /* Make illumination power closer to cycles for bigger radii. Cycles uses a cos^3 term that we
     * cannot reproduce so we account for that by scaling the light power. This function is the
     * result of a rough manual fitting. */
    power += 1.0f / (2.0f * M_PI); /* power *= 1 + r²/2 */
  }
  return power;
}

/* Update buffer with light data */
static void eevee_light_setup(Object *ob, EEVEE_Light *evli)
{
  Light *la = (Light *)ob->data;
  float mat[4][4], scale[3], power, att_radius;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const float light_threshold = draw_ctx->scene->eevee.light_threshold;

  /* Position */
  copy_v3_v3(evli->position, ob->obmat[3]);

  /* Color */
  copy_v3_v3(evli->color, &la->r);

  evli->spec = la->spec_fac;

  /* Influence Radius */
  att_radius = light_attenuation_radius_get(la, light_threshold);
  /* Take the inverse square of this distance. */
  evli->invsqrdist = 1.0 / max_ff(1e-4f, att_radius * att_radius);

  /* Vectors */
  normalize_m4_m4_ex(mat, ob->obmat, scale);
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

  power = light_shape_power_get(la, evli);
  mul_v3_fl(evli->color, power * la->energy);

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

void EEVEE_lights_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *UNUSED(vedata))
{
  EEVEE_LightsInfo *linfo = sldata->lights;

  sldata->common_data.la_num_light = linfo->num_light;

  GPU_uniformbuf_update(sldata->light_ubo, &linfo->light_data);
}
