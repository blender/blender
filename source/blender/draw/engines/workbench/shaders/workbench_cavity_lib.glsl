
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_data_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)

layout(std140) uniform samples_block
{
  vec4 samples_coords[512];
};

uniform sampler2D cavityJitter;

/*  From The Alchemy screen-space ambient obscurance algorithm
 * http://graphics.cs.williams.edu/papers/AlchemyHPG11/VV11AlchemyAO.pdf */

void cavity_compute(vec2 screenco,
                    sampler2D depthBuffer,
                    sampler2D normalBuffer,
                    out float cavities,
                    out float edges)
{
  cavities = edges = 0.0;

  float depth = texture(depthBuffer, screenco).x;

  /* Early out if background and infront. */
  if (depth == 1.0 || depth == 0.0) {
    return;
  }

  vec3 position = view_position_from_depth(screenco, depth, world_data.viewvecs, ProjectionMatrix);
  vec3 normal = workbench_normal_decode(texture(normalBuffer, screenco));

  vec2 jitter_co = (screenco * world_data.viewport_size.xy) * world_data.cavity_jitter_scale;
  vec3 noise = texture(cavityJitter, jitter_co).rgb;

  /* find the offset in screen space by multiplying a point
   * in camera space at the depth of the point by the projection matrix. */
  vec2 offset;
  float homcoord = ProjectionMatrix[2][3] * position.z + ProjectionMatrix[3][3];
  offset.x = ProjectionMatrix[0][0] * world_data.cavity_distance / homcoord;
  offset.y = ProjectionMatrix[1][1] * world_data.cavity_distance / homcoord;
  /* convert from -1.0...1.0 range to 0.0..1.0 for easy use with texture coordinates */
  offset *= 0.5;

  /* Note. Putting noise usage here to put some ALU after texture fetch. */
  vec2 rotX = noise.rg;
  vec2 rotY = vec2(-rotX.y, rotX.x);

  int sample_start = world_data.cavity_sample_start;
  int sample_end = world_data.cavity_sample_end;
  for (int i = sample_start; i < sample_end && i < 512; i++) {
    /* sample_coord.xy is sample direction (normalized).
     * sample_coord.z is sample distance from disk center. */
    vec3 sample_coord = samples_coords[i].xyz;
    /* Rotate with random direction to get jittered result. */
    vec2 dir_jittered = vec2(dot(sample_coord.xy, rotX), dot(sample_coord.xy, rotY));
    dir_jittered.xy *= sample_coord.z + noise.b;

    vec2 uvcoords = screenco + dir_jittered * offset;
    /* Out of screen case. */
    if (any(greaterThan(abs(uvcoords - 0.5), vec2(0.5)))) {
      continue;
    }
    /* Sample depth. */
    float s_depth = texture(depthBuffer, uvcoords).r;
    /* Handle Background case */
    bool is_background = (s_depth == 1.0);
    /* This trick provide good edge effect even if no neighbor is found. */
    s_depth = (is_background) ? depth : s_depth;
    vec3 s_pos = view_position_from_depth(
        uvcoords, s_depth, world_data.viewvecs, ProjectionMatrix);

    if (is_background) {
      s_pos.z -= world_data.cavity_distance;
    }

    vec3 dir = s_pos - position;
    float len = length(dir);
    float f_cavities = dot(dir, normal);
    float f_edge = -f_cavities;
    float f_bias = 0.05 * len + 0.0001;

    float attenuation = 1.0 / (len * (1.0 + len * len * world_data.cavity_attenuation));

    /* use minor bias here to avoid self shadowing */
    if (f_cavities > -f_bias) {
      cavities += f_cavities * attenuation;
    }

    if (f_edge > f_bias) {
      edges += f_edge * attenuation;
    }
  }
  cavities *= world_data.cavity_sample_count_inv;
  edges *= world_data.cavity_sample_count_inv;

  /* don't let cavity wash out the surface appearance */
  cavities = clamp(cavities * world_data.cavity_valley_factor, 0.0, 1.0);
  edges = edges * world_data.cavity_ridge_factor;
}
