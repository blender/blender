/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void set_value(float val, float &outval)
{
  outval = val;
}

[[node]]
void set_rgb(float3 col, float3 &outcol)
{
  outcol = col;
}

[[node]]
void set_rgba(float4 col, float4 &outcol)
{
  outcol = col;
}

[[node]]
void set_value_zero(float &outval)
{
  outval = 0.0f;
}

[[node]]
void set_value_one(float &outval)
{
  outval = 1.0f;
}

[[node]]
void set_rgb_zero(float3 &outval)
{
  outval = float3(0.0f);
}

[[node]]
void set_rgb_one(float3 &outval)
{
  outval = float3(1.0f);
}

[[node]]
void set_rgba_zero(float4 &outval)
{
  outval = float4(0.0f);
}

[[node]]
void set_rgba_one(float4 &outval)
{
  outval = float4(1.0f);
}
