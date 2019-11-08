#define SCE_PASS_Z (1 << 1)
#define SCE_PASS_AO (1 << 6)
#define SCE_PASS_NORMAL (1 << 8)
#define SCE_PASS_MIST (1 << 14)
#define SCE_PASS_SUBSURFACE_DIRECT (1 << 28)
#define SCE_PASS_SUBSURFACE_COLOR (1 << 30)

#define ACCUMULATED_COLOR_PASSES (SCE_PASS_SUBSURFACE_DIRECT | SCE_PASS_SUBSURFACE_COLOR)
#define ACCUMULATED_VALUE_PASSES (SCE_PASS_MIST)
uniform int renderpassType;
uniform int currentSample;
uniform sampler2D inputBuffer;

out vec4 fragColor;

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  if (renderpassType == SCE_PASS_Z) {
    float depth = texelFetch(depthBuffer, texel, 0).r;
    if (depth == 1.0f) {
      depth = 1e10;
    }
    else {
      depth = -get_view_z_from_depth(depth);
    }
    fragColor.r = depth;
  }

  else if (renderpassType == SCE_PASS_AO) {
    float ao_accum = texelFetch(inputBuffer, texel, 0).r;
    fragColor = vec4(vec3(min(1.0, ao_accum / currentSample)), 1.0);
  }

  else if (renderpassType == SCE_PASS_NORMAL) {
    float depth = texelFetch(depthBuffer, texel, 0).r;
    vec2 encoded_normal = texelFetch(inputBuffer, texel, 0).rg;
    /* decode the normals only when they are valid. otherwise the result buffer will be filled with
     * NaN's */
    if (depth != 1.0 && any(notEqual(encoded_normal, vec2(0.0)))) {
      vec3 decoded_normal = normal_decode(texelFetch(inputBuffer, texel, 0).rg, vec3(0.0));
      vec3 world_normal = mat3(ViewMatrixInverse) * decoded_normal;
      fragColor = vec4(world_normal, 1.0);
    }
    else {
      fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
  }

  else if ((renderpassType & ACCUMULATED_VALUE_PASSES) != 0) {
    float accumulated_value = texelFetch(inputBuffer, texel, 0).r;
    fragColor = vec4(vec3(accumulated_value / currentSample), 1.0);
  }

  else if ((renderpassType & ACCUMULATED_COLOR_PASSES) != 0) {
    vec3 accumulated_color = texelFetch(inputBuffer, texel, 0).rgb;
    fragColor = vec4(accumulated_color / currentSample, 1.0);
  }

  else {
    fragColor = vec4(1.0, 0.0, 1.0, 1.0);
  }
}