/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_sky_modal
 */

#ifndef __SKY_MODEL_H__
#define __SKY_MODEL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Nishita improved sky model */

void SKY_nishita_skymodel_precompute_texture(float *pixels,
                                             int stride,
                                             int start_y,
                                             int end_y,
                                             int width,
                                             int height,
                                             float sun_elevation,
                                             float altitude,
                                             float air_density,
                                             float dust_density,
                                             float ozone_density);

void SKY_nishita_skymodel_precompute_sun(float sun_elevation,
                                         float angular_diameter,
                                         float altitude,
                                         float air_density,
                                         float dust_density,
                                         float *r_pixel_bottom,
                                         float *r_pixel_top);

#ifdef __cplusplus
}
#endif

#endif  // __SKY_MODEL_H__
