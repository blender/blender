#pragma BLENDER_REQUIRE(gpu_shader_common_hash.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_material_noise.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_material_fractal_noise.glsl)

float calc_wave(vec3 p,
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
  p = (p + 0.000001) * 0.999999;

  float n;

  if (wave_type == 0) {   /* type bands */
    if (bands_dir == 0) { /* X axis */
      n = p.x * 20.0;
    }
    else if (bands_dir == 1) { /* Y axis */
      n = p.y * 20.0;
    }
    else if (bands_dir == 2) { /* Z axis */
      n = p.z * 20.0;
    }
    else { /* Diagonal axis */
      n = (p.x + p.y + p.z) * 10.0;
    }
  }
  else { /* type rings */
    vec3 rp = p;
    if (rings_dir == 0) { /* X axis */
      rp *= vec3(0.0, 1.0, 1.0);
    }
    else if (rings_dir == 1) { /* Y axis */
      rp *= vec3(1.0, 0.0, 1.0);
    }
    else if (rings_dir == 2) { /* Z axis */
      rp *= vec3(1.0, 1.0, 0.0);
    }
    /* else: Spherical */

    n = length(rp) * 20.0;
  }

  n += phase;

  if (distortion != 0.0) {
    n += distortion *
         (fractal_noise(p * detail_scale, detail, detail_roughness, 2.0, true) * 2.0 - 1.0);
  }

  if (wave_profile == 0) { /* profile sin */
    return 0.5 + 0.5 * sin(n - M_PI_2);
  }
  else if (wave_profile == 1) { /* profile saw */
    n /= 2.0 * M_PI;
    return n - floor(n);
  }
  else { /* profile tri */
    n /= 2.0 * M_PI;
    return abs(n - floor(n + 0.5)) * 2.0;
  }
}

void node_tex_wave(vec3 co,
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
                   out vec4 color,
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

  color = vec4(f, f, f, 1.0);
  fac = f;
}
