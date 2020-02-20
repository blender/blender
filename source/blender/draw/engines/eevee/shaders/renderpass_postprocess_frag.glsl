#define PASS_POST_UNDEFINED 0
#define PASS_POST_ACCUMULATED_COLOR 1
#define PASS_POST_ACCUMULATED_LIGHT 2
#define PASS_POST_ACCUMULATED_VALUE 3
#define PASS_POST_DEPTH 4
#define PASS_POST_AO 5
#define PASS_POST_NORMAL 6
#define PASS_POST_TWO_LIGHT_BUFFERS 7

uniform int postProcessType;
uniform int currentSample;
uniform sampler2D inputBuffer;
uniform sampler2D inputSecondLightBuffer;
uniform sampler2D inputColorBuffer;

out vec4 fragColor;

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  if (postProcessType == PASS_POST_DEPTH) {
    float depth = texelFetch(depthBuffer, texel, 0).r;
    if (depth == 1.0f) {
      depth = 1e10;
    }
    else {
      depth = -get_view_z_from_depth(depth);
    }
    fragColor.r = depth;
  }
  else if (postProcessType == PASS_POST_AO) {
    float ao_accum = texelFetch(inputBuffer, texel, 0).r;
    fragColor = vec4(vec3(min(1.0, ao_accum / currentSample)), 1.0);
  }
  else if (postProcessType == PASS_POST_NORMAL) {
    float depth = texelFetch(depthBuffer, texel, 0).r;
    vec2 encoded_normal = texelFetch(inputBuffer, texel, 0).rg;
    /* decode the normals only when they are valid. otherwise the result buffer will be filled
     * with NaN's */
    if (depth != 1.0 && any(notEqual(encoded_normal, vec2(0.0)))) {
      vec3 decoded_normal = normal_decode(texelFetch(inputBuffer, texel, 0).rg, vec3(0.0));
      vec3 world_normal = mat3(ViewMatrixInverse) * decoded_normal;
      fragColor = vec4(world_normal, 1.0);
    }
    else {
      fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
  }
  else if (postProcessType == PASS_POST_ACCUMULATED_VALUE) {
    float accumulated_value = texelFetch(inputBuffer, texel, 0).r;
    fragColor = vec4(vec3(accumulated_value / currentSample), 1.0);
  }
  else if (postProcessType == PASS_POST_ACCUMULATED_COLOR) {
    vec3 accumulated_color = texelFetch(inputBuffer, texel, 0).rgb;
    fragColor = vec4(accumulated_color / currentSample, 1.0);
  }
  else if (postProcessType == PASS_POST_ACCUMULATED_LIGHT) {
    vec3 accumulated_light = texelFetch(inputBuffer, texel, 0).rgb;
    vec3 accumulated_color = texelFetch(inputColorBuffer, texel, 0).rgb;

    /* Fix INF in the case a color component is 0.0 */
    if (accumulated_color.r == 0.0) {
      accumulated_color.r = 1.0;
      accumulated_light.r = 0.0;
    }
    if (accumulated_color.g == 0.0) {
      accumulated_color.g = 1.0;
      accumulated_light.g = 0.0;
    }
    if (accumulated_color.b == 0.0) {
      accumulated_color.b = 1.0;
      accumulated_light.b = 0.0;
    }
    fragColor = vec4(accumulated_light / accumulated_color, 1.0);
  }
  else if (postProcessType == PASS_POST_TWO_LIGHT_BUFFERS) {
    vec3 accumulated_light = texelFetch(inputBuffer, texel, 0).rgb +
                             texelFetch(inputSecondLightBuffer, texel, 0).rgb;
    vec3 accumulated_color = texelFetch(inputColorBuffer, texel, 0).rgb;

    /* Fix INF in the case a color component is 0.0 */
    if (accumulated_color.r == 0.0) {
      accumulated_color.r = 1.0;
      accumulated_light.r = 0.0;
    }
    if (accumulated_color.g == 0.0) {
      accumulated_color.g = 1.0;
      accumulated_light.g = 0.0;
    }
    if (accumulated_color.b == 0.0) {
      accumulated_color.b = 1.0;
      accumulated_light.b = 0.0;
    }
    fragColor = vec4(accumulated_light / accumulated_color, 1.0);
  }
  else {
    /* Output error color: Unknown how to post process this pass. */
    fragColor = vec4(1.0, 0.0, 1.0, 1.0);
  }
}
