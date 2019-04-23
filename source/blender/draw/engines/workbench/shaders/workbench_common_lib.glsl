#define NO_OBJECT_ID uint(0)
#define EPSILON 0.00001
#define M_PI 3.14159265358979323846

#define CAVITY_BUFFER_RANGE 4.0

/* 4x4 bayer matrix prepared for 8bit UNORM precision error. */
#define P(x) (((x + 0.5) * (1.0 / 16.0) - 0.5) * (1.0 / 255.0))
const vec4 dither_mat4x4[4] = vec4[4](vec4(P(0.0), P(8.0), P(2.0), P(10.0)),
                                      vec4(P(12.0), P(4.0), P(14.0), P(6.0)),
                                      vec4(P(3.0), P(11.0), P(1.0), P(9.0)),
                                      vec4(P(15.0), P(7.0), P(13.0), P(5.0)));

float bayer_dither_noise()
{
  ivec2 tx1 = ivec2(gl_FragCoord.xy) % 4;
  ivec2 tx2 = ivec2(gl_FragCoord.xy) % 2;
  return dither_mat4x4[tx1.x][tx1.y];
}

#ifdef WORKBENCH_ENCODE_NORMALS

#  define WB_Normal vec2

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
vec3 workbench_normal_decode(WB_Normal enc)
{
  vec2 fenc = enc.xy * 4.0 - 2.0;
  float f = dot(fenc, fenc);
  float g = sqrt(1.0 - f / 4.0);
  vec3 n;
  n.xy = fenc * g;
  n.z = 1 - f / 2;
  return n;
}

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
WB_Normal workbench_normal_encode(vec3 n)
{
  float p = sqrt(n.z * 8.0 + 8.0);
  n.xy = clamp(n.xy / p + 0.5, 0.0, 1.0);
  return n.xy;
}

#else
#  define WB_Normal vec3
/* Well just do nothing... */
#  define workbench_normal_encode(a) (a)
#  define workbench_normal_decode(a) (a)
#endif /* WORKBENCH_ENCODE_NORMALS */

/* Encoding into the alpha of a RGBA8 UNORM texture. */
#define TARGET_BITCOUNT 8u
#define METALLIC_BITS 3u /* Metallic channel is less important. */
#define ROUGHNESS_BITS (TARGET_BITCOUNT - METALLIC_BITS)
#define TOTAL_BITS (METALLIC_BITS + ROUGHNESS_BITS)

/* Encode 2 float into 1 with the desired precision. */
float workbench_float_pair_encode(float v1, float v2)
{
  // const uint total_mask = ~(0xFFFFFFFFu << TOTAL_BITS);
  // const uint v1_mask = ~(0xFFFFFFFFu << ROUGHNESS_BITS);
  // const uint v2_mask = ~(0xFFFFFFFFu << METALLIC_BITS);
  /* Same as above because some compiler are dumb af. and think we use mediump int.  */
  const int total_mask = 0xFF;
  const int v1_mask = 0x1F;
  const int v2_mask = 0x7;
  int iv1 = int(v1 * float(v1_mask));
  int iv2 = int(v2 * float(v2_mask)) << int(ROUGHNESS_BITS);
  return float(iv1 | iv2) * (1.0 / float(total_mask));
}

void workbench_float_pair_decode(float data, out float v1, out float v2)
{
  // const uint total_mask = ~(0xFFFFFFFFu << TOTAL_BITS);
  // const uint v1_mask = ~(0xFFFFFFFFu << ROUGHNESS_BITS);
  // const uint v2_mask = ~(0xFFFFFFFFu << METALLIC_BITS);
  /* Same as above because some compiler are dumb af. and think we use mediump int.  */
  const int total_mask = 0xFF;
  const int v1_mask = 0x1F;
  const int v2_mask = 0x7;
  int idata = int(data * float(total_mask));
  v1 = float(idata & v1_mask) * (1.0 / float(v1_mask));
  v2 = float(idata >> int(ROUGHNESS_BITS)) * (1.0 / float(v2_mask));
}

float calculate_transparent_weight(float z, float alpha)
{
#if 0
  /* Eq 10 : Good for surfaces with varying opacity (like particles) */
  float a = min(1.0, alpha * 10.0) + 0.01;
  float b = -gl_FragCoord.z * 0.95 + 1.0;
  float w = a * a * a * 3e2 * b * b * b;
#else
  /* Eq 7 put more emphasis on surfaces closer to the view. */
  // float w = 10.0 / (1e-5 + pow(abs(z) / 5.0, 2.0) + pow(abs(z) / 200.0, 6.0)); /* Eq 7 */
  // float w = 10.0 / (1e-5 + pow(abs(z) / 10.0, 3.0) + pow(abs(z) / 200.0, 6.0)); /* Eq 8 */
  // float w = 10.0 / (1e-5 + pow(abs(z) / 200.0, 4.0)); /* Eq 9 */
  /* Same as eq 7, but optimized. */
  float a = abs(z) / 5.0;
  float b = abs(z) / 200.0;
  b *= b;
  float w = 10.0 / ((1e-5 + a * a) + b * (b * b)); /* Eq 7 */
#endif
  return alpha * clamp(w, 1e-2, 3e2);
}

/* Special function only to be used with calculate_transparent_weight(). */
float linear_zdepth(float depth, vec4 viewvecs[3], mat4 proj_mat)
{
  if (proj_mat[3][3] == 0.0) {
    float d = 2.0 * depth - 1.0;
    return -proj_mat[3][2] / (d + proj_mat[2][2]);
  }
  else {
    /* Return depth from near plane. */
    return depth * viewvecs[1].z;
  }
}

vec3 view_vector_from_screen_uv(vec2 uv, vec4 viewvecs[3], mat4 proj_mat)
{
  return (proj_mat[3][3] == 0.0) ? normalize(viewvecs[0].xyz + vec3(uv, 0.0) * viewvecs[1].xyz) :
                                   vec3(0.0, 0.0, 1.0);
}

vec2 matcap_uv_compute(vec3 I, vec3 N, bool flipped)
{
  /* Quick creation of an orthonormal basis */
  float a = 1.0 / (1.0 + I.z);
  float b = -I.x * I.y * a;
  vec3 b1 = vec3(1.0 - I.x * I.x * a, b, -I.x);
  vec3 b2 = vec3(b, 1.0 - I.y * I.y * a, -I.y);
  vec2 matcap_uv = vec2(dot(b1, N), dot(b2, N));
  if (flipped) {
    matcap_uv.x = -matcap_uv.x;
  }
  return matcap_uv * 0.496 + 0.5;
}

vec4 workbench_sample_texture(sampler2D image, vec2 coord, bool nearest_sampling)
{
  vec2 tex_size = vec2(textureSize(image, 0).xy);
  /* TODO(fclem) We could do the same with sampler objects.
   * But this is a quick workaround instead of messing with the GPUTexture itself. */
  vec2 uv = nearest_sampling ? (floor(coord * tex_size) + 0.5) / tex_size : coord;
  vec4 color = texture(image, uv);

  /* Unpremultiply, ideally shaders would be added so this is not needed. */
  if (!(color.a == 0.0 || color.a == 1.0)) {
    color.rgb = color.rgb / color.a;
  }

  return color;
}
