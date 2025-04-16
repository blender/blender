/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_hash.glsl"
#include "gpu_shader_material_fractal_noise.glsl"
#include "gpu_shader_material_noise.glsl"

float calc_wave(float3 p,
                float distortion,
                float detail,
                float detail_scale,
                float detail_roughness,
                float phase,
                int wave_type,
                int bands_dir,
                int rings_dir,
                int wave_profile)
{
  /* Prevent precision issues on unit coordinates. */
  p = (p + 0.000001f) * 0.999999f;

  float n;

  if (wave_type == 0) {   /* type bands */
    if (bands_dir == 0) { /* X axis */
      n = p.x * 20.0f;
    }
    else if (bands_dir == 1) { /* Y axis */
      n = p.y * 20.0f;
    }
    else if (bands_dir == 2) { /* Z axis */
      n = p.z * 20.0f;
    }
    else { /* Diagonal axis */
      n = (p.x + p.y + p.z) * 10.0f;
    }
  }
  else { /* type rings */
    float3 rp = p;
    if (rings_dir == 0) { /* X axis */
      rp *= float3(0.0f, 1.0f, 1.0f);
    }
    else if (rings_dir == 1) { /* Y axis */
      rp *= float3(1.0f, 0.0f, 1.0f);
    }
    else if (rings_dir == 2) { /* Z axis */
      rp *= float3(1.0f, 1.0f, 0.0f);
    }
    /* else: Spherical */

    n = length(rp) * 20.0f;
  }

  n += phase;

  if (distortion != 0.0f) {
    n += distortion *
         (noise_fbm(p * detail_scale, detail, detail_roughness, 2.0f, 0.0f, 0.0f, true) * 2.0f -
          1.0f);
  }

  if (wave_profile == 0) { /* profile sin */
    return 0.5f + 0.5f * sin(n - M_PI_2);
  }
  else if (wave_profile == 1) { /* profile saw */
    n /= 2.0f * M_PI;
    return n - floor(n);
  }
  else { /* profile tri */
    n /= 2.0f * M_PI;
    return abs(n - floor(n + 0.5f)) * 2.0f;
  }
}

void node_tex_wave(float3 co,
                   float scale,
                   float distortion,
                   float detail,
                   float detail_scale,
                   float detail_roughness,
                   float phase,
                   float wave_type,
                   float bands_dir,
                   float rings_dir,
                   float wave_profile,
                   out float4 color,
                   out float fac)
{
  float f;
  f = calc_wave(co * scale,
                distortion,
                detail,
                detail_scale,
                detail_roughness,
                phase,
                int(wave_type),
                int(bands_dir),
                int(rings_dir),
                int(wave_profile));

  color = float4(f, f, f, 1.0f);
  fac = f;
}
