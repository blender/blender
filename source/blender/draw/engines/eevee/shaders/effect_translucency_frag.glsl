/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(lights_lib.glsl)

vec3 sss_profile(float s)
{
  s /= radii_max_radius.w * avg_inv_radius;
  return texture(sssTexProfile, saturate(s) * SSS_LUT_SCALE + SSS_LUT_BIAS).rgb;
}

float light_translucent_power_with_falloff(LightData ld, vec3 N, vec4 l_vector)
{
  float power, falloff;
  /* XXX: Removing Area Power. */
  /* TODO: put this out of the shader. */
  if (ld.l_type >= AREA_RECT) {
    power = (ld.l_sizex * ld.l_sizey * 4.0 * M_PI) * (1.0 / 80.0);
    if (ld.l_type == AREA_ELLIPSE) {
      power *= M_PI_4;
    }
    power *= 0.3 * 20.0 *
             max(0.0, dot(-ld.l_forward, l_vector.xyz / l_vector.w)); /* XXX ad hoc, empirical */
    power /= (l_vector.w * l_vector.w);
    falloff = dot(N, l_vector.xyz / l_vector.w);
  }
  else if (ld.l_type == SUN) {
    power = 1.0 / (1.0 + (ld.l_radius * ld.l_radius * 0.5));
    power *= ld.l_radius * ld.l_radius * M_PI; /* Removing area light power. */
    power *= M_2PI * 0.78;                     /* Matching cycles with point light. */
    power *= 0.082;                            /* XXX ad hoc, empirical */
    falloff = dot(N, -ld.l_forward);
  }
  else {
    power = (4.0 * ld.l_radius * ld.l_radius) * (1.0 / 10.0);
    power *= 1.5; /* XXX ad hoc, empirical */
    power /= (l_vector.w * l_vector.w);
    falloff = dot(N, l_vector.xyz / l_vector.w);
  }
  /* No transmittance at grazing angle (hide artifacts) */
  return power * saturate(falloff * 2.0);
}

/* Some driver poorly optimize this code. Use direct reference to matrices. */
#define sd(x) shadows_data[x]
#define scube(x) shadows_cube_data[x]
#define scascade(x) shadows_cascade_data[x]

float shadow_cube_radial_depth(vec3 cubevec, float tex_id, int shadow_id)
{
  float depth = sample_cube(sssShadowCubes, cubevec, tex_id).r;
  /* To reverting the constant bias from shadow rendering. (Tweaked for 16bit shadowmaps) */
  const float depth_bias = 3.1e-5;
  depth = saturate(depth - depth_bias);

  depth = linear_depth(true, depth, sd(shadow_id).sh_far, sd(shadow_id).sh_near);
  depth *= length(cubevec / max_v3(abs(cubevec)));
  return depth;
}

vec3 light_translucent(LightData ld, vec3 P, vec3 N, vec4 l_vector, vec2 rand, float sss_scale)
{
  int shadow_id = int(ld.l_shadowid);

  vec4 L = (ld.l_type != SUN) ? l_vector : vec4(-ld.l_forward, 1.0);

  /* We use the full l_vector.xyz so that the spread is minimize
   * if the shading point is further away from the light source */
  /* TODO(fclem): do something better than this. */
  vec3 T, B;
  make_orthonormal_basis(L.xyz / L.w, T, B);

  vec3 n;
  vec4 depths;
  float d, dist;
  int data_id = int(sd(shadow_id).sh_data_index);
  if (ld.l_type == SUN) {
    vec4 view_z = vec4(dot(P - cameraPos, cameraForward));

    vec4 weights = step(scascade(data_id).split_end_distances, view_z);
    float id = abs(4.0 - dot(weights, weights));
    if (id > 3.0) {
      return vec3(0.0);
    }

    /* Same factor as in get_cascade_world_distance(). */
    float range = abs(sd(shadow_id).sh_far - sd(shadow_id).sh_near);

    vec4 shpos = scascade(data_id).shadowmat[int(id)] * vec4(P, 1.0);
    dist = shpos.z * range;

    if (shpos.z > 1.0 || shpos.z < 0.0) {
      return vec3(0.0);
    }

    float tex_id = scascade(data_id).sh_tex_index + id;

    /* Assume cascades have same height and width. */
    vec2 ofs = vec2(1.0, 0.0) / float(textureSize(sssShadowCascades, 0).x);
    d = sample_cascade(sssShadowCascades, shpos.xy, tex_id).r;
    depths.x = sample_cascade(sssShadowCascades, shpos.xy + ofs.xy, tex_id).r;
    depths.y = sample_cascade(sssShadowCascades, shpos.xy + ofs.yx, tex_id).r;
    depths.z = sample_cascade(sssShadowCascades, shpos.xy - ofs.xy, tex_id).r;
    depths.w = sample_cascade(sssShadowCascades, shpos.xy - ofs.yx, tex_id).r;

    /* To reverting the constant bias from shadow rendering. (Tweaked for 16bit shadowmaps) */
    float depth_bias = 3.1e-5;
    depths = saturate(depths - depth_bias);
    d = saturate(d - depth_bias);

    /* Size of a texel in world space.
     * FIXME This is only correct if l_right is the same right vector used for shadowmap creation.
     * This won't work if the shadow matrix is rotated (soft shadows).
     * TODO: precompute. */
    float unit_world_in_uv_space = length(mat3(scascade(data_id).shadowmat[int(id)]) * ld.l_right);
    float dx_scale = 2.0 * ofs.x / unit_world_in_uv_space;

    d *= range;
    depths *= range;

    /* This is the normal of the occluder in world space. */
    // vec3 T = ld.l_forward * dx + ld.l_right * dx_scale;
    // vec3 B = ld.l_forward * dy + ld.l_up * dx_scale;
    // n = normalize(cross(T, B));
  }
  else {
    float ofs = 1.0 / float(textureSize(sssShadowCubes, 0).x);

    vec3 cubevec = transform_point(scube(data_id).shadowmat, P);
    dist = length(cubevec);
    cubevec /= dist;
    /* tex_id == data_id for cube shadowmap */
    float tex_id = float(data_id);
    d = shadow_cube_radial_depth(cubevec, tex_id, shadow_id);
    /* NOTE: The offset is irregular in respect to cubeface uvs. But it has
     * a much more uniform behavior than biasing based on face derivatives. */
    depths.x = shadow_cube_radial_depth(cubevec + T * ofs, tex_id, shadow_id);
    depths.y = shadow_cube_radial_depth(cubevec + B * ofs, tex_id, shadow_id);
    depths.z = shadow_cube_radial_depth(cubevec - T * ofs, tex_id, shadow_id);
    depths.w = shadow_cube_radial_depth(cubevec - B * ofs, tex_id, shadow_id);
  }

  float dx = depths.x - depths.z;
  float dy = depths.y - depths.w;

  float s = min(d, min_v4(depths));

  /* To avoid light leak from depth discontinuity and shadowmap aliasing. */
  float slope_bias = (abs(dx) + abs(dy)) * 0.5;
  s -= slope_bias;

  float delta = dist - s;

  float power = light_translucent_power_with_falloff(ld, N, l_vector);

  return power * sss_profile(abs(delta) / sss_scale);
}

