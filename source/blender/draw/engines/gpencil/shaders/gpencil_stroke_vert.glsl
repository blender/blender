uniform mat4 ModelMatrix;

uniform float pixsize; /* rv3d->pixsize */
uniform int keep_size;
uniform float objscale;
uniform float pixfactor;
uniform int viewport_xray;
uniform int shading_type[2];
uniform vec4 wire_color;

in vec3 pos;
in vec4 color;
in float thickness;
in vec2 uvdata;

out vec4 finalColor;
out float finalThickness;
out vec2 finaluvdata;

#define TRUE 1

#define OB_WIRE 2
#define OB_SOLID 3

#define V3D_SHADING_MATERIAL_COLOR 0
#define V3D_SHADING_TEXTURE_COLOR 3

float defaultpixsize = pixsize * (1000.0 / pixfactor);

void main(void)
{
  gl_Position = point_object_to_ndc(pos);
  finalColor = color;

  if (keep_size == TRUE) {
    finalThickness = thickness;
  }
  else {
    float size = (ProjectionMatrix[3][3] == 0.0) ? (thickness / (gl_Position.z * defaultpixsize)) :
                                                   (thickness / defaultpixsize);
    finalThickness = max(size * objscale, 1.0);
  }

  /* for wireframe override size and color */
  if (shading_type[0] == OB_WIRE) {
    finalThickness = 1.0;
    finalColor = wire_color;
  }
  /* for solid override color */
  if (shading_type[0] == OB_SOLID) {
    if ((shading_type[1] != V3D_SHADING_MATERIAL_COLOR) &&
        (shading_type[1] != V3D_SHADING_TEXTURE_COLOR)) {
      finalColor = wire_color;
    }
    if (viewport_xray == 1) {
      finalColor.a *= 0.5;
    }
  }

  finaluvdata = uvdata;
}
