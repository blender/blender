uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ProjectionMatrix;

uniform float pixsize;   /* rv3d->pixsize */
uniform float pixelsize; /* U.pixelsize */
uniform int keep_size;
uniform float objscale;
uniform int pixfactor;

in vec3 pos;
in vec4 color;
in float thickness;

out vec4 finalColor;
out float finalThickness;

#define TRUE 1

float defaultpixsize = pixsize * pixelsize * (1000.0 / pixfactor);

void main(void)
{
	gl_Position = ModelViewProjectionMatrix * vec4( pos, 1.0 );
	finalColor = color;

	if (keep_size == TRUE) {
		finalThickness = thickness;
	}
	else {
		float size = (ProjectionMatrix[3][3] == 0.0) ? (thickness / (gl_Position.z * defaultpixsize)) : (thickness / defaultpixsize);
		finalThickness = max(size * objscale, 1.0);
	}
}
