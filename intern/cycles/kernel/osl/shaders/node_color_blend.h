/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

color node_mix_blend(float t, color col1, color col2)
{
  return mix(col1, col2, t);
}

color node_mix_add(float t, color col1, color col2)
{
  return mix(col1, col1 + col2, t);
}

color node_mix_mul(float t, color col1, color col2)
{
  return mix(col1, col1 * col2, t);
}

color node_mix_screen(float t, color col1, color col2)
{
  float tm = 1.0 - t;

  return color(1.0) - (color(tm) + t * (color(1.0) - col2)) * (color(1.0) - col1);
}

color node_mix_overlay(float t, color col1, color col2)
{
  float tm = 1.0 - t;

  color outcol = col1;

  if (outcol[0] < 0.5)
    outcol[0] *= tm + 2.0 * t * col2[0];
  else
    outcol[0] = 1.0 - (tm + 2.0 * t * (1.0 - col2[0])) * (1.0 - outcol[0]);

  if (outcol[1] < 0.5)
    outcol[1] *= tm + 2.0 * t * col2[1];
  else
    outcol[1] = 1.0 - (tm + 2.0 * t * (1.0 - col2[1])) * (1.0 - outcol[1]);

  if (outcol[2] < 0.5)
    outcol[2] *= tm + 2.0 * t * col2[2];
  else
    outcol[2] = 1.0 - (tm + 2.0 * t * (1.0 - col2[2])) * (1.0 - outcol[2]);

  return outcol;
}

color node_mix_sub(float t, color col1, color col2)
{
  return mix(col1, col1 - col2, t);
}

color node_mix_div(float t, color col1, color col2)
{
  float tm = 1.0 - t;

  color outcol = col1;

  if (col2[0] != 0.0)
    outcol[0] = tm * outcol[0] + t * outcol[0] / col2[0];
  if (col2[1] != 0.0)
    outcol[1] = tm * outcol[1] + t * outcol[1] / col2[1];
  if (col2[2] != 0.0)
    outcol[2] = tm * outcol[2] + t * outcol[2] / col2[2];

  return outcol;
}

color node_mix_diff(float t, color col1, color col2)
{
  return mix(col1, abs(col1 - col2), t);
}

color node_mix_exclusion(float t, color col1, color col2)
{
  return max(mix(col1, col1 + col2 - 2.0 * col1 * col2, t), 0.0);
}

color node_mix_dark(float t, color col1, color col2)
{
  return mix(col1, min(col1, col2), t);
}

color node_mix_light(float t, color col1, color col2)
{
  return mix(col1, max(col1, col2), t);
}

color node_mix_dodge(float t, color col1, color col2)
{
  color outcol = col1;

  if (outcol[0] != 0.0) {
    float tmp = 1.0 - t * col2[0];
    if (tmp <= 0.0)
      outcol[0] = 1.0;
    else if ((tmp = outcol[0] / tmp) > 1.0)
      outcol[0] = 1.0;
    else
      outcol[0] = tmp;
  }
  if (outcol[1] != 0.0) {
    float tmp = 1.0 - t * col2[1];
    if (tmp <= 0.0)
      outcol[1] = 1.0;
    else if ((tmp = outcol[1] / tmp) > 1.0)
      outcol[1] = 1.0;
    else
      outcol[1] = tmp;
  }
  if (outcol[2] != 0.0) {
    float tmp = 1.0 - t * col2[2];
    if (tmp <= 0.0)
      outcol[2] = 1.0;
    else if ((tmp = outcol[2] / tmp) > 1.0)
      outcol[2] = 1.0;
    else
      outcol[2] = tmp;
  }

  return outcol;
}

