
uniform vec2 layerSelection;

uniform vec4 bokehParams;

#define bokeh_sides         bokehParams.x /* Polygon Bokeh shape number of sides */
#define bokeh_rotation      bokehParams.y
#define bokeh_ratio         bokehParams.z
#define bokeh_maxsize       bokehParams.w

uniform sampler2D colorBuffer;
uniform sampler2D cocBuffer;

flat out vec4 color;
out vec2 particlecoord;

#define M_PI 3.1415926535897932384626433832795

/* Scatter pass, calculate a triangle covering the CoC. */
void main()
{
	ivec2 tex_size = textureSize(cocBuffer, 0);
	vec2 texel_size = 1.0 / vec2(tex_size);

	int t_id = gl_VertexID / 3; /* Triangle Id */

	ivec2 texelco = ivec2(0);
	/* some math to get the target pixel */
	texelco.x = t_id % tex_size.x;
	texelco.y = t_id / tex_size.x;

	vec2 cocs = texelFetch(cocBuffer, texelco, 0).rg;
	float coc = dot(layerSelection, cocs);

	/* Clamp to max size for performance */
	coc = min(coc, bokeh_maxsize);

	if (coc >= 1.0) {
		color = texelFetch(colorBuffer, texelco, 0);
		/* find the area the pixel will cover and divide the color by it */
		color.a = 1.0 / (coc * coc * M_PI);
		color.rgb *= color.a;
	}
	else {
		/* Don't produce any fragments */
		color = vec4(0.0);
		gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	/* Generate Triangle : less memory fetches from a VBO */
	int v_id = gl_VertexID % 3; /* Vertex Id */

	/* Extend to cover at least the unit circle */
	const float extend = (cos(M_PI / 4.0) + 1.0) * 2.0;
	/* Crappy diagram
	 * ex 1
	 *    | \
	 *    |   \
	 *  1 |     \
	 *    |       \
	 *    |         \
	 *  0 |     x     \
	 *    |   Circle    \
	 *    |   Origin      \
	 * -1 0 --------------- 2
	 *   -1     0     1     ex
	 **/
	gl_Position.x = float(v_id / 2) * extend - 1.0; /* int divisor round down */
	gl_Position.y = float(v_id % 2) * extend - 1.0;
	gl_Position.z = 0.0;
	gl_Position.w = 1.0;

	/* Generate Triangle */
	particlecoord = gl_Position.xy;

	gl_Position.xy *= coc * texel_size * vec2(bokeh_ratio, 1.0);
	gl_Position.xy -= 1.0 - 0.5 * texel_size; /* NDC Bottom left */
	gl_Position.xy += (0.5 + vec2(texelco) * 2.0) * texel_size;

}
