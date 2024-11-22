/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "select_lib.glsl"

void main()
{
  fragColor = finalColor;
#ifdef LINE_OUTPUT
  lineOutput = vec4(0.0);
#endif
  select_id_output(select_id);
}
