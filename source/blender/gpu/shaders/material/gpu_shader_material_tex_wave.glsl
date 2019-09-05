float calc_wave(
    vec3 p, float distortion, float detail, float detail_scale, int wave_type, int wave_profile)
{
  float n;

  if (wave_type == 0) { /* type bands */
    n = (p.x + p.y + p.z) * 10.0;
  }
  else { /* type rings */
    n = length(p) * 20.0;
  }

  if (distortion != 0.0) {
    n += distortion * fractal_noise(p * detail_scale, detail);
  }

  if (wave_profile == 0) { /* profile sin */
    return 0.5 + 0.5 * sin(n);
  }
  else { /* profile saw */
    n /= 2.0 * M_PI;
    n -= int(n);
    return (n < 0.0) ? n + 1.0 : n;
  }
}

void node_tex_wave(vec3 co,
                   float scale,
                   float distortion,
                   float detail,
                   float detail_scale,
                   float wave_type,
                   float wave_profile,
                   out vec4 color,
                   out float fac)
{
  float f;
  f = calc_wave(co * scale, distortion, detail, detail_scale, int(wave_type), int(wave_profile));

  color = vec4(f, f, f, 1.0);
  fac = f;
}
