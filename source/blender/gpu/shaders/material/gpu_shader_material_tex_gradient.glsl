/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

float calc_gradient(float3 p, int gradient_type)
{
  float x, y, z;
  x = p.x;
  y = p.y;
  z = p.z;
  if (gradient_type == 0) { /* linear */
    return x;
  }
  else if (gradient_type == 1) { /* quadratic */
    float r = max(x, 0.0f);
    return r * r;
  }
  else if (gradient_type == 2) { /* easing */
    float r = min(max(x, 0.0f), 1.0f);
    float t = r * r;
    return (3.0f * t - 2.0f * t * r);
  }
  else if (gradient_type == 3) { /* diagonal */
    return (x + y) * 0.5f;
  }
  else if (gradient_type == 4) { /* radial */
    return atan(y, x) / (M_PI * 2) + 0.5f;
  }
  else {
    /* Bias a little bit for the case where p is a unit length vector,
     * to get exactly zero instead of a small random value depending
     * on float precision. */
    float r = max(0.999999f - sqrt(x * x + y * y + z * z), 0.0f);
    if (gradient_type == 5) { /* quadratic sphere */
      return r * r;
    }
    else if (gradient_type == 6) { /* sphere */
      return r;
    }
  }
  return 0.0f;
}

void node_tex_gradient(float3 co, float gradient_type, out float4 color, out float fac)
{
  float f = calc_gradient(co, int(gradient_type));
  f = clamp(f, 0.0f, 1.0f);

  color = float4(f, f, f, 1.0f);
  fac = f;
}
