
#pragma BLENDER_REQUIRE(volumetric_lib.glsl)

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Step 4 : Apply final integration on top of the scene color. */

uniform sampler2D inSceneDepth;

layout(local_size_x = 1, local_size_y = 1) in;

#ifdef TARGET_IMG_FLOAT
layout(binding = 0, rgba32f) uniform image2D target_img;
#else
layout(binding = 0, rgba16f) uniform image2D target_img;
#endif

void main()
{
  ivec2 co = ivec2(gl_GlobalInvocationID.xy);
  vec2 uvs = co / vec2(textureSize(inSceneDepth, 0));
  float scene_depth = texture(inSceneDepth, uvs).r;

  vec3 transmittance, scattering;
  volumetric_resolve(uvs, scene_depth, transmittance, scattering);

  /* Approximate volume alpha by using a monochromatic transmittance
   * and adding it to the scene alpha. */
  float alpha = dot(transmittance, vec3(1.0 / 3.0));

  vec4 color0 = vec4(scattering, 1.0 - alpha);
  vec4 color1 = vec4(transmittance, alpha);

  vec4 color_in = imageLoad(target_img, co);
  vec4 color_out = color0 + color1 * color_in;
  imageStore(target_img, co, color_out);
}
