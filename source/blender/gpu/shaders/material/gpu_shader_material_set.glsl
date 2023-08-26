/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void set_value(float val, out float outval)
{
  outval = val;
}

void set_rgb(vec3 col, out vec3 outcol)
{
  outcol = col;
}

void set_rgba(vec4 col, out vec4 outcol)
{
  outcol = col;
}

void set_value_zero(out float outval)
{
  outval = 0.0;
}

void set_value_one(out float outval)
{
  outval = 1.0;
}

void set_rgb_zero(out vec3 outval)
{
  outval = vec3(0.0);
}

void set_rgb_one(out vec3 outval)
{
  outval = vec3(1.0);
}

void set_rgba_zero(out vec4 outval)
{
  outval = vec4(0.0);
}

void set_rgba_one(out vec4 outval)
{
  outval = vec4(1.0);
}
