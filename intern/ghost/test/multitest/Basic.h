/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

int min_i(int a, int b);

int max_i(int a, int b);
int clamp_i(int val, int min, int max);

float min_f(float a, float b);
float max_f(float a, float b);
float clamp_f(float val, float min, float max);

void rect_copy(int dst[2][2], int src[2][2]);
int rect_contains_pt(int rect[2][2], int pt[2]);
int rect_width(int rect[2][2]);
int rect_height(int rect[2][2]);
