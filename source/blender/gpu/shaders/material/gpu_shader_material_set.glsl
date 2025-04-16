/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void set_value(float val, out float outval)
{
  outval = val;
}

void set_rgb(float3 col, out float3 outcol)
{
  outcol = col;
}

void set_rgba(float4 col, out float4 outcol)
{
  outcol = col;
}

void set_value_zero(out float outval)
{
  outval = 0.0f;
}

void set_value_one(out float outval)
{
  outval = 1.0f;
}

void set_rgb_zero(out float3 outval)
{
  outval = float3(0.0f);
}

void set_rgb_one(out float3 outval)
{
  outval = float3(1.0f);
}

void set_rgba_zero(out float4 outval)
{
  outval = float4(0.0f);
}

void set_rgba_one(out float4 outval)
{
  outval = float4(1.0f);
}
