/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "select_lib.glsl"

void main()
{
  fragColor = finalColor;
  select_id_output(select_id);
}
