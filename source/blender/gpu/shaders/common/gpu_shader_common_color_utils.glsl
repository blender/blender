void rgb_to_hsv(vec4 rgb, out vec4 outcol)
{
  float cmax, cmin, h, s, v, cdelta;
  vec3 c;

  cmax = max(rgb[0], max(rgb[1], rgb[2]));
  cmin = min(rgb[0], min(rgb[1], rgb[2]));
  cdelta = cmax - cmin;

  v = cmax;
  if (cmax != 0.0) {
    s = cdelta / cmax;
  }
  else {
    s = 0.0;
    h = 0.0;
  }

  if (s == 0.0) {
    h = 0.0;
  }
  else {
    c = (vec3(cmax) - rgb.xyz) / cdelta;

    if (rgb.x == cmax) {
      h = c[2] - c[1];
    }
    else if (rgb.y == cmax) {
      h = 2.0 + c[0] - c[2];
    }
    else {
      h = 4.0 + c[1] - c[0];
    }

    h /= 6.0;

    if (h < 0.0) {
      h += 1.0;
    }
  }

  outcol = vec4(h, s, v, rgb.w);
}

void hsv_to_rgb(vec4 hsv, out vec4 outcol)
{
  float i, f, p, q, t, h, s, v;
  vec3 rgb;

  h = hsv[0];
  s = hsv[1];
  v = hsv[2];

  if (s == 0.0) {
    rgb = vec3(v, v, v);
  }
  else {
    if (h == 1.0) {
      h = 0.0;
    }

    h *= 6.0;
    i = floor(h);
    f = h - i;
    rgb = vec3(f, f, f);
    p = v * (1.0 - s);
    q = v * (1.0 - (s * f));
    t = v * (1.0 - (s * (1.0 - f)));

    if (i == 0.0) {
      rgb = vec3(v, t, p);
    }
    else if (i == 1.0) {
      rgb = vec3(q, v, p);
    }
    else if (i == 2.0) {
      rgb = vec3(p, v, t);
    }
    else if (i == 3.0) {
      rgb = vec3(p, q, v);
    }
    else if (i == 4.0) {
      rgb = vec3(t, p, v);
    }
    else {
      rgb = vec3(v, p, q);
    }
  }

  outcol = vec4(rgb, hsv.w);
}

void rgb_to_hsl(vec4 rgb, out vec4 outcol)
{
  float cmax, cmin, h, s, l;

  cmax = max(rgb[0], max(rgb[1], rgb[2]));
  cmin = min(rgb[0], min(rgb[1], rgb[2]));
  l = min(1.0, (cmax + cmin) / 2.0);

  if (cmax == cmin) {
    h = s = 0.0; /* achromatic */
  }
  else {
    float cdelta = cmax - cmin;
    s = l > 0.5 ? cdelta / (2.0 - cmax - cmin) : cdelta / (cmax + cmin);
    if (cmax == rgb[0]) {
      h = (rgb[1] - rgb[2]) / cdelta + (rgb[1] < rgb[2] ? 6.0 : 0.0);
    }
    else if (cmax == rgb[1]) {
      h = (rgb[2] - rgb[0]) / cdelta + 2.0;
    }
    else {
      h = (rgb[0] - rgb[1]) / cdelta + 4.0;
    }
  }
  h /= 6.0;

  outcol = vec4(h, s, l, rgb.w);
}

void hsl_to_rgb(vec4 hsl, out vec4 outcol)
{
  float nr, ng, nb, chroma, h, s, l;

  h = hsl[0];
  s = hsl[1];
  l = hsl[2];

  nr = abs(h * 6.0 - 3.0) - 1.0;
  ng = 2.0 - abs(h * 6.0 - 2.0);
  nb = 2.0 - abs(h * 6.0 - 4.0);

  nr = clamp(nr, 0.0, 1.0);
  nb = clamp(nb, 0.0, 1.0);
  ng = clamp(ng, 0.0, 1.0);

  chroma = (1.0 - abs(2.0 * l - 1.0)) * s;

  outcol = vec4((nr - 0.5) * chroma + l, (ng - 0.5) * chroma + l, (nb - 0.5) * chroma + l, hsl.w);
}

void color_alpha_clear(vec4 color, out vec4 result)
{
  result = vec4(color.rgb, 1.0);
}

void color_alpha_premultiply(vec4 color, out vec4 result)
{
  result = vec4(color.rgb * color.a, 1.0);
}

void color_alpha_unpremultiply(vec4 color, out vec4 result)
{
  if (color.a == 0.0 || color.a == 1.0) {
    result = vec4(color.rgb, 1.0);
  }
  else {
    result = vec4(color.rgb / color.a, 1.0);
  }
}
