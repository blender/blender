
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

uniform sampler2DArray imageTileArray;
uniform sampler1DArray imageTileData;
uniform sampler2D imageTexture;

uniform float imageTransparencyCutoff = 0.1;
uniform bool imagePremult;

vec3 workbench_image_color(vec2 uvs)
{
#ifdef V3D_SHADING_TEXTURE_COLOR
  vec4 color;

#  ifdef TEXTURE_IMAGE_ARRAY
  vec3 co = vec3(uvs, 0.0);
  if (node_tex_tile_lookup(co, imageTileArray, imageTileData)) {
    color = texture(imageTileArray, co);
  }
  else {
    color = vec4(1.0, 0.0, 1.0, 1.0);
  }
#  else

  color = texture(imageTexture, uvs);
#  endif

  /* Unpremultiply if stored multiplied, since straight alpha is expected by shaders. */
  if (imagePremult && !(color.a == 0.0 || color.a == 1.0)) {
    color.rgb /= color.a;
  }

#  ifdef GPU_FRAGMENT_SHADER
  if (color.a < imageTransparencyCutoff) {
    discard;
  }
#  endif

  return color.rgb;
#else

  return vec3(1.0);
#endif
}
