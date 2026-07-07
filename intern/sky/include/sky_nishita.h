/* SPDX-FileCopyrightText: 2020-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_sky_modal
 */

#pragma once

void SKY_single_scattering_precompute_texture(float *pixels,
                                              int stride,
                                              int width,
                                              int height,
                                              float sun_elevation,
                                              float altitude,
                                              float air_density,
                                              float aerosol_density,
                                              float ozone_density);

void SKY_single_scattering_precompute_sun(float sun_elevation,
                                          float angular_diameter,
                                          float altitude,
                                          float air_density,
                                          float aerosol_density,
                                          float r_pixel_bottom[3],
                                          float r_pixel_top[3]);

void SKY_multiple_scattering_precompute_texture(float *pixels,
                                                int stride,
                                                int width,
                                                int height,
                                                float sun_elevation,
                                                float altitude,
                                                float air_density,
                                                float aerosol_density,
                                                float ozone_density);

void SKY_multiple_scattering_precompute_sun(float sun_elevation,
                                            float angular_diameter,
                                            float altitude,
                                            float air_density,
                                            float aerosol_density,
                                            float ozone_density,
                                            float r_pixel_bottom[3],
                                            float r_pixel_top[3]);

float SKY_earth_intersection_angle(float altitude);
