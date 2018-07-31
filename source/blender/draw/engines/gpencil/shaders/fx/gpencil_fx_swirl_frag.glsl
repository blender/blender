uniform mat4 ProjectionMatrix;
uniform mat4 ViewMatrix;

uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;

uniform vec2 Viewport;
uniform vec3 loc;
uniform int radius;
uniform float angle;
uniform int transparent;

uniform float pixsize;   /* rv3d->pixsize */
uniform float pixelsize; /* U.pixelsize */
uniform float pixfactor;

out vec4 FragColor;

float defaultpixsize = pixsize * pixelsize * (1000.0 / pixfactor);

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

/* This swirl shader is a modified version of original Geeks3d.com code */
void main()
{
	vec2 uv = vec2(gl_FragCoord.xy);
	float stroke_depth;
	vec4 outcolor;
	
	vec4 center3d = ProjectionMatrix * ViewMatrix * vec4(loc.xyz, 1.0); 
	vec2 center = toScreenSpace(center3d);
	vec2 tc = uv - center;

	float dist = length(tc);
	float pxradius = (ProjectionMatrix[3][3] == 0.0) ? (radius / (loc.z * defaultpixsize)) : (radius / defaultpixsize);
	pxradius = max(pxradius, 1);
	
	if (dist <= pxradius) {
		float percent = (pxradius - dist) / pxradius;
		float theta = percent * percent * angle * 8.0;
		float s = sin(theta);
		float c = cos(theta);
		tc = vec2(dot(tc, vec2(c, -s)), dot(tc, vec2(s, c)));
		tc += center;

		stroke_depth = texelFetch(strokeDepth, ivec2(tc), 0).r;
		outcolor = texelFetch(strokeColor, ivec2(tc), 0);
	}
	else {
		if (transparent == 1) {
			discard;
		}
		stroke_depth = texelFetch(strokeDepth, ivec2(uv), 0).r;
		outcolor = texelFetch(strokeColor, ivec2(uv), 0);
	}

	gl_FragDepth = stroke_depth;
	FragColor = outcolor;
}
