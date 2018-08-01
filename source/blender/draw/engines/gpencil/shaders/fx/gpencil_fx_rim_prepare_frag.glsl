uniform mat4 ProjectionMatrix;
uniform mat4 ViewMatrix;

/* ******************************************************************* */
/* create rim and mask                                                 */
/* ******************************************************************* */
uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;
uniform vec2 Viewport;

uniform int offset[2];
uniform vec3 rim_color;
uniform vec3 mask_color;

uniform vec3 loc;
uniform float pixsize;   /* rv3d->pixsize */
uniform float pixelsize; /* U.pixelsize */
uniform float pixfactor;

float defaultpixsize = pixsize * pixelsize * (1000.0 / pixfactor);
vec2 noffset = vec2(offset[0], offset[1]);

out vec4 FragColor;

void main()
{
	vec2 uv = vec2(gl_FragCoord.xy);
	vec4 nloc = ProjectionMatrix * ViewMatrix * vec4(loc.xyz, 1.0);

	float dx = (ProjectionMatrix[3][3] == 0.0) ? (noffset[0] / (nloc.z * defaultpixsize)) : (noffset[0] / defaultpixsize);
	float dy = (ProjectionMatrix[3][3] == 0.0) ? (noffset[1] / (nloc.z * defaultpixsize)) : (noffset[1] / defaultpixsize);

	float stroke_depth = texelFetch(strokeDepth, ivec2(uv.xy), 0).r;
	vec4 src_pixel= texelFetch(strokeColor, ivec2(uv.xy), 0);
	vec4 offset_pixel= texelFetch(strokeColor, ivec2(uv.x - dx, uv.y - dy), 0);
	vec4 outcolor;

	/* is transparent */
	if (src_pixel.a == 0.0f) {
		discard;
	}
	/* check inside viewport */
	else if ((uv.x - dx < 0) || (uv.x - dx > Viewport[0])) {
		discard;
	}
	else if ((uv.y - dy < 0) || (uv.y - dy > Viewport[1])) {
		discard;
	}
	/* pixel is equal to mask color, keep */
	else if (src_pixel.rgb == mask_color.rgb) {
		discard;
	}
	else {
		if ((src_pixel.a > 0) && (offset_pixel.a > 0)) {
			discard;
		}
		else {
			outcolor = vec4(rim_color, 1.0);
		}
	}

	gl_FragDepth = stroke_depth;
	FragColor = outcolor;
}
