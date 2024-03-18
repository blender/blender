/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma BLENDER_REQUIRE(draw_model_lib.glsl)

void main()
{
  gl_Position = drw_point_object_to_homogenous(pos);
}
