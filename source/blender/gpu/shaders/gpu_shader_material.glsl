
/* Converters */

float convert_rgba_to_float(vec4 color)
{
  return dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
}

float exp_blender(float f)
{
  return pow(2.71828182846, f);
}

float compatible_pow(float x, float y)
{
  if (y == 0.0) { /* x^0 -> 1, including 0^0 */
    return 1.0;
  }

  /* glsl pow doesn't accept negative x */
  if (x < 0.0) {
    if (mod(-y, 2.0) == 0.0) {
      return pow(-x, y);
    }
    else {
      return -pow(-x, y);
    }
  }
  else if (x == 0.0) {
    return 0.0;
  }

  return pow(x, y);
}

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

void color_to_normal_new_shading(vec3 color, out vec3 normal)
{
  normal = vec3(2.0) * color - vec3(1.0);
}

void color_to_blender_normal_new_shading(vec3 color, out vec3 normal)
{
  normal = vec3(2.0, -2.0, -2.0) * color - vec3(1.0);
}

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif
#ifndef M_1_PI
#  define M_1_PI 0.318309886183790671538
#endif

/*********** SHADER NODES ***************/

void particle_info(vec4 sprops,
                   vec4 loc,
                   vec3 vel,
                   vec3 avel,
                   out float index,
                   out float random,
                   out float age,
                   out float life_time,
                   out vec3 location,
                   out float size,
                   out vec3 velocity,
                   out vec3 angular_velocity)
{
  index = sprops.x;
  random = loc.w;
  age = sprops.y;
  life_time = sprops.z;
  size = sprops.w;

  location = loc.xyz;
  velocity = vel;
  angular_velocity = avel;
}

void vect_normalize(vec3 vin, out vec3 vout)
{
  vout = normalize(vin);
}

void direction_transform_m4v3(vec3 vin, mat4 mat, out vec3 vout)
{
  vout = (mat * vec4(vin, 0.0)).xyz;
}

void normal_transform_transposed_m4v3(vec3 vin, mat4 mat, out vec3 vout)
{
  vout = transpose(mat3(mat)) * vin;
}

void point_transform_m4v3(vec3 vin, mat4 mat, out vec3 vout)
{
  vout = (mat * vec4(vin, 1.0)).xyz;
}

void point_texco_remap_square(vec3 vin, out vec3 vout)
{
  vout = vin * 2.0 - 1.0;
}

void point_texco_clamp(vec3 vin, sampler2D ima, out vec3 vout)
{
  vec2 half_texel_size = 0.5 / vec2(textureSize(ima, 0).xy);
  vout = clamp(vin, half_texel_size.xyy, 1.0 - half_texel_size.xyy);
}

void point_map_to_sphere(vec3 vin, out vec3 vout)
{
  float len = length(vin);
  float v, u;
  if (len > 0.0) {
    if (vin.x == 0.0 && vin.y == 0.0) {
      u = 0.0;
    }
    else {
      u = (1.0 - atan(vin.x, vin.y) / M_PI) / 2.0;
    }

    v = 1.0 - acos(vin.z / len) / M_PI;
  }
  else {
    v = u = 0.0;
  }

  vout = vec3(u, v, 0.0);
}

void point_map_to_tube(vec3 vin, out vec3 vout)
{
  float u, v;
  v = (vin.z + 1.0) * 0.5;
  float len = sqrt(vin.x * vin.x + vin.y * vin[1]);
  if (len > 0.0) {
    u = (1.0 - (atan(vin.x / len, vin.y / len) / M_PI)) * 0.5;
  }
  else {
    v = u = 0.0;
  }

  vout = vec3(u, v, 0.0);
}

void mapping(
    vec3 vec, vec4 m0, vec4 m1, vec4 m2, vec4 m3, vec3 minvec, vec3 maxvec, out vec3 outvec)
{
  mat4 mat = mat4(m0, m1, m2, m3);
  outvec = (mat * vec4(vec, 1.0)).xyz;
  outvec = clamp(outvec, minvec, maxvec);
}

void camera(vec3 co, out vec3 outview, out float outdepth, out float outdist)
{
  outdepth = abs(co.z);
  outdist = length(co);
  outview = normalize(co);
}

void math_add(float val1, float val2, out float outval)
{
  outval = val1 + val2;
}

void math_subtract(float val1, float val2, out float outval)
{
  outval = val1 - val2;
}

void math_multiply(float val1, float val2, out float outval)
{
  outval = val1 * val2;
}

void math_divide(float val1, float val2, out float outval)
{
  if (val2 == 0.0) {
    outval = 0.0;
  }
  else {
    outval = val1 / val2;
  }
}

void math_sine(float val, out float outval)
{
  outval = sin(val);
}

void math_cosine(float val, out float outval)
{
  outval = cos(val);
}

void math_tangent(float val, out float outval)
{
  outval = tan(val);
}

void math_asin(float val, out float outval)
{
  if (val <= 1.0 && val >= -1.0) {
    outval = asin(val);
  }
  else {
    outval = 0.0;
  }
}

void math_acos(float val, out float outval)
{
  if (val <= 1.0 && val >= -1.0) {
    outval = acos(val);
  }
  else {
    outval = 0.0;
  }
}

void math_atan(float val, out float outval)
{
  outval = atan(val);
}

void math_pow(float val1, float val2, out float outval)
{
  if (val1 >= 0.0) {
    outval = compatible_pow(val1, val2);
  }
  else {
    float val2_mod_1 = mod(abs(val2), 1.0);

    if (val2_mod_1 > 0.999 || val2_mod_1 < 0.001) {
      outval = compatible_pow(val1, floor(val2 + 0.5));
    }
    else {
      outval = 0.0;
    }
  }
}

void math_log(float val1, float val2, out float outval)
{
  if (val1 > 0.0 && val2 > 0.0) {
    outval = log2(val1) / log2(val2);
  }
  else {
    outval = 0.0;
  }
}

void math_max(float val1, float val2, out float outval)
{
  outval = max(val1, val2);
}

void math_min(float val1, float val2, out float outval)
{
  outval = min(val1, val2);
}

void math_round(float val, out float outval)
{
  outval = floor(val + 0.5);
}

void math_less_than(float val1, float val2, out float outval)
{
  if (val1 < val2) {
    outval = 1.0;
  }
  else {
    outval = 0.0;
  }
}

void math_greater_than(float val1, float val2, out float outval)
{
  if (val1 > val2) {
    outval = 1.0;
  }
  else {
    outval = 0.0;
  }
}

void math_modulo(float val1, float val2, out float outval)
{
  if (val2 == 0.0) {
    outval = 0.0;
  }
  else {
    outval = mod(val1, val2);
  }

  /* change sign to match C convention, mod in GLSL will take absolute for negative numbers,
   * see https://www.opengl.org/sdk/docs/man/html/mod.xhtml */
  outval = (val1 > 0.0) ? outval : outval - val2;
}

void math_abs(float val1, out float outval)
{
  outval = abs(val1);
}

void math_atan2(float val1, float val2, out float outval)
{
  outval = atan(val1, val2);
}

void math_floor(float val, out float outval)
{
  outval = floor(val);
}

void math_ceil(float val, out float outval)
{
  outval = ceil(val);
}

void math_fract(float val, out float outval)
{
  outval = val - floor(val);
}

void math_sqrt(float val, out float outval)
{
  if (val > 0.0) {
    outval = sqrt(val);
  }
  else {
    outval = 0.0;
  }
}

void squeeze(float val, float width, float center, out float outval)
{
  outval = 1.0 / (1.0 + pow(2.71828183, -((val - center) * width)));
}

void vec_math_add(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
  outvec = v1 + v2;
  outval = (abs(outvec[0]) + abs(outvec[1]) + abs(outvec[2])) * 0.333333;
}

void vec_math_sub(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
  outvec = v1 - v2;
  outval = (abs(outvec[0]) + abs(outvec[1]) + abs(outvec[2])) * 0.333333;
}

void vec_math_average(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
  outvec = v1 + v2;
  outval = length(outvec);
  outvec = normalize(outvec);
}
void vec_math_mix(float strength, vec3 v1, vec3 v2, out vec3 outvec)
{
  outvec = strength * v1 + (1 - strength) * v2;
}

void vec_math_dot(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
  outvec = vec3(0);
  outval = dot(v1, v2);
}

void vec_math_cross(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
  outvec = cross(v1, v2);
  outval = length(outvec);
  outvec /= outval;
}

void vec_math_normalize(vec3 v, out vec3 outvec, out float outval)
{
  outval = length(v);
  outvec = normalize(v);
}

void vec_math_negate(vec3 v, out vec3 outv)
{
  outv = -v;
}

void invert_z(vec3 v, out vec3 outv)
{
  v.z = -v.z;
  outv = v;
}

void normal_new_shading(vec3 nor, vec3 dir, out vec3 outnor, out float outdot)
{
  outnor = dir;
  outdot = dot(normalize(nor), dir);
}

void curves_vec(float fac, vec3 vec, sampler1DArray curvemap, float layer, out vec3 outvec)
{
  vec4 co = vec4(vec * 0.5 + 0.5, layer);
  outvec.x = texture(curvemap, co.xw).x;
  outvec.y = texture(curvemap, co.yw).y;
  outvec.z = texture(curvemap, co.zw).z;
  outvec = mix(vec, outvec, fac);
}

/* ext is vec4(in_x, in_dy, out_x, out_dy). */
float curve_extrapolate(float x, float y, vec4 ext)
{
  if (x < 0.0) {
    return y + x * ext.y;
  }
  else if (x > 1.0) {
    return y + (x - 1.0) * ext.w;
  }
  else {
    return y;
  }
}

#define RANGE_RESCALE(x, min, range) ((x - min) * range)

void curves_rgb(float fac,
                vec4 col,
                sampler1DArray curvemap,
                float layer,
                vec4 range,
                vec4 ext_r,
                vec4 ext_g,
                vec4 ext_b,
                vec4 ext_a,
                out vec4 outcol)
{
  vec4 co = vec4(RANGE_RESCALE(col.rgb, ext_a.x, range.a), layer);
  vec3 samp;
  samp.r = texture(curvemap, co.xw).a;
  samp.g = texture(curvemap, co.yw).a;
  samp.b = texture(curvemap, co.zw).a;

  samp.r = curve_extrapolate(co.x, samp.r, ext_a);
  samp.g = curve_extrapolate(co.y, samp.g, ext_a);
  samp.b = curve_extrapolate(co.z, samp.b, ext_a);

  vec3 rgb_min = vec3(ext_r.x, ext_g.x, ext_b.x);
  co.xyz = RANGE_RESCALE(samp.rgb, rgb_min, range.rgb);

  samp.r = texture(curvemap, co.xw).r;
  samp.g = texture(curvemap, co.yw).g;
  samp.b = texture(curvemap, co.zw).b;

  outcol.r = curve_extrapolate(co.x, samp.r, ext_r);
  outcol.g = curve_extrapolate(co.y, samp.g, ext_g);
  outcol.b = curve_extrapolate(co.z, samp.b, ext_b);
  outcol.a = col.a;

  outcol = mix(col, outcol, fac);
}

void curves_rgb_opti(float fac,
                     vec4 col,
                     sampler1DArray curvemap,
                     float layer,
                     vec4 range,
                     vec4 ext_a,
                     out vec4 outcol)
{
  vec4 co = vec4(RANGE_RESCALE(col.rgb, ext_a.x, range.a), layer);
  vec3 samp;
  samp.r = texture(curvemap, co.xw).a;
  samp.g = texture(curvemap, co.yw).a;
  samp.b = texture(curvemap, co.zw).a;

  outcol.r = curve_extrapolate(co.x, samp.r, ext_a);
  outcol.g = curve_extrapolate(co.y, samp.g, ext_a);
  outcol.b = curve_extrapolate(co.z, samp.b, ext_a);
  outcol.a = col.a;

  outcol = mix(col, outcol, fac);
}

void set_value(float val, out float outval)
{
  outval = val;
}

void set_rgb(vec3 col, out vec3 outcol)
{
  outcol = col;
}

void set_rgba(vec4 col, out vec4 outcol)
{
  outcol = col;
}

void set_value_zero(out float outval)
{
  outval = 0.0;
}

void set_value_one(out float outval)
{
  outval = 1.0;
}

void set_rgb_zero(out vec3 outval)
{
  outval = vec3(0.0);
}

void set_rgb_one(out vec3 outval)
{
  outval = vec3(1.0);
}

void set_rgba_zero(out vec4 outval)
{
  outval = vec4(0.0);
}

void set_rgba_one(out vec4 outval)
{
  outval = vec4(1.0);
}

void brightness_contrast(vec4 col, float brightness, float contrast, out vec4 outcol)
{
  float a = 1.0 + contrast;
  float b = brightness - contrast * 0.5;

  outcol.r = max(a * col.r + b, 0.0);
  outcol.g = max(a * col.g + b, 0.0);
  outcol.b = max(a * col.b + b, 0.0);
  outcol.a = col.a;
}

