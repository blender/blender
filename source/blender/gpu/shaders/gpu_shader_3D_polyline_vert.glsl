/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
#ifndef UNIFORM
  interp.final_color = color;
#endif
#ifdef CLIP
  interp.clip = dot(ModelMatrix * vec4(pos, 1.0), ClipPlane);
#endif
}
