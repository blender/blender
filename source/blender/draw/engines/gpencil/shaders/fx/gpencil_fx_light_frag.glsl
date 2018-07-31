uniform mat4 ProjectionMatrix;
uniform mat4 ViewMatrix;

uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;
uniform vec2 Viewport;
uniform vec4 loc;
uniform float energy;
uniform float ambient;

uniform float pixsize;   /* rv3d->pixsize */
uniform float pixelsize; /* U.pixelsize */
uniform float pixfactor;

out vec4 FragColor;

float defaultpixsize = pixsize * pixelsize * (1000.0 / pixfactor);

#define height   loc.w

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
	float stroke_depth;
	vec4 objcolor;

	vec4 light_loc = ProjectionMatrix * ViewMatrix * vec4(loc.xyz, 1.0);
	vec2 light2d = toScreenSpace(light_loc);

	/* calc pixel scale */
	float pxscale = (ProjectionMatrix[3][3] == 0.0) ? (10.0 / (light_loc.z * defaultpixsize)) : (10.0 / defaultpixsize);
	pxscale = max(pxscale, 0.000001);

	/* the height over plane is received in the w component of the loc
	 * and needs a factor to adapt to pixels
	 */
	float peak = height * 10.0 * pxscale;
	vec3 light3d = vec3(light2d.x, light2d.y, peak);

	vec2 uv = vec2(gl_FragCoord.xy);
	vec3 frag_loc = vec3(uv.x, uv.y, 0);
	vec3 norm = vec3(0, 0, 1.0); /* always z-up */

	ivec2 iuv = ivec2(uv.x, uv.y);
	stroke_depth = texelFetch(strokeDepth, iuv, 0).r;
	objcolor = texelFetch(strokeColor, iuv, 0);

	/* diffuse light */
	vec3 lightdir = normalize(light3d - frag_loc);
	float diff = max(dot(norm, lightdir), 0.0);
	float dist  = length(light3d - frag_loc) / pxscale;
	float factor = diff * ((energy * 100.0) / (dist * dist));

    vec3 result = factor * max(ambient, 0.1) * vec3(objcolor);

	gl_FragDepth = stroke_depth;
	FragColor = vec4(result.r, result.g, result.b, objcolor.a);
}
