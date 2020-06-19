/**
 * Shaders that down-sample velocity buffer,
 *
 * Based on:
 * A Fast and Stable Feature-Aware Motion Blur Filter
 * by Jean-Philippe Guertin, Morgan McGuire, Derek Nowrouzezahrai
 *
 * Adapted from G3D Innovation Engine implementation.
 */

uniform sampler2D velocityBuffer;
uniform vec2 viewportSize;
uniform vec2 viewportSizeInv;
uniform ivec2 velocityBufferSize;

out vec4 tileMaxVelocity;

vec4 sample_velocity(ivec2 texel)
{
  texel = clamp(texel, ivec2(0), velocityBufferSize - 1);
  vec4 data = texelFetch(velocityBuffer, texel, 0);
  /* Decode data. */
  return (data * 2.0 - 1.0) * viewportSize.xyxy;
}

vec4 encode_velocity(vec4 velocity)
{
  return velocity * viewportSizeInv.xyxy * 0.5 + 0.5;
}

#ifdef TILE_GATHER

uniform ivec2 gatherStep;

void main()
{
  vec4 max_motion = vec4(0.0);
  float max_motion_len_sqr_prev = 0.0;
  float max_motion_len_sqr_next = 0.0;
  ivec2 texel = ivec2(gl_FragCoord.xy);
  texel = texel * gatherStep.yx + texel * EEVEE_VELOCITY_TILE_SIZE * gatherStep;

  for (int i = 0; i < EEVEE_VELOCITY_TILE_SIZE; ++i) {
    vec4 motion = sample_velocity(texel + i * gatherStep);
    float motion_len_sqr_prev = dot(motion.xy, motion.xy);
    float motion_len_sqr_next = dot(motion.zw, motion.zw);

    if (motion_len_sqr_prev > max_motion_len_sqr_prev) {
      max_motion_len_sqr_prev = motion_len_sqr_prev;
      max_motion.xy = motion.xy;
    }
    if (motion_len_sqr_next > max_motion_len_sqr_next) {
      max_motion_len_sqr_next = motion_len_sqr_next;
      max_motion.zw = motion.zw;
    }
  }

  tileMaxVelocity = encode_velocity(max_motion);
}

#else /* TILE_EXPANSION */

bool neighbor_affect_this_tile(ivec2 offset, vec2 velocity)
{
  /* Manhattan distance to the tiles, which is used for
   * differentiating corners versus middle blocks */
  float displacement = float(abs(offset.x) + abs(offset.y));
  /**
   * Relative sign on each axis of the offset compared
   * to the velocity for that tile.  In order for a tile
   * to affect the center tile, it must have a
   * neighborhood velocity in which x and y both have
   * identical or both have opposite signs relative to
   * offset. If the offset coordinate is zero then
   * velocity is irrelevant.
   **/
  vec2 point = sign(offset * velocity);

  float dist = (point.x + point.y);
  /**
   * Here's an example of the logic for this code.
   * In this diagram, the upper-left tile has offset = (-1, -1).
   * V1 is velocity = (1, -2). point in this case = (-1, 1), and therefore dist = 0,
   * so the upper-left tile does not affect the center.
   *
   * Now, look at another case. V2 = (-1, -2). point = (1, 1), so dist = 2 and the tile
   * does affect the center.
   *
   * V2(-1,-2)  V1(1, -2)
   *        \    /
   *         \  /
   *          \/___ ____ ____
   *  (-1, -1)|    |    |    |
   *          |____|____|____|
   *          |    |    |    |
   *          |____|____|____|
   *          |    |    |    |
   *          |____|____|____|
   **/
  return (abs(dist) == displacement);
}

/**
 * Only gather neighborhood velocity into tiles that could be affected by it.
 * In the general case, only six of the eight neighbors contribute:
 *
 *  This tile can't possibly be affected by the center one
 *     |
 *     v
 *    ____ ____ ____
 *   |    | ///|/// |
 *   |____|////|//__|
 *   |    |////|/   |
 *   |___/|////|____|
 *   |  //|////|    | <--- This tile can't possibly be affected by the center one
 *   |_///|///_|____|
 **/
void main()
{
  vec4 max_motion = vec4(0.0);
  float max_motion_len_sqr_prev = -1.0;
  float max_motion_len_sqr_next = -1.0;

  ivec2 tile = ivec2(gl_FragCoord.xy);
  ivec2 offset = ivec2(0);
  for (offset.y = -1; offset.y <= 1; ++offset.y) {
    for (offset.x = -1; offset.x <= 1; ++offset.x) {
      vec4 motion = sample_velocity(tile + offset);
      float motion_len_sqr_prev = dot(motion.xy, motion.xy);
      float motion_len_sqr_next = dot(motion.zw, motion.zw);

      if (motion_len_sqr_prev > max_motion_len_sqr_prev) {
        if (neighbor_affect_this_tile(offset, motion.xy)) {
          max_motion_len_sqr_prev = motion_len_sqr_prev;
          max_motion.xy = motion.xy;
        }
      }

      if (motion_len_sqr_next > max_motion_len_sqr_next) {
        if (neighbor_affect_this_tile(offset, motion.zw)) {
          max_motion_len_sqr_next = motion_len_sqr_next;
          max_motion.zw = motion.zw;
        }
      }
    }
  }

  tileMaxVelocity = encode_velocity(max_motion);
}

#endif