void mix_blend(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  outcol = mix(col1, col2, fac);
  outcol.a = col1.a;
}

void mix_add(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  outcol = mix(col1, col1 + col2, fac);
  outcol.a = col1.a;
}

void mix_mult(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  outcol = mix(col1, col1 * col2, fac);
  outcol.a = col1.a;
}

void mix_screen(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  float facm = 1.0 - fac;

  outcol = vec4(1.0) - (vec4(facm) + fac * (vec4(1.0) - col2)) * (vec4(1.0) - col1);
  outcol.a = col1.a;
}

void mix_overlay(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  float facm = 1.0 - fac;

  outcol = col1;

  if (outcol.r < 0.5) {
    outcol.r *= facm + 2.0 * fac * col2.r;
  }
  else {
    outcol.r = 1.0 - (facm + 2.0 * fac * (1.0 - col2.r)) * (1.0 - outcol.r);
  }

  if (outcol.g < 0.5) {
    outcol.g *= facm + 2.0 * fac * col2.g;
  }
  else {
    outcol.g = 1.0 - (facm + 2.0 * fac * (1.0 - col2.g)) * (1.0 - outcol.g);
  }

  if (outcol.b < 0.5) {
    outcol.b *= facm + 2.0 * fac * col2.b;
  }
  else {
    outcol.b = 1.0 - (facm + 2.0 * fac * (1.0 - col2.b)) * (1.0 - outcol.b);
  }
}

void mix_sub(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  outcol = mix(col1, col1 - col2, fac);
  outcol.a = col1.a;
}

void mix_div(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  float facm = 1.0 - fac;

  outcol = col1;

  if (col2.r != 0.0) {
    outcol.r = facm * outcol.r + fac * outcol.r / col2.r;
  }
  if (col2.g != 0.0) {
    outcol.g = facm * outcol.g + fac * outcol.g / col2.g;
  }
  if (col2.b != 0.0) {
    outcol.b = facm * outcol.b + fac * outcol.b / col2.b;
  }
}

void mix_diff(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  outcol = mix(col1, abs(col1 - col2), fac);
  outcol.a = col1.a;
}

void mix_dark(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  outcol.rgb = min(col1.rgb, col2.rgb * fac);
  outcol.a = col1.a;
}

void mix_light(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  outcol.rgb = max(col1.rgb, col2.rgb * fac);
  outcol.a = col1.a;
}

void mix_dodge(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  outcol = col1;

  if (outcol.r != 0.0) {
    float tmp = 1.0 - fac * col2.r;
    if (tmp <= 0.0) {
      outcol.r = 1.0;
    }
    else if ((tmp = outcol.r / tmp) > 1.0) {
      outcol.r = 1.0;
    }
    else {
      outcol.r = tmp;
    }
  }
  if (outcol.g != 0.0) {
    float tmp = 1.0 - fac * col2.g;
    if (tmp <= 0.0) {
      outcol.g = 1.0;
    }
    else if ((tmp = outcol.g / tmp) > 1.0) {
      outcol.g = 1.0;
    }
    else {
      outcol.g = tmp;
    }
  }
  if (outcol.b != 0.0) {
    float tmp = 1.0 - fac * col2.b;
    if (tmp <= 0.0) {
      outcol.b = 1.0;
    }
    else if ((tmp = outcol.b / tmp) > 1.0) {
      outcol.b = 1.0;
    }
    else {
      outcol.b = tmp;
    }
  }
}

void mix_burn(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  float tmp, facm = 1.0 - fac;

  outcol = col1;

  tmp = facm + fac * col2.r;
  if (tmp <= 0.0) {
    outcol.r = 0.0;
  }
  else if ((tmp = (1.0 - (1.0 - outcol.r) / tmp)) < 0.0) {
    outcol.r = 0.0;
  }
  else if (tmp > 1.0) {
    outcol.r = 1.0;
  }
  else {
    outcol.r = tmp;
  }

  tmp = facm + fac * col2.g;
  if (tmp <= 0.0) {
    outcol.g = 0.0;
  }
  else if ((tmp = (1.0 - (1.0 - outcol.g) / tmp)) < 0.0) {
    outcol.g = 0.0;
  }
  else if (tmp > 1.0) {
    outcol.g = 1.0;
  }
  else {
    outcol.g = tmp;
  }

  tmp = facm + fac * col2.b;
  if (tmp <= 0.0) {
    outcol.b = 0.0;
  }
  else if ((tmp = (1.0 - (1.0 - outcol.b) / tmp)) < 0.0) {
    outcol.b = 0.0;
  }
  else if (tmp > 1.0) {
    outcol.b = 1.0;
  }
  else {
    outcol.b = tmp;
  }
}

void mix_hue(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  float facm = 1.0 - fac;

  outcol = col1;

  vec4 hsv, hsv2, tmp;
  rgb_to_hsv(col2, hsv2);

  if (hsv2.y != 0.0) {
    rgb_to_hsv(outcol, hsv);
    hsv.x = hsv2.x;
    hsv_to_rgb(hsv, tmp);

    outcol = mix(outcol, tmp, fac);
    outcol.a = col1.a;
  }
}

void mix_sat(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  float facm = 1.0 - fac;

  outcol = col1;

  vec4 hsv, hsv2;
  rgb_to_hsv(outcol, hsv);

  if (hsv.y != 0.0) {
    rgb_to_hsv(col2, hsv2);

    hsv.y = facm * hsv.y + fac * hsv2.y;
    hsv_to_rgb(hsv, outcol);
  }
}

void mix_val(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  float facm = 1.0 - fac;

  vec4 hsv, hsv2;
  rgb_to_hsv(col1, hsv);
  rgb_to_hsv(col2, hsv2);

  hsv.z = facm * hsv.z + fac * hsv2.z;
  hsv_to_rgb(hsv, outcol);
}

void mix_color(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  float facm = 1.0 - fac;

  outcol = col1;

  vec4 hsv, hsv2, tmp;
  rgb_to_hsv(col2, hsv2);

  if (hsv2.y != 0.0) {
    rgb_to_hsv(outcol, hsv);
    hsv.x = hsv2.x;
    hsv.y = hsv2.y;
    hsv_to_rgb(hsv, tmp);

    outcol = mix(outcol, tmp, fac);
    outcol.a = col1.a;
  }
}

void mix_soft(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);
  float facm = 1.0 - fac;

  vec4 one = vec4(1.0);
  vec4 scr = one - (one - col2) * (one - col1);
  outcol = facm * col1 + fac * ((one - col1) * col2 * col1 + col1 * scr);
}

void mix_linear(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
  fac = clamp(fac, 0.0, 1.0);

  outcol = col1 + fac * (2.0 * (col2 - vec4(0.5)));
}

void valtorgb_opti_constant(
    float fac, float edge, vec4 color1, vec4 color2, out vec4 outcol, out float outalpha)
{
  outcol = (fac > edge) ? color2 : color1;
  outalpha = outcol.a;
}

void valtorgb_opti_linear(
    float fac, vec2 mulbias, vec4 color1, vec4 color2, out vec4 outcol, out float outalpha)
{
  fac = clamp(fac * mulbias.x + mulbias.y, 0.0, 1.0);
  outcol = mix(color1, color2, fac);
  outalpha = outcol.a;
}

void valtorgb(float fac, sampler1DArray colormap, float layer, out vec4 outcol, out float outalpha)
{
  outcol = texture(colormap, vec2(fac, layer));
  outalpha = outcol.a;
}

void valtorgb_nearest(
    float fac, sampler1DArray colormap, float layer, out vec4 outcol, out float outalpha)
{
  fac = clamp(fac, 0.0, 1.0);
  outcol = texelFetch(colormap, ivec2(fac * (textureSize(colormap, 0).x - 1), layer), 0);
  outalpha = outcol.a;
}

void rgbtobw(vec4 color, out float outval)
{
  vec3 factors = vec3(0.2126, 0.7152, 0.0722);
  outval = dot(color.rgb, factors);
}

void invert(float fac, vec4 col, out vec4 outcol)
{
  outcol.xyz = mix(col.xyz, vec3(1.0) - col.xyz, fac);
  outcol.w = col.w;
}

void clamp_vec3(vec3 vec, vec3 min, vec3 max, out vec3 out_vec)
{
  out_vec = clamp(vec, min, max);
}

void clamp_val(float value, float min, float max, out float out_value)
{
  out_value = clamp(value, min, max);
}

void hue_sat(float hue, float sat, float value, float fac, vec4 col, out vec4 outcol)
{
  vec4 hsv;

  rgb_to_hsv(col, hsv);

  hsv[0] = fract(hsv[0] + hue + 0.5);
  hsv[1] = clamp(hsv[1] * sat, 0.0, 1.0);
  hsv[2] = hsv[2] * value;

  hsv_to_rgb(hsv, outcol);

  outcol = mix(col, outcol, fac);
}

void separate_rgb(vec4 col, out float r, out float g, out float b)
{
  r = col.r;
  g = col.g;
  b = col.b;
}

void combine_rgb(float r, float g, float b, out vec4 col)
{
  col = vec4(r, g, b, 1.0);
}

void separate_xyz(vec3 vec, out float x, out float y, out float z)
{
  x = vec.r;
  y = vec.g;
  z = vec.b;
}

void combine_xyz(float x, float y, float z, out vec3 vec)
{
  vec = vec3(x, y, z);
}

void separate_hsv(vec4 col, out float h, out float s, out float v)
{
  vec4 hsv;

  rgb_to_hsv(col, hsv);
  h = hsv[0];
  s = hsv[1];
  v = hsv[2];
}

void combine_hsv(float h, float s, float v, out vec4 col)
{
  hsv_to_rgb(vec4(h, s, v, 1.0), col);
}

void output_node(vec4 rgb, float alpha, out vec4 outrgb)
{
  outrgb = vec4(rgb.rgb, alpha);
}

/*********** TEXTURES ***************/

void texco_norm(vec3 normal, out vec3 outnormal)
{
  /* corresponds to shi->orn, which is negated so cancels
     out blender normal negation */
  outnormal = normalize(normal);
}

vec3 mtex_2d_mapping(vec3 vec)
{
  return vec3(vec.xy * 0.5 + vec2(0.5), vec.z);
}

/** helper method to extract the upper left 3x3 matrix from a 4x4 matrix */
mat3 to_mat3(mat4 m4)
{
  mat3 m3;
  m3[0] = m4[0].xyz;
  m3[1] = m4[1].xyz;
  m3[2] = m4[2].xyz;
  return m3;
}

/*********** NEW SHADER UTILITIES **************/

float fresnel_dielectric_0(float eta)
{
  /* compute fresnel reflactance at normal incidence => cosi = 1.0 */
  float A = (eta - 1.0) / (eta + 1.0);

  return A * A;
}

float fresnel_dielectric_cos(float cosi, float eta)
{
  /* compute fresnel reflectance without explicitly computing
   * the refracted direction */
  float c = abs(cosi);
  float g = eta * eta - 1.0 + c * c;
  float result;

  if (g > 0.0) {
    g = sqrt(g);
    float A = (g - c) / (g + c);
    float B = (c * (g + c) - 1.0) / (c * (g - c) + 1.0);
    result = 0.5 * A * A * (1.0 + B * B);
  }
  else {
    result = 1.0; /* TIR (no refracted component) */
  }

  return result;
}

float fresnel_dielectric(vec3 Incoming, vec3 Normal, float eta)
{
  /* compute fresnel reflectance without explicitly computing
   * the refracted direction */
  return fresnel_dielectric_cos(dot(Incoming, Normal), eta);
}

float hypot(float x, float y)
{
  return sqrt(x * x + y * y);
}

void generated_from_orco(vec3 orco, out vec3 generated)
{
#ifdef VOLUMETRICS
#  ifdef MESH_SHADER
  generated = volumeObjectLocalCoord;
#  else
  generated = worldPosition;
#  endif
#else
  generated = orco;
#endif
}

int floor_to_int(float x)
{
  return int(floor(x));
}

int quick_floor(float x)
{
  return int(x) - ((x < 0) ? 1 : 0);
}

float integer_noise(int n)
{
  int nn;
  n = (n + 1013) & 0x7fffffff;
  n = (n >> 13) ^ n;
  nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
  return 0.5 * (float(nn) / 1073741824.0);
}

uint hash(uint kx, uint ky, uint kz)
{
#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
#define final(a, b, c) \
  { \
    c ^= b; \
    c -= rot(b, 14); \
    a ^= c; \
    a -= rot(c, 11); \
    b ^= a; \
    b -= rot(a, 25); \
    c ^= b; \
    c -= rot(b, 16); \
    a ^= c; \
    a -= rot(c, 4); \
    b ^= a; \
    b -= rot(a, 14); \
    c ^= b; \
    c -= rot(b, 24); \
  }
  // now hash the data!
  uint a, b, c, len = 3u;
  a = b = c = 0xdeadbeefu + (len << 2u) + 13u;

  c += kz;
  b += ky;
  a += kx;
  final(a, b, c);

  return c;
#undef rot
#undef final
}

