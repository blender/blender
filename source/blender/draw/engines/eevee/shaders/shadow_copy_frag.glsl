/* Copy the depth only shadowmap into another texture while converting
 * to linear depth (or other storage method) and doing a 3x3 box filter. */

layout(std140) uniform shadow_render_block
{
  /* Use vectors to avoid alignement padding. */
  ivec4 shadowSampleCount;
  vec4 shadowInvSampleCount;
  vec4 filterSize;
  int viewCount;
  int baseId;
  float cubeTexelSize;
  float storedTexelSize;
  float nearClip;
  float farClip;
  float exponent;
};

#ifdef CSM
uniform sampler2DArray shadowTexture;
#else
uniform samplerCube shadowTexture;
#endif

flat in int layerID;

#ifdef CSM
#  define cascadeID layerID
#else
#  define cascadeID 0
#endif

out vec4 FragColor;

#define linear_depth(z) \
  ((nearClip * farClip) / (clamp(z, 0.0, 0.999999) * (nearClip - farClip) + farClip))

/* add bias so background filtering does not bleed into shadow map */
#define BACKGROUND_BIAS 0.05

#ifdef CSM
vec4 get_world_distance(vec4 depths, vec3 cos[4])
{
  depths += step(vec4(0.9999), depths) * BACKGROUND_BIAS;
  return clamp(
      depths * abs(farClip - nearClip), 0.0, 1e10); /* Same factor as in shadow_cascade(). */
}

float get_world_distance(float depth, vec3 cos)
{
  depth += step(0.9999, depth) * BACKGROUND_BIAS;
  return clamp(
      depth * abs(farClip - nearClip), 0.0, 1e10); /* Same factor as in shadow_cascade(). */
}

#else /* CUBEMAP */
vec4 get_world_distance(vec4 depths, vec3 cos[4])
{
  depths = linear_depth(depths);
  cos[0] = normalize(abs(cos[0]));
  cos[1] = normalize(abs(cos[1]));
  cos[2] = normalize(abs(cos[2]));
  cos[3] = normalize(abs(cos[3]));
  vec4 cos_vec;
  cos_vec.x = max(cos[0].x, max(cos[0].y, cos[0].z));
  cos_vec.y = max(cos[1].x, max(cos[1].y, cos[1].z));
  cos_vec.z = max(cos[2].x, max(cos[2].y, cos[2].z));
  cos_vec.w = max(cos[3].x, max(cos[3].y, cos[3].z));
  return depths / cos_vec;
}

float get_world_distance(float depth, vec3 cos)
{
  depth = linear_depth(depth);
  cos = normalize(abs(cos));
  float cos_vec = max(cos.x, max(cos.y, cos.z));
  return depth / cos_vec;
}
#endif

/* Marco Salvi's GDC 2008 presentation about shadow maps pre-filtering techniques slide 24 */
#define ln_space_prefilter_step(ref, sample) exp(sample - ref)
#define ln_space_prefilter_finalize(ref, sum) (ref + log(SAMPLE_WEIGHT * sum))

#define SAMPLE_WEIGHT 0.11111

#ifdef ESM
void prefilter(vec4 depths, float ref, inout float accum)
{
  accum += dot(ln_space_prefilter_step(ref, depths), vec4(1.0));
}
#else /* VSM */
void prefilter(vec4 depths, float ref, inout vec2 accum)
{
  vec4 depths_sqr = depths * depths;
  accum += vec2(dot(vec4(1.0), depths), dot(vec4(1.0), depths_sqr)) * SAMPLE_WEIGHT;
}
#endif