#undef sd
#undef scube
#undef scsmd

/* Similar to https://atyuwen.github.io/posts/normal-reconstruction/.
 * This samples the depth buffer 4 time for each direction to get the most correct
 * implicit normal reconstruction out of the depth buffer. */
vec3 view_position_derivative_from_depth(vec2 uvs, vec2 ofs, vec3 vP, float depth_center)
{
  vec2 uv1 = uvs - ofs * 2.0;
  vec2 uv2 = uvs - ofs;
  vec2 uv3 = uvs + ofs;
  vec2 uv4 = uvs + ofs * 2.0;
  vec4 H;
  H.x = textureLod(depthBuffer, uv1, 0.0).r;
  H.y = textureLod(depthBuffer, uv2, 0.0).r;
  H.z = textureLod(depthBuffer, uv3, 0.0).r;
  H.w = textureLod(depthBuffer, uv4, 0.0).r;
  /* Fix issue with depth precision. Take even larger diff. */
  vec4 diff = abs(vec4(depth_center, H.yzw) - H.x);
  if (max_v4(diff) < 2.4e-7 && all(lessThan(diff.xyz, diff.www))) {
    return 0.25 * (get_view_space_from_depth(uv3, H.w) - get_view_space_from_depth(uv1, H.x));
  }
  /* Simplified (H.xw + 2.0 * (H.yz - H.xw)) - depth_center */
  vec2 deltas = abs((2.0 * H.yz - H.xw) - depth_center);
  if (deltas.x < deltas.y) {
    return vP - get_view_space_from_depth(uv2, H.y);
  }
  else {
    return get_view_space_from_depth(uv3, H.z) - vP;
  }
}

/* TODO(@fclem): port to a common place for other effects to use. */
bool reconstruct_view_position_and_normal_from_depth(vec2 uvs, out vec3 vP, out vec3 vNg)
{
  vec2 texel_size = vec2(abs(dFdx(uvs.x)), abs(dFdy(uvs.y)));
  float depth_center = textureLod(depthBuffer, uvs, 0.0).r;

  vP = get_view_space_from_depth(uvs, depth_center);

  vec3 dPdx = view_position_derivative_from_depth(uvs, texel_size * vec2(1, 0), vP, depth_center);
  vec3 dPdy = view_position_derivative_from_depth(uvs, texel_size * vec2(0, 1), vP, depth_center);

  vNg = safe_normalize(cross(dPdx, dPdy));

  /* Background case. */
  if (depth_center == 1.0) {
    return false;
  }

  return true;
}

void main(void)
{
  vec2 uvs = uvcoordsvar.xy;
  float sss_scale = texture(sssRadius, uvs).r;

  vec3 rand = texelfetch_noise_tex(gl_FragCoord.xy).zwy;
  rand.xy *= fast_sqrt(rand.z);

  vec3 vP, vNg;
  reconstruct_view_position_and_normal_from_depth(uvs, vP, vNg);

  vec3 P = point_view_to_world(vP);
  vec3 Ng = normal_view_to_world(vNg);

  vec3 accum = vec3(0.0);
  for (int i = 0; i < MAX_LIGHT && i < laNumLight; i++) {
    LightData ld = lights_data[i];

    /* Only shadowed light can produce translucency */
    if (ld.l_shadowid < 0.0) {
      continue;
    }

    vec4 l_vector; /* Non-Normalized Light Vector with length in last component. */
    l_vector.xyz = ld.l_position - P;
    l_vector.w = length(l_vector.xyz);

    float att = light_attenuation(ld, l_vector);
    if (att < 1e-8) {
      continue;
    }

    accum += att * ld.l_color * light_translucent(ld, P, -Ng, l_vector, rand.xy, sss_scale);
  }

  FragColor = vec4(accum, 1.0);
}