uint hash(int kx, int ky, int kz)
{
  return hash(uint(kx), uint(ky), uint(kz));
}

float bits_to_01(uint bits)
{
  return (float(bits) / 4294967295.0);
}

float cellnoise(vec3 p)
{
  int ix = quick_floor(p.x);
  int iy = quick_floor(p.y);
  int iz = quick_floor(p.z);

  return bits_to_01(hash(uint(ix), uint(iy), uint(iz)));
}

vec3 cellnoise_color(vec3 p)
{
  float r = cellnoise(p.xyz);
  float g = cellnoise(p.yxz);
  float b = cellnoise(p.yzx);

  return vec3(r, g, b);
}

float floorfrac(float x, out int i)
{
  float x_floor = floor(x);
  i = int(x_floor);
  return x - x_floor;
}

/* bsdfs */

vec3 tint_from_color(vec3 color)
{
  float lum = dot(color, vec3(0.3, 0.6, 0.1)); /* luminance approx. */
  return (lum > 0) ? color / lum : vec3(1.0);  /* normalize lum. to isolate hue+sat */
}

void convert_metallic_to_specular_tinted(vec3 basecol,
                                         vec3 basecol_tint,
                                         float metallic,
                                         float specular_fac,
                                         float specular_tint,
                                         out vec3 diffuse,
                                         out vec3 f0)
{
  vec3 tmp_col = mix(vec3(1.0), basecol_tint, specular_tint);
  f0 = mix((0.08 * specular_fac) * tmp_col, basecol, metallic);
  diffuse = basecol * (1.0 - metallic);
}

vec3 principled_sheen(float NV, vec3 basecol_tint, float sheen_tint)
{
  float f = 1.0 - NV;
  /* Temporary fix for T59784. Normal map seems to contain NaNs for tangent space normal maps,
   * therefore we need to clamp value. */
  f = clamp(f, 0.0, 1.0);
  /* Empirical approximation (manual curve fitting). Can be refined. */
  float sheen = f * f * f * 0.077 + f * 0.01 + 0.00026;
  return sheen * mix(vec3(1.0), basecol_tint, sheen_tint);
}

#ifndef VOLUMETRICS
void node_bsdf_diffuse(vec4 color, float roughness, vec3 N, out Closure result)
{
  N = normalize(N);
  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  eevee_closure_diffuse(N, color.rgb, 1.0, result.radiance);
  result.radiance *= color.rgb;
}

void node_bsdf_glossy(vec4 color, float roughness, vec3 N, float ssr_id, out Closure result)
{
  N = normalize(N);
  vec3 out_spec, ssr_spec;
  eevee_closure_glossy(N, vec3(1.0), int(ssr_id), roughness, 1.0, out_spec, ssr_spec);
  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.radiance = out_spec * color.rgb;
  result.ssr_data = vec4(ssr_spec * color.rgb, roughness);
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  result.ssr_id = int(ssr_id);
}

void node_bsdf_anisotropic(vec4 color,
                           float roughness,
                           float anisotropy,
                           float rotation,
                           vec3 N,
                           vec3 T,
                           out Closure result)
{
  node_bsdf_glossy(color, roughness, N, -1, result);
}

void node_bsdf_glass(
    vec4 color, float roughness, float ior, vec3 N, float ssr_id, out Closure result)
{
  N = normalize(N);
  vec3 out_spec, out_refr, ssr_spec;
  vec3 refr_color = (refractionDepth > 0.0) ? color.rgb * color.rgb :
                                              color.rgb; /* Simulate 2 transmission event */
  eevee_closure_glass(
      N, vec3(1.0), int(ssr_id), roughness, 1.0, ior, out_spec, out_refr, ssr_spec);
  out_refr *= refr_color;
  out_spec *= color.rgb;
  float fresnel = F_eta(ior, dot(N, cameraVec));
  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.radiance = mix(out_refr, out_spec, fresnel);
  result.ssr_data = vec4(ssr_spec * color.rgb * fresnel, roughness);
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  result.ssr_id = int(ssr_id);
}

void node_bsdf_toon(vec4 color, float size, float tsmooth, vec3 N, out Closure result)
{
  node_bsdf_diffuse(color, 0.0, N, result);
}

void node_bsdf_principled(vec4 base_color,
                          float subsurface,
                          vec3 subsurface_radius,
                          vec4 subsurface_color,
                          float metallic,
                          float specular,
                          float specular_tint,
                          float roughness,
                          float anisotropic,
                          float anisotropic_rotation,
                          float sheen,
                          float sheen_tint,
                          float clearcoat,
                          float clearcoat_roughness,
                          float ior,
                          float transmission,
                          float transmission_roughness,
                          vec4 emission,
                          float alpha,
                          vec3 N,
                          vec3 CN,
                          vec3 T,
                          vec3 I,
                          float ssr_id,
                          float sss_id,
                          vec3 sss_scale,
                          out Closure result)
{
  N = normalize(N);
  ior = max(ior, 1e-5);
  metallic = saturate(metallic);
  transmission = saturate(transmission);
  float dielectric = 1.0 - metallic;
  transmission *= dielectric;
  sheen *= dielectric;
  subsurface_color *= dielectric;

  vec3 diffuse, f0, out_diff, out_spec, out_trans, out_refr, ssr_spec;
  vec3 ctint = tint_from_color(base_color.rgb);
  convert_metallic_to_specular_tinted(
      base_color.rgb, ctint, metallic, specular, specular_tint, diffuse, f0);

  float NV = dot(N, cameraVec);
  vec3 out_sheen = sheen * principled_sheen(NV, ctint, sheen_tint);

  /* Far from being accurate, but 2 glossy evaluation is too expensive.
   * Most noticeable difference is at grazing angles since the bsdf lut
   * f0 color interpolation is done on top of this interpolation. */
  vec3 f0_glass = mix(vec3(1.0), base_color.rgb, specular_tint);
  float fresnel = F_eta(ior, NV);
  vec3 spec_col = F_color_blend(ior, fresnel, f0_glass) * fresnel;
  f0 = mix(f0, spec_col, transmission);

  vec3 mixed_ss_base_color = mix(diffuse, subsurface_color.rgb, subsurface);

  float sss_scalef = dot(sss_scale, vec3(1.0 / 3.0)) * subsurface;
  eevee_closure_principled(N,
                           mixed_ss_base_color,
                           f0,
                           int(ssr_id),
                           roughness,
                           CN,
                           clearcoat * 0.25,
                           clearcoat_roughness,
                           1.0,
                           sss_scalef,
                           ior,
                           out_diff,
                           out_trans,
                           out_spec,
                           out_refr,
                           ssr_spec);

  vec3 refr_color = base_color.rgb;
  refr_color *= (refractionDepth > 0.0) ? refr_color :
                                          vec3(1.0); /* Simulate 2 transmission event */
  out_refr *= refr_color * (1.0 - fresnel) * transmission;

  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.radiance = out_spec + out_refr;
  result.radiance += out_diff * out_sheen; /* Coarse approx. */
#  ifndef USE_SSS
  result.radiance += (out_diff + out_trans) * mixed_ss_base_color * (1.0 - transmission);
#  endif
  result.ssr_data = vec4(ssr_spec, roughness);
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  result.ssr_id = int(ssr_id);
#  ifdef USE_SSS
  result.sss_data.a = sss_scalef;
  result.sss_data.rgb = out_diff + out_trans;
#    ifdef USE_SSS_ALBEDO
  result.sss_albedo.rgb = mixed_ss_base_color;
#    else
  result.sss_data.rgb *= mixed_ss_base_color;
#    endif
  result.sss_data.rgb *= (1.0 - transmission);
#  endif
  result.radiance += emission.rgb;
  result.opacity = alpha;
}

void node_bsdf_principled_dielectric(vec4 base_color,
                                     float subsurface,
                                     vec3 subsurface_radius,
                                     vec4 subsurface_color,
                                     float metallic,
                                     float specular,
                                     float specular_tint,
                                     float roughness,
                                     float anisotropic,
                                     float anisotropic_rotation,
                                     float sheen,
                                     float sheen_tint,
                                     float clearcoat,
                                     float clearcoat_roughness,
                                     float ior,
                                     float transmission,
                                     float transmission_roughness,
                                     vec4 emission,
                                     float alpha,
                                     vec3 N,
                                     vec3 CN,
                                     vec3 T,
                                     vec3 I,
                                     float ssr_id,
                                     float sss_id,
                                     vec3 sss_scale,
                                     out Closure result)
{
  N = normalize(N);
  metallic = saturate(metallic);
  float dielectric = 1.0 - metallic;

  vec3 diffuse, f0, out_diff, out_spec, ssr_spec;
  vec3 ctint = tint_from_color(base_color.rgb);
  convert_metallic_to_specular_tinted(
      base_color.rgb, ctint, metallic, specular, specular_tint, diffuse, f0);

  float NV = dot(N, cameraVec);
  vec3 out_sheen = sheen * principled_sheen(NV, ctint, sheen_tint);

  eevee_closure_default(N, diffuse, f0, int(ssr_id), roughness, 1.0, out_diff, out_spec, ssr_spec);

  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.radiance = out_spec + out_diff * (diffuse + out_sheen);
  result.ssr_data = vec4(ssr_spec, roughness);
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  result.ssr_id = int(ssr_id);
  result.radiance += emission.rgb;
  result.opacity = alpha;
}

void node_bsdf_principled_metallic(vec4 base_color,
                                   float subsurface,
                                   vec3 subsurface_radius,
                                   vec4 subsurface_color,
                                   float metallic,
                                   float specular,
                                   float specular_tint,
                                   float roughness,
                                   float anisotropic,
                                   float anisotropic_rotation,
                                   float sheen,
                                   float sheen_tint,
                                   float clearcoat,
                                   float clearcoat_roughness,
                                   float ior,
                                   float transmission,
                                   float transmission_roughness,
                                   vec4 emission,
                                   float alpha,
                                   vec3 N,
                                   vec3 CN,
                                   vec3 T,
                                   vec3 I,
                                   float ssr_id,
                                   float sss_id,
                                   vec3 sss_scale,
                                   out Closure result)
{
  N = normalize(N);
  vec3 out_spec, ssr_spec;

  eevee_closure_glossy(N, base_color.rgb, int(ssr_id), roughness, 1.0, out_spec, ssr_spec);

  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.radiance = out_spec;
  result.ssr_data = vec4(ssr_spec, roughness);
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  result.ssr_id = int(ssr_id);
  result.radiance += emission.rgb;
  result.opacity = alpha;
}

void node_bsdf_principled_clearcoat(vec4 base_color,
                                    float subsurface,
                                    vec3 subsurface_radius,
                                    vec4 subsurface_color,
                                    float metallic,
                                    float specular,
                                    float specular_tint,
                                    float roughness,
                                    float anisotropic,
                                    float anisotropic_rotation,
                                    float sheen,
                                    float sheen_tint,
                                    float clearcoat,
                                    float clearcoat_roughness,
                                    float ior,
                                    float transmission,
                                    float transmission_roughness,
                                    vec4 emission,
                                    float alpha,
                                    vec3 N,
                                    vec3 CN,
                                    vec3 T,
                                    vec3 I,
                                    float ssr_id,
                                    float sss_id,
                                    vec3 sss_scale,
                                    out Closure result)
{
  vec3 out_spec, ssr_spec;
  N = normalize(N);

  eevee_closure_clearcoat(N,
                          base_color.rgb,
                          int(ssr_id),
                          roughness,
                          CN,
                          clearcoat * 0.25,
                          clearcoat_roughness,
                          1.0,
                          out_spec,
                          ssr_spec);

  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.radiance = out_spec;
  result.ssr_data = vec4(ssr_spec, roughness);
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  result.ssr_id = int(ssr_id);
  result.radiance += emission.rgb;
  result.opacity = alpha;
}