#ifdef CSM
vec3 get_texco(vec2 uvs, vec2 ofs)
{
  return vec3(uvs + ofs, float(cascadeID));
}
#else /* CUBEMAP */
const vec3 minorAxisX[6] = vec3[6](vec3(0.0f, 0.0f, -1.0f),
                                   vec3(0.0f, 0.0f, 1.0f),
                                   vec3(1.0f, 0.0f, 0.0f),
                                   vec3(1.0f, 0.0f, 0.0f),
                                   vec3(1.0f, 0.0f, 0.0f),
                                   vec3(-1.0f, 0.0f, 0.0f));

const vec3 minorAxisY[6] = vec3[6](vec3(0.0f, -1.0f, 0.0f),
                                   vec3(0.0f, -1.0f, 0.0f),
                                   vec3(0.0f, 0.0f, 1.0f),
                                   vec3(0.0f, 0.0f, -1.0f),
                                   vec3(0.0f, -1.0f, 0.0f),
                                   vec3(0.0f, -1.0f, 0.0f));

const vec3 majorAxis[6] = vec3[6](vec3(1.0f, 0.0f, 0.0f),
                                  vec3(-1.0f, 0.0f, 0.0f),
                                  vec3(0.0f, 1.0f, 0.0f),
                                  vec3(0.0f, -1.0f, 0.0f),
                                  vec3(0.0f, 0.0f, 1.0f),
                                  vec3(0.0f, 0.0f, -1.0f));

vec3 get_texco(vec2 uvs, vec2 ofs)
{
  uvs += ofs;
  return majorAxis[layerID] + uvs.x * minorAxisX[layerID] + uvs.y * minorAxisY[layerID];
}
#endif

void main()
{
  /* Copy the depth only shadowmap into another texture while converting
   * to linear depth and do a 3x3 box blur. */

#ifdef CSM
  vec2 uvs = gl_FragCoord.xy * storedTexelSize;
#else /* CUBEMAP */
  vec2 uvs = gl_FragCoord.xy * cubeTexelSize * 2.0 - 1.0;
#endif

  /* Center texel */
  vec3 co = get_texco(uvs, vec2(0.0));
  float depth = texture(shadowTexture, co).r;
  depth = get_world_distance(depth, co);

  if (filterSize[cascadeID] == 0.0) {
#ifdef ESM
    FragColor = vec4(depth);
#else /* VSM */
    FragColor = vec2(depth, depth * depth).xyxy;
#endif
    return;
  }

#ifdef ESM
  float ref = depth;
  float accum = 1.0;
#else /* VSM */
  float ref = 0.0; /* UNUSED */
  vec2 accum = vec2(depth, depth * depth) * SAMPLE_WEIGHT;
#endif

  vec3 ofs = vec3(1.0, 0.0, -1.0) * filterSize[cascadeID];

  vec3 cos[4];
  cos[0] = get_texco(uvs, ofs.zz);
  cos[1] = get_texco(uvs, ofs.yz);
  cos[2] = get_texco(uvs, ofs.xz);
  cos[3] = get_texco(uvs, ofs.zy);

  vec4 depths;
  depths.x = texture(shadowTexture, cos[0]).r;
  depths.y = texture(shadowTexture, cos[1]).r;
  depths.z = texture(shadowTexture, cos[2]).r;
  depths.w = texture(shadowTexture, cos[3]).r;
  depths = get_world_distance(depths, cos);
  prefilter(depths, ref, accum);

  cos[0] = get_texco(uvs, ofs.xy);
  cos[1] = get_texco(uvs, ofs.zx);
  cos[2] = get_texco(uvs, ofs.yx);
  cos[3] = get_texco(uvs, ofs.xx);
  depths.x = texture(shadowTexture, cos[0]).r;
  depths.y = texture(shadowTexture, cos[1]).r;
  depths.z = texture(shadowTexture, cos[2]).r;
  depths.w = texture(shadowTexture, cos[3]).r;
  depths = get_world_distance(depths, cos);
  prefilter(depths, ref, accum);

#ifdef ESM
  accum = ln_space_prefilter_finalize(ref, accum);
#endif

  FragColor = vec2(accum).xyxy;
}
