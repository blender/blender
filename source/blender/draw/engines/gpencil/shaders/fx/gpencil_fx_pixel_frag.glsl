uniform mat4 ProjectionMatrix;
uniform mat4 ViewMatrix;

uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;

uniform int size[3];
uniform vec4 color;

uniform vec3 loc;
uniform float pixsize;   /* rv3d->pixsize */
uniform float pixelsize; /* U.pixelsize */
uniform float pixfactor;

out vec4 FragColor;

int uselines = size[2];
float defaultpixsize = pixsize * pixelsize * (1000.0 / pixfactor);
vec2 nsize = max(vec2(size[0], size[1]), 3.0);

/* This pixelation shader is a modified version of original Geeks3d.com code */
void main()
{
	vec2 uv = vec2(gl_FragCoord.xy);
	vec4 nloc = ProjectionMatrix * ViewMatrix * vec4(loc.xyz, 1.0);
	
	float dx = (ProjectionMatrix[3][3] == 0.0) ? (nsize[0] / (nloc.z * defaultpixsize)) : (nsize[0] / defaultpixsize);
	float dy = (ProjectionMatrix[3][3] == 0.0) ? (nsize[1] / (nloc.z * defaultpixsize)) : (nsize[1] / defaultpixsize);

	dx = max(abs(dx), 3.0);
	dy = max(abs(dy), 3.0);
	
	vec2 coord = vec2(dx * floor(uv.x / dx), dy * floor(uv.y / dy));
	
	float stroke_depth = texelFetch(strokeDepth, ivec2(coord), 0).r;
	vec4 outcolor = texelFetch(strokeColor, ivec2(coord), 0);

	if (uselines == 1) {
		float difx = uv.x - (floor(uv.x / nsize[0]) * nsize[0]);
		if ((difx == 0.5) && (outcolor.a > 0)) {
			outcolor = color;
		}
		float dify = uv.y - (floor(uv.y / nsize[1]) * nsize[1]);
		if ((dify == 0.5) && (outcolor.a > 0)) {
			outcolor = color;
		}
	}
	gl_FragDepth = stroke_depth;
	FragColor = outcolor;
}