void node_bsdf_principled_subsurface(vec4 base_color,
                                     float subsurface,
                                     vec3 subsurface_radius,
                                     vec4 subsurface_color,
                                     float metallic,
                                     float specular,
                                     float specular_tint,
                                     float roughness,
                                     float anisotropic,
                                     float anisotropic_rotation,
                                     float sheen,
                                     float sheen_tint,
                                     float clearcoat,
                                     float clearcoat_roughness,
                                     float ior,
                                     float transmission,
                                     float transmission_roughness,
                                     vec4 emission,
                                     float alpha,
                                     vec3 N,
                                     vec3 CN,
                                     vec3 T,
                                     vec3 I,
                                     float ssr_id,
                                     float sss_id,
                                     vec3 sss_scale,
                                     out Closure result)
{
  metallic = saturate(metallic);
  N = normalize(N);

  vec3 diffuse, f0, out_diff, out_spec, out_trans, ssr_spec;
  vec3 ctint = tint_from_color(base_color.rgb);
  convert_metallic_to_specular_tinted(
      base_color.rgb, ctint, metallic, specular, specular_tint, diffuse, f0);

  subsurface_color = subsurface_color * (1.0 - metallic);
  vec3 mixed_ss_base_color = mix(diffuse, subsurface_color.rgb, subsurface);
  float sss_scalef = dot(sss_scale, vec3(1.0 / 3.0)) * subsurface;

  float NV = dot(N, cameraVec);
  vec3 out_sheen = sheen * principled_sheen(NV, ctint, sheen_tint);

  eevee_closure_skin(N,
                     mixed_ss_base_color,
                     f0,
                     int(ssr_id),
                     roughness,
                     1.0,
                     sss_scalef,
                     out_diff,
                     out_trans,
                     out_spec,
                     ssr_spec);

  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.radiance = out_spec;
  result.ssr_data = vec4(ssr_spec, roughness);
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  result.ssr_id = int(ssr_id);
#  ifdef USE_SSS
  result.sss_data.a = sss_scalef;
  result.sss_data.rgb = out_diff + out_trans;
#    ifdef USE_SSS_ALBEDO
  result.sss_albedo.rgb = mixed_ss_base_color;
#    else
  result.sss_data.rgb *= mixed_ss_base_color;
#    endif
#  else
  result.radiance += (out_diff + out_trans) * mixed_ss_base_color;
#  endif
  result.radiance += out_diff * out_sheen;
  result.radiance += emission.rgb;
  result.opacity = alpha;
}

void node_bsdf_principled_glass(vec4 base_color,
                                float subsurface,
                                vec3 subsurface_radius,
                                vec4 subsurface_color,
                                float metallic,
                                float specular,
                                float specular_tint,
                                float roughness,
                                float anisotropic,
                                float anisotropic_rotation,
                                float sheen,
                                float sheen_tint,
                                float clearcoat,
                                float clearcoat_roughness,
                                float ior,
                                float transmission,
                                float transmission_roughness,
                                vec4 emission,
                                float alpha,
                                vec3 N,
                                vec3 CN,
                                vec3 T,
                                vec3 I,
                                float ssr_id,
                                float sss_id,
                                vec3 sss_scale,
                                out Closure result)
{
  ior = max(ior, 1e-5);
  N = normalize(N);

  vec3 f0, out_spec, out_refr, ssr_spec;
  f0 = mix(vec3(1.0), base_color.rgb, specular_tint);

  eevee_closure_glass(
      N, vec3(1.0), int(ssr_id), roughness, 1.0, ior, out_spec, out_refr, ssr_spec);

  vec3 refr_color = base_color.rgb;
  refr_color *= (refractionDepth > 0.0) ? refr_color :
                                          vec3(1.0); /* Simulate 2 transmission events */
  out_refr *= refr_color;

  float fresnel = F_eta(ior, dot(N, cameraVec));
  vec3 spec_col = F_color_blend(ior, fresnel, f0);
  out_spec *= spec_col;
  ssr_spec *= spec_col * fresnel;

  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.radiance = mix(out_refr, out_spec, fresnel);
  result.ssr_data = vec4(ssr_spec, roughness);
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  result.ssr_id = int(ssr_id);
  result.radiance += emission.rgb;
  result.opacity = alpha;
}

void node_bsdf_translucent(vec4 color, vec3 N, out Closure result)
{
  node_bsdf_diffuse(color, 0.0, -N, result);
}

void node_bsdf_transparent(vec4 color, out Closure result)
{
  /* this isn't right */
  result = CLOSURE_DEFAULT;
  result.radiance = vec3(0.0);
  result.opacity = clamp(1.0 - dot(color.rgb, vec3(0.3333334)), 0.0, 1.0);
  result.ssr_id = TRANSPARENT_CLOSURE_FLAG;
}

void node_bsdf_velvet(vec4 color, float sigma, vec3 N, out Closure result)
{
  node_bsdf_diffuse(color, 0.0, N, result);
}

void node_subsurface_scattering(vec4 color,
                                float scale,
                                vec3 radius,
                                float sharpen,
                                float texture_blur,
                                vec3 N,
                                float sss_id,
                                out Closure result)
{
#  if defined(USE_SSS)
  N = normalize(N);
  vec3 out_diff, out_trans;
  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.ssr_data = vec4(0.0);
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  result.ssr_id = -1;
  result.sss_data.a = scale;
  eevee_closure_subsurface(N, color.rgb, 1.0, scale, out_diff, out_trans);
  result.sss_data.rgb = out_diff + out_trans;
#    ifdef USE_SSS_ALBEDO
  /* Not perfect for texture_blur not exactly equal to 0.0 or 1.0. */
  result.sss_albedo.rgb = mix(color.rgb, vec3(1.0), texture_blur);
  result.sss_data.rgb *= mix(vec3(1.0), color.rgb, texture_blur);
#    else
  result.sss_data.rgb *= color.rgb;
#    endif
#  else
  node_bsdf_diffuse(color, 0.0, N, result);
#  endif
}

void node_bsdf_refraction(vec4 color, float roughness, float ior, vec3 N, out Closure result)
{
  N = normalize(N);
  vec3 out_refr;
  color.rgb *= (refractionDepth > 0.0) ? color.rgb : vec3(1.0); /* Simulate 2 absorption event. */
  eevee_closure_refraction(N, roughness, ior, out_refr);
  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  result.radiance = out_refr * color.rgb;
  result.ssr_id = REFRACT_CLOSURE_FLAG;
}

void node_ambient_occlusion(
    vec4 color, float distance, vec3 normal, out vec4 result_color, out float result_ao)
{
  vec3 bent_normal;
  vec4 rand = texelFetch(utilTex, ivec3(ivec2(gl_FragCoord.xy) % LUT_SIZE, 2.0), 0);
  result_ao = occlusion_compute(normalize(normal), viewPosition, 1.0, rand, bent_normal);
  result_color = result_ao * color;
}

void node_wireframe(float size, vec2 barycentric, vec3 barycentric_dist, out float fac)
{
  vec3 barys = barycentric.xyy;
  barys.z = 1.0 - barycentric.x - barycentric.y;

  size *= 0.5;
  vec3 s = step(-size, -barys * barycentric_dist);

  fac = max(s.x, max(s.y, s.z));
}

void node_wireframe_screenspace(float size, vec2 barycentric, out float fac)
{
  vec3 barys = barycentric.xyy;
  barys.z = 1.0 - barycentric.x - barycentric.y;

  size *= (1.0 / 3.0);
  vec3 dx = dFdx(barys);
  vec3 dy = dFdy(barys);
  vec3 deltas = sqrt(dx * dx + dy * dy);

  vec3 s = step(-deltas * size, -barys);

  fac = max(s.x, max(s.y, s.z));
}

#else /* VOLUMETRICS */

/* Stub all bsdf functions not compatible with volumetrics. */
#  define node_bsdf_diffuse
#  define node_bsdf_glossy
#  define node_bsdf_anisotropic
#  define node_bsdf_glass
#  define node_bsdf_toon
#  define node_bsdf_principled
#  define node_bsdf_principled_dielectric
#  define node_bsdf_principled_metallic
#  define node_bsdf_principled_clearcoat
#  define node_bsdf_principled_subsurface
#  define node_bsdf_principled_glass
#  define node_bsdf_translucent
#  define node_bsdf_transparent
#  define node_bsdf_velvet
#  define node_subsurface_scattering
#  define node_bsdf_refraction
#  define node_ambient_occlusion
#  define node_wireframe
#  define node_wireframe_screenspace

#endif /* VOLUMETRICS */

/* emission */

void node_emission(vec4 color, float strength, vec3 vN, out Closure result)
{
#ifndef VOLUMETRICS
  color *= strength;
  result = CLOSURE_DEFAULT;
  result.radiance = color.rgb;
  result.opacity = color.a;
  result.ssr_normal = normal_encode(vN, viewCameraVec);
#else
  result = Closure(vec3(0.0), vec3(0.0), color.rgb * strength, 0.0);
#endif
}

/* background */

