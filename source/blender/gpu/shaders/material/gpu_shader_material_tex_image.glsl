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

/* 16bits floats limits. Higher/Lower values produce +/-inf. */
#define safe_color(a) (clamp(a, -65520.0, 65520.0))

void node_tex_image_linear(vec3 co, sampler2D ima, out vec4 color, out float alpha)
{
  color = safe_color(texture(ima, co.xy));
  alpha = color.a;
}

/** \param f: Signed distance to texel center. */
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
  ivec2 pix = ivec2(fract(uv.xy) * textureSize(ima, 0).xy);
  color1 = texelFetch(ima, pix, 0);
  /* Y projection */
  uv = texco.xz;
  if (N.y > 0.0) {
    uv.x = 1.0 - uv.x;
  }
  pix = ivec2(fract(uv.xy) * textureSize(ima, 0).xy);
  color2 = texelFetch(ima, pix, 0);
  /* Z projection */
  uv = texco.yx;
  if (N.z > 0.0) {
    uv.x = 1.0 - uv.x;
  }
  pix = ivec2(fract(uv.xy) * textureSize(ima, 0).xy);
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

bool node_tex_tile_lookup(inout vec3 co, sampler2DArray ima, sampler1DArray map)
{
  vec2 tile_pos = floor(co.xy);

  if (tile_pos.x < 0 || tile_pos.y < 0 || tile_pos.x >= 10)
    return false;

  float tile = 10 * tile_pos.y + tile_pos.x;
  if (tile >= textureSize(map, 0).x)
    return false;

  /* Fetch tile information. */
  float tile_layer = texelFetch(map, ivec2(tile, 0), 0).x;
  if (tile_layer < 0)
    return false;

  vec4 tile_info = texelFetch(map, ivec2(tile, 1), 0);

  co = vec3(((co.xy - tile_pos) * tile_info.zw) + tile_info.xy, tile_layer);
  return true;
}

void node_tex_tile_linear(
    vec3 co, sampler2DArray ima, sampler1DArray map, out vec4 color, out float alpha)
{
  if (node_tex_tile_lookup(co, ima, map)) {
    color = safe_color(texture(ima, co));
  }
  else {
    color = vec4(1.0, 0.0, 1.0, 1.0);
  }

  alpha = color.a;
}

void node_tex_tile_nearest(
    vec3 co, sampler2DArray ima, sampler1DArray map, out vec4 color, out float alpha)
{
  if (node_tex_tile_lookup(co, ima, map)) {
    ivec3 pix = ivec3(fract(co.xy) * textureSize(ima, 0).xy, co.z);
    color = safe_color(texelFetch(ima, pix, 0));
  }
  else {
    color = vec4(1.0, 0.0, 1.0, 1.0);
  }

  alpha = color.a;
}

void node_tex_tile_cubic(
    vec3 co, sampler2DArray ima, sampler1DArray map, out vec4 color, out float alpha)
{
  if (node_tex_tile_lookup(co, ima, map)) {
    vec2 tex_size = vec2(textureSize(ima, 0).xy);

    co.xy *= tex_size;
    /* texel center */
    vec2 tc = floor(co.xy - 0.5) + 0.5;
    vec2 w0, w1, w2, w3;
    cubic_bspline_coefs(co.xy - tc, w0, w1, w2, w3);

    vec2 s0 = w0 + w1;
    vec2 s1 = w2 + w3;

    vec2 f0 = w1 / (w0 + w1);
    vec2 f1 = w3 / (w2 + w3);

    vec4 final_co;
    final_co.xy = tc - 1.0 + f0;
    final_co.zw = tc + 1.0 + f1;
    final_co /= tex_size.xyxy;

    color = safe_color(textureLod(ima, vec3(final_co.xy, co.z), 0.0)) * s0.x * s0.y;
    color += safe_color(textureLod(ima, vec3(final_co.zy, co.z), 0.0)) * s1.x * s0.y;
    color += safe_color(textureLod(ima, vec3(final_co.xw, co.z), 0.0)) * s0.x * s1.y;
    color += safe_color(textureLod(ima, vec3(final_co.zw, co.z), 0.0)) * s1.x * s1.y;
  }
  else {
    color = vec4(1.0, 0.0, 1.0, 1.0);
  }

  alpha = color.a;
}

void node_tex_tile_smart(
    vec3 co, sampler2DArray ima, sampler1DArray map, out vec4 color, out float alpha)
{
  node_tex_tile_cubic(co, ima, map, color, alpha);
}
