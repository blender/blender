/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* Kahan summation, Same as +=, but reduces the numerical error when summing a sequence of numbers,
 * by keeping a running compensation. */
struct KahanSum {
  /* Running compensation for lost lower-order bits. */
  float c;
  /* Final sum. */
  float sum;

  void add(const float value)
  {
    const float y = value - c;
    const float t = sum + y;
    c = (t - sum) - y;
    sum = t;
  }
};
