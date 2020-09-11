#pragma BLENDER_REQUIRE(common_colormanagement_lib.glsl)

/* Keep in sync with image_engine.c */
#define SIMA_DRAW_FLAG_SHOW_ALPHA (1 << 0)
#define SIMA_DRAW_FLAG_APPLY_ALPHA (1 << 1)
#define SIMA_DRAW_FLAG_SHUFFLING (1 << 2)
#define SIMA_DRAW_FLAG_DEPTH (1 << 3)
#define SIMA_DRAW_FLAG_TILED (1 << 4)
#define SIMA_DRAW_FLAG_DO_REPEAT (1 << 5)

uniform sampler2DArray imageTileArray;
uniform sampler1DArray imageTileData;
uniform sampler2D imageTexture;

uniform bool imgPremultiplied;
uniform int drawFlags;
uniform vec2 farNearDistances;
uniform vec4 color;
uniform vec4 shuffle;

#define FAR_DISTANCE farNearDistances.x
#define NEAR_DISTANCE farNearDistances.y

in vec2 uvs;

out vec4 fragColor;

/* TODO(fclem) deduplicate code.  */
bool node_tex_tile_lookup(inout vec3 co, sampler2DArray ima, sampler1DArray map)
{
  vec2 tile_pos = floor(co.xy);

  if (tile_pos.x < 0 || tile_pos.y < 0 || tile_pos.x >= 10) {
    return false;
  }

  float tile = 10.0 * tile_pos.y + tile_pos.x;
  if (tile >= textureSize(map, 0).x) {
    return false;
  }

  /* Fetch tile information. */
  float tile_layer = texelFetch(map, ivec2(tile, 0), 0).x;
  if (tile_layer < 0.0) {
    return false;
  }

  vec4 tile_info = texelFetch(map, ivec2(tile, 1), 0);

  co = vec3(((co.xy - tile_pos) * tile_info.zw) + tile_info.xy, tile_layer);
  return true;
}

void main()
{
  vec4 tex_color;
  /* Read texture */
  if ((drawFlags & SIMA_DRAW_FLAG_TILED) != 0) {
    vec3 co = vec3(uvs, 0.0);
    if (node_tex_tile_lookup(co, imageTileArray, imageTileData)) {
      tex_color = texture(imageTileArray, co);
    }
    else {
      tex_color = vec4(1.0, 0.0, 1.0, 1.0);
    }
  }
  else {
    vec2 uvs_clamped = ((drawFlags & SIMA_DRAW_FLAG_DO_REPEAT) != 0) ?
                           fract(uvs) :
                           clamp(uvs, vec2(0.0), vec2(1.0));
    tex_color = texture(imageTexture, uvs_clamped);
  }

  if ((drawFlags & SIMA_DRAW_FLAG_APPLY_ALPHA) != 0) {
    if (!imgPremultiplied && tex_color.a != 0.0 && tex_color.a != 1.0) {
      tex_color.rgb *= tex_color.a;
    }
  }
  if ((drawFlags & SIMA_DRAW_FLAG_DEPTH) != 0) {
    tex_color = smoothstep(FAR_DISTANCE, NEAR_DISTANCE, tex_color);
  }

  if ((drawFlags & SIMA_DRAW_FLAG_SHUFFLING) != 0) {
    tex_color = color * dot(tex_color, shuffle);
  }
  if ((drawFlags & SIMA_DRAW_FLAG_SHOW_ALPHA) == 0) {
    tex_color.a = 1.0;
  }

  fragColor = tex_color;
}
