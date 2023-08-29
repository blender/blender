/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/* ** YCCA to RGBA ** */

void ycca_to_rgba_itu_601(vec4 ycca, out vec4 color)
{
  ycca.xyz *= 255.0;
  ycca.xyz -= vec3(16.0, 128.0, 128.0);
  color.rgb = mat3(1.164, 1.164, 1.164, 0.0, -0.392, 2.017, 1.596, -0.813, 0.0) * ycca.xyz;
  color.rgb /= 255.0;
  color.a = ycca.a;
}

void ycca_to_rgba_itu_709(vec4 ycca, out vec4 color)
{
  ycca.xyz *= 255.0;
  ycca.xyz -= vec3(16.0, 128.0, 128.0);
  color.rgb = mat3(1.164, 1.164, 1.164, 0.0, -0.213, 2.115, 1.793, -0.534, 0.0) * ycca.xyz;
  color.rgb /= 255.0;
  color.a = ycca.a;
}

void ycca_to_rgba_jpeg(vec4 ycca, out vec4 color)
{
  ycca.xyz *= 255.0;
  color.rgb = mat3(1.0, 1.0, 1.0, 0.0, -0.34414, 1.772, 1.402, -0.71414, 0.0) * ycca.xyz;
  color.rgb += vec3(-179.456, 135.45984, -226.816);
  color.rgb /= 255.0;
  color.a = ycca.a;
}

/* ** RGBA to YCCA ** */

void rgba_to_ycca_itu_601(vec4 rgba, out vec4 ycca)
{
  rgba.rgb *= 255.0;
  ycca.xyz = mat3(0.257, -0.148, 0.439, 0.504, -0.291, -0.368, 0.098, 0.439, -0.071) * rgba.rgb;
  ycca.xyz += vec3(16.0, 128.0, 128.0);
  ycca.xyz /= 255.0;
  ycca.a = rgba.a;
}

void rgba_to_ycca_itu_709(vec4 rgba, out vec4 ycca)
{
  rgba.rgb *= 255.0;
  ycca.xyz = mat3(0.183, -0.101, 0.439, 0.614, -0.338, -0.399, 0.062, 0.439, -0.040) * rgba.rgb;
  ycca.xyz += vec3(16.0, 128.0, 128.0);
  ycca.xyz /= 255.0;
  ycca.a = rgba.a;
}

void rgba_to_ycca_jpeg(vec4 rgba, out vec4 ycca)
{
  rgba.rgb *= 255.0;
  ycca.xyz = mat3(0.299, -0.16874, 0.5, 0.587, -0.33126, -0.41869, 0.114, 0.5, -0.08131) *
             rgba.rgb;
  ycca.xyz += vec3(0.0, 128.0, 128.0);
  ycca.xyz /= 255.0;
  ycca.a = rgba.a;
}

/* ** YUVA to RGBA ** */

void yuva_to_rgba_itu_709(vec4 yuva, out vec4 color)
{
  color.rgb = mat3(1.0, 1.0, 1.0, 0.0, -0.21482, 2.12798, 1.28033, -0.38059, 0.0) * yuva.xyz;
  color.a = yuva.a;
}

/* ** RGBA to YUVA ** */

void rgba_to_yuva_itu_709(vec4 rgba, out vec4 yuva)
{
  yuva.xyz = mat3(0.2126, -0.09991, 0.615, 0.7152, -0.33609, -0.55861, 0.0722, 0.436, -0.05639) *
             rgba.rgb;
  yuva.a = rgba.a;
}

/* ** Alpha Handling ** */

void color_alpha_clear(vec4 color, out vec4 result)
{
  result = vec4(color.rgb, 1.0);
}

void color_alpha_premultiply(vec4 color, out vec4 result)
{
  result = vec4(color.rgb * color.a, color.a);
}

void color_alpha_unpremultiply(vec4 color, out vec4 result)
{
  if (color.a == 0.0 || color.a == 1.0) {
    result = color;
  }
  else {
    result = vec4(color.rgb / color.a, color.a);
  }
}

float linear_rgb_to_srgb(float color)
{
  if (color < 0.0031308) {
    return (color < 0.0) ? 0.0 : color * 12.92;
  }

  return 1.055 * pow(color, 1.0 / 2.4) - 0.055;
}

vec3 linear_rgb_to_srgb(vec3 color)
{
  return vec3(
      linear_rgb_to_srgb(color.r), linear_rgb_to_srgb(color.g), linear_rgb_to_srgb(color.b));
}

float srgb_to_linear_rgb(float color)
{
  if (color < 0.04045) {
    return (color < 0.0) ? 0.0 : color * (1.0 / 12.92);
  }

  return pow((color + 0.055) * (1.0 / 1.055), 2.4);
}

vec3 srgb_to_linear_rgb(vec3 color)
{
  return vec3(
      srgb_to_linear_rgb(color.r), srgb_to_linear_rgb(color.g), srgb_to_linear_rgb(color.b));
}

float get_luminance(vec3 color, vec3 luminance_coefficients)
{
  return dot(color, luminance_coefficients);
}
