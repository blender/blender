#define S3D_DISPLAY_ANAGLYPH 0
#define S3D_DISPLAY_INTERLACE 1

#define S3D_INTERLACE_ROW 0
#define S3D_INTERLACE_COLUMN 1
#define S3D_INTERLACE_CHECKERBOARD 2

/* Composite stereo textures */

uniform sampler2D imageTexture;
uniform sampler2D overlayTexture;

uniform int stereoDisplaySettings;

#define stereo_display_mode (stereoDisplaySettings & ((1 << 3) - 1))
#define stereo_interlace_mode ((stereoDisplaySettings >> 3) & ((1 << 3) - 1))
#define stereo_interlace_swap bool(stereoDisplaySettings >> 6)

layout(location = 0) out vec4 imageColor;
layout(location = 1) out vec4 overlayColor;

bool interlace(ivec2 texel)
{
  int interlace_mode = stereo_interlace_mode;
  if (interlace_mode == S3D_INTERLACE_CHECKERBOARD) {
    return ((texel.x + texel.y) & 1) != 0;
  }
  else if (interlace_mode == S3D_INTERLACE_ROW) {
    return (texel.y & 1) != 0;
  }
  else if (interlace_mode == S3D_INTERLACE_COLUMN) {
    return (texel.x & 1) != 0;
  }
}

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  if (stereo_display_mode == S3D_DISPLAY_INTERLACE &&
      (interlace(texel) == stereo_interlace_swap)) {
    discard;
  }

  imageColor = texelFetch(imageTexture, texel, 0);
  overlayColor = texelFetch(overlayTexture, texel, 0);
}
