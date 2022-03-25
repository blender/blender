/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation. All rights reserved. */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

void BLI_bicubic_interpolation_fl(
    const float *buffer, float *output, int width, int height, int components, float u, float v);

void BLI_bicubic_interpolation_char(const unsigned char *buffer,
                                    unsigned char *output,
                                    int width,
                                    int height,
                                    int components,
                                    float u,
                                    float v);

void BLI_bilinear_interpolation_fl(
    const float *buffer, float *output, int width, int height, int components, float u, float v);

void BLI_bilinear_interpolation_char(const unsigned char *buffer,
                                     unsigned char *output,
                                     int width,
                                     int height,
                                     int components,
                                     float u,
                                     float v);

void BLI_bilinear_interpolation_wrap_fl(const float *buffer,
                                        float *output,
                                        int width,
                                        int height,
                                        int components,
                                        float u,
                                        float v,
                                        bool wrap_x,
                                        bool wrap_y);

void BLI_bilinear_interpolation_wrap_char(const unsigned char *buffer,
                                          unsigned char *output,
                                          int width,
                                          int height,
                                          int components,
                                          float u,
                                          float v,
                                          bool wrap_x,
                                          bool wrap_y);

#define EWA_MAXIDX 255
extern const float EWA_WTS[EWA_MAXIDX + 1];

typedef void (*ewa_filter_read_pixel_cb)(void *userdata, int x, int y, float result[4]);

void BLI_ewa_imp2radangle(
    float A, float B, float C, float F, float *a, float *b, float *th, float *ecc);

/**
 * TODO(sergey): Consider making this function inlined, so the pixel read callback
 * could also be inlined in order to avoid per-pixel function calls.
 */
void BLI_ewa_filter(int width,
                    int height,
                    bool intpol,
                    bool use_alpha,
                    const float uv[2],
                    const float du[2],
                    const float dv[2],
                    ewa_filter_read_pixel_cb read_pixel_cb,
                    void *userdata,
                    float result[4]);

#ifdef __cplusplus
}
#endif
