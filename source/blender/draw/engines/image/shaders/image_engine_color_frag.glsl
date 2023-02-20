#pragma BLENDER_REQUIRE(common_colormanagement_lib.glsl)

/* Keep in sync with image_engine.c */
#define IMAGE_DRAW_FLAG_SHOW_ALPHA (1 << 0)
#define IMAGE_DRAW_FLAG_APPLY_ALPHA (1 << 1)
#define IMAGE_DRAW_FLAG_SHUFFLING (1 << 2)
#define IMAGE_DRAW_FLAG_DEPTH (1 << 3)

#define FAR_DISTANCE farNearDistances.x
#define NEAR_DISTANCE farNearDistances.y

void main()
{
  ivec2 uvs_clamped = ivec2(uv_screen);
  float depth = texelFetch(depth_texture, uvs_clamped, 0).r;
  if (depth == 1.0) {
    discard;
    return;
  }

  vec4 tex_color = texelFetch(imageTexture, uvs_clamped - offset, 0);

  if ((drawFlags & IMAGE_DRAW_FLAG_APPLY_ALPHA) != 0) {
    if (!imgPremultiplied) {
      tex_color.rgb *= tex_color.a;
    }
  }
  if ((drawFlags & IMAGE_DRAW_FLAG_DEPTH) != 0) {
    tex_color = smoothstep(FAR_DISTANCE, NEAR_DISTANCE, tex_color);
  }

  if ((drawFlags & IMAGE_DRAW_FLAG_SHUFFLING) != 0) {
    tex_color = vec4(dot(tex_color, shuffle));
  }
  if ((drawFlags & IMAGE_DRAW_FLAG_SHOW_ALPHA) == 0) {
    tex_color.a = 1.0;
  }
  fragColor = tex_color;
}
