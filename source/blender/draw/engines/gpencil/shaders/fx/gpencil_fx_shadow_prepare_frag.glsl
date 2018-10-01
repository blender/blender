uniform mat4 ProjectionMatrix;
uniform mat4 ViewMatrix;

/* ******************************************************************* */
/* create shadow                                                       */
/* ******************************************************************* */
uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;
uniform vec2 Viewport;

uniform int offset[2];
uniform float scale[2];
uniform float rotation;
uniform vec4 shadow_color;

uniform float amplitude;
uniform float period;
uniform float phase;
uniform int orientation;

uniform vec3 loc;
uniform float pixsize;   /* rv3d->pixsize */
uniform float pixfactor;

#define M_PI 3.1415926535897932384626433832795

#define HORIZONTAL 0
#define VERTICAL 1

float defaultpixsize = pixsize * (1000.0 / pixfactor);
vec2 noffset = vec2(offset[0], offset[1]);

out vec4 FragColor;

/* project 3d point to 2d on screen space */
vec2 toScreenSpace(vec4 vertex)
{
	/* need to calculate ndc because this is not done by vertex shader */
	vec3 ndc = vec3(vertex).xyz / vertex.w;

	vec2 sc;
	sc.x = ((ndc.x + 1.0) / 2.0) * Viewport.x;
	sc.y = ((ndc.y + 1.0) / 2.0) * Viewport.y;

	return sc;
}

void main()
{
	vec2 uv = vec2(gl_FragCoord.xy);
	vec4 nloc = ProjectionMatrix * ViewMatrix * vec4(loc.xyz, 1.0);
	vec2 loc2d = toScreenSpace(nloc);

	float dx = (ProjectionMatrix[3][3] == 0.0) ? (noffset[0] / (nloc.z * defaultpixsize)) : (noffset[0] / defaultpixsize);
	float dy = (ProjectionMatrix[3][3] == 0.0) ? (noffset[1] / (nloc.z * defaultpixsize)) : (noffset[1] / defaultpixsize);

	float cosv = cos(rotation);
	float sinv = sin(rotation);

	/* move point to new coords system */
	vec2 tpos = vec2(uv.x, uv.y) - loc2d;

	/* rotation */
	if (rotation != 0) {
		vec2 rotpoint = vec2((tpos.x * cosv) - (tpos.y * sinv), (tpos.x * sinv) + (tpos.y * cosv));
		tpos = rotpoint;
	}
	
	/* apply offset */
	tpos = vec2(tpos.x - dx, tpos.y - dy);
	
	/* apply scale */
	tpos.x *= 1.0 / scale[0];
	tpos.y *= 1.0 / scale[1];
	
	/* back to original coords system */
	vec2 texpos = tpos + loc2d;
	
	/* wave */ 
	float value;
	if (orientation == HORIZONTAL) {
		float pval = (uv.x * M_PI) / Viewport[0];
		value = amplitude * sin((period * pval) + phase);
		texpos.y += value;
	}
	else if (orientation == VERTICAL){
		float pval = (uv.y * M_PI) / Viewport[1];
		value = amplitude * sin((period * pval) + phase);
		texpos.x += value;
	}
	
	float stroke_depth = texelFetch(strokeDepth, ivec2(texpos.x, texpos.y), 0).r;
	vec4 src_pixel= texelFetch(strokeColor, ivec2(texpos.x, texpos.y), 0);
	vec4 outcolor;
	outcolor = shadow_color;

	/* is transparent */
	if (src_pixel.a == 0.0f) {
		discard;
	}

	gl_FragDepth = stroke_depth;
	FragColor = outcolor;
}