color node_mix_burn(float t, color col1, color col2)
{
  float tmp, tm = 1.0 - t;

  color outcol = col1;

  tmp = tm + t * col2[0];
  if (tmp <= 0.0)
    outcol[0] = 0.0;
  else if ((tmp = (1.0 - (1.0 - outcol[0]) / tmp)) < 0.0)
    outcol[0] = 0.0;
  else if (tmp > 1.0)
    outcol[0] = 1.0;
  else
    outcol[0] = tmp;

  tmp = tm + t * col2[1];
  if (tmp <= 0.0)
    outcol[1] = 0.0;
  else if ((tmp = (1.0 - (1.0 - outcol[1]) / tmp)) < 0.0)
    outcol[1] = 0.0;
  else if (tmp > 1.0)
    outcol[1] = 1.0;
  else
    outcol[1] = tmp;

  tmp = tm + t * col2[2];
  if (tmp <= 0.0)
    outcol[2] = 0.0;
  else if ((tmp = (1.0 - (1.0 - outcol[2]) / tmp)) < 0.0)
    outcol[2] = 0.0;
  else if (tmp > 1.0)
    outcol[2] = 1.0;
  else
    outcol[2] = tmp;

  return outcol;
}

color node_mix_hue(float t, color col1, color col2)
{
  color outcol = col1;
  color hsv2 = rgb_to_hsv(col2);

  if (hsv2[1] != 0.0) {
    color hsv = rgb_to_hsv(outcol);
    hsv[0] = hsv2[0];
    color tmp = hsv_to_rgb(hsv);

    outcol = mix(outcol, tmp, t);
  }

  return outcol;
}

color node_mix_sat(float t, color col1, color col2)
{
  float tm = 1.0 - t;

  color outcol = col1;

  color hsv = rgb_to_hsv(outcol);

  if (hsv[1] != 0.0) {
    color hsv2 = rgb_to_hsv(col2);

    hsv[1] = tm * hsv[1] + t * hsv2[1];
    outcol = hsv_to_rgb(hsv);
  }

  return outcol;
}

color node_mix_val(float t, color col1, color col2)
{
  float tm = 1.0 - t;

  color hsv = rgb_to_hsv(col1);
  color hsv2 = rgb_to_hsv(col2);

  hsv[2] = tm * hsv[2] + t * hsv2[2];

  return hsv_to_rgb(hsv);
}

color node_mix_color(float t, color col1, color col2)
{
  color outcol = col1;
  color hsv2 = rgb_to_hsv(col2);

  if (hsv2[1] != 0.0) {
    color hsv = rgb_to_hsv(outcol);
    hsv[0] = hsv2[0];
    hsv[1] = hsv2[1];
    color tmp = hsv_to_rgb(hsv);

    outcol = mix(outcol, tmp, t);
  }

  return outcol;
}

color node_mix_soft(float t, color col1, color col2)
{
  float tm = 1.0 - t;

  color one = color(1.0);
  color scr = one - (one - col2) * (one - col1);

  return tm * col1 + t * ((one - col1) * col2 * col1 + col1 * scr);
}

color node_mix_linear(float t, color col1, color col2)
{
  color outcol = col1;

  if (col2[0] > 0.5)
    outcol[0] = col1[0] + t * (2.0 * (col2[0] - 0.5));
  else
    outcol[0] = col1[0] + t * (2.0 * (col2[0]) - 1.0);

  if (col2[1] > 0.5)
    outcol[1] = col1[1] + t * (2.0 * (col2[1] - 0.5));
  else
    outcol[1] = col1[1] + t * (2.0 * (col2[1]) - 1.0);

  if (col2[2] > 0.5)
    outcol[2] = col1[2] + t * (2.0 * (col2[2] - 0.5));
  else
    outcol[2] = col1[2] + t * (2.0 * (col2[2]) - 1.0);

  return outcol;
}

color node_mix_clamp(color col)
{
  color outcol = col;

  outcol[0] = clamp(col[0], 0.0, 1.0);
  outcol[1] = clamp(col[1], 0.0, 1.0);
  outcol[2] = clamp(col[2], 0.0, 1.0);

  return outcol;
}
