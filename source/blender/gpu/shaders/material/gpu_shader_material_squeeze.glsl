/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void squeeze(float val, float width, float center, out float outval)
{
  outval = 1.0f / (1.0f + pow(2.71828183f, -((val - center) * width)));
}
