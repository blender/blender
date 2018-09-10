
uniform vec4 bokehParams[2];

#define bokeh_rotation      bokehParams[0].x
#define bokeh_ratio         bokehParams[0].y
#define bokeh_maxsize       bokehParams[0].z

uniform sampler2D nearBuffer;
uniform sampler2D farBuffer;
uniform sampler2D cocBuffer;

flat out vec4 color;
flat out float smoothFac;
flat out ivec2 edge;
out vec2 particlecoord;

#define M_PI 3.1415926535897932384626433832795

/* Scatter pass, calculate a triangle covering the CoC. */
void main()
{
	ivec2 tex_size = textureSize(cocBuffer, 0);
	/* We render to a double width texture so compute
	 * the target texel size accordingly */
	vec2 texel_size = vec2(0.5, 1.0) / vec2(tex_size);

	int t_id = gl_VertexID / 3; /* Triangle Id */

	ivec2 texelco = ivec2(0);
	/* some math to get the target pixel */
	texelco.x = t_id % tex_size.x;
	texelco.y = t_id / tex_size.x;

	vec2 cocs = texelFetch(cocBuffer, texelco, 0).rg;

	bool is_near = (cocs.x > cocs.y);
	float coc = (is_near) ? cocs.x : cocs.y;

	/* Clamp to max size for performance */
	coc = min(coc, bokeh_maxsize);

	if (coc >= 1.0) {
		if (is_near) {
			color = texelFetch(nearBuffer, texelco, 0);
		}
		else {
			color = texelFetch(farBuffer, texelco, 0);
		}
		/* find the area the pixel will cover and divide the color by it */
		/* HACK: 4.0 out of nowhere (I suppose it's 4 pixels footprint for coc 0?)
		 * Makes near in focus more closer to 1.0 alpha. */
		color.a = 4.0 / (coc * coc * M_PI);
		color.rgb *= color.a;

		/* Compute edge to discard fragment that does not belong to the other layer. */
		edge.x = (is_near) ? 1 : -1;
		edge.y = (is_near) ? -tex_size.x + 1 : tex_size.x;
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

	/* Push far plane to left side. */
	if (!is_near) {
		gl_Position.x += 2.0 / 2.0;
	}

	/* don't do smoothing for small sprites */
	if (coc > 3.0) {
		smoothFac = 1.0 - 1.5 / coc;
	}
	else {
		smoothFac = 1.0;
	}
}