void node_tex_environment_texco(vec3 viewvec, out vec3 worldvec)
{
#ifdef MESH_SHADER
  worldvec = worldPosition;
#else
  vec4 v = (ProjectionMatrix[3][3] == 0.0) ? vec4(viewvec, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
  vec4 co_homogenous = (ProjectionMatrixInverse * v);

  vec3 co = co_homogenous.xyz / co_homogenous.w;
#  if defined(WORLD_BACKGROUND) || defined(PROBE_CAPTURE)
  worldvec = mat3(ViewMatrixInverse) * co;
#  else
  worldvec = mat3(ModelMatrixInverse) * (mat3(ViewMatrixInverse) * co);
#  endif
#endif
}

void node_background(vec4 color, float strength, out Closure result)
{
#ifndef VOLUMETRICS
  color *= strength;
  result = CLOSURE_DEFAULT;
  result.radiance = color.rgb;
  result.opacity = color.a;
#else
  result = CLOSURE_DEFAULT;
#endif
}

/* volumes */

void node_volume_scatter(vec4 color, float density, float anisotropy, out Closure result)
{
#ifdef VOLUMETRICS
  result = Closure(vec3(0.0), color.rgb * density, vec3(0.0), anisotropy);
#else
  result = CLOSURE_DEFAULT;
#endif
}

void node_volume_absorption(vec4 color, float density, out Closure result)
{
#ifdef VOLUMETRICS
  result = Closure((1.0 - color.rgb) * density, vec3(0.0), vec3(0.0), 0.0);
#else
  result = CLOSURE_DEFAULT;
#endif
}

void node_blackbody(float temperature, sampler1DArray spectrummap, float layer, out vec4 color)
{
  if (temperature >= 12000.0) {
    color = vec4(0.826270103, 0.994478524, 1.56626022, 1.0);
  }
  else if (temperature < 965.0) {
    color = vec4(4.70366907, 0.0, 0.0, 1.0);
  }
  else {
    float t = (temperature - 965.0) / (12000.0 - 965.0);
    color = vec4(texture(spectrummap, vec2(t, layer)).rgb, 1.0);
  }
}

void node_volume_principled(vec4 color,
                            float density,
                            float anisotropy,
                            vec4 absorption_color,
                            float emission_strength,
                            vec4 emission_color,
                            float blackbody_intensity,
                            vec4 blackbody_tint,
                            float temperature,
                            float density_attribute,
                            vec4 color_attribute,
                            float temperature_attribute,
                            sampler1DArray spectrummap,
                            float layer,
                            out Closure result)
{
#ifdef VOLUMETRICS
  vec3 absorption_coeff = vec3(0.0);
  vec3 scatter_coeff = vec3(0.0);
  vec3 emission_coeff = vec3(0.0);

  /* Compute density. */
  density = max(density, 0.0);

  if (density > 1e-5) {
    density = max(density * density_attribute, 0.0);
  }

  if (density > 1e-5) {
    /* Compute scattering and absorption coefficients. */
    vec3 scatter_color = color.rgb * color_attribute.rgb;

    scatter_coeff = scatter_color * density;
    absorption_color.rgb = sqrt(max(absorption_color.rgb, 0.0));
    absorption_coeff = max(1.0 - scatter_color, 0.0) * max(1.0 - absorption_color.rgb, 0.0) *
                       density;
  }

  /* Compute emission. */
  emission_strength = max(emission_strength, 0.0);

  if (emission_strength > 1e-5) {
    emission_coeff += emission_strength * emission_color.rgb;
  }

  if (blackbody_intensity > 1e-3) {
    /* Add temperature from attribute. */
    float T = max(temperature * max(temperature_attribute, 0.0), 0.0);

    /* Stefan-Boltzman law. */
    float T2 = T * T;
    float T4 = T2 * T2;
    float sigma = 5.670373e-8 * 1e-6 / M_PI;
    float intensity = sigma * mix(1.0, T4, blackbody_intensity);

    if (intensity > 1e-5) {
      vec4 bb;
      node_blackbody(T, spectrummap, layer, bb);
      emission_coeff += bb.rgb * blackbody_tint.rgb * intensity;
    }
  }

  result = Closure(absorption_coeff, scatter_coeff, emission_coeff, anisotropy);
#else
  result = CLOSURE_DEFAULT;
#endif
}

/* closures */

void node_mix_shader(float fac, Closure shader1, Closure shader2, out Closure shader)
{
  shader = closure_mix(shader1, shader2, fac);
}

void node_add_shader(Closure shader1, Closure shader2, out Closure shader)
{
  shader = closure_add(shader1, shader2);
}

/* fresnel */

void node_fresnel(float ior, vec3 N, vec3 I, out float result)
{
  N = normalize(N);
  /* handle perspective/orthographic */
  vec3 I_view = (ProjectionMatrix[3][3] == 0.0) ? normalize(I) : vec3(0.0, 0.0, -1.0);

  float eta = max(ior, 0.00001);
  result = fresnel_dielectric(I_view, N, (gl_FrontFacing) ? eta : 1.0 / eta);
}

/* layer_weight */

void node_layer_weight(float blend, vec3 N, vec3 I, out float fresnel, out float facing)
{
  N = normalize(N);

  /* fresnel */
  float eta = max(1.0 - blend, 0.00001);
  vec3 I_view = (ProjectionMatrix[3][3] == 0.0) ? normalize(I) : vec3(0.0, 0.0, -1.0);

  fresnel = fresnel_dielectric(I_view, N, (gl_FrontFacing) ? 1.0 / eta : eta);

  /* facing */
  facing = abs(dot(I_view, N));
  if (blend != 0.5) {
    blend = clamp(blend, 0.0, 0.99999);
    blend = (blend < 0.5) ? 2.0 * blend : 0.5 / (1.0 - blend);
    facing = pow(facing, blend);
  }
  facing = 1.0 - facing;
}

/* gamma */

void node_gamma(vec4 col, float gamma, out vec4 outcol)
{
  outcol = col;

  if (col.r > 0.0) {
    outcol.r = compatible_pow(col.r, gamma);
  }
  if (col.g > 0.0) {
    outcol.g = compatible_pow(col.g, gamma);
  }
  if (col.b > 0.0) {
    outcol.b = compatible_pow(col.b, gamma);
  }
}

/* geometry */

void node_attribute_volume_density(sampler3D tex, out vec4 outcol, out vec3 outvec, out float outf)
{
#if defined(MESH_SHADER) && defined(VOLUMETRICS)
  vec3 cos = volumeObjectLocalCoord;
#else
  vec3 cos = vec3(0.0);
#endif
  outvec = texture(tex, cos).aaa;
  outcol = vec4(outvec, 1.0);
  outf = dot(vec3(1.0 / 3.0), outvec);
}

uniform vec3 volumeColor = vec3(1.0);

void node_attribute_volume_color(sampler3D tex, out vec4 outcol, out vec3 outvec, out float outf)
{
#if defined(MESH_SHADER) && defined(VOLUMETRICS)
  vec3 cos = volumeObjectLocalCoord;
#else
  vec3 cos = vec3(0.0);
#endif

  vec4 value = texture(tex, cos).rgba;
  /* Density is premultiplied for interpolation, divide it out here. */
  if (value.a > 1e-8) {
    value.rgb /= value.a;
  }

  outvec = value.rgb * volumeColor;
  outcol = vec4(outvec, 1.0);
  outf = dot(vec3(1.0 / 3.0), outvec);
}

void node_attribute_volume_flame(sampler3D tex, out vec4 outcol, out vec3 outvec, out float outf)
{
#if defined(MESH_SHADER) && defined(VOLUMETRICS)
  vec3 cos = volumeObjectLocalCoord;
#else
  vec3 cos = vec3(0.0);
#endif
  outf = texture(tex, cos).r;
  outvec = vec3(outf, outf, outf);
  outcol = vec4(outf, outf, outf, 1.0);
}

void node_attribute_volume_temperature(
    sampler3D tex, vec2 temperature, out vec4 outcol, out vec3 outvec, out float outf)
{
#if defined(MESH_SHADER) && defined(VOLUMETRICS)
  vec3 cos = volumeObjectLocalCoord;
#else
  vec3 cos = vec3(0.0);
#endif
  float flame = texture(tex, cos).r;

  outf = (flame > 0.01) ? temperature.x + flame * (temperature.y - temperature.x) : 0.0;
  outvec = vec3(outf, outf, outf);
  outcol = vec4(outf, outf, outf, 1.0);
}

void node_attribute(vec3 attr, out vec4 outcol, out vec3 outvec, out float outf)
{
  outcol = vec4(attr, 1.0);
  outvec = attr;
  outf = dot(vec3(1.0 / 3.0), attr);
}

void node_uvmap(vec3 attr_uv, out vec3 outvec)
{
  outvec = attr_uv;
}

void tangent_orco_x(vec3 orco_in, out vec3 orco_out)
{
  orco_out = orco_in.xzy * vec3(0.0, -0.5, 0.5) + vec3(0.0, 0.25, -0.25);
}

void tangent_orco_y(vec3 orco_in, out vec3 orco_out)
{
  orco_out = orco_in.zyx * vec3(-0.5, 0.0, 0.5) + vec3(0.25, 0.0, -0.25);
}

void tangent_orco_z(vec3 orco_in, out vec3 orco_out)
{
  orco_out = orco_in.yxz * vec3(-0.5, 0.5, 0.0) + vec3(0.25, -0.25, 0.0);
}

void node_tangentmap(vec4 attr_tangent, out vec3 tangent)
{
  tangent = normalize(attr_tangent.xyz);
}

void node_tangent(vec3 N, vec3 orco, mat4 objmat, out vec3 T)
{
  T = (objmat * vec4(orco, 0.0)).xyz;
  T = cross(N, normalize(cross(T, N)));
}

void node_geometry(vec3 I,
                   vec3 N,
                   vec3 orco,
                   mat4 objmat,
                   mat4 toworld,
                   vec2 barycentric,
                   out vec3 position,
                   out vec3 normal,
                   out vec3 tangent,
                   out vec3 true_normal,
                   out vec3 incoming,
                   out vec3 parametric,
                   out float backfacing,
                   out float pointiness)
{
  /* handle perspective/orthographic */
  vec3 I_view = (ProjectionMatrix[3][3] == 0.0) ? normalize(I) : vec3(0.0, 0.0, -1.0);
  incoming = -(toworld * vec4(I_view, 0.0)).xyz;

#if defined(WORLD_BACKGROUND) || defined(PROBE_CAPTURE)
  position = -incoming;
  true_normal = normal = incoming;
  tangent = parametric = vec3(0.0);
  vec3(0.0);
  backfacing = 0.0;
  pointiness = 0.0;
#else

  position = worldPosition;
#  ifndef VOLUMETRICS
  normal = normalize(N);
  vec3 B = dFdx(worldPosition);
  vec3 T = dFdy(worldPosition);
  true_normal = normalize(cross(B, T));
#  else
  normal = (toworld * vec4(N, 0.0)).xyz;
  true_normal = normal;
#  endif
  tangent_orco_z(orco, orco);
  node_tangent(N, orco, objmat, tangent);

  parametric = vec3(barycentric, 0.0);
  backfacing = (gl_FrontFacing) ? 0.0 : 1.0;
  pointiness = 0.5;
#endif
}

void generated_texco(vec3 I, vec3 attr_orco, out vec3 generated)
{
  vec4 v = (ProjectionMatrix[3][3] == 0.0) ? vec4(I, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
  vec4 co_homogenous = (ProjectionMatrixInverse * v);
  vec4 co = vec4(co_homogenous.xyz / co_homogenous.w, 0.0);
  co.xyz = normalize(co.xyz);
#if defined(WORLD_BACKGROUND) || defined(PROBE_CAPTURE)
  generated = (ViewMatrixInverse * co).xyz;
#else
  generated_from_orco(attr_orco, generated);
#endif
}

void node_tex_coord(vec3 I,
                    vec3 wN,
                    mat4 obmatinv,
                    vec4 camerafac,
                    vec3 attr_orco,
                    vec3 attr_uv,
                    out vec3 generated,
                    out vec3 normal,
                    out vec3 uv,
                    out vec3 object,
                    out vec3 camera,
                    out vec3 window,
                    out vec3 reflection)
{
  generated = attr_orco;
  normal = normalize(normal_world_to_object(wN));
  uv = attr_uv;
  object = (obmatinv * (ViewMatrixInverse * vec4(I, 1.0))).xyz;
  camera = vec3(I.xy, -I.z);
  vec4 projvec = ProjectionMatrix * vec4(I, 1.0);
  window = vec3(mtex_2d_mapping(projvec.xyz / projvec.w).xy * camerafac.xy + camerafac.zw, 0.0);
  reflection = -reflect(cameraVec, normalize(wN));
}

void node_tex_coord_background(vec3 I,
                               vec3 N,
                               mat4 obmatinv,
                               vec4 camerafac,
                               vec3 attr_orco,
                               vec3 attr_uv,
                               out vec3 generated,
                               out vec3 normal,
                               out vec3 uv,
                               out vec3 object,
                               out vec3 camera,
                               out vec3 window,
                               out vec3 reflection)
{
  vec4 v = (ProjectionMatrix[3][3] == 0.0) ? vec4(I, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
  vec4 co_homogenous = (ProjectionMatrixInverse * v);

  vec4 co = vec4(co_homogenous.xyz / co_homogenous.w, 0.0);

  co = normalize(co);

  vec3 coords = (ViewMatrixInverse * co).xyz;

  generated = coords;
  normal = -coords;
  uv = vec3(attr_uv.xy, 0.0);
  object = coords;

  camera = vec3(co.xy, -co.z);
  window = vec3(mtex_2d_mapping(I).xy * camerafac.xy + camerafac.zw, 0.0);

  reflection = -coords;
}

#if defined(WORLD_BACKGROUND) || (defined(PROBE_CAPTURE) && !defined(MESH_SHADER))
#  define node_tex_coord node_tex_coord_background
#endif

/* textures */

float calc_gradient(vec3 p, int gradient_type)
{
  float x, y, z;
  x = p.x;
  y = p.y;
  z = p.z;
  if (gradient_type == 0) { /* linear */
    return x;
  }
  else if (gradient_type == 1) { /* quadratic */
    float r = max(x, 0.0);
    return r * r;
  }
  else if (gradient_type == 2) { /* easing */
    float r = min(max(x, 0.0), 1.0);
    float t = r * r;
    return (3.0 * t - 2.0 * t * r);
  }
  else if (gradient_type == 3) { /* diagonal */
    return (x + y) * 0.5;
  }
  else if (gradient_type == 4) { /* radial */
    return atan(y, x) / (M_PI * 2) + 0.5;
  }
  else {
    /* Bias a little bit for the case where p is a unit length vector,
     * to get exactly zero instead of a small random value depending
     * on float precision. */
    float r = max(0.999999 - sqrt(x * x + y * y + z * z), 0.0);
    if (gradient_type == 5) { /* quadratic sphere */
      return r * r;
    }
    else if (gradient_type == 6) { /* sphere */
      return r;
    }
  }
  return 0.0;
}

void node_tex_gradient(vec3 co, float gradient_type, out vec4 color, out float fac)
{
  float f = calc_gradient(co, int(gradient_type));
  f = clamp(f, 0.0, 1.0);

  color = vec4(f, f, f, 1.0);
  fac = f;
}

void node_tex_checker(
    vec3 co, vec4 color1, vec4 color2, float scale, out vec4 color, out float fac)
{
  vec3 p = co * scale;

  /* Prevent precision issues on unit coordinates. */
  p = (p + 0.000001) * 0.999999;

  int xi = int(abs(floor(p.x)));
  int yi = int(abs(floor(p.y)));
  int zi = int(abs(floor(p.z)));

  bool check = ((mod(xi, 2) == mod(yi, 2)) == bool(mod(zi, 2)));

  color = check ? color1 : color2;
  fac = check ? 1.0 : 0.0;
}

vec2 calc_brick_texture(vec3 p,
                        float mortar_size,
                        float mortar_smooth,
                        float bias,
                        float brick_width,
                        float row_height,
                        float offset_amount,
                        int offset_frequency,
                        float squash_amount,
                        int squash_frequency)
{
  int bricknum, rownum;
  float offset = 0.0;
  float x, y;

  rownum = floor_to_int(p.y / row_height);

  if (offset_frequency != 0 && squash_frequency != 0) {
    brick_width *= (rownum % squash_frequency != 0) ? 1.0 : squash_amount;           /* squash */
    offset = (rownum % offset_frequency != 0) ? 0.0 : (brick_width * offset_amount); /* offset */
  }

  bricknum = floor_to_int((p.x + offset) / brick_width);

  x = (p.x + offset) - brick_width * bricknum;
  y = p.y - row_height * rownum;

  float tint = clamp((integer_noise((rownum << 16) + (bricknum & 0xFFFF)) + bias), 0.0, 1.0);

  float min_dist = min(min(x, y), min(brick_width - x, row_height - y));
  if (min_dist >= mortar_size) {
    return vec2(tint, 0.0);
  }
  else if (mortar_smooth == 0.0) {
    return vec2(tint, 1.0);
  }
  else {
    min_dist = 1.0 - min_dist / mortar_size;
    return vec2(tint, smoothstep(0.0, mortar_smooth, min_dist));
  }
}

void node_tex_brick(vec3 co,
                    vec4 color1,
                    vec4 color2,
                    vec4 mortar,
                    float scale,
                    float mortar_size,
                    float mortar_smooth,
                    float bias,
                    float brick_width,
                    float row_height,
                    float offset_amount,
                    float offset_frequency,
                    float squash_amount,
                    float squash_frequency,
                    out vec4 color,
                    out float fac)
{
  vec2 f2 = calc_brick_texture(co * scale,
                               mortar_size,
                               mortar_smooth,
                               bias,
                               brick_width,
                               row_height,
                               offset_amount,
                               int(offset_frequency),
                               squash_amount,
                               int(squash_frequency));
  float tint = f2.x;
  float f = f2.y;
  if (f != 1.0) {
    float facm = 1.0 - tint;
    color1 = facm * color1 + tint * color2;
  }
  color = mix(color1, mortar, f);
  fac = f;
}

void node_tex_clouds(vec3 co, float size, out vec4 color, out float fac)
{
  color = vec4(1.0);
  fac = 1.0;
}

void node_tex_environment_equirectangular(vec3 co, float clamp_size, sampler2D ima, out vec3 uv)
{
  vec3 nco = normalize(co);
  uv.x = -atan(nco.y, nco.x) / (2.0 * M_PI) + 0.5;
  uv.y = atan(nco.z, hypot(nco.x, nco.y)) / M_PI + 0.5;

  /* Fix pole bleeding */
  float half_height = clamp_size / float(textureSize(ima, 0).y);
  uv.y = clamp(uv.y, half_height, 1.0 - half_height);
  uv.z = 0.0;
}

void node_tex_environment_mirror_ball(vec3 co, out vec3 uv)
{
  vec3 nco = normalize(co);
  nco.y -= 1.0;

  float div = 2.0 * sqrt(max(-0.5 * nco.y, 0.0));
  nco /= max(1e-8, div);

  uv = 0.5 * nco.xzz + 0.5;
}

void node_tex_environment_empty(vec3 co, out vec4 color)
{
  color = vec4(1.0, 0.0, 1.0, 1.0);
}

/* 16bits floats limits. Higher/Lower values produce +/-inf. */
#define safe_color(a) (clamp(a, -65520.0, 65520.0))

void tex_color_alpha_clear(vec4 color, out vec4 result)
{
  result = vec4(color.rgb, 1.0);
}

void tex_color_alpha_unpremultiply(vec4 color, out vec4 result)
{
  if (color.a == 0.0 || color.a == 1.0) {
    result = vec4(color.rgb, 1.0);
  }
  else {
    result = vec4(color.rgb / color.a, 1.0);
  }
}

void node_tex_image_linear(vec3 co, sampler2D ima, out vec4 color, out float alpha)
{
  color = safe_color(texture(ima, co.xy));
  alpha = color.a;
}

void node_tex_image_linear_no_mip(vec3 co, sampler2D ima, out vec4 color, out float alpha)
{
  color = safe_color(textureLod(ima, co.xy, 0.0));
  alpha = color.a;
}

void node_tex_image_nearest(vec3 co, sampler2D ima, out vec4 color, out float alpha)
{
  ivec2 pix = ivec2(fract(co.xy) * textureSize(ima, 0).xy);
  color = safe_color(texelFetch(ima, pix, 0));
  alpha = color.a;
}

/* @arg f: signed distance to texel center. */
void cubic_bspline_coefs(vec2 f, out vec2 w0, out vec2 w1, out vec2 w2, out vec2 w3)
{
  vec2 f2 = f * f;
  vec2 f3 = f2 * f;
  /* Bspline coefs (optimized) */
  w3 = f3 / 6.0;
  w0 = -w3 + f2 * 0.5 - f * 0.5 + 1.0 / 6.0;
  w1 = f3 * 0.5 - f2 * 1.0 + 2.0 / 3.0;
  w2 = 1.0 - w0 - w1 - w3;
}

void node_tex_image_cubic_ex(
    vec3 co, sampler2D ima, float do_extend, out vec4 color, out float alpha)
{
  vec2 tex_size = vec2(textureSize(ima, 0).xy);

  co.xy *= tex_size;
  /* texel center */
  vec2 tc = floor(co.xy - 0.5) + 0.5;
  vec2 w0, w1, w2, w3;
  cubic_bspline_coefs(co.xy - tc, w0, w1, w2, w3);

#if 1 /* Optimized version using 4 filtered tap. */
  vec2 s0 = w0 + w1;
  vec2 s1 = w2 + w3;

  vec2 f0 = w1 / (w0 + w1);
  vec2 f1 = w3 / (w2 + w3);

  vec4 final_co;
  final_co.xy = tc - 1.0 + f0;
  final_co.zw = tc + 1.0 + f1;

  if (do_extend == 1.0) {
    final_co = clamp(final_co, vec4(0.5), tex_size.xyxy - 0.5);
  }
  final_co /= tex_size.xyxy;

  color = safe_color(textureLod(ima, final_co.xy, 0.0)) * s0.x * s0.y;
  color += safe_color(textureLod(ima, final_co.zy, 0.0)) * s1.x * s0.y;
  color += safe_color(textureLod(ima, final_co.xw, 0.0)) * s0.x * s1.y;
  color += safe_color(textureLod(ima, final_co.zw, 0.0)) * s1.x * s1.y;

#else /* Reference bruteforce 16 tap. */
  color = texelFetch(ima, ivec2(tc + vec2(-1.0, -1.0)), 0) * w0.x * w0.y;
  color += texelFetch(ima, ivec2(tc + vec2(0.0, -1.0)), 0) * w1.x * w0.y;
  color += texelFetch(ima, ivec2(tc + vec2(1.0, -1.0)), 0) * w2.x * w0.y;
  color += texelFetch(ima, ivec2(tc + vec2(2.0, -1.0)), 0) * w3.x * w0.y;

  color += texelFetch(ima, ivec2(tc + vec2(-1.0, 0.0)), 0) * w0.x * w1.y;
  color += texelFetch(ima, ivec2(tc + vec2(0.0, 0.0)), 0) * w1.x * w1.y;
  color += texelFetch(ima, ivec2(tc + vec2(1.0, 0.0)), 0) * w2.x * w1.y;
  color += texelFetch(ima, ivec2(tc + vec2(2.0, 0.0)), 0) * w3.x * w1.y;

  color += texelFetch(ima, ivec2(tc + vec2(-1.0, 1.0)), 0) * w0.x * w2.y;
  color += texelFetch(ima, ivec2(tc + vec2(0.0, 1.0)), 0) * w1.x * w2.y;
  color += texelFetch(ima, ivec2(tc + vec2(1.0, 1.0)), 0) * w2.x * w2.y;
  color += texelFetch(ima, ivec2(tc + vec2(2.0, 1.0)), 0) * w3.x * w2.y;

  color += texelFetch(ima, ivec2(tc + vec2(-1.0, 2.0)), 0) * w0.x * w3.y;
  color += texelFetch(ima, ivec2(tc + vec2(0.0, 2.0)), 0) * w1.x * w3.y;
  color += texelFetch(ima, ivec2(tc + vec2(1.0, 2.0)), 0) * w2.x * w3.y;
  color += texelFetch(ima, ivec2(tc + vec2(2.0, 2.0)), 0) * w3.x * w3.y;
#endif

  alpha = color.a;
}

void node_tex_image_cubic(vec3 co, sampler2D ima, out vec4 color, out float alpha)
{
  node_tex_image_cubic_ex(co, ima, 0.0, color, alpha);
}

void node_tex_image_cubic_extend(vec3 co, sampler2D ima, out vec4 color, out float alpha)
{
  node_tex_image_cubic_ex(co, ima, 1.0, color, alpha);
}

void node_tex_image_smart(vec3 co, sampler2D ima, out vec4 color, out float alpha)
{
  /* use cubic for now */
  node_tex_image_cubic_ex(co, ima, 0.0, color, alpha);
}

void tex_box_sample_linear(
    vec3 texco, vec3 N, sampler2D ima, out vec4 color1, out vec4 color2, out vec4 color3)
{
  /* X projection */
  vec2 uv = texco.yz;
  if (N.x < 0.0) {
    uv.x = 1.0 - uv.x;
  }
  color1 = texture(ima, uv);
  /* Y projection */
  uv = texco.xz;
  if (N.y > 0.0) {
    uv.x = 1.0 - uv.x;
  }
  color2 = texture(ima, uv);
  /* Z projection */
  uv = texco.yx;
  if (N.z > 0.0) {
    uv.x = 1.0 - uv.x;
  }
  color3 = texture(ima, uv);
}

void tex_box_sample_nearest(
    vec3 texco, vec3 N, sampler2D ima, out vec4 color1, out vec4 color2, out vec4 color3)
{
  /* X projection */
  vec2 uv = texco.yz;
  if (N.x < 0.0) {
    uv.x = 1.0 - uv.x;
  }
  ivec2 pix = ivec2(uv.xy * textureSize(ima, 0).xy);
  color1 = texelFetch(ima, pix, 0);
  /* Y projection */
  uv = texco.xz;
  if (N.y > 0.0) {
    uv.x = 1.0 - uv.x;
  }
  pix = ivec2(uv.xy * textureSize(ima, 0).xy);
  color2 = texelFetch(ima, pix, 0);
  /* Z projection */
  uv = texco.yx;
  if (N.z > 0.0) {
    uv.x = 1.0 - uv.x;
  }
  pix = ivec2(uv.xy * textureSize(ima, 0).xy);
  color3 = texelFetch(ima, pix, 0);
}

void tex_box_sample_cubic(
    vec3 texco, vec3 N, sampler2D ima, out vec4 color1, out vec4 color2, out vec4 color3)
{
  float alpha;
  /* X projection */
  vec2 uv = texco.yz;
  if (N.x < 0.0) {
    uv.x = 1.0 - uv.x;
  }
  node_tex_image_cubic_ex(uv.xyy, ima, 0.0, color1, alpha);
  /* Y projection */
  uv = texco.xz;
  if (N.y > 0.0) {
    uv.x = 1.0 - uv.x;
  }
  node_tex_image_cubic_ex(uv.xyy, ima, 0.0, color2, alpha);
  /* Z projection */
  uv = texco.yx;
  if (N.z > 0.0) {
    uv.x = 1.0 - uv.x;
  }
  node_tex_image_cubic_ex(uv.xyy, ima, 0.0, color3, alpha);
}

void tex_box_sample_smart(
    vec3 texco, vec3 N, sampler2D ima, out vec4 color1, out vec4 color2, out vec4 color3)
{
  tex_box_sample_cubic(texco, N, ima, color1, color2, color3);
}

void node_tex_image_box(vec3 texco,
                        vec3 N,
                        vec4 color1,
                        vec4 color2,
                        vec4 color3,
                        sampler2D ima,
                        float blend,
                        out vec4 color,
                        out float alpha)
{
  /* project from direction vector to barycentric coordinates in triangles */
  N = abs(N);
  N /= dot(N, vec3(1.0));

  /* basic idea is to think of this as a triangle, each corner representing
   * one of the 3 faces of the cube. in the corners we have single textures,
   * in between we blend between two textures, and in the middle we a blend
   * between three textures.
   *
   * the Nxyz values are the barycentric coordinates in an equilateral
   * triangle, which in case of blending, in the middle has a smaller
   * equilateral triangle where 3 textures blend. this divides things into
   * 7 zones, with an if () test for each zone
   * EDIT: Now there is only 4 if's. */

  float limit = 0.5 + 0.5 * blend;

  vec3 weight;
  weight = N.xyz / (N.xyx + N.yzz);
  weight = clamp((weight - 0.5 * (1.0 - blend)) / max(1e-8, blend), 0.0, 1.0);

  /* test for mixes between two textures */
  if (N.z < (1.0 - limit) * (N.y + N.x)) {
    weight.z = 0.0;
    weight.y = 1.0 - weight.x;
  }
  else if (N.x < (1.0 - limit) * (N.y + N.z)) {
    weight.x = 0.0;
    weight.z = 1.0 - weight.y;
  }
  else if (N.y < (1.0 - limit) * (N.x + N.z)) {
    weight.y = 0.0;
    weight.x = 1.0 - weight.z;
  }
  else {
    /* last case, we have a mix between three */
    weight = ((2.0 - limit) * N + (limit - 1.0)) / max(1e-8, blend);
  }

  color = weight.x * color1 + weight.y * color2 + weight.z * color3;
  alpha = color.a;
}

void tex_clip_linear(vec3 co, sampler2D ima, vec4 icolor, out vec4 color, out float alpha)
{
  vec2 tex_size = vec2(textureSize(ima, 0).xy);
  vec2 minco = min(co.xy, 1.0 - co.xy);
  minco = clamp(minco * tex_size + 0.5, 0.0, 1.0);
  float fac = minco.x * minco.y;

  color = mix(vec4(0.0), icolor, fac);
  alpha = color.a;
}

void tex_clip_nearest(vec3 co, sampler2D ima, vec4 icolor, out vec4 color, out float alpha)
{
  vec4 minco = vec4(co.xy, 1.0 - co.xy);
  color = (any(lessThan(minco, vec4(0.0)))) ? vec4(0.0) : icolor;
  alpha = color.a;
}

void tex_clip_cubic(vec3 co, sampler2D ima, vec4 icolor, out vec4 color, out float alpha)
{
  vec2 tex_size = vec2(textureSize(ima, 0).xy);

  co.xy *= tex_size;
  /* texel center */
  vec2 tc = floor(co.xy - 0.5) + 0.5;
  vec2 w0, w1, w2, w3;
  cubic_bspline_coefs(co.xy - tc, w0, w1, w2, w3);

  /* TODO Optimize this part. I'm sure there is a smarter way to do that.
   * Could do that when sampling? */
#define CLIP_CUBIC_SAMPLE(samp, size) \
  (float(all(greaterThan(samp, vec2(-0.5)))) * float(all(lessThan(ivec2(samp), itex_size))))
  ivec2 itex_size = textureSize(ima, 0).xy;
  float fac;
  fac = CLIP_CUBIC_SAMPLE(tc + vec2(-1.0, -1.0), itex_size) * w0.x * w0.y;
  fac += CLIP_CUBIC_SAMPLE(tc + vec2(0.0, -1.0), itex_size) * w1.x * w0.y;
  fac += CLIP_CUBIC_SAMPLE(tc + vec2(1.0, -1.0), itex_size) * w2.x * w0.y;
  fac += CLIP_CUBIC_SAMPLE(tc + vec2(2.0, -1.0), itex_size) * w3.x * w0.y;

  fac += CLIP_CUBIC_SAMPLE(tc + vec2(-1.0, 0.0), itex_size) * w0.x * w1.y;
  fac += CLIP_CUBIC_SAMPLE(tc + vec2(0.0, 0.0), itex_size) * w1.x * w1.y;
  fac += CLIP_CUBIC_SAMPLE(tc + vec2(1.0, 0.0), itex_size) * w2.x * w1.y;
  fac += CLIP_CUBIC_SAMPLE(tc + vec2(2.0, 0.0), itex_size) * w3.x * w1.y;

  fac += CLIP_CUBIC_SAMPLE(tc + vec2(-1.0, 1.0), itex_size) * w0.x * w2.y;
  fac += CLIP_CUBIC_SAMPLE(tc + vec2(0.0, 1.0), itex_size) * w1.x * w2.y;
  fac += CLIP_CUBIC_SAMPLE(tc + vec2(1.0, 1.0), itex_size) * w2.x * w2.y;
  fac += CLIP_CUBIC_SAMPLE(tc + vec2(2.0, 1.0), itex_size) * w3.x * w2.y;

  fac += CLIP_CUBIC_SAMPLE(tc + vec2(-1.0, 2.0), itex_size) * w0.x * w3.y;
  fac += CLIP_CUBIC_SAMPLE(tc + vec2(0.0, 2.0), itex_size) * w1.x * w3.y;
  fac += CLIP_CUBIC_SAMPLE(tc + vec2(1.0, 2.0), itex_size) * w2.x * w3.y;
  fac += CLIP_CUBIC_SAMPLE(tc + vec2(2.0, 2.0), itex_size) * w3.x * w3.y;
#undef CLIP_CUBIC_SAMPLE

  color = mix(vec4(0.0), icolor, fac);
  alpha = color.a;
}

void tex_clip_smart(vec3 co, sampler2D ima, vec4 icolor, out vec4 color, out float alpha)
{
  tex_clip_cubic(co, ima, icolor, color, alpha);
}

void node_tex_image_empty(vec3 co, out vec4 color, out float alpha)
{
  color = vec4(0.0);
  alpha = 0.0;
}

void node_tex_magic(
    vec3 co, float scale, float distortion, float depth, out vec4 color, out float fac)
{
  vec3 p = co * scale;
  float x = sin((p.x + p.y + p.z) * 5.0);
  float y = cos((-p.x + p.y - p.z) * 5.0);
  float z = -cos((-p.x - p.y + p.z) * 5.0);

  if (depth > 0) {
    x *= distortion;
    y *= distortion;
    z *= distortion;
    y = -cos(x - y + z);
    y *= distortion;
    if (depth > 1) {
      x = cos(x - y - z);
      x *= distortion;
      if (depth > 2) {
        z = sin(-x - y - z);
        z *= distortion;
        if (depth > 3) {
          x = -cos(-x + y - z);
          x *= distortion;
          if (depth > 4) {
            y = -sin(-x + y + z);
            y *= distortion;
            if (depth > 5) {
              y = -cos(-x + y + z);
              y *= distortion;
              if (depth > 6) {
                x = cos(x + y + z);
                x *= distortion;
                if (depth > 7) {
                  z = sin(x + y - z);
                  z *= distortion;
                  if (depth > 8) {
                    x = -cos(-x - y + z);
                    x *= distortion;
                    if (depth > 9) {
                      y = -sin(x - y + z);
                      y *= distortion;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  if (distortion != 0.0) {
    distortion *= 2.0;
    x /= distortion;
    y /= distortion;
    z /= distortion;
  }

  color = vec4(0.5 - x, 0.5 - y, 0.5 - z, 1.0);
  fac = (color.x + color.y + color.z) / 3.0;
}

float noise_fade(float t)
{
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float noise_scale3(float result)
{
  return 0.9820 * result;
}

float noise_nerp(float t, float a, float b)
{
  return (1.0 - t) * a + t * b;
}

float noise_grad(uint hash, float x, float y, float z)
{
  uint h = hash & 15u;
  float u = h < 8u ? x : y;
  float vt = ((h == 12u) || (h == 14u)) ? x : z;
  float v = h < 4u ? y : vt;
  return (((h & 1u) != 0u) ? -u : u) + (((h & 2u) != 0u) ? -v : v);
}

float noise_perlin(float x, float y, float z)
{
  int X;
  float fx = floorfrac(x, X);
  int Y;
  float fy = floorfrac(y, Y);
  int Z;
  float fz = floorfrac(z, Z);

  float u = noise_fade(fx);
  float v = noise_fade(fy);
  float w = noise_fade(fz);

  float noise_u[2], noise_v[2];

  noise_u[0] = noise_nerp(
      u, noise_grad(hash(X, Y, Z), fx, fy, fz), noise_grad(hash(X + 1, Y, Z), fx - 1.0, fy, fz));

  noise_u[1] = noise_nerp(u,
                          noise_grad(hash(X, Y + 1, Z), fx, fy - 1.0, fz),
                          noise_grad(hash(X + 1, Y + 1, Z), fx - 1.0, fy - 1.0, fz));

  noise_v[0] = noise_nerp(v, noise_u[0], noise_u[1]);

  noise_u[0] = noise_nerp(u,
                          noise_grad(hash(X, Y, Z + 1), fx, fy, fz - 1.0),
                          noise_grad(hash(X + 1, Y, Z + 1), fx - 1.0, fy, fz - 1.0));

  noise_u[1] = noise_nerp(u,
                          noise_grad(hash(X, Y + 1, Z + 1), fx, fy - 1.0, fz - 1.0),
                          noise_grad(hash(X + 1, Y + 1, Z + 1), fx - 1.0, fy - 1.0, fz - 1.0));

  noise_v[1] = noise_nerp(v, noise_u[0], noise_u[1]);

  float r = noise_scale3(noise_nerp(w, noise_v[0], noise_v[1]));

  return (isinf(r)) ? 0.0 : r;
}

float noise(vec3 p)
{
  return 0.5 * noise_perlin(p.x, p.y, p.z) + 0.5;
}

float snoise(vec3 p)
{
  return noise_perlin(p.x, p.y, p.z);
}

float noise_turbulence(vec3 p, float octaves, int hard)
{
  float fscale = 1.0;
  float amp = 1.0;
  float sum = 0.0;
  octaves = clamp(octaves, 0.0, 16.0);
  int n = int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = noise(fscale * p);
    if (hard != 0) {
      t = abs(2.0 * t - 1.0);
    }
    sum += t * amp;
    amp *= 0.5;
    fscale *= 2.0;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = noise(fscale * p);
    if (hard != 0) {
      t = abs(2.0 * t - 1.0);
    }
    float sum2 = sum + t * amp;
    sum *= (float(1 << n) / float((1 << (n + 1)) - 1));
    sum2 *= (float(1 << (n + 1)) / float((1 << (n + 2)) - 1));
    return (1.0 - rmd) * sum + rmd * sum2;
  }
  else {
    sum *= (float(1 << n) / float((1 << (n + 1)) - 1));
    return sum;
  }
}

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

/* Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

float noise_musgrave_fBm(vec3 p, float H, float lacunarity, float octaves)
{
  float rmd;
  float value = 0.0;
  float pwr = 1.0;
  float pwHL = pow(lacunarity, -H);

  for (int i = 0; i < int(octaves); i++) {
    value += snoise(p) * pwr;
    pwr *= pwHL;
    p *= lacunarity;
  }

  rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    value += rmd * snoise(p) * pwr;
  }

  return value;
}

/* Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

float noise_musgrave_multi_fractal(vec3 p, float H, float lacunarity, float octaves)
{
  float rmd;
  float value = 1.0;
  float pwr = 1.0;
  float pwHL = pow(lacunarity, -H);

  for (int i = 0; i < int(octaves); i++) {
    value *= (pwr * snoise(p) + 1.0);
    pwr *= pwHL;
    p *= lacunarity;
  }

  rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    value *= (rmd * pwr * snoise(p) + 1.0); /* correct? */
  }

  return value;
}

/* Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float noise_musgrave_hetero_terrain(vec3 p, float H, float lacunarity, float octaves, float offset)
{
  float value, increment, rmd;
  float pwHL = pow(lacunarity, -H);
  float pwr = pwHL;

  /* first unscaled octave of function; later octaves are scaled */
  value = offset + snoise(p);
  p *= lacunarity;

  for (int i = 1; i < int(octaves); i++) {
    increment = (snoise(p) + offset) * pwr * value;
    value += increment;
    pwr *= pwHL;
    p *= lacunarity;
  }

  rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    increment = (snoise(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

/* Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float noise_musgrave_hybrid_multi_fractal(
    vec3 p, float H, float lacunarity, float octaves, float offset, float gain)
{
  float result, signal, weight, rmd;
  float pwHL = pow(lacunarity, -H);
  float pwr = pwHL;

  result = snoise(p) + offset;
  weight = gain * result;
  p *= lacunarity;

  for (int i = 1; (weight > 0.001f) && (i < int(octaves)); i++) {
    if (weight > 1.0) {
      weight = 1.0;
    }

    signal = (snoise(p) + offset) * pwr;
    pwr *= pwHL;
    result += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    result += rmd * ((snoise(p) + offset) * pwr);
  }

  return result;
}

/* Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float noise_musgrave_ridged_multi_fractal(
    vec3 p, float H, float lacunarity, float octaves, float offset, float gain)
{
  float result, signal, weight;
  float pwHL = pow(lacunarity, -H);
  float pwr = pwHL;

  signal = offset - abs(snoise(p));
  signal *= signal;
  result = signal;
  weight = 1.0;

  for (int i = 1; i < int(octaves); i++) {
    p *= lacunarity;
    weight = clamp(signal * gain, 0.0, 1.0);
    signal = offset - abs(snoise(p));
    signal *= signal;
    signal *= weight;
    result += signal * pwr;
    pwr *= pwHL;
  }

  return result;
}

float svm_musgrave(int type,
                   float dimension,
                   float lacunarity,
                   float octaves,
                   float offset,
                   float intensity,
                   float gain,
                   vec3 p)
{
  if (type == 0 /* NODE_MUSGRAVE_MULTIFRACTAL */) {
    return intensity * noise_musgrave_multi_fractal(p, dimension, lacunarity, octaves);
  }
  else if (type == 1 /* NODE_MUSGRAVE_FBM */) {
    return intensity * noise_musgrave_fBm(p, dimension, lacunarity, octaves);
  }
  else if (type == 2 /* NODE_MUSGRAVE_HYBRID_MULTIFRACTAL */) {
    return intensity *
           noise_musgrave_hybrid_multi_fractal(p, dimension, lacunarity, octaves, offset, gain);
  }
  else if (type == 3 /* NODE_MUSGRAVE_RIDGED_MULTIFRACTAL */) {
    return intensity *
           noise_musgrave_ridged_multi_fractal(p, dimension, lacunarity, octaves, offset, gain);
  }
  else if (type == 4 /* NODE_MUSGRAVE_HETERO_TERRAIN */) {
    return intensity * noise_musgrave_hetero_terrain(p, dimension, lacunarity, octaves, offset);
  }
  return 0.0;
}

void node_tex_musgrave(vec3 co,
                       float scale,
                       float detail,
                       float dimension,
                       float lacunarity,
                       float offset,
                       float gain,
                       float type,
                       out vec4 color,
                       out float fac)
{
  fac = svm_musgrave(int(type), dimension, lacunarity, detail, offset, 1.0, gain, co *scale);

  color = vec4(fac, fac, fac, 1.0);
}

void node_tex_sky(vec3 co, out vec4 color)
{
  color = vec4(1.0);
}

void node_tex_voronoi(vec3 co,
                      float scale,
                      float exponent,
                      float coloring,
                      float metric,
                      float feature,
                      out vec4 color,
                      out float fac)
{
  vec3 p = co * scale;
  int xx, yy, zz, xi, yi, zi;
  vec4 da = vec4(1e10);
  vec3 pa[4] = vec3[4](vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0));

  xi = floor_to_int(p[0]);
  yi = floor_to_int(p[1]);
  zi = floor_to_int(p[2]);

  for (xx = xi - 1; xx <= xi + 1; xx++) {
    for (yy = yi - 1; yy <= yi + 1; yy++) {
      for (zz = zi - 1; zz <= zi + 1; zz++) {
        vec3 ip = vec3(xx, yy, zz);
        vec3 vp = cellnoise_color(ip);
        vec3 pd = p - (vp + ip);

        float d = 0.0;
        if (metric == 0.0) { /* SHD_VORONOI_DISTANCE 0 */
          d = dot(pd, pd);
        }
        else if (metric == 1.0) { /* SHD_VORONOI_MANHATTAN 1 */
          d = abs(pd[0]) + abs(pd[1]) + abs(pd[2]);
        }
        else if (metric == 2.0) { /* SHD_VORONOI_CHEBYCHEV 2 */
          d = max(abs(pd[0]), max(abs(pd[1]), abs(pd[2])));
        }
        else if (metric == 3.0) { /* SHD_VORONOI_MINKOWSKI 3 */
          d = pow(pow(abs(pd[0]), exponent) + pow(abs(pd[1]), exponent) +
                      pow(abs(pd[2]), exponent),
                  1.0 / exponent);
        }

        vp += vec3(xx, yy, zz);
        if (d < da[0]) {
          da.yzw = da.xyz;
          da[0] = d;

          pa[3] = pa[2];
          pa[2] = pa[1];
          pa[1] = pa[0];
          pa[0] = vp;
        }
        else if (d < da[1]) {
          da.zw = da.yz;
          da[1] = d;

          pa[3] = pa[2];
          pa[2] = pa[1];
          pa[1] = vp;
        }
        else if (d < da[2]) {
          da[3] = da[2];
          da[2] = d;

          pa[3] = pa[2];
          pa[2] = vp;
        }
        else if (d < da[3]) {
          da[3] = d;
          pa[3] = vp;
        }
      }
    }
  }

  if (coloring == 0.0) {
    /* Intensity output */
    if (feature == 0.0) { /* F1 */
      fac = abs(da[0]);
    }
    else if (feature == 1.0) { /* F2 */
      fac = abs(da[1]);
    }
    else if (feature == 2.0) { /* F3 */
      fac = abs(da[2]);
    }
    else if (feature == 3.0) { /* F4 */
      fac = abs(da[3]);
    }
    else if (feature == 4.0) { /* F2F1 */
      fac = abs(da[1] - da[0]);
    }
    color = vec4(fac, fac, fac, 1.0);
  }
  else {
    /* Color output */
    vec3 col = vec3(fac, fac, fac);
    if (feature == 0.0) { /* F1 */
      col = pa[0];
    }
    else if (feature == 1.0) { /* F2 */
      col = pa[1];
    }
    else if (feature == 2.0) { /* F3 */
      col = pa[2];
    }
    else if (feature == 3.0) { /* F4 */
      col = pa[3];
    }
    else if (feature == 4.0) { /* F2F1 */
      col = abs(pa[1] - pa[0]);
    }

    color = vec4(cellnoise_color(col), 1.0);
    fac = (color.x + color.y + color.z) * (1.0 / 3.0);
  }
}

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
    n += distortion * noise_turbulence(p * detail_scale, detail, 0);
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

/* light path */

void node_light_path(out float is_camera_ray,
                     out float is_shadow_ray,
                     out float is_diffuse_ray,
                     out float is_glossy_ray,
                     out float is_singular_ray,
                     out float is_reflection_ray,
                     out float is_transmission_ray,
                     out float ray_length,
                     out float ray_depth,
                     out float diffuse_depth,
                     out float glossy_depth,
                     out float transparent_depth,
                     out float transmission_depth)
{
  /* Supported. */
  is_camera_ray = (rayType == EEVEE_RAY_CAMERA) ? 1.0 : 0.0;
  is_shadow_ray = (rayType == EEVEE_RAY_SHADOW) ? 1.0 : 0.0;
  is_diffuse_ray = (rayType == EEVEE_RAY_DIFFUSE) ? 1.0 : 0.0;
  is_glossy_ray = (rayType == EEVEE_RAY_GLOSSY) ? 1.0 : 0.0;
  /* Kind of supported. */
  is_singular_ray = is_glossy_ray;
  is_reflection_ray = is_glossy_ray;
  is_transmission_ray = is_glossy_ray;
  ray_depth = rayDepth;
  diffuse_depth = (is_diffuse_ray == 1.0) ? rayDepth : 0.0;
  glossy_depth = (is_glossy_ray == 1.0) ? rayDepth : 0.0;
  transmission_depth = (is_transmission_ray == 1.0) ? glossy_depth : 0.0;
  /* Not supported. */
  ray_length = 1.0;
  transparent_depth = 0.0;
}

void node_light_falloff(
    float strength, float tsmooth, out float quadratic, out float linear, out float constant)
{
  quadratic = strength;
  linear = strength;
  constant = strength;
}

void node_object_info(mat4 obmat,
                      vec4 info,
                      float mat_index,
                      out vec3 location,
                      out float object_index,
                      out float material_index,
                      out float random)
{
  location = obmat[3].xyz;
  object_index = info.x;
  material_index = mat_index;
  random = info.z;
}

void node_normal_map(vec4 info, vec4 tangent, vec3 normal, vec3 texnormal, out vec3 outnormal)
{
  if (all(equal(tangent, vec4(0.0, 0.0, 0.0, 1.0)))) {
    outnormal = normal;
    return;
  }
  tangent *= (gl_FrontFacing ? 1.0 : -1.0);
  vec3 B = tangent.w * cross(normal, tangent.xyz) * info.w;

  outnormal = texnormal.x * tangent.xyz + texnormal.y * B + texnormal.z * normal;
  outnormal = normalize(outnormal);
}

void node_bump(
    float strength, float dist, float height, vec3 N, vec3 surf_pos, float invert, out vec3 result)
{
  N = mat3(ViewMatrix) * normalize(N);
  dist *= invert;

  vec3 dPdx = dFdx(surf_pos);
  vec3 dPdy = dFdy(surf_pos);

  /* Get surface tangents from normal. */
  vec3 Rx = cross(dPdy, N);
  vec3 Ry = cross(N, dPdx);

  /* Compute surface gradient and determinant. */
  float det = dot(dPdx, Rx);

  float dHdx = dFdx(height);
  float dHdy = dFdy(height);
  vec3 surfgrad = dHdx * Rx + dHdy * Ry;

  strength = max(strength, 0.0);

  result = normalize(abs(det) * N - dist * sign(det) * surfgrad);
  result = normalize(mix(N, result, strength));

  result = mat3(ViewMatrixInverse) * result;
}

void node_bevel(float radius, vec3 N, out vec3 result)
{
  result = N;
}

void node_hair_info(out float is_strand,
                    out float intercept,
                    out float thickness,
                    out vec3 tangent,
                    out float random)
{
#ifdef HAIR_SHADER
  is_strand = 1.0;
  intercept = hairTime;
  thickness = hairThickness;
  tangent = normalize(worldNormal);
  random = wang_hash_noise(
      uint(hairStrandID)); /* TODO: could be precomputed per strand instead. */
#else
  is_strand = 0.0;
  intercept = 0.0;
  thickness = 0.0;
  tangent = vec3(1.0);
  random = 0.0;
#endif
}

void node_displacement_object(
    float height, float midlevel, float scale, vec3 N, mat4 obmat, out vec3 result)
{
  N = (vec4(N, 0.0) * obmat).xyz;
  result = (height - midlevel) * scale * normalize(N);
  result = (obmat * vec4(result, 0.0)).xyz;
}

void node_displacement_world(float height, float midlevel, float scale, vec3 N, out vec3 result)
{
  result = (height - midlevel) * scale * normalize(N);
}

void node_vector_displacement_tangent(vec4 vector,
                                      float midlevel,
                                      float scale,
                                      vec4 tangent,
                                      vec3 normal,
                                      mat4 obmat,
                                      mat4 viewmat,
                                      out vec3 result)
{
  /* TODO(fclem) this is broken. revisit latter. */
  vec3 N_object = normalize(((vec4(normal, 0.0) * viewmat) * obmat).xyz);
  vec3 T_object = normalize(((vec4(tangent.xyz, 0.0) * viewmat) * obmat).xyz);
  vec3 B_object = tangent.w * normalize(cross(N_object, T_object));

  vec3 offset = (vector.xyz - vec3(midlevel)) * scale;
  result = offset.x * T_object + offset.y * N_object + offset.z * B_object;
  result = (obmat * vec4(result, 0.0)).xyz;
}

void node_vector_displacement_object(
    vec4 vector, float midlevel, float scale, mat4 obmat, out vec3 result)
{
  result = (vector.xyz - vec3(midlevel)) * scale;
  result = (obmat * vec4(result, 0.0)).xyz;
}

void node_vector_displacement_world(vec4 vector, float midlevel, float scale, out vec3 result)
{
  result = (vector.xyz - vec3(midlevel)) * scale;
}

/* output */

void node_output_material(Closure surface, Closure volume, vec3 displacement, out Closure result)
{
#ifdef VOLUMETRICS
  result = volume;
#else
  result = surface;
#endif
}

uniform float backgroundAlpha;

void node_output_world(Closure surface, Closure volume, out Closure result)
{
#ifndef VOLUMETRICS
  result.radiance = surface.radiance * backgroundAlpha;
  result.opacity = backgroundAlpha;
#else
  result = volume;
#endif /* VOLUMETRICS */
}

/* TODO : clean this ifdef mess */
/* EEVEE output */
void world_normals_get(out vec3 N)
{
#ifndef VOLUMETRICS
#  ifdef HAIR_SHADER
  vec3 B = normalize(cross(worldNormal, hairTangent));
  float cos_theta;
  if (hairThicknessRes == 1) {
    vec4 rand = texelFetch(utilTex, ivec3(ivec2(gl_FragCoord.xy) % LUT_SIZE, 2.0), 0);
    /* Random cosine normal distribution on the hair surface. */
    cos_theta = rand.x * 2.0 - 1.0;
  }
  else {
    /* Shade as a cylinder. */
    cos_theta = hairThickTime / hairThickness;
  }
  float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
  N = normalize(worldNormal * sin_theta + B * cos_theta);
#  else
  N = gl_FrontFacing ? worldNormal : -worldNormal;
#  endif
#else
  generated_from_orco(vec3(0.0), N);
#endif
}

#ifndef VOLUMETRICS
void node_eevee_specular(vec4 diffuse,
                         vec4 specular,
                         float roughness,
                         vec4 emissive,
                         float transp,
                         vec3 normal,
                         float clearcoat,
                         float clearcoat_roughness,
                         vec3 clearcoat_normal,
                         float occlusion,
                         float ssr_id,
                         out Closure result)
{
  vec3 out_diff, out_spec, ssr_spec;
  eevee_closure_default_clearcoat(normal,
                                  diffuse.rgb,
                                  specular.rgb,
                                  int(ssr_id),
                                  roughness,
                                  clearcoat_normal,
                                  clearcoat * 0.25,
                                  clearcoat_roughness,
                                  occlusion,
                                  out_diff,
                                  out_spec,
                                  ssr_spec);

  vec3 vN = normalize(mat3(ViewMatrix) * normal);
  result = CLOSURE_DEFAULT;
  result.radiance = out_diff * diffuse.rgb + out_spec + emissive.rgb;
  result.opacity = 1.0 - transp;
  result.ssr_data = vec4(ssr_spec, roughness);
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  result.ssr_id = int(ssr_id);
}

void node_shader_to_rgba(Closure cl, out vec4 outcol, out float outalpha)
{
  vec4 spec_accum = vec4(0.0);
  if (ssrToggle && cl.ssr_id == outputSsrId) {
    vec3 V = cameraVec;
    vec3 vN = normal_decode(cl.ssr_normal, viewCameraVec);
    vec3 N = transform_direction(ViewMatrixInverse, vN);
    float roughness = cl.ssr_data.a;
    float roughnessSquared = max(1e-3, roughness * roughness);
    fallback_cubemap(N, V, worldPosition, viewPosition, roughness, roughnessSquared, spec_accum);
  }

  outalpha = cl.opacity;
  outcol = vec4((spec_accum.rgb * cl.ssr_data.rgb) + cl.radiance, 1.0);

#  ifdef USE_SSS
#    ifdef USE_SSS_ALBEDO
  outcol.rgb += cl.sss_data.rgb * cl.sss_albedo;
#    else
  outcol.rgb += cl.sss_data.rgb;
#    endif
#  endif
}

#endif /* VOLUMETRICS */
