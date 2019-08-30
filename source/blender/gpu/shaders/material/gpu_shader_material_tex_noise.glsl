void node_tex_noise(
    vec3 co, float scale, float detail, float distortion, out vec4 color, out float fac)
{
  vec3 p = co * scale;
  int hard = 0;
  if (distortion != 0.0) {
    vec3 r, offset = vec3(13.5, 13.5, 13.5);
    r.x = noise(p + offset) * distortion;
    r.y = noise(p) * distortion;
    r.z = noise(p - offset) * distortion;
    p += r;
  }

  fac = noise_turbulence(p, detail, hard);
  color = vec4(fac,
               noise_turbulence(vec3(p.y, p.x, p.z), detail, hard),
               noise_turbulence(vec3(p.y, p.z, p.x), detail, hard),
               1);
}
